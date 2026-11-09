// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QObject>
#include <QString>

// Which palette is showing. The colours themselves live in T.qml.
//
// A switch runs in two steps so the window can freeze its old look first: setMode()
// asks through switchRequested, and the palette flips when commit() is called. With
// no window listening, setMode() flips it at once.
class ThemeController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString mode READ mode NOTIFY modeChanged)
    Q_PROPERTY(bool dark READ dark NOTIFY darkChanged)
    Q_PROPERTY(bool animated READ animated CONSTANT)
    // False when Windows has "Transparency effects" turned off: glass becomes opaque.
    Q_PROPERTY(bool transparency READ transparency CONSTANT)
    // Windows' "Make text bigger", 1 to 1.5; beyond that the layouts stop holding.
    Q_PROPERTY(qreal textScale READ textScale CONSTANT)
    // A Windows contrast theme is on: text and outlines go to full strength.
    Q_PROPERTY(bool highContrast READ highContrast CONSTANT)
    // Set by the window that handles switchRequested.
    Q_PROPERTY(bool fading MEMBER m_fading)
public:
    explicit ThemeController(const QString &mode, QObject *parent = nullptr);

    QString mode() const { return m_mode; }
    bool dark() const { return m_dark; }
    // False when Windows has "Show animations" turned off.
    bool animated() const;
    bool transparency() const;
    qreal textScale() const;
    bool highContrast() const;

    Q_INVOKABLE void setMode(const QString &mode);
    Q_INVOKABLE void commit();

signals:
    void modeChanged();
    void darkChanged();
    void switchRequested();

private:
    bool wantsDark() const;
    void follow();

    QString m_mode;
    bool m_dark = false;
    bool m_fading = false;
};
