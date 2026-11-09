// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only

#include "ThemeController.h"

#include <QGuiApplication>
#include <QSettings>
#include <QStyleHints>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif

ThemeController::ThemeController(const QString &mode, QObject *parent)
    : QObject(parent), m_mode(mode)
{
    m_dark = wantsDark();
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (m_mode == QLatin1String("system"))
            follow();
    });
}

bool ThemeController::animated() const
{
#ifdef Q_OS_WIN
    BOOL animate = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animate, 0))
        return animate != FALSE;
#endif
    return true;
}

bool ThemeController::transparency() const
{
    const QSettings personalize(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"), QSettings::NativeFormat);
    return personalize.value(QStringLiteral("EnableTransparency"), 1).toInt() != 0;
}

qreal ThemeController::textScale() const
{
    bool forced = false;
    const qreal fromEnvironment = qEnvironmentVariable("MIKU_LIBRARY_TEXT_SCALE").toDouble(&forced);
    const QSettings accessibility(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Accessibility"), QSettings::NativeFormat);
    return qBound(1.0, forced ? fromEnvironment : accessibility.value(QStringLiteral("TextScaleFactor"), 100).toDouble() / 100.0, 1.5);
}

bool ThemeController::highContrast() const
{
    if (qEnvironmentVariableIsSet("MIKU_LIBRARY_HIGH_CONTRAST"))
        return true;
#ifdef Q_OS_WIN
    HIGHCONTRASTW contrast{};
    contrast.cbSize = sizeof(contrast);
    if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0))
        return (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
#endif
    return false;
}

bool ThemeController::wantsDark() const
{
    if (m_mode == QLatin1String("system"))
        return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    return m_mode == QLatin1String("dark");
}

void ThemeController::setMode(const QString &mode)
{
    if (mode == m_mode)
        return;
    m_mode = mode;
    emit modeChanged();
    follow();
}

void ThemeController::follow()
{
    if (wantsDark() == m_dark)
        return;
    if (m_fading && animated())
        emit switchRequested();
    else
        commit();
}

void ThemeController::commit()
{
    const bool dark = wantsDark();
    if (dark == m_dark)
        return;
    m_dark = dark;
    emit darkChanged();
}
