#include "MainWindow.h"

#include "core/AppConfig.h"
#include "core/Catalog.h"
#include "core/Database.h"
#include "net/BangumiClient.h"
#include "net/SearchAggregator.h"
#include "net/TorrentEngine.h"
#include "ui/ImageLoader.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "ui/Controls.h"
#include "ui/Overlays.h"
#include "ui/pages/AnimeDetailPage.h"
#include "ui/pages/DownloadsPage.h"
#include "ui/pages/HomePage.h"
#include "ui/pages/LibraryPage.h"
#include "core/LocalLibrary.h"
#include "net/AnimeDownloads.h"
#include "net/ExternalDownloader.h"
#include "ui/pages/SearchPage.h"
#include "ui/pages/SettingsPage.h"
#include "ui/pages/SitesPage.h"

#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <iterator>
#include <QDesktopServices>
#include <QFrame>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QPointer>
#include <QStackedWidget>
#include <QUrl>
#include <QTimer>
#include <QVBoxLayout>
#include <QUrlQuery>
#include <QFileInfo>
#include <QRegularExpression>
#include "core/TitleParser.h"
#include <libtorrent/load_torrent.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_info.hpp>

MainWindow::MainWindow(Catalog *catalog, Database *db, AppConfig *config, HttpClient *http,
                       TorrentEngine *engine, SearchAggregator *aggregator, BangumiClient *bangumi,
                       ImageLoader *images, QWidget *parent)
    : FramelessWindow(parent)
    , m_catalog(catalog)
    , m_db(db)
    , m_config(config)
    , m_http(http)
    , m_engine(engine)
    , m_aggregator(aggregator)
    , m_bangumi(bangumi)
    , m_images(images)
{
    setWindowTitle(QStringLiteral("Anime Nexus"));
    setMinimumSize(1080, 700);
    resize(1320, 860);

    for (int id : m_db->followedIds())
        m_following.insert(id);

    m_localLibrary=new LocalLibrary(QDir(AppConfig::dataDir()).filePath("local-library.json"),this);
    QString libraryError;
    m_localLibrary->load(&libraryError);
    m_animeDownloads=new AnimeDownloads(m_engine,m_localLibrary,m_config,this);
    buildUi();
    connect(m_animeDownloads,&AnimeDownloads::queued,this,[this]{selectPage(kDownloadsPage);});
    connect(m_animeDownloads,&AnimeDownloads::information,this,[this](const QString &text){
        Ui::message(this,QStringLiteral("下载提示"),text);
    });
    connect(m_animeDownloads,&AnimeDownloads::error,this,[this](const QString &text){
        Ui::message(this,QStringLiteral("下载失败"),text,Ui::Notice::Warning);
    });
    if(!libraryError.isEmpty())QTimer::singleShot(0,this,[this,libraryError]{
        Ui::message(this,QStringLiteral("本地记录读取失败"),libraryError,Ui::Notice::Warning);
    });
    connect(m_engine,&TorrentEngine::engineError,this,[this](const QString &message) {
        Ui::message(this,QStringLiteral("下载任务异常"),message,Ui::Notice::Warning);
    });
    QTimer::singleShot(0,this,[this]{m_animeDownloads->adoptCachedSubjects(m_db->cachedSubjects());});
    setEngineStatus(m_engine->running() ? QStringLiteral("运行中") : QStringLiteral("未启动"));
}

namespace {
struct NavPage { const char *name; const char *icon; };
const NavPage kPages[] = {
    {"本季放送","calendar"},{"搜索番剧","search"},{"我的追番","library"},
    {"下载任务","download"},{"资源站点","sites"},{"偏好设置","settings"}};

// Display name of a magnet link or local .torrent handed over on the command line.
QString importedTitle(const QString &input)
{
    if(input.startsWith("magnet:"))return QUrlQuery(QUrl(input)).queryItemValue("dn");
    try{return QString::fromStdString(lt::load_torrent_file(input.toStdString()).ti->name());}
    catch(...){return QFileInfo(input).completeBaseName();}
}
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    central->setObjectName("AppBody");
    central->setAttribute(Qt::WA_StyledBackground,true);
    auto *shell = new QVBoxLayout(central);
    shell->setContentsMargins(0,0,0,0); shell->setSpacing(0);
    shell->addWidget(titleBar());
    auto *body = new QWidget(central);
    auto *root = new QHBoxLayout(body);
    root->setContentsMargins(0,0,0,0); root->setSpacing(0);
    shell->addWidget(body,1);

