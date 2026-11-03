#include "Widgets.h"

#include "FlowLayout.h"
#include "ImageLoader.h"
#include "Theme.h"
#include "Controls.h"
#include "net/BangumiClient.h"

#include <QDateTime>
#include <QEnterEvent>
#include <QHash>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QPointer>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QTableWidget>
#include <QHeaderView>
#include <QHBoxLayout>

namespace {

QPair<QString, QString> confidenceColors(Confidence confidence)
{
    switch (confidence) {
    case Confidence::Probed: return {Theme::PROBED_BG, Theme::PROBED_FG};
    case Confidence::Metadata: return {Theme::SUB_BG, Theme::SUB_FG};
    case Confidence::Guessed: return {Theme::GUESSED_BG, Theme::GUESSED_FG};
    case Confidence::Unknown: break;
    }
    return {Theme::UNKNOWN_BG, Theme::UNKNOWN_FG};
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

Chip::Chip(const QString &text, const QString &background, const QString &foreground,
           const QString &tooltip, QWidget *parent)
    : QLabel(text, parent)
{
    setObjectName(QStringLiteral("Chip"));
    setStyleSheet(QStringLiteral("background:%1;color:%2;border-radius:5px;padding:2px 7px;"
                                 "font-size:8.5pt;font-weight:600;")
                      .arg(background, foreground));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    if (!tooltip.isEmpty())
        setToolTip(tooltip);
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
    add(tags.hdrType, tags.hdrConf, QStringLiteral("HDR"));
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
    setFixedSize(9, 9);
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
    QColor color = QColor(Theme::TEXT3);
    if (m_state == QStringLiteral("ok"))
        color = QColor(Theme::OK);
    else if (m_state == QStringLiteral("warn"))
        color = QColor(Theme::WARN);
    else if (m_state == QStringLiteral("err"))
        color = QColor(Theme::ERR);
    else if (m_state == QStringLiteral("busy"))
        color = QColor(Theme::ACCENT);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(rect().adjusted(0, 0, -1, -1));
}

QPixmap coverPixmap(const QImage &image, int width, int height, int radius)
{
    if (image.isNull())
        return {};
    const QImage scaled = image.scaled(width, height, Qt::KeepAspectRatioByExpanding,
                                       Qt::SmoothTransformation);
    const int x = qMax(0, (scaled.width() - width) / 2);
    const int y = qMax(0, (scaled.height() - height) / 2);
    const QImage cropped = scaled.copy(x, y, width, height);

    QPixmap pixmap(width, height);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, width, height), radius, radius);
    painter.setClipPath(path);
    painter.drawImage(0, 0, cropped);
    return pixmap;
}

