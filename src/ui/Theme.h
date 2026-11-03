#pragma once

#include <QColor>
#include <QFont>
#include <QIcon>
#include <QObject>
#include <QString>

class QApplication;

// Design tokens for the light and dark palettes. The application stylesheet is
// generated from the active palette, and custom painting reads it at paint time,
// so switching modes only needs a restyle and repaint.
namespace Theme {

struct Palette {
    bool dark = false;
    QColor chrome, bg, surface, surface2, surface3, border, borderStrong;
    QColor text, text2, text3, disabledText;
    QColor accent, accentHover, accentPressed, accentInk, accentText, accentSoft, accentSoft2;
    QColor primaryDisabled, primaryDisabledText;
    QColor ok, warn, warnSoft, err, errSoft, errBorder, info;
    QColor navHover, rowHover, separator, scroll, scrollHover, segmentHover, indicatorBorder, indicatorBg;
    QColor probedBg, probedFg, guessedBg, guessedFg, unknownBg, unknownFg, hdrBg, hdrFg, subBg, subFg;
    QColor shadow;
};

// A palette entry chosen by reference, resolved whenever something is drawn.
using Role = QColor Palette::*;

enum class Mode { System, Light, Dark };

const Palette &c();
inline QColor color(Role role) { return c().*role; }
Mode mode();
Mode modeFromString(const QString &value);
QString modeToString(Mode mode);
QString modeName(Mode mode);
// Restyles the running application; emits Notifier::changed when the palette flips.
void setMode(Mode mode);

class Notifier : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
signals:
    void changed();
};
Notifier *notifier();

QFont uiFont(int pointSize = 10, QFont::Weight weight = QFont::Normal);
QFont monoFont(int pointSize = 9);
void applyTheme(QApplication *app, Mode mode = Mode::System);
// Line icons painted on demand in the given palette role, so they follow theme changes.
QIcon icon(const QString &name, Role role = &Palette::text2);
// Two-state icon for checkable buttons: muted when off, accent when checked.
QIcon stateIcon(const QString &name);

} // namespace Theme
