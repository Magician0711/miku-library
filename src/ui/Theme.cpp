#include "Theme.h"
#include "Overlays.h"
#include "Motion.h"
#include <QApplication>
#include <QDir>
#include <QFrame>
#include <QHash>
#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QProxyStyle>
#include <QRegularExpression>
#include <QStyleFactory>
#include <QStyleHintReturn>
#include <QStyleHints>

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

using Theme::Palette;

Palette lightPalette()
{
    Palette p;
    p.dark=false;
    p.chrome=QColor("#eef3f3"); p.bg=QColor("#f8fafa"); p.surface=QColor("#ffffff");
    p.surface2=QColor("#f3f7f7"); p.surface3=QColor("#e9f0f0"); p.border=QColor("#e1e9e9"); p.borderStrong=QColor("#c3d4d3");
    p.text=QColor("#17302e"); p.text2=QColor("#566a68"); p.text3=QColor("#7d8e8c"); p.disabledText=QColor("#a3b2b0");
    p.accent=QColor("#39c5bb"); p.accentHover=QColor("#2fb5ab"); p.accentPressed=QColor("#28a49b");
    p.accentInk=QColor("#083b37"); p.accentText=QColor("#0f6b64"); p.accentSoft=QColor("#e0f5f2"); p.accentSoft2=QColor("#c8ede8");
    p.primaryDisabled=QColor("#b9dcd8"); p.primaryDisabledText=QColor("#4d716d");
    p.ok=QColor("#1f8f6a"); p.warn=QColor("#b07a1c"); p.warnSoft=QColor("#fbf2df");
    p.err=QColor("#c2504a"); p.errSoft=QColor("#fcecea"); p.errBorder=QColor("#eab9b5"); p.info=QColor("#3f76a6");
    p.navHover=QColor("#e2eaea"); p.rowHover=QColor("#fafcfc"); p.separator=QColor("#b5c4c3");
    p.scroll=QColor("#cbd7d6"); p.scrollHover=QColor("#a3b6b4"); p.segmentHover=QColor(255,255,255,140);
    p.segmentThumb=QColor("#ffffff"); p.segmentThumbBorder=QColor("#e1e9e9");
    p.indicatorBorder=QColor("#b6cac8"); p.indicatorBg=QColor("#ffffff");
    p.probedBg=QColor("#dff3ec"); p.probedFg=QColor("#1d7a62"); p.guessedBg=QColor("#f6f0e1"); p.guessedFg=QColor("#8a6a1f");
    p.unknownBg=QColor("#edf2f2"); p.unknownFg=QColor("#6f8381"); p.hdrBg=QColor("#efeaf6"); p.hdrFg=QColor("#6f5a8c");
    p.subBg=QColor("#e7eef6"); p.subFg=QColor("#43678b");
    p.shadow=QColor(25,70,65,32);
    return p;
}

Palette darkPalette()
{
    Palette p;
    p.dark=true;
    p.chrome=QColor("#121718"); p.bg=QColor("#181e1f"); p.surface=QColor("#1f2628");
    p.surface2=QColor("#252d2f"); p.surface3=QColor("#2c3537"); p.border=QColor("#2e383a"); p.borderStrong=QColor("#435250");
    p.text=QColor("#e2ebe9"); p.text2=QColor("#a7b5b3"); p.text3=QColor("#7b8a88"); p.disabledText=QColor("#56625f");
    p.accent=QColor("#39c5bb"); p.accentHover=QColor("#4ccfc5"); p.accentPressed=QColor("#31b2a8");
    p.accentInk=QColor("#04302c"); p.accentText=QColor("#5ad3c9"); p.accentSoft=QColor("#163a37"); p.accentSoft2=QColor("#1d4c48");
    p.primaryDisabled=QColor("#24433f"); p.primaryDisabledText=QColor("#6f8f8b");
    p.ok=QColor("#4cc28f"); p.warn=QColor("#e0aa4c"); p.warnSoft=QColor("#3a301c");
    p.err=QColor("#ef6f67"); p.errSoft=QColor("#3b2322"); p.errBorder=QColor("#6b3532"); p.info=QColor("#6ea8dc");
    p.navHover=QColor("#1d2526"); p.rowHover=QColor("#232b2d"); p.separator=QColor("#3c4a48");
    p.scroll=QColor("#3a4547"); p.scrollHover=QColor("#52605e"); p.segmentHover=QColor(255,255,255,18);
    p.segmentThumb=QColor("#3a4547"); p.segmentThumbBorder=QColor("#4a5856");
    p.indicatorBorder=QColor("#4d5c5a"); p.indicatorBg=QColor("#1a2122");
    p.probedBg=QColor("#193630"); p.probedFg=QColor("#6fd3ac"); p.guessedBg=QColor("#3a311b"); p.guessedFg=QColor("#e3c06f");
    p.unknownBg=QColor("#2a3233"); p.unknownFg=QColor("#9aaaa8"); p.hdrBg=QColor("#2f2840"); p.hdrFg=QColor("#bea8e3");
    p.subBg=QColor("#1f2d3b"); p.subFg=QColor("#8db6de");
    p.shadow=QColor(0,0,0,90);
    return p;
}

Theme::Mode g_mode=Theme::Mode::System;
Palette g_palette=lightPalette();

bool systemPrefersDark()
{
    return qApp && QGuiApplication::styleHints()->colorScheme()==Qt::ColorScheme::Dark;
}

