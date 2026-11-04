#include "AnimeDetailPage.h"

#include "core/Catalog.h"
#include "core/LocalLibrary.h"
#include "core/TitleParser.h"
#include "net/BangumiClient.h"
#include "net/SearchAggregator.h"
#include "ui/FlowLayout.h"
#include "ui/Controls.h"
#include "ui/ImageLoader.h"
#include "ui/Motion.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#include <QAction>
#include <QCheckBox>
#include <QDialog>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QScreen>
#include <QSpinBox>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSignalBlocker>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <limits>

namespace {
constexpr int kPosterWidth = 164;
constexpr int kPosterHeight = 232;
constexpr int kSummaryLines = 3;
constexpr int kRowsPerGroup = 6;
constexpr int kInitialGroups = 3;

// Height of the collapsed synopsis: whole lines only, never a clipped partial line.
int collapsedHeight(QLabel *summary)
{
    summary->ensurePolished();
    return summary->fontMetrics().lineSpacing() * kSummaryLines;
}

QString groupTitle(const QString &name, qsizetype count)
{
    if (name == kUnknownGroup)
        return QStringLiteral("未归档资源    %1 个").arg(count);
    return QStringLiteral("%1    %2 个资源").arg(name).arg(count);
}

// Options behind the ▾ half of 一键下载: subtitle, resolution and an episode
// window. Every change re-runs the real download planner, so the estimate shown
// is exactly what 开始下载 will queue.
struct GroupDownloadOptions {
    QString language;
    QString subtitleType;
    QString resolution;
    EpisodeRange range;
};

QVector<ResourceResult> applyOptions(const QVector<ResourceResult> &resources, const GroupDownloadOptions &options)
{
    QVector<ResourceResult> out;
    for (const auto &r : resources) {
        const auto langs = r.tags.subtitleLang.split(QLatin1Char(','), Qt::SkipEmptyParts);
        if (options.language == QLatin1String("bilingual")) {
            if (!langs.contains(QStringLiteral("zh-Hans")) || !langs.contains(QStringLiteral("zh-Hant"))) continue;
        } else if (!options.language.isEmpty() && !langs.contains(options.language)) {
            continue;
        }
        if (!options.subtitleType.isEmpty() && r.tags.subtitleType != options.subtitleType) continue;
        if (!options.resolution.isEmpty() && r.tags.resolution != options.resolution) continue;
        out.append(r);
    }
    return out;
}

class GroupDownloadPopup final : public QDialog {
public:
    using Preview = std::function<QString(const QVector<ResourceResult> &, const EpisodeRange &, bool *ready)>;

