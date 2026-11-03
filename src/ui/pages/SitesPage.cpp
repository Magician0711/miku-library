#include "SitesPage.h"

#include "core/Catalog.h"
#include "net/HttpClient.h"
#include "net/Providers.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "ui/Overlays.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPointer>

SitesPage::SitesPage(Catalog *catalog, HttpClient *http, QWidget *parent)
    : QWidget(parent),m_catalog(catalog),m_http(http)
{
    auto *layout=new QVBoxLayout(this); layout->setContentsMargins(32,24,32,20); layout->setSpacing(18);
    auto *header=new QHBoxLayout; header->setSpacing(8);
    header->addWidget(pageHeading(QStringLiteral("资源站点"),QStringLiteral("管理搜索来源，让每次检索更有方向。")),1);
    auto *importButton=new QPushButton(QStringLiteral("导入配置"),this);
    auto *exportButton=new QPushButton(QStringLiteral("导出配置"),this);
    connect(importButton,&QPushButton::clicked,this,&SitesPage::importRequested);
    connect(exportButton,&QPushButton::clicked,this,&SitesPage::exportRequested);
    header->addWidget(importButton); header->addWidget(exportButton); layout->addLayout(header);
    auto *bar=new QHBoxLayout; bar->setSpacing(10);
    m_filter=new QLineEdit(this); m_filter->setPlaceholderText(QStringLiteral("筛选站点名称、类别或语言"));
    m_filter->setClearButtonEnabled(true); m_filter->setMaximumWidth(360); bar->addWidget(m_filter);
    bar->addStretch();
    auto *test=new QPushButton(QStringLiteral("测试连接"),this);
    auto *toggle=new QPushButton(QStringLiteral("启用 / 禁用"),this); toggle->setObjectName("Primary");
    connect(test,&QPushButton::clicked,this,&SitesPage::testSelected);
    connect(toggle,&QPushButton::clicked,this,&SitesPage::toggleSelected);
    bar->addWidget(test); bar->addWidget(toggle); layout->addLayout(bar);
    m_status=label(QString(),"Faint",false,this); layout->addWidget(m_status);
    m_table=new QTableWidget(this); m_table->setColumnCount(8);
    m_table->setHorizontalHeaderLabels({QStringLiteral("启用"),QStringLiteral("名称"),QStringLiteral("类别"),
        QStringLiteral("语言"),QStringLiteral("适配器"),QStringLiteral("RSS"),QStringLiteral("搜索"),QStringLiteral("连接状态")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows); m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers); m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
    m_table->setColumnWidth(0,54); m_table->setColumnWidth(3,55); m_table->setColumnWidth(5,50); m_table->setColumnWidth(6,60);
    prepareTable(m_table); layout->addWidget(m_table,1);
    connect(m_filter,&QLineEdit::textChanged,this,&SitesPage::filterRows);
    connect(m_table,&QTableWidget::itemSelectionChanged,this,[this,test,toggle] {
        bool selected=selectedRow()>=0; test->setEnabled(selected); toggle->setEnabled(selected);
    });
    test->setEnabled(false); toggle->setEnabled(false);
    connect(m_catalog,&Catalog::catalogChanged,this,&SitesPage::refresh); refresh();
}

void SitesPage::refresh()
{
    const auto sites = m_catalog->sites();
    m_table->setRowCount(sites.size());
    int active = 0;
    for (int row = 0; row < sites.size(); ++row) {
        const auto &site = sites.at(row);
        if (site.isSearchable())
            ++active;
        auto set = [&](int column, const QString &text) {
            auto *item = new QTableWidgetItem(text);
            item->setData(Qt::UserRole, site.id);
            m_table->setItem(row, column, item);
        };
        set(0, site.adapter==AdapterType::None?QStringLiteral("网页"):site.enabled ? QStringLiteral("✓") : QString());
        set(1, site.name);
        set(2, site.category);
        set(3, site.language);
        set(4, site.adapterName());
        set(5, site.rssFlag ? QStringLiteral("是") : QString());
        set(6, site.searchAvailable ? QStringLiteral("✓") : QStringLiteral("—"));
        set(7, site.lastError.isEmpty() ? (site.lastSuccess.isValid() ? QStringLiteral("OK") : QStringLiteral("未测试"))
                                        : site.lastError);
    }
    m_status->setText(QStringLiteral("%1 个目录条目 · %2 个已启用搜索源").arg(sites.size()).arg(active));
    filterRows();
}

int SitesPage::selectedRow() const
{
    const auto rows = m_table->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

void SitesPage::toggleSelected()
{
    const int row = selectedRow();
    if (row < 0)
        return;
    const QString id = m_table->item(row, 1)->data(Qt::UserRole).toString();
    if (auto *site = m_catalog->get(id)) {
        QString error;if(!m_catalog->setEnabled(id,!site->enabled,&error))Ui::message(this,QStringLiteral("站点设置"),error,Ui::Notice::Warning);
    }
    refresh();
}

void SitesPage::testSelected()
{
    const int row = selectedRow();
    if (row < 0)
        return;
    const QString id = m_table->item(row, 1)->data(Qt::UserRole).toString();
    auto *site = m_catalog->get(id);
    if (!site)
        return;

    auto provider = std::shared_ptr<SearchProvider>(makeProvider(*site, m_http).release());
    if (!provider) {
        site->lastError = QStringLiteral("无适配器");
        site->searchAvailable = false;
        refresh();
        return;
    }
    const QString siteId = site->id;
    const QString siteName = site->name;
    QPointer<SitesPage> self(this);
    provider->testConnection([this,self,siteId,provider](bool ok, QString message) {
        if(!self)return;
        if (auto *target = m_catalog->get(siteId)) {
            target->searchAvailable = ok;
            target->lastError = ok ? QString() : message;
            if (ok)
                target->lastSuccess = QDateTime::currentDateTimeUtc();
        }
        refresh();
    });
    m_status->setText(QStringLiteral("正在测试 %1…").arg(siteName));
}

void SitesPage::filterRows()
{
    const QString query=m_filter->text().trimmed();
    for(int row=0;row<m_table->rowCount();++row) {
        QString text;
        for(int col=0;col<m_table->columnCount();++col)
            if(auto *item=m_table->item(row,col)) text+=item->text()+" ";
        m_table->setRowHidden(row,!query.isEmpty() && !text.contains(query,Qt::CaseInsensitive));
    }
}
