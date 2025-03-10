/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "editdictionaries.hh"
#include "loaddictionaries.hh"
#include "dictinfo.hh"
#include "mainwindow.hh"
#include "helpwindow.hh"
#include <QMessageBox>

using std::vector;

EditDictionaries::EditDictionaries( QWidget * parent, Config::Class & cfg_,
                                    vector< sptr< Dictionary::Class > > & dictionaries_,
                                    Instances::Groups & groupInstances_,
                                    QNetworkAccessManager & dictNetMgr_ ):
    QDialog( parent ), cfg( cfg_ ), dictionaries( dictionaries_ ),
    groupInstances( groupInstances_ ),
    dictNetMgr( dictNetMgr_ ),
    origCfg( cfg ),
    sources( this, cfg ),
    orderAndProps( new OrderAndProps( this, cfg.dictionaryOrder, cfg.inactiveDictionaries,
                                      dictionaries ) ),
    groups( new Groups( this, dictionaries, cfg.groups, orderAndProps->getCurrentDictionaryOrder() ) ),
    dictionariesChanged( false ),
    groupsChanged( false ),
    lastCurrentTab( 0 )
  , helpWindow( 0 )
  , helpAction( this )
{
    // Some groups may have contained links to non-existnent dictionaries. We
    // would like to preserve them if no edits were done. To that end, we save
    // the initial group readings so that if no edits were really done, we won't
    // be changing groups.
    origCfg.groups = groups->getGroups();
    origCfg.dictionaryOrder = orderAndProps->getCurrentDictionaryOrder();
    origCfg.inactiveDictionaries = orderAndProps->getCurrentInactiveDictionaries();

    ui.setupUi( this );

    setWindowIcon( QIcon(":/icons/book.png") );

    ui.tabs->clear();

    ui.tabs->addTab( &sources, QIcon(":/icons/reload.png"), tr( "&Sources" ) );
    ui.tabs->addTab( orderAndProps.get(), QIcon(":/icons/book.png"), tr( "&Dictionaries" ) );
    ui.tabs->addTab( groups.get(), QIcon(":/icons/bookcase.png"), tr( "&Groups" ) );

    connect( ui.buttons, SIGNAL( clicked( QAbstractButton * ) ),
             this, SLOT( buttonBoxClicked( QAbstractButton * ) ) );

    connect( &sources, SIGNAL( rescan() ), this, SLOT( rescanSources() ) );

    connect( groups.get(), SIGNAL( showDictionaryInfo( QString const & ) ),
             this, SIGNAL( showDictionaryInfo( QString const & ) ) );

    connect( orderAndProps.get(), SIGNAL( showDictionaryHeadwords( QString const & ) ),
             this, SIGNAL( showDictionaryHeadwords( QString const & ) ) );
    connect( orderAndProps.get(), SIGNAL( userDictNameChanged( QString const &, QString const & ) ),
             this, SIGNAL( userDictNameChanged( QString const &, QString const & ) ) );

    connect( ui.buttons, SIGNAL( helpRequested() ),
             this, SLOT( helpRequested() ) );

    helpAction.setShortcut( QKeySequence( "F1" ) );
    helpAction.setShortcutContext( Qt::WidgetWithChildrenShortcut );

    connect( &helpAction, SIGNAL( triggered() ),
             this, SLOT( helpRequested() ) );

    addAction( &helpAction );

}

EditDictionaries::~EditDictionaries()
{ if( helpWindow ) delete helpWindow; }

void EditDictionaries::editGroup( unsigned id )
{
    if ( id == Instances::Group::AllGroupId )
        ui.tabs->setCurrentIndex( 1 );
    else
    {
        ui.tabs->setCurrentIndex( 2 );
        groups->editGroup( id );
    }
}

