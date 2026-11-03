#include "AnimeDetailPage.h"

#include "core/Catalog.h"
#include "core/LocalLibrary.h"
#include "net/BangumiClient.h"
#include "net/SearchAggregator.h"
#include "ui/FlowLayout.h"
#include "ui/Controls.h"
#include "ui/ImageLoader.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <limits>

namespace {

QLabel *statLabel(const QString &text)
{
    auto *label = new QLabel(text);
    label->setStyleSheet(QStringLiteral("color:#243b3a;font-size:10pt;"));
    return label;
}

} // namespace

AnimeDetailPage::AnimeDetailPage(SearchAggregator *aggregator, BangumiClient *bangumi, Catalog *catalog,
                                 ImageLoader *images, LocalLibrary *library, std::function<bool(int)> isFollowing, QWidget *parent)
    : QWidget(parent)
    , m_library(library)
    , m_aggregator(aggregator)
    , m_bangumi(bangumi)
    , m_catalog(catalog)
    , m_images(images)
    , m_isFollowing(std::move(isFollowing))
{
    m_renderTimer = new QTimer(this);
    m_renderTimer->setSingleShot(true);
    m_renderTimer->setInterval(180);
    connect(m_renderTimer, &QTimer::timeout, this, &AnimeDetailPage::render);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea(this);
    m_scroll = scroll;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scroll);

    auto *body = new QWidget();
    body->setObjectName(QStringLiteral("ScrollBody"));
    scroll->setWidget(body);
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(32,20,32,24);
    layout->setSpacing(18);

    auto *top = new QHBoxLayout();
    auto *back = new QPushButton(this);
    back->setIcon(Theme::icon("back"));
    back->setObjectName(QStringLiteral("IconButton"));
    back->setFixedSize(34, 34);
    back->setToolTip(QStringLiteral("返回 (Esc)"));
    connect(back, &QPushButton::clicked, this, &AnimeDetailPage::backRequested);
    top->addWidget(back);
    top->addWidget(label(QStringLiteral("番剧详情"), QStringLiteral("Muted")));
    top->addStretch(1);
    auto *refresh = new QPushButton(QStringLiteral("重新搜索资源"), this);
    m_resourceSearch=refresh;refresh->setObjectName("ResourceSearchAction");
    connect(refresh, &QPushButton::clicked, this, [this]{
        if(m_localOnly)emit resourceSearchRequested(m_subject);
        else {resetFilters();startSearch();}
    });
    top->addWidget(refresh);
    layout->addLayout(top);

    buildHero(layout);
    buildSourceStrip(layout);

    auto *section = new QHBoxLayout();
    m_sectionTitle=label(QStringLiteral("可用资源"),QStringLiteral("H2"));
    section->addWidget(m_sectionTitle);
    m_countLabel = label(QString(), QStringLiteral("Faint"));
    section->addWidget(m_countLabel);
    section->addStretch(1);
    layout->addLayout(section);

    buildFilters(layout);

    m_groupsBox = new QWidget();
    m_groupsLayout = new QVBoxLayout(m_groupsBox);
    m_groupsLayout->setContentsMargins(0, 0, 0, 0);
    m_groupsLayout->setSpacing(12);
    layout->addWidget(m_groupsBox);

    m_empty = new EmptyState(Icons::SEARCH, QStringLiteral("正在搜索资源…"),
                             QStringLiteral("从已启用的资源站并发检索，结果会逐步出现。"));
    m_empty->setMinimumHeight(150);
    layout->addWidget(m_empty);
    layout->addStretch(1);

    connect(m_library,&LocalLibrary::changed,this,&AnimeDetailPage::scheduleRender);
    connect(m_aggregator, &SearchAggregator::batchReady, this, &AnimeDetailPage::onBatch);
    connect(m_aggregator, &SearchAggregator::siteFinished, this, &AnimeDetailPage::onSiteStatus);
    connect(m_aggregator, &SearchAggregator::finished, this, &AnimeDetailPage::onFinished);
    connect(m_aggregator,&SearchAggregator::started,this,[this](int count){m_statusTotal=count;m_statusDone=0;m_progress->setText(QStringLiteral("检索中 0/%1").arg(count));});
}

