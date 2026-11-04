#pragma once

#include <QString>
#include <QPoint>
#include <QStringList>

class QApplication;
class QWidget;

namespace Ui {
enum class Notice { Information, Warning, Error };
enum class Removal { Cancel, KeepFiles, DeleteFiles };

void installOverlays(QApplication *app);
void showTooltip(QWidget *owner, const QString &text, const QPoint &globalPosition);
void hideTooltip();
void message(QWidget *parent, const QString &title, const QString &text,
             Notice notice = Notice::Information);
Removal confirmRemoval(QWidget *parent);
QString chooseDirectory(QWidget *parent, const QString &title, const QString &path);
QString openExecutable(QWidget *parent,const QString &path);
QString openJson(QWidget *parent, const QString &title);
QString saveJson(QWidget *parent, const QString &title, const QString &fileName);
QString replaceDownloadDirectory(QWidget *parent, const QString &path, const QString &reason);
QString chooseVideo(QWidget *parent,const QStringList &paths);
// Version of the usage declaration; bump it when the wording changes materially
// so every user is asked to accept the new text once.
constexpr int kDisclaimerVersion = 1;
QStringList disclaimerItems();
// Short notes shown on the welcome page and in Settings → About.
QString playerNote();
QString freeSoftwareNote();
// First launch: quick start plus the declaration, returns false if declined.
// With review set it only shows the declaration for re-reading. Given downloadDir,
// the first page offers it as the download folder and writes back the choice.
bool firstRunGuide(QWidget *parent, bool review = false, QString *downloadDir = nullptr);
}
