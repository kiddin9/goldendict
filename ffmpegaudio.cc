#ifdef MAKE_FFMPEG_PLAYER

#include "ffmpegaudio.hh"

#include <math.h>
#include <errno.h>

#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#endif

#ifndef UINT64_C
#define UINT64_C(c) (c ## ULL)
#endif

#include <ao/ao.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

#include <QString>
#include <QDataStream>
#include <QDebug>

#include <vector>

#include "qt4x5.hh"

using std::vector;

namespace Ffmpeg
{

static inline QString avErrorString( int errnum )
{
    char buf[64];
    av_strerror( errnum, buf, 64 );
    return QString::fromLatin1( buf );
}


AudioService::AudioService()
{
    dt = new DecoderThread(this);
    connect( dt, SIGNAL( error( QString ) ), this, SIGNAL( error( QString ) ) );
}

AudioService::~AudioService()
{
    delete dt;
    ao_shutdown();
}

void AudioService::init()
{
#if LIBAVFORMAT_VERSION_MAJOR < 58 || ( LIBAVFORMAT_VERSION_MAJOR == 58 && LIBAVFORMAT_VERSION_MINOR < 9 )
    av_register_all();
#endif
    ao_initialize();
    dt->start();
}

void AudioService::playMemory( const char * ptr, int size )
{
    dt->play(QByteArray(ptr, size));
}

void AudioService::stop()
{
    dt->cancel();
}

struct DecoderContext
{
    enum
    {
        kBufferSize = 32768
    };

    AtomicInt32 & isCancelled_;
    QDataStream audioDataStream_;
    AVFormatContext * formatContext_;
    AVCodec * codec_;
    AVCodecContext * codecContext_;
    AVIOContext * avioContext_;
    AVStream * audioStream_;
    ao_device * aoDevice_;

    DecoderContext( QByteArray const & audioData, AtomicInt32 & isCancelled );
    ~DecoderContext();

