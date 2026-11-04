// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "SitesPage.h"

#include "core/Catalog.h"
#include "net/HttpClient.h"
#include "net/Providers.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "ui/Controls.h"
#include "ui/Overlays.h"

#include <QAction>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {
enum Column { EnabledColumn, NameColumn, CategoryColumn, LanguageColumn, AdapterColumn, StatusColumn, ColumnCount };
enum Scope { AllSites, Searchable, Directory };
constexpr int kIdRole = Qt::UserRole;
constexpr int kSearchableRole = Qt::UserRole + 1;
}

SitesPage::SitesPage(Catalog *catalog, HttpClient *http, QWidget *parent)
    : QWidget(parent),m_catalog(catalog),m_http(http)
{
    auto *layout=pageLayout(this);
    auto header=pageHeader(QStringLiteral("资源站点"),this);
    m_status=header.meta;
    auto *importButton=new QPushButton(QStringLiteral("导入配置"),this); importButton->setIcon(Theme::icon("import"));
    auto *exportButton=new QPushButton(QStringLiteral("导出配置"),this); exportButton->setIcon(Theme::icon("export"));
    connect(importButton,&QPushButton::clicked,this,&SitesPage::importRequested);
    connect(exportButton,&QPushButton::clicked,this,&SitesPage::exportRequested);
    header.actions->addWidget(importButton); header.actions->addWidget(exportButton);
    layout->addWidget(header.widget);

    auto *bar=new QHBoxLayout; bar->setSpacing(10);
    m_filter=new QLineEdit(this); m_filter->setObjectName("InlineFilter");
    m_filter->setPlaceholderText(QStringLiteral("筛选名称、类别或语言"));
    m_filter->addAction(Theme::icon("search",&Theme::Palette::text3),QLineEdit::LeadingPosition);
    m_filter->setClearButtonEnabled(true); m_filter->setMaximumWidth(320); bar->addWidget(m_filter,1);
    m_scope=new SelectBox(this);
    m_scope->addItem(QStringLiteral("全部站点"),AllSites);
    m_scope->addItem(QStringLiteral("可搜索来源"),Searchable);
    m_scope->addItem(QStringLiteral("网页目录"),Directory);
    m_scope->setFixedWidth(150); bar->addWidget(m_scope);
    bar->addStretch();
    bar->addWidget(label(QStringLiteral("勾选即启用搜索 · 双击测试连接"),"Faint",false,this));
    m_test=new QPushButton(QStringLiteral("测试连接"),this); m_test->setIcon(Theme::icon("pulse"));
    connect(m_test,&QPushButton::clicked,this,&SitesPage::testSelected);
    bar->addWidget(m_test); layout->addLayout(bar);

    m_table=new QTableWidget(this); m_table->setColumnCount(ColumnCount);
    m_table->setHorizontalHeaderLabels({QStringLiteral("启用"),QStringLiteral("名称"),QStringLiteral("类别"),
        QStringLiteral("语言"),QStringLiteral("适配器"),QStringLiteral("连接状态")});
    prepareTable(m_table);
    m_table->verticalHeader()->setDefaultSectionSize(44);
    m_table->horizontalHeader()->setSectionResizeMode(NameColumn,QHeaderView::Stretch);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->setColumnWidth(EnabledColumn,64); m_table->setColumnWidth(CategoryColumn,120);
    m_table->setColumnWidth(LanguageColumn,80); m_table->setColumnWidth(AdapterColumn,130); m_table->setColumnWidth(StatusColumn,220);
    layout->addWidget(m_table,1);

    connect(m_filter,&QLineEdit::textChanged,this,&SitesPage::filterRows);
    connect(m_scope,qOverload<int>(&QComboBox::currentIndexChanged),this,&SitesPage::filterRows);
    connect(m_table,&QTableWidget::itemSelectionChanged,this,[this]{m_test->setEnabled(selectedRow()>=0);});
    connect(m_table,&QTableWidget::itemDoubleClicked,this,[this](QTableWidgetItem *item){if(item->column()!=EnabledColumn)testSelected();});
    connect(m_table,&QTableWidget::itemChanged,this,[this](QTableWidgetItem *item) {
        if(item->column()!=EnabledColumn||!(item->flags()&Qt::ItemIsUserCheckable))return;
        const QString id=item->data(kIdRole).toString();const bool enabled=item->checkState()==Qt::Checked;
        // Catalog changes trigger a table refresh; leave the itemChanged handler first.
        QTimer::singleShot(0,this,[this,id,enabled]{
            QString error;
            if(!m_catalog->setEnabled(id,enabled,&error)){Ui::message(this,QStringLiteral("站点设置"),error,Ui::Notice::Warning);refresh();}
        });
    });
    m_test->setEnabled(false);
    connect(m_catalog,&Catalog::catalogChanged,this,&SitesPage::refresh);
    // Item colours are baked into the model, so repaint them with the new palette.
    connect(Theme::notifier(),&Theme::Notifier::changed,this,&SitesPage::refresh);
    refresh();
}