// Glyphs are authored on a 24 x 24 grid.
void drawGlyph(QPainter &p, const QString &name, const QColor &color)
{
 p.setRenderHint(QPainter::Antialiasing); p.setPen(QPen(color,1.7,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin)); p.setBrush(Qt::NoBrush); QPainterPath path;
 if(name=="home"){ path.moveTo(3,11); path.lineTo(12,4); path.lineTo(21,11); path.moveTo(6,10); path.lineTo(6,20); path.lineTo(18,20); path.lineTo(18,10); path.moveTo(10,20); path.lineTo(10,14); path.lineTo(14,14); path.lineTo(14,20); }
 else if(name=="calendar"){ p.drawRoundedRect(QRectF(3.5,5,17,15.5),3,3); p.drawLine(QPointF(3.5,10),QPointF(20.5,10)); p.drawLine(8,3,8,7); p.drawLine(16,3,16,7); p.drawPoint(QPointF(8,14.5)); p.drawPoint(QPointF(12,14.5)); p.drawPoint(QPointF(16,14.5)); }
 else if(name=="search"){ p.drawEllipse(QRectF(4,4,12.5,12.5)); p.drawLine(QPointF(14.5,14.5),QPointF(20,20)); }
 else if(name=="library" || name=="heart"){ path.moveTo(12,20); path.cubicTo(5,15,3,12,3,8.5); path.cubicTo(3,5.5,5.3,3.8,7.6,3.8); path.cubicTo(9.6,3.8,11,5,12,6.6); path.cubicTo(13,5,14.4,3.8,16.4,3.8); path.cubicTo(18.7,3.8,21,5.5,21,8.5); path.cubicTo(21,12,19,15,12,20); }
 else if(name=="download"){ path.moveTo(12,3.5); path.lineTo(12,15); path.moveTo(7,10); path.lineTo(12,15); path.lineTo(17,10); path.moveTo(4,16); path.lineTo(4,20); path.lineTo(20,20); path.lineTo(20,16); }
 else if(name=="sites"){ p.drawEllipse(QRectF(3,3,18,18)); p.drawEllipse(QRectF(8,3,8,18)); p.drawLine(3,12,21,12); }
 else if(name=="settings"){ for(int y:{6,12,18}) p.drawLine(4,y,20,y); p.setBrush(g_palette.surface); p.drawEllipse(QPointF(9,6),2.2,2.2); p.drawEllipse(QPointF(15,12),2.2,2.2); p.drawEllipse(QPointF(10,18),2.2,2.2); }
 else if(name=="refresh"){ path.moveTo(19,8); path.cubicTo(16,1,5,3,5,11); path.moveTo(5,16); path.cubicTo(9,23,20,21,20,13); path.moveTo(19,3); path.lineTo(19,8); path.lineTo(14,8); path.moveTo(5,21); path.lineTo(5,16); path.lineTo(10,16); }
 else if(name=="chevron-down"){ path.moveTo(7,10);path.lineTo(12,15);path.lineTo(17,10); }
 else if(name=="chevron-up"){ path.moveTo(7,14);path.lineTo(12,9);path.lineTo(17,14); }
 else if(name=="chevron-right"){ path.moveTo(10,7);path.lineTo(15,12);path.lineTo(10,17); }
 else if(name=="chevron-left"){ path.moveTo(14,7);path.lineTo(9,12);path.lineTo(14,17); }
 else if(name=="selector"){ path.moveTo(8,9);path.lineTo(12,5);path.lineTo(16,9);path.moveTo(8,15);path.lineTo(12,19);path.lineTo(16,15); }
 else if(name=="back"){ path.moveTo(14,5); path.lineTo(7,12); path.lineTo(14,19); path.moveTo(7,12); path.lineTo(21,12); }
 else if(name=="forward"){ path.moveTo(10,5);path.lineTo(17,12);path.lineTo(10,19);path.moveTo(3,12);path.lineTo(17,12); }
 else if(name=="up"){ path.moveTo(5,11);path.lineTo(12,4);path.lineTo(19,11);path.moveTo(12,4);path.lineTo(12,21); }
 else if(name=="folder" || name=="folder-plus"){ path.moveTo(3,8);path.lineTo(3,5);path.lineTo(10,5);path.lineTo(12,8);path.lineTo(21,8);path.lineTo(21,20);path.lineTo(3,20);path.closeSubpath();if(name=="folder-plus"){path.moveTo(9,14);path.lineTo(15,14);path.moveTo(12,11);path.lineTo(12,17);} }
 else if(name=="grid"){ for(int x:{4,14})for(int y:{4,14})p.drawRoundedRect(QRectF(x,y,6,6),1.2,1.2); }
 else if(name=="list"){ for(int y:{6,12,18}){p.drawPoint(4,y);p.drawLine(9,y,20,y);} }
 else if(name=="external"){ path.moveTo(14,4); path.lineTo(20,4); path.lineTo(20,10); path.moveTo(20,4); path.lineTo(11,13); path.moveTo(10,5); path.lineTo(5,5); path.lineTo(5,19); path.lineTo(19,19); path.lineTo(19,14); }
 else if(name=="copy"){ p.drawRoundedRect(QRectF(8,7.5,12,13),2,2); path.moveTo(15,4); path.lineTo(6,4); path.cubicTo(4.9,4,4,4.9,4,6); path.lineTo(4,16.5); }
 else if(name=="close" || name=="error"){ if(name=="error")p.drawEllipse(QRectF(3,3,18,18));path.moveTo(8,8);path.lineTo(16,16);path.moveTo(16,8);path.lineTo(8,16); }
 else if(name=="warning"){ path.moveTo(12,3.5);path.lineTo(21.5,20);path.lineTo(2.5,20);path.closeSubpath();p.drawLine(12,9.5,12,13.5);p.drawPoint(12,17); }
 else if(name=="info"){ p.drawEllipse(QRectF(3,3,18,18));p.drawLine(12,11,12,17);p.drawPoint(12,7.5); }
 else if(name=="play"){ path.moveTo(8,5);path.lineTo(19,12);path.lineTo(8,19);path.closeSubpath(); }
 else if(name=="pause"){ p.drawLine(9,5.5,9,18.5); p.drawLine(15,5.5,15,18.5); }
 else if(name=="trash"){ p.drawLine(4,7,20,7); path.moveTo(9,7);path.lineTo(9,4.5);path.lineTo(15,4.5);path.lineTo(15,7); path.moveTo(6,7);path.lineTo(7,20);path.lineTo(17,20);path.lineTo(18,7); p.drawLine(10,11,10,16); p.drawLine(14,11,14,16); }
 else if(name=="plus"){ p.drawLine(12,5,12,19); p.drawLine(5,12,19,12); }
 else if(name=="minus"){ p.drawLine(5,12,19,12); }
 else if(name=="check"){ path.moveTo(5,12.5);path.lineTo(10,17);path.lineTo(19,7.5); }
 else if(name=="star"){ path.moveTo(12,3.5);path.lineTo(14.6,9);path.lineTo(20.5,9.6);path.lineTo(16,13.6);path.lineTo(17.3,19.5);path.lineTo(12,16.5);path.lineTo(6.7,19.5);path.lineTo(8,13.6);path.lineTo(3.5,9.6);path.lineTo(9.4,9);path.closeSubpath(); }
 else if(name=="filter"){ path.moveTo(4,5);path.lineTo(20,5);path.lineTo(14,12.5);path.lineTo(14,19);path.lineTo(10,17);path.lineTo(10,12.5);path.closeSubpath(); }
 else if(name=="import"){ path.moveTo(12,3.5);path.lineTo(12,14);path.moveTo(8,10);path.lineTo(12,14);path.lineTo(16,10);path.moveTo(5,13);path.lineTo(5,20);path.lineTo(19,20);path.lineTo(19,13); }
 else if(name=="export"){ path.moveTo(12,14);path.lineTo(12,3.5);path.moveTo(8,7.5);path.lineTo(12,3.5);path.lineTo(16,7.5);path.moveTo(5,13);path.lineTo(5,20);path.lineTo(19,20);path.lineTo(19,13); }
 else if(name=="pulse"){ path.moveTo(3,12);path.lineTo(7,12);path.lineTo(10,5);path.lineTo(14,19);path.lineTo(17,12);path.lineTo(21,12); }
 else if(name=="sun"){ p.drawEllipse(QPointF(12,12),4,4); for(int i=0;i<8;++i){ const QLineF ray=QLineF::fromPolar(7,i*45.0).translated(12,12); p.drawLine(ray.p2(),QLineF::fromPolar(9.5,i*45.0).translated(12,12).p2()); } }
 else if(name=="moon"){ path.moveTo(19.5,14.5); path.cubicTo(18,18.5,14.5,20.5,11,20); path.cubicTo(6.5,19.5,3.5,15.5,4,11); path.cubicTo(4.5,7,7.5,4.2,11,3.8); path.cubicTo(8.8,6.5,8.8,11,11.3,13.6); path.cubicTo(13.6,16,17,16.4,19.5,14.5); }
 else if(name=="monitor"){ p.drawRoundedRect(QRectF(3,4,18,12.5),2,2); p.drawLine(QPointF(12,16.5),QPointF(12,20)); p.drawLine(QPointF(8,20),QPointF(16,20)); }
 p.drawPath(path);
}

