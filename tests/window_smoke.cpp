// Anime Nexus - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "ui/FramelessWindow.h"
#include "ui/Theme.h"
#include <QApplication>
#include <QLabel>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QFile>
#include <QDebug>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#endif

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    Theme::applyTheme(&app);
    FramelessWindow window;
    window.setMinimumSize(640, 400);
    window.resize(1000, 650);
    window.setContextTitle(QStringLiteral("窗口操作验证"));
    auto *body = new QWidget;
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(window.titleBar());
    layout->addWidget(new QLabel(QStringLiteral("Frameless window smoke test")), 1);
    window.setCentralWidget(body);
    window.show();
    auto *maximize = window.findChild<QToolButton *>("WindowMaximize");
    auto *minimize = window.findChild<QToolButton *>("WindowMinimize");
    auto *close = window.findChild<QToolButton *>("WindowClose");
    if (!maximize || !minimize || !close) return 2;
    int failures = 0;
    QFile report(QCoreApplication::applicationDirPath() + "/window-smoke.log");
    if (!report.open(QIODevice::WriteOnly | QIODevice::Text)) return 3;
    auto check = [&failures, &report](bool value, const char *name) {
        report.write(QByteArray(name) + (value ? " PASS\n" : " FAIL\n"));
        report.flush();
        qInfo() << name << (value ? "PASS" : "FAIL");
        if (!value) ++failures;
    };
    QTimer::singleShot(200, &window, [&] {
        check(window.windowFlags().testFlag(Qt::FramelessWindowHint), "frameless");
#ifdef Q_OS_WIN
        const auto hwnd = reinterpret_cast<HWND>(window.winId());
        auto hit = [hwnd, &window](QPoint p) {
            POINT origin = {0, 0};
            ClientToScreen(hwnd, &origin);
            return SendMessage(hwnd, WM_NCHITTEST, 0,
                MAKELPARAM(origin.x + qRound(p.x() * window.devicePixelRatioF()),
                           origin.y + qRound(p.y() * window.devicePixelRatioF())));
        };
        check(hit(QPoint(2, 2)) == HTTOPLEFT, "corner resize");
        check(hit(QPoint(window.width() - 2, 200)) == HTRIGHT, "edge resize");
        check(hit(QPoint(400, 25)) == HTCAPTION, "title drag");
        const QPoint buttonPos = maximize->mapTo(&window, maximize->rect().center());
        check(hit(buttonPos) == HTCLIENT, "caption button interactive");
#endif
        maximize->click();
        QTimer::singleShot(200, &window, [&] {
            check(window.isMaximized(), "maximize");
            check(maximize->accessibleName() == QStringLiteral("还原"), "restore label");
            maximize->click();
            QTimer::singleShot(200, &window, [&] {
                check(!window.isMaximized(), "restore");
                check(window.size() == QSize(1000, 650), "restore geometry");
                minimize->click();
                QTimer::singleShot(200, &window, [&] {
                    check(window.isMinimized(), "minimize");
                    window.showNormal();
                    close->click();
                    app.exit(failures ? 1 : 0);
                });
            });
        });
    });
    return app.exec();
}
