#include "SearchPage.h"

#include "ui/ImageLoader.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "ui/Controls.h"
#include "net/BangumiClient.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QPointer>

SearchPage::SearchPage(BangumiClient *bangumi, ImageLoader *images, QWidget *parent)
    : QWidget(parent), m_bangumi(bangumi), m_images(images)
{
    auto *layout=new QVBoxLayout(this);
    layout->setContentsMargins(32,24,32,20); layout->setSpacing(18);
    layout->addWidget(pageHeading(QStringLiteral("搜索番剧"),QStringLiteral("从一个名字，找到想看的故事。")));
    auto *panel=new QFrame(this); panel->setObjectName("SearchPanel");
    auto *bar=new QHBoxLayout(panel); bar->setContentsMargins(16,8,8,8); bar->setSpacing(12);
    auto *searchIcon=new QLabel(panel); searchIcon->setPixmap(Theme::icon("search").pixmap(24,24));
    bar->addWidget(searchIcon);
    m_edit=new SearchInput(panel); m_edit->setObjectName("SearchBox");
    m_edit->setPlaceholderText(QStringLiteral("搜索中文、日文或英文番名"));
    m_edit->setClearButtonEnabled(true); m_edit->setAccessibleName(QStringLiteral("番剧名称"));
    connect(m_edit,&QLineEdit::returnPressed,this,&SearchPage::search); bar->addWidget(m_edit,1);
    auto *go=new QPushButton(QStringLiteral("搜索"),panel); go->setObjectName("Primary");
    go->setFixedSize(88,42); connect(go,&QPushButton::clicked,this,&SearchPage::search); bar->addWidget(go);
    layout->addWidget(panel);
    auto *meta=new QHBoxLayout; m_status=label(QStringLiteral("支持原名与别名检索"),"Faint",false,this);
    meta->addWidget(m_status); meta->addStretch(); meta->addWidget(label(QStringLiteral("Ctrl + K  快速搜索"),"Faint"));
    layout->addLayout(meta);
    auto *results=new QGridLayout; results->setContentsMargins(0,0,0,0);
    auto *scroll=new QScrollArea(this); scroll->setWidgetResizable(true); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_grid=new CardGrid(m_images,scroll); connect(m_grid,&CardGrid::cardClicked,this,&SearchPage::animeSelected);
    scroll->setWidget(m_grid); results->addWidget(scroll,0,0);
    m_empty=new EmptyState(Icons::SEARCH,QStringLiteral("想看些什么？"),QStringLiteral("输入番名开始搜索，进入作品后可查看各站资源。"),this);
    results->addWidget(m_empty,0,0); layout->addLayout(results,1);
    connect(m_edit,&QLineEdit::textChanged,this,[this](const QString &text) {
        if(text.trimmed().isEmpty()&&!m_edit->composing())resetSearch();
    });
    connect(m_edit,&SearchInput::compositionChanged,this,[this](bool composing) {
        if(!composing&&m_edit->text().trimmed().isEmpty())resetSearch();
    });
}

void SearchPage::setQuery(const QString &query)
{
    m_edit->setText(query);
    search();
}

void SearchPage::focusInput()
{
    m_edit->setFocus();
}

void SearchPage::search()
{
    if(m_edit->composing())return;
    const QString query = m_edit->text().trimmed();
    if (query.isEmpty()) {
        resetSearch();
        return;
    }

    m_grid->clear();
    m_status->setText(QStringLiteral("搜索中…"));
    m_empty->setText(QStringLiteral("搜索中…"), QStringLiteral("Bangumi 查询「%1」").arg(query));
    m_empty->show();

    const int generation = ++m_generation;
    QPointer<SearchPage> self(this);
    m_bangumi->search(query, 48, [this, self, generation](QVector<AnimeSubject> subjects, QString error) {
        if (!self || generation != m_generation)
            return;
        if (!error.isEmpty()) {
            m_status->setText(QStringLiteral("搜索失败：") + error);
            m_empty->setText(QStringLiteral("搜索失败"), error);
            return;
        }
        if (subjects.isEmpty()) {
            m_status->setText(QStringLiteral("未找到匹配的番剧"));
            m_empty->setText(QStringLiteral("未找到匹配的番剧"),
                             QStringLiteral("换个关键字或使用日文原名试试。"));
            return;
        }
        m_grid->setSubjects(subjects);
        m_status->setText(QStringLiteral("找到 %1 部番剧").arg(subjects.size()));
        m_empty->hide();
    });
}

void SearchPage::resetSearch()
{
    ++m_generation;
    m_grid->clear();
    m_status->setText(QStringLiteral("支持原名与别名检索"));
    m_empty->setText(QStringLiteral("想看些什么？"),QStringLiteral("输入番名开始搜索，进入作品后可查看各站资源。"));
    m_empty->show();
}