const char *kStyleSheet = R"QSS(
QWidget { color:$text$; font-family:"Segoe UI","Microsoft YaHei UI"; font-size:10pt; }
QMainWindow,#AppBody { background:$chrome$; }
QStackedWidget,#ScrollBody { background:transparent; }
#Canvas { background:$bg$; border:1px solid $border$; border-right:none; border-bottom:none; border-top-left-radius:14px; }
QLabel { background:transparent; }

/* window chrome */
#TitleBar { background:$chrome$; }
QToolButton#ThemeButton { border-radius:7px; padding:4px 6px; }
QToolButton#ThemeButton::menu-indicator { image:none; width:0; }
#AppMark { background:$accent$; color:$accentInk$; border-radius:7px; font-size:11pt; font-weight:700; }
#TitleBrand { color:$text$; font-size:9.5pt; font-weight:600; }
#TitleContext { color:$text3$; font-size:9.5pt; }
#TitleSeparator { color:$separator$; font-size:9.5pt; }
QToolButton { border:1px solid transparent; background:transparent; border-radius:6px; padding:4px; }
QToolButton:hover { background:$surface3$; }
QToolButton#WindowMinimize,QToolButton#WindowMaximize,QToolButton#WindowClose { border-radius:0; }
QToolButton#WindowClose:hover { background:#e5534b; }
QToolButton:focus { border:1px solid $accentText$; }

/* navigation */
#NavRail { background:$chrome$; }
#NavSection { color:$text3$; font-size:8.5pt; font-weight:600; padding:14px 12px 6px; }
QPushButton#NavButton { background:transparent; border:none; border-radius:8px; text-align:left; padding:0 12px; min-height:40px; max-height:40px; color:$text2$; font-size:10pt; }
QPushButton#NavButton:hover { background:$navHover$; color:$text$; }
QPushButton#NavButton:focus { background:$navHover$; }
/* the checked background is a Motion::SlidingIndicator gliding behind the buttons */
QPushButton#NavButton:checked { background:transparent; color:$accentText$; font-weight:600; }
QLabel#NavBadge { background:$accent$; color:$accentInk$; border-radius:9px; font-size:8pt; font-weight:700; padding:0 6px; }
QPushButton#EngineBadge { background:$surface$; border:1px solid $border$; border-radius:10px; padding:0; min-height:54px; max-height:54px; text-align:left; }
QPushButton#EngineBadge:hover { border-color:$borderStrong$; }
QPushButton#EngineBadge:focus { border-color:$accentText$; }
QLabel#EngineCaption { color:$text3$; font-size:8.5pt; }
QLabel#EngineName { color:$text$; font-size:9.5pt; font-weight:600; }
QToolButton#DownloaderSwitch { background:transparent; border:none; border-radius:6px; }
QToolButton#DownloaderSwitch:hover { background:$surface3$; }

