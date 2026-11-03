#include "SettingsPage.h"

#include "core/AppConfig.h"
#include "net/TorrentEngine.h"
#include "net/ExternalDownloader.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "ui/Controls.h"
#include "ui/Overlays.h"

#include <QAbstractSocket>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QSignalBlocker>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkInterface>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QTabWidget>

SettingsPage::SettingsPage(TorrentEngine *engine, AppConfig *config, QWidget *parent)
    : QWidget(parent)
    , m_engine(engine)
    , m_config(config)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(32,24,32,20);
    root->setSpacing(18);

    auto *header=new QHBoxLayout;
    header->addWidget(pageHeading(QStringLiteral("偏好设置"),QStringLiteral("按你的习惯，安排存储与传输。")),1);
    m_status=label(QString(),"Faint",false,this); header->addWidget(m_status); root->addLayout(header);
    auto *tabs=new QTabWidget(this);m_tabs=tabs;
    QWidget *body=nullptr;
    auto addSection=[&](const QString &name,const QString &description) {
        auto *scroll=new QScrollArea(tabs); scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        body=new QWidget(scroll);
        auto *form=new QFormLayout(body); form->setContentsMargins(26,26,26,26);
        form->setHorizontalSpacing(32); form->setVerticalSpacing(16);
        form->setLabelAlignment(Qt::AlignLeft|Qt::AlignVCenter);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->addRow(label(description,"Muted",true));
        scroll->setWidget(body); tabs->addTab(scroll,name); return form;
    };
    auto *form=addSection(QStringLiteral("下载器"),QStringLiteral("按你的习惯选择下载方式，切换仅影响新任务。"));
    m_downloaderTab=tabs->widget(tabs->count()-1);
    auto addCard=[&](const QString &title,const char *name) {
        auto *card=new QFrame(body);card->setObjectName(name);card->setProperty("settingsCard",true);
        auto *col=new QVBoxLayout(card);col->setSizeConstraint(QLayout::SetMinimumSize);col->setContentsMargins(20,18,20,18);col->setSpacing(12);
        col->addWidget(label(title,"SettingsCardTitle"));form->addRow(card);return card;
    };
    auto *choiceCard=addCard(QStringLiteral("下载方式"),"DownloaderChoiceSection");
    m_downloaderType=new SelectBox(choiceCard);m_downloaderType->setObjectName("DownloaderType");
    for(const auto &type:DownloaderSettings::types())m_downloaderType->addItem(DownloaderSettings::name(type),type);
    choiceCard->layout()->addWidget(m_downloaderType);
    m_downloaderHelp=label(QString(),"DownloaderHint",true,choiceCard);choiceCard->layout()->addWidget(m_downloaderHelp);
    m_programCard=addCard(QStringLiteral("程序位置"),"DownloaderProgramSection");
    auto *programRow=new QWidget(m_programCard);auto *programLayout=new QHBoxLayout(programRow);programLayout->setContentsMargins(0,0,0,0);programLayout->setSpacing(8);
    m_downloaderPath=new QLineEdit(programRow);m_downloaderPath->setObjectName("DownloaderProgram");m_downloaderPath->setPlaceholderText(QStringLiteral("选择已安装下载器的 .exe 文件"));programLayout->addWidget(m_downloaderPath,1);
    auto *detect=new QPushButton(QStringLiteral("自动查找"),programRow);detect->setIcon(Theme::icon("search"));programLayout->addWidget(detect);
    auto *pick=new QPushButton(QStringLiteral("浏览…"),programRow);pick->setIcon(Theme::icon("folder"));pick->setObjectName("BrowseDownloader");programLayout->addWidget(pick);
    m_programCard->layout()->addWidget(programRow);
    m_argumentsCard=addCard(QStringLiteral("启动参数"),"DownloaderArgumentSection");
    m_customArguments=new QLineEdit(m_argumentsCard);m_customArguments->setObjectName("DownloaderArguments");m_argumentsCard->layout()->addWidget(m_customArguments);
    m_argumentsCard->layout()->addWidget(label(QStringLiteral("{input} 资源链接或种子    ·    {saveDir} 保存目录    ·    {name} 资源标题"),"Faint",true,m_argumentsCard));
    m_argumentsCard->layout()->addWidget(label(QStringLiteral("包含空格的参数请使用双引号，例如：\"{input}\"。按对应软件的参数格式填写即可。"),"Faint",true,m_argumentsCard));
    m_rpcCard=addCard(QStringLiteral("本机连接管理"),"DownloaderRpcSection");
    m_rpcEnabled=new QCheckBox(QStringLiteral("同步进度，并在本应用中管理任务"),m_rpcCard);m_rpcEnabled->setObjectName("DownloaderRpcEnabled");m_rpcCard->layout()->addWidget(m_rpcEnabled);
    auto *rpcForm=new QFormLayout;rpcForm->setSpacing(12);m_rpcUrl=new QLineEdit(m_rpcCard);m_rpcUrl->setObjectName("DownloaderRpcUrl");m_rpcUser=new QLineEdit(m_rpcCard);m_rpcPassword=new QLineEdit(m_rpcCard);m_rpcPassword->setEchoMode(QLineEdit::Password);
    rpcForm->addRow(QStringLiteral("连接地址"),m_rpcUrl);rpcForm->addRow(QStringLiteral("用户名"),m_rpcUser);rpcForm->addRow(QStringLiteral("密码 / aria2 密钥"),m_rpcPassword);static_cast<QVBoxLayout*>(m_rpcCard->layout())->addLayout(rpcForm);
    m_rpcCard->layout()->addWidget(label(QStringLiteral("请先在下载器中启用 Web UI 或 RPC。本应用仅连接本机地址；凭据保存在当前用户的配置文件中。"),"Faint",true,m_rpcCard));
    m_externalNote=label(QStringLiteral("外部任务的进度、限速和完成后停止由对应软件管理，应用会保留发送记录。"),"Faint",true,body);form->addRow(m_externalNote);
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
    form=addSection(QStringLiteral("存储"),QStringLiteral("为接下来的下载选择一个合适的位置。"));
    auto *pathRow = new QWidget(body);
    auto *pathLayout = new QHBoxLayout(pathRow);
    pathLayout->setContentsMargins(0, 0, 0, 0);
    pathLayout->setSpacing(6);
    m_savePath = new QLineEdit(pathRow);
    auto *browse = new QPushButton(QStringLiteral("浏览…"), pathRow);
    browse->setFixedWidth(64);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString path = Ui::chooseDirectory(
            this, QStringLiteral("选择下载目录"), m_savePath->text());
        if (!path.isEmpty())
            m_savePath->setText(path);
    });
    pathLayout->addWidget(m_savePath, 1);
    pathLayout->addWidget(browse);
    form->addRow(QStringLiteral("默认下载目录"), pathRow);

    form=addSection(QStringLiteral("网络"),QStringLiteral("内置下载器的连接方式与节点发现。外部软件使用自身的网络设置。"));
    m_interface = new SelectBox(body);
    m_interface->addItem(QStringLiteral("自动选择（默认路由）"), QString());
    QSet<QString> seenAddresses;
    for (const auto &iface : QNetworkInterface::allInterfaces()) {
        const auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || (flags & QNetworkInterface::IsLoopBack))
            continue;
        for (const auto &entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol || entry.ip().isLoopback())
                continue;
            const QString address = entry.ip().toString();
            if (seenAddresses.contains(address))
                continue;
            seenAddresses.insert(address);
            m_interface->addItem(QStringLiteral("%1 · %2").arg(address, iface.humanReadableName()), address);
        }
    }
    form->addRow(QStringLiteral("网络接口"), m_interface);

    m_port = new QSpinBox(body);
    m_port->setRange(0, 65535);
    m_port->setSpecialValueText(QStringLiteral("随机"));
    form->addRow(QStringLiteral("监听端口"), m_port);

    m_dht = new QCheckBox(QStringLiteral("启用 DHT"), body);
    m_lsd = new QCheckBox(QStringLiteral("启用 LSD"), body);
    m_upnp = new QCheckBox(QStringLiteral("启用 UPnP / NAT-PMP"), body);
    form->addRow(QStringLiteral("网络发现"), m_dht);
    form->addRow(QString(), m_lsd);
    form->addRow(QString(), m_upnp);
    m_publicTrackerFallback=new QCheckBox(QStringLiteral("为无 Tracker 的公开种子补充公共 Tracker"),body);
    m_publicTrackerFallback->setObjectName("PublicTrackerFallback");m_publicTrackerFallback->setToolTip(QStringLiteral("仅为缺少节点发现地址的公开种子补充，私有种子保留原有地址。"));form->addRow(QString(),m_publicTrackerFallback);

    m_encryption = new SelectBox(body);
    m_encryption->addItem(QStringLiteral("首选"), 0);
    m_encryption->addItem(QStringLiteral("强制"), 1);
    m_encryption->addItem(QStringLiteral("禁用"), 2);
    form->addRow(QStringLiteral("协议加密"), m_encryption);

    form=addSection(QStringLiteral("传输与队列"),QStringLiteral("内置下载器默认上传上限为 1 MiB/s，其余速率和数量不限。外部软件使用自身的传输设置。"));
    m_stopOnCompletion=new QCheckBox(QStringLiteral("下载完成后自动停止下载和上传"),body);
    m_stopOnCompletion->setObjectName("StopOnCompletion");
    form->addRow(QStringLiteral("完成后"),m_stopOnCompletion);
    m_sourceStall=new QSpinBox(body);m_sourceStall->setObjectName("SourceStallSeconds");m_sourceStall->setRange(0,3600);m_sourceStall->setSuffix(QStringLiteral(" 秒"));m_sourceStall->setSpecialValueText(QStringLiteral("关闭"));
    m_sourceStall->setToolTip(QStringLiteral("内置下载器在尚未收到视频数据时尝试其他来源。已开始传输的任务保留原来源，避免重复文件。"));form->addRow(QStringLiteral("无数据时切换来源"),m_sourceStall);
    m_downloadLimit = new QSpinBox(body);
    m_downloadLimit->setObjectName("DownloadLimit");
    m_downloadLimit->setRange(0, 1000000);
    m_downloadLimit->setSuffix(QStringLiteral(" KiB/s"));
    m_downloadLimit->setSpecialValueText(QStringLiteral("不限"));
    form->addRow(QStringLiteral("全局下载限速"), m_downloadLimit);

    m_uploadLimit = new QSpinBox(body);
    m_uploadLimit->setObjectName("UploadLimit");
    m_uploadLimit->setRange(0, 1000000);
    m_uploadLimit->setSuffix(QStringLiteral(" KiB/s"));
    m_uploadLimit->setToolTip(QStringLiteral("1024 KiB/s = 1 MiB/s，所有任务共享此上传上限。"));
    m_uploadLimit->setSpecialValueText(QStringLiteral("不限"));
    form->addRow(QStringLiteral("全局上传限速"), m_uploadLimit);

    m_maxConnections = new QSpinBox(body);
    m_maxConnections->setObjectName("ConnectionLimit");
    m_maxConnections->setRange(0, 100000);
    m_maxConnections->setSpecialValueText(QStringLiteral("不限"));
    form->addRow(QStringLiteral("最大连接数"), m_maxConnections);

    m_maxUploads = new QSpinBox(body);
    m_maxUploads->setObjectName("UploadSlots");
    m_maxUploads->setRange(0, 10000);
    m_maxUploads->setSpecialValueText(QStringLiteral("不限"));
    form->addRow(QStringLiteral("最大上传槽"), m_maxUploads);

    m_queueing = new QCheckBox(QStringLiteral("启用任务队列"), body);
    form->addRow(QStringLiteral("队列"), m_queueing);

    m_activeDownloads = new QSpinBox(body);
    m_activeDownloads->setObjectName("ActiveDownloads");
    m_activeDownloads->setRange(0, 100000);
    m_activeDownloads->setSpecialValueText(QStringLiteral("不限"));
    form->addRow(QStringLiteral("同时下载"), m_activeDownloads);

    m_activeSeeds = new QSpinBox(body);
    m_activeSeeds->setObjectName("ActiveSeeds");
    m_activeSeeds->setRange(0, 100000);
    m_activeSeeds->setSpecialValueText(QStringLiteral("不限"));
    form->addRow(QStringLiteral("同时做种"), m_activeSeeds);

    m_activeLimit = new QSpinBox(body);
    m_activeLimit->setObjectName("ActiveLimit");
    m_activeLimit->setRange(0, 100000);
    m_activeLimit->setSpecialValueText(QStringLiteral("不限"));
    form->addRow(QStringLiteral("活动任务上限"), m_activeLimit);

    root->addWidget(tabs, 1);

    auto *actions = new QHBoxLayout();
    actions->addWidget(label(QStringLiteral("点击应用后生效；下载目录只影响新任务。"),
                             QStringLiteral("Faint"), true), 1);
    auto *reload = new QPushButton(QStringLiteral("重新加载"), this);
    connect(reload, &QPushButton::clicked, this, &SettingsPage::load);
    actions->addWidget(reload);
    auto *applyButton = new QPushButton(QStringLiteral("应用设置"), this);
    applyButton->setObjectName(QStringLiteral("Primary"));
    connect(applyButton, &QPushButton::clicked, this, &SettingsPage::apply);
    actions->addWidget(applyButton);
    root->addLayout(actions);

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
                          ? QStringLiteral("运行中 · %1").arg(m_engine->version())
                          : QStringLiteral("引擎未启动"));
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
