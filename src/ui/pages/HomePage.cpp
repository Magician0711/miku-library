#include "HomePage.h"

#include "ui/ImageLoader.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "net/BangumiClient.h"

#include <QButtonGroup>
#include <QDate>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFrame>

namespace {
const QStringList kDayNames = {QStringLiteral("周一"), QStringLiteral("周二"), QStringLiteral("周三"),
                               QStringLiteral("周四"), QStringLiteral("周五"), QStringLiteral("周六"),
                               QStringLiteral("周日")};
}

HomePage::HomePage(BangumiClient *bangumi, ImageLoader *images, QWidget *parent)
    : QWidget(parent), m_bangumi(bangumi), m_images(images)
{
    auto *layout=new QVBoxLayout(this);
    layout->setContentsMargins(32,24,32,20); layout->setSpacing(0);
    auto *heading=new QHBoxLayout;
    heading->addWidget(pageHeading(QStringLiteral("本季放送"),QStringLiteral("跟随每周更新，发现下一部喜欢的作品。")),1);
    m_status=label(QString(),"Faint");
    heading->addWidget(m_status);
    auto *refresh=new QPushButton(this); refresh->setObjectName("IconButton");
    refresh->setIcon(Theme::icon("refresh")); refresh->setFixedSize(36,36);
    refresh->setToolTip(QStringLiteral("刷新放送表"));
    refresh->setAccessibleName(QStringLiteral("刷新放送表"));
    connect(refresh,&QPushButton::clicked,this,&HomePage::load); heading->addWidget(refresh);
    layout->addLayout(heading);
    layout->addSpacing(18);
    auto *dayBar=new QFrame(this); dayBar->setObjectName("DayBar");
    dayBar->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    auto *tabRow=new QHBoxLayout(dayBar); tabRow->setContentsMargins(5,5,5,5); tabRow->setSpacing(3);
    m_tabs=new QButtonGroup(this); m_tabs->setExclusive(true);
    for(int day=1;day<=7;++day) {
        auto *button=new QPushButton(kDayNames.at(day-1),dayBar); button->setObjectName("DayTab");
        button->setCheckable(true); button->setCursor(Qt::PointingHandCursor);
        m_tabs->addButton(button,day); tabRow->addWidget(button);
    }
    connect(m_tabs,&QButtonGroup::idClicked,this,&HomePage::showDay);
    auto *dayLine=new QHBoxLayout;
    dayLine->addStretch(1); dayLine->addWidget(dayBar); dayLine->addStretch(1);
    layout->addLayout(dayLine);
    layout->addSpacing(10);
    auto *results=new QGridLayout; results->setContentsMargins(0,0,0,0);
    auto *scroll=new QScrollArea(this); scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_grid=new CardGrid(m_images,scroll);
    connect(m_grid,&CardGrid::cardClicked,this,&HomePage::animeSelected); scroll->setWidget(m_grid);
    results->addWidget(scroll,0,0);
    m_empty=new EmptyState(Icons::HOME,QStringLiteral("正在准备放送表"),QStringLiteral("新一周的故事，即将开始。"),this);
    results->addWidget(m_empty,0,0); layout->addLayout(results,1);
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
        if (!error.isEmpty()) {
            m_status->setText(QStringLiteral("加载失败：") + error);
            m_empty->setText(QStringLiteral("加载失败"), error);
            return;
        }
        m_calendar.clear();
        int total = 0;
        for (const auto &[weekday, items] : days) {
            m_calendar.insert(weekday, items);
            total += items.size();
        }
        m_loaded = true;
        m_status->setText(QStringLiteral("共 %1 部").arg(total));
        selectToday();
    });
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