/* type scale */
QLabel#PageTitle { font-size:17pt; font-weight:650; color:$text$; }
QLabel#PageMeta { color:$text3$; font-size:9.5pt; padding-top:6px; }
QLabel#H1 { font-size:21pt; font-weight:650; color:$text$; }
QLabel#H2 { font-size:13pt; font-weight:600; color:$text$; }
QLabel#Muted { color:$text2$; }
QLabel#Faint { color:$text3$; font-size:9pt; }
QLabel#CardTitle { color:$text$; font-size:10pt; font-weight:600; }
QLabel#MetricValue { color:$text$; font-size:15pt; font-weight:600; }
QLabel#MetricCaption { color:$text3$; font-size:9pt; }
QLabel#StatValue { color:$text$; font-size:10pt; font-weight:600; }

/* surfaces */
#Panel,#SearchPanel,#MetricCard,#DetailHero,#FilterBar { background:$surface$; border:1px solid $border$; border-radius:12px; }
#SearchPanel { border-radius:14px; }
#SearchPanel[focused="true"] { border-color:$accent$; }
#AnimeCard { background:transparent; border:none; }
#AnimeCard:hover QLabel#CardTitle { color:$accentText$; }
#Poster { background:$surface3$; border-radius:10px; color:$text3$; font-size:9pt; font-weight:600; }
QLabel#RatingBadge { background:rgba(10,30,28,184); color:white; border-radius:6px; padding:2px 6px; font-size:8.5pt; font-weight:600; }
#EmptyState { background:transparent; }
#EmptyIcon { background:$accentSoft$; border-radius:28px; }
QLabel#StatIcon { background:$accentSoft$; border-radius:10px; }
QLabel#EmptyTitle { font-size:12pt; font-weight:600; color:$text$; }

/* scrolling */
QScrollArea { border:none; background:transparent; }
QScrollArea > QWidget > QWidget { background:transparent; }
QScrollBar:vertical { background:transparent; width:8px; margin:4px 1px; }
QScrollBar::handle:vertical { background:$scroll$; min-height:36px; border-radius:3px; }
QScrollBar::handle:vertical:hover { background:$scrollHover$; }
QScrollBar:horizontal { background:transparent; height:8px; margin:1px 4px; }
QScrollBar::handle:horizontal { background:$scroll$; min-width:32px; border-radius:3px; }
QScrollBar::add-line,QScrollBar::sub-line { width:0; height:0; }
QScrollBar::add-page,QScrollBar::sub-page { background:none; }

/* inputs */
QLineEdit,QComboBox,QSpinBox,QDoubleSpinBox { background:$surface$; border:1px solid $border$; border-radius:8px; padding:7px 11px; color:$text$; selection-background-color:$accentSoft2$; selection-color:$accentInk$; min-height:20px; }
QLineEdit:hover,QSpinBox:hover,QDoubleSpinBox:hover { border-color:$borderStrong$; }
QLineEdit:focus,QComboBox:focus,QSpinBox:focus,QDoubleSpinBox:focus { border-color:$accent$; }
QLineEdit#SearchBox { font-size:12pt; border:none; background:transparent; padding:12px 4px; }
QLineEdit#InlineFilter { padding-left:10px; }
QComboBox { padding:7px 34px 7px 12px; }
QComboBox:hover { border-color:$borderStrong$; }
QComboBox:on { border-color:$accent$; }
QComboBox:disabled { color:$disabledText$; background:$surface2$; border-color:$border$; }
QComboBox::drop-down { subcontrol-origin:border; subcontrol-position:top right; border:none; width:30px; }
QComboBox::down-arrow { image:url("$imgChevronDown$"); width:14px; height:14px; }
QComboBox::down-arrow:on { image:url("$imgChevronUp$"); }
QComboBox::down-arrow:disabled { width:12px; height:12px; }
QSpinBox,QDoubleSpinBox { padding:7px 36px 7px 12px; }
QAbstractSpinBox QLineEdit,QComboBox QLineEdit { background:transparent; border:none; padding:0; min-height:0; }
QSpinBox::up-button,QDoubleSpinBox::up-button { subcontrol-origin:border; subcontrol-position:top right; width:26px; margin:1px 1px 0 0; border:none; border-left:1px solid $border$; border-top-right-radius:7px; background:$surface2$; }
QSpinBox::down-button,QDoubleSpinBox::down-button { subcontrol-origin:border; subcontrol-position:bottom right; width:26px; margin:0 1px 1px 0; border:none; border-left:1px solid $border$; border-bottom-right-radius:7px; background:$surface2$; }
QSpinBox::up-button:hover,QSpinBox::down-button:hover,QDoubleSpinBox::up-button:hover,QDoubleSpinBox::down-button:hover { background:$accentSoft$; }
QSpinBox::up-button:pressed,QSpinBox::down-button:pressed { background:$accentSoft2$; }
QSpinBox::up-arrow,QDoubleSpinBox::up-arrow { image:url("$imgPlus$"); width:11px; height:11px; }
QSpinBox::down-arrow,QDoubleSpinBox::down-arrow { image:url("$imgMinus$"); width:11px; height:11px; }
QSpinBox:disabled,QDoubleSpinBox:disabled,QLineEdit:disabled { background:$surface2$; color:$disabledText$; border-color:$border$; }
QSpinBox::up-button:disabled,QSpinBox::down-button:disabled { background:$surface2$; }
QComboBox QAbstractItemView { background:$surface$; border:1px solid $border$; selection-background-color:$accentSoft$; selection-color:$accentText$; padding:4px; }

