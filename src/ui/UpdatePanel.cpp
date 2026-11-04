// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "UpdatePanel.h"

#include "Motion.h"
#include "Theme.h"
#include "Widgets.h"

#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>

QString episodeSummary(const QStringList &episodes)
{
    if (episodes.isEmpty())
        return {};
    bool consecutive = true;
    for (int i = 1; i < episodes.size() && consecutive; ++i)
        consecutive = episodes.at(i).toDouble() == episodes.at(i - 1).toDouble() + 1;
    if (episodes.size() == 1)
        return QStringLiteral("第 %1 话").arg(episodes.first());
    if (consecutive)
        return QStringLiteral("第 %1–%2 话").arg(episodes.first(), episodes.last());
    if (episodes.size() <= 4)
        return QStringLiteral("第 %1 话").arg(episodes.join(QStringLiteral("、")));
    return QStringLiteral("第 %1 等 %2 话").arg(episodes.mid(0, 3).join(QStringLiteral("、"))).arg(episodes.size());
}

UpdatePanel::UpdatePanel(QWidget *canvas)
    : QFrame(canvas)
{
    setObjectName("UpdatePanel");
    setFixedWidth(380);
    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(28); shadow->setOffset(0, 6); shadow->setColor(Theme::c().shadow);
    setGraphicsEffect(shadow);
    connect(Theme::notifier(), &Theme::Notifier::changed, this, [shadow] { shadow->setColor(Theme::c().shadow); });

    auto *col = new QVBoxLayout(this);
    col->setContentsMargins(16, 12, 8, 14);
    col->setSpacing(8);
    auto *header = new QHBoxLayout; header->setSpacing(10);
    auto *glyph = new IconLabel("download", &Theme::Palette::accentText, 18, this);
    glyph->setObjectName("StatIcon"); glyph->setFixedSize(32, 32);
    header->addWidget(glyph);
    auto *titles = new QVBoxLayout; titles->setSpacing(0);
    titles->addWidget(label(QStringLiteral("追番有新集"), "GuideNoteTitle", false, this));
    m_count = label(QString(), "SettingHint", false, this);
    titles->addWidget(m_count);
    header->addLayout(titles, 1);
    auto *close = iconButton("close", QStringLiteral("暂时关闭（下次启动会再次检查）"), this, 30);
    connect(close, &QPushButton::clicked, this, [this] { Motion::crossfade(parentWidget(), Motion::Fast); hide(); });
    header->addWidget(close, 0, Qt::AlignTop);
    col->addLayout(header);

    auto *body = new QWidget; body->setObjectName("ScrollBody");
    m_rows = new QVBoxLayout(body); m_rows->setContentsMargins(0, 0, 8, 0); m_rows->setSpacing(0);
    auto *scroll = scrollArea(body, this);
    scroll->setObjectName("UpdateRows");
    col->addWidget(scroll);

    m_all = new QPushButton(QStringLiteral("全部下载"), this);
    m_all->setObjectName("Primary");
    m_all->setIcon(Theme::icon("download", &Theme::Palette::accentInk));
    connect(m_all, &QPushButton::clicked, this, [this] {
        const auto notices = m_notices;
        for (const auto &[row, notice] : notices) { emit downloadRequested(notice); removeRow(row); }
    });
    auto *footer = new QHBoxLayout; footer->setContentsMargins(0, 0, 8, 0); footer->addStretch(1); footer->addWidget(m_all);
    col->addLayout(footer);

    hide();
    canvas->installEventFilter(this);
}

void UpdatePanel::addNotice(const UpdateNotice &notice)
{
    for (int i = 0; i < m_notices.size(); ++i) {
        if (m_notices.at(i).second.target.subject.bangumiId != notice.target.subject.bangumiId) continue;
        auto *old = m_notices.takeAt(i).first;
        old->hide(); old->deleteLater();
        break;
    }
    auto *row = new QFrame(this);
    row->setObjectName("UpdateRow");
    auto *line = new QHBoxLayout(row); line->setContentsMargins(0, 8, 0, 8); line->setSpacing(8);
    auto *text = new QVBoxLayout; text->setSpacing(1);
    auto *name = label(notice.target.subject.displayName(), "CardTitle", false, row);
    name->setTextFormat(Qt::PlainText);
    name->setMinimumWidth(1);
    name->setToolTip(notice.target.subject.displayName());
    text->addWidget(name);
    QStringList kind = {notice.target.preference.group};
    if (!notice.target.preference.resolution.isEmpty()) kind << notice.target.preference.resolution;
    kind << episodeSummary(notice.episodes);
    auto *detail = label(kind.join(QStringLiteral(" · ")), "SettingHint", false, row);
    detail->setMinimumWidth(1);
    text->addWidget(detail);
    line->addLayout(text, 1);
    auto *ignore = new QPushButton(QStringLiteral("忽略"), row); ignore->setObjectName("Ghost");
    ignore->setToolTip(QStringLiteral("不再提示这些集数，之后有更新的集时仍会提醒"));
    auto *download = new QPushButton(QStringLiteral("下载"), row); download->setObjectName("Soft");
    line->addWidget(ignore); line->addWidget(download);
    connect(ignore, &QPushButton::clicked, this, [this, row, notice] { emit ignoreRequested(notice); removeRow(row); });
    connect(download, &QPushButton::clicked, this, [this, row, notice] { emit downloadRequested(notice); removeRow(row); });

    m_rows->addWidget(row);
    m_notices.append({row, notice});
    if (isHidden()) {
        Motion::crossfade(parentWidget(), Motion::Normal);
        show();
    }
    refresh();
}

void UpdatePanel::removeRow(QWidget *row)
{
    for (int i = 0; i < m_notices.size(); ++i) {
        if (m_notices.at(i).first != row) continue;
        m_notices.removeAt(i);
        row->hide();
        row->deleteLater();
        break;
    }
    if (m_notices.isEmpty()) {
        Motion::crossfade(parentWidget(), Motion::Fast);
        hide();
        return;
    }
    refresh();
}

void UpdatePanel::refresh()
{
    int episodes = 0;
    for (int i = 0; i < m_notices.size(); ++i) {
        auto *row = m_notices.at(i).first;
        episodes += m_notices.at(i).second.episodes.size();
        // Hairlines separate rows; the first row has none.
        if (row->property("first").toBool() != (i == 0)) {
            row->setProperty("first", i == 0);
            row->style()->unpolish(row); row->style()->polish(row);
        }
    }
    m_count->setText(QStringLiteral("%1 部番剧 · 共 %2 集").arg(m_notices.size()).arg(episodes));
    m_all->setVisible(m_notices.size() > 1);
    // Show up to four rows; the rest scroll.
    auto *scroll = findChild<QScrollArea *>("UpdateRows");
    const int rows = qMin<qsizetype>(m_notices.size(), 4);
    scroll->setFixedHeight(rows * 58);
    adjustSize();
    place();
}

void UpdatePanel::place()
{
    if (!parentWidget()) return;
    const QRect area = parentWidget()->rect();
    move(area.right() - width() - 20, area.bottom() - height() - 20);
    raise();
}

bool UpdatePanel::eventFilter(QObject *object, QEvent *event)
{
    if (object == parentWidget() && (event->type() == QEvent::Resize || event->type() == QEvent::LayoutRequest))
        place();
    return QFrame::eventFilter(object, event);
}
