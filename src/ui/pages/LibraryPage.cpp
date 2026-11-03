#include "LibraryPage.h"

#include "core/Database.h"
#include "core/LocalLibrary.h"
#include "ui/ImageLoader.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "net/BangumiClient.h"

#include <QHash>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFrame>

LibraryPage::LibraryPage(Database *db, BangumiClient *bangumi, ImageLoader *images, LocalLibrary *library, QWidget *parent)
    : QWidget(parent), m_db(db), m_bangumi(bangumi), m_images(images), m_library(library)
{
    connect(library,&LocalLibrary::changed,this,[this]{if(isVisible())refresh();});
    auto *layout=new QVBoxLayout(this);
    layout->setContentsMargins(32,24,32,20); layout->setSpacing(26);
    layout->addWidget(pageHeading(QStringLiteral("我的追番"),QStringLiteral("把喜欢的作品留在这里，随时接着看。")));
    auto *results=new QGridLayout; results->setContentsMargins(0,0,0,0);
    auto *scroll=new QScrollArea(this); scroll->setWidgetResizable(true); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_grid=new CardGrid(m_images,scroll); connect(m_grid,&CardGrid::cardClicked,this,&LibraryPage::animeSelected);
    scroll->setWidget(m_grid); results->addWidget(scroll,0,0);
    m_empty=new EmptyState(Icons::HEART,QStringLiteral("收藏室，等你添上第一部"),QStringLiteral("在作品详情中点击「追番」，就能在这里找到它。"),this);
    results->addWidget(m_empty,0,0); layout->addLayout(results,1);
}

void LibraryPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refresh();
}

void LibraryPage::refresh()
{
    auto ids = m_db->followedIds();
    QHash<int,AnimeSubject> local;
    for(const auto &subject:m_library->subjects()) {
        local.insert(subject.bangumiId,subject);
        if(!ids.contains(subject.bangumiId))ids.append(subject.bangumiId);
    }
    if (ids.isEmpty()) {
        m_grid->clear();
        m_empty->setText(QStringLiteral("还没有追番"),
                         QStringLiteral("在番剧详情页点击「追番」即可加入这里。"));
        m_empty->show();
        return;
    }
    m_empty->hide();

    QHash<int, AnimeSubject> loaded=local;
    for (int id : ids) {
        if (auto subject = m_db->getSubject(id))
            loaded.insert(id, *subject);
    }

    QVector<AnimeSubject> subjects;
    for (int id : ids) {
        auto it = loaded.find(id);
        if (it != loaded.end()) {
            subjects.push_back(it.value());
            continue;
        }
        m_bangumi->getSubject(id, [this, id](AnimeSubject subject, QString error) {
            if (error.isEmpty() && subject.bangumiId == id) {
                m_db->upsertSubject(subject);
                refresh();
            }
        });
    }
    m_grid->setSubjects(subjects);
}