    GroupDownloadPopup(QWidget *anchor, const QString &group, const QVector<ResourceResult> &resources, Preview preview,
                       const GroupDownloadOptions &initial = {})
        : QDialog(anchor->window(), Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
        , m_anchor(anchor), m_resources(resources), m_preview(std::move(preview))
    {
        setObjectName("DownloaderPopup");
        setAttribute(Qt::WA_TranslucentBackground);
        auto *outer = new QVBoxLayout(this); outer->setContentsMargins(10, 8, 10, 14);
        auto *panel = new QFrame(this); panel->setObjectName("DownloaderPopupPanel"); outer->addWidget(panel);
        auto *shadow = new QGraphicsDropShadowEffect(panel);
        shadow->setBlurRadius(18); shadow->setOffset(0, 3); shadow->setColor(Theme::c().shadow);
        panel->setGraphicsEffect(shadow);
        auto *body = new QVBoxLayout(panel); body->setContentsMargins(16, 14, 16, 14); body->setSpacing(12);
        body->addWidget(label(QStringLiteral("下载选项"), "DownloaderPopupHeading", false, panel));
        auto *title = label(group, "SettingsCardTitle", false, panel);
        title->setTextFormat(Qt::PlainText);
        body->addWidget(title);

        QHash<QString, int> langs, types, resolutions;
        QSet<int> episodes;
        for (const auto &r : resources) {
            const auto list = r.tags.subtitleLang.split(QLatin1Char(','), Qt::SkipEmptyParts);
            for (const auto &lang : list) ++langs[lang];
            if (list.contains(QStringLiteral("zh-Hans")) && list.contains(QStringLiteral("zh-Hant"))) ++langs[QStringLiteral("bilingual")];
            if (!r.tags.subtitleType.isEmpty()) ++types[r.tags.subtitleType];
            if (!r.tags.resolution.isEmpty()) ++resolutions[r.tags.resolution];
        }
        for (const auto &release : annotate(resources))
            for (const auto &ep : episodeNumbers(release.episode)) episodes.insert(ep.toInt());

        auto fill = [](SelectBox *box, const QString &any, const QVector<QPair<QString, QString>> &known, const QHash<QString, int> &counts) {
            box->addItem(any, QString());
            for (const auto &[value, name] : known)
                if (counts.value(value) > 0) box->addItem(QStringLiteral("%1（%2）").arg(name).arg(counts.value(value)), value);
            box->setEnabled(box->count() > 1);
        };
        m_language = new SelectBox(panel); m_language->setObjectName("GroupDownloadLanguage");
        fill(m_language, QStringLiteral("不限语言"), {{"zh-Hans", QStringLiteral("简体中文")}, {"zh-Hant", QStringLiteral("繁体中文")},
             {"bilingual", QStringLiteral("简繁都有")}, {"zh", QStringLiteral("中文")}, {"ja", QStringLiteral("日文")}, {"en", QStringLiteral("英文")}}, langs);
        m_type = new SelectBox(panel); m_type->setObjectName("GroupDownloadSubtitleType");
        fill(m_type, QStringLiteral("不限形式"), {{"internal", QStringLiteral("内封字幕")}, {"hardcoded", QStringLiteral("内嵌字幕")},
             {"external", QStringLiteral("外挂字幕")}, {"ass", QStringLiteral("ASS")}, {"srt", QStringLiteral("SRT")}, {"pgs", QStringLiteral("PGS")}}, types);
        m_resolution = new SelectBox(panel); m_resolution->setObjectName("GroupDownloadResolution");
        fill(m_resolution, QStringLiteral("不限（自动选最高）"), {{"2160p", QStringLiteral("4K")}, {"1440p", QStringLiteral("1440p")},
             {"1080p", QStringLiteral("1080p")}, {"720p", QStringLiteral("720p")}, {"480p", QStringLiteral("480p")}}, resolutions);

        auto *form = new QGridLayout; form->setHorizontalSpacing(12); form->setVerticalSpacing(10);
        form->addWidget(label(QStringLiteral("字幕"), "SettingTitle", false, panel), 0, 0);
        auto *subtitleRow = new QHBoxLayout; subtitleRow->setSpacing(8);
        subtitleRow->addWidget(m_language, 1); subtitleRow->addWidget(m_type, 1);
        form->addLayout(subtitleRow, 0, 1);
        form->addWidget(label(QStringLiteral("清晰度"), "SettingTitle", false, panel), 1, 0);
        form->addWidget(m_resolution, 1, 1);

        form->addWidget(label(QStringLiteral("集数"), "SettingTitle", false, panel), 2, 0);
        auto *rangeRow = new QHBoxLayout; rangeRow->setSpacing(8);
        m_first = new QSpinBox(panel); m_first->setObjectName("GroupDownloadFirst");
        m_last = new QSpinBox(panel); m_last->setObjectName("GroupDownloadLast");
        const int low = episodes.isEmpty() ? 1 : *std::min_element(episodes.cbegin(), episodes.cend());
        const int high = episodes.isEmpty() ? 1 : *std::max_element(episodes.cbegin(), episodes.cend());
        for (auto *box : {m_first, m_last}) {
            box->setRange(1, 1899); box->setPrefix(QStringLiteral("第 ")); box->setSuffix(QStringLiteral(" 话"));
            box->setEnabled(!episodes.isEmpty());
        }
        m_first->setValue(low); m_last->setValue(high);
        rangeRow->addWidget(m_first, 1); rangeRow->addWidget(label(QStringLiteral("至"), "Faint", false, panel)); rangeRow->addWidget(m_last, 1);
        form->addLayout(rangeRow, 2, 1);
        auto *quick = new QHBoxLayout; quick->setSpacing(6);
        auto addQuick = [&](const QString &text, int from, int to) {
            auto *button = new QPushButton(text, panel); button->setObjectName("Ghost"); button->setCursor(Qt::PointingHandCursor);
            button->setEnabled(!episodes.isEmpty());
            connect(button, &QPushButton::clicked, this, [this, from, to] { m_first->setValue(from); m_last->setValue(to); });
            quick->addWidget(button);
        };
        addQuick(QStringLiteral("全部 %1–%2 话").arg(low).arg(high), low, high);
        if (high - low >= 12) addQuick(QStringLiteral("最新 12 话"), high - 11, high);
        if (high - low >= 50) addQuick(QStringLiteral("最新 50 话"), high - 49, high);
        quick->addStretch(1);
        form->addLayout(quick, 3, 1);
        form->setColumnStretch(1, 1);
        body->addLayout(form);
        if (episodes.isEmpty())
            body->addWidget(label(QStringLiteral("这些资源都是集数未知的合集，将按合集内容下载。"), "SettingHint", true, panel));

        m_summary = label(QString(), "DownloaderHint", true, panel);
        m_summary->setObjectName("GroupDownloadSummary");
        body->addWidget(m_summary);
        auto *actions = new QHBoxLayout; actions->addStretch(1);
        auto *cancel = new QPushButton(QStringLiteral("取消"), panel); cancel->setObjectName("Ghost");
        m_start = new QPushButton(QStringLiteral("开始下载"), panel); m_start->setObjectName("Primary");
        m_start->setIcon(Theme::icon("download", &Theme::Palette::accentInk)); m_start->setAutoDefault(true);
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        connect(m_start, &QPushButton::clicked, this, &QDialog::accept);
        actions->addWidget(cancel); actions->addWidget(m_start);
        body->addLayout(actions);

        for (auto *box : {m_language, m_type, m_resolution})
            connect(box, qOverload<int>(&QComboBox::currentIndexChanged), this, &GroupDownloadPopup::refresh);
        connect(m_first, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) { if (m_last->value() < v) m_last->setValue(v); refresh(); });
        connect(m_last, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) { if (m_first->value() > v) m_first->setValue(v); refresh(); });
        // Start from the last choice made for this group, where it still applies.
        for (const auto &[box, value] : {std::pair{m_language, initial.language}, {m_type, initial.subtitleType}, {m_resolution, initial.resolution}})
            if (const int index = box->findData(value); !value.isEmpty() && index >= 0) box->setCurrentIndex(index);
        setFixedWidth(460);
        refresh();
    }

