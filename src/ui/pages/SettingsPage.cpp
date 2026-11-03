#include "SettingsPage.h"

#include "core/AppConfig.h"
#include "net/TorrentEngine.h"
#include "net/ExternalDownloader.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "ui/Controls.h"
#include "ui/Overlays.h"

#include <QAbstractSocket>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkInterface>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {
constexpr int kControlWidth = 240;
// Settings read as a column; very wide rows separate labels from their controls.
constexpr int kColumnWidth = 980;

QFrame *settingsCard(QVBoxLayout *column, const QString &title, const char *name = nullptr)
{
    auto *card=new QFrame; card->setProperty("settingsCard",true);
    if(name)card->setObjectName(name);
    auto *col=new QVBoxLayout(card); col->setSizeConstraint(QLayout::SetMinimumSize);
    col->setContentsMargins(20,16,20,16); col->setSpacing(12);
    col->addWidget(label(title,"SettingsCardTitle",false,card));
    column->addWidget(card);
    return card;
}

// One setting: title and explanation on the left, its control on the right.
// Wide controls (paths) take the remaining width instead of the standard column.
void addSetting(QWidget *card, const QString &title, const QString &hint, QWidget *control, bool wide = false)
{
    auto *col=static_cast<QVBoxLayout *>(card->layout());
    if(col->count()>1)col->addWidget(divider(card));
    auto *row=new QHBoxLayout; row->setSpacing(24);
    auto *text=new QVBoxLayout; text->setSpacing(2);
    text->addWidget(label(title,"SettingTitle",false,card));
    if(!hint.isEmpty())text->addWidget(label(hint,"SettingHint",true,card));
    row->addLayout(text,1);
    control->setParent(card);
    if(wide) row->addWidget(control,2);
    else if(qobject_cast<QCheckBox *>(control) || control->objectName()==QLatin1String("Segmented"))
        row->addWidget(control,0,Qt::AlignRight|Qt::AlignVCenter);
    else { control->setFixedWidth(kControlWidth); row->addWidget(control,0,Qt::AlignVCenter); }
    col->addLayout(row);
}

QSpinBox *limitBox(const char *name, int maximum, const QString &suffix = QString())
{
    auto *box=new QSpinBox; box->setObjectName(name);
    box->setRange(0,maximum); box->setSpecialValueText(QStringLiteral("不限"));
    if(!suffix.isEmpty())box->setSuffix(suffix);
    return box;
}
}