    auto *nav = new QWidget(body);
    nav->setObjectName("NavRail"); nav->setFixedWidth(212);
    nav->setAttribute(Qt::WA_StyledBackground,true);
    auto *rail = new QVBoxLayout(nav);
    rail->setContentsMargins(12,4,12,14); rail->setSpacing(2);
    for(int i=0;i<int(std::size(kPages));++i) {
        if(i==0) rail->addWidget(label(QStringLiteral("发现"),"NavSection"));
        if(i==kDownloadsPage) rail->addWidget(label(QStringLiteral("管理"),"NavSection"));
        if(i==kSettingsPage) rail->addStretch(1);
        const QString name=QString::fromUtf8(kPages[i].name);
        auto *button=new QPushButton(name,nav);
        button->setIcon(Theme::stateIcon(QString::fromLatin1(kPages[i].icon))); button->setIconSize(QSize(18,18));
        button->setObjectName("NavButton"); button->setCheckable(true);
        button->setFixedHeight(40); button->setCursor(Qt::PointingHandCursor);
        button->setAccessibleName(name);
        if(i==kSearchPage) button->setToolTip(QStringLiteral("Ctrl + K"));
        connect(button,&QPushButton::clicked,this,[this,i]{selectPage(i);});
        rail->addWidget(button); m_navButtons.push_back(button);
    }
    // Active task count rides on the downloads entry so progress is visible from any page.
    auto *badgeRow=new QHBoxLayout(m_navButtons.at(kDownloadsPage));
    badgeRow->setContentsMargins(0,0,10,0); badgeRow->addStretch(1);
    m_downloadBadge=label(QString(),"NavBadge",false,m_navButtons.at(kDownloadsPage));
    m_downloadBadge->setFixedHeight(18); m_downloadBadge->setMinimumWidth(18); m_downloadBadge->setAlignment(Qt::AlignCenter);
    m_downloadBadge->setAttribute(Qt::WA_TransparentForMouseEvents); m_downloadBadge->hide();
    badgeRow->addWidget(m_downloadBadge);

    rail->addSpacing(10);
    auto *engineBox = new QFrame(nav); engineBox->setObjectName("EngineBadge");
    auto *engineRow = new QHBoxLayout(engineBox); engineRow->setContentsMargins(12,9,6,9); engineRow->setSpacing(10);
    m_engineDot = new StatusDot(m_engine->running() ? "ok" : "err",engineBox);
    engineRow->addWidget(m_engineDot);
    auto *engineText=new QVBoxLayout; engineText->setSpacing(0);
    engineText->addWidget(label(QStringLiteral("下载器"),"EngineCaption",false,engineBox));
    m_engineStatus=label(QString(),"EngineName",false,engineBox);
    engineText->addWidget(m_engineStatus);
    engineRow->addLayout(engineText,1);
    auto *switcher=new QToolButton(engineBox);switcher->setIcon(Theme::icon("selector"));switcher->setFixedSize(28,32);
    switcher->setObjectName("DownloaderSwitch");switcher->setAccessibleName(QStringLiteral("切换下载器"));switcher->setToolTip(QStringLiteral("切换下载器"));
    switcher->setCursor(Qt::PointingHandCursor);
    connect(switcher,&QToolButton::clicked,this,[this,switcher]{showDownloaderMenu(switcher);});engineRow->addWidget(switcher);
    rail->addWidget(engineBox);
    m_navButtons.first()->setChecked(true);
    m_navButtons.first()->setFocus(Qt::OtherFocusReason);
    root->addWidget(nav);

    auto *canvas=new QFrame(body); canvas->setObjectName("Canvas");
    auto *canvasLayout=new QVBoxLayout(canvas); canvasLayout->setContentsMargins(1,1,0,0);
    m_stack=new QStackedWidget(canvas); canvasLayout->addWidget(m_stack);
    root->addWidget(canvas,1);
    setCentralWidget(central);