    GroupDownloadOptions options() const
    {
        GroupDownloadOptions o;
        o.language = m_language->currentData().toString();
        o.subtitleType = m_type->currentData().toString();
        o.resolution = m_resolution->currentData().toString();
        if (m_first->isEnabled()) o.range = {m_first->value(), m_last->value()};
        return o;
    }

protected:
    void showEvent(QShowEvent *event) override
    {
        QDialog::showEvent(event);
        adjustSize();
        const QRect anchor(m_anchor->mapToGlobal(QPoint(0, 0)), m_anchor->size());
        const QRect available = m_anchor->screen()->availableGeometry().adjusted(4, 4, -4, -4);
        const int x = qBound(available.left(), anchor.right() - width() + 10, qMax(available.left(), available.right() - width() + 1));
        const int y = available.bottom() - anchor.bottom() >= height() ? anchor.bottom() + 2 : anchor.top() - height() - 2;
        move(x, qBound(available.top(), y, qMax(available.top(), available.bottom() - height() + 1)));
        m_start->setFocus(Qt::PopupFocusReason);
    }

private:
    void refresh()
    {
        const auto o = options();
        const auto filtered = applyOptions(m_resources, o);
        bool ready = false;
        const QString text = m_preview(filtered, o.range, &ready);
        m_summary->setText(QStringLiteral("符合条件 %1 个资源 · %2").arg(filtered.size()).arg(text));
        m_start->setEnabled(ready);
    }

    QWidget *m_anchor;
    QVector<ResourceResult> m_resources;
    Preview m_preview;
    SelectBox *m_language, *m_type, *m_resolution;
    QSpinBox *m_first, *m_last;
    QLabel *m_summary;
    QPushButton *m_start;
};

}

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
    // Keep the render timer as the first QTimer child; tests drive it directly.
    m_renderTimer = new QTimer(this);
    m_renderTimer->setSingleShot(true);
    m_renderTimer->setInterval(180);
    connect(m_renderTimer, &QTimer::timeout, this, &AnimeDetailPage::render);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *body = new QWidget();
    body->setObjectName(QStringLiteral("ScrollBody"));
    m_scroll = scrollArea(body, this);
    outer->addWidget(m_scroll);

    // Keep the reading column comfortable on maximised windows: cap and centre it.
    auto *centered = new QHBoxLayout(body);
    centered->setContentsMargins(0, 0, 0, 0);
    auto *column = new QWidget(body);
    column->setMaximumWidth(1280);
    centered->addStretch(1);
    centered->addWidget(column, 100);
    centered->addStretch(1);
    auto *layout = new QVBoxLayout(column);
    layout->setContentsMargins(28, 18, 28, 24);
    layout->setSpacing(16);

    auto *top = new QHBoxLayout();
    auto *back = new QPushButton(QStringLiteral("返回"), this);
    back->setObjectName(QStringLiteral("BackButton"));
    back->setIcon(Theme::icon("chevron-left"));
    back->setCursor(Qt::PointingHandCursor);
    back->setToolTip(QStringLiteral("返回 (Esc)"));
    connect(back, &QPushButton::clicked, this, &AnimeDetailPage::backRequested);
    top->addWidget(back);
    top->addStretch(1);
    m_resourceSearch = new QPushButton(QStringLiteral("重新搜索资源"), this);
    m_resourceSearch->setObjectName("ResourceSearchAction");
    m_resourceSearch->setIcon(Theme::icon("refresh"));
    connect(m_resourceSearch, &QPushButton::clicked, this, [this]{
        if(m_localOnly)emit resourceSearchRequested(m_subject);
        else {resetFilters();startSearch();}
    });
    top->addWidget(m_resourceSearch);
    layout->addLayout(top);

    buildHero(layout);
    buildSourceStrip(layout);

    auto *section = new QHBoxLayout();
    section->setSpacing(10);
    m_sectionTitle=label(QStringLiteral("可用资源"),QStringLiteral("H2"));
    section->addWidget(m_sectionTitle,0,Qt::AlignVCenter);
    m_countLabel = label(QString(), QStringLiteral("Faint"));
    section->addWidget(m_countLabel,0,Qt::AlignVCenter);
    section->addStretch(1);
    section->addWidget(label(QStringLiteral("排序"),"Faint"),0,Qt::AlignVCenter);
    m_sort = new SelectBox();
    for (const auto &[text, data] : QVector<QPair<QString, QString>>{
             {QStringLiteral("推荐"), QStringLiteral("popular")},
             {QStringLiteral("最近更新"), QStringLiteral("latest")}, {QStringLiteral("做种最多"), QStringLiteral("seeders")},
             {QStringLiteral("资源最多"), QStringLiteral("count")}, {QStringLiteral("名称"), QStringLiteral("name")}})
        m_sort->addItem(text, data);
    m_sort->setFixedWidth(132);
    section->addWidget(m_sort);
    layout->addLayout(section);

    buildFilters(layout);

    m_groupsBox = new QWidget();
    m_groupsLayout = new QVBoxLayout(m_groupsBox);
    m_groupsLayout->setContentsMargins(0, 0, 0, 0);
    m_groupsLayout->setSpacing(12);
    layout->addWidget(m_groupsBox);

    m_empty = new EmptyState("search", QStringLiteral("正在搜索资源…"),
                             QStringLiteral("从已启用的资源站并发检索，结果会逐步出现。"));
    m_empty->setMinimumHeight(180);
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
    row->setContentsMargins(20,20,24,20);
    row->setSpacing(24);

    m_poster = new QLabel(hero);
    m_poster->setObjectName("Poster");
    m_poster->setFixedSize(kPosterWidth,kPosterHeight);
    m_poster->setAlignment(Qt::AlignCenter);
    row->addWidget(m_poster, 0, Qt::AlignTop);

    auto *col = new QVBoxLayout();
    col->setSpacing(8);

    m_title = label(QString(), QStringLiteral("H1"), true);
    m_title->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_subtitle = label(QString(), QStringLiteral("Muted"), true);
    m_subtitle->setTextInteractionFlags(Qt::TextSelectableByMouse);
    col->addWidget(m_title);
    col->addWidget(m_subtitle);

    m_stats = new QWidget();
    auto *statsLayout = new FlowLayout(m_stats, 6, 6);
    Q_UNUSED(statsLayout);
    col->addWidget(m_stats);

    m_tagsBox = new QWidget();
    m_tagsLayout = new FlowLayout(nullptr, 6, 6);
    m_tagsBox->setLayout(m_tagsLayout);
    col->addWidget(m_tagsBox);

    m_summary = label(QString(), QStringLiteral("Muted"), true);
    m_summary->setMaximumHeight(collapsedHeight(m_summary));
    m_summary->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    col->addWidget(m_summary);

    auto *actions = new QHBoxLayout();
    actions->setSpacing(8);
    m_followButton = new QPushButton(QStringLiteral("追番"), this);
    m_followButton->setObjectName(QStringLiteral("Primary"));
    m_followButton->setCheckable(true);
    m_followButton->setCursor(Qt::PointingHandCursor);
    connect(m_followButton, &QPushButton::toggled, this, [this](bool checked) {
        updateFollowButton(checked);
        emit followChanged(m_subject, checked);
    });
    actions->addWidget(m_followButton);

    auto *bangumiButton = new QPushButton(QStringLiteral("Bangumi 页面"), this);
    bangumiButton->setIcon(Theme::icon("external"));
    connect(bangumiButton, &QPushButton::clicked, this, [this] {
        if (!m_subject.bangumiUrl.isEmpty())
            QDesktopServices::openUrl(QUrl(m_subject.bangumiUrl));
    });
    actions->addWidget(bangumiButton);

    m_moreButton = new QPushButton(QStringLiteral("展开简介"), this);
    m_moreButton->setObjectName(QStringLiteral("Ghost"));
    connect(m_moreButton, &QPushButton::clicked, this, [this] {
        const int collapsed = collapsedHeight(m_summary);
        const bool expanded = m_summary->maximumHeight() > collapsed;
        const int full = qMax(collapsed, m_summary->heightForWidth(m_summary->width()));
        m_moreButton->setText(expanded ? QStringLiteral("展开简介") : QStringLiteral("收起简介"));
        auto *animation = new QPropertyAnimation(m_summary, "maximumHeight", m_summary);
        animation->setDuration(Motion::duration(Motion::Normal));
        animation->setStartValue(expanded ? m_summary->height() : collapsed);
        animation->setEndValue(expanded ? collapsed : full);
        animation->setEasingCurve(QEasingCurve::OutCubic);
        connect(animation, &QPropertyAnimation::finished, m_summary, [this, expanded] {
            if (!expanded) m_summary->setMaximumHeight(QWIDGETSIZE_MAX);
        });
        animation->start(QAbstractAnimation::DeleteWhenStopped);
    });
    actions->addWidget(m_moreButton);
    actions->addStretch(1);
    col->addSpacing(4);
    col->addLayout(actions);
    col->addStretch(1);

    row->addLayout(col, 1);
    layout->addWidget(hero);
}

