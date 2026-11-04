#include "ui/Overlays.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "ui/ImageLoader.h"
#include "ui/Motion.h"

#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QEnterEvent>
#include <QFile>
#include <QFileDialog>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QPushButton>
#include <QScreen>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

int main(int argc,char **argv)
{
    QApplication app(argc,argv);Theme::applyTheme(&app);
    QWidget window;window.setWindowTitle(QStringLiteral("Nexus 界面审核预览"));window.resize(860,570);
    window.setAutoFillBackground(true);
    auto *root=new QVBoxLayout(&window);root->setContentsMargins(28,24,28,24);root->setSpacing(18);
    root->addWidget(new QLabel(QStringLiteral("统一浮层与卡片")));
    auto *cards=new QHBoxLayout;
    auto *card=new AnimeCard(nullptr,&window);AnimeSubject subject;subject.nameCn=QStringLiteral("海报卡片 · 无整卡外框");subject.year=2026;subject.rating=8.5;
    card->setSubject(subject);card->setCardWidth(200);cards->addWidget(card);cards->addStretch();root->addLayout(cards);
    auto *actions=new QHBoxLayout;
    auto *notice=new QPushButton(QStringLiteral("提示窗口"));
    auto *remove=new QPushButton(QStringLiteral("移除确认"));
    auto *folder=new QPushButton(QStringLiteral("选择目录"));
    auto *missing=new QPushButton(QStringLiteral("目录丢失提示"));
    auto *tip=new QPushButton(QStringLiteral("悬停查看提示"));tip->setToolTip(QStringLiteral("复制种子链接"));
    actions->addWidget(notice);actions->addWidget(remove);actions->addWidget(folder);actions->addWidget(missing);actions->addStretch();actions->addWidget(tip);root->addLayout(actions);root->addStretch();
    QObject::connect(notice,&QPushButton::clicked,&window,[&]{Ui::message(&window,QStringLiteral("下载失败"),QStringLiteral("无法保存种子文件，请检查下载目录后重试。"),Ui::Notice::Error);});
    QObject::connect(remove,&QPushButton::clicked,&window,[&]{Ui::confirmRemoval(&window);});
    QObject::connect(folder,&QPushButton::clicked,&window,[&]{Ui::chooseDirectory(&window,QStringLiteral("选择下载目录"),QDir::homePath());});
    QObject::connect(missing,&QPushButton::clicked,&window,[&]{Ui::replaceDownloadDirectory(&window,QStringLiteral("D:/Downloads/AnimeNexus"),QStringLiteral("保存的下载目录不存在，请更换目标下载目录。"));});
    QObject::connect(tip,&QPushButton::clicked,&window,[&]{Ui::showTooltip(tip,tip->toolTip(),tip->mapToGlobal(tip->rect().bottomRight()));});
    window.show();
    if(!app.arguments().contains("--test"))return app.exec();
    QFile report(QCoreApplication::applicationDirPath()+"/ui-audit-smoke.log");if(!report.open(QIODevice::WriteOnly|QIODevice::Text))return 2;
    int failures=0;
    auto check=[&](bool ok,const char *name){report.write(QByteArray(name)+(ok?" PASS\n":" FAIL\n"));report.flush();if(!ok)++failures;};
    QTemporaryDir sandbox;
    QTimer::singleShot(20000,&window,[&]{report.write("test timeout FAIL\n");report.flush();app.exit(1);});
    QTimer::singleShot(200,&window,[&] {
        const QColor outside=card->grab().toImage().pixelColor(1,1);
        card->setAttribute(Qt::WA_UnderMouse,true);card->update();app.processEvents();
        check(card->grab().toImage().pixelColor(1,1)==outside,"hover does not draw an outer card frame");
        int activated=0;QObject::connect(card,&AnimeCard::clicked,&window,[&]{++activated;});
        QKeyEvent enter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);QApplication::sendEvent(card,&enter);
        check(activated==1,"card keyboard activation remains available");
        const QPoint edge=window.mapToGlobal(QPoint(window.width()-4,window.height()-4));
        QHelpEvent help(QEvent::ToolTip,tip->rect().center(),edge);QApplication::sendEvent(tip,&help);app.processEvents();
        QWidget *bubble=nullptr;
        for(auto *w:app.topLevelWidgets())if(w->objectName()=="TooltipBubble")bubble=w;
        check(bubble&&bubble->isVisible(),"themed tooltip opens");
        check(bubble&&bubble->windowFlags().testFlag(Qt::FramelessWindowHint),"tooltip has no native frame");
        check(bubble&&bubble->screen()->availableGeometry().contains(bubble->geometry()),"tooltip stays inside screen");
        check(bubble&&QRect(window.mapToGlobal(QPoint()),window.size()).contains(bubble->geometry()),"edge tooltip stays inside application");
        check(bubble&&bubble->grab().toImage().pixelColor(0,0).alpha()==0,"tooltip corners are transparent");
        QEvent leave(QEvent::Leave);QApplication::sendEvent(tip,&leave);
        check(bubble&&!bubble->isVisible(),"tooltip closes when pointer leaves owner");
        QTimer::singleShot(0,&window,[&] {
            auto *dialog=qobject_cast<QDialog *>(app.activeModalWidget());
            check(dialog&&dialog->windowFlags().testFlag(Qt::FramelessWindowHint),"notice uses frameless surface");
            check(dialog&&dialog->screen()->availableGeometry().contains(dialog->geometry()),"notice fits screen");
            check(dialog&&dialog->height()<320,"short notice has no excessive empty space");
            if(dialog)dialog->accept();
        });
        Ui::message(&window,QStringLiteral("测试提示"),QStringLiteral("<资源名> & 路径以普通文字显示，错误说明支持复制。"),Ui::Notice::Warning);
        QTimer::singleShot(0,&window,[&]{if(auto *d=qobject_cast<QDialog *>(app.activeModalWidget()))d->reject();});
        check(Ui::confirmRemoval(&window)==Ui::Removal::Cancel,"closing removal dialog cancels action");
        QTimer::singleShot(0,&window,[&] {
            if(auto *d=app.activeModalWidget())for(auto *b:d->findChildren<QPushButton *>())
                if(b->text()==QStringLiteral("仅移除任务")){b->click();break;}
        });
        check(Ui::confirmRemoval(&window)==Ui::Removal::KeepFiles,"keep-files choice has explicit meaning");
        QTimer::singleShot(100,&window,[&] {
            auto *dialog=app.activeModalWidget();auto *picker=dialog?dialog->findChild<QFileDialog *>("FilePicker"):nullptr;
            check(picker&&picker->testOption(QFileDialog::DontUseNativeDialog),"directory picker uses themed widgets");
            int modern=0;
            if(picker)for(auto *button:picker->findChildren<QToolButton *>())if(button->property("pickerTool").toBool())++modern;
            check(modern==6,"file toolbar uses six modern line-icon buttons");
            if(picker){picker->setDirectory(sandbox.path());static_cast<QDialog *>(picker)->accept();}
            else if(auto *d=qobject_cast<QDialog *>(dialog))d->reject();
        });
        const QString chosen=Ui::chooseDirectory(&window,QStringLiteral("目录选择测试"),sandbox.path());
        check(QDir::cleanPath(chosen)==QDir::cleanPath(sandbox.path()),"directory picker returns chosen path");
        const QString exe=QDir(sandbox.path()).filePath(QStringLiteral("自定义 下载器.exe"));QFile executable(exe);executable.open(QIODevice::WriteOnly);executable.write("picker fixture");executable.close();
        QTimer::singleShot(100,&window,[&]{
            auto *dialog=app.activeModalWidget();auto *picker=dialog?dialog->findChild<QFileDialog*>("FilePicker"):nullptr;
            check(picker&&picker->testOption(QFileDialog::DontUseNativeDialog)&&dialog->windowFlags().testFlag(Qt::FramelessWindowHint),"program picker matches other frameless file dialogs");
            check(picker&&picker->nameFilters().join(';').contains("*.exe"),"program picker filters executable files");
            if(picker){picker->selectFile(exe);static_cast<QDialog*>(picker)->accept();}else if(auto *d=qobject_cast<QDialog*>(dialog))d->reject();
        });
        check(Ui::openExecutable(&window,exe)==exe,"program picker preserves Unicode and space paths");
        QTimer::singleShot(100,&window,[&]{if(auto *d=qobject_cast<QDialog *>(app.activeModalWidget()))d->reject();});
        check(Ui::openJson(&window,QStringLiteral("导入测试")).isEmpty(),"cancelled file picker returns no path");
        QTimer::singleShot(100,&window,[&] {
            auto *d=app.activeModalWidget();auto *p=d?d->findChild<QFileDialog *>("FilePicker"):nullptr;
            if(p){p->setDirectory(sandbox.path());p->selectFile("export");static_cast<QDialog *>(p)->accept();}
        });
        const QString exported=Ui::saveJson(&window,QStringLiteral("导出测试"),"export.json");
        check(exported==QDir(sandbox.path()).filePath("export.json"),"JSON save picker preserves default extension");
        QFile existing(exported);existing.open(QIODevice::WriteOnly);existing.write("original");existing.close();
        QTimer::singleShot(100,&window,[&] {
            auto *d=app.activeModalWidget();auto *p=d?d->findChild<QFileDialog *>("FilePicker"):nullptr;
            if(p){p->selectFile(exported);static_cast<QDialog *>(p)->accept();}
        });
        QTimer::singleShot(250,&window,[&] {
            auto *d=qobject_cast<QDialog *>(app.activeModalWidget());
            check(d&&d->windowTitle()==QStringLiteral("替换已有配置？"),"overwrite confirmation uses themed surface");
            if(d)d->reject();
        });
        check(Ui::saveJson(&window,QStringLiteral("覆盖测试"),exported).isEmpty(),"cancelled overwrite does not return target path");
        QTimer::singleShot(0,&window,[&] {
            if(auto *d=app.activeModalWidget())for(auto *b:d->findChildren<QPushButton *>())
                if(b->text()==QStringLiteral("选择新目录")){b->click();break;}
        });
        QTimer::singleShot(100,&window,[&] {
            auto *d=app.activeModalWidget();auto *p=d?d->findChild<QFileDialog *>("FilePicker"):nullptr;
            if(p){p->setDirectory(sandbox.path());static_cast<QDialog *>(p)->accept();}
        });
        check(Ui::replaceDownloadDirectory(&window,sandbox.path()+"/missing",QStringLiteral("目录已不存在。"))==sandbox.path(),
            "missing-directory prompt returns replacement selection");
        QTimer::singleShot(0,&window,[&]{if(auto *d=qobject_cast<QDialog *>(app.activeModalWidget()))d->reject();});
        check(Ui::replaceDownloadDirectory(&window,"missing",QStringLiteral("目录已不存在。")).isEmpty(),
            "closing missing-directory prompt does not silently choose a path");
        {
            // Welcome page: the download folder row shows the proposal and takes a new folder.
            QString folder=sandbox.filePath("AnimeNexus");
            const QString picked=sandbox.path();
            bool shown=false,changed=false;
            QTimer::singleShot(150,&window,[&] {
                auto *d=qobject_cast<QDialog *>(app.activeModalWidget());
                QPushButton *change=nullptr;
                if(d)for(auto *b:d->findChildren<QPushButton *>())if(b->text()==QStringLiteral("更改…"))change=b;
                auto *where=change?change->parentWidget()->findChild<QLabel *>("GuideNoteText"):nullptr;
                shown=where&&where->toolTip()==QDir::toNativeSeparators(folder);
                if(change) {
                    QTimer::singleShot(150,&window,[&] {
                        auto *m=app.activeModalWidget();auto *p=m?m->findChild<QFileDialog *>("FilePicker"):nullptr;
                        if(p){p->setDirectory(picked);static_cast<QDialog *>(p)->accept();}
                    });
                    change->click();   // returns once the folder picker closes
                    changed=where&&where->toolTip()==QDir::toNativeSeparators(picked);
                }
                if(d)d->reject();
            });
            const bool accepted=Ui::firstRunGuide(&window,false,&folder);
            check(shown,"welcome page shows the proposed download folder");
            check(changed&&QDir(folder)==QDir(picked),"welcome page download folder can be changed");
            check(!accepted,"closing the welcome page still declines");
        }
        {
            // Crossfade: an opaque layer blends the old still over the new one, then leaves.
            auto *panel=new QWidget(&window);panel->setGeometry(420,300,120,80);panel->setAutoFillBackground(true);
            QPalette colors=panel->palette();colors.setColor(QPalette::Window,Qt::red);panel->setPalette(colors);panel->show();
            auto pixel=[&]{return window.grab(QRect(panel->geometry().center(),QSize(1,1))).toImage().pixelColor(0,0);};
            QElapsedTimer clock;clock.start();
            while(clock.elapsed()<100)QCoreApplication::processEvents(QEventLoop::AllEvents,10);
            Motion::crossfade(panel,Motion::Normal);
            colors.setColor(QPalette::Window,Qt::blue);panel->setPalette(colors);
            auto *fade=window.findChild<Motion::SnapshotFade *>(QString(),Qt::FindDirectChildrenOnly);
            check(!Motion::enabled()||(fade&&fade->testAttribute(Qt::WA_OpaquePaintEvent)),"crossfade layer is opaque so content underneath never repaints per frame");
            QColor mid;clock.restart();
            while(clock.elapsed()<1500&&window.findChild<Motion::SnapshotFade *>(QString(),Qt::FindDirectChildrenOnly)) {
                QCoreApplication::processEvents(QEventLoop::AllEvents,5);
                if(!mid.isValid()&&clock.elapsed()>=90)mid=pixel();
            }
            // The fade starts on its first paint, which a hidden test window never gets.
            const bool exposed=window.windowHandle()&&window.windowHandle()->isExposed();
            check(!Motion::enabled()||!exposed||(mid.red()>10&&mid.blue()>10),"crossfade shows a blend of old and new content mid-way");
            const QColor end=pixel();
            check(!window.findChild<Motion::SnapshotFade *>(QString(),Qt::FindDirectChildrenOnly)&&end.blue()>200&&end.red()<40,"crossfade removes itself and leaves the new content");
            delete panel;
        }
        {
            // Switching a stacked page must not show the new page before the fade:
            // QStackedWidget raises the new page above its own children.
            auto *stack=new QStackedWidget(&window);stack->setGeometry(420,300,120,80);
            for(const QColor color:{QColor(Qt::red),QColor(Qt::blue)}) {
                auto *page=new QWidget;page->setAutoFillBackground(true);
                QPalette colors=page->palette();colors.setColor(QPalette::Window,color);page->setPalette(colors);
                stack->addWidget(page);
            }
            stack->show();
            QElapsedTimer clock;clock.start();
            while(clock.elapsed()<100)QCoreApplication::processEvents(QEventLoop::AllEvents,10);
            Motion::crossfade(stack,Motion::Normal);
            stack->setCurrentIndex(1);
            const QColor first=window.grab(QRect(stack->geometry().center(),QSize(1,1))).toImage().pixelColor(0,0);
            check(!Motion::enabled()||(first.red()>200&&first.blue()<40),"page switch starts from the old page, not a flash of the new one");
            clock.restart();
            while(clock.elapsed()<1500&&window.findChild<Motion::SnapshotFade *>(QString(),Qt::FindDirectChildrenOnly))
                QCoreApplication::processEvents(QEventLoop::AllEvents,5);
            const QColor end=window.grab(QRect(stack->geometry().center(),QSize(1,1))).toImage().pixelColor(0,0);
            check(end.blue()>200&&end.red()<40,"page switch ends on the new page");
            delete stack;
        }
        {
            const QString covers=sandbox.filePath("covers");QDir().mkpath(covers);
            QFile stale(covers+"/old.img");stale.open(QIODevice::WriteOnly);stale.write("x");stale.close();
            auto *loader=new ImageLoader(covers);bool called=false,empty=false;
            loader->request(QStringLiteral("http://example.com/a.jpg"),[&](QImage image){called=true;empty=image.isNull();});
            QElapsedTimer clock;clock.start();
            while((!called||QFile::exists(covers+"/old.img"))&&clock.elapsed()<3000)QCoreApplication::processEvents(QEventLoop::AllEvents,20);
            check(called&&empty,"cover loader answers non-https urls with an empty image");
            check(!QFile::exists(covers+"/old.img")&&QDir(covers+"/400x600").exists(),"cover loader removes legacy cache files and keeps sized folder");
            loader->request(QStringLiteral("http://example.com/b.jpg"),[](QImage){});delete loader;
            check(true,"cover loader destructor waits for pending workers");
        }
        app.exit(failures?1:0);
    });
    return app.exec();
}
