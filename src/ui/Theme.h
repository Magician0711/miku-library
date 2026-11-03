#pragma once

#include <QColor>
#include <QFont>
#include <QIcon>
#include <QString>

class QApplication;

namespace Theme {

inline const QString BG = QStringLiteral("#f5f9f9");
inline const QString SURFACE = QStringLiteral("#ffffff");
inline const QString SURFACE2 = QStringLiteral("#edf5f4");
inline const QString SURFACE3 = QStringLiteral("#e8f3f2");
inline const QString BORDER = QStringLiteral("#dce9e7");
inline const QString BORDER_STRONG = QStringLiteral("#a8ceca");
inline const QString TEXT = QStringLiteral("#243b3a");
inline const QString TEXT2 = QStringLiteral("#5f7775");
inline const QString TEXT3 = QStringLiteral("#718785");
inline const QString ACCENT = QStringLiteral("#39c5bb");
inline const QString ACCENT_TEXT = QStringLiteral("#126c66");
inline const QString ACCENT_SOFT = QStringLiteral("#def4f1");
inline const QString OK = QStringLiteral("#208f85");
inline const QString WARN = QStringLiteral("#ad7c28");
inline const QString ERR = QStringLiteral("#b85a53");
inline const QString INFO = QStringLiteral("#4c7b9d");

inline const QString PROBED_BG = QStringLiteral("#e0f4ef");
inline const QString PROBED_FG = QStringLiteral("#217b70");
inline const QString GUESSED_BG = QStringLiteral("#f4efdf");
inline const QString GUESSED_FG = QStringLiteral("#8f7229");
inline const QString UNKNOWN_BG = QStringLiteral("#edf3f2");
inline const QString UNKNOWN_FG = QStringLiteral("#728683");
inline const QString HDR_BG = QStringLiteral("#eee9f4");
inline const QString HDR_FG = QStringLiteral("#77618f");
inline const QString SUB_BG = QStringLiteral("#e8eef4");
inline const QString SUB_FG = QStringLiteral("#496b8e");

QFont uiFont(int pointSize = 10, QFont::Weight weight = QFont::Normal);
QFont monoFont(int pointSize = 9);
void applyTheme(QApplication *app);
QIcon icon(const QString &name, const QColor &color = QColor("#5f7775"));

} // namespace Theme

namespace Icons {
inline const QString HOME = QStringLiteral("⌂");
inline const QString SEARCH = QStringLiteral("⌕");
inline const QString HEART = QStringLiteral("♥");
inline const QString DOWNLOAD = QStringLiteral("⬇");
inline const QString GLOBE = QStringLiteral("◎");
inline const QString SETTINGS = QStringLiteral("⚙");
inline const QString BACK = QStringLiteral("←");
inline const QString REFRESH = QStringLiteral("⟳");
inline const QString LINK = QStringLiteral("↗");
inline const QString COPY = QStringLiteral("⧉");
inline const QString CHEVRON_DOWN = QStringLiteral("▾");
inline const QString CHEVRON_RIGHT = QStringLiteral("▸");
inline const QString CHECK = QStringLiteral("✓");
inline const QString WARNING = QStringLiteral("!");
inline const QString PLAY = QStringLiteral("▶");
inline const QString PAUSE = QStringLiteral("Ⅱ");
inline const QString DELETE = QStringLiteral("×");
} // namespace Icons
