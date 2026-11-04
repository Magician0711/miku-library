#include "Widgets.h"

#include "FlowLayout.h"
#include "ImageLoader.h"
#include "Theme.h"
#include "Controls.h"
#include "Motion.h"
#include "net/BangumiClient.h"

#include <QDateTime>
#include <QEnterEvent>
#include <QFocusEvent>
#include <QGridLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QVariantAnimation>

namespace {

constexpr int kCardGap = 16;
constexpr int kCardMin = 156;
constexpr int kCardMax = 216;
constexpr int kCardPadding = 4;

QPair<Theme::Role, Theme::Role> confidenceColors(Confidence confidence)
{
    using P = Theme::Palette;
    switch (confidence) {
    case Confidence::Probed: return {&P::probedBg, &P::probedFg};
    case Confidence::Metadata: return {&P::subBg, &P::subFg};
    case Confidence::Guessed: return {&P::guessedBg, &P::guessedFg};
    case Confidence::Unknown: break;
    }
    return {&P::unknownBg, &P::unknownFg};
}

QString confidenceTip(Confidence confidence)
{
    switch (confidence) {
    case Confidence::Probed: return QStringLiteral("已实测（MediaInfo/FFprobe）");
    case Confidence::Metadata: return QStringLiteral("种子元数据确认");
    case Confidence::Guessed: return QStringLiteral("文件名推测，未经实测");
    case Confidence::Unknown: break;
    }
    return QStringLiteral("未知");
}

QString subtitleText(const ResourceTags &tags)
{
    if (tags.subtitleLang.isEmpty() && tags.subtitleType.isEmpty())
        return {};
    static const QHash<QString, QString> labels = {
        {QStringLiteral("zh-Hans"), QStringLiteral("简")},
        {QStringLiteral("zh-Hant"), QStringLiteral("繁")},
        {QStringLiteral("zh"), QStringLiteral("中")},
        {QStringLiteral("ja"), QStringLiteral("日")},
        {QStringLiteral("en"), QStringLiteral("英")},
        {QStringLiteral("multi"), QStringLiteral("多语")},
    };
    QStringList langs;
    for (const auto &lang : tags.subtitleLang.split(QLatin1Char(','), Qt::SkipEmptyParts))
        langs << labels.value(lang, lang);
    QString text = langs.join(QStringLiteral("/"));
    if (!tags.subtitleType.isEmpty()) {
        static const QHash<QString, QString> types = {
            {QStringLiteral("internal"), QStringLiteral("内封")},
            {QStringLiteral("hardcoded"), QStringLiteral("内嵌")},
            {QStringLiteral("external"), QStringLiteral("外挂")},
            {QStringLiteral("ass"), QStringLiteral("ASS")},
            {QStringLiteral("srt"), QStringLiteral("SRT")},
            {QStringLiteral("pgs"), QStringLiteral("PGS")},
        };
        text += types.value(tags.subtitleType, tags.subtitleType);
    }
    return text;
}

} // namespace

Chip::Chip(const QString &text, Theme::Role background, Theme::Role foreground,
           const QString &tooltip, QWidget *parent)
    : QLabel(text, parent), m_background(background), m_foreground(foreground)
{
    setObjectName(QStringLiteral("Chip"));
    QFont chipFont = font();
    chipFont.setPointSizeF(8.5);
    chipFont.setWeight(QFont::DemiBold);
    setFont(chipFont);
    setContentsMargins(7, 2, 7, 2);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    if (!tooltip.isEmpty())
        setToolTip(tooltip);
}

void Chip::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Theme::color(m_background));
    painter.drawRoundedRect(QRectF(rect()), 5, 5);
    painter.setPen(Theme::color(m_foreground));
    painter.drawText(contentsRect(), Qt::AlignCenter, text());
}

IconLabel::IconLabel(const QString &icon, Theme::Role role, int iconSize, QWidget *parent)
    : QLabel(parent), m_icon(Theme::icon(icon, role)), m_iconSize(iconSize)
{
}

void IconLabel::paintEvent(QPaintEvent *event)
{
    QLabel::paintEvent(event);
    QPainter painter(this);
    const QRect target(QPoint(), QSize(m_iconSize, m_iconSize));
    m_icon.paint(&painter, QRect(rect().center() - target.center(), target.size()));
}

