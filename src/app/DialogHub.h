// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QObject>
#include <QQueue>
#include <QVariantMap>

#include <functional>

// Questions and notices for the user. They appear one at a time inside the window;
// the answer arrives through a callback, so nothing blocks while one is open.
class DialogHub final : public QObject {
    Q_OBJECT
    // Empty while nothing is open. kind says which dialog to draw.
    Q_PROPERTY(QVariantMap current READ current NOTIFY currentChanged)
public:
    using Answer = std::function<void(const QString &choice, bool remember)>;
    using QObject::QObject;

    QVariantMap current() const { return m_current; }

    // notice: "info", "warning" or "error".
    void message(const QString &title, const QString &text, const QString &notice = QStringLiteral("info"));
    // With rejectIsDefault the safe answer is the highlighted one that Enter triggers.
    void ask(const QString &title, const QString &text, const QString &acceptText, const QString &rejectText,
             std::function<void(bool)> done, bool rejectIsDefault = false);
    // choice: "cancel", "keep" or "delete".
    void confirmRemoval(Answer done);
    // choice: "cancel", "tray" or "quit".
    void askClose(int activeDownloads, Answer done);
    // choice: the chosen path, empty when dismissed. One path needs no question.
    void chooseVideo(const QStringList &paths, std::function<void(const QString &)> done);
    void toast(const QString &text, const QString &icon = QStringLiteral("check"));

    // Called by the dialog being closed.
    Q_INVOKABLE void answer(const QString &choice, bool remember = false);

signals:
    void currentChanged();
    void toastRequested(const QString &text, const QString &icon);

private:
    struct Pending { QVariantMap spec; Answer done; };
    void show(const QVariantMap &spec, Answer done);
    void next();

    QVariantMap m_current;
    Answer m_done;
    QQueue<Pending> m_queue;
};