void SitesPage::refresh()
{
    const auto sites = m_catalog->sites();
    QSignalBlocker blocker(m_table);
    m_table->setRowCount(sites.size());
    int active = 0;
    for (int row = 0; row < sites.size(); ++row) {
        const auto &site = sites.at(row);
        const bool searchable = site.adapter != AdapterType::None;
        if (site.isSearchable())
            ++active;
        auto set = [&](int column, const QString &text) {
            auto *item = m_table->item(row, column);
            if (!item) { item = new QTableWidgetItem; m_table->setItem(row, column, item); }
            item->setText(text);
            item->setData(kIdRole, site.id);
            item->setData(kSearchableRole, searchable);
            item->setData(Qt::ForegroundRole, QVariant());
            return item;
        };
        auto *enabled = set(EnabledColumn, searchable ? QString() : QStringLiteral("网页"));
        if (searchable) {
            enabled->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
            enabled->setCheckState(site.enabled ? Qt::Checked : Qt::Unchecked);
            enabled->setToolTip(QStringLiteral("参与番剧资源搜索"));
        } else {
            enabled->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            enabled->setData(Qt::CheckStateRole, QVariant());
            enabled->setForeground(QColor(Theme::c().text3));
            enabled->setToolTip(QStringLiteral("网页目录，没有搜索适配器"));
        }
        set(NameColumn, site.name)->setToolTip(site.url);
        set(CategoryColumn, site.category)->setForeground(QColor(Theme::c().text2));
        set(LanguageColumn, site.language)->setForeground(QColor(Theme::c().text2));
        set(AdapterColumn, searchable ? site.adapterName() : QStringLiteral("—"))->setForeground(QColor(Theme::c().text2));
        QString status = QStringLiteral("未测试"); QColor color = Theme::c().text3;
        if (!site.lastError.isEmpty()) { status = site.lastError; color = Theme::c().err; }
        else if (site.lastSuccess.isValid()) { status = QStringLiteral("连接正常"); color = Theme::c().ok; }
        auto *state = set(StatusColumn, (site.lastError.isEmpty() && site.lastSuccess.isValid() ? QStringLiteral("● ") : QString()) + status);
        state->setForeground(color); state->setToolTip(status);
    }
    m_status->setText(QStringLiteral("%1 个条目 · %2 个搜索源已启用").arg(sites.size()).arg(active));
    filterRows();
}

int SitesPage::selectedRow() const
{
    const auto rows = m_table->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

void SitesPage::testSelected()
{
    const int row = selectedRow();
    if (row < 0)
        return;
    const QString id = m_table->item(row, NameColumn)->data(kIdRole).toString();
    auto *site = m_catalog->get(id);
    if (!site)
        return;

    auto provider = std::shared_ptr<SearchProvider>(makeProvider(*site, m_http).release());
    if (!provider) {
        site->lastError = QStringLiteral("无搜索适配器");
        site->searchAvailable = false;
        refresh();
        return;
    }
    const QString siteId = site->id;
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
    if (auto *state = m_table->item(row, StatusColumn)) {
        state->setText(QStringLiteral("正在测试…"));
        state->setForeground(QColor(Theme::c().accentText));
    }
}

void SitesPage::filterRows()
{
    const QString query=m_filter->text().trimmed();
    const int scope=m_scope->currentData().toInt();
    for(int row=0;row<m_table->rowCount();++row) {
        const bool searchable=m_table->item(row,NameColumn)->data(kSearchableRole).toBool();
        bool visible=scope==AllSites||(scope==Searchable)==searchable;
        if(visible&&!query.isEmpty()) {
            visible=false;
            for(int col=NameColumn;col<=AdapterColumn&&!visible;++col)
                if(auto *item=m_table->item(row,col)) visible=item->text().contains(query,Qt::CaseInsensitive);
        }
        m_table->setRowHidden(row,!visible);
    }
}