void AnimeDetailPage::buildHero(QVBoxLayout *layout)
{
    auto *hero = new QFrame();
    hero->setObjectName(QStringLiteral("DetailHero"));
    auto *row = new QHBoxLayout(hero);
    row->setContentsMargins(0,8,0,14);
    row->setSpacing(28);

    m_poster = new QLabel(hero);
    m_poster->setFixedSize(172,244);
    m_poster->setAlignment(Qt::AlignCenter);
    m_poster->setStyleSheet(QStringLiteral("background:#e6f1ef;border-radius:12px;color:#718785;"));
    row->addWidget(m_poster, 0, Qt::AlignTop);

    auto *col = new QVBoxLayout();
    col->setSpacing(6);

    m_title = label(QString(), QStringLiteral("H1"), true);
    m_subtitle = label(QString(), QStringLiteral("Muted"), true);
    m_stats = new QLabel();
    m_stats->setObjectName(QStringLiteral("Muted"));
    m_stats->setWordWrap(true);
    m_summary = label(QString(), QStringLiteral("Muted"), true);
    m_summary->setMaximumHeight(92);
    m_summary->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    col->addWidget(m_title);
    col->addWidget(m_subtitle);
    col->addWidget(m_stats);

    m_tagsBox = new QWidget();
    m_tagsLayout = new FlowLayout(nullptr, 6, 6);
    m_tagsBox->setLayout(m_tagsLayout);
    col->addWidget(m_tagsBox);

    col->addWidget(m_summary);

    auto *actions = new QHBoxLayout();
    actions->setSpacing(8);
    m_followButton = new QPushButton(QStringLiteral("＋ 追番"), this);
    m_followButton->setObjectName(QStringLiteral("Primary"));
    m_followButton->setCheckable(true);
    connect(m_followButton, &QPushButton::toggled, this, [this](bool checked) {
        m_followButton->setText(checked ? QStringLiteral("✓ 已追番") : QStringLiteral("＋ 追番"));
        emit followChanged(m_subject, checked);
    });
    actions->addWidget(m_followButton);

    auto *bangumiButton = new QPushButton(QStringLiteral("Bangumi 页面"), this);
    connect(bangumiButton, &QPushButton::clicked, this, [this] {
        if (!m_subject.bangumiUrl.isEmpty())
            QDesktopServices::openUrl(QUrl(m_subject.bangumiUrl));
    });
    actions->addWidget(bangumiButton);

    m_moreButton = new QPushButton(QStringLiteral("展开简介"), this);
    m_moreButton->setObjectName(QStringLiteral("Ghost"));
    connect(m_moreButton, &QPushButton::clicked, this, [this] {
        const bool expanded = m_summary->maximumHeight() > 92;
        m_summary->setMaximumHeight(expanded ? 92 : QWIDGETSIZE_MAX);
        m_moreButton->setText(expanded ? QStringLiteral("展开简介") : QStringLiteral("收起简介"));
    });
    actions->addWidget(m_moreButton);
    actions->addStretch(1);
    col->addLayout(actions);
    col->addStretch(1);

    row->addLayout(col, 1);
    layout->addWidget(hero);
}

void AnimeDetailPage::buildSourceStrip(QVBoxLayout *layout)
{
    auto *panel = new QFrame();
    m_sourcePanel=panel;
    panel->setObjectName(QStringLiteral("Panel"));
    auto *row = new QHBoxLayout(panel);
    row->setContentsMargins(16, 9, 16, 9);
    row->setSpacing(14);

    m_progress = label(QStringLiteral("待检索"), QStringLiteral("Muted"));
    row->addWidget(m_progress);

    m_dotsBox = new QWidget(panel);
    m_dotsLayout = new FlowLayout(m_dotsBox,12,10);
    m_dotsLayout->setContentsMargins(0, 0, 0, 0);

    refreshSources();
    row->addWidget(m_dotsBox,1);
    layout->addWidget(panel);
}

void AnimeDetailPage::refreshSources()
{
    while(auto *item=m_dotsLayout->takeAt(0)){if(auto *widget=item->widget()){widget->hide();widget->deleteLater();}delete item;}
    m_dots.clear();
    for (const auto &site : m_catalog->sites(true)) {
        auto *widget = new QWidget(m_dotsBox);
        auto *dotLayout = new QHBoxLayout(widget);
        dotLayout->setContentsMargins(0, 0, 0, 0);
        dotLayout->setSpacing(5);
        auto *dot = new StatusDot(QStringLiteral("busy"), widget);
        auto *name = label(site.name, QStringLiteral("Faint"), false, widget);
        dotLayout->addWidget(dot);
        dotLayout->addWidget(name);
        m_dotsLayout->addWidget(widget);
        m_dots.insert(site.id, {dot, name});
    }
}