    bool openCodec( QString & errorString );
    void closeCodec();
    bool openOutputDevice( QString & errorString );
    void closeOutputDevice();
    bool play( QString & errorString );
    bool normalizeAudio( AVFrame * frame, vector<char> & samples );
    void playFrame( AVFrame * frame );
};

DecoderContext::DecoderContext(QByteArray const & audioData, AtomicInt32 &isCancelled):
    isCancelled_( isCancelled ),
    audioDataStream_( audioData ),
    formatContext_( NULL ),
    codec_( NULL ),
    codecContext_( NULL ),
    avioContext_( NULL ),
    audioStream_( NULL ),
    aoDevice_( NULL )
{
}

DecoderContext::~DecoderContext()
{
    closeOutputDevice();
    closeCodec();
}

static int readAudioData( void * opaque, unsigned char * buffer, int bufferSize )
{
    QDataStream * pStream = ( QDataStream * )opaque;
    return pStream->readRawData( ( char * )buffer, bufferSize );
}

bool DecoderContext::openCodec( QString & errorString )
{
    AVFormatContext * formatContext = avformat_alloc_context();
    if ( !formatContext )
    {
        errorString = QObject::tr( "avformat_alloc_context() failed." );
        return false;
    }

#if LIBAVCODEC_VERSION_MAJOR < 56 || ( LIBAVCODEC_VERSION_MAJOR == 56 && LIBAVCODEC_VERSION_MINOR < 56 )
    unsigned char * avioBuffer = ( unsigned char * )av_malloc( kBufferSize + FF_INPUT_BUFFER_PADDING_SIZE );
#else
    unsigned char * avioBuffer = ( unsigned char * )av_malloc( kBufferSize + AV_INPUT_BUFFER_PADDING_SIZE );
#endif
    if ( !avioBuffer )
    {
        errorString = QObject::tr( "av_malloc() failed." );
        avformat_free_context(formatContext);
        return false;
    }

    // Don't free buffer allocated here (if succeeded), it will be cleaned up automatically.
    avioContext_ = avio_alloc_context( avioBuffer, kBufferSize, 0, &audioDataStream_, readAudioData, NULL, NULL );
    if ( !avioContext_ )
    {
        av_free( avioBuffer );
        errorString = QObject::tr( "avio_alloc_context() failed." );
        avformat_free_context(formatContext);
        return false;
    }

    avioContext_->seekable = 0;
    avioContext_->write_flag = 0;

    formatContext_ = formatContext;
    // If pb not set, avformat_open_input() simply crash.
    formatContext_->pb = avioContext_;
    formatContext_->flags |= AVFMT_FLAG_CUSTOM_IO;

    int ret = 0;

    ret = avformat_open_input( &formatContext_, "_STREAM_", NULL, NULL );
    if ( ret < 0 )
    {
        errorString = QObject::tr( "avformat_open_input() failed: %1." ).arg( avErrorString( ret ) );
        return false;
    }

    ret = avformat_find_stream_info( formatContext_, NULL );
    if ( ret < 0 )
    {
        errorString = QObject::tr( "avformat_find_stream_info() failed: %1." ).arg( avErrorString( ret ) );
        return false;
    }

    // Find audio stream, use the first audio stream if available
    for ( unsigned i = 0; i < formatContext_->nb_streams; i++ )
    {
#if LIBAVCODEC_VERSION_MAJOR < 57 || ( LIBAVCODEC_VERSION_MAJOR == 57 && LIBAVCODEC_VERSION_MINOR < 33 )
        if ( formatContext_->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO )
#else
        if ( formatContext_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO )
#endif
        {
            audioStream_ = formatContext_->streams[i];
            break;
        }
    }
    if ( !audioStream_ )
    {
        errorString = QObject::tr( "Could not find audio stream." );
        return false;
    }

#if LIBAVCODEC_VERSION_MAJOR < 57 || ( LIBAVCODEC_VERSION_MAJOR == 57 && LIBAVCODEC_VERSION_MINOR < 33 )
    codecContext_ = audioStream_->codec;
    codec_ = avcodec_find_decoder( codecContext_->codec_id );
    if ( !codec_ )
    {
        errorString = QObject::tr( "Codec [id: %1] not found." ).arg( codecContext_->codec_id );
        return false;
    }
#else
    codec_ = avcodec_find_decoder( audioStream_->codecpar->codec_id );
    if ( !codec_ )
    {
        errorString = QObject::tr( "Codec [id: %1] not found." ).arg( audioStream_->codecpar->codec_id );
        return false;
    }
    codecContext_ = avcodec_alloc_context3( codec_ );
    if ( !codecContext_ )
    {
        errorString = QObject::tr( "avcodec_alloc_context3() failed." );
        return false;
    }
    avcodec_parameters_to_context( codecContext_, audioStream_->codecpar );
#endif

    ret = avcodec_open2( codecContext_, codec_, NULL );
    if ( ret < 0 )
    {
        errorString = QObject::tr( "avcodec_open2() failed: %1." ).arg( avErrorString( ret ) );
        return false;
    }

#ifndef NO_CONSOLE
    av_log( NULL, AV_LOG_INFO, "Codec open: %s: channels: %d, rate: %d, format: %s\n", codec_->long_name,
            codecContext_->channels, codecContext_->sample_rate, av_get_sample_fmt_name( codecContext_->sample_fmt ) );
#endif
    return true;
}

void DecoderContext::closeCodec()
{
    if ( !formatContext_ )
        return;

    // Closing a codec context without prior avcodec_open2() will result in
    // a crash in ffmpeg
    if ( audioStream_ && codecContext_ && codec_ )
    {
        audioStream_->discard = AVDISCARD_ALL;
        avcodec_close( codecContext_ );
#if LIBAVCODEC_VERSION_MAJOR > 57 || ( LIBAVCODEC_VERSION_MAJOR == 57 && LIBAVCODEC_VERSION_MINOR >= 33 )
        avcodec_free_context( &codecContext_ );
#endif
    }

    avformat_close_input( &formatContext_ );
    av_free( avioContext_->buffer );
}

bool DecoderContext::openOutputDevice( QString & errorString )
{
    // Prepare for audio output
    int aoDriverId = ao_default_driver_id();
    ao_info * aoDrvInfo = ao_driver_info( aoDriverId );

    if ( aoDriverId < 0 || !aoDrvInfo )
    {
        errorString = QObject::tr( "Cannot find usable audio output device." );
        return false;
    }

    ao_sample_format aoSampleFormat;
    aoSampleFormat.channels = codecContext_->channels;
    aoSampleFormat.rate = codecContext_->sample_rate;
    aoSampleFormat.byte_format = AO_FMT_NATIVE;
    aoSampleFormat.matrix = 0;
    aoSampleFormat.bits = qMin( 32, av_get_bytes_per_sample( codecContext_->sample_fmt ) << 3 );

    if ( aoSampleFormat.bits == 0 )
    {
        errorString = QObject::tr( "Unsupported sample format." );
        return false;
    }

#ifndef NO_CONSOLE
    av_log( NULL, AV_LOG_INFO, "ao_open_live(): %s: channels: %d, rate: %d, bits: %d\n",
            aoDrvInfo->name, aoSampleFormat.channels, aoSampleFormat.rate, aoSampleFormat.bits );
#endif

    aoDevice_ = ao_open_live( aoDriverId, &aoSampleFormat, NULL );
    if ( !aoDevice_ )
    {
        errorString = QObject::tr( "ao_open_live() failed: " );

        switch ( errno )
        {
        case AO_ENODRIVER:
            errorString += QObject::tr( "No driver." );
            break;
        case AO_ENOTLIVE:
            errorString += QObject::tr( "This driver is not a live output device." );
            break;
        case AO_EBADOPTION:
            errorString += QObject::tr( "A valid option key has an invalid value." );
            break;
        case AO_EOPENDEVICE:
            errorString += QObject::tr( "Cannot open the device: %1, channels: %2, rate: %3, bits: %4." )
                    .arg( aoDrvInfo->short_name )
                    .arg( aoSampleFormat.channels )
                    .arg( aoSampleFormat.rate )
                    .arg( aoSampleFormat.bits );
            break;
        default:
            errorString += QObject::tr( "Unknown error." );
            break;
        }

        return false;
    }

    return true;
}

void DecoderContext::closeOutputDevice()
{
    // ao_close() is synchronous, it will wait until all audio streams flushed
    if ( aoDevice_ )
    {
        ao_close( aoDevice_ );
        aoDevice_ = NULL;
    }
}

bool DecoderContext::play( QString & errorString )
{
#if LIBAVCODEC_VERSION_MAJOR < 55 || ( LIBAVCODEC_VERSION_MAJOR == 55 && LIBAVCODEC_VERSION_MINOR < 28 )
    AVFrame * frame = avcodec_alloc_frame();
#else
    AVFrame * frame = av_frame_alloc();
#endif
    if ( !frame )
    {
        errorString = QObject::tr( "avcodec_alloc_frame() failed." );
        return false;
    }

    AVPacket packet;
    av_init_packet( &packet );

    while ( !Qt4x5::AtomicInt::loadAcquire( isCancelled_ ) &&
            av_read_frame( formatContext_, &packet ) >= 0 )
    {
        if ( packet.stream_index == audioStream_->index )
        {
            AVPacket pack = packet;
#if LIBAVCODEC_VERSION_MAJOR < 57 || ( LIBAVCODEC_VERSION_MAJOR == 57 && LIBAVCODEC_VERSION_MINOR < 37 )
            int gotFrame = 0;
            do
            {
                int len = avcodec_decode_audio4( codecContext_, frame, &gotFrame, &pack );
                if ( !Qt4x5::AtomicInt::loadAcquire( isCancelled_ ) && gotFrame )
                {
                    playFrame( frame );
                }
                if( len <= 0 || Qt4x5::AtomicInt::loadAcquire( isCancelled_ ) )
                    break;
                pack.size -= len;
                pack.data += len;
            }
            while( pack.size > 0 );
#else
            int ret = avcodec_send_packet( codecContext_, &pack );
            /* read all the output frames (in general there may be any number of them) */
            while( ret >= 0 )
            {
                ret = avcodec_receive_frame( codecContext_, frame);

                if ( Qt4x5::AtomicInt::loadAcquire( isCancelled_ ) || ret < 0 )
                    break;

                playFrame( frame );
            }
#endif
        }
        // av_free_packet() must be called after each call to av_read_frame()
#if LIBAVCODEC_VERSION_MAJOR < 57 || ( LIBAVCODEC_VERSION_MAJOR == 57 && LIBAVCODEC_VERSION_MINOR < 7 )
        av_free_packet( &packet );
#else
        av_packet_unref( &packet );
#endif
    }

#if LIBAVCODEC_VERSION_MAJOR < 57 || ( LIBAVCODEC_VERSION_MAJOR == 57 && LIBAVCODEC_VERSION_MINOR < 37 )
    if ( !Qt4x5::AtomicInt::loadAcquire( isCancelled_ ) &&
         codecContext_->codec->capabilities & CODEC_CAP_DELAY )
    {
        av_init_packet( &packet );
        int gotFrame = 0;
        while ( avcodec_decode_audio4( codecContext_, frame, &gotFrame, &packet ) >= 0 && gotFrame )
        {
            if ( Qt4x5::AtomicInt::loadAcquire( isCancelled_ ) )
                break;
            playFrame( frame );
        }
    }
#else
    /* flush the decoder */
    av_init_packet( &packet );
    packet.data = NULL;
    packet.size = 0;
    int ret = avcodec_send_packet(codecContext_, &packet );
    while( ret >= 0 )
    {
        ret = avcodec_receive_frame(codecContext_, frame);
        if ( Qt4x5::AtomicInt::loadAcquire( isCancelled_ ) || ret < 0 )
            break;
        playFrame( frame );
    }
#endif

#if LIBAVCODEC_VERSION_MAJOR < 54
    av_free( frame );
#elif LIBAVCODEC_VERSION_MAJOR < 55 || ( LIBAVCODEC_VERSION_MAJOR == 55 && LIBAVCODEC_VERSION_MINOR < 28 )
    avcodec_free_frame( &frame );
#else
    av_frame_free( &frame );
#endif

    return true;
}

static inline int32_t toInt32( double v )
{
    if ( v >= 1.0 )
        return 0x7fffffffL;
    else if ( v <= -1.0 )
        return 0x80000000L;
    return floor( v * 2147483648.0 );
}

bool DecoderContext::normalizeAudio( AVFrame * frame, vector<char> & samples )
{
    int lineSize = 0;
    int dataSize = av_samples_get_buffer_size( &lineSize, codecContext_->channels,
                                               frame->nb_samples, codecContext_->sample_fmt, 1 );

    // Portions from: https://code.google.com/p/lavfilters/source/browse/decoder/LAVAudio/LAVAudio.cpp
    // But this one use 8, 16, 32 bits integer, respectively.
    switch ( codecContext_->sample_fmt )
    {
    case AV_SAMPLE_FMT_U8:
    case AV_SAMPLE_FMT_S16:
    case AV_SAMPLE_FMT_S32:
    {
        samples.resize( dataSize );
        memcpy( &samples.front(), frame->data[0], lineSize );
    }
        break;
    case AV_SAMPLE_FMT_FLT:
    {
        samples.resize( dataSize );

        int32_t * out = ( int32_t * )&samples.front();
        for ( int i = 0; i < dataSize; i += sizeof( float ) )
        {
            *out++ = toInt32( *( float * )frame->data[i] );
        }
    }
        break;
    case AV_SAMPLE_FMT_DBL:
    {
        samples.resize( dataSize / 2 );

        int32_t * out = ( int32_t * )&samples.front();
        for ( int i = 0; i < dataSize; i += sizeof( double ) )
        {
            *out++ = toInt32( *( double * )frame->data[i] );
        }
    }
        break;
        // Planar
    case AV_SAMPLE_FMT_U8P:
    {
        samples.resize( dataSize );

        uint8_t * out = ( uint8_t * )&samples.front();
        for ( int i = 0; i < frame->nb_samples; i++ )
        {
            for ( int ch = 0; ch < codecContext_->channels; ch++ )
            {
                *out++ = ( ( uint8_t * )frame->extended_data[ch] )[i];
            }
        }
    }
        break;
    case AV_SAMPLE_FMT_S16P:
    {
        samples.resize( dataSize );

        int16_t * out = ( int16_t * )&samples.front();
        for ( int i = 0; i < frame->nb_samples; i++ )
        {
            for ( int ch = 0; ch < codecContext_->channels; ch++ )
            {
                *out++ = ( ( int16_t * )frame->extended_data[ch] )[i];
            }
        }
    }
        break;
    case AV_SAMPLE_FMT_S32P:
    {
        samples.resize( dataSize );

        int32_t * out = ( int32_t * )&samples.front();
        for ( int i = 0; i < frame->nb_samples; i++ )
        {
            for ( int ch = 0; ch < codecContext_->channels; ch++ )
            {
                *out++ = ( ( int32_t * )frame->extended_data[ch] )[i];
            }
        }
    }
        break;
    case AV_SAMPLE_FMT_FLTP:
    {
        samples.resize( dataSize );

        float ** data = ( float ** )frame->extended_data;
        int32_t * out = ( int32_t * )&samples.front();
        for ( int i = 0; i < frame->nb_samples; i++ )
        {
            for ( int ch = 0; ch < codecContext_->channels; ch++ )
            {
                *out++ = toInt32( data[ch][i] );
            }
        }
    }
        break;
    case AV_SAMPLE_FMT_DBLP:
    {
        samples.resize( dataSize / 2 );

        double ** data = ( double ** )frame->extended_data;
        int32_t * out = ( int32_t * )&samples.front();
        for ( int i = 0; i < frame->nb_samples; i++ )
        {
            for ( int ch = 0; ch < codecContext_->channels; ch++ )
            {
                *out++ = toInt32( data[ch][i] );
            }
        }
    }
        break;
    default:
        return false;
    }

    return true;
}

void DecoderContext::playFrame( AVFrame * frame )
{
    if ( !frame )
        return;

    vector<char> samples;
    if ( normalizeAudio( frame, samples ) )
        ao_play( aoDevice_, &samples.front(), samples.size() );
}

DecoderThread::DecoderThread( QObject * parent ) :
    QThread( parent ), deviceWC_( 0 ), isCancelled_( 0 )
{
}

DecoderThread::~DecoderThread()
{
    if(QThread::isRunning())
    {
        isCancelled_.storeRelease(2);
        deviceWC_.release();
        QThread::wait();
    }
}

void DecoderThread::run()
{
    while(Qt4x5::AtomicInt::loadAcquire( isCancelled_) != 2) {
        if (!deviceWC_.tryAcquire(1, -1))
            continue;
        if ( Qt4x5::AtomicInt::loadAcquire( isCancelled_ ) == 2 )
            break;

        QByteArray audioData;
        bufferMutex_.lock();
        audioData.swap(audioData_);
        bufferMutex_.unlock();
        if(audioData.isEmpty())
            continue;
        isCancelled_.testAndSetRelease(1, 0);

        QString errorString;
        DecoderContext d( audioData, isCancelled_ );

        if ( d.openCodec( errorString ) )
        {
            if ( Qt4x5::AtomicInt::loadAcquire( isCancelled_ ) )
                continue;
            if ( d.openOutputDevice( errorString ) && d.play( errorString ) )
                continue;
        }
        emit error( errorString );
    }
}

void DecoderThread::play(const QByteArray & audioData)
{
    isCancelled_.testAndSetRelease(0, 1);
    bufferMutex_.lock();
    deviceWC_.release();
    audioData_ = audioData;
    bufferMutex_.unlock();
}

void DecoderThread::cancel()
{
    if(Qt4x5::AtomicInt::loadAcquire( isCancelled_) != 2)
    {
        isCancelled_.storeRelease(1);
    }
}

}

#endif // MAKE_FFMPEG_PLAYER