QVector<Chip *> qualityChips(const ResourceTags &tags)
{
    QVector<Chip *> chips;
    auto add = [&](const QString &text, Confidence confidence, const QString &kind) {
        if (text.isEmpty())
            return;
        const auto colors = confidenceColors(confidence);
        chips << new Chip(text, colors.first, colors.second,
                          QStringLiteral("%1：%2").arg(kind, confidenceTip(confidence)));
    };

    add(tags.resolution, tags.resolutionConf, QStringLiteral("分辨率"));
    QString video = tags.videoCodec;
    if (tags.bitDepth > 0)
        video += QStringLiteral(" %1bit").arg(tags.bitDepth);
    add(video, tags.videoConf, QStringLiteral("视频"));
    if (!tags.hdrType.isEmpty())
        chips << new Chip(tags.hdrType, &Theme::Palette::hdrBg, &Theme::Palette::hdrFg,
                          QStringLiteral("HDR：%1").arg(confidenceTip(tags.hdrConf)));
    QString audio = tags.audioCodec;
    if (!tags.audioChannels.isEmpty())
        audio += QStringLiteral(" %1").arg(tags.audioChannels);
    add(audio, tags.audioConf, QStringLiteral("音频"));
    add(subtitleText(tags), tags.subtitleConf, QStringLiteral("字幕"));
    add(tags.sourceType, Confidence::Guessed, QStringLiteral("来源"));
    return chips;
}

StatusDot::StatusDot(const QString &state, QWidget *parent)
    : QWidget(parent)
    , m_state(state)
{
    setFixedSize(8, 8);
}

void StatusDot::setState(const QString &state)
{
    if (m_state == state)
        return;
    m_state = state;
    update();
}

void StatusDot::paintEvent(QPaintEvent *)
{
    using P = Theme::Palette;
    static const QHash<QString, Theme::Role> roles = {
        {QStringLiteral("ok"), &P::ok}, {QStringLiteral("warn"), &P::warn},
        {QStringLiteral("err"), &P::err}, {QStringLiteral("busy"), &P::accent}};
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Theme::color(roles.value(m_state, &P::text3)));
    painter.drawEllipse(rect().adjusted(0, 0, -1, -1));
}

QPixmap coverPixmap(const QImage &image, int width, int height, int radius)
{
    if (image.isNull() || width <= 0 || height <= 0)
        return {};
    const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
    const QSize device = QSize(width, height) * dpr;
    const QImage scaled = image.scaled(device, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const int x = qMax(0, (scaled.width() - device.width()) / 2);
    const int y = qMax(0, (scaled.height() - device.height()) / 2);

    QPixmap pixmap(device);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, width, height), radius, radius);
    painter.setClipPath(path);
    painter.drawImage(QRectF(0, 0, width, height), scaled, QRectF(x, y, device.width(), device.height()));
    return pixmap;
}

AnimeCard::AnimeCard(ImageLoader *images, QWidget *parent)
    : QWidget(parent), m_images(images)
{
    setObjectName("AnimeCard"); setAttribute(Qt::WA_StyledBackground,true);
    setCursor(Qt::PointingHandCursor); setFocusPolicy(Qt::StrongFocus);
    auto *layout=new QVBoxLayout(this);
    layout->setContentsMargins(kCardPadding,kCardPadding,kCardPadding,6); layout->setSpacing(0);
    m_poster=new QLabel(this); m_poster->setObjectName("Poster"); m_poster->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_poster);
    m_rating=new QLabel(m_poster); m_rating->setObjectName("RatingBadge"); m_rating->hide();
    layout->addSpacing(10);
    m_title=label(QString(),"CardTitle",true,this);
    m_title->setTextFormat(Qt::PlainText);
    m_title->setAlignment(Qt::AlignTop|Qt::AlignLeft);
    layout->addWidget(m_title);
    layout->addSpacing(3);
    m_subtitle=label(QString(),"Faint",false,this);
    layout->addWidget(m_subtitle);
    setCardWidth(182);
}

void AnimeCard::setCardWidth(int width)
{
    const int posterWidth=width-2*kCardPadding, posterHeight=qRound(posterWidth*1.414);
    if(this->width()==width && m_poster->width()==posterWidth) { updateCardLayout(); return; }
    setFixedWidth(width);
    m_poster->setFixedSize(posterWidth,posterHeight);
    m_rating->move(8,8);
    updateCardLayout();
    updateCover();
}

void AnimeCard::updateCover()
{
    if(!m_cover.isNull()) m_poster->setPixmap(coverPixmap(m_cover,m_poster->width(),m_poster->height(),10));
}