AnimeCard::AnimeCard(ImageLoader *images, QWidget *parent)
    : QWidget(parent), m_images(images)
{
    setObjectName("AnimeCard"); setAttribute(Qt::WA_StyledBackground,true);
    setCursor(Qt::PointingHandCursor); setFocusPolicy(Qt::StrongFocus);
    auto *layout=new QVBoxLayout(this);
    layout->setContentsMargins(6,6,6,8); layout->setSpacing(0);
    m_poster=new QLabel(this); m_poster->setObjectName("Poster"); m_poster->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_poster);
    layout->addSpacing(8);
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
    if(this->width()==width && m_poster->width()==width-12) { updateCardLayout(); return; }
    const int posterWidth=width-12, posterHeight=qRound(posterWidth*1.414);
    setFixedWidth(width);
    m_poster->setFixedSize(posterWidth,posterHeight);
    updateCardLayout();
    if(!m_cover.isNull()) m_poster->setPixmap(coverPixmap(m_cover,posterWidth,posterHeight,8));
}
void AnimeCard::setSubject(const AnimeSubject &subject)
{
    m_subject=subject; m_cover=QImage();
    m_title->setText(subject.displayName());
    setAccessibleName(subject.displayName());
    QStringList meta;
    if(subject.year>0) meta<<QString::number(subject.year);
    if(!subject.platform.isEmpty()) meta<<subject.platform;
    if(subject.rating>0) meta<<QStringLiteral("%1 分").arg(subject.rating,0,'f',1);
    m_subtitle->setText(meta.join(QStringLiteral(" · ")));
    updateCardLayout();
    m_poster->clear(); m_poster->setText(QStringLiteral("NEXUS"));
    const QString url=BangumiClient::coverUrl(subject.coverUrl,480);
    if(url.isEmpty() || !m_images)return;
    const int id=subject.bangumiId;
    QPointer<AnimeCard> self(this);
    m_images->request(url,[self,id](QImage image) {
        if(!self || self->m_subject.bangumiId!=id || image.isNull())return;
        self->m_cover=image;
        self->m_poster->setPixmap(coverPixmap(image,self->m_poster->width(),self->m_poster->height(),8));
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
    setFixedHeight(m_poster->height()+titleHeight+metaHeight+25);
    updateGeometry();
}
void AnimeCard::keyPressEvent(QKeyEvent *event)
{
    if(event->key()==Qt::Key_Return || event->key()==Qt::Key_Space) {
        emit clicked(m_subject); event->accept(); return;
    }
    QWidget::keyPressEvent(event);
}
void AnimeCard::enterEvent(QEnterEvent *event) { QWidget::enterEvent(event); }
void AnimeCard::leaveEvent(QEvent *event) { QWidget::leaveEvent(event); }
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
    if(m_keyboardFocus && hasFocus()) {
        QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(Qt::NoBrush);painter.setPen(QPen(QColor(Theme::ACCENT),1.5));
        painter.drawRoundedRect(QRectF(m_poster->geometry()).adjusted(-2,-2,2,2),10,10);
    }
}


CardGrid::CardGrid(ImageLoader *images, QWidget *parent)
    : QWidget(parent)
    , m_images(images)
    , m_layout(new FlowLayout(this, 12, 18))
{
    setLayout(m_layout);
}

void CardGrid::setSubjects(const QVector<AnimeSubject> &subjects)
{
    clear();
    for (const auto &subject : subjects) {
        auto *card = new AnimeCard(m_images, this);
        card->setSubject(subject);
        connect(card, &AnimeCard::clicked, this, &CardGrid::cardClicked);
        m_layout->addWidget(card);
    }
    updateCardWidths();
}

void CardGrid::clear()
{
    m_layout->clear();
}

EmptyState::EmptyState(const QString &icon, const QString &title, const QString &description, QWidget *parent)
    : QWidget(parent)
{
    setObjectName("EmptyState");
    setAttribute(Qt::WA_StyledBackground,true);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28,40,28,40);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(8);

    m_icon = new QLabel(this);
    m_icon->setObjectName("EmptyIcon");
    m_icon->setAlignment(Qt::AlignCenter);
    m_icon->setFixedSize(64,64);
    const QString name=icon==Icons::SEARCH ? "search" : icon==Icons::HOME ? "home" : icon==Icons::DOWNLOAD ? "download" : "library";
    m_icon->setPixmap(Theme::icon(name,QColor(Theme::ACCENT_TEXT)).pixmap(30,30));
    layout->addWidget(m_icon,0,Qt::AlignHCenter);
    layout->addSpacing(8);

    m_title = new QLabel(this);
    m_title->setObjectName(QStringLiteral("H2"));
    m_title->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_title);

    m_description = new QLabel(this);
    m_description->setObjectName(QStringLiteral("Muted"));
    m_description->setAlignment(Qt::AlignCenter);
    m_description->setWordWrap(true);
    m_description->setMaximumWidth(420);
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

QFrame *divider(QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet(QStringLiteral("color:#dce9e7;background:#dce9e7;max-height:1px;"));
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

void CardGrid::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event); updateCardWidths();
}
void CardGrid::updateCardWidths()
{
    const int columns=qMax(1,(width()+12)/190);
    const int cardWidth=qBound(150,(width()-12*(columns-1)-2)/columns,224);
    for(int i=0;i<m_layout->count();++i)
        if(auto *card=qobject_cast<AnimeCard *>(m_layout->itemAt(i)->widget())) card->setCardWidth(cardWidth);
    m_layout->invalidate();
}
QWidget *pageHeading(const QString &title, const QString &description)
{
    auto *widget=new QWidget; auto *col=new QVBoxLayout(widget);
    col->setContentsMargins(0,0,0,0); col->setSpacing(8);
    col->addWidget(label(title,"H1"));
    if(!description.isEmpty()) col->addWidget(label(description,"Muted",true));
    return widget;
}
QLabel *metricCard(QHBoxLayout *row, const QString &caption)
{
    auto *card=new QFrame; card->setObjectName("MetricCard");
    auto *col=new QVBoxLayout(card); col->setContentsMargins(18,14,18,14); col->setSpacing(6);
    col->addWidget(label(caption,"MetricCaption"));
    auto *value=label(QStringLiteral("—"),"MetricValue");
    col->addWidget(value); row->addWidget(card,1); return value;
}
void prepareTable(QTableWidget *table)
{
    table->setItemDelegate(new SoftFocusDelegate(table));
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table->setShowGrid(false); table->setAlternatingRowColors(false);
    table->verticalHeader()->setDefaultSectionSize(52);
    table->setMouseTracking(true); table->setWordWrap(false);
    table->horizontalHeader()->setHighlightSections(false);
    table->setFocusPolicy(Qt::StrongFocus);
}
