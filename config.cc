/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "config.hh"
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#ifdef GD_PUGIXML_XSERIAL
#include "pugixml_Qt.h"
#else
#include <QtXml>
#endif
#include "gddebug.hh"
#include "fmodex_player.hh"
#if defined( _MSC_VER ) && _MSC_VER < 1800 // VS2012 and older
#include <stdint_msvc.h>
#else
#include <stdint.h>
#endif

#ifdef Q_OS_WIN32
#include "shlobj.h"
#endif

#include "atomic_rename.hh"
#include "qt4x5.hh"

namespace Config {

namespace
{
QDir getHomeDir()
{
    if ( isPortableVersion() )
        return QDir( QCoreApplication::applicationDirPath() + "/portable" );

    QDir result;

    result = QDir::home();
#ifdef Q_OS_WIN32
    if ( result.cd( "Application Data/GoldenDict" ) )
        return result;
    char const * pathInHome = "GoldenDict";
    result = QDir::fromNativeSeparators( QString::fromWCharArray( _wgetenv( L"APPDATA" ) ) );
#else
    char const * pathInHome = ".goldendict";
#endif

    result.mkpath( pathInHome );

    if ( !result.cd( pathInHome ) )
        throw exCantUseHomeDir();

    return result;
}

}

ProxyServer::ProxyServer(): enabled( false ), useSystemProxy( false ), type( Socks5 ), port( 3128 )
{
}

HotKey::HotKey(): modifiers( 0 ), key1( 0 ), key2( 0 )
{
}

// Does anyone know how to separate modifiers from the keycode? We'll
// use our own mask.

uint32_t const keyMask = 0x01FFFFFF;

HotKey::HotKey( QKeySequence const & seq ):
    modifiers( seq[ 0 ] & ~keyMask ),
    key1( seq[ 0 ] & keyMask ),
    key2( seq[ 1 ] & keyMask )
{
}

QKeySequence HotKey::toKeySequence() const
{
    int v2 = key2 ? ( key2 | modifiers ): 0;

    return QKeySequence( key1 | modifiers, v2 );
}

bool InternalPlayerBackend::anyAvailable()
{
#if defined( MAKE_FFMPEG_PLAYER ) || defined( MAKE_QTMULTIMEDIA_PLAYER )
    return true;
#elif defined( MAKE_FMODEX_PLAYER )
    return FmodexAudioPlayer::available();
#else
    return false;
#endif
}

InternalPlayerBackend InternalPlayerBackend::defaultBackend()
{
#if defined( MAKE_FFMPEG_PLAYER )
    return ffmpeg();
#elif defined( MAKE_FMODEX_PLAYER )
    if(FmodexAudioPlayer::available())
        return fmodex();
#endif
#if defined( MAKE_QTMULTIMEDIA_PLAYER )
    return qtmultimedia();
#else
    return InternalPlayerBackend( QString() );
#endif
}

QStringList InternalPlayerBackend::nameList()
{
    QStringList result;
#ifdef MAKE_FFMPEG_PLAYER
    result.push_back( ffmpeg().uiName() );
#endif
#ifdef MAKE_FMODEX_PLAYER
    if(FmodexAudioPlayer::available())
        result.push_back( fmodex().uiName() );
#endif
#ifdef MAKE_QTMULTIMEDIA_PLAYER
    result.push_back( qtmultimedia().uiName() );
#endif
    return result;
}

bool InternalPlayerBackend::isFfmpeg() const
{
#ifdef MAKE_FFMPEG_PLAYER
    return *this == ffmpeg();
#else
    return false;
#endif
}

bool InternalPlayerBackend::isQtmultimedia() const
{
#ifdef MAKE_QTMULTIMEDIA_PLAYER
    return *this == qtmultimedia();
#else
    return false;
#endif
}

bool InternalPlayerBackend::isFmodex() const
{
#ifdef MAKE_FMODEX_PLAYER
    return *this == fmodex();
#else
    return false;
#endif
}

ScanPopupWindowFlags spwfFromInt( int id )
{
    if( id == SPWF_Popup )
        return SPWF_Popup;
    if( id == SPWF_Tool )
        return SPWF_Tool;

    if( id != SPWF_default )
        gdWarning( "Invalid ScanPopup unpinned window flags: %d\n", id );
    return SPWF_default;
}

Preferences::Preferences():
    newTabsOpenAfterCurrentOne( false ),
    newTabsOpenInBackground( true ),
    hideSingleTab( false ),
    mruTabOrder ( false ),
    hideMenubar( false ),
    enableTrayIcon( true ),
    startToTray( false ),
    closeToTray( true ),
    autoStart( false ),
    doubleClickTranslates( true ),
    selectWordBySingleClick( false ),
    escKeyHidesMainWindow( false ),
    alwaysOnTop ( false ),
    searchInDock ( false ),

    enableMainWindowHotkey( true ),
    mainWindowHotkey( QKeySequence( "Ctrl+F11,F11" ) ),
    enableClipboardHotkey( true ),
    clipboardHotkey( QKeySequence( "Ctrl+C,C" ) ),

