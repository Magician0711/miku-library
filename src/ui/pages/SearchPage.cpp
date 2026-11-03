#include "SearchPage.h"

#include "ui/ImageLoader.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "ui/Controls.h"
#include "net/BangumiClient.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>

namespace {
const QString kIdleStatus = QStringLiteral("支持原名与别名检索");
}

SearchPage::SearchPage(BangumiClient *bangumi, ImageLoader *images, QWidget *parent)
    : QWidget(parent), m_bangumi(bangumi), m_images(images)
{
    auto *layout=pageLayout(this);
    // The status line is the first "Faint" child; the search input tests rely on it.
    m_status=label(kIdleStatus,"Faint",false,this);
    layout->addWidget(pageHeader(QStringLiteral("搜索番剧"),this).widget);

    m_panel=new QFrame(this); m_panel->setObjectName("SearchPanel");
    auto *bar=new QHBoxLayout(m_panel); bar->setContentsMargins(16,6,6,6); bar->setSpacing(10);
    auto *searchIcon=new IconLabel("search",&Theme::Palette::text3,20,m_panel); searchIcon->setFixedSize(22,22);
    bar->addWidget(searchIcon);
    m_edit=new SearchInput(m_panel); m_edit->setObjectName("SearchBox");
    m_edit->setPlaceholderText(QStringLiteral("输入中文、日文或英文番名，按回车搜索"));
    m_edit->setClearButtonEnabled(true); m_edit->setAccessibleName(QStringLiteral("番剧名称"));
    m_edit->installEventFilter(this);
    connect(m_edit,&QLineEdit::returnPressed,this,&SearchPage::search); bar->addWidget(m_edit,1);
    auto *go=new QPushButton(QStringLiteral("搜索"),m_panel); go->setObjectName("Primary");
    go->ensurePolished(); go->setFixedSize(84,40); go->setCursor(Qt::PointingHandCursor);
    connect(go,&QPushButton::clicked,this,&SearchPage::search); bar->addWidget(go);
    layout->addWidget(m_panel);

    auto *meta=new QHBoxLayout;
    meta->addWidget(m_status); meta->addStretch();
    meta->addWidget(label(QStringLiteral("Ctrl + K 随时打开搜索"),"Faint"));
    layout->addLayout(meta);

    m_grid=new CardGrid(m_images); connect(m_grid,&CardGrid::cardClicked,this,&SearchPage::animeSelected);
    m_empty=new EmptyState("search",QStringLiteral("想看些什么？"),QStringLiteral("输入番名开始搜索，进入作品后可查看各站资源。"),this);
    layout->addWidget(overlayStack(scrollArea(m_grid),m_empty,this),1);
    connect(m_edit,&QLineEdit::textChanged,this,[this](const QString &text) {
        if(text.trimmed().isEmpty()&&!m_edit->composing())resetSearch();
    });
    connect(m_edit,&SearchInput::compositionChanged,this,[this](bool composing) {
        if(!composing&&m_edit->text().trimmed().isEmpty())resetSearch();
    });
}

bool SearchPage::eventFilter(QObject *object, QEvent *event)
{
    if(object==m_edit && (event->type()==QEvent::FocusIn || event->type()==QEvent::FocusOut)) {
        m_panel->setProperty("focused",event->type()==QEvent::FocusIn);
        m_panel->style()->unpolish(m_panel); m_panel->style()->polish(m_panel);
    }
    return QWidget::eventFilter(object,event);
}

void SearchPage::setQuery(const QString &query)
{
    m_edit->setText(query);
    search();
}

void SearchPage::focusInput()
{
    m_edit->setFocus();
    m_edit->selectAll();
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
    m_status->setText(kIdleStatus);
    m_empty->setText(QStringLiteral("想看些什么？"),QStringLiteral("输入番名开始搜索，进入作品后可查看各站资源。"));
    m_empty->show();
}
