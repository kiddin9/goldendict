#ifndef __FFMPEGAUDIO_HH_INCLUDED__
#define __FFMPEGAUDIO_HH_INCLUDED__

#ifdef MAKE_FFMPEG_PLAYER

#include "mutex.hh"
#include <QByteArray>
#include <QThread>

namespace Ffmpeg
{
class DecoderThread;

class AudioService : public QObject
{
    Q_OBJECT

public:
    AudioService();
    ~AudioService();
    void playMemory( const char * ptr, int size );
    void stop();

    void init();

signals:
    void error( QString const & message );

private:
    DecoderThread *dt;

};

class DecoderThread: public QThread
{
    Q_OBJECT

    QByteArray audioData_;
    Mutex bufferMutex_;
    QSemaphore deviceWC_;
    AtomicInt32 isCancelled_;

public:
    DecoderThread( QObject * parent );
    virtual ~DecoderThread();

    void play(const QByteArray &audioData);
    void cancel();
protected:
    void run();

signals:
    void error( QString const & message );
};

}

#endif // MAKE_FFMPEG_PLAYER

#endif // __FFMPEGAUDIO_HH_INCLUDED__