/* buttons */
QPushButton { background:$surface$; border:1px solid $border$; border-radius:8px; padding:7px 14px; min-height:20px; color:$text$; }
QPushButton:hover { background:$surface2$; border-color:$borderStrong$; }
QPushButton:pressed { background:$surface3$; }
QPushButton:disabled { color:$disabledText$; background:$surface2$; border-color:$border$; }
QPushButton:focus { border-color:$accentText$; }
QPushButton#Primary { background:$accent$; color:$accentInk$; border:1px solid $accent$; font-weight:600; }
QPushButton#Primary:hover { background:$accentHover$; border-color:$accentHover$; }
QPushButton#Primary:pressed { background:$accentPressed$; }
QPushButton#Primary:focus { border-color:$accentInk$; }
QPushButton#Primary:disabled { background:$primaryDisabled$; border-color:$primaryDisabled$; color:$primaryDisabledText$; }
QPushButton#Primary:checked { background:$accentSoft$; border-color:$accentSoft2$; color:$accentText$; }
QPushButton#Soft,QPushButton#GroupDownload { background:$accentSoft$; color:$accentText$; border:1px solid transparent; font-weight:600; }
QPushButton#Soft:hover,QPushButton#GroupDownload:hover { background:$accentSoft2$; }
QPushButton#Soft:focus,QPushButton#GroupDownload:focus { border-color:$accentText$; }
QPushButton#Soft:disabled,QPushButton#GroupDownload:disabled { background:$surface2$; color:$disabledText$; }
QPushButton#Ghost,QPushButton#IconButton,QPushButton#BackButton { background:transparent; border:1px solid transparent; color:$text2$; }
QPushButton#Ghost:hover,QPushButton#IconButton:hover,QPushButton#BackButton:hover { background:$surface3$; color:$text$; }
QPushButton#Ghost:focus,QPushButton#IconButton:focus,QPushButton#BackButton:focus { border-color:$borderStrong$; }
QPushButton#Ghost:disabled,QPushButton#IconButton:disabled { background:transparent; }
QPushButton#IconButton { padding:3px; min-height:0; border-radius:7px; }
QPushButton#BackButton { padding:6px 12px 6px 8px; font-weight:600; }
QPushButton#Danger { color:$err$; }
QPushButton#Danger:hover { background:$errSoft$; border-color:$errBorder$; }
QPushButton#Danger:disabled { color:$disabledText$; }
QPushButton#GroupDownload { padding:6px 14px; }
QPushButton#GroupDownload[split="true"] { border-top-right-radius:0; border-bottom-right-radius:0; }
QPushButton#GroupDownloadOptions { background:$accentSoft$; border:1px solid transparent; border-left:1px solid $accentSoft2$; border-top-left-radius:0; border-bottom-left-radius:0; padding:6px 7px; min-height:20px; }
QPushButton#GroupDownloadOptions:hover { background:$accentSoft2$; }
QPushButton#GroupDownloadOptions:focus { border-color:$accentText$; }
QPushButton#GroupDownloadOptions:disabled { background:$surface2$; border-left-color:$border$; }

/* segmented control */
#Segmented { background:$surface3$; border-radius:10px; }
QPushButton#DayTab,QPushButton#SegmentButton { background:transparent; border:1px solid transparent; border-radius:8px; color:$text2$; padding:6px 14px; }
QPushButton#DayTab:hover,QPushButton#SegmentButton:hover { color:$text$; background:$segmentHover$; }
QPushButton#DayTab:checked,QPushButton#SegmentButton:checked { background:transparent; color:$accentText$; font-weight:600; border-color:transparent; }
QPushButton#DayTab:focus,QPushButton#SegmentButton:focus { border-color:$borderStrong$; }

/* resource groups */
#GroupModule { background:$surface$; border:1px solid $border$; border-radius:12px; }
QPushButton#GroupHeader { border:none; background:transparent; text-align:left; padding:14px 8px 14px 14px; font-weight:600; font-size:10.5pt; border-radius:10px; }
QPushButton#GroupHeader:hover,QPushButton#GroupHeader:focus { color:$accentText$; }
#ResourceRow { background:transparent; border-top:1px solid $surface3$; }
#ResourceRow:hover { background:$rowHover$; }
QLabel#EpisodeBadge { background:$surface3$; color:$text$; border-radius:7px; padding:5px 0; font-size:9pt; font-weight:600; }
QLabel#EpisodeBadge[batch="true"] { background:$accentSoft$; color:$accentText$; }
QLabel#ResourceTitle { font-size:9.5pt; color:$text$; }
QLabel#ResourceSources { color:$text3$; font-size:9pt; }

/* tables */
QTableView { background:$surface$; alternate-background-color:$rowHover$; border:1px solid $border$; border-radius:12px; gridline-color:transparent; selection-background-color:$accentSoft$; selection-color:$text$; outline:0; }
QTableView::item { border-bottom:1px solid $surface3$; padding:0 10px; }
QTableView::item:selected { background:$accentSoft$; color:$text$; }
QTableView::indicator { width:16px; height:16px; border:1px solid $indicatorBorder$; background:$indicatorBg$; border-radius:4px; }
QTableView::indicator:checked { background:$accent$; border-color:$accent$; image:url("$imgCheck$"); }
QHeaderView { background:transparent; }
QHeaderView::section { background:$surface$; color:$text3$; border:none; border-bottom:1px solid $border$; padding:11px 10px; font-size:9pt; font-weight:600; }
QHeaderView::section:first { border-top-left-radius:12px; }
QHeaderView::section:last { border-top-right-radius:12px; }
QTableView QTableCornerButton::section { background:$surface$; border:none; }

