#include "Theme.h"
#include "Overlays.h"
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QFrame>
#include <QStyleHintReturn>
namespace {
class QuietStyle : public QProxyStyle {
public:
    QuietStyle() : QProxyStyle(QStyleFactory::create("Fusion")) {}
    int styleHint(StyleHint hint, const QStyleOption *option=nullptr,
                  const QWidget *widget=nullptr, QStyleHintReturn *data=nullptr) const override {
        if(hint==SH_ComboBox_Popup || hint==SH_ComboBox_UseNativePopup) return 0;
        if(hint==SH_ComboBox_PopupFrameStyle) return QFrame::NoFrame;
        if(hint==SH_Menu_Mask && option && data) {
            if(auto *mask=qstyleoption_cast<QStyleHintReturnMask *>(data)) {
                QPainterPath shape;shape.addRoundedRect(option->rect,8,8);
                mask->region=QRegion(shape.toFillPolygon().toPolygon());return 1;
            }
        }
        return QProxyStyle::styleHint(hint,option,widget,data);
    }
    void drawPrimitive(PrimitiveElement element,const QStyleOption *option,QPainter *painter,
                       const QWidget *widget=nullptr) const override {
        if(element==PE_FrameFocusRect)return;
        QProxyStyle::drawPrimitive(element,option,painter,widget);
    }
};
}
namespace Theme {
QFont uiFont(int size, QFont::Weight weight) { QFont f; f.setFamilies({"Segoe UI","Microsoft YaHei UI"}); f.setPointSize(size); f.setWeight(weight); return f; }
QFont monoFont(int size) { QFont f("Cascadia Mono"); f.setPointSize(size); return f; }
QIcon icon(const QString &name, const QColor &color) {
 const qreal dpr=qApp?qApp->devicePixelRatio():1; QPixmap pm(QSize(24,24)*dpr); pm.setDevicePixelRatio(dpr); pm.fill(Qt::transparent);
 QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing); p.setPen(QPen(color,1.6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin)); p.setBrush(Qt::NoBrush); QPainterPath path;
 if(name=="home"){ path.moveTo(3,11); path.lineTo(12,4); path.lineTo(21,11); path.moveTo(6,10); path.lineTo(6,20); path.lineTo(18,20); path.lineTo(18,10); path.moveTo(10,20); path.lineTo(10,14); path.lineTo(14,14); path.lineTo(14,20); }
 else if(name=="search"){ p.drawEllipse(QRectF(4,4,12,12)); p.drawLine(14,14,20,20); }
 else if(name=="library"){ p.drawRoundedRect(QRectF(4,4,12,16),2,2); p.drawLine(8,8,12,8); path.moveTo(19,5); path.lineTo(21,19); }
 else if(name=="download"){ path.moveTo(12,3); path.lineTo(12,15); path.moveTo(7,10); path.lineTo(12,15); path.lineTo(17,10); path.moveTo(4,16); path.lineTo(4,21); path.lineTo(20,21); path.lineTo(20,16); }
 else if(name=="sites"){ p.drawEllipse(QRectF(3,3,18,18)); p.drawEllipse(QRectF(8,3,8,18)); p.drawLine(3,12,21,12); }
 else if(name=="settings"){ for(int y:{6,12,18}) p.drawLine(3,y,21,y); p.setBrush(QColor(SURFACE)); p.drawEllipse(QPointF(8,6),2,2); p.drawEllipse(QPointF(16,12),2,2); p.drawEllipse(QPointF(10,18),2,2); }
 else if(name=="refresh"){ path.moveTo(19,8); path.cubicTo(16,1,5,3,5,11); path.moveTo(5,16); path.cubicTo(9,23,20,21,20,13); path.moveTo(19,3); path.lineTo(19,8); path.lineTo(14,8); path.moveTo(5,21); path.lineTo(5,16); path.lineTo(10,16); }
 else if(name=="chevron-down"){ path.moveTo(6,9);path.lineTo(12,15);path.lineTo(18,9); }
 else if(name=="back"){ path.moveTo(14,5); path.lineTo(7,12); path.lineTo(14,19); path.moveTo(7,12); path.lineTo(21,12); }
 else if(name=="forward"){ path.moveTo(10,5);path.lineTo(17,12);path.lineTo(10,19);path.moveTo(3,12);path.lineTo(17,12); }
 else if(name=="up"){ path.moveTo(5,11);path.lineTo(12,4);path.lineTo(19,11);path.moveTo(12,4);path.lineTo(12,21); }
 else if(name=="folder" || name=="folder-plus"){ path.moveTo(3,8);path.lineTo(3,5);path.lineTo(10,5);path.lineTo(12,8);path.lineTo(21,8);path.lineTo(21,20);path.lineTo(3,20);path.closeSubpath();if(name=="folder-plus"){path.moveTo(9,14);path.lineTo(15,14);path.moveTo(12,11);path.lineTo(12,17);} }
 else if(name=="grid"){ for(int x:{4,14})for(int y:{4,14})p.drawRoundedRect(QRectF(x,y,6,6),1.2,1.2); }
 else if(name=="list"){ for(int y:{6,12,18}){p.drawPoint(4,y);p.drawLine(9,y,20,y);} }
 else if(name=="external"){ path.moveTo(14,4); path.lineTo(20,4); path.lineTo(20,10); path.moveTo(20,4); path.lineTo(10,14); path.moveTo(10,5); path.lineTo(5,5); path.lineTo(5,20); path.lineTo(19,20); path.lineTo(19,14); }
 else if(name=="copy"){ p.drawRoundedRect(QRectF(8,7,12,14),2,2); path.moveTo(15,4); path.lineTo(4,4); path.lineTo(4,17); }
 else if(name=="close" || name=="error"){ if(name=="error")p.drawEllipse(QRectF(3,3,18,18));path.moveTo(8,8);path.lineTo(16,16);path.moveTo(16,8);path.lineTo(8,16); }
 else if(name=="warning"){ path.moveTo(12,3);path.lineTo(22,20);path.lineTo(2,20);path.closeSubpath();p.drawLine(12,9,12,13);p.drawPoint(12,17); }
 else if(name=="info"){ p.drawEllipse(QRectF(3,3,18,18));p.drawLine(12,11,12,17);p.drawPoint(12,7); }
 p.drawPath(path); return QIcon(pm);
}
void applyTheme(QApplication *app) {
 if(!app)return; app->setStyle(new QuietStyle); app->setFont(uiFont(10)); QPalette pal;
 // Native roll animations capture a square menu before our popup geometry is set.
 QApplication::setEffectEnabled(Qt::UI_AnimateCombo,false);
 QApplication::setEffectEnabled(Qt::UI_AnimateTooltip,false);
 QApplication::setEffectEnabled(Qt::UI_FadeTooltip,false);
 Ui::installOverlays(app);
 pal.setColor(QPalette::Window,QColor(BG)); pal.setColor(QPalette::WindowText,QColor(TEXT)); pal.setColor(QPalette::Base,QColor(SURFACE));
 pal.setColor(QPalette::AlternateBase,QColor(SURFACE2)); pal.setColor(QPalette::Text,QColor(TEXT)); pal.setColor(QPalette::Button,QColor(SURFACE));
 pal.setColor(QPalette::ButtonText,QColor(TEXT)); pal.setColor(QPalette::Highlight,QColor(ACCENT)); pal.setColor(QPalette::HighlightedText,QColor("#103f3c"));
 pal.setColor(QPalette::PlaceholderText,QColor(TEXT3)); pal.setColor(QPalette::Link,QColor(ACCENT)); app->setPalette(pal);
 app->setStyleSheet(QStringLiteral(R"QSS(
QWidget { color:#243b3a; font-family:"Segoe UI","Microsoft YaHei UI"; font-size:10pt; }
QMainWindow,QStackedWidget,#Content,#ScrollBody { background:#f5f9f9; }
QLabel { background:transparent; }
#TitleBar { background:white; border-bottom:1px solid #dce9e7; }
#AppMark { background:#39c5bb; color:#103f3c; border-radius:8px; font-size:13pt; font-weight:700; }
#TitleBrand { color:#243b3a; font-size:9pt; font-weight:600; }
#TitleContext { color:#718785; font-size:9pt; }
QToolButton { border:1px solid transparent; background:transparent; border-radius:6px; padding:4px; }
QToolButton:hover { background:#e8f4f2; }
QToolButton#WindowClose:hover { background:#f7d9d8; }
QToolButton:focus { border:1px solid #147d76; }
#NavRail { background:white; border-right:1px solid #dce9e7; }
#Brand { font-size:16pt; font-weight:700; }
#BrandSub,#NavFooter { color:#718785; font-size:8.5pt; }
#NavSection { color:#869b99; font-size:8pt; padding:10px 12px 4px; }
QPushButton#NavButton { background:transparent; border:none; border-radius:8px; text-align:left; padding:11px 16px; color:#5f7775; }
QPushButton#NavButton:hover { background:#eef7f5; color:#243b3a; }
QPushButton#NavButton:focus { background:#edf7f5; color:#126c66; }
QPushButton#NavButton:checked { background:#def4f1; color:#126c66; font-weight:600; }
#EngineBadge { background:#eef7f5; border:1px solid #dce9e7; border-radius:8px; }
QLabel#H1 { font-size:23pt; font-weight:650; color:#203f3d; }
QLabel#H2 { font-size:14pt; font-weight:600; color:#243b3a; }
QLabel#H3 { font-size:11pt; font-weight:600; }
QLabel#Muted { color:#5f7775; }
QLabel#Faint,QLabel#Subtle { color:#718785; font-size:9pt; }
QLabel#Eyebrow { color:#147d76; font-size:8pt; font-weight:600; }
QLabel#CardTitle { color:#243b3a; font-size:10pt; font-weight:600; }
QLabel#MetricValue { color:#243b3a; font-size:21pt; font-weight:600; }
QLabel#MetricCaption { color:#718785; font-size:9pt; }
#Panel,#SearchPanel,#MetricCard,#SettingsSection { background:white; border:1px solid #dce9e7; border-radius:12px; }
#AnimeCard { background:transparent; border:none; }
#AnimeCard:hover QLabel#CardTitle { color:#147d76; }
#Poster { background:#e6f1ef; border-radius:8px; color:#869b99; }
#EmptyState { background:#f5f9f9; }
#EmptyIcon { background:#def4f1; border-radius:24px; padding:12px; }
QScrollArea { border:none; background:transparent; }
QScrollArea > QWidget > QWidget { background:transparent; }
QScrollBar:vertical { background:transparent; width:7px; margin:4px 0; }
QScrollBar::handle:vertical { background:#c1d5d3; min-height:36px; border-radius:3px; }
QScrollBar::handle:vertical:hover { background:#91b4b0; }
QScrollBar:horizontal { background:transparent; height:7px; }
QScrollBar::handle:horizontal { background:#c1d5d3; min-width:32px; border-radius:3px; }
QScrollBar::add-line,QScrollBar::sub-line { width:0; height:0; }
QScrollBar::add-page,QScrollBar::sub-page { background:none; }
QLineEdit,QComboBox,QSpinBox,QDoubleSpinBox { background:white; border:1px solid #d5e6e3; border-radius:7px; padding:8px 11px; color:#243b3a; selection-background-color:#b8eee7; selection-color:#103f3c; min-height:19px; }
QLineEdit:focus,QComboBox:focus,QSpinBox:focus { border-color:#39c5bb; background:#fcfefe; }
QLineEdit#SearchBox { font-size:12pt; border:none; background:transparent; padding:13px 8px; }
QComboBox { padding:8px 36px 8px 13px; border-radius:8px; }
QComboBox:hover { border-color:#a8ceca; background:#fcfefe; }
QComboBox:on { border-color:#39c5bb; background:#f4fbfa; }
QComboBox:disabled { color:#9aafac; background:#eff5f4; border-color:#e0eeeb; }
QComboBox::drop-down { subcontrol-origin:border; subcontrol-position:top right; border:none; width:32px; }
QComboBox::down-arrow { image:url(:/assets/icons/chevron-down.png); width:16px; height:16px; }
QComboBox::down-arrow:on { image:url(:/assets/icons/chevron-up.png); }
QComboBox::down-arrow:disabled { width:12px; height:12px; }
QSpinBox,QDoubleSpinBox { padding:8px 38px 8px 13px; border-radius:8px; }
QAbstractSpinBox QLineEdit,QComboBox QLineEdit { background:transparent; border:none; padding:0; min-height:0; }
QSpinBox:hover,QDoubleSpinBox:hover,QLineEdit:hover { border-color:#a8ceca; }
QSpinBox::up-button,QDoubleSpinBox::up-button { subcontrol-origin:border; subcontrol-position:top right; width:28px; margin:1px 1px 0 0; border:none; border-left:1px solid #e2eeeb; border-top-right-radius:7px; background:#f3f8f7; }
QSpinBox::down-button,QDoubleSpinBox::down-button { subcontrol-origin:border; subcontrol-position:bottom right; width:28px; margin:0 1px 1px 0; border:none; border-left:1px solid #e2eeeb; border-bottom-right-radius:7px; background:#f3f8f7; }
QSpinBox::up-button:hover,QSpinBox::down-button:hover,QDoubleSpinBox::up-button:hover,QDoubleSpinBox::down-button:hover { background:#def4f1; }
QSpinBox::up-button:pressed,QSpinBox::down-button:pressed { background:#b8eee7; }
QSpinBox::up-arrow,QDoubleSpinBox::up-arrow { image:url(:/assets/icons/plus.png); width:12px; height:12px; }
QSpinBox::down-arrow,QDoubleSpinBox::down-arrow { image:url(:/assets/icons/minus.png); width:12px; height:12px; }
QSpinBox:disabled,QDoubleSpinBox:disabled,QLineEdit:disabled { background:#eff5f4; color:#9aafac; border-color:#e0eeeb; }
QSpinBox::up-button:disabled,QSpinBox::down-button:disabled { background:#eff5f4; }
QComboBox QAbstractItemView { background:white; border:1px solid #d5e6e3; selection-background-color:#def4f1; selection-color:#126c66; padding:4px; }
QPushButton { background:white; border:1px solid #d5e6e3; border-radius:7px; padding:8px 15px; min-height:18px; color:#3c5b57; }
QPushButton:hover { background:#e8f6f3; border-color:#9dd7d1; }
QPushButton:pressed { background:#ccece7; }
QPushButton:disabled { color:#a4b8b5; background:#eff5f4; border-color:#e0eeeb; }
QPushButton:focus { border-color:#147d76; }
QPushButton#Primary { background:#39c5bb; color:#103f3c; border:1px solid #39c5bb; font-weight:600; }
QPushButton#Primary:hover { background:#31b1a8; }
QPushButton#Primary:pressed { background:#2ba197; }
QPushButton#Primary:disabled { background:#accfcb; border-color:#accfcb; }
QPushButton#Ghost,QPushButton#IconButton { background:transparent; border:none; color:#5f7775; }
QPushButton#Ghost:hover,QPushButton#IconButton:hover { background:#e6f3f1; color:#243b3a; }
QPushButton#Ghost:focus,QPushButton#IconButton:focus { background:#def4f1; color:#126c66; }
QPushButton#IconButton { padding:3px; min-height:0; }
QPushButton#Danger { color:#aa4545; }
QPushButton#Danger:hover { background:#fbedeb; border-color:#ddb4b1; }
#DayBar { background:#e8f2f1; border-radius:10px; }
QPushButton#DayTab { background:transparent; border:none; border-radius:7px; color:#5f7775; padding:7px 17px; }
QPushButton#DayTab:hover { background:#f4faf9; }
QPushButton#DayTab:focus { background:#edf7f5; color:#126c66; }
QPushButton#DayTab:checked { background:white; color:#147d76; font-weight:600; }
QPushButton#Chip { border:1px solid #dce9e7; background:white; color:#5f7775; border-radius:6px; padding:5px 10px; }
QPushButton#Chip:checked { border-color:#8ad4cc; background:#def4f1; color:#147d76; }
#GroupModule { background:white; border:1px solid #d8e9e5; border-radius:10px; }
QPushButton#GroupHeader { border:none; background:transparent; text-align:left; padding:15px 18px; font-weight:600; }
QPushButton#GroupHeader:hover,QPushButton#GroupHeader:focus { background:#edf7f5; border-radius:9px; }
#ResourceRow { background:transparent; border-top:1px solid #e8f1ef; }
#ResourceRow:hover { background:#f4faf9; }
QTableView { background:white; alternate-background-color:#fcfefd; border:1px solid #dce9e7; border-radius:10px; gridline-color:transparent; selection-background-color:#def4f1; selection-color:#243b3a; outline:0; }
QTableView::item { border-bottom:1px solid #e7f1ef; padding:10px 8px; }
QTableView::item:selected { background:#def4f1; color:#126c66; }
QHeaderView::section { background:#edf5f4; color:#5f7775; border:none; border-bottom:1px solid #dce9e7; padding:13px 10px; font-weight:600; }
QHeaderView::section:first { border-top-left-radius:8px; }
QTableView QTableCornerButton::section { background:#edf5f4; border:none; }
QTabWidget::pane { background:white; border:1px solid #dce9e7; border-radius:10px; top:-1px; }
QTabBar::tab { background:transparent; color:#5f7775; padding:12px 22px; border-bottom:2px solid transparent; }
QTabBar::tab:selected { color:#147d76; border-bottom-color:#39c5bb; font-weight:600; }
QTabBar::tab:hover:!selected { color:#147d76; background:#edf7f5; }
QCheckBox { spacing:8px; padding:4px 0; }
QCheckBox::indicator { width:16px; height:16px; border:1px solid #b2d0ca; background:white; border-radius:4px; }
QCheckBox::indicator:checked { background:#147d76; border-color:#147d76; image:url(:/assets/icons/check.png); }
QCheckBox::indicator:hover { border-color:#39c5bb; }
QCheckBox::indicator:indeterminate { background:#def4f1; image:url(:/assets/icons/dash.png); }
QCheckBox::indicator:disabled { background:#e7efed; border-color:#cededb; }
QCheckBox:disabled { color:#9aafac; }
#DownloaderPopup { background:transparent; }
#DownloaderPopupPanel { background:white; border:1px solid #dce9e7; border-radius:12px; }
QLabel#DownloaderPopupHeading { color:#718785; font-size:9pt; padding:0 10px 6px; }
QPushButton#DownloaderOption { background:transparent; border:1px solid transparent; border-radius:8px; padding:0; min-height:54px; max-height:54px; text-align:left; }
QPushButton#DownloaderOption:hover,QPushButton#DownloaderOption:focus { background:#f0f7f6; border-color:#dce9e7; }
QPushButton#DownloaderOption:checked { background:#def4f1; border-color:#c3e8e3; }
QLabel#DownloaderOptionIcon { background:#edf6f4; border-radius:8px; }
QLabel#DownloaderOptionName { color:#243b3a; font-weight:600; }
QLabel#DownloaderOptionDescription,QLabel#DownloaderUnconfigured { color:#718785; font-size:8.5pt; }
QLabel#DownloaderCurrent { background:#c5eae5; color:#126c66; border-radius:5px; padding:3px 6px; font-size:8pt; }
#DownloaderPopupDivider { background:#e5efec; border:0; margin:0 10px; }
QPushButton#DownloaderConfigure { background:transparent; border:0; border-radius:7px; text-align:left; padding:8px 10px; }
QPushButton#DownloaderConfigure:hover,QPushButton#DownloaderConfigure:focus { background:#edf7f5; }
QFrame[settingsCard="true"] { background:white; border:1px solid #dce9e7; border-radius:12px; }
QLabel#SettingsCardTitle { font-size:11pt; font-weight:600; color:#243b3a; }
QLabel#DownloaderHint { color:#5f7775; font-size:9pt; }
QPushButton#ChooseDownloader { border-radius:8px; text-align:left; padding:8px 12px; }
QMenu { background:white; color:#243b3a; border:1px solid #d5e6e3; padding:6px; border-radius:8px; }
QMenu::item { padding:9px 26px; }
QMenu::item:selected { background:#def4f1; color:#126c66; }
QToolTip { background:#fcfffe; color:#375b57; border:1px solid #dce9e7; border-radius:8px; padding:8px 10px; }
#TooltipBubble { background:transparent; }
QLabel#TooltipText { background:transparent; color:#375b57; font-size:9pt; }
#DialogSurface { background:transparent; }
#DialogPanel { background:white; border:1px solid #dce9e7; border-radius:14px; }
#DialogHeader { background:transparent; }
QLabel#DialogTitle { font-size:13pt; font-weight:600; color:#203f3d; }
QLabel#DialogBody { color:#5f7775; font-size:10pt; background:white; }
QLabel#DirectoryNotice { color:#375b57; background:#f0f7f5; border:1px solid #dce9e7; border-radius:8px; padding:12px; }
#DialogClose:hover { background:#edf5f4; }
QToolButton#DownloaderSwitch { background:transparent; border:none; border-radius:6px; }
QToolButton#DownloaderSwitch:hover { background:#d8efeb; }
QPushButton#GroupDownload { background:#e3f5f2; color:#167a72; border:0; border-radius:7px; padding:7px 13px; font-weight:600; }
QPushButton#GroupDownload:hover { background:#c9eeE8; }
QPushButton#GroupDownload:disabled { background:#eef4f3; color:#8ca6a2; }
QListWidget#VideoList { background:white; border:1px solid #dce9e7; border-radius:8px; padding:6px; outline:0; }
QListWidget#VideoList::item { padding:10px; border-radius:6px; }
QListWidget#VideoList::item:selected { background:#def4f1; color:#126c66; }
#NoticeInfo { background:#def4f1; border-radius:12px; }
#NoticeWarning { background:#fbf3e2; border-radius:12px; }
#NoticeError { background:#fbedeb; border-radius:12px; }
#FilePicker { background:white; }
#FilePicker QTreeView,#FilePicker QListView { border:1px solid #dce9e7; border-radius:8px; padding:4px; background:white; outline:0; }
#FilePicker QTreeView::item,#FilePicker QListView::item { min-height:28px; padding:3px 6px; border:none; }
#FilePicker QTreeView::item:selected,#FilePicker QListView::item:selected { background:#def4f1; color:#126c66; border-radius:4px; }
#FilePicker QHeaderView::section { padding:8px; background:#f4f9f8; }
#FilePicker QToolButton { min-width:24px; min-height:24px; }
#FilePicker QToolButton[pickerTool="true"] { background:transparent; border:1px solid transparent; border-radius:8px; padding:5px; min-width:0; min-height:0; }
#FilePicker QToolButton[pickerTool="true"]:hover { background:#edf7f5; border-color:#dce9e7; }
#FilePicker QToolButton[pickerTool="true"]:checked { background:#def4f1; border-color:#b8e3dd; }
#FilePicker QToolButton[pickerTool="true"]:pressed { background:#ccece7; }
#FilePicker QToolButton[pickerTool="true"]:focus { border-color:#39c5bb; }
#FilePicker QToolButton[pickerTool="true"]:disabled { background:transparent; border-color:transparent; }
#FilePicker QSplitter::handle { background:white; width:10px; }
QPlainTextEdit { background:white; border:1px solid #d5e6e3; border-radius:8px; padding:10px; }
)QSS"));
}
}

