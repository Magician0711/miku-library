#include "ui/Controls.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "ui/pages/SearchPage.h"

#include <QApplication>
#include <QFile>
#include <QEventLoop>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QTimer>

int main(int argc,char **argv)
{
    QApplication app(argc,argv);Theme::applyTheme(&app);
    SearchPage page(nullptr,nullptr);page.resize(1000,700);page.show();
    auto *input=static_cast<SearchInput *>(page.findChild<QLineEdit *>("SearchBox"));
    if(!input)return 2;
    input->disconnect(&page);
    QFile report(QCoreApplication::applicationDirPath()+"/search-input-smoke.log");
    if(!report.open(QIODevice::WriteOnly|QIODevice::Text))return 3;
    int failures=0;
    auto check=[&](bool ok,const char *name){report.write(QByteArray(name)+(ok?" PASS\n":" FAIL\n"));report.flush();if(!ok)++failures;};
    QStringList submitted;
    QObject::connect(input,&QLineEdit::returnPressed,&page,[&]{submitted.append(input->text());});
    auto preedit=[&](const QString &text){QInputMethodEvent event(text,{});QApplication::sendEvent(input,&event);};
    auto commit=[&](const QString &text){QInputMethodEvent event;event.setCommitString(text);QApplication::sendEvent(input,&event);};
    auto key=[&](QEvent::Type type,int value,bool repeat=false){QKeyEvent event(type,value,Qt::NoModifier,QString(),repeat);QApplication::sendEvent(input,&event);};
    QTimer::singleShot(100,&page,[&] {
        input->setFocus();preedit("chaodian");
        check(input->composing()&&input->text().isEmpty(),"pinyin preedit is not treated as submitted text");
        key(QEvent::ShortcutOverride,Qt::Key_Return);commit(QStringLiteral("超电磁炮"));
        key(QEvent::KeyPress,Qt::Key_Return);key(QEvent::KeyRelease,Qt::Key_Return);
        check(submitted.isEmpty(),"candidate confirmation Enter does not submit search");
        check(input->text()==QStringLiteral("超电磁炮")&&!input->composing(),"candidate commit preserves Chinese text");
        key(QEvent::KeyPress,Qt::Key_Return);key(QEvent::KeyRelease,Qt::Key_Return);
        check(submitted==QStringList{QStringLiteral("超电磁炮")},"next Enter submits complete committed query once");

        input->clear();preedit("fusong");commit(QStringLiteral("葬送的芙莉莲"));
        key(QEvent::KeyPress,Qt::Key_Return);key(QEvent::KeyRelease,Qt::Key_Return);
        check(submitted.size()==2&&submitted.last()==QStringLiteral("葬送的芙莉莲"),"Space-or-click candidate commit permits subsequent search");

        input->setText(QStringLiteral("某"));preedit("kexue");
        const int before=submitted.size();key(QEvent::KeyPress,Qt::Key_Return);
        check(submitted.size()==before,"active composition never submits partial query");
        commit(QStringLiteral("科学"));key(QEvent::KeyRelease,Qt::Key_Return);
        key(QEvent::KeyPress,Qt::Key_Return);key(QEvent::KeyRelease,Qt::Key_Return);
        check(submitted.last()==QStringLiteral("某科学"),"mixed committed text and candidate retain correct order");

        const int held=submitted.size();key(QEvent::KeyPress,Qt::Key_Return,true);
        key(QEvent::KeyRelease,Qt::Key_Return);
        check(submitted.size()==held,"held Enter does not repeat search submissions");

        input->selectAll();input->insert(QStringLiteral("とある / Railgun 2026"));
        check(input->text()==QStringLiteral("とある / Railgun 2026"),"selection replacement and mixed-language input remain intact");
        preedit("cancel");preedit(QString());
        check(!input->composing(),"cancelled composition releases composition state");
#ifdef Q_OS_WIN
        preedit("houxuan");key(QEvent::ShortcutOverride,Qt::Key_Return);commit(QStringLiteral("候选"));
        QEventLoop releaseWait;QTimer::singleShot(30,&releaseWait,&QEventLoop::quit);releaseWait.exec();
        const int afterFilteredRelease=submitted.size();
        key(QEvent::KeyPress,Qt::Key_Return);key(QEvent::KeyRelease,Qt::Key_Return);
        check(submitted.size()==afterFilteredRelease+1,"IME-filtered key release does not block next search");
#endif

        SearchPage resetPage(nullptr,nullptr);
        auto *resetInput=resetPage.findChild<QLineEdit *>("SearchBox");
        auto *grid=resetPage.findChild<CardGrid *>();
        resetInput->setText(QStringLiteral("旧搜索"));
        AnimeSubject subject;subject.nameCn=QStringLiteral("旧结果");grid->setSubjects({subject});
        resetInput->clear();
        check(grid->layout()->count()==0,"clearing input removes stale results");
        auto *status=resetPage.findChild<QLabel *>("Faint");
        check(status&&status->text()==QStringLiteral("支持原名与别名检索"),"clearing input restores initial search state");
        resetInput->setText(QStringLiteral("旧搜索"));grid->setSubjects({subject});
        QInputMethodEvent composing("pinyin",{});QApplication::sendEvent(resetInput,&composing);
        resetInput->clear();
        QInputMethodEvent cancelled;QApplication::sendEvent(resetInput,&cancelled);
        check(grid->layout()->count()==0,"cancelling empty composition also clears stale results");
        app.exit(failures?1:0);
    });
    return app.exec();
}