/* tabs rendered as a segmented control */
QTabWidget::pane { background:transparent; border:none; top:0; }
QTabWidget::tab-bar { left:0; }
QTabBar { background:transparent; }
QTabBar::tab { background:$surface3$; color:$text2$; padding:8px 18px; margin:0; border:1px solid $surface3$; }
QTabBar::tab:first { border-top-left-radius:9px; border-bottom-left-radius:9px; }
QTabBar::tab:last { border-top-right-radius:9px; border-bottom-right-radius:9px; }
QTabBar::tab:selected { background:$segmentThumb$; color:$accentText$; font-weight:600; border-color:$segmentThumbBorder$; border-radius:8px; }
QTabBar::tab:hover:!selected { color:$text$; }

/* checkboxes */
QCheckBox { spacing:8px; padding:4px 0; }
QCheckBox::indicator { width:16px; height:16px; border:1px solid $indicatorBorder$; background:$indicatorBg$; border-radius:4px; }
QCheckBox::indicator:checked { background:$accent$; border-color:$accent$; image:url("$imgCheck$"); }
QCheckBox::indicator:hover { border-color:$accent$; }
QCheckBox::indicator:indeterminate { background:$accentSoft$; image:url("$imgDash$"); }
QCheckBox::indicator:disabled { background:$surface3$; border-color:$border$; }
QCheckBox:disabled { color:$disabledText$; }

/* settings */
QFrame[settingsCard="true"] { background:$surface$; border:1px solid $border$; border-radius:12px; }
QLabel#SettingsCardTitle { font-size:10.5pt; font-weight:600; color:$text$; }
QLabel#SettingTitle { color:$text$; font-size:10pt; }
QLabel#SettingHint { color:$text3$; font-size:9pt; }
#SettingDivider { background:$surface3$; border:none; }
QLabel#DownloaderHint { color:$text2$; font-size:9pt; }
#SaveBar { background:$surface$; border:1px solid $border$; border-radius:12px; }
QPushButton#ChooseDownloader { text-align:left; padding:7px 12px; }

/* downloader popup */
#DownloaderPopup { background:transparent; }
#DownloaderPopupPanel { background:$surface$; border:1px solid $border$; border-radius:12px; }
QLabel#DownloaderPopupHeading { color:$text3$; font-size:9pt; padding:0 10px 6px; }
QPushButton#DownloaderOption { background:transparent; border:1px solid transparent; border-radius:8px; padding:0; min-height:54px; max-height:54px; text-align:left; }
QPushButton#DownloaderOption:hover,QPushButton#DownloaderOption:focus { background:$surface2$; border-color:$border$; }
QPushButton#DownloaderOption:checked { background:$accentSoft$; border-color:$accentSoft2$; }
QLabel#DownloaderOptionIcon { background:$surface2$; border-radius:8px; }
QLabel#DownloaderOptionName { color:$text$; font-weight:600; }
QLabel#DownloaderOptionDescription,QLabel#DownloaderUnconfigured { color:$text3$; font-size:8.5pt; }
QLabel#DownloaderCurrent { background:$accentSoft2$; color:$accentText$; border-radius:5px; padding:3px 6px; font-size:8pt; }
#DownloaderPopupDivider { background:$surface3$; border:0; margin:0 10px; }
QPushButton#DownloaderConfigure { background:transparent; border:0; border-radius:7px; text-align:left; padding:8px 10px; }
QPushButton#DownloaderConfigure:hover,QPushButton#DownloaderConfigure:focus { background:$surface2$; }