    m_home=new HomePage(m_bangumi,m_images);
    connect(m_home,&HomePage::animeSelected,this,[this](const AnimeSubject &s){openAnime(s);}); m_stack->addWidget(m_home);
    m_search=new SearchPage(m_bangumi,m_images);
    connect(m_search,&SearchPage::animeSelected,this,[this](const AnimeSubject &s){openAnime(s);}); m_stack->addWidget(m_search);
    m_library=new LibraryPage(m_db,m_bangumi,m_images,m_localLibrary);
    connect(m_library,&LibraryPage::animeSelected,this,[this](const AnimeSubject &s){openAnime(s,true);}); m_stack->addWidget(m_library);
    m_downloads=new DownloadsPage(m_engine,m_localLibrary,m_config); m_stack->addWidget(m_downloads);
    m_downloads->setRpc(m_animeDownloads->rpc());
    connect(m_downloads,&DownloadsPage::openDirectoryRequested,this,[this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_config->engine.savePath));
    });
    m_sites=new SitesPage(m_catalog,m_http); m_stack->addWidget(m_sites);
    connect(m_sites,&SitesPage::importRequested,this,&MainWindow::importCatalog);
    connect(m_sites,&SitesPage::exportRequested,this,&MainWindow::exportCatalog);
    m_settings=new SettingsPage(m_engine,m_config); m_stack->addWidget(m_settings);
    connect(m_settings,&SettingsPage::downloaderChanged,this,&MainWindow::updateDownloader);
    connect(m_downloads,&DownloadsPage::switchDownloaderRequested,this,[this](QWidget *anchor){showDownloaderMenu(anchor);});

    // Appearance: quick switch in the title bar, mirrored in 偏好设置 → 外观.
    m_themeButton=new QToolButton(titleBar()); m_themeButton->setObjectName("ThemeButton");
    m_themeButton->setFixedSize(36,30); m_themeButton->setIconSize(QSize(18,18));
    m_themeButton->setPopupMode(QToolButton::InstantPopup); m_themeButton->setCursor(Qt::PointingHandCursor);
    auto *themeMenu=new QMenu(m_themeButton); auto *themeGroup=new QActionGroup(themeMenu);
    for(const auto &[mode,icon]:{std::pair{Theme::Mode::System,"monitor"},{Theme::Mode::Light,"sun"},{Theme::Mode::Dark,"moon"}}) {
        auto *action=themeMenu->addAction(Theme::icon(icon),Theme::modeName(mode));
        action->setCheckable(true); action->setData(int(mode)); themeGroup->addAction(action);
        connect(action,&QAction::triggered,this,[this,mode]{setAppearance(mode);});
    }
    m_themeButton->setMenu(themeMenu);
    addTitleWidget(m_themeButton);
    connect(Theme::notifier(),&Theme::Notifier::changed,this,&MainWindow::updateThemeButton);
    connect(m_settings,&SettingsPage::appearanceRequested,this,&MainWindow::setAppearance);
    updateThemeButton();

    auto *searchShortcut=new QAction(this);
    searchShortcut->setShortcut(QKeySequence("Ctrl+K"));
    connect(searchShortcut,&QAction::triggered,this,[this]{selectPage(kSearchPage);});
    addAction(searchShortcut);

    // Engine change notifications are frequent; the badge only needs a coarse refresh.
    m_badgeTimer=new QTimer(this); m_badgeTimer->setSingleShot(true); m_badgeTimer->setInterval(600);
    connect(m_badgeTimer,&QTimer::timeout,this,&MainWindow::updateDownloadBadge);
    connect(m_engine,&TorrentEngine::changed,m_badgeTimer,qOverload<>(&QTimer::start));
    m_badgeTimer->start();
}

void MainWindow::setAppearance(Theme::Mode mode)
{
    if(mode==Theme::mode())return;
    Theme::setMode(mode);
    m_config->appearance=Theme::modeToString(mode);
    if(!m_config->save())
        Ui::message(this,QStringLiteral("外观未保存"),QStringLiteral("已切换外观，但无法写入配置文件，下次启动将恢复原设置。"),Ui::Notice::Warning);
}

void MainWindow::updateThemeButton()
{
    const auto mode=Theme::mode();
    m_themeButton->setIcon(Theme::icon(mode==Theme::Mode::Dark?"moon":mode==Theme::Mode::Light?"sun":"monitor"));
    const QString tip=QStringLiteral("外观：")+Theme::modeName(mode);
    m_themeButton->setToolTip(tip); m_themeButton->setAccessibleName(tip);
    for(auto *action:m_themeButton->menu()->actions())action->setChecked(action->data().toInt()==int(mode));
}