    enableScanPopup( true ),
    startWithScanPopupOn( false ),
    enableScanPopupModifiers( false ),
    scanPopupModifiers( 0 ),
    scanPopupAltMode( false ),
    scanPopupAltModeSecs( 3 ),
    ignoreOwnClipboardChanges( false ),
    scanPopupUseUIAutomation( true ),
    scanPopupUseIAccessibleEx( true ),
    scanPopupUseGDMessage( true ),
    scanPopupUnpinnedWindowFlags( SPWF_default ),
    scanPopupUnpinnedBypassWMHint( false ),
    scanToMainWindow( false ),
    ignoreDiacritics( false ),
    #ifdef HAVE_X11
    showScanFlag( false ),
    #endif
    pronounceOnLoadMain( false ),
    pronounceOnLoadPopup( false ),
    useInternalPlayer( InternalPlayerBackend::anyAvailable() ),
    internalPlayerBackend( InternalPlayerBackend::defaultBackend() ),
    checkForNewReleases( true ),
    disallowContentFromOtherSites( false ),
    enableWebPlugins( false ),
    hideGoldenDictHeader( false ),
    zoomFactor( 1 ),
    helpZoomFactor( 1 ),
    wordsZoomLevel( 0 ),
    maxStringsInHistory( 500 ),
    storeHistory( 1 ),
    alwaysExpandOptionalParts( false )
  , historyStoreInterval( 0 )
  , favoritesStoreInterval( 0 )
  , confirmFavoritesDeletion( true )
  , collapseBigArticles( false )
  , articleSizeLimit( 2000 )
  , maxDictionaryRefsInContextMenu ( 20 )
  #ifndef Q_WS_X11
  , trackClipboardChanges( false )
  #endif
  , synonymSearchEnabled( true )
{
}

#ifdef GD_TRANSLITERATION_SUPPORT
Chinese::Chinese():
    enable( false ),
    enableSCToTWConversion( true ),
    enableSCToHKConversion( true ),
    enableTCToSCConversion( true )
{
}

Romaji::Romaji():
    enable( false ),
    enableHepburn( true ),
    enableNihonShiki( false ),
    enableKunreiShiki( false ),
    enableHiragana( true ),
    enableKatakana( true )
{
}
#endif

Group * Class::getGroup( unsigned id )
{
    for( int x = 0; x < groups.size(); x++ )
        if( groups.at( x ).id == id )
            return &groups[ x ];
    return 0;
}

Group const * Class::getGroup( unsigned id ) const
{
    for( int x = 0; x < groups.size(); x++ )
        if( groups.at( x ).id == id )
            return &groups.at( x );
    return 0;
}

void Events::signalMutedDictionariesChanged()
{
    emit mutedDictionariesChanged();
}

namespace {

#ifdef GD_MEDIAWIKI_SUPPORT
MediaWikis makeDefaultMediaWikis( bool enable )
{
    MediaWikis mw;

    mw.push_back( MediaWiki( "ae6f89aac7151829681b85f035d54e48", "English Wikipedia", "https://en.wikipedia.org/w", enable, "" ) );
    mw.push_back( MediaWiki( "affcf9678e7bfe701c9b071f97eccba3", "English Wiktionary", "https://en.wiktionary.org/w", false, ""  ) );
    mw.push_back( MediaWiki( "8e0c1c2b6821dab8bdba8eb869ca7176", "Russian Wikipedia", "https://ru.wikipedia.org/w", false, "" ) );
    mw.push_back( MediaWiki( "b09947600ae3902654f8ad4567ae8567", "Russian Wiktionary", "https://ru.wiktionary.org/w", false, "" ) );
    mw.push_back( MediaWiki( "a8a66331a1242ca2aeb0b4aed361c41d", "German Wikipedia", "https://de.wikipedia.org/w", false, "" ) );
    mw.push_back( MediaWiki( "21c64bca5ec10ba17ff19f3066bc962a", "German Wiktionary", "https://de.wiktionary.org/w", false, "" ) );
    mw.push_back( MediaWiki( "96957cb2ad73a20c7a1d561fc83c253a", "Portuguese Wikipedia", "https://pt.wikipedia.org/w", false, "" ) );
    mw.push_back( MediaWiki( "ed4c3929196afdd93cc08b9a903aad6a", "Portuguese Wiktionary", "https://pt.wiktionary.org/w", false, "" ) );
    mw.push_back( MediaWiki( "f3b4ec8531e52ddf5b10d21e4577a7a2", "Greek Wikipedia", "https://el.wikipedia.org/w", false, "" ) );
    mw.push_back( MediaWiki( "5d45232075d06e002dea72fe3e137da1", "Greek Wiktionary", "https://el.wiktionary.org/w", false, "" ) );

    return mw;
}
#endif

#ifdef GD_WEBSITE_SUPPORT
WebSites makeDefaultWebSites()
{
    WebSites ws;

    ws.push_back( WebSite( "b88cb2898e634c6638df618528284c2d", "Google En-En (Oxford)", "https://www.google.com/search?q=define:%GDWORD%&hl=en", false, "", true ) );
    ws.push_back( WebSite( "f376365a0de651fd7505e7e5e683aa45", "Urban Dictionary", "https://www.urbandictionary.com/define.php?term=%GDWORD%", false, "", true ) );
    ws.push_back( WebSite( "324ca0306187df7511b26d3847f4b07c", "Multitran (En)", "https://multitran.ru/c/m.exe?CL=1&l1=1&s=%GD1251%", false, "", true ) );
    ws.push_back( WebSite( "924db471b105299c82892067c0f10787", "Lingvo (En-Ru)", "http://lingvopro.abbyyonline.com/en/Search/en-ru/%GDWORD%", false, "", true ) );
    ws.push_back( WebSite( "087a6d65615fb047f4c80eef0a9465db", "Michaelis (Pt-En)", "http://michaelis.uol.com.br/moderno/ingles/index.php?lingua=portugues-ingles&palavra=%GDISO1%", false, "", true ) );

    return ws;
}
#endif

#ifdef GD_DICTSERVER_SUPPORT
DictServers makeDefaultDictServers()
{
    DictServers ds;

    return ds;
}
#endif

#ifdef GD_PROGRAM_SUPPORT
Programs makeDefaultPrograms()
{
    Programs programs;

    // The following list doesn't make a lot of sense under Windows
#ifndef Q_OS_WIN
    programs.push_back( Program( false, Program::Audio, "428b4c2b905ef568a43d9a16f59559b0", "Festival", "festival --tts", "" ) );
    programs.push_back( Program( false, Program::Audio, "2cf8b3a60f27e1ac812de0b57c148340", "Espeak", "espeak %GDWORD%", "" ) );
    programs.push_back( Program( false, Program::Html, "4f898f7582596cea518c6b0bfdceb8b3", "Manpages", "man -a --html=/bin/cat %GDWORD%", "" ) );
#endif

    return programs;
}
#endif

#ifndef GD_PUGIXML_XSERIAL
/// Sets option to true of false if node is "1" or "0" respectively, or leaves
/// it intact if it's neither "1" nor "0".
void applyBoolOption( bool & option, QDomNode const & node )
{
    QString value = node.toElement().text();

    if ( value == "1" )
        option = true;
    else
        if ( value == "0" )
            option = false;
}

Group loadGroup( QDomElement grp, unsigned * nextId = 0 )
{
    Group g;

    if ( grp.hasAttribute( "id" ) )
        g.id = grp.attribute( "id" ).toUInt();
    else
        g.id = nextId ? (*nextId)++ : 0;

    g.name = grp.attribute( "name" );
    g.icon = grp.attribute( "icon" );
    g.favoritesFolder = grp.attribute( "favoritesFolder" );

    if ( !grp.attribute( "iconData" ).isEmpty() )
        g.iconData = QByteArray::fromBase64( grp.attribute( "iconData" ).toLatin1() );

    if ( !grp.attribute( "shortcut" ).isEmpty() )
        g.shortcut = QKeySequence::fromString( grp.attribute( "shortcut" ) );

    QDomNodeList dicts = grp.elementsByTagName( "dictionary" );

    for( Qt4x5::Dom::size_type y = 0; y < dicts.length(); ++y )
        g.dictionaries.push_back( DictionaryRef( dicts.item( y ).toElement().text(),
                                                 dicts.item( y ).toElement().attribute( "name" ) ) );

    QDomNode muted = grp.namedItem( "mutedDictionaries" );
    dicts = muted.toElement().elementsByTagName( "mutedDictionary" );
    for( Qt4x5::Dom::size_type x = 0; x < dicts.length(); ++x )
        g.mutedDictionaries.insert( dicts.item( x ).toElement().text() );

    dicts = muted.toElement().elementsByTagName( "popupMutedDictionary" );
    for( Qt4x5::Dom::size_type x = 0; x < dicts.length(); ++x )
        g.popupMutedDictionaries.insert( dicts.item( x ).toElement().text() );

    return g;
}

MutedDictionaries loadMutedDictionaries( QDomNode mutedDictionaries )
{
    MutedDictionaries result;

    if ( !mutedDictionaries.isNull() )
    {
        QDomNodeList nl = mutedDictionaries.toElement().
                elementsByTagName( "mutedDictionary" );

        for( Qt4x5::Dom::size_type x = 0; x < nl.length(); ++x )
            result.insert( nl.item( x ).toElement().text() );
    }

    return result;
}

void saveMutedDictionaries( QDomDocument & dd, QDomElement & muted,
                            MutedDictionaries const & mutedDictionaries )
{
    for( MutedDictionaries::const_iterator i = mutedDictionaries.begin();
         i != mutedDictionaries.end(); ++i )
    {
        QDomElement dict = dd.createElement( "mutedDictionary" );
        muted.appendChild( dict );

        QDomText value = dd.createTextNode( *i );
        dict.appendChild( value );
    }
}
#endif
}

#ifndef GD_PUGIXML_XSERIAL
Class load() THROW_SPEC( exError )
{
    QString configName  = getConfigFileName();

    bool loadFromTemplate = false;

    if ( !QFile::exists( configName ) )
    {
        // Make the default config, save it and return it
        Class c;

#ifdef Q_OS_LINUX
        if ( QDir( "/usr/share/stardict/dic" ).exists() )
            c.paths.push_back( Path( "/usr/share/stardict/dic", true ) );

        if ( QDir( "/usr/share/dictd" ).exists() )
            c.paths.push_back( Path( "/usr/share/dictd", true ) );

        if ( QDir( "/usr/share/opendict/dictionaries" ).exists() )
            c.paths.push_back( Path( "/usr/share/opendict/dictionaries", true ) );

        if ( QDir( "/usr/share/goldendict-wordnet" ).exists() )
            c.paths.push_back( Path( "/usr/share/goldendict-wordnet", true ) );

        if ( QDir( "/usr/share/WyabdcRealPeopleTTS" ).exists() )
            c.soundDirs.push_back( SoundDir( "/usr/share/WyabdcRealPeopleTTS", "WyabdcRealPeopleTTS" ) );

        if ( QDir( "/usr/share/myspell/dicts" ).exists() )
            c.hunspell.dictionariesPath = "/usr/share/myspell/dicts";

#endif

#ifdef Q_OS_WIN32

        // get path to Program Files
        wchar_t buf[ MAX_PATH ];
        SHGetFolderPathW( NULL, CSIDL_PROGRAM_FILES, NULL, 0, buf );
        QString pathToProgramFiles = QString::fromWCharArray( buf );
        if ( pathToProgramFiles.isEmpty() )
            pathToProgramFiles = "C:\\Program Files";

        if ( QDir( pathToProgramFiles + "\\StarDict\\dic" ).exists() )
            c.paths.push_back( Path( pathToProgramFiles + "\\StarDict\\dic", true ) );

#ifdef GD_SOUND_DIRS_SUPPORT
        if ( QDir( pathToProgramFiles + "\\StarDict\\WyabdcRealPeopleTTS" ).exists() )
            c.soundDirs.push_back( SoundDir( pathToProgramFiles + "\\StarDict\\WyabdcRealPeopleTTS", "WyabdcRealPeopleTTS" ) );
        else
            if ( QDir( pathToProgramFiles + "\\WyabdcRealPeopleTTS" ).exists() )
                c.soundDirs.push_back( SoundDir( pathToProgramFiles + "\\WyabdcRealPeopleTTS", "WyabdcRealPeopleTTS" ) );
#endif

        // #### "C:/Program Files" is bad! will not work for German Windows etc.
        // #### should be replaced to system path

        //    if ( QDir( "C:/Program Files/StarDict/dic" ).exists() )
        //      c.paths.push_back( Path( "C:/Program Files/StarDict/dic", true ) );
        //
        //    if ( QDir( "C:/Program Files/StarDict/WyabdcRealPeopleTTS" ).exists() )
        //      c.soundDirs.push_back( SoundDir( "C:/Program Files/StarDict/WyabdcRealPeopleTTS", "WyabdcRealPeopleTTS" ) );
        //    else
        //    if ( QDir( "C:/Program Files/WyabdcRealPeopleTTS" ).exists() )
        //      c.soundDirs.push_back( SoundDir( "C:/Program Files/WyabdcRealPeopleTTS", "WyabdcRealPeopleTTS" ) );

#endif

#ifndef Q_OS_WIN32
        c.preferences.audioPlaybackProgram = "mplayer";
#endif

#ifdef GD_HUNSPELL_SUPPORT
        QString possibleMorphologyPath = getProgramDataDir() + "/content/morphology";

        if ( QDir( possibleMorphologyPath ).exists() )
            c.hunspell.dictionariesPath = possibleMorphologyPath;
#endif

#ifdef GD_MEDIAWIKI_SUPPORT
        c.mediawikis = makeDefaultMediaWikis( true );
#endif
#ifdef GD_WEBSITE_SUPPORT
        c.webSites = makeDefaultWebSites();
#endif
#ifdef GD_DICTSERVER_SUPPORT
        c.dictServers = makeDefaultDictServers();
#endif

        // Check if we have a template config file. If we do, load it instead

        configName = getProgramDataDir() + "/content/defconfig";
        loadFromTemplate = QFile( configName ).exists();

        if ( !loadFromTemplate )
        {
            save( c );

            return c;
        }
    }

    getStylesDir();

    QFile configFile( configName );

    if ( !configFile.open( QFile::ReadOnly ) )
        throw exCantReadConfigFile();

    QDomDocument dd;

    QString errorStr;
    int errorLine, errorColumn;

    if ( !loadFromTemplate )
    {
        // Load the config as usual
        if ( !dd.setContent( &configFile, false, &errorStr, &errorLine, &errorColumn  ) )
        {
            GD_DPRINTF( "Error: %s at %d,%d\n", errorStr.toLocal8Bit().constData(),  errorLine,  errorColumn );
            throw exMalformedConfigFile();
        }
    }
    else
    {
        // We need to replace all %PROGRAMDIR% with the program data dir
        QByteArray data = configFile.readAll();

        data.replace( "%PROGRAMDIR%", getProgramDataDir().toUtf8() );

        QBuffer bufferedData( &data );

        if ( !dd.setContent( &bufferedData, false, &errorStr, &errorLine, &errorColumn  ) )
        {
            GD_DPRINTF( "Error: %s at %d,%d\n", errorStr.toLocal8Bit().constData(),  errorLine,  errorColumn );
            throw exMalformedConfigFile();
        }
    }

    configFile.close();

    QDomNode root = dd.namedItem( "config" );

    Class c;

    QDomNode paths = root.namedItem( "paths" );

    if ( !paths.isNull() )
    {
        QDomNodeList nl = paths.toElement().elementsByTagName( "path" );

        for( Qt4x5::Dom::size_type x = 0; x < nl.length(); ++x )
            c.paths.push_back(
                        Path( nl.item( x ).toElement().text(),
                              nl.item( x ).toElement().attribute( "recursive" ) == "1" ) );
    }

#ifdef GD_SOUND_DIRS_SUPPORT
    QDomNode soundDirs = root.namedItem( "sounddirs" );

    if ( !soundDirs.isNull() )
    {
        QDomNodeList nl = soundDirs.toElement().elementsByTagName( "sounddir" );

        for( Qt4x5::Dom::size_type x = 0; x < nl.length(); ++x )
            c.soundDirs.push_back(
                        SoundDir( nl.item( x ).toElement().text(),
                                  nl.item( x ).toElement().attribute( "name" ),
                                  nl.item( x ).toElement().attribute( "icon" ) ) );
    }
#endif

    QDomNode dictionaryOrder = root.namedItem( "dictionaryOrder" );

    if ( !dictionaryOrder.isNull() )
        c.dictionaryOrder = loadGroup( dictionaryOrder.toElement() );

    QDomNode inactiveDictionaries = root.namedItem( "inactiveDictionaries" );

    if ( !inactiveDictionaries.isNull() )
        c.inactiveDictionaries = loadGroup( inactiveDictionaries.toElement() );

    QDomNode groups = root.namedItem( "groups" );

    if ( !groups.isNull() )
    {
        c.groups.nextId = groups.toElement().attribute( "nextId", "1" ).toUInt();

        QDomNodeList nl = groups.toElement().elementsByTagName( "group" );

        for( Qt4x5::Dom::size_type x = 0; x < nl.length(); ++x )
        {
            QDomElement grp = nl.item( x ).toElement();

            c.groups.push_back( loadGroup( grp, &c.groups.nextId ) );
        }
    }

#ifdef GD_HUNSPELL_SUPPORT
    QDomNode hunspell = root.namedItem( "hunspell" );

    if ( !hunspell.isNull() )
    {
        c.hunspell.dictionariesPath = hunspell.toElement().attribute( "dictionariesPath" );

        QDomNodeList nl = hunspell.toElement().elementsByTagName( "enabled" );

        for( Qt4x5::Dom::size_type x = 0; x < nl.length(); ++x )
            c.hunspell.enabledDictionaries.push_back( nl.item( x ).toElement().text() );
    }
#endif

#ifdef GD_TRANSLITERATION_SUPPORT
    QDomNode transliteration = root.namedItem( "transliteration" );

    if ( !transliteration.isNull() )
    {
        applyBoolOption( c.transliteration.enableRussianTransliteration,
                         transliteration.namedItem( "enableRussianTransliteration" ) );

        applyBoolOption( c.transliteration.enableGermanTransliteration,
                         transliteration.namedItem( "enableGermanTransliteration" ) );

        applyBoolOption( c.transliteration.enableGreekTransliteration,
                         transliteration.namedItem( "enableGreekTransliteration" ) );

        applyBoolOption( c.transliteration.enableBelarusianTransliteration,
                         transliteration.namedItem( "enableBelarusianTransliteration" ) );

        QDomNode chinese = transliteration.namedItem( "chinese" );

        if ( !chinese.isNull() )
        {
            applyBoolOption( c.transliteration.chinese.enable, chinese.namedItem( "enable" ) );
            applyBoolOption( c.transliteration.chinese.enableSCToTWConversion, chinese.namedItem( "enableSCToTWConversion" ) );
            applyBoolOption( c.transliteration.chinese.enableSCToHKConversion, chinese.namedItem( "enableSCToHKConversion" ) );
            applyBoolOption( c.transliteration.chinese.enableTCToSCConversion, chinese.namedItem( "enableTCToSCConversion" ) );
        }

        QDomNode romaji = transliteration.namedItem( "romaji" );

        if ( !romaji.isNull() )
        {
            applyBoolOption( c.transliteration.romaji.enable, romaji.namedItem( "enable" ) );
            applyBoolOption( c.transliteration.romaji.enableHepburn, romaji.namedItem( "enableHepburn" ) );
            applyBoolOption( c.transliteration.romaji.enableNihonShiki, romaji.namedItem( "enableNihonShiki" ) );
            applyBoolOption( c.transliteration.romaji.enableKunreiShiki, romaji.namedItem( "enableKunreiShiki" ) );
            applyBoolOption( c.transliteration.romaji.enableHiragana, romaji.namedItem( "enableHiragana" ) );
            applyBoolOption( c.transliteration.romaji.enableKatakana, romaji.namedItem( "enableKatakana" ) );
        }
    }
#endif

#ifdef GD_FORVO_API_SUPPORT
    QDomNode forvo = root.namedItem( "forvo" );

    if ( !forvo.isNull() )
    {
        applyBoolOption( c.forvo.enable,
                         forvo.namedItem( "enable" ) );

        c.forvo.apiKey = forvo.namedItem( "apiKey" ).toElement().text();
        c.forvo.languageCodes = forvo.namedItem( "languageCodes" ).toElement().text();
    }
    else
        c.forvo.languageCodes = "en, ru"; // Default demo values
#endif

#ifdef GD_PROGRAM_SUPPORT
    QDomNode programs = root.namedItem( "programs" );

    if ( !programs.isNull() )
    {
        QDomNodeList nl = programs.toElement().elementsByTagName( "program" );

        for( Qt4x5::Dom::size_type x = 0; x < nl.length(); ++x )
        {
            QDomElement pr = nl.item( x ).toElement();

            Program p;

            p.id = pr.attribute( "id" );
            p.name = pr.attribute( "name" );
            p.commandLine = pr.attribute( "commandLine" );
            p.enabled = ( pr.attribute( "enabled" ) == "1" );
            p.type = (Program::Type)( pr.attribute( "type" ).toInt() );
            p.iconFilename = pr.attribute( "icon" );

            c.programs.push_back( p );
        }
    }
    else
        c.programs = makeDefaultPrograms();
#endif

#ifdef GD_MEDIAWIKI_SUPPORT
    QDomNode mws = root.namedItem( "mediawikis" );

    if ( !mws.isNull() )
    {
        QDomNodeList nl = mws.toElement().elementsByTagName( "mediawiki" );

        for( Qt4x5::Dom::size_type x = 0; x < nl.length(); ++x )
        {
            QDomElement mw = nl.item( x ).toElement();

            MediaWiki w;

            w.id = mw.attribute( "id" );
            w.name = mw.attribute( "name" );
            w.url = mw.attribute( "url" );
            w.enabled = ( mw.attribute( "enabled" ) == "1" );
            w.icon = mw.attribute( "icon" );

            c.mediawikis.push_back( w );
        }
    }
    else
    {
        // When upgrading, populate the list with some choices, but don't enable
        // anything.
        c.mediawikis = makeDefaultMediaWikis( false );
    }
#endif

#ifdef GD_WEBSITE_SUPPORT
    QDomNode wss = root.namedItem( "websites" );

    if ( !wss.isNull() )
    {
        QDomNodeList nl = wss.toElement().elementsByTagName( "website" );

        for( Qt4x5::Dom::size_type x = 0; x < nl.length(); ++x )
        {
            QDomElement ws = nl.item( x ).toElement();

            WebSite w;

            w.id = ws.attribute( "id" );
            w.name = ws.attribute( "name" );
            w.url = ws.attribute( "url" );
            w.enabled = ( ws.attribute( "enabled" ) == "1" );
            w.iconFilename = ws.attribute( "icon" );
            w.inside_iframe = ( ws.attribute( "inside_iframe", "1" ) == "1" );

            c.webSites.push_back( w );
        }
    }
    else
    {
        // Upgrading
        c.webSites = makeDefaultWebSites();
    }
#endif

#ifdef GD_DICTSERVER_SUPPORT
    QDomNode dss = root.namedItem( "dictservers" );

    if ( !dss.isNull() )
    {
        QDomNodeList nl = dss.toElement().elementsByTagName( "server" );

        for( Qt4x5::Dom::size_type x = 0; x < nl.length(); ++x )
        {
            QDomElement ds = nl.item( x ).toElement();

            DictServer d;

            d.id = ds.attribute( "id" );
            d.name = ds.attribute( "name" );
            d.url = ds.attribute( "url" );
            d.enabled = ( ds.attribute( "enabled" ) == "1" );
            d.databases = ds.attribute( "databases" );
            d.strategies = ds.attribute( "strategies" );
            d.iconFilename = ds.attribute( "icon" );

            c.dictServers.push_back( d );
        }
    }
    else
    {
        // Upgrading
        c.dictServers = makeDefaultDictServers();
    }
#endif

#ifdef GD_VOICE_ENGINE_SUPPORT
    QDomNode ves = root.namedItem( "voiceEngines" );

    if ( !ves.isNull() )
    {
        QDomNodeList nl = ves.toElement().elementsByTagName( "voiceEngine" );

        for ( Qt4x5::Dom::size_type x = 0; x < nl.length(); ++x )
        {
            QDomElement ve = nl.item( x ).toElement();
            VoiceEngine v;

            v.enabled = ve.attribute( "enabled" ) == "1";
            v.id = ve.attribute( "id" );
            v.name = ve.attribute( "name" );
            v.iconFilename = ve.attribute( "icon" );
            v.volume = ve.attribute( "volume", "50" ).toInt();
            if( v.volume < 0 || v.volume > 100 )
                v.volume = 50;
            v.rate = ve.attribute( "rate", "50" ).toInt();
            if( v.rate < 0 || v.rate > 100 )
                v.rate = 50;
            c.voiceEngines.push_back( v );
        }
    }
#endif

    c.mutedDictionaries = loadMutedDictionaries( root.namedItem( "mutedDictionaries" ) );
    c.popupMutedDictionaries = loadMutedDictionaries( root.namedItem( "popupMutedDictionaries" ) );

    QDomNode preferences = root.namedItem( "preferences" );

    if ( !preferences.isNull() )
    {
        c.preferences.interfaceLanguage = preferences.namedItem( "interfaceLanguage" ).toElement().text();
        c.preferences.helpLanguage = preferences.namedItem( "helpLanguage" ).toElement().text();
        c.preferences.displayStyle = preferences.namedItem( "displayStyle" ).toElement().text();
        c.preferences.newTabsOpenAfterCurrentOne = ( preferences.namedItem( "newTabsOpenAfterCurrentOne" ).toElement().text() == "1" );
        c.preferences.newTabsOpenInBackground = ( preferences.namedItem( "newTabsOpenInBackground" ).toElement().text() == "1" );
        c.preferences.hideSingleTab = ( preferences.namedItem( "hideSingleTab" ).toElement().text() == "1" );
        c.preferences.mruTabOrder = ( preferences.namedItem( "mruTabOrder" ).toElement().text() == "1" );
        c.preferences.hideMenubar = ( preferences.namedItem( "hideMenubar" ).toElement().text() == "1" );
        c.preferences.enableTrayIcon = ( preferences.namedItem( "enableTrayIcon" ).toElement().text() == "1" );
        c.preferences.startToTray = ( preferences.namedItem( "startToTray" ).toElement().text() == "1" );
        c.preferences.closeToTray = ( preferences.namedItem( "closeToTray" ).toElement().text() == "1" );
        c.preferences.autoStart = ( preferences.namedItem( "autoStart" ).toElement().text() == "1" );
        c.preferences.alwaysOnTop = ( preferences.namedItem( "alwaysOnTop" ).toElement().text() == "1" );
        c.preferences.searchInDock = ( preferences.namedItem( "searchInDock" ).toElement().text() == "1" );

        if ( !preferences.namedItem( "doubleClickTranslates" ).isNull() )
            c.preferences.doubleClickTranslates = ( preferences.namedItem( "doubleClickTranslates" ).toElement().text() == "1" );

        if ( !preferences.namedItem( "selectWordBySingleClick" ).isNull() )
            c.preferences.selectWordBySingleClick = ( preferences.namedItem( "selectWordBySingleClick" ).toElement().text() == "1" );

        if ( !preferences.namedItem( "escKeyHidesMainWindow" ).isNull() )
            c.preferences.escKeyHidesMainWindow = ( preferences.namedItem( "escKeyHidesMainWindow" ).toElement().text() == "1" );

        if ( !preferences.namedItem( "zoomFactor" ).isNull() )
            c.preferences.zoomFactor = preferences.namedItem( "zoomFactor" ).toElement().text().toDouble();

        if ( !preferences.namedItem( "helpZoomFactor" ).isNull() )
            c.preferences.helpZoomFactor = preferences.namedItem( "helpZoomFactor" ).toElement().text().toDouble();

        if ( !preferences.namedItem( "wordsZoomLevel" ).isNull() )
            c.preferences.wordsZoomLevel = preferences.namedItem( "wordsZoomLevel" ).toElement().text().toInt();

        applyBoolOption( c.preferences.enableMainWindowHotkey, preferences.namedItem( "enableMainWindowHotkey" ) );
        if ( !preferences.namedItem( "mainWindowHotkey" ).isNull() )
            c.preferences.mainWindowHotkey = QKeySequence::fromString( preferences.namedItem( "mainWindowHotkey" ).toElement().text() );
        applyBoolOption( c.preferences.enableClipboardHotkey, preferences.namedItem( "enableClipboardHotkey" ) );
        if ( !preferences.namedItem( "clipboardHotkey" ).isNull() )
            c.preferences.clipboardHotkey = QKeySequence::fromString( preferences.namedItem( "clipboardHotkey" ).toElement().text() );

        c.preferences.enableScanPopup = ( preferences.namedItem( "enableScanPopup" ).toElement().text() == "1" );
        c.preferences.startWithScanPopupOn = ( preferences.namedItem( "startWithScanPopupOn" ).toElement().text() == "1" );
        c.preferences.enableScanPopupModifiers = ( preferences.namedItem( "enableScanPopupModifiers" ).toElement().text() == "1" );
        c.preferences.scanPopupModifiers = ( preferences.namedItem( "scanPopupModifiers" ).toElement().text().toULong() );
        c.preferences.scanPopupAltMode = ( preferences.namedItem( "scanPopupAltMode" ).toElement().text() == "1" );
        if ( !preferences.namedItem( "scanPopupAltModeSecs" ).isNull() )
            c.preferences.scanPopupAltModeSecs = preferences.namedItem( "scanPopupAltModeSecs" ).toElement().text().toUInt();
        c.preferences.ignoreOwnClipboardChanges = ( preferences.namedItem( "ignoreOwnClipboardChanges" ).toElement().text() == "1" );
        c.preferences.scanToMainWindow = ( preferences.namedItem( "scanToMainWindow" ).toElement().text() == "1" );
        c.preferences.ignoreDiacritics = ( preferences.namedItem( "ignoreDiacritics" ).toElement().text() == "1" );
#ifdef HAVE_X11
        c.preferences.showScanFlag= ( preferences.namedItem( "showScanFlag" ).toElement().text() == "1" );
#endif
        c.preferences.scanPopupUseUIAutomation = ( preferences.namedItem( "scanPopupUseUIAutomation" ).toElement().text() == "1" );
        c.preferences.scanPopupUseIAccessibleEx = ( preferences.namedItem( "scanPopupUseIAccessibleEx" ).toElement().text() == "1" );
        c.preferences.scanPopupUseGDMessage = ( preferences.namedItem( "scanPopupUseGDMessage" ).toElement().text() == "1" );
        c.preferences.scanPopupUnpinnedWindowFlags = spwfFromInt( preferences.namedItem( "scanPopupUnpinnedWindowFlags" ).toElement().text().toInt() );
        c.preferences.scanPopupUnpinnedBypassWMHint = ( preferences.namedItem( "scanPopupUnpinnedBypassWMHint" ).toElement().text() == "1" );

        c.preferences.pronounceOnLoadMain = ( preferences.namedItem( "pronounceOnLoadMain" ).toElement().text() == "1" );
        c.preferences.pronounceOnLoadPopup = ( preferences.namedItem( "pronounceOnLoadPopup" ).toElement().text() == "1" );

        if ( InternalPlayerBackend::anyAvailable() )
        {
            if ( !preferences.namedItem( "useInternalPlayer" ).isNull() )
                c.preferences.useInternalPlayer = ( preferences.namedItem( "useInternalPlayer" ).toElement().text() == "1" );
        }
        else
            c.preferences.useInternalPlayer = false;

        if ( !preferences.namedItem( "internalPlayerBackend" ).isNull() )
            c.preferences.internalPlayerBackend.setUiName( preferences.namedItem( "internalPlayerBackend" ).toElement().text() );

        if ( !preferences.namedItem( "audioPlaybackProgram" ).isNull() )
            c.preferences.audioPlaybackProgram = preferences.namedItem( "audioPlaybackProgram" ).toElement().text();
        else
            c.preferences.audioPlaybackProgram = "mplayer";

        QDomNode proxy = preferences.namedItem( "proxyserver" );

        if ( !proxy.isNull() )
        {
            c.preferences.proxyServer.enabled = ( proxy.toElement().attribute( "enabled" ) == "1" );
            c.preferences.proxyServer.useSystemProxy = ( proxy.toElement().attribute( "useSystemProxy" ) == "1" );
            c.preferences.proxyServer.type = ( ProxyServer::Type ) proxy.namedItem( "type" ).toElement().text().toULong();
            c.preferences.proxyServer.host = proxy.namedItem( "host" ).toElement().text();
            c.preferences.proxyServer.port = proxy.namedItem( "port" ).toElement().text().toULong();
            c.preferences.proxyServer.user = proxy.namedItem( "user" ).toElement().text();
            c.preferences.proxyServer.password = proxy.namedItem( "password" ).toElement().text();
            c.preferences.proxyServer.systemProxyUser = proxy.namedItem( "systemProxyUser" ).toElement().text();
            c.preferences.proxyServer.systemProxyPassword = proxy.namedItem( "systemProxyPassword" ).toElement().text();
        }

        if ( !preferences.namedItem( "checkForNewReleases" ).isNull() )
            c.preferences.checkForNewReleases = ( preferences.namedItem( "checkForNewReleases" ).toElement().text() == "1" );

        if ( !preferences.namedItem( "disallowContentFromOtherSites" ).isNull() )
            c.preferences.disallowContentFromOtherSites = ( preferences.namedItem( "disallowContentFromOtherSites" ).toElement().text() == "1" );

        if ( !preferences.namedItem( "enableWebPlugins" ).isNull() )
            c.preferences.enableWebPlugins = ( preferences.namedItem( "enableWebPlugins" ).toElement().text() == "1" );

        if ( !preferences.namedItem( "hideGoldenDictHeader" ).isNull() )
            c.preferences.hideGoldenDictHeader = ( preferences.namedItem( "hideGoldenDictHeader" ).toElement().text() == "1" );

        if ( !preferences.namedItem( "maxStringsInHistory" ).isNull() )
            c.preferences.maxStringsInHistory = preferences.namedItem( "maxStringsInHistory" ).toElement().text().toUInt() ;

        if ( !preferences.namedItem( "storeHistory" ).isNull() )
            c.preferences.storeHistory = preferences.namedItem( "storeHistory" ).toElement().text().toUInt() ;

        if ( !preferences.namedItem( "alwaysExpandOptionalParts" ).isNull() )
            c.preferences.alwaysExpandOptionalParts = preferences.namedItem( "alwaysExpandOptionalParts" ).toElement().text().toUInt() ;

        if ( !preferences.namedItem( "addonStyle" ).isNull() )
            c.preferences.addonStyle = preferences.namedItem( "addonStyle" ).toElement().text();

        if ( !preferences.namedItem( "historyStoreInterval" ).isNull() )
            c.preferences.historyStoreInterval = preferences.namedItem( "historyStoreInterval" ).toElement().text().toUInt() ;

        if ( !preferences.namedItem( "favoritesStoreInterval" ).isNull() )
            c.preferences.favoritesStoreInterval = preferences.namedItem( "favoritesStoreInterval" ).toElement().text().toUInt() ;

        if ( !preferences.namedItem( "confirmFavoritesDeletion" ).isNull() )
            c.preferences.confirmFavoritesDeletion = ( preferences.namedItem( "confirmFavoritesDeletion" ).toElement().text() == "1" );

        if ( !preferences.namedItem( "collapseBigArticles" ).isNull() )
            c.preferences.collapseBigArticles = ( preferences.namedItem( "collapseBigArticles" ).toElement().text() == "1" );

        if ( !preferences.namedItem( "articleSizeLimit" ).isNull() )
            c.preferences.articleSizeLimit = preferences.namedItem( "articleSizeLimit" ).toElement().text().toUInt() ;

        if ( !preferences.namedItem( "maxDictionaryRefsInContextMenu" ).isNull() )
            c.preferences.maxDictionaryRefsInContextMenu = preferences.namedItem( "maxDictionaryRefsInContextMenu" ).toElement().text().toUShort();

#ifndef Q_WS_X11
        if ( !preferences.namedItem( "trackClipboardChanges" ).isNull() )
            c.preferences.trackClipboardChanges = ( preferences.namedItem( "trackClipboardChanges" ).toElement().text() == "1" );
#endif

        if ( !preferences.namedItem( "synonymSearchEnabled" ).isNull() )
            c.preferences.synonymSearchEnabled = ( preferences.namedItem( "synonymSearchEnabled" ).toElement().text() == "1" );

        QDomNode fts = preferences.namedItem( "fullTextSearch" );

        if ( !fts.isNull() )
        {
            if ( !fts.namedItem( "searchMode" ).isNull() )
                c.preferences.fts.searchMode = fts.namedItem( "searchMode" ).toElement().text().toInt();

            if ( !fts.namedItem( "matchCase" ).isNull() )
                c.preferences.fts.matchCase = ( fts.namedItem( "matchCase" ).toElement().text() == "1" );

            if ( !fts.namedItem( "maxArticlesPerDictionary" ).isNull() )
                c.preferences.fts.maxArticlesPerDictionary = fts.namedItem( "maxArticlesPerDictionary" ).toElement().text().toInt();

            if ( !fts.namedItem( "maxDistanceBetweenWords" ).isNull() )
                c.preferences.fts.maxDistanceBetweenWords = fts.namedItem( "maxDistanceBetweenWords" ).toElement().text().toInt();

            if ( !fts.namedItem( "useMaxArticlesPerDictionary" ).isNull() )
                c.preferences.fts.useMaxArticlesPerDictionary = ( fts.namedItem( "useMaxArticlesPerDictionary" ).toElement().text() == "1" );

            if ( !fts.namedItem( "useMaxDistanceBetweenWords" ).isNull() )
                c.preferences.fts.useMaxDistanceBetweenWords = ( fts.namedItem( "useMaxDistanceBetweenWords" ).toElement().text() == "1" );

            if ( !fts.namedItem( "dialogGeometry" ).isNull() )
                c.preferences.fts.dialogGeometry = QByteArray::fromBase64( fts.namedItem( "dialogGeometry" ).toElement().text().toLatin1() );

            if( !fts.namedItem( "disabledTypes" ).isNull() )
                c.preferences.fts.disabledTypes = fts.namedItem( "disabledTypes" ).toElement().text();

            if ( !fts.namedItem( "enabled" ).isNull() )
                c.preferences.fts.enabled = ( fts.namedItem( "enabled" ).toElement().text() == "1" );

            if ( !fts.namedItem( "ignoreWordsOrder" ).isNull() )
                c.preferences.fts.ignoreWordsOrder = ( fts.namedItem( "ignoreWordsOrder" ).toElement().text() == "1" );

            if ( !fts.namedItem( "ignoreDiacritics" ).isNull() )
                c.preferences.fts.ignoreDiacritics = ( fts.namedItem( "ignoreDiacritics" ).toElement().text() == "1" );

            if ( !fts.namedItem( "maxDictionarySize" ).isNull() )
                c.preferences.fts.maxDictionarySize = fts.namedItem( "maxDictionarySize" ).toElement().text().toUInt();
        }

    }

    c.lastMainGroupId = root.namedItem( "lastMainGroupId" ).toElement().text().toUInt();
    c.lastPopupGroupId = root.namedItem( "lastPopupGroupId" ).toElement().text().toUInt();

    QDomNode popupWindowState = root.namedItem( "popupWindowState" );

    if ( !popupWindowState.isNull() )
        c.popupWindowState = QByteArray::fromBase64( popupWindowState.toElement().text().toLatin1() );

    QDomNode popupWindowGeometry = root.namedItem( "popupWindowGeometry" );

    if ( !popupWindowGeometry.isNull() )
        c.popupWindowGeometry = QByteArray::fromBase64( popupWindowGeometry.toElement().text().toLatin1() );

    c.pinPopupWindow = ( root.namedItem( "pinPopupWindow" ).toElement().text() == "1" );

    c.popupWindowAlwaysOnTop = ( root.namedItem( "popupWindowAlwaysOnTop" ).toElement().text() == "1" );

    QDomNode mainWindowState = root.namedItem( "mainWindowState" );

    if ( !mainWindowState.isNull() )
        c.mainWindowState = QByteArray::fromBase64( mainWindowState.toElement().text().toLatin1() );

    QDomNode mainWindowGeometry = root.namedItem( "mainWindowGeometry" );

    if ( !mainWindowGeometry.isNull() )
        c.mainWindowGeometry = QByteArray::fromBase64( mainWindowGeometry.toElement().text().toLatin1() );

    QDomNode helpWindowGeometry = root.namedItem( "helpWindowGeometry" );

    if ( !helpWindowGeometry.isNull() )
        c.helpWindowGeometry = QByteArray::fromBase64( helpWindowGeometry.toElement().text().toLatin1() );

    QDomNode helpSplitterState = root.namedItem( "helpSplitterState" );

    if ( !helpSplitterState.isNull() )
        c.helpSplitterState = QByteArray::fromBase64( helpSplitterState.toElement().text().toLatin1() );

#ifdef Q_OS_WIN
    QDomNode maximizedMainWindowGeometry = root.namedItem( "maximizedMainWindowGeometry" );

    if ( !maximizedMainWindowGeometry.isNull() )
    {
        int x = 0, y = 0, width = 0, height = 0;
        if( !maximizedMainWindowGeometry.namedItem( "x" ).isNull() )
            x = maximizedMainWindowGeometry.namedItem( "x" ).toElement().text().toInt();
        if( !maximizedMainWindowGeometry.namedItem( "y" ).isNull() )
            y = maximizedMainWindowGeometry.namedItem( "y" ).toElement().text().toInt();
        if( !maximizedMainWindowGeometry.namedItem( "width" ).isNull() )
            width = maximizedMainWindowGeometry.namedItem( "width" ).toElement().text().toInt();
        if( !maximizedMainWindowGeometry.namedItem( "height" ).isNull() )
            height = maximizedMainWindowGeometry.namedItem( "height" ).toElement().text().toInt();
        c.maximizedMainWindowGeometry = QRect( x, y, width, height );
    }

    QDomNode normalMainWindowGeometry = root.namedItem( "normalMainWindowGeometry" );

    if ( !normalMainWindowGeometry.isNull() )
    {
        int x = 0, y = 0, width = 0, height = 0;
        if( !normalMainWindowGeometry.namedItem( "x" ).isNull() )
            x = normalMainWindowGeometry.namedItem( "x" ).toElement().text().toInt();
        if( !normalMainWindowGeometry.namedItem( "y" ).isNull() )
            y = normalMainWindowGeometry.namedItem( "y" ).toElement().text().toInt();
        if( !normalMainWindowGeometry.namedItem( "width" ).isNull() )
            width = normalMainWindowGeometry.namedItem( "width" ).toElement().text().toInt();
        if( !normalMainWindowGeometry.namedItem( "height" ).isNull() )
            height = normalMainWindowGeometry.namedItem( "height" ).toElement().text().toInt();
        c.normalMainWindowGeometry = QRect( x, y, width, height );
    }
#endif

    QDomNode dictInfoGeometry = root.namedItem( "dictInfoGeometry" );

    if ( !dictInfoGeometry.isNull() )
        c.dictInfoGeometry = QByteArray::fromBase64( dictInfoGeometry.toElement().text().toLatin1() );

    QDomNode inspectorGeometry = root.namedItem( "inspectorGeometry" );

    if ( !inspectorGeometry.isNull() )
        c.inspectorGeometry = QByteArray::fromBase64( inspectorGeometry.toElement().text().toLatin1() );

    QDomNode timeForNewReleaseCheck = root.namedItem( "timeForNewReleaseCheck" );

    if ( !timeForNewReleaseCheck.isNull() )
        c.timeForNewReleaseCheck = QDateTime::fromString( timeForNewReleaseCheck.toElement().text(),
                                                          Qt::ISODate );

    c.skippedRelease = root.namedItem( "skippedRelease" ).toElement().text();

    c.showingDictBarNames = ( root.namedItem( "showingDictBarNames" ).toElement().text() == "1" );

    c.usingSmallIconsInToolbars = ( root.namedItem( "usingSmallIconsInToolbars" ).toElement().text() == "1" );

    if ( !root.namedItem( "historyExportPath" ).isNull() )
        c.historyExportPath = root.namedItem( "historyExportPath" ).toElement().text();

    if ( !root.namedItem( "resourceSavePath" ).isNull() )
        c.resourceSavePath = root.namedItem( "resourceSavePath" ).toElement().text();

    if ( !root.namedItem( "articleSavePath" ).isNull() )
        c.articleSavePath = root.namedItem( "articleSavePath" ).toElement().text();

    if ( !root.namedItem( "editDictionaryCommandLine" ).isNull() )
        c.editDictionaryCommandLine = root.namedItem( "editDictionaryCommandLine" ).toElement().text();

    if ( !root.namedItem( "maxPictureWidth" ).isNull() )
        c.maxPictureWidth = root.namedItem( "maxPictureWidth" ).toElement().text().toInt();

    if ( !root.namedItem( "maxHeadwordSize" ).isNull() )
    {
        unsigned int value = root.namedItem( "maxHeadwordSize" ).toElement().text().toUInt();
        if ( value != 0 ) // 0 is invalid value for our purposes
        {
            c.maxHeadwordSize = value;
        }
    }

    if ( !root.namedItem( "maxHeadwordsToExpand" ).isNull() )
        c.maxHeadwordsToExpand = root.namedItem( "maxHeadwordsToExpand" ).toElement().text().toUInt();

    QDomNode headwordsDialog = root.namedItem( "headwordsDialog" );

    if ( !headwordsDialog.isNull() )
    {
        if ( !headwordsDialog.namedItem( "searchMode" ).isNull() )
            c.headwordsDialog.searchMode = headwordsDialog.namedItem( "searchMode" ).toElement().text().toInt();

        if ( !headwordsDialog.namedItem( "matchCase" ).isNull() )
            c.headwordsDialog.matchCase = ( headwordsDialog.namedItem( "matchCase" ).toElement().text() == "1" );

        if ( !headwordsDialog.namedItem( "autoApply" ).isNull() )
            c.headwordsDialog.autoApply = ( headwordsDialog.namedItem( "autoApply" ).toElement().text() == "1" );

        if ( !headwordsDialog.namedItem( "headwordsExportPath" ).isNull() )
            c.headwordsDialog.headwordsExportPath = headwordsDialog.namedItem( "headwordsExportPath" ).toElement().text();

        if ( !headwordsDialog.namedItem( "headwordsDialogGeometry" ).isNull() )
            c.headwordsDialog.headwordsDialogGeometry = QByteArray::fromBase64( headwordsDialog.namedItem( "headwordsDialogGeometry" ).toElement().text().toLatin1() );
    }

    return c;
}

namespace {
void saveGroup( Group const & data, QDomElement & group )
{
    QDomDocument dd = group.ownerDocument();

    QDomAttr id = dd.createAttribute( "id" );

    id.setValue( QString::number( data.id ) );

    group.setAttributeNode( id );

    QDomAttr name = dd.createAttribute( "name" );

    name.setValue( data.name );

    group.setAttributeNode( name );

    if( data.favoritesFolder.size() )
    {
        QDomAttr folder = dd.createAttribute( "favoritesFolder" );
        folder.setValue( data.favoritesFolder );
        group.setAttributeNode( folder );
    }

    if ( data.icon.size() )
    {
        QDomAttr icon = dd.createAttribute( "icon" );

        icon.setValue( data.icon );

        group.setAttributeNode( icon );
    }

    if ( data.iconData.size() )
    {
        QDomAttr iconData = dd.createAttribute( "iconData" );

        iconData.setValue( QString::fromLatin1( data.iconData.toBase64() ) );

        group.setAttributeNode( iconData );
    }

    if ( !data.shortcut.isEmpty() )
    {
        QDomAttr shortcut = dd.createAttribute( "shortcut" );

        shortcut.setValue(  data.shortcut.toString() );

        group.setAttributeNode( shortcut );
    }

    for( QVector< DictionaryRef >::const_iterator j = data.dictionaries.begin(); j != data.dictionaries.end(); ++j )
    {
        QDomElement dictionary = dd.createElement( "dictionary" );

        group.appendChild( dictionary );

        QDomText value = dd.createTextNode( j->id );

        dictionary.appendChild( value );

        QDomAttr name = dd.createAttribute( "name" );

        name.setValue( j->name );

        dictionary.setAttributeNode( name );
    }

    QDomElement muted = dd.createElement( "mutedDictionaries" );
    group.appendChild( muted );

    for( MutedDictionaries::const_iterator i = data.mutedDictionaries.begin();
         i != data.mutedDictionaries.end(); ++i )
    {
        QDomElement dict = dd.createElement( "mutedDictionary" );
        muted.appendChild( dict );

        QDomText value = dd.createTextNode( *i );
        dict.appendChild( value );
    }

    for( MutedDictionaries::const_iterator i = data.popupMutedDictionaries.begin();
         i != data.popupMutedDictionaries.end(); ++i )
    {
        QDomElement dict = dd.createElement( "popupMutedDictionary" );
        muted.appendChild( dict );

        QDomText value = dd.createTextNode( *i );
        dict.appendChild( value );
    }
}

}
#endif

#ifdef GD_PUGIXML_XSERIAL
void Path::serial(pugi::xml_node &node, const bool read)
{
    if(read)
    {
        XEC_R(node, recursive);
        XEC_R(node, path);
    }
    else
    {
        XEC_W(node, recursive);
        XEC_W(node, path);
    }
}

#ifdef GD_SOUND_DIRS_SUPPORT
void SoundDir::serial(pugi::xml_node &node, const bool read)
{
    if(read)
    {
        XEC_R(node, path);
        XEC_R(node, name);
        XEC_R(node, iconFilename);
    }
    else
    {
        XEC_W(node, path);
        XEC_W(node, name);
        XEC_W(node, iconFilename);
    }
}
#endif

void DictionaryRef::serial(pugi::xml_node &dict, const bool read)
{
    if(read)
    {
        XEC_R(dict, name);
        XEC_R(dict, id);
    }
    else
    {
        XEC_W(dict, name);
        XEC_W(dict, id);
    }
}

void Group::serial(pugi::xml_node &group, const bool read)
{
    if(read)
    {
        XEC_R(group, id);
        XEC_R(group, name);
        XEC_R(group, favoritesFolder);
        XEC_R(group, icon);
        XEC_R(group, iconData);
        QString sT;
        XEC_R_RAW(group, sT, "shortcut");
        shortcut = QKeySequence::fromString(sT);
    }
    else
    {
        XEC_W(group, id);
        XEC_W(group, name);
        XEC_W(group, favoritesFolder);
        XEC_W(group, icon);
        XEC_W(group, iconData);
        XEC_W_RAW(group, shortcut.toString(), "shortcut");
    }

    XO_STDARRAY(group, dictionaries, DictionaryRef, read);

    XE_STDSET(group, mutedDictionaries, QString, read);
    //unsigned int csize = mutedDictionaries.size();
    //XE_STLITERATOR_RAW(group, mutedDictionaries.toList().begin(), csize, false, "mutedDictionaries");

    XE_STDSET(group, popupMutedDictionaries, QString, read);
    //  csize = popupMutedDictionaries.size();
    //  XE_STLITERATOR_RAW(group, popupMutedDictionaries.toList().begin(), csize, false, "popupMutedDictionaries");
}

void Groups::serial(pugi::xml_node &parent, const bool read)
{
    if(read)
        XEC_R(parent, nextId);
    else
        XEC_W(parent, nextId);
    XO_STDARRAY_RAW(parent, *this, Group, "this", read);
}

void ProxyServer::serial(pugi::xml_node &parent, const bool read)
{
    if(read)
    {
        XEC_R(parent, enabled);
        XEC_R(parent, useSystemProxy);
        int iT = 0;
        XEC_R_RAW(parent, iT, "type"); type = (Type)iT;
        XEC_R(parent, host);
        XEC_R(parent, port);
        XEC_R(parent, user);
        XEC_R(parent, password);
        XEC_R(parent, systemProxyUser);
        XEC_R(parent, systemProxyPassword);
    }
    else
    {
        XEC_W(parent, enabled);
        XEC_W(parent, useSystemProxy);
        XEC_W(parent, type);
        XEC_W(parent, host);
        XEC_W(parent, port);
        XEC_W(parent, user);
        XEC_W(parent, password);
        XEC_W(parent, systemProxyUser);
        XEC_W(parent, systemProxyPassword);
    }
}

void FullTextSearch::serial(pugi::xml_node &parent, const bool read)
{
    if(read)
    {
        XEC_R(parent, searchMode);
        XEC_R(parent, matchCase);
        XEC_R(parent, maxArticlesPerDictionary);
        XEC_R(parent, maxDistanceBetweenWords);
        XEC_R(parent, useMaxArticlesPerDictionary);
        XEC_R(parent, useMaxDistanceBetweenWords);
        XEC_R(parent, dialogGeometry);
        XEC_R(parent, disabledTypes);
        XEC_R(parent, enabled);
        XEC_R(parent, ignoreWordsOrder);
        XEC_R(parent, ignoreDiacritics);
        XEC_R(parent, maxDictionarySize);
    }
    else
    {
        XEC_W(parent, searchMode);
        XEC_W(parent, matchCase);
        XEC_W(parent, maxArticlesPerDictionary);
        XEC_W(parent, maxDistanceBetweenWords);
        XEC_W(parent, useMaxArticlesPerDictionary);
        XEC_W(parent, useMaxDistanceBetweenWords);
        XEC_W(parent, dialogGeometry);
        XEC_W(parent, disabledTypes);
        XEC_W(parent, enabled);
        XEC_W(parent, ignoreWordsOrder);
        XEC_W(parent, ignoreDiacritics);
        XEC_W(parent, maxDictionarySize);
    }
}

void Preferences::serial(pugi::xml_node &parent, const bool read)
{

    if(read)
    {
        XEC_R(parent, interfaceLanguage);
        XEC_R(parent, helpLanguage);
        XEC_R(parent, displayStyle);
        XEC_R(parent, newTabsOpenAfterCurrentOne);
        XEC_R(parent, newTabsOpenInBackground);
        XEC_R(parent, hideSingleTab);
        XEC_R(parent, mruTabOrder);
        XEC_R(parent, hideMenubar);
        XEC_R(parent, enableTrayIcon);
        XEC_R(parent, startToTray);
        XEC_R(parent, closeToTray);
        XEC_R(parent, autoStart);
        XEC_R(parent, doubleClickTranslates);
        XEC_R(parent, selectWordBySingleClick);
        XEC_R(parent, escKeyHidesMainWindow);
        XEC_R(parent, zoomFactor);
        XEC_R(parent, helpZoomFactor);
        XEC_R(parent, wordsZoomLevel);

        QString tStr;
        XEC_R(parent, enableMainWindowHotkey);
        XEC_R_RAW(parent, tStr, "mainWindowHotkey");
        mainWindowHotkey = QKeySequence::fromString(tStr);

        XEC_R(parent, enableClipboardHotkey);
        XEC_R_RAW(parent, tStr, "clipboardHotkey");
        clipboardHotkey = QKeySequence::fromString(tStr);

        XEC_R(parent, enableScanPopup);
        XEC_R(parent, startWithScanPopupOn);
        XEC_R(parent, enableScanPopupModifiers);
        XEC_R(parent, scanPopupModifiers);
        XEC_R(parent, scanPopupAltMode);
        XEC_R(parent, scanPopupAltModeSecs);
        XEC_R(parent, ignoreOwnClipboardChanges);
        XEC_R(parent, scanToMainWindow);
        XEC_R(parent, ignoreDiacritics);

#ifdef HAVE_X11
        XEC_R(parent, showScanFlag);
#endif

        XEC_R(parent, scanPopupUseUIAutomation);
        XEC_R(parent, scanPopupUseIAccessibleEx);
        XEC_R(parent, scanPopupUseGDMessage);

        int iT = 0;
        XEC_R_RAW(parent, iT, "scanPopupUnpinnedWindowFlags");
        scanPopupUnpinnedWindowFlags = spwfFromInt(iT);

        XEC_R(parent, scanPopupUnpinnedBypassWMHint);

        XEC_R(parent, pronounceOnLoadMain);
        XEC_R(parent, pronounceOnLoadPopup);

        XEC_R(parent, useInternalPlayer);
        XEC_R_RAW(parent, tStr, "internalPlayerBackend");
        internalPlayerBackend.setUiName(tStr);

        XEC_R(parent, audioPlaybackProgram);
        XEC_R(parent, alwaysOnTop);
        XEC_R(parent, searchInDock);
        XEC_R(parent, historyStoreInterval);
        XEC_R(parent, favoritesStoreInterval);
        XEC_R(parent, confirmFavoritesDeletion);

        XEC_R(parent, checkForNewReleases);
        XEC_R(parent, disallowContentFromOtherSites);
        XEC_R(parent, enableWebPlugins);
        XEC_R(parent, hideGoldenDictHeader);
        XEC_R(parent, maxStringsInHistory);
        XEC_R(parent, storeHistory);
        XEC_R(parent, alwaysExpandOptionalParts);
        XEC_R(parent, addonStyle);
        XEC_R(parent, collapseBigArticles);
        XEC_R(parent, articleSizeLimit);

        XEC_R(parent, maxDictionaryRefsInContextMenu);
#ifndef Q_WS_X11
        XEC_R(parent, trackClipboardChanges);
#endif
        XEC_R(parent, synonymSearchEnabled);

        XEC_R(parent, articleSizeLimit);
        XEC_R(parent, articleSizeLimit);
        XEC_R(parent, articleSizeLimit);
    }
    else
    {
        XEC_W(parent, interfaceLanguage);
        XEC_W(parent, helpLanguage);
        XEC_W(parent, displayStyle);
        XEC_W(parent, newTabsOpenAfterCurrentOne);
        XEC_W(parent, newTabsOpenInBackground);
        XEC_W(parent, hideSingleTab);
        XEC_W(parent, mruTabOrder);
        XEC_W(parent, hideMenubar);
        XEC_W(parent, enableTrayIcon);
        XEC_W(parent, startToTray);
        XEC_W(parent, closeToTray);
        XEC_W(parent, autoStart);
        XEC_W(parent, doubleClickTranslates);
        XEC_W(parent, selectWordBySingleClick);
        XEC_W(parent, escKeyHidesMainWindow);
        XEC_W(parent, zoomFactor);
        XEC_W(parent, helpZoomFactor);
        XEC_W(parent, wordsZoomLevel);

        XEC_W(parent, enableMainWindowHotkey);
        XEC_W_RAW(parent, mainWindowHotkey.toKeySequence().toString(), "mainWindowHotkey");

        XEC_W(parent, enableClipboardHotkey);
        XEC_W_RAW(parent, clipboardHotkey.toKeySequence().toString(), "clipboardHotkey");

        XEC_W(parent, enableScanPopup);
        XEC_W(parent, startWithScanPopupOn);
        XEC_W(parent, enableScanPopupModifiers);
        XEC_W(parent, scanPopupModifiers);
        XEC_W(parent, scanPopupAltMode);
        XEC_W(parent, scanPopupAltModeSecs);
        XEC_W(parent, ignoreOwnClipboardChanges);
        XEC_W(parent, scanToMainWindow);
        XEC_W(parent, ignoreDiacritics);

#ifdef HAVE_X11
        XEC_W(parent, showScanFlag);
#endif

        XEC_W(parent, scanPopupUseUIAutomation);
        XEC_W(parent, scanPopupUseIAccessibleEx);
        XEC_W(parent, scanPopupUseGDMessage);
        XEC_W(parent, scanPopupUnpinnedWindowFlags);
        XEC_W(parent, scanPopupUnpinnedBypassWMHint);

        XEC_W(parent, pronounceOnLoadMain);
        XEC_W(parent, pronounceOnLoadPopup);
        XEC_W(parent, useInternalPlayer);
        XEC_W_RAW(parent, internalPlayerBackend.uiName(), "internalPlayerBackend");
        XEC_W(parent, audioPlaybackProgram);
        XEC_W(parent, alwaysOnTop);
        XEC_W(parent, searchInDock);
        XEC_W(parent, historyStoreInterval);
        XEC_W(parent, favoritesStoreInterval);
        XEC_W(parent, confirmFavoritesDeletion);

        XEC_W(parent, checkForNewReleases);
        XEC_W(parent, disallowContentFromOtherSites);
        XEC_W(parent, enableWebPlugins);
        XEC_W(parent, hideGoldenDictHeader);
        XEC_W(parent, maxStringsInHistory);
        XEC_W(parent, storeHistory);
        XEC_W(parent, alwaysExpandOptionalParts);
        XEC_W(parent, addonStyle);
        XEC_W(parent, collapseBigArticles);
        XEC_W(parent, articleSizeLimit);

        XEC_W(parent, maxDictionaryRefsInContextMenu);
#ifndef Q_WS_X11
        XEC_W(parent, trackClipboardChanges);
#endif
        XEC_W(parent, synonymSearchEnabled);

        XEC_W(parent, articleSizeLimit);
        XEC_W(parent, articleSizeLimit);
        XEC_W(parent, articleSizeLimit);
    }

    pugi::xml_node xn;
    proxyServer.serial(xn = XO_NODE(parent, ProxyServer, proxyServer), read);
    fts.serial(xn = XO_NODE(parent, FullTextSearch, fts), read);
}

#ifdef GD_HUNSPELL_SUPPORT
void Hunspell::serial(pugi::xml_node &parent, const bool read)
{
    if(read)
        XEC_R(parent, dictionariesPath);
    else
        XEC_W(parent, dictionariesPath);
    XE_STDARRAY(parent, enabledDictionaries, QString, read);
    //unsigned int csize = enabledDictionaries.size();
    //XE_CARRAY(parent, enabledDictionaries, csize, false);
}
#endif

#ifdef GD_TRANSLITERATION_SUPPORT
void Chinese::serial(pugi::xml_node &parent, const bool read)
{
    if(read)
    {
        XEC_R(parent, enable);
        XEC_R(parent, enableSCToTWConversion);
        XEC_R(parent, enableSCToHKConversion);
        XEC_R(parent, enableTCToSCConversion);
    }
    else
    {
        XEC_W(parent, enable);
        XEC_W(parent, enableSCToTWConversion);
        XEC_W(parent, enableSCToHKConversion);
        XEC_W(parent, enableTCToSCConversion);
    }
}
void Romaji::serial(pugi::xml_node &parent, const bool read)
{
    if(read)
    {
        XEC_R(parent, enable);
        XEC_R(parent, enableHepburn);
        XEC_R(parent, enableNihonShiki);
        XEC_R(parent, enableKunreiShiki);
        XEC_R(parent, enableHiragana);
        XEC_R(parent, enableKatakana);
    }
    else
    {
        XEC_W(parent, enable);
        XEC_W(parent, enableHepburn);
        XEC_W(parent, enableNihonShiki);
        XEC_W(parent, enableKunreiShiki);
        XEC_W(parent, enableHiragana);
        XEC_W(parent, enableKatakana);
    }
}
void Transliteration::serial(pugi::xml_node &parent, const bool read)
{
    if(read)
    {
        XEC_R(parent, enableRussianTransliteration);
        XEC_R(parent, enableGermanTransliteration);
        XEC_R(parent, enableGreekTransliteration);
        XEC_R(parent, enableBelarusianTransliteration);
    }
    else
    {
        XEC_W(parent, enableRussianTransliteration);
        XEC_W(parent, enableGermanTransliteration);
        XEC_W(parent, enableGreekTransliteration);
        XEC_W(parent, enableBelarusianTransliteration);
    }
    pugi::xml_node xn;
    chinese.serial(xn = XO_NODE(parent, Chinese, chinese), read);
    romaji.serial(xn = XO_NODE(parent, Romaji, romaji), read);
}
#endif

#ifdef GD_FORVO_API_SUPPORT
void Forvo::serial(pugi::xml_node &parent, const bool read)
{
    if(read)
    {
        XEC_R(parent, enable);
        XEC_R(parent, apiKey);
        XEC_R(parent, languageCodes);
    }
    else
    {
        XEC_W(parent, enable);
        XEC_W(parent, apiKey);
        XEC_W(parent, languageCodes);
    }
}
#endif
#ifdef GD_PROGRAM_SUPPORT
void Program::serial(pugi::xml_node &parent, const bool read)
{
    if(read)
    {
        XEC_R(parent, enabled);
        int _t = 0;
        XEC_R_RAW(parent, _t, "type");type = (Type)_t;
        XEC_R(parent, id);
        XEC_R(parent, name);
        XEC_R(parent, commandLine);
        XEC_R(parent, iconFilename);
    }
    else
    {
        XEC_W(parent, enabled);
        XEC_W(parent, type);
        XEC_W(parent, id);
        XEC_W(parent, name);
        XEC_W(parent, commandLine);
        XEC_W(parent, iconFilename);
    }
}
#endif
#ifdef GD_VOICE_ENGINE_SUPPORT
void VoiceEngine::serial(pugi::xml_node &parent, const bool read)
{
    if(read)
    {
        XEC_R(parent, enabled);
        XEC_R(parent, id);
        XEC_R(parent, name);
        XEC_R(parent, iconFilename);
        XEC_R(parent, volume);
        XEC_R(parent, rate);
    }
    else
    {
        XEC_W(parent, enabled);
        XEC_W(parent, id);
        XEC_W(parent, name);
        XEC_W(parent, iconFilename);
        XEC_W(parent, volume);
        XEC_W(parent, rate);
    }
}
#endif
#ifdef GD_MEDIAWIKI_SUPPORT
void MediaWiki::serial(pugi::xml_node &parent, const bool read)
{
    if(read)
    {
        XEC_R(parent, id);
        XEC_R(parent, name);
        XEC_R(parent, url);
        XEC_R(parent, enabled);
        XEC_R(parent, icon);
    }
    else
    {
        XEC_W(parent, id);
        XEC_W(parent, name);
        XEC_W(parent, url);
        XEC_W(parent, enabled);
        XEC_W(parent, icon);
    }
}
#endif
#ifdef GD_WEBSITE_SUPPORT
void WebSite::serial(pugi::xml_node &parent, const bool read)
{
    if(read)
    {
        XEC_R(parent, id);
        XEC_R(parent, name);
        XEC_R(parent, url);
        XEC_R(parent, enabled);
        XEC_R(parent, iconFilename);
        XEC_R(parent, inside_iframe);
    }
    else
    {
        XEC_W(parent, id);
        XEC_W(parent, name);
        XEC_W(parent, url);
        XEC_W(parent, enabled);
        XEC_W(parent, iconFilename);
        XEC_W(parent, inside_iframe);
    }
}
#endif
#ifdef GD_DICTSERVER_SUPPORT
void DictServer::serial(pugi::xml_node &parent, const bool read)
{
    if(read)
    {
        XEC_R(parent, id);
        XEC_R(parent, name);
        XEC_R(parent, url);
        XEC_R(parent, enabled);
        XEC_R(parent, databases);
        XEC_R(parent, strategies);
        XEC_R(parent, iconFilename);
    }
    else
    {
        XEC_W(parent, id);
        XEC_W(parent, name);
        XEC_W(parent, url);
        XEC_W(parent, enabled);
        XEC_W(parent, databases);
        XEC_W(parent, strategies);
        XEC_W(parent, iconFilename);
    }
}
#endif

void HeadwordsDialog::serial(pugi::xml_node &parent, const bool read)
{
    if(read)
    {
        XEC_R(parent, searchMode);
        XEC_R(parent, matchCase);
        XEC_R(parent, autoApply);
        XEC_R(parent, headwordsExportPath);
        XEC_R(parent, headwordsDialogGeometry);
    }
    else
    {
        XEC_W(parent, searchMode);
        XEC_W(parent, matchCase);
        XEC_W(parent, autoApply);
        XEC_W(parent, headwordsExportPath);
        XEC_W(parent, headwordsDialogGeometry);
    }
}

void Class::save() THROW_SPEC( exError )
{
    pugi::xml_document pdoc;
    pugi::xml_node pd = XD_CHK_ONLY(pdoc, "config");
    serial(pd, false);
    QString pdoc_file = getConfigFileName();
    if ( !XF_SAVE(pdoc, pdoc_file.toLocal8Bit().data()) )
        throw exCantWriteConfigFile();
}

void Class::load() THROW_SPEC( exError )
{
    QString pdoc_file = getConfigFileName();
    bool exist = QFile::exists( pdoc_file );
    if ( !exist )
    {
        // Check if we have a template config file. If we do, load it instead
        pdoc_file = getProgramDataDir() + "/content/defconfig";
    }

    pugi::xml_document pdoc;
    if ( XF_OPEN(pdoc, pdoc_file.toLocal8Bit().data()) )
    {
        pugi::xml_node pd = pdoc.select_single_node("config").node();
        if(!pd.empty())
            serial(pd, true);
    }

    if ( !exist )
    {
        // Make the default config, save it and return it
#ifdef Q_OS_LINUX
        if ( QDir( "/usr/share/stardict/dic" ).exists() )
            paths.push_back( Path( "/usr/share/stardict/dic", true ) );

        if ( QDir( "/usr/share/dictd" ).exists() )
            paths.push_back( Path( "/usr/share/dictd", true ) );

        if ( QDir( "/usr/share/opendict/dictionaries" ).exists() )
            paths.push_back( Path( "/usr/share/opendict/dictionaries", true ) );

        if ( QDir( "/usr/share/goldendict-wordnet" ).exists() )
            paths.push_back( Path( "/usr/share/goldendict-wordnet", true ) );

        if ( QDir( "/usr/share/WyabdcRealPeopleTTS" ).exists() )
            soundDirs.push_back( SoundDir( "/usr/share/WyabdcRealPeopleTTS", "WyabdcRealPeopleTTS" ) );

        if ( QDir( "/usr/share/myspell/dicts" ).exists() )
            hunspell.dictionariesPath = "/usr/share/myspell/dicts";

#endif
#ifdef Q_OS_WIN32
        // get path to Program Files
        wchar_t buf[ MAX_PATH ];
        SHGetFolderPathW( NULL, CSIDL_PROGRAM_FILES, NULL, 0, buf );
        QString pathToProgramFiles = QString::fromWCharArray( buf );
        if ( pathToProgramFiles.isEmpty() )
            pathToProgramFiles = "C:\\Program Files";
        if ( QDir( pathToProgramFiles + "\\StarDict\\dic" ).exists() )
            paths.push_back( Path( pathToProgramFiles + "\\StarDict\\dic", true ) );

#ifdef GD_SOUND_DIRS_SUPPORT
        if ( QDir( pathToProgramFiles + "\\StarDict\\WyabdcRealPeopleTTS" ).exists() )
            soundDirs.push_back( SoundDir( pathToProgramFiles + "\\StarDict\\WyabdcRealPeopleTTS", "WyabdcRealPeopleTTS" ) );
        else if ( QDir( pathToProgramFiles + "\\WyabdcRealPeopleTTS" ).exists() )
            soundDirs.push_back( SoundDir( pathToProgramFiles + "\\WyabdcRealPeopleTTS", "WyabdcRealPeopleTTS" ) );
#endif

#endif

#ifndef Q_OS_WIN32
        preferences.audioPlaybackProgram = "mplayer";
#endif

#ifdef GD_HUNSPELL_SUPPORT
        QString possibleMorphologyPath = getProgramDataDir() + "/content/morphology";
        if ( QDir( possibleMorphologyPath ).exists() )
            hunspell.dictionariesPath = possibleMorphologyPath;
#endif

#ifdef GD_MEDIAWIKI_SUPPORT
        mediawikis = makeDefaultMediaWikis( true );
#endif
#ifdef GD_WEBSITE_SUPPORT
        webSites = makeDefaultWebSites();
#endif
#ifdef GD_DICTSERVER_SUPPORT
        dictServers = makeDefaultDictServers();
#endif
    }
}

void Class::serial(pugi::xml_node &pd, const bool read)
{
    //  unsigned int csize = 0;
    pugi::xml_node xn;

    XO_STDARRAY(pd, paths, Path, read);
#ifdef GD_SOUND_DIRS_SUPPORT
    XO_STDARRAY(pd, soundDirs, SoundDir, read);
#endif
    dictionaryOrder.serial(xn = XO_NODE(pd, Group, dictionaryOrder), read);
    inactiveDictionaries.serial(xn = XO_NODE(pd, Group, inactiveDictionaries), read);
    groups.serial(xn = XO_NODE(pd, Groups, groups), read);
#ifdef GD_HUNSPELL_SUPPORT
    hunspell.serial(xn = XO_NODE(pd, Hunspell, hunspell), read);
#endif
#ifdef GD_TRANSLITERATION_SUPPORT
    transliteration.serial(xn = XO_NODE(pd, Transliteration, transliteration), read);
#endif
#ifdef GD_FORVO_API_SUPPORT
    forvo.serial(xn = XO_NODE(pd, Forvo, forvo), read);
#endif
#ifdef GD_MEDIAWIKI_SUPPORT
    XO_STDARRAY(pd, mediawikis, MediaWiki, read);
#endif

#ifdef GD_WEBSITE_SUPPORT
    XO_STDARRAY(pd, webSites, WebSite, read);
#endif

#ifdef GD_DICTSERVER_SUPPORT
    XO_STDARRAY(pd, dictServers, DictServer, read);
#endif

#ifdef GD_PROGRAM_SUPPORT
    XO_STDARRAY(pd, programs, Program, read);
#endif

#ifdef GD_VOICE_ENGINE_SUPPORT
    XO_STDARRAY(pd, voiceEngines, VoiceEngine, read);
#endif

    XE_STDSET(pd, mutedDictionaries, QString, read);
    XE_STDSET(pd, popupMutedDictionaries, QString, read);

    preferences.serial(xn = XO_NODE(pd, Preferences, preferences), read);

    if(read)
    {
        XEC_R(pd, lastMainGroupId);

        XEC_R(pd, lastPopupGroupId);

        XEC_R(pd, popupWindowState);

        XEC_R(pd, popupWindowGeometry);

        XEC_R(pd, pinPopupWindow);

        XEC_R(pd, popupWindowAlwaysOnTop);

        XEC_R(pd, mainWindowState);

        XEC_R(pd, mainWindowGeometry);

        XEC_R(pd, helpWindowGeometry);

        XEC_R(pd, helpSplitterState);

#ifdef Q_OS_WIN
        XEC_R(pd, maximizedMainWindowGeometry);
        XEC_R(pd, normalMainWindowGeometry);
#endif

        XEC_R(pd, dictInfoGeometry);

        XEC_R(pd, inspectorGeometry);

        XEC_R(pd, timeForNewReleaseCheck);

        XEC_R(pd, skippedRelease);

        XEC_R(pd, showingDictBarNames);

        XEC_R(pd, usingSmallIconsInToolbars);

        XEC_R(pd, historyExportPath);

        XEC_R(pd, resourceSavePath);

        XEC_R(pd, articleSavePath);

        XEC_R(pd, editDictionaryCommandLine);

        XEC_R(pd, maxPictureWidth);

        XEC_R(pd, maxHeadwordSize);

        XEC_R(pd, maxHeadwordsToExpand);
    }
    else
    {
        XEC_W(pd, lastMainGroupId);

        XEC_W(pd, lastPopupGroupId);

        XEC_W(pd, popupWindowState);

        XEC_W(pd, popupWindowGeometry);

        XEC_W(pd, pinPopupWindow);

        XEC_W(pd, popupWindowAlwaysOnTop);

        XEC_W(pd, mainWindowState);

        XEC_W(pd, mainWindowGeometry);

        XEC_W(pd, helpWindowGeometry);

        XEC_W(pd, helpSplitterState);

#ifdef Q_OS_WIN
        XEC_W(pd, maximizedMainWindowGeometry);
        XEC_W(pd, normalMainWindowGeometry);
#endif

        XEC_W(pd, dictInfoGeometry);

        XEC_W(pd, inspectorGeometry);

        XEC_W(pd, timeForNewReleaseCheck);

        XEC_W(pd, skippedRelease);

        XEC_W(pd, showingDictBarNames);

        XEC_W(pd, usingSmallIconsInToolbars);

        XEC_W(pd, historyExportPath);

        XEC_W(pd, resourceSavePath);

        XEC_W(pd, articleSavePath);

        XEC_W(pd, editDictionaryCommandLine);

        XEC_W(pd, maxPictureWidth);

        XEC_W(pd, maxHeadwordSize);

        XEC_W(pd, maxHeadwordsToExpand);
    }

    headwordsDialog.serial(xn = XO_NODE(pd, HeadwordsDialog, headwordsDialog), read);
}
#else
void save( Class const & c ) THROW_SPEC( exError )
{
    QFile configFile( getConfigFileName() + ".tmp" );

    if ( !configFile.open( QFile::WriteOnly ) )
        throw exCantWriteConfigFile();

    QDomDocument dd;

    QDomElement root = dd.createElement( "config" );
    dd.appendChild( root );

    {
        QDomElement paths = dd.createElement( "paths" );
        root.appendChild( paths );

        for( Paths::const_iterator i = c.paths.begin(); i != c.paths.end(); ++i )
        {
            QDomElement path = dd.createElement( "path" );
            paths.appendChild( path );

            QDomAttr recursive = dd.createAttribute( "recursive" );
            recursive.setValue( i->recursive ? "1" : "0" );
            path.setAttributeNode( recursive );

            QDomText value = dd.createTextNode( i->path );

            path.appendChild( value );
        }
    }

#ifdef GD_SOUND_DIRS_SUPPORT
    {
        QDomElement soundDirs = dd.createElement( "sounddirs" );
        root.appendChild( soundDirs );

        for( SoundDirs::const_iterator i = c.soundDirs.begin(); i != c.soundDirs.end(); ++i )
        {
            QDomElement soundDir = dd.createElement( "sounddir" );
            soundDirs.appendChild( soundDir );

            QDomAttr name = dd.createAttribute( "name" );
            name.setValue( i->name );
            soundDir.setAttributeNode( name );

            QDomAttr icon = dd.createAttribute( "icon" );
            icon.setValue( i->iconFilename );
            soundDir.setAttributeNode( icon );

            QDomText value = dd.createTextNode( i->path );

            soundDir.appendChild( value );
        }
    }
#endif

    {
        QDomElement dictionaryOrder = dd.createElement( "dictionaryOrder" );
        root.appendChild( dictionaryOrder );
        saveGroup( c.dictionaryOrder, dictionaryOrder );
    }

    {
        QDomElement inactiveDictionaries = dd.createElement( "inactiveDictionaries" );
        root.appendChild( inactiveDictionaries );
        saveGroup( c.inactiveDictionaries, inactiveDictionaries );
    }

    {
        QDomElement groups = dd.createElement( "groups" );
        root.appendChild( groups );

        QDomAttr nextId = dd.createAttribute( "nextId" );
        nextId.setValue( QString::number( c.groups.nextId ) );
        groups.setAttributeNode( nextId );

        for( Groups::const_iterator i = c.groups.begin(); i != c.groups.end(); ++i )
        {
            QDomElement group = dd.createElement( "group" );
            groups.appendChild( group );

            saveGroup( *i, group );
        }
    }

#ifdef GD_HUNSPELL_SUPPORT
    {
        QDomElement hunspell = dd.createElement( "hunspell" );
        QDomAttr path = dd.createAttribute( "dictionariesPath" );
        path.setValue( c.hunspell.dictionariesPath );
        hunspell.setAttributeNode( path );
        root.appendChild( hunspell );

        for( int x = 0; x < c.hunspell.enabledDictionaries.size(); ++x )
        {
            QDomElement en = dd.createElement( "enabled" );
            QDomText value = dd.createTextNode( c.hunspell.enabledDictionaries.at( x ) );

            en.appendChild( value );
            hunspell.appendChild( en );
        }
    }
#endif

#ifdef GD_TRANSLITERATION_SUPPORT
    {
        // Russian translit

        QDomElement transliteration = dd.createElement( "transliteration" );
        root.appendChild( transliteration );

        QDomElement opt = dd.createElement( "enableRussianTransliteration" );
        opt.appendChild( dd.createTextNode( c.transliteration.enableRussianTransliteration ? "1":"0" ) );
        transliteration.appendChild( opt );

        // German translit

        opt = dd.createElement( "enableGermanTransliteration" );
        opt.appendChild( dd.createTextNode( c.transliteration.enableGermanTransliteration ? "1":"0" ) );
        transliteration.appendChild( opt );

        // Greek translit

        opt = dd.createElement( "enableGreekTransliteration" );
        opt.appendChild( dd.createTextNode( c.transliteration.enableGreekTransliteration ? "1":"0" ) );
        transliteration.appendChild( opt );

        // Belarusian translit

        opt = dd.createElement( "enableBelarusianTransliteration" );
        opt.appendChild( dd.createTextNode( c.transliteration.enableBelarusianTransliteration ? "1":"0" ) );
        transliteration.appendChild( opt );

        // Chinese

        QDomElement chinese = dd.createElement( "chinese" );
        transliteration.appendChild( chinese );

        opt = dd.createElement( "enable" );
        opt.appendChild( dd.createTextNode( c.transliteration.chinese.enable ? "1":"0" ) );
        chinese.appendChild( opt );

        opt = dd.createElement( "enableSCToTWConversion" );
        opt.appendChild( dd.createTextNode( c.transliteration.chinese.enableSCToTWConversion ? "1":"0" ) );
        chinese.appendChild( opt );

        opt = dd.createElement( "enableSCToHKConversion" );
        opt.appendChild( dd.createTextNode( c.transliteration.chinese.enableSCToHKConversion ? "1":"0" ) );
        chinese.appendChild( opt );

        opt = dd.createElement( "enableTCToSCConversion" );
        opt.appendChild( dd.createTextNode( c.transliteration.chinese.enableTCToSCConversion ? "1":"0" ) );
        chinese.appendChild( opt );

        // Romaji

        QDomElement romaji = dd.createElement( "romaji" );
        transliteration.appendChild( romaji );

        opt = dd.createElement( "enable" );
        opt.appendChild( dd.createTextNode( c.transliteration.romaji.enable ? "1":"0" ) );
        romaji.appendChild( opt );

        opt = dd.createElement( "enableHepburn" );
        opt.appendChild( dd.createTextNode( c.transliteration.romaji.enableHepburn ? "1":"0" ) );
        romaji.appendChild( opt );

        opt = dd.createElement( "enableNihonShiki" );
        opt.appendChild( dd.createTextNode( c.transliteration.romaji.enableNihonShiki ? "1":"0" ) );
        romaji.appendChild( opt );

        opt = dd.createElement( "enableKunreiShiki" );
        opt.appendChild( dd.createTextNode( c.transliteration.romaji.enableKunreiShiki ? "1":"0" ) );
        romaji.appendChild( opt );

        opt = dd.createElement( "enableHiragana" );
        opt.appendChild( dd.createTextNode( c.transliteration.romaji.enableHiragana ? "1":"0" ) );
        romaji.appendChild( opt );

        opt = dd.createElement( "enableKatakana" );
        opt.appendChild( dd.createTextNode( c.transliteration.romaji.enableKatakana ? "1":"0" ) );
        romaji.appendChild( opt );
    }
#endif

#ifdef GD_FORVO_API_SUPPORT
    {
        // Forvo

        QDomElement forvo = dd.createElement( "forvo" );
        root.appendChild( forvo );

        QDomElement opt = dd.createElement( "enable" );
        opt.appendChild( dd.createTextNode( c.forvo.enable ? "1":"0" ) );
        forvo.appendChild( opt );

        opt = dd.createElement( "apiKey" );
        opt.appendChild( dd.createTextNode( c.forvo.apiKey ) );
        forvo.appendChild( opt );

        opt = dd.createElement( "languageCodes" );
        opt.appendChild( dd.createTextNode( c.forvo.languageCodes ) );
        forvo.appendChild( opt );
    }
#endif

#ifdef GD_MEDIAWIKI_SUPPORT
    {
        QDomElement mws = dd.createElement( "mediawikis" );
        root.appendChild( mws );

        for( MediaWikis::const_iterator i = c.mediawikis.begin(); i != c.mediawikis.end(); ++i )
        {
            QDomElement mw = dd.createElement( "mediawiki" );
            mws.appendChild( mw );

            QDomAttr id = dd.createAttribute( "id" );
            id.setValue( i->id );
            mw.setAttributeNode( id );

            QDomAttr name = dd.createAttribute( "name" );
            name.setValue( i->name );
            mw.setAttributeNode( name );

            QDomAttr url = dd.createAttribute( "url" );
            url.setValue( i->url );
            mw.setAttributeNode( url );

            QDomAttr enabled = dd.createAttribute( "enabled" );
            enabled.setValue( i->enabled ? "1" : "0" );
            mw.setAttributeNode( enabled );

            QDomAttr icon = dd.createAttribute( "icon" );
            icon.setValue( i->icon );
            mw.setAttributeNode( icon );
        }
    }
#endif

#ifdef GD_WEBSITE_SUPPORT
    {
        QDomElement wss = dd.createElement( "websites" );
        root.appendChild( wss );

        for( WebSites::const_iterator i = c.webSites.begin(); i != c.webSites.end(); ++i )
        {
            QDomElement ws = dd.createElement( "website" );
            wss.appendChild( ws );

            QDomAttr id = dd.createAttribute( "id" );
            id.setValue( i->id );
            ws.setAttributeNode( id );

            QDomAttr name = dd.createAttribute( "name" );
            name.setValue( i->name );
            ws.setAttributeNode( name );

            QDomAttr url = dd.createAttribute( "url" );
            url.setValue( i->url );
            ws.setAttributeNode( url );

            QDomAttr enabled = dd.createAttribute( "enabled" );
            enabled.setValue( i->enabled ? "1" : "0" );
            ws.setAttributeNode( enabled );

            QDomAttr icon = dd.createAttribute( "icon" );
            icon.setValue( i->iconFilename );
            ws.setAttributeNode( icon );

            QDomAttr inside_iframe = dd.createAttribute( "inside_iframe" );
            inside_iframe.setValue( i->inside_iframe ? "1" : "0" );
            ws.setAttributeNode( inside_iframe );
        }
    }
#endif

#ifdef GD_DICTSERVER_SUPPORT
    {
        QDomElement dss = dd.createElement( "dictservers" );
        root.appendChild( dss );

        for( DictServers::const_iterator i = c.dictServers.begin(); i != c.dictServers.end(); ++i )
        {
            QDomElement ds = dd.createElement( "server" );
            dss.appendChild( ds );

            QDomAttr id = dd.createAttribute( "id" );
            id.setValue( i->id );
            ds.setAttributeNode( id );

            QDomAttr name = dd.createAttribute( "name" );
            name.setValue( i->name );
            ds.setAttributeNode( name );

            QDomAttr url = dd.createAttribute( "url" );
            url.setValue( i->url );
            ds.setAttributeNode( url );

            QDomAttr enabled = dd.createAttribute( "enabled" );
            enabled.setValue( i->enabled ? "1" : "0" );
            ds.setAttributeNode( enabled );

            QDomAttr databases = dd.createAttribute( "databases" );
            databases.setValue( i->databases );
            ds.setAttributeNode( databases );

            QDomAttr strategies = dd.createAttribute( "strategies" );
            strategies.setValue( i->strategies );
            ds.setAttributeNode( strategies );

            QDomAttr icon = dd.createAttribute( "icon" );
            icon.setValue( i->iconFilename );
            ds.setAttributeNode( icon );
        }
    }
#endif

#ifdef GD_PROGRAM_SUPPORT
    {
        QDomElement programs = dd.createElement( "programs" );
        root.appendChild( programs );

        for( Programs::const_iterator i = c.programs.begin(); i != c.programs.end(); ++i )
        {
            QDomElement p = dd.createElement( "program" );
            programs.appendChild( p );

            QDomAttr id = dd.createAttribute( "id" );
            id.setValue( i->id );
            p.setAttributeNode( id );

            QDomAttr name = dd.createAttribute( "name" );
            name.setValue( i->name );
            p.setAttributeNode( name );

            QDomAttr commandLine = dd.createAttribute( "commandLine" );
            commandLine.setValue( i->commandLine );
            p.setAttributeNode( commandLine );

            QDomAttr enabled = dd.createAttribute( "enabled" );
            enabled.setValue( i->enabled ? "1" : "0" );
            p.setAttributeNode( enabled );

            QDomAttr type = dd.createAttribute( "type" );
            type.setValue( QString::number( i->type ) );
            p.setAttributeNode( type );

            QDomAttr icon = dd.createAttribute( "icon" );
            icon.setValue( i->iconFilename );
            p.setAttributeNode( icon );
        }
    }
#endif

#ifdef GD_VOICE_ENGINE_SUPPORT
    {
        QDomNode ves = dd.createElement( "voiceEngines" );
        root.appendChild( ves );

        for ( VoiceEngines::const_iterator i = c.voiceEngines.begin(); i != c.voiceEngines.end(); ++i )
        {
            QDomElement v = dd.createElement( "voiceEngine" );
            ves.appendChild( v );

            QDomAttr id = dd.createAttribute( "id" );
            id.setValue( i->id );
            v.setAttributeNode( id );

            QDomAttr name = dd.createAttribute( "name" );
            name.setValue( i->name );
            v.setAttributeNode( name );

            QDomAttr enabled = dd.createAttribute( "enabled" );
            enabled.setValue( i->enabled ? "1" : "0" );
            v.setAttributeNode( enabled );

            QDomAttr icon = dd.createAttribute( "icon" );
            icon.setValue( i->iconFilename );
            v.setAttributeNode( icon );

            QDomAttr volume = dd.createAttribute( "volume" );
            volume.setValue( QString::number( i->volume ) );
            v.setAttributeNode( volume );

            QDomAttr rate = dd.createAttribute( "rate" );
            rate.setValue( QString::number( i->rate ) );
            v.setAttributeNode( rate );
        }
    }
#endif

    {
        QDomElement muted = dd.createElement( "mutedDictionaries" );
        root.appendChild( muted );
        saveMutedDictionaries( dd, muted, c.mutedDictionaries );
    }

    {
        QDomElement muted = dd.createElement( "popupMutedDictionaries" );
        root.appendChild( muted );
        saveMutedDictionaries( dd, muted, c.popupMutedDictionaries );
    }

    {
        QDomElement preferences = dd.createElement( "preferences" );
        root.appendChild( preferences );

        QDomElement opt = dd.createElement( "interfaceLanguage" );
        opt.appendChild( dd.createTextNode( c.preferences.interfaceLanguage ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "helpLanguage" );
        opt.appendChild( dd.createTextNode( c.preferences.helpLanguage ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "displayStyle" );
        opt.appendChild( dd.createTextNode( c.preferences.displayStyle ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "newTabsOpenAfterCurrentOne" );
        opt.appendChild( dd.createTextNode( c.preferences.newTabsOpenAfterCurrentOne ? "1":"0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "newTabsOpenInBackground" );
        opt.appendChild( dd.createTextNode( c.preferences.newTabsOpenInBackground ? "1":"0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "hideSingleTab" );
        opt.appendChild( dd.createTextNode( c.preferences.hideSingleTab ? "1":"0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "mruTabOrder" );
        opt.appendChild( dd.createTextNode( c.preferences.mruTabOrder ? "1":"0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "hideMenubar" );
        opt.appendChild( dd.createTextNode( c.preferences.hideMenubar ? "1":"0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "enableTrayIcon" );
        opt.appendChild( dd.createTextNode( c.preferences.enableTrayIcon ? "1":"0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "startToTray" );
        opt.appendChild( dd.createTextNode( c.preferences.startToTray ? "1":"0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "closeToTray" );
        opt.appendChild( dd.createTextNode( c.preferences.closeToTray ? "1":"0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "autoStart" );
        opt.appendChild( dd.createTextNode( c.preferences.autoStart ? "1":"0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "doubleClickTranslates" );
        opt.appendChild( dd.createTextNode( c.preferences.doubleClickTranslates ? "1":"0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "selectWordBySingleClick" );
        opt.appendChild( dd.createTextNode( c.preferences.selectWordBySingleClick ? "1":"0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "escKeyHidesMainWindow" );
        opt.appendChild( dd.createTextNode( c.preferences.escKeyHidesMainWindow ? "1":"0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "zoomFactor" );
        opt.appendChild( dd.createTextNode( QString::number( c.preferences.zoomFactor ) ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "helpZoomFactor" );
        opt.appendChild( dd.createTextNode( QString::number( c.preferences.helpZoomFactor ) ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "wordsZoomLevel" );
        opt.appendChild( dd.createTextNode( QString::number( c.preferences.wordsZoomLevel ) ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "enableMainWindowHotkey" );
        opt.appendChild( dd.createTextNode( c.preferences.enableMainWindowHotkey ? "1":"0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "mainWindowHotkey" );
        opt.appendChild( dd.createTextNode( c.preferences.mainWindowHotkey.toKeySequence().toString() ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "enableClipboardHotkey" );
        opt.appendChild( dd.createTextNode( c.preferences.enableClipboardHotkey ? "1":"0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "clipboardHotkey" );
        opt.appendChild( dd.createTextNode( c.preferences.clipboardHotkey.toKeySequence().toString() ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "enableScanPopup" );
        opt.appendChild( dd.createTextNode( c.preferences.enableScanPopup ? "1":"0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "startWithScanPopupOn" );
        opt.appendChild( dd.createTextNode( c.preferences.startWithScanPopupOn ? "1":"0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "enableScanPopupModifiers" );
        opt.appendChild( dd.createTextNode( c.preferences.enableScanPopupModifiers ? "1":"0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "scanPopupModifiers" );
        opt.appendChild( dd.createTextNode( QString::number( c.preferences.scanPopupModifiers ) ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "scanPopupAltMode" );
        opt.appendChild( dd.createTextNode( c.preferences.scanPopupAltMode ? "1":"0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "scanPopupAltModeSecs" );
        opt.appendChild( dd.createTextNode( QString::number( c.preferences.scanPopupAltModeSecs ) ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "ignoreOwnClipboardChanges" );
        opt.appendChild( dd.createTextNode( c.preferences.ignoreOwnClipboardChanges ? "1":"0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "scanToMainWindow" );
        opt.appendChild( dd.createTextNode( c.preferences.scanToMainWindow ? "1":"0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "ignoreDiacritics" );
        opt.appendChild( dd.createTextNode( c.preferences.ignoreDiacritics ? "1":"0" ) );
        preferences.appendChild( opt );

#ifdef HAVE_X11
        opt = dd.createElement( "showScanFlag" );
        opt.appendChild( dd.createTextNode( c.preferences.showScanFlag? "1":"0" ) );
        preferences.appendChild( opt );
#endif

        opt = dd.createElement( "scanPopupUseUIAutomation" );
        opt.appendChild( dd.createTextNode( c.preferences.scanPopupUseUIAutomation ? "1":"0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "scanPopupUseIAccessibleEx" );
        opt.appendChild( dd.createTextNode( c.preferences.scanPopupUseIAccessibleEx ? "1":"0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "scanPopupUseGDMessage" );
        opt.appendChild( dd.createTextNode( c.preferences.scanPopupUseGDMessage ? "1":"0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "scanPopupUnpinnedWindowFlags" );
        opt.appendChild( dd.createTextNode( QString::number( c.preferences.scanPopupUnpinnedWindowFlags ) ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "scanPopupUnpinnedBypassWMHint" );
        opt.appendChild( dd.createTextNode( c.preferences.scanPopupUnpinnedBypassWMHint ? "1":"0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "pronounceOnLoadMain" );
        opt.appendChild( dd.createTextNode( c.preferences.pronounceOnLoadMain ? "1" : "0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "pronounceOnLoadPopup" );
        opt.appendChild( dd.createTextNode( c.preferences.pronounceOnLoadPopup ? "1" : "0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "useInternalPlayer" );
        opt.appendChild( dd.createTextNode( c.preferences.useInternalPlayer ? "1" : "0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "internalPlayerBackend" );
        opt.appendChild( dd.createTextNode( c.preferences.internalPlayerBackend.uiName() ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "audioPlaybackProgram" );
        opt.appendChild( dd.createTextNode( c.preferences.audioPlaybackProgram ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "alwaysOnTop" );
        opt.appendChild( dd.createTextNode( c.preferences.alwaysOnTop ? "1" : "0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "searchInDock" );
        opt.appendChild( dd.createTextNode( c.preferences.searchInDock ? "1" : "0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "historyStoreInterval" );
        opt.appendChild( dd.createTextNode( QString::number( c.preferences.historyStoreInterval ) ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "favoritesStoreInterval" );
        opt.appendChild( dd.createTextNode( QString::number( c.preferences.favoritesStoreInterval ) ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "confirmFavoritesDeletion" );
        opt.appendChild( dd.createTextNode( c.preferences.confirmFavoritesDeletion ? "1" : "0" ) );
        preferences.appendChild( opt );

        {
            QDomElement proxy = dd.createElement( "proxyserver" );
            preferences.appendChild( proxy );

            QDomAttr enabled = dd.createAttribute( "enabled" );
            enabled.setValue( c.preferences.proxyServer.enabled ? "1" : "0" );
            proxy.setAttributeNode( enabled );

            QDomAttr useSystemProxy = dd.createAttribute( "useSystemProxy" );
            useSystemProxy.setValue( c.preferences.proxyServer.useSystemProxy ? "1" : "0" );
            proxy.setAttributeNode( useSystemProxy );

            opt = dd.createElement( "type" );
            opt.appendChild( dd.createTextNode( QString::number( c.preferences.proxyServer.type ) ) );
            proxy.appendChild( opt );

            opt = dd.createElement( "host" );
            opt.appendChild( dd.createTextNode( c.preferences.proxyServer.host ) );
            proxy.appendChild( opt );

            opt = dd.createElement( "port" );
            opt.appendChild( dd.createTextNode( QString::number( c.preferences.proxyServer.port ) ) );
            proxy.appendChild( opt );

            opt = dd.createElement( "user" );
            opt.appendChild( dd.createTextNode( c.preferences.proxyServer.user ) );
            proxy.appendChild( opt );

            opt = dd.createElement( "password" );
            opt.appendChild( dd.createTextNode( c.preferences.proxyServer.password ) );
            proxy.appendChild( opt );

            opt = dd.createElement( "systemProxyUser" );
            opt.appendChild( dd.createTextNode( c.preferences.proxyServer.systemProxyUser ) );
            proxy.appendChild( opt );

            opt = dd.createElement( "systemProxyPassword" );
            opt.appendChild( dd.createTextNode( c.preferences.proxyServer.systemProxyPassword ) );
            proxy.appendChild( opt );
        }

        opt = dd.createElement( "checkForNewReleases" );
        opt.appendChild( dd.createTextNode( c.preferences.checkForNewReleases ? "1" : "0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "disallowContentFromOtherSites" );
        opt.appendChild( dd.createTextNode( c.preferences.disallowContentFromOtherSites ? "1" : "0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "enableWebPlugins" );
        opt.appendChild( dd.createTextNode( c.preferences.enableWebPlugins ? "1" : "0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "hideGoldenDictHeader" );
        opt.appendChild( dd.createTextNode( c.preferences.hideGoldenDictHeader ? "1" : "0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "maxStringsInHistory" );
        opt.appendChild( dd.createTextNode( QString::number( c.preferences.maxStringsInHistory ) ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "storeHistory" );
        opt.appendChild( dd.createTextNode( QString::number( c.preferences.storeHistory ) ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "alwaysExpandOptionalParts" );
        opt.appendChild( dd.createTextNode( QString::number( c.preferences.alwaysExpandOptionalParts ) ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "addonStyle" );
        opt.appendChild( dd.createTextNode( c.preferences.addonStyle ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "collapseBigArticles" );
        opt.appendChild( dd.createTextNode( c.preferences.collapseBigArticles ? "1" : "0" ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "articleSizeLimit" );
        opt.appendChild( dd.createTextNode( QString::number( c.preferences.articleSizeLimit ) ) );
        preferences.appendChild( opt );

        opt = dd.createElement( "maxDictionaryRefsInContextMenu" );
        opt.appendChild( dd.createTextNode( QString::number( c.preferences.maxDictionaryRefsInContextMenu ) ) );
        preferences.appendChild( opt );

#ifndef Q_WS_X11
        opt = dd.createElement( "trackClipboardChanges" );
        opt.appendChild( dd.createTextNode( c.preferences.trackClipboardChanges ? "1" : "0" ) );
        preferences.appendChild( opt );
#endif

        opt = dd.createElement( "synonymSearchEnabled" );
        opt.appendChild( dd.createTextNode( c.preferences.synonymSearchEnabled ? "1" : "0" ) );
        preferences.appendChild( opt );

        {
            QDomNode hd = dd.createElement( "fullTextSearch" );
            preferences.appendChild( hd );

            QDomElement opt = dd.createElement( "searchMode" );
            opt.appendChild( dd.createTextNode( QString::number( c.preferences.fts.searchMode ) ) );
            hd.appendChild( opt );

            opt = dd.createElement( "matchCase" );
            opt.appendChild( dd.createTextNode( c.preferences.fts.matchCase ? "1" : "0" ) );
            hd.appendChild( opt );

            opt = dd.createElement( "maxArticlesPerDictionary" );
            opt.appendChild( dd.createTextNode( QString::number( c.preferences.fts.maxArticlesPerDictionary ) ) );
            hd.appendChild( opt );

            opt = dd.createElement( "maxDistanceBetweenWords" );
            opt.appendChild( dd.createTextNode( QString::number( c.preferences.fts.maxDistanceBetweenWords ) ) );
            hd.appendChild( opt );

            opt = dd.createElement( "useMaxArticlesPerDictionary" );
            opt.appendChild( dd.createTextNode( c.preferences.fts.useMaxArticlesPerDictionary ? "1" : "0" ) );
            hd.appendChild( opt );

            opt = dd.createElement( "useMaxDistanceBetweenWords" );
            opt.appendChild( dd.createTextNode( c.preferences.fts.useMaxDistanceBetweenWords ? "1" : "0" ) );
            hd.appendChild( opt );

            opt = dd.createElement( "dialogGeometry" );
            opt.appendChild( dd.createTextNode( QString::fromLatin1( c.preferences.fts.dialogGeometry.toBase64() ) ) );
            hd.appendChild( opt );

            opt = dd.createElement( "disabledTypes" );
            opt.appendChild( dd.createTextNode( c.preferences.fts.disabledTypes ) );
            hd.appendChild( opt );

            opt = dd.createElement( "enabled" );
            opt.appendChild( dd.createTextNode( c.preferences.fts.enabled ? "1" : "0" ) );
            hd.appendChild( opt );

            opt = dd.createElement( "ignoreWordsOrder" );
            opt.appendChild( dd.createTextNode( c.preferences.fts.ignoreWordsOrder ? "1" : "0" ) );
            hd.appendChild( opt );

            opt = dd.createElement( "ignoreDiacritics" );
            opt.appendChild( dd.createTextNode( c.preferences.fts.ignoreDiacritics ? "1" : "0" ) );
            hd.appendChild( opt );

            opt = dd.createElement( "maxDictionarySize" );
            opt.appendChild( dd.createTextNode( QString::number( c.preferences.fts.maxDictionarySize ) ) );
            hd.appendChild( opt );
        }

    }

    {
        QDomElement opt = dd.createElement( "lastMainGroupId" );
        opt.appendChild( dd.createTextNode( QString::number( c.lastMainGroupId ) ) );
        root.appendChild( opt );

        opt = dd.createElement( "lastPopupGroupId" );
        opt.appendChild( dd.createTextNode( QString::number( c.lastPopupGroupId ) ) );
        root.appendChild( opt );

        opt = dd.createElement( "popupWindowState" );
        opt.appendChild( dd.createTextNode( QString::fromLatin1( c.popupWindowState.toBase64() ) ) );
        root.appendChild( opt );

        opt = dd.createElement( "popupWindowGeometry" );
        opt.appendChild( dd.createTextNode( QString::fromLatin1( c.popupWindowGeometry.toBase64() ) ) );
        root.appendChild( opt );

        opt = dd.createElement( "pinPopupWindow" );
        opt.appendChild( dd.createTextNode( c.pinPopupWindow ? "1" : "0" ) );
        root.appendChild( opt );

        opt = dd.createElement( "popupWindowAlwaysOnTop" );
        opt.appendChild( dd.createTextNode( c.popupWindowAlwaysOnTop ? "1" : "0" ) );
        root.appendChild( opt );

        opt = dd.createElement( "mainWindowState" );
        opt.appendChild( dd.createTextNode( QString::fromLatin1( c.mainWindowState.toBase64() ) ) );
        root.appendChild( opt );

        opt = dd.createElement( "mainWindowGeometry" );
        opt.appendChild( dd.createTextNode( QString::fromLatin1( c.mainWindowGeometry.toBase64() ) ) );
        root.appendChild( opt );

        opt = dd.createElement( "helpWindowGeometry" );
        opt.appendChild( dd.createTextNode( QString::fromLatin1( c.helpWindowGeometry.toBase64() ) ) );
        root.appendChild( opt );

        opt = dd.createElement( "helpSplitterState" );
        opt.appendChild( dd.createTextNode( QString::fromLatin1( c.helpSplitterState.toBase64() ) ) );
        root.appendChild( opt );

#ifdef Q_OS_WIN
        {
            QDomElement maximizedMainWindowGeometry = dd.createElement( "maximizedMainWindowGeometry" );
            root.appendChild( maximizedMainWindowGeometry );

            opt = dd.createElement( "x" );
            opt.appendChild( dd.createTextNode( QString::number( c.maximizedMainWindowGeometry.x() ) ) );
            maximizedMainWindowGeometry.appendChild( opt );

            opt = dd.createElement( "y" );
            opt.appendChild( dd.createTextNode( QString::number( c.maximizedMainWindowGeometry.y() ) ) );
            maximizedMainWindowGeometry.appendChild( opt );

            opt = dd.createElement( "width" );
            opt.appendChild( dd.createTextNode( QString::number( c.maximizedMainWindowGeometry.width() ) ) );
            maximizedMainWindowGeometry.appendChild( opt );

            opt = dd.createElement( "height" );
            opt.appendChild( dd.createTextNode( QString::number( c.maximizedMainWindowGeometry.height() ) ) );
            maximizedMainWindowGeometry.appendChild( opt );

            QDomElement normalMainWindowGeometry = dd.createElement( "normalMainWindowGeometry" );
            root.appendChild( normalMainWindowGeometry );

            opt = dd.createElement( "x" );
            opt.appendChild( dd.createTextNode( QString::number( c.normalMainWindowGeometry.x() ) ) );
            normalMainWindowGeometry.appendChild( opt );

            opt = dd.createElement( "y" );
            opt.appendChild( dd.createTextNode( QString::number( c.normalMainWindowGeometry.y() ) ) );
            normalMainWindowGeometry.appendChild( opt );

            opt = dd.createElement( "width" );
            opt.appendChild( dd.createTextNode( QString::number( c.normalMainWindowGeometry.width() ) ) );
            normalMainWindowGeometry.appendChild( opt );

            opt = dd.createElement( "height" );
            opt.appendChild( dd.createTextNode( QString::number( c.normalMainWindowGeometry.height() ) ) );
            normalMainWindowGeometry.appendChild( opt );
        }
#endif

        opt = dd.createElement( "dictInfoGeometry" );
        opt.appendChild( dd.createTextNode( QString::fromLatin1( c.dictInfoGeometry.toBase64() ) ) );
        root.appendChild( opt );

        opt = dd.createElement( "inspectorGeometry" );
        opt.appendChild( dd.createTextNode( QString::fromLatin1( c.inspectorGeometry.toBase64() ) ) );
        root.appendChild( opt );

        opt = dd.createElement( "timeForNewReleaseCheck" );
        opt.appendChild( dd.createTextNode( c.timeForNewReleaseCheck.toString( Qt::ISODate ) ) );
        root.appendChild( opt );

        opt = dd.createElement( "skippedRelease" );
        opt.appendChild( dd.createTextNode( c.skippedRelease ) );
        root.appendChild( opt );

        opt = dd.createElement( "showingDictBarNames" );
        opt.appendChild( dd.createTextNode( c.showingDictBarNames ? "1" : "0" ) );
        root.appendChild( opt );

        opt = dd.createElement( "usingSmallIconsInToolbars" );
        opt.appendChild( dd.createTextNode( c.usingSmallIconsInToolbars ? "1" : "0" ) );
        root.appendChild( opt );

        if( !c.historyExportPath.isEmpty() )
        {
            opt = dd.createElement( "historyExportPath" );
            opt.appendChild( dd.createTextNode( c.historyExportPath ) );
            root.appendChild( opt );
        }

        if( !c.resourceSavePath.isEmpty() )
        {
            opt = dd.createElement( "resourceSavePath" );
            opt.appendChild( dd.createTextNode( c.resourceSavePath ) );
            root.appendChild( opt );
        }

        if( !c.articleSavePath.isEmpty() )
        {
            opt = dd.createElement( "articleSavePath" );
            opt.appendChild( dd.createTextNode( c.articleSavePath ) );
            root.appendChild( opt );
        }

        opt = dd.createElement( "editDictionaryCommandLine" );
        opt.appendChild( dd.createTextNode( c.editDictionaryCommandLine ) );
        root.appendChild( opt );

        opt = dd.createElement( "maxPictureWidth" );
        opt.appendChild( dd.createTextNode( QString::number( c.maxPictureWidth ) ) );
        root.appendChild( opt );

        opt = dd.createElement( "maxHeadwordSize" );
        opt.appendChild( dd.createTextNode( QString::number( c.maxHeadwordSize ) ) );
        root.appendChild( opt );

        opt = dd.createElement( "maxHeadwordsToExpand" );
        opt.appendChild( dd.createTextNode( QString::number( c.maxHeadwordsToExpand ) ) );
        root.appendChild( opt );
    }

    {
        QDomNode hd = dd.createElement( "headwordsDialog" );
        root.appendChild( hd );

        QDomElement opt = dd.createElement( "searchMode" );
        opt.appendChild( dd.createTextNode( QString::number( c.headwordsDialog.searchMode ) ) );
        hd.appendChild( opt );

        opt = dd.createElement( "matchCase" );
        opt.appendChild( dd.createTextNode( c.headwordsDialog.matchCase ? "1" : "0" ) );
        hd.appendChild( opt );

        opt = dd.createElement( "autoApply" );
        opt.appendChild( dd.createTextNode( c.headwordsDialog.autoApply ? "1" : "0" ) );
        hd.appendChild( opt );

        opt = dd.createElement( "headwordsExportPath" );
        opt.appendChild( dd.createTextNode( c.headwordsDialog.headwordsExportPath ) );
        hd.appendChild( opt );

        opt = dd.createElement( "headwordsDialogGeometry" );
        opt.appendChild( dd.createTextNode( QString::fromLatin1( c.headwordsDialog.headwordsDialogGeometry.toBase64() ) ) );
        hd.appendChild( opt );
    }

    QByteArray result( dd.toByteArray() );

    if ( configFile.write( result ) != result.size() )
        throw exCantWriteConfigFile();

    configFile.close();

    renameAtomically( configFile.fileName(), getConfigFileName() );
}
#endif

QString getConfigFileName()
{
#ifdef GD_PUGIXML_XSERIAL
    return getHomeDir().absoluteFilePath( "config.nwx" );
#else
    return getHomeDir().absoluteFilePath( "config" );
#endif
}

QString getConfigDir() THROW_SPEC( exError )
{
    return getHomeDir().path() + QDir::separator();
}

QString getIndexDir() THROW_SPEC( exError )
{
    QDir result = getHomeDir();

    result.mkpath( "index" );

    if ( !result.cd( "index" ) )
        throw exCantUseIndexDir();

    return result.path() + QDir::separator();
}

QString getPidFileName() THROW_SPEC( exError )
{
    return getHomeDir().filePath( "pid" );
}

QString getHistoryFileName() THROW_SPEC( exError )
{
#ifdef GD_PUGIXML_XSERIAL
    return getHomeDir().filePath( "history.nwx" );
#else
    return getHomeDir().filePath( "history" );
#endif
}

QString getFavoritiesFileName() THROW_SPEC( exError )
{
#ifdef GD_PUGIXML_XSERIAL
    return getHomeDir().filePath( "favorites.nwx" );
#else
    return getHomeDir().filePath( "favorites" );
#endif
}

QString getUserCssFileName() THROW_SPEC( exError )
{
    return getHomeDir().filePath( "article-style.css" );
}

QString getUserCssPrintFileName() THROW_SPEC( exError )
{
    return getHomeDir().filePath( "article-style-print.css" );
}

QString getUserQtCssFileName() THROW_SPEC( exError )
{
    return getHomeDir().filePath( "qt-style.css" );
}

QString getProgramDataDir() throw()
{
    if ( isPortableVersion() )
        return QCoreApplication::applicationDirPath();

#ifdef PROGRAM_DATA_DIR
    return PROGRAM_DATA_DIR;
#else
    return QCoreApplication::applicationDirPath();
#endif
}

QString getLocDir() throw()
{
    if ( QDir( getProgramDataDir() ).cd( "locale" ) )
        return getProgramDataDir() + "/locale";
    else
        return QCoreApplication::applicationDirPath() + "/locale";
}

QString getHelpDir() throw()
{
    if ( QDir( getProgramDataDir() ).cd( "help" ) )
        return getProgramDataDir() + "/help";
    else
        return QCoreApplication::applicationDirPath() + "/help";
}

#ifdef GD_TRANSLITERATION_SUPPORT
QString getOpenCCDir() throw()
{
#if defined( Q_OS_WIN )
    if ( QDir( "opencc" ).exists() )
        return "opencc";
    else
        return QCoreApplication::applicationDirPath() + "/opencc";
#elif defined( Q_OS_MAC )
    QString path = QCoreApplication::applicationDirPath() + "/opencc";
    if ( QDir( path ).exists() )
        return path;

    return QString();
#else
    return QString();
#endif
}
#endif

bool isPortableVersion() throw()
{
    struct IsPortable
    {
        bool isPortable;

        IsPortable(): isPortable( QFileInfo( QCoreApplication::applicationDirPath() + "/portable" ).isDir() )
        {}
    };

    static const IsPortable p;

    return p.isPortable;
}

QString getPortableVersionDictionaryDir() throw()
{
    if ( isPortableVersion() )
        return getProgramDataDir() + "/content";
    else
        return QString();
}

QString getPortableVersionMorphoDir() throw()
{
    if ( isPortableVersion() )
        return getPortableVersionDictionaryDir() + "/morphology";
    else
        return QString();
}

QString getStylesDir() throw()
{
    QDir result = getHomeDir();

    result.mkpath( "styles" );

    if ( !result.cd( "styles" ) )
        return QString();

    return result.path() + QDir::separator();
}

QString getRenamesFileName() throw()
{
#ifdef GD_PUGIXML_XSERIAL
    return getHomeDir().filePath( "renames.nwx" );
#else
    return getHomeDir().filePath( "renames.xml" );
#endif
}
}