/* menus, tooltips and dialogs */
QMenu { background:$surface$; color:$text$; border:1px solid $border$; padding:6px; border-radius:8px; }
QMenu::item { padding:8px 26px 8px 14px; border-radius:6px; }
QMenu::item:selected { background:$accentSoft$; color:$accentText$; }
QMenu::item:disabled { color:$disabledText$; }
QMenu::separator { height:1px; background:$surface3$; margin:5px 8px; }
QMenu::icon { padding-left:8px; }
QToolTip { background:$surface$; color:$text$; border:1px solid $border$; border-radius:8px; padding:8px 10px; }
#TooltipBubble { background:transparent; }
QLabel#TooltipText { background:transparent; color:$text$; font-size:9pt; }
#DialogSurface { background:transparent; }
#DialogPanel { background:$surface$; border:1px solid $border$; border-radius:14px; }
#DialogHeader { background:transparent; }
QLabel#DialogTitle { font-size:12.5pt; font-weight:600; color:$text$; }
QLabel#DialogBody { color:$text2$; font-size:10pt; background:$surface$; }
QLabel#DirectoryNotice { color:$text$; background:$surface2$; border:1px solid $border$; border-radius:8px; padding:12px; }
#DialogClose:hover { background:$surface3$; }
QListWidget#VideoList { background:$surface$; border:1px solid $border$; border-radius:8px; padding:6px; outline:0; }
QListWidget#VideoList::item { padding:10px; border-radius:6px; }
QListWidget#VideoList::item:selected { background:$accentSoft$; color:$accentText$; }
#NoticeInfo { background:$accentSoft$; border-radius:12px; }
QLabel#GuideNumber { background:$accentSoft$; color:$accentText$; border-radius:11px; font-size:9pt; font-weight:700; }
QLabel#GuideEmphasis { color:$text$; font-weight:600; font-size:10pt; }
QFrame#GuideNote { background:$surface2$; border:1px solid $border$; border-radius:10px; }
QLabel#GuideSection { color:$text3$; font-size:9pt; font-weight:600; }
QFrame#UpdatePanel { background:$surface$; border:1px solid $border$; border-radius:14px; }
QFrame#UpdateRow { background:transparent; border:none; border-top:1px solid $surface3$; }
QFrame#UpdateRow[first="true"] { border-top:none; }
QLabel#GuideTile { background:$accentSoft$; border-radius:10px; }
QLabel#GuideTile[tone="warn"] { background:$warnSoft$; }
QLabel#GuideNoteTitle { color:$text$; font-weight:600; font-size:9.5pt; }
QLabel#GuideNoteText { color:$text2$; font-size:9pt; }
#NoticeWarning { background:$warnSoft$; border-radius:12px; }
#NoticeError { background:$errSoft$; border-radius:12px; }
#FilePicker { background:$surface$; }
#FilePicker QTreeView,#FilePicker QListView { border:1px solid $border$; border-radius:8px; padding:4px; background:$surface$; outline:0; }
#FilePicker QTreeView::item,#FilePicker QListView::item { min-height:28px; padding:3px 6px; border:none; }
#FilePicker QTreeView::item:selected,#FilePicker QListView::item:selected { background:$accentSoft$; color:$accentText$; border-radius:4px; }
#FilePicker QHeaderView::section { padding:8px; background:$surface2$; border-radius:0; }
#FilePicker QToolButton { min-width:24px; min-height:24px; }
#FilePicker QToolButton[pickerTool="true"] { background:transparent; border:1px solid transparent; border-radius:8px; padding:5px; min-width:0; min-height:0; }
#FilePicker QToolButton[pickerTool="true"]:hover { background:$surface2$; border-color:$border$; }
#FilePicker QToolButton[pickerTool="true"]:checked { background:$accentSoft$; border-color:$accentSoft2$; }
#FilePicker QToolButton[pickerTool="true"]:pressed { background:$accentSoft2$; }
#FilePicker QToolButton[pickerTool="true"]:focus { border-color:$accent$; }
#FilePicker QToolButton[pickerTool="true"]:disabled { background:transparent; border-color:transparent; }
#FilePicker QSplitter::handle { background:$surface$; width:10px; }
QListView#SelectOptions { background:$surface$; color:$text$; border:1px solid $border$; border-radius:10px; padding:6px; outline:0; }
QListView#SelectOptions::item { border:none; padding:0; }
QPlainTextEdit { background:$surface$; border:1px solid $border$; border-radius:8px; padding:10px; }
)QSS";

class LineIconEngine final : public QIconEngine {
public:
    LineIconEngine(const QString &name, Theme::Role off, Theme::Role on) : m_name(name), m_off(off), m_on(on) {}
    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override {
        QColor color=g_palette.*(state==QIcon::On ? m_on : m_off);
        if(mode==QIcon::Disabled) color=g_palette.disabledText;
        painter->save();
        painter->translate(rect.topLeft());
        painter->scale(rect.width()/24.0,rect.height()/24.0);
        drawGlyph(*painter,m_name,color);
        painter->restore();
    }
    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override {
        return scaledPixmap(size,mode,state,1.0);
    }
    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale) override {
        QPixmap pm(size*scale); pm.setDevicePixelRatio(scale); pm.fill(Qt::transparent);
        QPainter p(&pm); paint(&p,QRect(QPoint(),size),mode,state);
        return pm;
    }
    QSize actualSize(const QSize &size, QIcon::Mode, QIcon::State) override { return size; }
    QIconEngine *clone() const override { return new LineIconEngine(*this); }
    QString key() const override { return QStringLiteral("NexusLineIcon"); }
private:
    QString m_name;
    Theme::Role m_off, m_on;
};