void EditDictionaries::save()
{
    Config::Groups newGroups = groups->getGroups();
    Config::Group newOrder = orderAndProps->getCurrentDictionaryOrder();
    Config::Group newInactive = orderAndProps->getCurrentInactiveDictionaries();

    if ( isSourcesChanged() )
        acceptChangedSources( false );

    if ( origCfg.groups != newGroups || origCfg.dictionaryOrder != newOrder ||
         origCfg.inactiveDictionaries != newInactive )
    {
        groupsChanged = true;
        cfg.groups = newGroups;
        cfg.dictionaryOrder = newOrder;
        cfg.inactiveDictionaries = newInactive;
    }
}

void EditDictionaries::accept()
{
    save();
    QDialog::accept();
}

void EditDictionaries::on_tabs_currentChanged( int index )
{
    if ( index == -1 || !isVisible() )
        return; // Sent upon the construction/destruction

    if ( !lastCurrentTab && index )
    {
        // We're switching away from the Sources tab -- if its contents were
        // changed, we need to either apply or reject now.

        if ( isSourcesChanged() )
        {
            ui.tabs->setCurrentIndex( 0 );

            QMessageBox question( QMessageBox::Question, tr( "Sources changed" ),
                                  tr( "Some sources were changed. Would you like to accept the changes?" ),
                                  QMessageBox::NoButton, this );

            QPushButton * accept = question.addButton( tr( "Accept" ), QMessageBox::AcceptRole );

            question.addButton( tr( "Cancel" ), QMessageBox::RejectRole );

            question.exec();

            if ( question.clickedButton() == accept )
            {
                acceptChangedSources( true );

                lastCurrentTab = index;
                ui.tabs->setCurrentIndex( index );
            }
            else
            {
                // Prevent tab from switching
                lastCurrentTab = 0;
                return;
            }
        }
    }
    else
        if ( lastCurrentTab == 1 && index != 1 )
        {
            // When switching from the dictionary order, we need to propagate any
            // changes to the groups.
            groups->updateDictionaryOrder( orderAndProps->getCurrentDictionaryOrder() );
        }

    lastCurrentTab = index;
}

void EditDictionaries::rescanSources()
{
    acceptChangedSources( true );
}

void EditDictionaries::buttonBoxClicked( QAbstractButton * button )
{
    if (ui.buttons->buttonRole(button) == QDialogButtonBox::ApplyRole) {
        if ( isSourcesChanged() ) {
            acceptChangedSources( true );
        }
        save();
    }
}

bool EditDictionaries::isSourcesChanged() const
{
    return sources.getPaths() != cfg.paths
        #ifdef GD_SOUND_DIRS_SUPPORT
            || sources.getSoundDirs() != cfg.soundDirs
        #endif
        #ifdef GD_HUNSPELL_SUPPORT
            || sources.getHunspell() != cfg.hunspell
        #endif
        #ifdef GD_TRANSLITERATION_SUPPORT
            || sources.getTransliteration() != cfg.transliteration
        #endif
        #ifdef GD_FORVO_API_SUPPORT
            || sources.getForvo() != cfg.forvo
        #endif
        #ifdef GD_MEDIAWIKI_SUPPORT
            || sources.getMediaWikis() != cfg.mediawikis
        #endif
        #ifdef GD_WEBSITE_SUPPORT
            || sources.getWebSites() != cfg.webSites
        #endif
        #ifdef GD_DICTSERVER_SUPPORT
            || sources.getDictServers() != cfg.dictServers
        #endif
        #ifdef GD_PROGRAM_SUPPORT
            || sources.getPrograms() != cfg.programs
        #endif
        #ifdef GD_VOICE_ENGINE_SUPPORT
            || sources.getVoiceEngines() != cfg.voiceEngines
        #endif
            ;
}