SettingsPage::SettingsPage(TorrentEngine *engine, AppConfig *config, QWidget *parent)
    : QWidget(parent)
    , m_engine(engine)
    , m_config(config)
{
    auto *root=pageLayout(this);
    auto header=pageHeader(QStringLiteral("偏好设置"),this);
    m_status=header.meta;
    root->addWidget(header.widget);

    m_tabs=new QTabWidget(this);
    m_tabs->tabBar()->setDrawBase(false);
    auto addSection=[&](const QString &name,const QString &description) {
        auto *body=new QWidget; body->setObjectName("ScrollBody"); body->setMaximumWidth(kColumnWidth);
        auto *col=new QVBoxLayout(body); col->setContentsMargins(0,4,10,12); col->setSpacing(14);
        col->addWidget(label(description,"Muted",true,body));
        auto *scroll=scrollArea(body,m_tabs);
        m_tabs->addTab(scroll,name);
        return col;
    };

    // Appearance
    auto *col=addSection(QStringLiteral("外观"),QStringLiteral("选择界面配色，切换后立即生效。"));
    auto *look=settingsCard(col,QStringLiteral("界面配色"));
    auto *modeBar=new QFrame; modeBar->setObjectName("Segmented");
    auto *modeRow=new QHBoxLayout(modeBar); modeRow->setContentsMargins(4,4,4,4); modeRow->setSpacing(2);
    m_appearance=new QButtonGroup(this);
    for(const auto &[mode,icon]:{std::pair{Theme::Mode::System,"monitor"},{Theme::Mode::Light,"sun"},{Theme::Mode::Dark,"moon"}}) {
        auto *button=new QPushButton(Theme::icon(icon),Theme::modeName(mode),modeBar);
        button->setObjectName("SegmentButton"); button->setCheckable(true); button->setCursor(Qt::PointingHandCursor);
        m_appearance->addButton(button,int(mode)); modeRow->addWidget(button);
    }
    connect(m_appearance,&QButtonGroup::idClicked,this,[this](int id){emit appearanceRequested(Theme::Mode(id));});
    auto syncAppearance=[this]{if(auto *button=m_appearance->button(int(Theme::mode())))button->setChecked(true);};
    connect(Theme::notifier(),&Theme::Notifier::changed,this,syncAppearance);
    syncAppearance();
    addSetting(look,QStringLiteral("主题"),QStringLiteral("「跟随系统」会随 Windows 的深色 / 浅色设置自动切换。"),modeBar);
    col->addStretch(1);

    // Downloader
    col=addSection(QStringLiteral("下载器"),QStringLiteral("选择新任务使用的下载方式，已添加的任务继续由原下载器处理。"));
    m_downloaderTab=m_tabs->widget(m_tabs->count()-1);
    auto *choiceCard=settingsCard(col,QStringLiteral("下载方式"),"DownloaderChoiceSection");
    m_downloaderType=new SelectBox; m_downloaderType->setObjectName("DownloaderType");
    for(const auto &type:DownloaderSettings::types())m_downloaderType->addItem(DownloaderSettings::name(type),type);
    addSetting(choiceCard,QStringLiteral("使用的下载器"),QString(),m_downloaderType);
    m_downloaderHelp=label(QString(),"DownloaderHint",true,choiceCard);choiceCard->layout()->addWidget(m_downloaderHelp);

    m_programCard=settingsCard(col,QStringLiteral("程序位置"),"DownloaderProgramSection");
    auto *programRow=new QWidget(m_programCard);auto *programLayout=new QHBoxLayout(programRow);programLayout->setContentsMargins(0,0,0,0);programLayout->setSpacing(8);
    m_downloaderPath=new QLineEdit(programRow);m_downloaderPath->setObjectName("DownloaderProgram");m_downloaderPath->setPlaceholderText(QStringLiteral("选择已安装下载器的 .exe 文件"));programLayout->addWidget(m_downloaderPath,1);
    auto *detect=new QPushButton(QStringLiteral("自动查找"),programRow);detect->setIcon(Theme::icon("search"));programLayout->addWidget(detect);
    auto *pick=new QPushButton(QStringLiteral("浏览…"),programRow);pick->setIcon(Theme::icon("folder"));pick->setObjectName("BrowseDownloader");programLayout->addWidget(pick);
    m_programCard->layout()->addWidget(programRow);

    m_argumentsCard=settingsCard(col,QStringLiteral("启动参数"),"DownloaderArgumentSection");
    m_customArguments=new QLineEdit(m_argumentsCard);m_customArguments->setObjectName("DownloaderArguments");m_argumentsCard->layout()->addWidget(m_customArguments);
    m_argumentsCard->layout()->addWidget(label(QStringLiteral("{input} 资源链接或种子  ·  {saveDir} 保存目录  ·  {name} 资源标题\n包含空格的参数请使用双引号，例如 \"{input}\"。"),"SettingHint",true,m_argumentsCard));

    m_rpcCard=settingsCard(col,QStringLiteral("本机连接管理"),"DownloaderRpcSection");
    m_rpcEnabled=new ToggleSwitch;m_rpcEnabled->setObjectName("DownloaderRpcEnabled");
    addSetting(m_rpcCard,QStringLiteral("同步进度并在本应用中管理任务"),QStringLiteral("需先在下载器中启用 Web UI 或 RPC，仅连接本机地址。"),m_rpcEnabled);
    m_rpcUrl=new QLineEdit;m_rpcUrl->setObjectName("DownloaderRpcUrl");m_rpcUrl->setPlaceholderText(QStringLiteral("例如 http://127.0.0.1:8080"));
    m_rpcUser=new QLineEdit;m_rpcPassword=new QLineEdit;m_rpcPassword->setEchoMode(QLineEdit::Password);
    addSetting(m_rpcCard,QStringLiteral("连接地址"),QString(),m_rpcUrl);
    addSetting(m_rpcCard,QStringLiteral("用户名"),QString(),m_rpcUser);
    addSetting(m_rpcCard,QStringLiteral("密码 / aria2 密钥"),QStringLiteral("凭据保存在当前用户的配置文件中。"),m_rpcPassword);
    m_externalNote=label(QString(),"SettingHint",true);col->addWidget(m_externalNote);
    col->addStretch(1);
    connect(m_rpcEnabled,&QCheckBox::toggled,this,[this](bool enabled){
        m_programCard->setVisible(m_editType!="builtin"&&!enabled);
        m_rpcUrl->setEnabled(enabled);m_rpcUser->setEnabled(enabled);m_rpcPassword->setEnabled(enabled);
        m_externalNote->setText(enabled?QStringLiteral("连接后同步任务与进度。自动停止仅在本应用运行时执行；限速应用于新建任务，下载器的全局上限仍然有效。"):QStringLiteral("程序启动模式：进度和完成后停止由对应软件管理。"));
    });
    connect(m_downloaderType,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{saveDownloaderDraft();updateDownloaderFields();});
    connect(detect,&QPushButton::clicked,this,[this]{
        const QString path=DownloaderSettings::detect(m_editType);
        if(!path.isEmpty())m_downloaderPath->setText(path);
        else Ui::message(this,QStringLiteral("未找到程序"),QStringLiteral("未在常见安装位置找到此下载器，请点击浏览手动选择。"));
    });
    connect(pick,&QPushButton::clicked,this,[this]{
        const QString path=Ui::openExecutable(this,m_downloaderPath->text());
        if(!path.isEmpty())m_downloaderPath->setText(path);
    });

    // Storage
    col=addSection(QStringLiteral("存储"),QStringLiteral("新下载会在此目录下按「番剧名 / 字幕组 / 第 N 季」自动整理。"));
    auto *storage=settingsCard(col,QStringLiteral("下载位置"));
    auto *pathRow=new QWidget;
    auto *pathLayout=new QHBoxLayout(pathRow); pathLayout->setContentsMargins(0,0,0,0); pathLayout->setSpacing(8);
    m_savePath=new QLineEdit(pathRow);
    auto *browse=new QPushButton(QStringLiteral("浏览…"),pathRow); browse->setIcon(Theme::icon("folder"));
    connect(browse,&QPushButton::clicked,this,[this] {
        const QString path=Ui::chooseDirectory(this,QStringLiteral("选择下载目录"),m_savePath->text());
        if(!path.isEmpty())m_savePath->setText(path);
    });
    pathLayout->addWidget(m_savePath,1); pathLayout->addWidget(browse);
    addSetting(storage,QStringLiteral("默认下载目录"),QStringLiteral("只影响之后新建的任务。"),pathRow,true);
    col->addStretch(1);

    // Network
    col=addSection(QStringLiteral("网络"),QStringLiteral("内置下载器的连接方式与节点发现。外部软件使用自身的网络设置。"));
    auto *connection=settingsCard(col,QStringLiteral("连接"));
    m_interface=new SelectBox;
    m_interface->addItem(QStringLiteral("自动选择（默认路由）"),QString());
    QSet<QString> seenAddresses;
    for(const auto &iface:QNetworkInterface::allInterfaces()) {
        const auto flags=iface.flags();
        if(!(flags&QNetworkInterface::IsUp)||(flags&QNetworkInterface::IsLoopBack))continue;
        for(const auto &entry:iface.addressEntries()) {
            if(entry.ip().protocol()!=QAbstractSocket::IPv4Protocol||entry.ip().isLoopback())continue;
            const QString address=entry.ip().toString();
            if(seenAddresses.contains(address))continue;
            seenAddresses.insert(address);
            m_interface->addItem(QStringLiteral("%1 · %2").arg(address,iface.humanReadableName()),address);
        }
    }
    addSetting(connection,QStringLiteral("网络接口"),QStringLiteral("自动模式由系统为每个连接选择路由。"),m_interface);
    m_port=new QSpinBox; m_port->setRange(0,65535); m_port->setSpecialValueText(QStringLiteral("随机"));
    addSetting(connection,QStringLiteral("监听端口"),QStringLiteral("0 表示每次启动随机选择。"),m_port);
    m_encryption=new SelectBox;
    m_encryption->addItem(QStringLiteral("首选"),0); m_encryption->addItem(QStringLiteral("强制"),1); m_encryption->addItem(QStringLiteral("禁用"),2);
    addSetting(connection,QStringLiteral("协议加密"),QString(),m_encryption);

    auto *discovery=settingsCard(col,QStringLiteral("节点发现"));
    m_dht=new ToggleSwitch; m_lsd=new ToggleSwitch; m_upnp=new ToggleSwitch;
    addSetting(discovery,QStringLiteral("DHT 网络"),QStringLiteral("不依赖 Tracker 查找节点。"),m_dht);
    addSetting(discovery,QStringLiteral("本地节点发现（LSD）"),QStringLiteral("在局域网内寻找同一资源的节点。"),m_lsd);
    addSetting(discovery,QStringLiteral("UPnP / NAT-PMP 端口映射"),QStringLiteral("自动在路由器上开放监听端口。"),m_upnp);
    m_publicTrackerFallback=new ToggleSwitch; m_publicTrackerFallback->setObjectName("PublicTrackerFallback");
    addSetting(discovery,QStringLiteral("补充公共 Tracker"),QStringLiteral("仅为缺少 Tracker 的公开种子补充，私有种子保留原有地址。"),m_publicTrackerFallback);
    col->addStretch(1);

    // Transfer
    col=addSection(QStringLiteral("传输与队列"),QStringLiteral("内置下载器默认上传上限为 1 MiB/s，其余速率和数量不限；填 0 表示不限。"));
    auto *behaviour=settingsCard(col,QStringLiteral("任务行为"));
    m_stopOnCompletion=new ToggleSwitch; m_stopOnCompletion->setObjectName("StopOnCompletion");
    addSetting(behaviour,QStringLiteral("下载完成后自动停止"),QStringLiteral("停止下载和上传，任务保留在列表中。"),m_stopOnCompletion);
    m_sourceStall=new QSpinBox; m_sourceStall->setObjectName("SourceStallSeconds"); m_sourceStall->setRange(0,3600);
    m_sourceStall->setSuffix(QStringLiteral(" 秒")); m_sourceStall->setSpecialValueText(QStringLiteral("关闭"));
    addSetting(behaviour,QStringLiteral("无数据时切换来源"),QStringLiteral("尚未收到视频数据时尝试其他来源；已开始传输的任务保留原来源。"),m_sourceStall);

    auto *speed=settingsCard(col,QStringLiteral("速度与连接"));
    m_downloadLimit=limitBox("DownloadLimit",1000000,QStringLiteral(" KiB/s"));
    addSetting(speed,QStringLiteral("全局下载限速"),QString(),m_downloadLimit);
    m_uploadLimit=limitBox("UploadLimit",1000000,QStringLiteral(" KiB/s"));
    addSetting(speed,QStringLiteral("全局上传限速"),QStringLiteral("所有任务共享；1024 KiB/s = 1 MiB/s。"),m_uploadLimit);
    m_maxConnections=limitBox("ConnectionLimit",100000);
    addSetting(speed,QStringLiteral("最大连接数"),QString(),m_maxConnections);
    m_maxUploads=limitBox("UploadSlots",10000);
    addSetting(speed,QStringLiteral("最大上传槽"),QString(),m_maxUploads);

    auto *queue=settingsCard(col,QStringLiteral("队列"));
    m_queueing=new ToggleSwitch;
    addSetting(queue,QStringLiteral("启用任务队列"),QStringLiteral("超过下列数量的任务会排队等待。"),m_queueing);
    m_activeDownloads=limitBox("ActiveDownloads",100000);
    addSetting(queue,QStringLiteral("同时下载"),QString(),m_activeDownloads);
    m_activeSeeds=limitBox("ActiveSeeds",100000);
    addSetting(queue,QStringLiteral("同时做种"),QString(),m_activeSeeds);
    m_activeLimit=limitBox("ActiveLimit",100000);
    addSetting(queue,QStringLiteral("活动任务上限"),QString(),m_activeLimit);
    col->addStretch(1);

    root->addWidget(m_tabs,1);

    auto *saveBar=new QFrame(this); saveBar->setObjectName("SaveBar"); saveBar->setMaximumWidth(kColumnWidth-10);
    auto *actions=new QHBoxLayout(saveBar); actions->setContentsMargins(16,10,10,10); actions->setSpacing(8);
    actions->addWidget(label(QStringLiteral("修改在点击「应用设置」后生效"),"Faint",true,saveBar),1);
    auto *reload=new QPushButton(QStringLiteral("放弃更改"),saveBar);
    connect(reload,&QPushButton::clicked,this,&SettingsPage::load);
    actions->addWidget(reload);
    auto *applyButton=new QPushButton(QStringLiteral("应用设置"),saveBar);
    applyButton->setObjectName(QStringLiteral("Primary")); applyButton->setIcon(Theme::icon("check",&Theme::Palette::accentInk));
    connect(applyButton,&QPushButton::clicked,this,&SettingsPage::apply);
    actions->addWidget(applyButton);
    root->addWidget(saveBar);
    // Appearance applies instantly, so the apply bar would only mislead there.
    const int appearanceTab=0;
    connect(m_tabs,&QTabWidget::currentChanged,saveBar,[saveBar](int index){saveBar->setVisible(index!=appearanceTab);});
    saveBar->setVisible(m_tabs->currentIndex()!=appearanceTab);

    load();
}

void SettingsPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    load();
}

void SettingsPage::load()
{
    m_downloaderDraft=m_config->downloader;m_editType.clear();
    {QSignalBlocker block(m_downloaderType);m_downloaderType->setCurrentIndex(qMax(0,m_downloaderType->findData(m_downloaderDraft.selected)));}
    updateDownloaderFields();
    const auto &settings = m_config->engine;
    m_savePath->setText(settings.savePath);
    m_sourceStall->setValue(settings.sourceStallSeconds);
    int interfaceIndex = m_interface->findData(settings.bindAddress);
    if (interfaceIndex < 0 && !settings.bindAddress.isEmpty()) {
        m_interface->addItem(QStringLiteral("当前：%1").arg(settings.bindAddress), settings.bindAddress);
        interfaceIndex = m_interface->count() - 1;
    }
    m_interface->setCurrentIndex(qMax(0, interfaceIndex));
    m_port->setValue(settings.listenPort);
    m_dht->setChecked(settings.dht);
    m_lsd->setChecked(settings.lsd);
    m_upnp->setChecked(settings.upnp);
    m_publicTrackerFallback->setChecked(settings.publicTrackerFallback);
    m_encryption->setCurrentIndex(qBound(0, settings.encryption, 2));
    m_downloadLimit->setValue(settings.downloadLimit);
    m_uploadLimit->setValue(settings.uploadLimit);
    m_maxConnections->setValue(settings.maxConnections);
    m_maxUploads->setValue(settings.maxUploads);
    m_queueing->setChecked(settings.queueing);
    m_stopOnCompletion->setChecked(settings.stopOnCompletion);
    m_activeDownloads->setValue(settings.activeDownloads);
    m_activeSeeds->setValue(settings.activeSeeds);
    m_activeLimit->setValue(settings.activeLimit);
    m_status->setText(m_engine->running()
                          ? QStringLiteral("内置引擎运行中 · %1").arg(m_engine->version())
                          : QStringLiteral("内置引擎未启动"));
}