void AnimeDetailPage::buildFilters(QVBoxLayout *layout)
{
    m_filterBar = new QFrame();
    m_filterBar->setObjectName(QStringLiteral("Panel"));
    auto *v = new QVBoxLayout(m_filterBar);
    v->setContentsMargins(16, 12, 16, 12);
    v->setSpacing(8);

    auto makeCombo = [](const QVector<QPair<QString, QVariant>> &items) {
        auto *combo = new SelectBox();
        for (const auto &[text, data] : items)
            combo->addItem(text, data);
        return combo;
    };

    m_season = makeCombo({{QStringLiteral("全部"), -1}});
    m_kind = makeCombo({{QStringLiteral("全部"), QStringLiteral("all")},
                        {QStringLiteral("合集"), QStringLiteral("batch")},
                        {QStringLiteral("单集"), QStringLiteral("single")}});
    m_resolution = makeCombo({{QStringLiteral("全部"), QString()},
                              {QStringLiteral("4K"), QStringLiteral("2160p")},
                              {QStringLiteral("1080p"), QStringLiteral("1080p")},
                              {QStringLiteral("720p"), QStringLiteral("720p")}});
    m_subtitleFilter = makeCombo({{QStringLiteral("全部"), QString()},
                            {QStringLiteral("中文"), QStringLiteral("zh")},
                            {QStringLiteral("简体"), QStringLiteral("zh-Hans")},
                            {QStringLiteral("繁体"), QStringLiteral("zh-Hant")},
                            {QStringLiteral("英文"), QStringLiteral("en")}});
    m_downloadStatus=makeCombo({{QStringLiteral("全部"),"all"},{QStringLiteral("已下载"),"downloaded"},{QStringLiteral("未下载"),"missing"}});
    m_downloadStatus->setObjectName("DownloadStatusFilter");
    m_sort = makeCombo({{QStringLiteral("最近更新"), QStringLiteral("latest")},
                        {QStringLiteral("做种最多"), QStringLiteral("seeders")},
                        {QStringLiteral("资源最多"), QStringLiteral("count")},
                        {QStringLiteral("名称"), QStringLiteral("name")}});

    auto *choices=new QHBoxLayout;
    choices->setSpacing(16);
    auto addField=[choices](const QString &name,QComboBox *combo) {
        auto *col=new QVBoxLayout; col->setSpacing(6);
        col->addWidget(label(name,"Faint")); col->addWidget(combo);
        choices->addLayout(col,1);
    };
    addField(QStringLiteral("放送季度"),m_season);
    addField(QStringLiteral("资源类型"),m_kind);
    addField(QStringLiteral("清晰度"),m_resolution);
    addField(QStringLiteral("字幕语言"),m_subtitleFilter);
    addField(QStringLiteral("状态"),m_downloadStatus);
    v->addLayout(choices);
    auto *tools=new QHBoxLayout; tools->setSpacing(12);
    m_text=new QLineEdit; m_text->setPlaceholderText(QStringLiteral("筛选资源标题…"));
    m_text->setClearButtonEnabled(true); tools->addWidget(m_text,1);
    m_hdrOnly=new QCheckBox(QStringLiteral("HDR / 杜比视界")); tools->addWidget(m_hdrOnly);
    tools->addWidget(m_sort);
    auto *reset=new QPushButton(QStringLiteral("重置")); reset->setObjectName("Ghost"); tools->addWidget(reset);
    v->addSpacing(6); v->addLayout(tools);

    for (auto *combo : {m_season, m_kind, m_resolution, m_subtitleFilter, m_sort, m_downloadStatus})
        connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this, &AnimeDetailPage::scheduleRender);
    connect(m_hdrOnly, &QCheckBox::toggled, this, &AnimeDetailPage::scheduleRender);
    connect(m_text, &QLineEdit::textChanged, this, &AnimeDetailPage::scheduleRender);
    connect(reset, &QPushButton::clicked, this, &AnimeDetailPage::resetFilters);

    layout->addWidget(m_filterBar);
}