void EditDictionaries::acceptChangedSources( bool rebuildGroups )
{
    dictionariesChanged = true;

    Config::Groups savedGroups = groups->getGroups();
    Config::Group savedOrder = orderAndProps->getCurrentDictionaryOrder();
    Config::Group savedInactive = orderAndProps->getCurrentInactiveDictionaries();

    cfg.paths = sources.getPaths();
#ifdef GD_SOUND_DIRS_SUPPORT
    cfg.soundDirs = sources.getSoundDirs();
#endif
#ifdef GD_HUNSPELL_SUPPORT
    cfg.hunspell = sources.getHunspell();
#endif
#ifdef GD_TRANSLITERATION_SUPPORT
    cfg.transliteration = sources.getTransliteration();
#endif
#ifdef GD_FORVO_API_SUPPORT
    cfg.forvo = sources.getForvo();
#endif
#ifdef GD_MEDIAWIKI_SUPPORT
    cfg.mediawikis = sources.getMediaWikis();
#endif
#ifdef GD_WEBSITE_SUPPORT
    cfg.webSites = sources.getWebSites();
#endif
#ifdef GD_DICTSERVER_SUPPORT
    cfg.dictServers = sources.getDictServers();
#endif
#ifdef GD_PROGRAM_SUPPORT
    cfg.programs = sources.getPrograms();
#endif
#ifdef GD_VOICE_ENGINE_SUPPORT
    cfg.voiceEngines = sources.getVoiceEngines();
#endif

    groupInstances.clear(); // Those hold pointers to dictionaries, we need to
    // free them.

    ui.tabs->setUpdatesEnabled( false );
    ui.tabs->removeTab( 1 );
    ui.tabs->removeTab( 1 );
    groups.reset();
    orderAndProps.reset();

    LoadDictionaries::loadDictionaries( this, false, cfg, dictionaries, dictNetMgr );

    // If no changes to groups were made, update the original data
    bool noGroupEdits = ( origCfg.groups == savedGroups );

    if ( noGroupEdits )
        savedGroups = cfg.groups;

    Instances::updateNames( savedGroups, dictionaries );

    bool noOrderEdits = ( origCfg.dictionaryOrder == savedOrder );

    if ( noOrderEdits )
        savedOrder = cfg.dictionaryOrder;

    Instances::updateNames( savedOrder, dictionaries );

    bool noInactiveEdits = ( origCfg.inactiveDictionaries == savedInactive );

    if ( noInactiveEdits )
        savedInactive  = cfg.inactiveDictionaries;

    Instances::updateNames( savedInactive, dictionaries );

    if ( rebuildGroups )
    {
        orderAndProps = sptr< OrderAndProps >(new OrderAndProps( this, savedOrder, savedInactive, dictionaries ));
        ui.tabs->insertTab( 1, orderAndProps.get(), QIcon(":/icons/book.png"), tr( "&Dictionaries" ) );

        groups = sptr< Groups >(new Groups( this, dictionaries, savedGroups, orderAndProps->getCurrentDictionaryOrder() ));
        ui.tabs->insertTab( 2, groups.get(), QIcon(":/icons/bookcase.png"), tr( "&Groups" ) );

        ui.tabs->setUpdatesEnabled( true );

        if ( noGroupEdits )
            origCfg.groups = groups->getGroups();

        if ( noOrderEdits )
            origCfg.dictionaryOrder = orderAndProps->getCurrentDictionaryOrder();

        if ( noInactiveEdits )
            origCfg.inactiveDictionaries = orderAndProps->getCurrentInactiveDictionaries();
    }
}

void EditDictionaries::helpRequested()
{
    if( !helpWindow )
    {
        MainWindow * mainWindow = qobject_cast< MainWindow * >( parentWidget() );
        if( mainWindow )
            mainWindow->closeGDHelp();

        helpWindow = new Help::HelpWindow( this, cfg );

        if( helpWindow )
        {
            helpWindow->setWindowFlags( Qt::Window );

            connect( helpWindow, SIGNAL( needClose() ),
                     this, SLOT( closeHelp() ) );
            helpWindow->showHelpFor( "Manage dictionaries" );
            helpWindow->show();
        }
    }
    else
    {
        if( !helpWindow->isVisible() )
            helpWindow->show();

        helpWindow->activateWindow();
    }
}

void EditDictionaries::closeHelp()
{
    if( helpWindow )
        helpWindow->hide();
}