void AnimeCard::setSubject(const AnimeSubject &subject)
{
    const bool sameCover=m_subject.bangumiId==subject.bangumiId && m_subject.coverUrl==subject.coverUrl && !m_cover.isNull();
    m_subject=subject;
    m_title->setText(subject.displayName());
    setAccessibleName(subject.displayName());
    QStringList meta;
    if(subject.year>0) meta<<QString::number(subject.year);
    if(!subject.platform.isEmpty()) meta<<subject.platform;
    if(subject.episodeCount>0) meta<<QStringLiteral("%1 话").arg(subject.episodeCount);
    m_subtitle->setText(meta.join(QStringLiteral(" · ")));
    m_rating->setVisible(subject.rating>0);
    if(subject.rating>0) {
        m_rating->setText(QStringLiteral("★ %1").arg(subject.rating,0,'f',1));
        m_rating->adjustSize();
    }
    updateCardLayout();
    if(sameCover)return;
    m_cover=QImage();
    m_poster->clear(); m_poster->setText(QStringLiteral("NEXUS"));
    const QString url=BangumiClient::coverUrl(subject.coverUrl,480);
    if(url.isEmpty() || !m_images)return;
    const int id=subject.bangumiId;
    QPointer<AnimeCard> self(this);
    m_images->request(url,[self,id](QImage image) {
        if(!self || self->m_subject.bangumiId!=id || image.isNull())return;
        self->m_cover=image;
        self->updateCover();
    });
}

void AnimeCard::mousePressEvent(QMouseEvent *event)
{
    if(event->button()==Qt::LeftButton) {
        m_keyboardFocus=false;update();
        setFocus(Qt::MouseFocusReason);emit clicked(m_subject);
    }
    QWidget::mousePressEvent(event);
}

void AnimeCard::updateCardLayout()
{
    m_title->ensurePolished(); m_subtitle->ensurePolished();
    const QFontMetrics metrics(m_title->font());
    const int measured=metrics.boundingRect(QRect(0,0,qMax(1,m_poster->width()),1000),
        Qt::TextWordWrap|Qt::AlignLeft,m_title->text()).height();
    const int titleHeight=qBound(metrics.height(),measured,metrics.lineSpacing()*2);
    const int metaHeight=m_subtitle->text().isEmpty() ? 0 : m_subtitle->fontMetrics().height();
    m_title->setFixedHeight(titleHeight);
    m_subtitle->setVisible(metaHeight>0);
    m_subtitle->setFixedHeight(metaHeight);
    setFixedHeight(m_poster->height()+titleHeight+metaHeight+kCardPadding+6+13);
    updateGeometry();
}

void AnimeCard::keyPressEvent(QKeyEvent *event)
{
    if(event->key()==Qt::Key_Return || event->key()==Qt::Key_Enter || event->key()==Qt::Key_Space) {
        emit clicked(m_subject); event->accept(); return;
    }
    QWidget::keyPressEvent(event);
}

void AnimeCard::enterEvent(QEnterEvent *event) { animateHover(true); QWidget::enterEvent(event); }
void AnimeCard::leaveEvent(QEvent *event) { animateHover(false); QWidget::leaveEvent(event); }

void AnimeCard::animateHover(bool hover)
{
    if(!m_hoverAnimation) {
        m_hoverAnimation=new QVariantAnimation(this);
        m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_hoverAnimation,&QVariantAnimation::valueChanged,this,[this](const QVariant &value){m_hoverLevel=value.toReal();update();});
    }
    m_hoverAnimation->stop();
    m_hoverAnimation->setDuration(Motion::duration(Motion::Fast));
    m_hoverAnimation->setStartValue(m_hoverLevel);
    m_hoverAnimation->setEndValue(hover?1.0:0.0);
    if(m_hoverAnimation->duration()<=0){m_hoverLevel=hover?1:0;update();return;}
    m_hoverAnimation->start();
}

void AnimeCard::focusInEvent(QFocusEvent *event)
{
    m_keyboardFocus=event->reason()==Qt::TabFocusReason || event->reason()==Qt::BacktabFocusReason
        || event->reason()==Qt::ShortcutFocusReason;
    QWidget::focusInEvent(event);update();
}

void AnimeCard::focusOutEvent(QFocusEvent *event)
{
    m_keyboardFocus=false;QWidget::focusOutEvent(event);update();
}

