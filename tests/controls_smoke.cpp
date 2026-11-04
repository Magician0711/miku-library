// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "ui/Controls.h"
#include "ui/Theme.h"
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QFile>
#include <QFormLayout>
#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QSpinBox>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QAbstractItemView>

int main(int argc,char **argv)
{
    QApplication app(argc,argv); Theme::applyTheme(&app);
    QWidget window; window.setWindowTitle(QStringLiteral("Nexus 控件预览")); window.resize(760,560);
    auto *root=new QVBoxLayout(&window); root->setContentsMargins(30,26,30,26); root->setSpacing(18);
    auto *title=new QLabel(QStringLiteral("控件样式预览")); title->setObjectName("H2"); root->addWidget(title);
    auto *form=new QFormLayout; form->setSpacing(16);
    SelectBox seasons; seasons.addItem(QStringLiteral("全部"),-1); seasons.addItem(QStringLiteral("第 1 季"),1); seasons.addItem(QStringLiteral("第 2 季"),2);
    seasons.setAccessibleName(QStringLiteral("放送季度")); form->addRow(QStringLiteral("放送季度"),&seasons);
    SelectBox languages; languages.addItems({QStringLiteral("全部语言"),QStringLiteral("简体中文"),QStringLiteral("繁体中文"),QStringLiteral("日文"),QStringLiteral("英文")}); form->addRow(QStringLiteral("字幕语言"),&languages);
    QSpinBox port; port.setRange(0,65535); port.setValue(6881); form->addRow(QStringLiteral("监听端口"),&port);
    QCheckBox option(QStringLiteral("启用此选项")); option.setChecked(true); form->addRow(QString(),&option);
    root->addLayout(form);
    QTableWidget table(2,2); table.setHorizontalHeaderLabels({QStringLiteral("样式示例"),QStringLiteral("状态")});
    table.setItem(0,0,new QTableWidgetItem(QStringLiteral("选中行"))); table.setItem(0,1,new QTableWidgetItem(QStringLiteral("就绪")));
    table.setItem(1,0,new QTableWidgetItem(QStringLiteral("普通行"))); table.setItem(1,1,new QTableWidgetItem(QStringLiteral("等待")));
    table.setItemDelegate(new SoftFocusDelegate(&table)); root->addWidget(&table,1);
    auto *buttons=new QHBoxLayout; auto *primary=new QPushButton(QStringLiteral("主要操作")); primary->setObjectName("Primary"); buttons->addStretch(); buttons->addWidget(primary); buttons->addWidget(new QPushButton(QStringLiteral("普通操作"))); root->addLayout(buttons);
    window.show();
    if(!app.arguments().contains("--test")) return app.exec();
    QFile report(QCoreApplication::applicationDirPath()+"/controls-smoke.log");
    if(!report.open(QIODevice::WriteOnly|QIODevice::Text))return 2;
    int failures=0;
    auto check=[&](bool value,const char *name){report.write(QByteArray(name)+(value?" PASS\n":" FAIL\n"));report.flush();if(!value)++failures;};
    window.raise(); window.activateWindow();
    QTimer::singleShot(300,&window,[&]{
        seasons.showPopup(); app.processEvents();
        auto *popup=seasons.view()->parentWidget();
        check(popup->isVisible(),"popup opens");
        check(popup->windowFlags().testFlag(Qt::FramelessWindowHint),"popup has no native frame");
        check(popup->geometry().height()>=3*38,"comfortable option rows");
        check(popup->screen()->availableGeometry().contains(popup->geometry()),"popup inside available screen");
        QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier); QApplication::sendEvent(seasons.view(),&escape); app.processEvents();
        check(!popup->isVisible(),"escape dismisses popup");
        check(seasons.currentData().toInt()==-1,"escape preserves selection");
        seasons.setCurrentIndex(1); seasons.showPopup(); app.processEvents();
        QKeyEvent down(QEvent::KeyPress,Qt::Key_Down,Qt::NoModifier); QApplication::sendEvent(seasons.view(),&down);
        QKeyEvent enter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier); QApplication::sendEvent(seasons.view(),&enter); app.processEvents();
        check(seasons.currentData().toInt()==2,"keyboard selects next option");
        check(!popup->isVisible(),"selection closes popup");
        seasons.hidePopup();
        DownloaderSettings backend;
        QTimer::singleShot(80,&window,[&]{
            QDialog *chooser=nullptr;for(auto *w:app.topLevelWidgets())if(w->objectName()=="DownloaderPopup")chooser=qobject_cast<QDialog*>(w);
            check(chooser&&chooser->isVisible()&&chooser->windowFlags().testFlag(Qt::FramelessWindowHint),"downloader chooser uses the frameless themed surface");
            if(!chooser){app.exit(1);return;}
            int rows=0;for(auto *b:chooser->findChildren<QPushButton*>())if(b->property("downloaderType").isValid()){++rows;check(b->height()>=54,"downloader option has room for title and description");}
            check(rows==6,"all downloader types remain reachable");
            check(chooser->screen()->availableGeometry().contains(chooser->geometry()),"downloader chooser stays inside screen");
            auto *current=chooser->findChild<QLabel*>("DownloaderCurrent");check(current&&current->text()==QStringLiteral("当前"),"current downloader is visibly marked");
            chooser->grab().save(QCoreApplication::applicationDirPath()+"/downloader-popup-preview.png");
            QKeyEvent esc(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QApplication::sendEvent(chooser,&esc);
        });
        check(Ui::chooseDownloader(primary,backend).isEmpty()&&backend.selected=="builtin","dismissing chooser preserves active downloader");
        QTimer::singleShot(50,&window,[&]{
            QDialog *chooser=nullptr;for(auto *w:app.topLevelWidgets())if(w->objectName()=="DownloaderPopup")chooser=qobject_cast<QDialog*>(w);
            if(!chooser){app.exit(1);return;}
            auto *focus=chooser->focusWidget();QKeyEvent next(QEvent::KeyPress,Qt::Key_Down,Qt::NoModifier);QApplication::sendEvent(focus,&next);
            QKeyEvent pick(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);QApplication::sendEvent(chooser->focusWidget(),&pick);
        });
        check(Ui::chooseDownloader(&seasons,backend)=="qbittorrent","keyboard navigation selects downloader");
        app.exit(failures?1:0);
    });
    return app.exec();
}