void AnimeDetailPage::showSubject(const AnimeSubject &subject,bool localOnly)
{
    m_groupExpanded.clear();
    m_groupShowAll.clear();
    m_subject = subject;
    m_localOnly=localOnly;
    m_resourceSearch->setText(localOnly?QStringLiteral("查找可下载资源"):QStringLiteral("重新搜索资源"));
    m_sourcePanel->setVisible(!localOnly);
    m_sectionTitle->setText(localOnly?QStringLiteral("本地视频"):QStringLiteral("可用资源"));
    m_downloadStatus->setEnabled(!localOnly);
    m_releases.clear();
    clearGroups();
    resetFilters();
    updateHero();
    startSearch();
    if (m_scroll)
        m_scroll->verticalScrollBar()->setValue(0);
}

void AnimeDetailPage::updateHero()
{
    m_title->setText(m_subject.displayName());
    QStringList alt;
    if (!m_subject.nameJp.isEmpty() && m_subject.nameJp != m_subject.displayName())
        alt << m_subject.nameJp;
    if (!m_subject.nameEn.isEmpty() && m_subject.nameEn != m_subject.displayName())
        alt << m_subject.nameEn;
    m_subtitle->setText(alt.join(QStringLiteral("  /  ")));
    m_subtitle->setVisible(!alt.isEmpty());

    QStringList stats;
    if (m_subject.rating > 0)
        stats << QStringLiteral("★ %1").arg(m_subject.rating, 0, 'f', 1)
                     + (m_subject.rank > 0 ? QStringLiteral("  ·  #%1").arg(m_subject.rank) : QString());
    if (m_subject.episodeCount > 0)
        stats << QStringLiteral("%1 话").arg(m_subject.episodeCount);
    if (!m_subject.airDate.isEmpty())
        stats << m_subject.airDate;
    if (!m_subject.studio.isEmpty())
        stats << m_subject.studio;
    m_stats->setText(stats.join(QStringLiteral("     ")));

    m_tagsLayout->clear();
    for (int i = 0; i < m_subject.tags.size() && i < 10; ++i)
        m_tagsLayout->addWidget(new Chip(m_subject.tags.at(i), Theme::SURFACE3, Theme::TEXT2));
    m_tagsBox->setVisible(!m_subject.tags.isEmpty());

    m_summary->setText(m_subject.summary.isEmpty() ? QStringLiteral("暂无简介。") : m_subject.summary);
    m_summary->setMaximumHeight(92);
    m_moreButton->setText(QStringLiteral("展开简介"));
    m_moreButton->setVisible(m_subject.summary.size() > 140);

    const bool following = m_isFollowing(m_subject.bangumiId);
    m_followButton->blockSignals(true);
    m_followButton->setChecked(following);
    m_followButton->setText(following ? QStringLiteral("✓ 已追番") : QStringLiteral("＋ 追番"));
    m_followButton->blockSignals(false);

    m_poster->setPixmap(QPixmap());
    m_poster->setText(QStringLiteral("…"));
    const QString cover = BangumiClient::coverUrl(m_subject.coverUrl, 400);
    if (cover.isEmpty()) {
        m_poster->setText(QStringLiteral("无封面"));
        return;
    }
    const int id = m_subject.bangumiId;
    m_images->request(cover, [this, id](QImage image) {
        if (m_subject.bangumiId != id || image.isNull())
            return;
        m_poster->setPixmap(coverPixmap(image, 172, 244, 10));
        m_poster->setText(QString());
    });
}

void AnimeDetailPage::startSearch()
{
    if (m_subject.bangumiId == 0)
        return;
    m_aggregator->cancel();const int generation=++m_searchGeneration;
    refreshSources();m_searching=true;
    m_releases.clear();
    clearGroups();
    m_groupLimit = 3;
    m_countLabel->clear();
    m_empty->setText(QStringLiteral("正在搜索资源…"),
                     QStringLiteral("从已启用的资源站并发检索，结果会逐步出现。"));
    m_empty->show();

    m_statusDone = 0;
    m_statusTotal = m_dots.size();
    for (auto it = m_dots.begin(); it != m_dots.end(); ++it) {
        it->first->setState(QStringLiteral("busy"));
        it->second->setToolTip(QString());
    }
    m_progress->setText(QStringLiteral("检索中 0/%1").arg(m_statusTotal));

    render();
    if(m_localOnly){m_searching=false;onFinished();return;}
    if (m_subject.aliases.isEmpty()) {
        const int id = m_subject.bangumiId;
        m_bangumi->getSubject(id, [this, id,generation](AnimeSubject full, QString error) {
            if (m_subject.bangumiId != id||generation!=m_searchGeneration)
                return;
            if (!error.isEmpty() || full.bangumiId != id) {searchWith(m_subject);return;}
            full.airWeekday = full.airWeekday > 0 ? full.airWeekday : m_subject.airWeekday;
            m_subject = full;
            updateHero();
            searchWith(m_subject);
        });
    } else {
        searchWith(m_subject);
    }
}