void AnimeCard::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    const bool focusRing=m_keyboardFocus && hasFocus();
    if(!focusRing && m_hoverLevel<=0.01)return;
    // The ring hugs the poster so the card itself never gains an outer frame.
    QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(Qt::NoBrush);
    QColor ring=focusRing ? Theme::c().accentText : Theme::c().accent;
    if(!focusRing) ring.setAlphaF(m_hoverLevel);
    painter.setPen(QPen(ring,2));
    painter.drawRoundedRect(QRectF(m_poster->geometry()).adjusted(-2,-2,2,2),12,12);
}


CardGrid::CardGrid(ImageLoader *images, QWidget *parent)
    : QWidget(parent)
    , m_images(images)
    , m_layout(new FlowLayout(this, kCardGap, 22))
{
    setLayout(m_layout);
}

void CardGrid::setSubjects(const QVector<AnimeSubject> &subjects)
{
    // Drop surplus cards from the end, then refill or extend the existing ones.
    while (m_layout->count() > subjects.size()) {
        if (auto *item = m_layout->takeAt(m_layout->count() - 1)) {
            if (auto *widget = item->widget()) { widget->hide(); widget->deleteLater(); }
            delete item;
        }
    }
    for (int i = 0; i < subjects.size(); ++i) {
        auto *card = i < m_layout->count() ? qobject_cast<AnimeCard *>(m_layout->itemAt(i)->widget()) : nullptr;
        if (!card) {
            card = new AnimeCard(m_images, this);
            if (m_cardWidth > 0) card->setCardWidth(m_cardWidth);
            connect(card, &AnimeCard::clicked, this, &CardGrid::cardClicked);
            m_layout->addWidget(card);
            card->show();
        }
        card->setSubject(subjects.at(i));
    }
    updateCardWidths();
}

void CardGrid::clear()
{
    m_layout->clear();
}

void CardGrid::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event); updateCardWidths();
}

void CardGrid::updateCardWidths()
{
    const int available=qMax(kCardMin,width()-2);
    const int columns=qMax(1,(available+kCardGap)/(kCardMin+kCardGap));
    m_cardWidth=qBound(kCardMin,(available-kCardGap*(columns-1))/columns,kCardMax);
    for(int i=0;i<m_layout->count();++i)
        if(auto *card=qobject_cast<AnimeCard *>(m_layout->itemAt(i)->widget())) card->setCardWidth(m_cardWidth);
    m_layout->invalidate();
}

EmptyState::EmptyState(const QString &icon, const QString &title, const QString &description, QWidget *parent)
    : QWidget(parent)
{
    setObjectName("EmptyState");
    setAttribute(Qt::WA_StyledBackground,true);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28,40,28,40);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(6);

    m_icon = new IconLabel(icon,&Theme::Palette::accentText,26,this);
    m_icon->setObjectName("EmptyIcon");
    m_icon->setFixedSize(56,56);
    layout->addWidget(m_icon,0,Qt::AlignHCenter);
    layout->addSpacing(10);

    m_title = new QLabel(this);
    m_title->setObjectName(QStringLiteral("EmptyTitle"));
    m_title->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_title);

    m_description = new QLabel(this);
    m_description->setObjectName(QStringLiteral("Muted"));
    m_description->setAlignment(Qt::AlignCenter);
    m_description->setWordWrap(true);
    m_description->setMinimumWidth(300);
    m_description->setMaximumWidth(440);
    layout->addWidget(m_description,0,Qt::AlignHCenter);

    setText(title, description);
}

void EmptyState::setText(const QString &title, const QString &description)
{
    m_title->setText(title);
    m_description->setText(description);
    m_description->setVisible(!description.isEmpty());
    setVisible(!title.isEmpty() || !description.isEmpty());
}

PageHeader pageHeader(const QString &title, QWidget *parent)
{
    PageHeader header;
    header.widget=new QWidget(parent);
    auto *row=new QHBoxLayout(header.widget);
    row->setContentsMargins(0,0,0,0); row->setSpacing(12);
    row->addWidget(label(title,"PageTitle",false,header.widget),0,Qt::AlignVCenter);
    header.meta=label(QString(),"PageMeta",false,header.widget);
    row->addWidget(header.meta,0,Qt::AlignVCenter);
    row->addStretch(1);
    header.actions=new QHBoxLayout; header.actions->setSpacing(8);
    row->addLayout(header.actions);
    return header;
}

