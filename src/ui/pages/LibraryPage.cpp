// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "LibraryPage.h"

#include "core/Database.h"
#include "core/LocalLibrary.h"
#include "ui/ImageLoader.h"
#include "ui/Motion.h"
#include "ui/Overlays.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "net/BangumiClient.h"

#include <QButtonGroup>
#include <QFrame>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QTimer>
#include <QVBoxLayout>

namespace {
enum Scope { All, Followed, Local };
}

LibraryPage::LibraryPage(Database *db, BangumiClient *bangumi, ImageLoader *images, LocalLibrary *library, QWidget *parent)
    : QWidget(parent), m_library(library), m_db(db), m_bangumi(bangumi), m_images(images)
{
    // Library changes and metadata fetches can arrive in bursts; refresh once per burst.
    m_refreshTimer=new QTimer(this); m_refreshTimer->setSingleShot(true); m_refreshTimer->setInterval(50);
    connect(m_refreshTimer,&QTimer::timeout,this,&LibraryPage::refresh);
    connect(library,&LocalLibrary::changed,this,[this]{if(isVisible())m_refreshTimer->start();});

    auto *layout=pageLayout(this);
    auto header=pageHeader(QStringLiteral("我的追番"),this);
    m_count=header.meta;
    auto *scopeBar=new QFrame(this); scopeBar->setObjectName("Segmented");
    auto *scopeRow=new QHBoxLayout(scopeBar); scopeRow->setContentsMargins(4,4,4,4); scopeRow->setSpacing(2);
    m_scope=new QButtonGroup(this);
    const QStringList scopes={QStringLiteral("全部"),QStringLiteral("追番"),QStringLiteral("已下载")};
    for(int i=0;i<scopes.size();++i) {
        auto *button=new QPushButton(scopes.at(i),scopeBar); button->setObjectName("SegmentButton");
        button->setCheckable(true); button->setChecked(i==All); button->setCursor(Qt::PointingHandCursor);
        m_scope->addButton(button,i); scopeRow->addWidget(button);
    }
    new Motion::SlidingIndicator(scopeBar,m_scope->buttons(),Motion::SlidingIndicator::Look::Outlined,8);
    connect(m_scope,&QButtonGroup::idClicked,this,[this]{Motion::crossfade(m_results);refresh();});
    m_checkUpdates=new QPushButton(Theme::icon("refresh"),QStringLiteral("检查追更"),this);
    m_checkUpdates->setToolTip(QStringLiteral("立即检查下载过的番剧在同一字幕组是否有新集"));
    m_checkUpdates->setCursor(Qt::PointingHandCursor);
    connect(m_checkUpdates,&QPushButton::clicked,this,&LibraryPage::checkUpdatesRequested);
    header.actions->addWidget(m_checkUpdates);
    header.actions->addWidget(scopeBar);
    layout->addWidget(header.widget);

    m_grid=new CardGrid(m_images); connect(m_grid,&CardGrid::cardClicked,this,&LibraryPage::animeSelected);
    m_empty=new EmptyState("library",QStringLiteral("收藏室，等你添上第一部"),QStringLiteral("在作品详情中点击「追番」，就能在这里找到它。"),this);
    m_results=overlayStack(scrollArea(m_grid),m_empty,this);
    layout->addWidget(m_results,1);
}

void LibraryPage::setUpdateCheck(bool running,const QString &result)
{
    m_checkUpdates->setEnabled(!running);
    m_checkUpdates->setText(running?QStringLiteral("检查中…"):QStringLiteral("检查追更"));
    if(!running&&!result.isEmpty()&&m_checkUpdates->isVisible())
        Ui::showTooltip(m_checkUpdates,result,m_checkUpdates->mapToGlobal(QPoint(0,m_checkUpdates->height()+6)));
}

void LibraryPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refresh();
}

void LibraryPage::refresh()
{
    const int scope=m_scope->checkedId();
    const auto followed=m_db->followedIds();
    QHash<int,AnimeSubject> local;
    for(const auto &subject:m_library->subjects()) local.insert(subject.bangumiId,subject);

    QVector<int> ids;
    if(scope!=Local) ids=followed;
    if(scope!=Followed)
        for(auto it=local.cbegin();it!=local.cend();++it)
            if(!ids.contains(it.key()))ids.append(it.key());

    m_count->setText(QStringLiteral("%1 部收藏 · %2 部已下载").arg(followed.size()).arg(local.size()));
    if (ids.isEmpty()) {
        m_grid->clear();
        if(scope==Local) m_empty->setText(QStringLiteral("还没有下载完成的番剧"),QStringLiteral("下载完成的视频会出现在这里，可直接播放。"));
        else m_empty->setText(QStringLiteral("收藏室，等你添上第一部"),QStringLiteral("在作品详情中点击「追番」，就能在这里找到它。"));
        m_empty->show();
        return;
    }
    m_empty->hide();

    QVector<AnimeSubject> subjects;
    QPointer<LibraryPage> self(this);
    for (int id : ids) {
        if (auto subject = m_db->getSubject(id)) { subjects.push_back(*subject); continue; }
        if (local.contains(id)) { subjects.push_back(local.value(id)); continue; }
        if (m_fetching.contains(id)) continue;
        m_fetching.insert(id);
        m_bangumi->getSubject(id, [self, id](AnimeSubject subject, QString error) {
            if (!self) return;
            self->m_fetching.remove(id);
            if (error.isEmpty() && subject.bangumiId == id) {
                self->m_db->upsertSubject(subject);
                self->m_refreshTimer->start();
            }
        });
    }
    m_grid->setSubjects(subjects);
}
