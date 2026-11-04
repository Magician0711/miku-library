#include "HomePage.h"

#include "ui/ImageLoader.h"
#include "ui/Motion.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "net/BangumiClient.h"

#include <QButtonGroup>
#include <QDate>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {
const QStringList kDayNames = {QStringLiteral("周一"), QStringLiteral("周二"), QStringLiteral("周三"),
                               QStringLiteral("周四"), QStringLiteral("周五"), QStringLiteral("周六"),
                               QStringLiteral("周日")};

// Bangumi seasons start in January, April, July and October.
QString seasonLabel()
{
    const QDate today = QDate::currentDate();
    return QStringLiteral("%1 年 %2 月新番").arg(today.year()).arg((today.month() - 1) / 3 * 3 + 1);
}
}

HomePage::HomePage(BangumiClient *bangumi, ImageLoader *images, QWidget *parent)
    : QWidget(parent), m_bangumi(bangumi), m_images(images)
{
    auto *layout=pageLayout(this);
    auto header=pageHeader(QStringLiteral("本季放送"),this);
    m_status=header.meta;
    auto *refresh=iconButton("refresh",QStringLiteral("刷新放送表"),this,34);
    connect(refresh,&QPushButton::clicked,this,&HomePage::load);
    header.actions->addWidget(refresh);
    layout->addWidget(header.widget);

    auto *dayBar=new QFrame(this); dayBar->setObjectName("Segmented");
    dayBar->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    auto *tabRow=new QHBoxLayout(dayBar); tabRow->setContentsMargins(4,4,4,4); tabRow->setSpacing(2);
    m_tabs=new QButtonGroup(this); m_tabs->setExclusive(true);
    for(int day=1;day<=7;++day) {
        auto *button=new QPushButton(kDayNames.at(day-1),dayBar); button->setObjectName("DayTab");
        button->setCheckable(true); button->setCursor(Qt::PointingHandCursor);
        m_tabs->addButton(button,day); tabRow->addWidget(button);
    }
    new Motion::SlidingIndicator(dayBar,m_tabs->buttons(),Motion::SlidingIndicator::Look::Outlined,8);
    connect(m_tabs,&QButtonGroup::idClicked,this,[this](int day){Motion::crossfade(m_results);showDay(day);});
    auto *dayLine=new QHBoxLayout;
    dayLine->addStretch(1); dayLine->addWidget(dayBar); dayLine->addStretch(1);
    layout->addLayout(dayLine);

    m_grid=new CardGrid(m_images);
    connect(m_grid,&CardGrid::cardClicked,this,&HomePage::animeSelected);
    m_empty=new EmptyState("calendar",QStringLiteral("正在准备放送表"),QStringLiteral("新一周的故事，即将开始。"),this);
    m_results=overlayStack(scrollArea(m_grid),m_empty,this);
    layout->addWidget(m_results,1);
    updateTabs();
}

void HomePage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_loaded)
        load();
}

void HomePage::load()
{
    m_status->setText(QStringLiteral("加载中…"));
    m_empty->setText(QStringLiteral("加载中…"), QStringLiteral("正在从 Bangumi 获取本季放送表。"));
    m_empty->show();
    m_grid->clear();

    const int generation = ++m_generation;
    m_bangumi->calendar([this, generation](QVector<QPair<int, QVector<AnimeSubject>>> days, QString error) {
        if (generation != m_generation)
            return;
        // The loading placeholder fades into the result rather than snapping.
        Motion::crossfade(m_results);
        if (!error.isEmpty()) {
            m_status->setText(QStringLiteral("加载失败"));
            m_empty->setText(QStringLiteral("加载失败"), error + QStringLiteral("\n点击右上角刷新重试。"));
            return;
        }
        m_calendar.clear();
        int total = 0;
        for (const auto &[weekday, items] : days) {
            m_calendar.insert(weekday, items);
            total += items.size();
        }
        m_loaded = true;
        m_status->setText(QStringLiteral("%1 · 共 %2 部").arg(seasonLabel()).arg(total));
        updateTabs();
        selectToday();
    });
}

void HomePage::updateTabs()
{
    const int today = QDate::currentDate().dayOfWeek();
    for (int day = 1; day <= 7; ++day) {
        auto *button = m_tabs->button(day);
        const QString &name = kDayNames.at(day - 1);
        const auto count = m_calendar.value(day).size();
        button->setText(count > 0 ? QStringLiteral("%1  %2").arg(name).arg(count) : name);
        button->setToolTip(day == today ? QStringLiteral("今天") : QString());
        button->setAccessibleName(QStringLiteral("%1，%2 部").arg(kDayNames.at(day - 1)).arg(count));
    }
}

void HomePage::selectToday()
{
    const int today = QDate::currentDate().dayOfWeek();
    const int target = m_calendar.contains(today) ? today : m_calendar.keys().value(0, 1);
    if (auto *button = m_tabs->button(target))
        button->setChecked(true);
    showDay(target);
}

void HomePage::showDay(int weekday)
{
    const auto subjects = m_calendar.value(weekday);
    m_grid->setSubjects(subjects);
    if (subjects.isEmpty()) {
        m_empty->setText(QStringLiteral("%1暂无放送").arg(kDayNames.value(weekday - 1)),
                         QStringLiteral("换一天看看，或点击刷新。"));
        m_empty->show();
    } else {
        m_empty->hide();
    }
}