QString colorToken(const QColor &color)
{
    if(color.alpha()==255) return color.name();
    return QStringLiteral("rgba(%1,%2,%3,%4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(color.alpha());
}

// Stylesheet sub-control images cannot be QIcons, so render them per palette into a cache folder.
QString themeImage(const QString &name, const QColor &color)
{
    const QString dir=QDir::temp().filePath(QStringLiteral("anime-nexus-theme"));
    QDir().mkpath(dir);
    const QString path=QDir(dir).filePath(QStringLiteral("%1-%2.png").arg(name,color.name(QColor::HexRgb).mid(1)));
    // Always re-render: a cached file from an older build may hold a different glyph.
    QPixmap pm(48,48); pm.fill(Qt::transparent);
    QPainter p(&pm); p.scale(2,2);
    drawGlyph(p,name,color); p.end();
    pm.save(path);
    return QDir::fromNativeSeparators(path);
}

// $token$ placeholders are resolved from the active palette before the sheet is applied.
QString resolveTokens(const QString &sheet)
{
    const Palette &c=g_palette;
    const QHash<QString,QColor> colors = {
        {"chrome",c.chrome},{"bg",c.bg},{"surface",c.surface},{"surface2",c.surface2},{"surface3",c.surface3},
        {"border",c.border},{"borderStrong",c.borderStrong},{"text",c.text},{"text2",c.text2},{"text3",c.text3},
        {"disabledText",c.disabledText},{"accent",c.accent},{"accentHover",c.accentHover},{"accentPressed",c.accentPressed},
        {"accentInk",c.accentInk},{"accentText",c.accentText},{"accentSoft",c.accentSoft},{"accentSoft2",c.accentSoft2},
        {"primaryDisabled",c.primaryDisabled},{"primaryDisabledText",c.primaryDisabledText},
        {"ok",c.ok},{"warn",c.warn},{"warnSoft",c.warnSoft},{"err",c.err},{"errSoft",c.errSoft},{"errBorder",c.errBorder},
        {"navHover",c.navHover},{"rowHover",c.rowHover},{"separator",c.separator},{"scroll",c.scroll},{"scrollHover",c.scrollHover},
        {"segmentHover",c.segmentHover},{"segmentThumb",c.segmentThumb},{"segmentThumbBorder",c.segmentThumbBorder},{"indicatorBorder",c.indicatorBorder},{"indicatorBg",c.indicatorBg}};
    const QHash<QString,QString> images = {
        {"imgChevronDown",themeImage("chevron-down",c.text2)},{"imgChevronUp",themeImage("chevron-up",c.text2)},
        {"imgPlus",themeImage("plus",c.text2)},{"imgMinus",themeImage("minus",c.text2)},
        {"imgCheck",themeImage("check",c.accentInk)},{"imgDash",themeImage("minus",c.accentText)}};
    static const QRegularExpression placeholder(QStringLiteral("\\$(\\w+)\\$"));
    QString out;out.reserve(sheet.size()+sheet.size()/4);
    qsizetype last=0;
    for(auto it=placeholder.globalMatch(sheet);it.hasNext();) {
        const auto match=it.next();
        const QString key=match.captured(1);
        out+=QStringView(sheet).mid(last,match.capturedStart()-last);
        if(const auto color=colors.constFind(key);color!=colors.cend()) out+=colorToken(color.value());
        else out+=images.value(key,match.captured(0));
        last=match.capturedEnd();
    }
    out+=QStringView(sheet).mid(last);
    return out;
}

void applyPalette(bool initial=false)
{
    const bool dark=g_mode==Theme::Mode::Dark || (g_mode==Theme::Mode::System && systemPrefersDark());
    if(!initial && dark==g_palette.dark) { emit Theme::notifier()->changed(); return; }
    // Restyling repaints everything at once; dissolve from a snapshot instead.
    // The snapshot must be taken while the old palette is still active, since
    // custom-painted widgets read it directly.
    if(!initial && qApp)
        for(auto *window:QApplication::topLevelWidgets())
            if(window->isVisible() && !window->isMinimized() && !(window->windowType()==Qt::Popup || window->windowType()==Qt::ToolTip))
                Motion::crossfadeWindow(window,Motion::Slow);
    g_palette=dark ? darkPalette() : lightPalette();
    if(!qApp)return;
    const Palette &c=g_palette;
    QPalette pal;
    pal.setColor(QPalette::Window,c.bg); pal.setColor(QPalette::WindowText,c.text); pal.setColor(QPalette::Base,c.surface);
    pal.setColor(QPalette::AlternateBase,c.surface2); pal.setColor(QPalette::Text,c.text); pal.setColor(QPalette::Button,c.surface);
    pal.setColor(QPalette::ButtonText,c.text); pal.setColor(QPalette::Highlight,c.accent); pal.setColor(QPalette::HighlightedText,c.accentInk);
    pal.setColor(QPalette::PlaceholderText,c.text3); pal.setColor(QPalette::Link,c.accentText); pal.setColor(QPalette::ToolTipBase,c.surface);
    pal.setColor(QPalette::ToolTipText,c.text);
    for(auto role:{QPalette::Text,QPalette::WindowText,QPalette::ButtonText}) pal.setColor(QPalette::Disabled,role,c.disabledText);
    qApp->setPalette(pal);
    qApp->setStyleSheet(resolveTokens(QString::fromUtf8(kStyleSheet)));
    emit Theme::notifier()->changed();
}
} // namespace

namespace Theme {
const Palette &c() { return g_palette; }
Mode mode() { return g_mode; }

Mode modeFromString(const QString &value)
{
    if(value==QLatin1String("light")) return Mode::Light;
    if(value==QLatin1String("dark")) return Mode::Dark;
    return Mode::System;
}

QString modeToString(Mode mode)
{
    switch(mode) {
    case Mode::Light: return QStringLiteral("light");
    case Mode::Dark: return QStringLiteral("dark");
    case Mode::System: break;
    }
    return QStringLiteral("system");
}

QString modeName(Mode mode)
{
    switch(mode) {
    case Mode::Light: return QStringLiteral("浅色");
    case Mode::Dark: return QStringLiteral("深色");
    case Mode::System: break;
    }
    return QStringLiteral("跟随系统");
}

void setMode(Mode mode)
{
    g_mode=mode;
    applyPalette();
}

namespace { bool g_styled=false; }

Notifier *notifier()
{
    static Notifier *instance=new Notifier(qApp);
    return instance;
}

QFont uiFont(int size, QFont::Weight weight) { QFont f; f.setFamilies({"Segoe UI","Microsoft YaHei UI"}); f.setPointSize(size); f.setWeight(weight); return f; }

QIcon icon(const QString &name, Role role) { return QIcon(new LineIconEngine(name,role,role)); }
QIcon stateIcon(const QString &name) { return QIcon(new LineIconEngine(name,&Palette::text2,&Palette::accentText)); }

void applyTheme(QApplication *app, Mode mode) {
 if(!app)return; app->setStyle(new QuietStyle); app->setFont(uiFont(10));
 // Native roll animations capture a square menu before our popup geometry is set.
 QApplication::setEffectEnabled(Qt::UI_AnimateCombo,false);
 QApplication::setEffectEnabled(Qt::UI_AnimateTooltip,false);
 QApplication::setEffectEnabled(Qt::UI_FadeTooltip,false);
 Ui::installOverlays(app);
 static bool watching=false;
 if(!watching) {
     watching=true;
     QObject::connect(QGuiApplication::styleHints(),&QStyleHints::colorSchemeChanged,notifier(),[] {
         if(g_mode==Mode::System) applyPalette();
     });
 }
 g_mode=mode;
 applyPalette(!g_styled);
 g_styled=true;
}
}