void AnimeDetailPage::searchWith(const AnimeSubject &subject)
{
    const QString query = subject.nameCn.isEmpty() ? subject.displayName() : subject.nameCn;
    m_aggregator->search(query, subject);
}

void AnimeDetailPage::onBatch(QVector<ResourceResult> results)
{
    const auto keys = subjectKeys(m_subject);
    QVector<ResourceResult> relevant;
    for (auto &result : results) {
        if(!result.animeId.isEmpty()&&result.animeId!=QString::number(m_subject.bangumiId))continue;
        if (isRelevant(result.title, keys))
            relevant.push_back(std::move(result));
    }
    if (relevant.isEmpty())
        return;
    emit resourcesDiscovered(m_subject,relevant);
    const auto annotated = annotate(relevant);
    for(const auto &release:annotated) {
        auto found=std::find_if(m_releases.begin(),m_releases.end(),[&](const Release &prior){
            return prior.result.rowKey()==release.result.rowKey();
        });
        if(found==m_releases.end())m_releases.append(release);
        else mergeResource(found->result,release.result);
    }
    scheduleRender();
}

void AnimeDetailPage::onSiteStatus(SiteStatus status)
{
    ++m_statusDone;
    auto it = m_dots.find(status.siteId);
    if (it != m_dots.end()) {
        if (status.ok && status.count > 0)
            it->first->setState(QStringLiteral("ok"));
        else if (status.ok)
            it->first->setState(QStringLiteral("warn"));
        else
            it->first->setState(QStringLiteral("err"));
        QString tooltip = QStringLiteral("%1 条 · %2s").arg(status.count).arg(status.elapsed, 0, 'f', 1);
        if (!status.error.isEmpty())
            tooltip += QStringLiteral("\n") + status.error;
        it->second->setToolTip(tooltip);
    }
    m_progress->setText(QStringLiteral("检索中 %1/%2").arg(m_statusDone).arg(m_statusTotal));
}

void AnimeDetailPage::onFinished()
{
    m_searching=false;
    m_progress->setText(QStringLiteral("已检索 %1 个站点").arg(m_statusTotal));
    setSeasons(seasonsPresent(m_releases+m_library->localReleases(m_subject.bangumiId)));
    render();
}

void AnimeDetailPage::scheduleRender()
{
    m_renderTimer->start();
}