void SettingsPage::apply()
{
    saveDownloaderDraft();m_downloaderDraft.selected=m_downloaderType->currentData().toString();
    QString error;
    if(!ExternalDownloader::validate(m_downloaderDraft,&error)){Ui::message(this,QStringLiteral("下载器配置未完成"),error,Ui::Notice::Warning);return;}
    EngineSettings settings = m_config->engine;
    settings.savePath = m_savePath->text().trimmed();
    settings.bindAddress = m_interface->currentData().toString();
    settings.listenPort = m_port->value();
    settings.dht = m_dht->isChecked();
    settings.lsd = m_lsd->isChecked();
    settings.upnp = m_upnp->isChecked();
    settings.publicTrackerFallback=m_publicTrackerFallback->isChecked();
    settings.encryption = m_encryption->currentData().toInt();
    settings.downloadLimit = m_downloadLimit->value();
    settings.uploadLimit = m_uploadLimit->value();
    settings.maxConnections = m_maxConnections->value();
    settings.maxUploads = m_maxUploads->value();
    settings.queueing = m_queueing->isChecked();
    settings.stopOnCompletion=m_stopOnCompletion->isChecked();
    settings.sourceStallSeconds=m_sourceStall->value();
    settings.activeDownloads = m_activeDownloads->value();
    settings.activeSeeds = m_activeSeeds->value();
    settings.activeLimit = m_activeLimit->value();

    const AppConfig before=*m_config;
    m_config->engine=settings;m_config->paths.downloadDir=settings.savePath;m_config->downloader=m_downloaderDraft;
    if(!m_config->save()){*m_config=before;Ui::message(this,QStringLiteral("保存失败"),QStringLiteral("无法保存设置，请检查配置文件写入权限。"),Ui::Notice::Error);return;}
    emit downloaderChanged();
    m_engine->applySettings(settings);
    m_status->setText(QStringLiteral("已应用 · %1").arg(m_engine->version()));
}