void MainWindow::updateDownloadBadge()
{
    int active=0;
    for(const auto &task:m_engine->listTorrents())
        if(task.status==DownloadStatus::Downloading||task.status==DownloadStatus::Pending)++active;
    m_downloadBadge->setText(QString::number(active));
    m_downloadBadge->setVisible(active>0);
    m_navButtons.at(kDownloadsPage)->setAccessibleName(active>0
        ? QStringLiteral("下载任务，%1 个进行中").arg(active) : QStringLiteral("下载任务"));
}

void MainWindow::selectPage(int index)
{
    if (index < 0 || index >= m_navButtons.size()) return;
    setContextTitle(QString::fromUtf8(kPages[index].name));
    m_currentNav = index;
    for (int i = 0; i < m_navButtons.size(); ++i)
        m_navButtons.at(i)->setChecked(i == index);
    m_stack->setCurrentIndex(index);
    if (index == kSearchPage)
        m_search->focusInput();
}

void MainWindow::searchQuery(const QString &query)
{
    selectPage(kSearchPage);
    m_search->setQuery(query);
}

void MainWindow::openSubjectById(int bangumiId)
{
    m_bangumi->getSubject(bangumiId, [this](AnimeSubject subject, QString error) {
        if (!error.isEmpty()) {
            m_pendingInput.clear();
            Ui::message(this, QStringLiteral("番剧查询失败"), error, Ui::Notice::Warning);
            return;
        }
        openAnime(subject);
    });
}

void MainWindow::openAnime(const AnimeSubject &subject,bool localOnly)
{
    setContextTitle(QStringLiteral("番剧详情 · ")+subject.displayName());
    if (m_stack->currentWidget() != m_detail)
        m_currentNav = m_stack->currentIndex();
    if (!m_detail) {
        m_detail = new AnimeDetailPage(m_aggregator, m_bangumi, m_catalog, m_images, m_localLibrary,
                                       [this](int id) { return m_following.contains(id); }, this);
        connect(m_detail, &AnimeDetailPage::backRequested, this, &MainWindow::closeDetail);
        connect(m_detail,&AnimeDetailPage::resourceSearchRequested,this,[this](const AnimeSubject &subject){
            selectPage(kSearchPage);m_search->setQuery(subject.displayName());openAnime(subject,false);
        });
        connect(m_detail, &AnimeDetailPage::downloadRequested, this, &MainWindow::onDownloadRequested);
        connect(m_detail, &AnimeDetailPage::followChanged, this, &MainWindow::onFollowChanged);
        connect(m_detail,&AnimeDetailPage::groupDownloadRequested,m_animeDownloads,&AnimeDownloads::downloadGroup);
        connect(m_detail,&AnimeDetailPage::resourcesDiscovered,m_animeDownloads,&AnimeDownloads::adoptExisting);
        connect(m_detail,&AnimeDetailPage::playRequested,this,[this](AnimeSubject subject,ResourceResult resource){
            const QStringList files=m_localLibrary->playableFiles(subject.bangumiId,resource);
            if(files.isEmpty()) {
                Ui::message(this,QStringLiteral("视频不可用"),QStringLiteral("本地视频已移动、删除或尚未下载完成。"),Ui::Notice::Warning);
                return;
            }
            const QString file=Ui::chooseVideo(this,files);
            if(!file.isEmpty()&&!QDesktopServices::openUrl(QUrl::fromLocalFile(file)))
                Ui::message(this,QStringLiteral("无法播放"),QStringLiteral("请为此视频格式设置默认播放器。"),Ui::Notice::Warning);
        });
        m_stack->addWidget(m_detail);
    }
    m_detail->showSubject(subject,localOnly);
    m_stack->setCurrentWidget(m_detail);
    if(!m_pendingInput.isEmpty()) {
        ResourceResult resource;const QString input=std::exchange(m_pendingInput,QString());
        if(input.startsWith("magnet:")) {
            resource.magnetUri=input;
            resource.infoHash=btihFromMagnet(input).value_or(QString());
        } else {
            resource.torrentUrl=input;
        }
        resource.title=importedTitle(input);
        if(resource.title.isEmpty())resource.title=subject.displayName();
        resource.tags=parseTitle(resource.title);resource.releaseGroup=resource.tags.releaseGroup;
        if(resource.releaseGroup.isEmpty())resource.releaseGroup=kUnknownGroup;
        onDownloadRequested(subject,resource);
    }
}