void AnimeDetailPage::render()
{
    if (m_subject.bangumiId == 0)
        return;

    const Filters filter = filters();
    const QString state=m_downloadStatus->currentData().toString();
    const auto local=m_library->localReleases(m_subject.bangumiId);
    QVector<Release> pool=(m_localOnly||state=="downloaded")?local:m_releases;
    if(!m_localOnly&&state=="all") {
        for(const auto &r:local) {
            bool represented=false;
            for(const auto &online:m_releases)
                if(m_library->downloaded(m_subject.bangumiId,online.result)
                   && m_library->playableFiles(m_subject.bangumiId,online.result).contains(r.result.localPath)) {represented=true;break;}
            if(!represented)pool.append(r);
        }
    }
    QVector<Release> shown;
    for(const auto &release:pool) {
        const bool downloaded=m_library->downloaded(m_subject.bangumiId,release.result);
        if(state=="missing"&&downloaded)continue;
        if(passes(release,filter))shown.append(release);
    }

    const auto groups = groupReleases(shown, sortKey());
    clearGroups();
    const int limit = groups.size() < m_groupLimit ? static_cast<int>(groups.size()) : m_groupLimit;
    for (int i = 0; i < limit; ++i) {
        const auto &group = groups.at(i);
        auto *module = new QFrame(m_groupsBox);
        module->setObjectName(QStringLiteral("GroupModule"));
        auto *moduleLayout = new QVBoxLayout(module);
        moduleLayout->setContentsMargins(0, 0, 0, 0);
        moduleLayout->setSpacing(0);

        const bool expanded = m_groupExpanded.value(group.name, i < 3);
        const bool showAll = m_groupShowAll.value(group.name, false);
        const QString baseText = QStringLiteral("%1 个资源").arg(group.releases.size());
        auto *header = new QPushButton(module);
        header->setObjectName(QStringLiteral("GroupHeader"));
        header->setCheckable(true);
        header->setChecked(expanded);
        header->setCursor(Qt::PointingHandCursor);
        header->setText((expanded ? Icons::CHEVRON_DOWN : Icons::CHEVRON_RIGHT) + QStringLiteral("  ")
                        + group.name + QStringLiteral("   ·   ") + baseText);
        auto *groupBar=new QHBoxLayout;
        groupBar->setContentsMargins(0,0,12,0);groupBar->setSpacing(8);
        groupBar->addWidget(header,1);
        const bool history=m_library->groupHasHistory(m_subject.bangumiId,group.name);
        auto *bulk=new QPushButton(history?QStringLiteral("更新"):QStringLiteral("一键下载"),module);
        bulk->setObjectName("GroupDownload");
        bulk->setAccessibleName(group.name+QStringLiteral(" ")+bulk->text());
        QVector<ResourceResult> candidates;
        for(const auto &r:m_releases)
            if(releaseGroupKey(r.result.releaseGroup)==releaseGroupKey(group.name)
               && passes(r,filter))candidates.append(r.result);
        bulk->setEnabled(!m_searching&&!candidates.isEmpty()&&group.name!=kUnknownGroup);
        bulk->setToolTip(m_searching?QStringLiteral("正在检索最新话数…"):QStringLiteral("下载本字幕组缺少的正片，同集只选择一个版本"));
        connect(bulk,&QPushButton::clicked,this,[this,candidates]{emit groupDownloadRequested(m_subject,candidates);});
        groupBar->addWidget(bulk);
        moduleLayout->addLayout(groupBar);

        auto *body = new QWidget(module);
        auto *bodyLayout = new QVBoxLayout(body);
        bodyLayout->setContentsMargins(0, 0, 0, 0);
        bodyLayout->setSpacing(0);
        moduleLayout->addWidget(body);
        body->setVisible(expanded);

        connect(header, &QPushButton::toggled, this, [this, header, body, group, baseText](bool on) {
            m_groupExpanded.insert(group.name, on);
            body->setVisible(on);
            header->setText((on ? Icons::CHEVRON_DOWN : Icons::CHEVRON_RIGHT) + QStringLiteral("  ")
                            + group.name + QStringLiteral("   ·   ") + baseText);
        });

        int visibleCount = 0;
        QVector<QWidget *> rows;
        for (const auto &release : group.releases) {
            const auto &result = release.result;
            auto *row = new QFrame(body);
            row->setObjectName(QStringLiteral("ResourceRow"));
            auto *rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(14, 10, 12, 10);
            rowLayout->setSpacing(14);

            QString episodeText;
            if (release.isBatch)
                episodeText = release.episode.isEmpty() || release.episode == QStringLiteral("合集")
                    ? QStringLiteral("合集") : release.episode;
            else if (!release.episode.isEmpty())
                episodeText = QStringLiteral("第 %1 话").arg(release.episode);
            else
                episodeText = QStringLiteral("—");
            auto *episode = new QLabel(episodeText, row);
            episode->setFixedWidth(64);
            episode->setAlignment(Qt::AlignCenter);
            const QString bg = release.isBatch ? Theme::ACCENT_SOFT : Theme::SURFACE3;
            const QString fg = release.isBatch ? Theme::ACCENT_TEXT : Theme::TEXT;
            episode->setStyleSheet(QStringLiteral("background:%1;color:%2;border-radius:6px;"
                                                  "padding:5px 0;font-weight:600;font-size:9pt;")
                                       .arg(bg, fg));
            rowLayout->addWidget(episode, 0, Qt::AlignTop);

            auto *middle = new QVBoxLayout();
            middle->setSpacing(4);
            auto *title = new QLabel(result.title, row);
            title->setWordWrap(true);
            title->setTextInteractionFlags(Qt::TextSelectableByMouse);
            title->setStyleSheet(QStringLiteral("font-size:9.5pt;"));
            middle->addWidget(title);

            auto *chipsBox = new QWidget(row);
            auto *chipsLayout = new FlowLayout(nullptr, 4, 4);
            chipsBox->setLayout(chipsLayout);
            for (auto *chip : qualityChips(result.tags))
                chipsLayout->addWidget(chip);
            middle->addWidget(chipsBox);

            QStringList meta = {fmtSize(result.fileSize), fmtAge(result.publishTime)};
            const auto sites = result.seenOn.isEmpty() ? QStringList{result.siteName} : result.seenOn;
            meta << sites.join(QStringLiteral(" · "));
            auto *metaLabel = label(meta.join(QStringLiteral("   ")), QStringLiteral("Faint"), true, row);
            metaLabel->setObjectName(QStringLiteral("ResourceSources"));
            metaLabel->setStyleSheet(QStringLiteral("color:#718785;font-size:9pt;"));
            metaLabel->setToolTip(QStringLiteral("来源：") + sites.join(QStringLiteral("、")));
            middle->addWidget(metaLabel);
            rowLayout->addLayout(middle, 1);

            auto *right = new QVBoxLayout();
            right->setSpacing(6);
            right->setAlignment(Qt::AlignTop | Qt::AlignRight);
            auto *buttons = new QHBoxLayout();
            buttons->setSpacing(2);
            if (!result.sourcePage.isEmpty()) {
                auto *open = new QPushButton(row);
                open->setIcon(Theme::icon("external"));
                open->setAccessibleName(QStringLiteral("打开发布页"));
                open->setObjectName(QStringLiteral("IconButton"));
                open->setFixedSize(28, 28);
                open->setToolTip(QStringLiteral("打开发布页"));
                const QString page = result.sourcePage;
                connect(open, &QPushButton::clicked, this, [page] { QDesktopServices::openUrl(QUrl(page)); });
                buttons->addWidget(open);
            }
            const QString link = result.magnetUri.isEmpty() ? result.torrentUrl : result.magnetUri;
            auto *copy = new QPushButton(row);
            copy->setIcon(Theme::icon("copy"));
            copy->setAccessibleName(QStringLiteral("复制下载链接"));
            copy->setObjectName(QStringLiteral("IconButton"));
            copy->setFixedSize(28, 28);
            copy->setToolTip(result.magnetUri.isEmpty() ? QStringLiteral("复制种子链接") : QStringLiteral("复制磁力链接"));
            copy->setEnabled(!link.isEmpty());
            connect(copy, &QPushButton::clicked, this, [link] {
                QGuiApplication::clipboard()->setText(link);
            });
            copy->setVisible(result.localPath.isEmpty());
            buttons->addWidget(copy);

            const bool ready=m_library->downloaded(m_subject.bangumiId,result);
            const bool active=m_library->busy(m_subject.bangumiId,result);
            const bool external=m_library->external(m_subject.bangumiId,result);
            auto *download = new QPushButton(ready?QStringLiteral("播放"):(external?QStringLiteral("已发送"):(active?QStringLiteral("下载中"):QStringLiteral("下载"))),row);
            if(external)download->setToolTip(QStringLiteral("已交给外部下载器，请在该软件中查看任务。"));
            download->setObjectName(QStringLiteral("Primary"));
            download->setEnabled(ready||(!active&&!link.isEmpty()&&!m_localOnly));
            connect(download,&QPushButton::clicked,this,[this,result,ready]{
                if(ready)emit playRequested(m_subject,result);
                else emit downloadRequested(m_subject,result);
            });
            buttons->addWidget(download);
            right->addLayout(buttons);
            rowLayout->addLayout(right);

            bodyLayout->addWidget(row);
            rows.push_back(row);
            row->setVisible(visibleCount < 6 || showAll);
            ++visibleCount;
        }

        if (group.releases.size() > 6) {
            auto *more = new QPushButton(showAll ? QStringLiteral("收起")
                : QStringLiteral("显示其余 %1 个资源").arg(group.releases.size() - 6), body);
            more->setObjectName(QStringLiteral("Ghost"));
            bodyLayout->addWidget(more);
            connect(more, &QPushButton::clicked, this, [this, rows, more, key=group.name] {
                const bool anyHidden = !m_groupShowAll.value(key, false);
                m_groupShowAll.insert(key, anyHidden);
                for (int j = 6; j < rows.size(); ++j)
                    rows.at(j)->setVisible(anyHidden);
                more->setText(anyHidden ? QStringLiteral("收起") : QStringLiteral("显示其余 %1 个资源").arg(rows.size() - 6));
            });
        }
        m_groupsLayout->addWidget(module);
    }

    if (groups.size() > limit) {
        auto *moreGroups = new QPushButton(
            QStringLiteral("显示更多发布组（剩余 %1 个）").arg(groups.size() - limit), m_groupsBox);
        moreGroups->setObjectName(QStringLiteral("Ghost"));
        connect(moreGroups, &QPushButton::clicked, this, [this] {
            m_groupLimit = std::numeric_limits<int>::max();
            render();
        });
        m_groupsLayout->addWidget(moreGroups);
    }

    m_countLabel->setText(QStringLiteral("%1 个资源 · %2 个发布组").arg(shown.size()).arg(groups.size())
                          + (!m_localOnly && state=="all" && shown.size() != m_releases.size()
                                 ? QStringLiteral("（共 %1）").arg(m_releases.size())
                                 : QString()));
    if (!groups.isEmpty()) {
        m_empty->hide();
    } else if(m_localOnly) {
        m_empty->setText(QStringLiteral("还没有下载完成的视频"),QStringLiteral("下载完成的视频会按字幕组显示在这里。"));m_empty->show();
    } else if(m_searching&&m_releases.isEmpty()) {
        m_empty->setText(QStringLiteral("正在搜索资源…"),QStringLiteral("正在获取各字幕组已发布的话数。"));m_empty->show();
    } else if (m_releases.isEmpty()) {
        m_empty->setText(QStringLiteral("没有找到资源"),
                         QStringLiteral("各资源站均未返回与本作匹配的结果。可在「站点管理」检查站点状态。"));
        m_empty->show();
    } else {
        m_empty->setText(QStringLiteral("没有符合筛选条件的资源"),
                         QStringLiteral("试试放宽画质或字幕条件。"));
        m_empty->show();
    }
}