void SettingsPage::saveDownloaderDraft()
{
    if(DownloaderSettings::supportsRpc(m_editType))m_downloaderDraft.rpc[m_editType]={m_rpcEnabled->isChecked(),m_rpcUrl->text().trimmed(),m_rpcUser->text(),m_rpcPassword->text()};
    if(!m_editType.isEmpty()&&m_editType!="builtin")m_downloaderDraft.programs[m_editType]=m_downloaderPath->text().trimmed();
    if(m_editType=="custom")m_downloaderDraft.customArguments=m_customArguments->text();
}

void SettingsPage::updateDownloaderFields()
{
    m_editType=m_downloaderType->currentData().toString();const bool external=m_editType!="builtin";
    const auto rpc=m_downloaderDraft.rpcProfile(m_editType);m_rpcEnabled->setChecked(rpc.enabled);m_rpcUrl->setText(rpc.url);m_rpcUser->setText(rpc.username);m_rpcPassword->setText(rpc.password);m_rpcCard->setVisible(DownloaderSettings::supportsRpc(m_editType));
    m_downloaderPath->setText(m_downloaderDraft.programs.value(m_editType));m_downloaderPath->parentWidget()->setEnabled(external);
    m_customArguments->setText(m_downloaderDraft.customArguments);m_customArguments->setEnabled(m_editType=="custom");
    m_programCard->setVisible(external&&!rpc.enabled);m_argumentsCard->setVisible(m_editType=="custom");m_externalNote->setVisible(external);
    m_rpcUrl->setEnabled(rpc.enabled);m_rpcUser->setEnabled(rpc.enabled);m_rpcPassword->setEnabled(rpc.enabled);
    m_externalNote->setText(rpc.enabled?QStringLiteral("连接后同步任务与进度。自动停止仅在本应用运行时执行；限速应用于新建任务，下载器的全局上限仍然有效。"):QStringLiteral("程序启动模式：进度和完成后停止由对应软件管理。"));
    QString help=QStringLiteral("直接管理下载进度、补缺集与本地视频，无需额外配置。");
    if(m_editType=="qbittorrent"||m_editType=="bitcomet")help=QStringLiteral("自动传入番剧保存目录和种子，已有任务继续由原下载器处理。");
    else if(m_editType=="transmission")help=QStringLiteral("请在 Transmission 添加任务窗口中核对保存目录。");
    else if(m_editType=="aria2")help=QStringLiteral("选择 aria2c.exe，支持后台运行与本地合集缺集选择。");
    else if(m_editType=="custom")help=QStringLiteral("选择其他下载器程序，并按该软件的说明填写启动参数。");
    m_downloaderHelp->setText(rpc.enabled?QStringLiteral("通过本机接口传入番剧目录，支持暂停、恢复、移除与完成后停止。"):help);
}

void SettingsPage::editDownloader(const QString &type)
{
    load();m_tabs->setCurrentWidget(m_downloaderTab);
    if(!type.isEmpty())m_downloaderType->setCurrentIndex(qMax(0,m_downloaderType->findData(type)));
}