void MainWindow::handleArguments(const QStringList &args)
{
    showNormal();raise();activateWindow();
    auto value=[&](const QString &key){const int i=args.indexOf(key);return i>=0&&i+1<args.size()?args.at(i+1):QString();};
    QString input=value("--add-magnet");if(input.isEmpty())input=value("--add-torrent");
    if(!input.isEmpty()) {
        if(!m_pendingInput.isEmpty()){Ui::message(this,QStringLiteral("等待选择番剧"),QStringLiteral("请先选择上一份导入资源所属的番剧。"));return;}
        m_pendingInput=input;
        const int id=value("--subject").toInt();if(id>0){openSubjectById(id);return;}
        QString title=importedTitle(input);
        QVector<AnimeSubject> matches;for(const auto &subject:m_db->cachedSubjects())if(!title.isEmpty()&&isRelevant(title,subjectKeys(subject)))matches.append(subject);
        if(matches.size()==1){openAnime(matches.first(),false);return;}
        title.remove(QRegularExpression("\\[[^\\]]*\\]|【[^】]*】"));
        title.remove(QRegularExpression("\\s*(?:S\\d+E\\d+|[-–]\\s*\\d+).*$",QRegularExpression::CaseInsensitiveOption));
        searchQuery(title.trimmed());Ui::message(this,QStringLiteral("选择资源所属番剧"),QStringLiteral("请在搜索结果中选择对应番剧，随后将按当前下载器和番剧目录导入资源。"));return;
    }
    const int id=value("--subject").toInt();if(id>0)openSubjectById(id);
    const QString query=value("--search");if(!query.isEmpty())searchQuery(query);
}

void MainWindow::closeDetail()
{
    if (m_detail)
        m_detail->cancel();
    selectPage(m_currentNav);
}

void MainWindow::onFollowChanged(const AnimeSubject &subject, bool following)
{
    if (following) {
        m_db->follow(subject.bangumiId);
        m_db->upsertSubject(subject);
        m_following.insert(subject.bangumiId);
    } else {
        m_db->unfollow(subject.bangumiId);
        m_following.remove(subject.bangumiId);
    }
}

void MainWindow::onDownloadRequested(const AnimeSubject &subject,const ResourceResult &result)
{
    m_db->upsertSubject(subject);
    m_animeDownloads->downloadOne(subject,result);
}

void MainWindow::setEngineStatus(const QString &text)
{
    const bool builtin=m_config->downloader.selected=="builtin";
    m_engineStatus->setText(builtin?QStringLiteral("内置 · ")+text:DownloaderSettings::name(m_config->downloader.selected));
    m_engineDot->setState(!builtin||m_engine->running()?"ok":"err");
}

void MainWindow::updateDownloader()
{
    setEngineStatus(m_engine->running()?QStringLiteral("运行中"):QStringLiteral("未启动"));
    m_downloads->updateDownloader();
}
void MainWindow::showDownloaderMenu(QWidget *anchor)
{
    const QString type=Ui::chooseDownloader(anchor,m_config->downloader);
    if(type.isEmpty())return;
    if(type=="configure"){selectPage(kSettingsPage);m_settings->editDownloader();return;}
    auto candidate=m_config->downloader;candidate.selected=type;
    if(type!="builtin"&&candidate.programs.value(type).isEmpty())candidate.programs[type]=DownloaderSettings::detect(type);
    QString message;
    if(!ExternalDownloader::validate(candidate,&message)){selectPage(kSettingsPage);m_settings->editDownloader(type);return;}
    const auto before=m_config->downloader;m_config->downloader=candidate;
    if(!m_config->save()){m_config->downloader=before;Ui::message(this,QStringLiteral("切换失败"),QStringLiteral("无法保存下载器设置。"),Ui::Notice::Error);return;}
    updateDownloader();
}

void MainWindow::importCatalog()
{
    const QString path = Ui::openJson(this, QStringLiteral("导入站点配置"));
    if (path.isEmpty())
        return;
    QString error;
    if(!m_catalog->importFrom(path,&error)){Ui::message(this,QStringLiteral("导入失败"),error,Ui::Notice::Warning);return;}
    m_sites->refresh();
}

void MainWindow::exportCatalog()
{
    const QString path = Ui::saveJson(this, QStringLiteral("导出站点配置"),
                                    QStringLiteral("anime_sites_export.json"));
    if (path.isEmpty())
        return;
    m_catalog->save(path);
}