void AnimeDetailPage::updateFollowButton(bool following)
{
    m_followButton->setText(following ? QStringLiteral("已追番") : QStringLiteral("追番"));
    m_followButton->setIcon(following ? Theme::icon("check", &Theme::Palette::accentText)
                                      : Theme::icon("plus", &Theme::Palette::accentInk));
}

void AnimeDetailPage::buildSourceStrip(QVBoxLayout *layout)
{
    auto *panel = new QFrame();
    m_sourcePanel=panel;
    panel->setObjectName(QStringLiteral("Panel"));
    auto *row = new QHBoxLayout(panel);
    row->setContentsMargins(16, 10, 16, 10);
    row->setSpacing(14);

    m_progress = label(QStringLiteral("待检索"), QStringLiteral("StatValue"));
    m_progress->setMinimumWidth(96);
    row->addWidget(m_progress, 0, Qt::AlignTop);

    m_dotsBox = new QWidget(panel);
    m_dotsLayout = new FlowLayout(m_dotsBox,14,8);
    m_dotsLayout->setContentsMargins(0, 2, 0, 2);

    refreshSources();
    row->addWidget(m_dotsBox,1);
    layout->addWidget(panel);
}

void AnimeDetailPage::refreshSources()
{
    m_dotsLayout->clear();
    m_dots.clear();
    for (const auto &site : m_catalog->sites(true)) {
        auto *widget = new QWidget(m_dotsBox);
        auto *dotLayout = new QHBoxLayout(widget);
        dotLayout->setContentsMargins(0, 0, 0, 0);
        dotLayout->setSpacing(6);
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
    m_filterBar->setObjectName(QStringLiteral("FilterBar"));
    auto *v = new QVBoxLayout(m_filterBar);
    v->setContentsMargins(14, 12, 14, 12);
    v->setSpacing(10);

    auto makeCombo = [](const QVector<QPair<QString, QVariant>> &items, const QString &tip) {
        auto *combo = new SelectBox();
        for (const auto &[text, data] : items)
            combo->addItem(text, data);
        combo->setToolTip(tip);
        combo->setAccessibleName(tip);
        return combo;
    };

    // "全部…" entries name their own dimension, so the row needs no separate labels.
    m_season = makeCombo({{QStringLiteral("全部季度"), -1}}, QStringLiteral("放送季度"));
    m_kind = makeCombo({{QStringLiteral("全部类型"), QStringLiteral("all")},
                        {QStringLiteral("合集"), QStringLiteral("batch")},
                        {QStringLiteral("单集"), QStringLiteral("single")}}, QStringLiteral("资源类型"));
    m_resolution = makeCombo({{QStringLiteral("全部清晰度"), QString()},
                              {QStringLiteral("4K"), QStringLiteral("2160p")},
                              {QStringLiteral("1080p"), QStringLiteral("1080p")},
                              {QStringLiteral("720p"), QStringLiteral("720p")}}, QStringLiteral("清晰度"));
    m_subtitleFilter = makeCombo({{QStringLiteral("全部字幕"), QString()},
                                  {QStringLiteral("中文"), QStringLiteral("zh")},
                                  {QStringLiteral("简体"), QStringLiteral("zh-Hans")},
                                  {QStringLiteral("繁体"), QStringLiteral("zh-Hant")},
                                  {QStringLiteral("英文"), QStringLiteral("en")}}, QStringLiteral("字幕语言"));
    m_downloadStatus = makeCombo({{QStringLiteral("全部状态"),"all"},{QStringLiteral("已下载"),"downloaded"},{QStringLiteral("未下载"),"missing"}},
                                 QStringLiteral("下载状态"));
    m_downloadStatus->setObjectName("DownloadStatusFilter");

    auto *choices=new QHBoxLayout;
    choices->setSpacing(10);
    for (auto *combo : {m_season, m_kind, m_resolution, m_subtitleFilter, m_downloadStatus})
        choices->addWidget(combo, 1);
    v->addLayout(choices);

    auto *tools=new QHBoxLayout; tools->setSpacing(12);
    m_text=new QLineEdit; m_text->setObjectName("InlineFilter");
    m_text->setPlaceholderText(QStringLiteral("在结果中筛选标题关键词…"));
    m_text->addAction(Theme::icon("filter",&Theme::Palette::text3),QLineEdit::LeadingPosition);
    m_text->setClearButtonEnabled(true); tools->addWidget(m_text,1);
    m_hdrOnly=new ToggleSwitch(QStringLiteral("仅 HDR / 杜比视界")); tools->addWidget(m_hdrOnly);
    auto *reset=new QPushButton(QStringLiteral("重置筛选")); reset->setObjectName("Ghost"); tools->addWidget(reset);
    v->addLayout(tools);

    // Changes the user makes dissolve into the new list; background batches do not.
    auto userRender = [this] { m_fadeNextRender = true; scheduleRender(); };
    for (auto *combo : {m_season, m_kind, m_resolution, m_subtitleFilter, m_sort, m_downloadStatus})
        connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this, userRender);
    connect(m_hdrOnly, &QCheckBox::toggled, this, userRender);
    connect(m_text, &QLineEdit::textChanged, this, userRender);
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
    m_resourceSearch->setIcon(Theme::icon(localOnly?"search":"refresh"));
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

    auto *statsLayout = static_cast<FlowLayout *>(m_stats->layout());
    statsLayout->clear();
    if (m_subject.rating > 0)
        statsLayout->addWidget(new Chip(QStringLiteral("★ %1").arg(m_subject.rating, 0, 'f', 1)
                                            + (m_subject.rank > 0 ? QStringLiteral("  #%1").arg(m_subject.rank) : QString()),
                                        &Theme::Palette::guessedBg, &Theme::Palette::guessedFg, QStringLiteral("Bangumi 评分与排名")));
    QStringList facts;
    if (m_subject.episodeCount > 0) facts << QStringLiteral("%1 话").arg(m_subject.episodeCount);
    if (!m_subject.airDate.isEmpty()) facts << m_subject.airDate;
    if (!m_subject.platform.isEmpty()) facts << m_subject.platform;
    if (!m_subject.studio.isEmpty()) facts << m_subject.studio;
    for (const auto &fact : facts)
        statsLayout->addWidget(new Chip(fact, &Theme::Palette::surface2, &Theme::Palette::text2));
    m_stats->setVisible(statsLayout->count() > 0);

    m_tagsLayout->clear();
    for (int i = 0; i < m_subject.tags.size() && i < 10; ++i)
        m_tagsLayout->addWidget(new Chip(m_subject.tags.at(i), &Theme::Palette::accentSoft, &Theme::Palette::accentText));
    m_tagsBox->setVisible(!m_subject.tags.isEmpty());

    m_summary->setText(m_subject.summary.isEmpty() ? QStringLiteral("暂无简介。") : m_subject.summary);
    m_summary->setMaximumHeight(collapsedHeight(m_summary));
    m_moreButton->setText(QStringLiteral("展开简介"));
    m_moreButton->setVisible(m_subject.summary.size() > 120);

    const bool following = m_isFollowing(m_subject.bangumiId);
    {
        QSignalBlocker blocker(m_followButton);
        m_followButton->setChecked(following);
    }
    updateFollowButton(following);

    m_poster->setPixmap(QPixmap());
    m_poster->setText(QStringLiteral("NEXUS"));
    const QString cover = BangumiClient::coverUrl(m_subject.coverUrl, 400);
    if (cover.isEmpty()) {
        m_poster->setText(QStringLiteral("无封面"));
        return;
    }
    const int id = m_subject.bangumiId;
    QPointer<AnimeDetailPage> self(this);
    m_images->request(cover, [self, id](QImage image) {
        if (!self || self->m_subject.bangumiId != id || image.isNull())
            return;
        self->m_poster->setPixmap(coverPixmap(image, kPosterWidth, kPosterHeight, 10));
        self->m_poster->setText(QString());
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
    m_groupLimit = kInitialGroups;
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
        QPointer<AnimeDetailPage> self(this);
        m_bangumi->getSubject(id, [self, id,generation](AnimeSubject full, QString error) {
            if (!self || self->m_subject.bangumiId != id||generation!=self->m_searchGeneration)
                return;
            if (!error.isEmpty() || full.bangumiId != id) {self->searchWith(self->m_subject);return;}
            full.airWeekday = full.airWeekday > 0 ? full.airWeekday : self->m_subject.airWeekday;
            self->m_subject = full;
            self->updateHero();
            self->searchWith(self->m_subject);
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
    QHash<QString,qsizetype> index;
    for(qsizetype i=0;i<m_releases.size();++i)index.insert(m_releases.at(i).result.rowKey(),i);
    for(const auto &release:annotate(relevant)) {
        const QString key=release.result.rowKey();
        const auto found=index.constFind(key);
        if(found==index.cend()){index.insert(key,m_releases.size());m_releases.append(release);}
        else mergeResource(m_releases[found.value()].result,release.result);
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

QWidget *AnimeDetailPage::buildResourceRow(const Release &release, const RowState &state, QWidget *parent)
{
    const auto &result = release.result;
    auto *row = new QFrame(parent);
    row->setObjectName(QStringLiteral("ResourceRow"));
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(16, 12, 14, 12);
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
    episode->setObjectName("EpisodeBadge");
    episode->setProperty("batch", release.isBatch);
    episode->setFixedWidth(68);
    episode->setAlignment(Qt::AlignCenter);
    rowLayout->addWidget(episode, 0, Qt::AlignTop);

    auto *middle = new QVBoxLayout();
    middle->setSpacing(6);
    auto *title = new QLabel(result.title, row);
    title->setObjectName("ResourceTitle");
    title->setWordWrap(true);
    title->setTextInteractionFlags(Qt::TextSelectableByMouse);
    middle->addWidget(title);

    const auto chips = qualityChips(result.tags);
    if (!chips.isEmpty()) {
        auto *chipsBox = new QWidget(row);
        auto *chipsLayout = new FlowLayout(chipsBox, 4, 4);
        for (auto *chip : chips)
            chipsLayout->addWidget(chip);
        middle->addWidget(chipsBox);
    }

    QStringList meta;
    if (result.fileSize > 0) meta << fmtSize(result.fileSize);
    if (const QString age = fmtAge(result.publishTime); !age.isEmpty()) meta << age;
    QStringList sites = result.seenOn.isEmpty() ? QStringList{result.siteName} : result.seenOn;
    sites.removeAll(QString());
    if (!sites.isEmpty()) meta << sites.join(QStringLiteral(" · "));
    auto *metaLabel = label(meta.join(QStringLiteral("  ·  ")), QStringLiteral("ResourceSources"), true, row);
    if (!sites.isEmpty()) metaLabel->setToolTip(QStringLiteral("来源：") + sites.join(QStringLiteral("、")));
    metaLabel->setVisible(!meta.isEmpty());
    middle->addWidget(metaLabel);
    rowLayout->addLayout(middle, 1);

    auto *buttons = new QHBoxLayout();
    buttons->setSpacing(4);
    if (!result.sourcePage.isEmpty()) {
        auto *open = iconButton("external", QStringLiteral("打开发布页"), row);
        const QString page = result.sourcePage;
        connect(open, &QPushButton::clicked, this, [page] { QDesktopServices::openUrl(QUrl(page)); });
        buttons->addWidget(open);
    }
    const QString link = result.magnetUri.isEmpty() ? result.torrentUrl : result.magnetUri;
    if (result.localPath.isEmpty()) {
        auto *copy = iconButton("copy", result.magnetUri.isEmpty() ? QStringLiteral("复制种子链接") : QStringLiteral("复制磁力链接"), row);
        copy->setEnabled(!link.isEmpty());
        connect(copy, &QPushButton::clicked, this, [link] { QGuiApplication::clipboard()->setText(link); });
        buttons->addWidget(copy);
    }
    buttons->addSpacing(4);

    QString text = QStringLiteral("下载");
    if (state.ready) text = QStringLiteral("播放");
    else if (state.external) text = QStringLiteral("已发送");
    else if (state.active) text = QStringLiteral("下载中");
    auto *action = new QPushButton(text, row);
    action->setObjectName(state.ready ? QStringLiteral("Primary") : QStringLiteral("Soft"));
    action->setIcon(state.ready ? Theme::icon("play",&Theme::Palette::accentInk)
                                : Theme::icon(state.external ? "external" : "download",&Theme::Palette::accentText));
    action->setMinimumWidth(84);
    action->setCursor(Qt::PointingHandCursor);
    if (state.external) action->setToolTip(QStringLiteral("已交给外部下载器，请在该软件中查看任务。"));
    action->setEnabled(state.ready || (!state.active && !link.isEmpty() && !m_localOnly));
    const bool ready = state.ready;
    connect(action, &QPushButton::clicked, this, [this, result, ready] {
        if (ready) { emit playRequested(m_subject, result); return; }
        rememberChoice(result.releaseGroup, std::array{result.tags.subtitleLang.section(QLatin1Char(','), 0, 0),
                                                       result.tags.subtitleType, result.tags.resolution});
        emit downloadRequested(m_subject, result);
    });
    buttons->addWidget(action);
    rowLayout->addLayout(buttons);
    rowLayout->setAlignment(buttons, Qt::AlignTop);
    return row;
}

void AnimeDetailPage::render()
{
    if (m_subject.bangumiId == 0)
        return;

    const Filters filter = filters();
    const QString state=m_downloadStatus->currentData().toString();
    const int id=m_subject.bangumiId;

    // LocalLibrary lookups hash keys and stat files; resolve each row once per render.
    QHash<QString,RowState> states;
    auto stateOf=[&](const ResourceResult &result) {
        const QString key=result.rowKey()+QLatin1Char('\n')+result.localPath;
        auto found=states.constFind(key);
        if(found!=states.cend())return found.value();
        RowState s;
        s.ready=m_library->downloaded(id,result);
        s.external=!s.ready&&m_library->external(id,result);
        s.active=!s.ready&&m_library->busy(id,result);
        states.insert(key,s);
        return s;
    };

    const auto local=m_library->localReleases(id);
    QVector<Release> pool=(m_localOnly||state=="downloaded")?local:m_releases;
    if(!m_localOnly&&state=="all") {
        QSet<QString> representedFiles;
        for(const auto &online:m_releases)
            if(stateOf(online.result).ready)
                for(const auto &path:m_library->playableFiles(id,online.result))representedFiles.insert(path);
        for(const auto &r:local)
            if(!representedFiles.contains(r.result.localPath))pool.append(r);
    }
    QVector<Release> shown;
    for(const auto &release:pool) {
        if(state=="missing"&&stateOf(release.result).ready)continue;
        if(passes(release,filter))shown.append(release);
    }

    QHash<QString,QVector<ResourceResult>> candidates;
    for(const auto &r:m_releases)
        if(passes(r,filter))candidates[releaseGroupKey(r.result.releaseGroup)].append(r.result);

    const auto groups = groupReleases(shown, sortKey(), subjectKeys(m_subject));
    const int scroll=m_scroll?m_scroll->verticalScrollBar()->value():0;
    if(std::exchange(m_fadeNextRender,false)) Motion::crossfade(m_groupsBox, Motion::Fast);
    m_groupsBox->setUpdatesEnabled(false);
    clearGroups();
    const int limit = groups.size() < m_groupLimit ? static_cast<int>(groups.size()) : m_groupLimit;
    for (int i = 0; i < limit; ++i) {
        const auto &group = groups.at(i);
        auto *module = new QFrame(m_groupsBox);
        module->setObjectName(QStringLiteral("GroupModule"));
        auto *moduleLayout = new QVBoxLayout(module);
        moduleLayout->setContentsMargins(0, 0, 0, 0);
        moduleLayout->setSpacing(0);

        const bool unarchived = group.name == kUnknownGroup;
        // The merged bucket of unattributed releases stays folded unless opened.
        const bool expanded = m_groupExpanded.value(group.name, i < kInitialGroups && !unarchived);
        const bool showAll = m_groupShowAll.value(group.name, false);
        auto *header = new QPushButton(groupTitle(group.name, group.releases.size()), module);
        header->setObjectName(QStringLiteral("GroupHeader"));
        header->setCheckable(true);
        header->setChecked(expanded);
        header->setCursor(Qt::PointingHandCursor);
        header->setIcon(Theme::icon(expanded ? "chevron-down" : "chevron-right"));
        header->setIconSize(QSize(18,18));
        if (unarchived)
            header->setToolTip(QStringLiteral("未能识别字幕组的资源合并显示在这里，可单独下载，不支持一键下载。"));
        auto *groupBar=new QHBoxLayout;
        groupBar->setContentsMargins(0,0,14,0);groupBar->setSpacing(8);
        groupBar->addWidget(header,1);
        const bool history=m_library->groupHasHistory(id,group.name);
        auto *bulk=new QPushButton(history?QStringLiteral("更新"):QStringLiteral("一键下载"),module);
        bulk->setObjectName("GroupDownload");
        bulk->setProperty("split",true);
        bulk->setVisible(!unarchived);
        bulk->setIcon(Theme::icon(history?"refresh":"download",&Theme::Palette::accentText));
        bulk->setAccessibleName(group.name+QStringLiteral(" ")+bulk->text());
        const auto groupCandidates=candidates.value(releaseGroupKey(group.name));
        bulk->setEnabled(!m_searching&&!groupCandidates.isEmpty()&&group.name!=kUnknownGroup);
        bulk->setToolTip(m_searching?QStringLiteral("正在检索最新话数…"):QStringLiteral("下载本字幕组缺少的正片，同集只选择一个版本"));
        connect(bulk,&QPushButton::clicked,this,[this,groupCandidates,name=group.name]{
            rememberChoice(name,std::nullopt);
            emit groupDownloadRequested(m_subject,groupCandidates,{});
        });
        // ▾: choose subtitle, resolution and episode window before downloading.
        auto *more=new QPushButton(module);
        more->setObjectName("GroupDownloadOptions");
        more->setIcon(Theme::icon("chevron-down",&Theme::Palette::accentText));
        more->setToolTip(QStringLiteral("下载选项：字幕、清晰度、集数范围"));
        more->setAccessibleName(group.name+QStringLiteral(" 下载选项"));
        more->setCursor(Qt::PointingHandCursor);
        more->setEnabled(bulk->isEnabled());
        more->setVisible(!unarchived);
        connect(more,&QPushButton::clicked,this,[this,more,groupCandidates,name=group.name]{showGroupOptions(more,name,groupCandidates);});
        auto *split=new QHBoxLayout; split->setSpacing(0);
        split->addWidget(bulk); split->addWidget(more);
        groupBar->addLayout(split);
        moduleLayout->addLayout(groupBar);

        auto *body = new QWidget(module);
        auto *bodyLayout = new QVBoxLayout(body);
        bodyLayout->setContentsMargins(0, 0, 0, 0);
        bodyLayout->setSpacing(0);
        moduleLayout->addWidget(body);
        body->setVisible(expanded);

        connect(header, &QPushButton::toggled, this, [this, header, body, name=group.name](bool on) {
            m_groupExpanded.insert(name, on);
            Motion::setExpanded(body, on);
            header->setIcon(Theme::icon(on ? "chevron-down" : "chevron-right"));
        });

        // Rows beyond the first few are only built once the user asks for them.
        const int rowCount = showAll ? int(group.releases.size()) : qMin(int(group.releases.size()), kRowsPerGroup);
        for (int r = 0; r < rowCount; ++r)
            bodyLayout->addWidget(buildResourceRow(group.releases.at(r), stateOf(group.releases.at(r).result), body));

        if (group.releases.size() > kRowsPerGroup) {
            const int hidden = int(group.releases.size()) - kRowsPerGroup;
            auto *more = new QPushButton(showAll ? QStringLiteral("收起")
                : QStringLiteral("显示其余 %1 个资源").arg(hidden), body);
            more->setObjectName(QStringLiteral("Ghost"));
            more->setIcon(Theme::icon(showAll ? "chevron-up" : "chevron-down"));
            more->ensurePolished(); more->setFixedHeight(40);
            bodyLayout->addWidget(more);
            connect(more, &QPushButton::clicked, this, [this, key=group.name] {
                m_groupShowAll.insert(key, !m_groupShowAll.value(key, false));
                m_fadeNextRender = true;
                render();
            });
        }
        m_groupsLayout->addWidget(module);
    }

    if (groups.size() > limit) {
        auto *moreGroups = new QPushButton(
            QStringLiteral("显示更多发布组（剩余 %1 个）").arg(groups.size() - limit), m_groupsBox);
        moreGroups->setObjectName(QStringLiteral("Soft"));
        moreGroups->setIcon(Theme::icon("chevron-down",&Theme::Palette::accentText));
        moreGroups->ensurePolished(); moreGroups->setFixedHeight(42);
        connect(moreGroups, &QPushButton::clicked, this, [this] {
            m_groupLimit = std::numeric_limits<int>::max();
            m_fadeNextRender = true;
            render();
        });
        m_groupsLayout->addWidget(moreGroups);
    }
    m_groupsBox->setUpdatesEnabled(true);
    if(m_scroll)m_scroll->verticalScrollBar()->setValue(scroll);

    m_countLabel->setText(QStringLiteral("%1 个资源 · %2 个发布组").arg(shown.size()).arg(groups.size())
                          + (!m_localOnly && state=="all" && shown.size() != m_releases.size()
                                 ? QStringLiteral("（共 %1）").arg(m_releases.size())
                                 : QString()));
    if (!groups.isEmpty()) {
        m_empty->hide();
    } else if(m_localOnly) {
        m_empty->setText(QStringLiteral("还没有下载完成的视频"),QStringLiteral("下载完成的视频会按字幕组显示在这里。点击右上角「查找可下载资源」开始下载。"));m_empty->show();
    } else if(m_searching&&m_releases.isEmpty()) {
        m_empty->setText(QStringLiteral("正在搜索资源…"),QStringLiteral("正在获取各字幕组已发布的话数。"));m_empty->show();
    } else if (m_releases.isEmpty()) {
        m_empty->setText(QStringLiteral("没有找到资源"),
                         QStringLiteral("各资源站均未返回与本作匹配的结果。可在「资源站点」检查站点状态。"));
        m_empty->show();
    } else {
        m_empty->setText(QStringLiteral("没有符合筛选条件的资源"),
                         QStringLiteral("试试放宽画质或字幕条件，或点击「重置筛选」。"));
        m_empty->show();
    }
}

void AnimeDetailPage::showGroupOptions(QWidget *anchor, const QString &group, const QVector<ResourceResult> &candidates)
{
    const AnimeSubject subject = m_subject;
    const auto saved = m_library->preference(subject.bangumiId);
    GroupDownloadOptions initial;
    if (releaseGroupKey(saved.group) == releaseGroupKey(group))
        initial = {saved.language, saved.subtitleType, saved.resolution, {}};
    GroupDownloadPopup popup(anchor, group, candidates,
        [this, subject](const QVector<ResourceResult> &resources, const EpisodeRange &range, bool *ready) {
            QString reason;
            const auto plans = m_library->plan(subject, resources, &reason, false, range);
            *ready = !plans.isEmpty();
            if (plans.isEmpty())
                return reason.isEmpty() ? QStringLiteral("没有需要下载的集数（已下载、下载中或无对应资源）") : reason;
            int episodes = 0;
            for (const auto &plan : plans)
                episodes += plan.episodes.isEmpty() ? plan.onlyEpisodes.size() : plan.episodes.size();
            return episodes > 0 ? QStringLiteral("预计新建 %1 个任务，共 %2 集").arg(plans.size()).arg(episodes)
                                : QStringLiteral("预计新建 %1 个任务").arg(plans.size());
        });
    if (popup.exec() != QDialog::Accepted || m_subject.bangumiId != subject.bangumiId)
        return;
    const auto options = popup.options();
    rememberChoice(group, std::array{options.language, options.subtitleType, options.resolution});
    emit groupDownloadRequested(m_subject, applyOptions(candidates, options), options.range);
}

void AnimeDetailPage::rememberChoice(const QString &group, std::optional<std::array<QString, 3>> kind)
{
    if (group.isEmpty() || group == kUnknownGroup || m_subject.bangumiId == 0)
        return;
    auto preference = m_library->preference(m_subject.bangumiId);
    const bool sameGroup = releaseGroupKey(preference.group) == releaseGroupKey(group);
    if (!sameGroup) preference = {};          // a new group starts with no filters or dismissals
    preference.group = group;
    if (kind) {
        preference.language = (*kind)[0];
        preference.subtitleType = (*kind)[1];
        preference.resolution = (*kind)[2];
    }
    m_library->setPreference(m_subject.bangumiId, preference);
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
    m_season->addItem(QStringLiteral("全部季度"), -1);
    for (int season : seasons)
        m_season->addItem(QStringLiteral("第 %1 季").arg(season), season);
    const int preferred=m_season->findData(LocalLibrary::subjectSeason(m_subject));
    m_season->setCurrentIndex(preferred<0?0:preferred);
    m_season->setEnabled(!seasons.isEmpty());
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