QVBoxLayout *pageLayout(QWidget *page)
{
    auto *layout=new QVBoxLayout(page);
    layout->setContentsMargins(28,22,28,18);
    layout->setSpacing(16);
    return layout;
}

QScrollArea *scrollArea(QWidget *content, QWidget *parent)
{
    auto *scroll=new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    if(content)scroll->setWidget(content);
    return scroll;
}

QWidget *overlayStack(QWidget *content, EmptyState *empty, QWidget *parent)
{
    auto *box=new QWidget(parent);
    auto *grid=new QGridLayout(box);
    grid->setContentsMargins(0,0,0,0);
    grid->addWidget(content,0,0);
    grid->addWidget(empty,0,0);
    return box;
}

QPushButton *iconButton(const QString &icon, const QString &tip, QWidget *parent, int size)
{
    auto *button=new QPushButton(parent);
    button->setObjectName(QStringLiteral("IconButton"));
    button->setIcon(Theme::icon(icon));
    button->setIconSize(QSize(18,18));
    button->ensurePolished();
    button->setFixedSize(size,size);
    button->setCursor(Qt::PointingHandCursor);
    button->setToolTip(tip);
    button->setAccessibleName(tip);
    return button;
}

QFrame *divider(QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setObjectName(QStringLiteral("SettingDivider"));
    line->setFixedHeight(1);
    return line;
}

QLabel *label(const QString &text, const QString &objectName, bool wrap, QWidget *parent)
{
    auto *widget = new QLabel(text, parent);
    if (!objectName.isEmpty())
        widget->setObjectName(objectName);
    widget->setWordWrap(wrap);
    return widget;
}

QString fmtSize(qint64 bytes)
{
    if (bytes <= 0)
        return QStringLiteral("—");
    static const QVector<QPair<QString, qint64>> units = {
        {QStringLiteral("TiB"), 1LL << 40},
        {QStringLiteral("GiB"), 1LL << 30},
        {QStringLiteral("MiB"), 1LL << 20},
        {QStringLiteral("KiB"), 1LL << 10},
    };
    for (const auto &[unit, factor] : units) {
        if (bytes >= factor)
            return QStringLiteral("%1 %2").arg(QString::number(static_cast<double>(bytes) / factor, 'f', 1), unit);
    }
    return QStringLiteral("%1 B").arg(bytes);
}

QString fmtSpeed(qint64 bytes)
{
    if (bytes >= (1LL << 20))
        return QStringLiteral("%1 MB/s").arg(bytes / 1048576.0, 0, 'f', 1);
    if (bytes >= (1LL << 10))
        return QStringLiteral("%1 KB/s").arg(bytes / 1024.0, 0, 'f', 0);
    return QStringLiteral("%1 B/s").arg(bytes);
}

QString fmtAge(const QDateTime &time)
{
    if (!time.isValid())
        return {};
    const qint64 days = time.date().daysTo(QDate::currentDate());
    if (days <= 0)
        return QStringLiteral("今天");
    if (days < 31)
        return QStringLiteral("%1 天前").arg(days);
    return time.date().toString(QStringLiteral("yyyy-MM-dd"));
}

QLabel *statTile(QHBoxLayout *row, const QString &icon, const QString &caption)
{
    auto *tile=new QFrame; tile->setObjectName("MetricCard");
    auto *line=new QHBoxLayout(tile); line->setContentsMargins(16,12,16,12); line->setSpacing(12);
    auto *glyph=new IconLabel(icon,&Theme::Palette::accentText,18,tile); glyph->setObjectName("StatIcon"); glyph->setFixedSize(36,36);
    line->addWidget(glyph);
    auto *col=new QVBoxLayout; col->setSpacing(1);
    col->addWidget(label(caption,"MetricCaption",false,tile));
    auto *value=label(QStringLiteral("—"),"MetricValue",false,tile);
    col->addWidget(value); line->addLayout(col,1);
    row->addWidget(tile,1);
    return value;
}

void prepareTable(QTableWidget *table)
{
    table->setItemDelegate(new SoftFocusDelegate(table));
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table->horizontalHeader()->setHighlightSections(false);
    table->horizontalHeader()->setMinimumSectionSize(48);
    table->setShowGrid(false); table->setAlternatingRowColors(false);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(48);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setMouseTracking(true); table->setWordWrap(false);
    table->setFocusPolicy(Qt::StrongFocus);
}
