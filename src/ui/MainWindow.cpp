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
    connect(m_animeDownloads,&AnimeDownloads::queued,this,[this]{selectPage(3);});
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

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *shell = new QVBoxLayout(central);
    shell->setContentsMargins(0,0,0,0); shell->setSpacing(0);
    shell->addWidget(titleBar());
    auto *body = new QWidget(central);
    auto *root = new QHBoxLayout(body);
    root->setContentsMargins(0,0,0,0); root->setSpacing(0);
    shell->addWidget(body,1);

    auto *nav = new QWidget(body);
    nav->setObjectName("NavRail"); nav->setFixedWidth(190);
    auto *rail = new QVBoxLayout(nav);
    rail->setContentsMargins(12,26,12,18); rail->setSpacing(5);
    auto *brand = new QVBoxLayout;
    brand->setContentsMargins(12,0,0,16);
    brand->addWidget(label(QStringLiteral("Nexus"),"Brand"));
    brand->addWidget(label(QStringLiteral("你的动画收藏室"),"BrandSub"));
    rail->addLayout(brand);
    rail->addWidget(label(QStringLiteral("浏览"),"NavSection"));
    const QStringList names = {QStringLiteral("本季放送"),QStringLiteral("搜索番剧"),
        QStringLiteral("我的追番"),QStringLiteral("下载任务"),QStringLiteral("资源站点"),
        QStringLiteral("偏好设置")};
    const QStringList icons = {"home","search","library","download","sites","settings"};
    for(int i=0;i<names.size();++i) {
        if(i==3) { rail->addSpacing(16); rail->addWidget(label(QStringLiteral("管理"),"NavSection")); }
        if(i==5) rail->addStretch(1);
        auto *button=new QPushButton(names.at(i),nav);
        button->setIcon(Theme::icon(icons.at(i))); button->setIconSize(QSize(19,19));
        button->setObjectName("NavButton"); button->setCheckable(true);
        button->setFixedHeight(44); button->setCursor(Qt::PointingHandCursor);
        button->setAccessibleName(names.at(i));
        connect(button,&QPushButton::clicked,this,[this,i]{selectPage(i);});
        rail->addWidget(button); m_navButtons.push_back(button);
    }
    rail->addSpacing(12);
    auto *engineBox = new QFrame(nav); engineBox->setObjectName("EngineBadge");
    auto *engineRow = new QHBoxLayout(engineBox); engineRow->setContentsMargins(12,10,12,10);
    auto *dot = new StatusDot(m_engine->running() ? "ok" : "err",engineBox);
    engineRow->addWidget(dot);
    m_engineStatus=label(QString(),"NavFooter",true,engineBox);
    engineRow->addWidget(m_engineStatus,1);
    auto *switcher=new QToolButton(engineBox);switcher->setIcon(Theme::icon("chevron-down"));switcher->setFixedSize(26,28);
    switcher->setObjectName("DownloaderSwitch");switcher->setAccessibleName(QStringLiteral("切换下载器"));switcher->setToolTip(QStringLiteral("切换下载器"));
    connect(switcher,&QToolButton::clicked,this,[this,switcher]{showDownloaderMenu(switcher);});engineRow->addWidget(switcher);
    rail->addWidget(engineBox);
    m_navButtons.first()->setChecked(true);
    m_navButtons.first()->setFocus(Qt::OtherFocusReason);
    root->addWidget(nav);
    m_stack=new QStackedWidget(body); root->addWidget(m_stack,1);
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

    auto *searchShortcut=new QAction(this);
    searchShortcut->setShortcut(QKeySequence("Ctrl+K"));
    connect(searchShortcut,&QAction::triggered,this,[this]{selectPage(1);});
    addAction(searchShortcut);
}

void MainWindow::selectPage(int index)
{
    const QStringList titles = {QStringLiteral("本季放送"), QStringLiteral("搜索番剧"),
        QStringLiteral("我的追番"), QStringLiteral("下载任务"), QStringLiteral("站点管理"),
        QStringLiteral("设置")};
    if (index < 0 || index >= m_navButtons.size()) return;
    setContextTitle(titles.at(index));
    m_currentNav = index;
    for (int i = 0; i < m_navButtons.size(); ++i)
        m_navButtons.at(i)->setChecked(i == index);
    m_stack->setCurrentIndex(index);
    if (index == 1)
        m_search->focusInput();
}

void MainWindow::searchQuery(const QString &query)
{
    selectPage(1);
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
    setContextTitle(QStringLiteral("番剧详情"));
    if (m_stack->currentWidget() != m_detail)
        m_currentNav = m_stack->currentIndex();
    if (!m_detail) {
        m_detail = new AnimeDetailPage(m_aggregator, m_bangumi, m_catalog, m_images, m_localLibrary,
                                       [this](int id) { return m_following.contains(id); }, this);
        connect(m_detail, &AnimeDetailPage::backRequested, this, &MainWindow::closeDetail);
        connect(m_detail,&AnimeDetailPage::resourceSearchRequested,this,[this](const AnimeSubject &subject){
            selectPage(1);m_search->setQuery(subject.displayName());openAnime(subject,false);
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
            resource.magnetUri=input;resource.title=QUrlQuery(QUrl(input)).queryItemValue("dn");
            resource.infoHash=btihFromMagnet(input).value_or(QString());
        } else {
            resource.torrentUrl=input;
            try{const auto torrent=lt::load_torrent_file(input.toStdString());resource.title=QString::fromStdString(torrent.ti->name());}catch(...){resource.title=QFileInfo(input).completeBaseName();}
        }
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
        QString title;
        if(input.startsWith("magnet:"))title=QUrlQuery(QUrl(input)).queryItemValue("dn");
        else try{const auto torrent=lt::load_torrent_file(input.toStdString());title=QString::fromStdString(torrent.ti->name());}catch(...){title=QFileInfo(input).completeBaseName();}
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
    m_engineStatus->setText(m_config->downloader.selected=="builtin"?QStringLiteral("内置 · ")+text:DownloaderSettings::name(m_config->downloader.selected));
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
    if(type=="configure"){selectPage(5);m_settings->editDownloader();return;}
    auto candidate=m_config->downloader;candidate.selected=type;
    if(type!="builtin"&&candidate.programs.value(type).isEmpty())candidate.programs[type]=DownloaderSettings::detect(type);
    QString message;
    if(!ExternalDownloader::validate(candidate,&message)){selectPage(5);m_settings->editDownloader(type);return;}
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
