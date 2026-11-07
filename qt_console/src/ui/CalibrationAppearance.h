#pragma once
#include <QProxyStyle>
#include <QStyleFactory>
#include <QStyleOptionTitleBar>
#include <QMdiSubWindow>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QLinearGradient>
#include <QPointer>
namespace tunuino {
// Original vector glyphs and MDI chrome shared by calibration windows.
namespace CalibrationAppearance {
enum Glyph {Parameters,Open,Save,Defaults,Help,Group,Curve,Wizard};
inline QIcon icon(Glyph kind){QIcon result;for(int size:{16,24,32,64}){QPixmap pix(size,size);pix.fill(Qt::transparent);QPainter p(&pix);p.setRenderHint(QPainter::Antialiasing);p.scale(size/32.0,size/32.0);p.setPen(QPen(QColor("#edf4ed"),1.7,Qt::SolidLine,Qt::SquareCap,Qt::MiterJoin));
    if(kind==Parameters){p.setPen(QPen(QColor("#899b64"),1.4));p.setBrush(QColor("#d8dd91"));p.drawRect(QRectF(6,3,21,26));p.setPen(QPen(QColor("#819763"),1));for(int y=8;y<=25;y+=4)p.drawLine(9,y,24,y);p.drawLine(11,6,11,27);}
    else if(kind==Open){p.setBrush(QColor("#303c42"));QPainterPath a;a.moveTo(3,10);a.lineTo(3,5);a.lineTo(13,5);a.lineTo(16,9);a.lineTo(26,9);a.lineTo(28,24);a.lineTo(4,26);a.closeSubpath();p.drawPath(a);QPainterPath b;b.moveTo(4,14);b.lineTo(29,13);b.lineTo(25,27);b.lineTo(3,27);b.closeSubpath();p.setBrush(QColor("#445456"));p.drawPath(b);}
    else if(kind==Save){p.setBrush(QColor("#35444b"));p.drawRect(QRectF(4,3,24,26));p.setBrush(QColor("#e2e9df"));p.drawRect(QRectF(9,4,13,9));p.setBrush(QColor("#344751"));p.drawRect(QRectF(17,5,3,6));p.setBrush(Qt::NoBrush);p.drawRect(QRectF(9,18,14,10));}
    else if(kind==Defaults){p.setBrush(QColor("#35444b"));p.drawRect(QRectF(4,3,18,25));p.setPen(QColor("#ecf3e9"));p.setFont(QFont("Segoe UI",8,QFont::DemiBold));p.drawText(QRectF(4,5,18,15),Qt::AlignCenter,"101");p.setBrush(Qt::NoBrush);p.setPen(QPen(QColor("#dfc778"),2));p.drawArc(QRectF(16,16,13,13),35*16,290*16);p.drawLine(29,18,28,23);p.drawLine(29,18,24,18);}
    else if(kind==Help){p.setPen(QPen(QColor("#68d5f0"),1));QLinearGradient g(4,3,22,29);g.setColorAt(0,QColor("#71d5ee"));g.setColorAt(1,QColor("#0ba6dd"));p.setBrush(g);p.drawEllipse(QRectF(3,3,26,26));p.setPen(Qt::white);p.setFont(QFont("Segoe UI",19,QFont::DemiBold));p.drawText(QRectF(2,1,28,29),Qt::AlignCenter,"?");}
    else if(kind==Group){p.setPen(QColor("#a4a478"));for(int i=0;i<3;++i){p.setBrush(QColor(i==0?"#e87a67":i==1?"#e1cf55":"#8ec655"));p.drawRect(QRectF(4,4+i*9,24,7));}}
    else if(kind==Curve){p.setPen(QColor("#d9ddd5"));p.drawLine(5,3,5,29);p.drawLine(3,27,29,27);for(int y=8;y<26;y+=6)p.drawLine(4,y,27,y);p.setPen(QPen(QColor("#ef8070"),2));p.drawPolyline(QPolygonF{{7,22},{13,18},{18,19},{25,6}});}
    else {p.setPen(QPen(QColor("#e9b24d"),3));p.drawLine(10,28,22,7);p.setPen(QPen(QColor("#f4d166"),1.5));p.drawEllipse(QRectF(4,4,23,9));p.drawLine(25,1,25,9);p.drawLine(21,5,29,5);}
    p.end();result.addPixmap(pix);}return result;}
class FrameStyle final:public QProxyStyle {
public:
    FrameStyle():QProxyStyle(QStyleFactory::create("Fusion")){}
    int pixelMetric(PixelMetric m,const QStyleOption *o=nullptr,const QWidget *w=nullptr)const override{
        if(qobject_cast<const QMdiSubWindow*>(w)){if(m==PM_TitleBarHeight)return qMax(28,o?o->fontMetrics.height()+12:28);if(m==PM_MdiSubWindowFrameWidth)return 4;}
        return QProxyStyle::pixelMetric(m,o,w);
    }
    // Qt also requests these rectangles without a widget for title-bar drag regions.
    QRect subControlRect(ComplexControl c,const QStyleOptionComplex *o,SubControl sc,const QWidget *w=nullptr)const override{
        const auto *t=qstyleoption_cast<const QStyleOptionTitleBar*>(o);if(c!=CC_TitleBar||!t||(w&&!qobject_cast<const QMdiSubWindow*>(w)))return QProxyStyle::subControlRect(c,o,sc,w);
        const QRect r=t->rect;const int width=qMax(34,r.height()+6);int right=r.right()-2;
        const bool minimized=t->titleBarState&Qt::WindowMinimized,maximized=t->titleBarState&Qt::WindowMaximized;
        const auto slot=[&](SubControl control){const QRect result(right-width+1,r.top(),width,r.height());right-=width;return sc==control?result:QRect();};QRect result;
        if(t->titleBarFlags&Qt::WindowCloseButtonHint){result=slot(SC_TitleBarCloseButton);if(!result.isEmpty())return result;}
        if(t->titleBarFlags&Qt::WindowMaximizeButtonHint){result=slot(maximized?SC_TitleBarNormalButton:SC_TitleBarMaxButton);if(!result.isEmpty())return result;}
        if(t->titleBarFlags&Qt::WindowMinimizeButtonHint){result=slot(minimized?SC_TitleBarNormalButton:SC_TitleBarMinButton);if(!result.isEmpty())return result;}
        if(sc==SC_TitleBarSysMenu)return QRect(r.left()+3,r.top()+2,22,r.height()-4);
        if(sc==SC_TitleBarLabel)return QRect(r.left()+27,r.top(),qMax(0,right-r.left()-28),r.height());return {};
    }
    SubControl hitTestComplexControl(ComplexControl c,const QStyleOptionComplex *o,const QPoint &position,const QWidget *w=nullptr)const override{
        if(c==CC_TitleBar&&(!w||qobject_cast<const QMdiSubWindow*>(w))){
            for(auto control:{SC_TitleBarCloseButton,SC_TitleBarNormalButton,SC_TitleBarMaxButton,SC_TitleBarMinButton,SC_TitleBarSysMenu,SC_TitleBarLabel})if((o->subControls&control)&&subControlRect(c,o,control,w).contains(position)){return control;}return SC_None;
        }return QProxyStyle::hitTestComplexControl(c,o,position,w);
    }
    void drawPrimitive(PrimitiveElement element,const QStyleOption *o,QPainter *p,const QWidget *w=nullptr)const override{
        if(element==PE_FrameWindow&&qobject_cast<const QMdiSubWindow*>(w)){p->save();const bool active=o->state&State_Active;const QColor colors[]={active?QColor("#54d1ee"):QColor("#a6b6bd"),QColor("#edf0e7"),QColor("#c7d1d2"),QColor("#8a9a9d")};for(int i=0;i<4;++i){p->setPen(colors[i]);p->drawRect(o->rect.adjusted(i,i,-i-1,-i-1));}p->restore();return;}
        QProxyStyle::drawPrimitive(element,o,p,w);
    }
    void drawComplexControl(ComplexControl c,const QStyleOptionComplex *o,QPainter *p,const QWidget *w=nullptr)const override{
        const auto *t=qstyleoption_cast<const QStyleOptionTitleBar*>(o);
        if(c!=CC_TitleBar||!t||!qobject_cast<const QMdiSubWindow*>(w)){QProxyStyle::drawComplexControl(c,o,p,w);return;}
        p->save();const bool active=t->state&State_Active;QLinearGradient bg(t->rect.topLeft(),t->rect.bottomLeft());bg.setColorAt(0,QColor(active?"#d3eaf3":"#e0e5e3"));bg.setColorAt(1,QColor(active?"#9bc4d4":"#bec9cb"));p->fillRect(t->rect,bg);p->setPen(QColor("#e6f2f4"));p->drawLine(t->rect.bottomLeft(),t->rect.bottomRight());
        const auto area=[&](SubControl sc){return subControlRect(CC_TitleBar,t,sc,w);};
        if(t->subControls&SC_TitleBarSysMenu){const auto r=area(SC_TitleBarSysMenu);t->icon.paint(p,r.adjusted(2,2,-2,-2));}
        if(t->subControls&SC_TitleBarLabel){p->setPen(QColor(active?"#425b67":"#7e8d90"));QFont f=w->font();f.setPointSizeF(9.5);f.setBold(false);p->setFont(f);const auto r=area(SC_TitleBarLabel);p->drawText(r.adjusted(3,0,-3,0),Qt::AlignVCenter|Qt::AlignLeft,QFontMetrics(f).elidedText(t->text,Qt::ElideRight,r.width()-6));}
        for(auto sc:{SC_TitleBarMinButton,SC_TitleBarMaxButton,SC_TitleBarNormalButton,SC_TitleBarCloseButton}){
            if(!(t->subControls&sc))continue;const auto r=area(sc).adjusted(2,4,-2,-4);if(r.isEmpty())continue;const bool close=sc==SC_TitleBarCloseButton;const bool hover=t->activeSubControls&sc;
            QLinearGradient g(r.topLeft(),r.bottomLeft());g.setColorAt(0,QColor(close&&active?"#edbba9":hover?"#effbff":"#e4eff2"));g.setColorAt(1,QColor(close&&active?"#ba6048":hover?"#a8d8e9":"#a8bfc9"));p->setBrush(g);p->setPen(QColor("#849da9"));p->drawRoundedRect(r,1,1);const QPoint center=r.center();p->setPen(QPen(QColor(close&&active?"#ffffff":"#6d8795"),1.8));
            if(sc==SC_TitleBarMinButton)p->drawLine(center+QPoint(-4,3),center+QPoint(4,3));
            else if(sc==SC_TitleBarMaxButton)p->drawRect(QRect(center-QPoint(4,4),QSize(8,8)));
            else if(sc==SC_TitleBarNormalButton){p->drawRect(QRect(center-QPoint(2,5),QSize(7,7)));p->setBrush(g);p->drawRect(QRect(center-QPoint(5,2),QSize(7,7)));}
            else {p->drawLine(center+QPoint(-4,-4),center+QPoint(4,4));p->drawLine(center+QPoint(-4,4),center+QPoint(4,-4));}
        }p->restore();
    }
};
inline void frame(QMdiSubWindow *window){static QPointer<FrameStyle> shared;if(!shared){shared=new FrameStyle;shared->setParent(qApp);}window->setStyle(shared);window->setWindowIcon(icon(Parameters));window->setWindowFlags(Qt::SubWindow|Qt::WindowTitleHint|Qt::WindowSystemMenuHint|Qt::WindowMinMaxButtonsHint|Qt::WindowCloseButtonHint);}
}
}