void AnimeDetailPage::clearGroups()
{
    while (auto *item = m_groupsLayout->takeAt(0)) {
        if (auto *widget = item->widget()) {
            widget->hide();widget->deleteLater();
        }
        delete item;
    }
}

void AnimeDetailPage::resetFilters()
{
    QSignalBlocker b1(m_season), b2(m_resolution), b3(m_subtitleFilter), b4(m_kind), b5(m_sort),
        b6(m_hdrOnly), b7(m_text), b8(m_downloadStatus);
    m_season->setCurrentIndex(0);
    m_resolution->setCurrentIndex(0);
    m_subtitleFilter->setCurrentIndex(0);
    m_kind->setCurrentIndex(0);
    m_sort->setCurrentIndex(0);
    m_downloadStatus->setCurrentIndex(m_localOnly?1:0);
    m_hdrOnly->setChecked(false);
    m_text->clear();
    render();
}

Filters AnimeDetailPage::filters() const
{
    Filters out;
    const int season = m_season->currentData().toInt();
    if (season > 0)
        out.season = season;
    out.resolution = m_resolution->currentData().toString();
    out.subtitle = m_subtitleFilter->currentData().toString();
    out.kind = m_kind->currentData().toString();
    out.hdrOnly = m_hdrOnly->isChecked();
    out.text = m_text->text().trimmed();
    return out;
}

QString AnimeDetailPage::sortKey() const
{
    return m_sort->currentData().toString();
}

void AnimeDetailPage::setSeasons(const QVector<int> &seasons)
{
    QSignalBlocker blocker(m_season);
    m_season->clear();
    m_season->addItem(QStringLiteral("全部"), -1);
    for (int season : seasons)
        m_season->addItem(QStringLiteral("第 %1 季").arg(season), season);
    const int preferred=m_season->findData(LocalLibrary::subjectSeason(m_subject));
    m_season->setCurrentIndex(preferred<0?0:preferred);
    m_season->setVisible(!seasons.isEmpty());
}

void AnimeDetailPage::cancel()
{
    ++m_searchGeneration;m_aggregator->cancel();m_searching=false;
    m_renderTimer->stop();
}

void AnimeDetailPage::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        emit backRequested();
    else
        QWidget::keyPressEvent(event);
}
