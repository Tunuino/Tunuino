#include "TuneDisplayWidget.h"
#include "../protocol/SensorCalibrationCodec.h"
#include <QPainter>
#include <QLinearGradient>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QSettings>
#include <QTimer>
#include <QLocale>
#include <QResizeEvent>
#include <functional>
#include <algorithm>
namespace tunuino {
namespace {
constexpr double Missing=std::numeric_limits<double>::quiet_NaN();
QString title(int channel){
    if(channel<0)return QObject::tr("None");
    if(channel==7)return QObject::tr("Target lambda");
    const QString name=QString::fromLatin1(LogNames[channel]);
    if(channel==3)return name;
    const QString unit=(channel==4||channel==5)?QStringLiteral("°C"):QString::fromLatin1(LogUnits[channel]);
    return QStringLiteral("%1 [%2]").arg(name,unit);
}
QString number(int channel,double value){
    if(!std::isfinite(value))return QStringLiteral("---");
    const int decimals=channel==0?0:channel==3||channel==7?3:channel==6?2:1;
    return QLocale().toString(value,'f',decimals);
}
QFont fittedFont(const QString &text,QRectF rect,int desired,bool bold=true){
    QFont f(QStringLiteral("Consolas"));f.setStyleHint(QFont::Monospace);f.setBold(bold);f.setPixelSize(qMax(9,desired));
    const double width=QFontMetricsF(f).horizontalAdvance(text);
    if(width>rect.width())f.setPixelSize(qMax(9,int(f.pixelSize()*rect.width()/width)));
    return f;
}
class Readout final:public QWidget {
public:
    explicit Readout(QWidget *parent):QWidget(parent){setFocusPolicy(Qt::StrongFocus);setToolTip(tr("Double-click or right-click to select a channel. Enter also opens channel selection."));}
    QString label=tr("None"),value=QStringLiteral("---");
    std::function<void()> choose;
protected:
    void paintEvent(QPaintEvent*)override{
        QPainter p(this);p.fillRect(rect(),QColor("#101112"));
        const int h=qBound(22,height()/4,38);QRectF header(0,0,width(),h);
        QLinearGradient background(header.topLeft(),header.bottomLeft());background.setColorAt(0,QColor("#626265"));background.setColorAt(1,QColor("#46474a"));p.fillRect(header,background);
        p.setPen(QColor("#efefef"));p.setFont(fittedFont(label,header.adjusted(7,0,-7,0),h*.67));p.drawText(header.adjusted(7,0,-7,0),Qt::AlignVCenter|Qt::AlignLeft,label);
        const QRectF body(7,h+2,width()-14,height()-h-4);
        p.setPen(QColor("#6dff00"));p.setFont(fittedFont(value,body,qMin(int(body.height()*.62),width()/5)));p.drawText(body,Qt::AlignCenter,value);
        p.setPen(hasFocus()?QColor("#e6b34d"):QColor("#77777a"));p.drawRect(rect().adjusted(0,0,-1,-1));
    }
    void contextMenuEvent(QContextMenuEvent *event)override{if(choose)choose();event->accept();}
    void mouseDoubleClickEvent(QMouseEvent *event)override{if(event->button()==Qt::LeftButton&&choose)choose();}
    void keyPressEvent(QKeyEvent *event)override{if(event->key()==Qt::Key_Return||event->key()==Qt::Key_Enter||event->key()==Qt::Key_Space){if(choose)choose();event->accept();}else QWidget::keyPressEvent(event);}
};
class LambdaBars final:public QWidget {
public:
    explicit LambdaBars(QWidget *parent):QWidget(parent){setToolTip(tr("LAM.: measured lambda. TRGT: enabled target curve read from the ECU. Scale 0.60–1.40; out-of-range values keep their numeric reading."));}
    double measured=Missing,target=Missing;
protected:
    void paintEvent(QPaintEvent*)override{
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.fillRect(rect(),QColor("#101112"));
        const double labels=qBound(110.0,width()*.245,210.0),left=labels+9,right=width()-22;
        const double line=(height()-24)/2.0,barHeight=qMax(14.0,line*.56);
        const QString names[]={QStringLiteral("LAM."),QStringLiteral("TRGT")};const double values[]={measured,target};
        for(int row=0;row<2;++row){
            const double top=row*line+6;QRectF label(5,top,labels*.47,line-4),numeric(labels*.47,top,labels*.53,line-4);
            p.setPen(QColor("#eeeeee"));p.setFont(fittedFont(names[row],label,line*.49));p.drawText(label,Qt::AlignVCenter|Qt::AlignLeft,names[row]);
            const auto text=number(3,values[row]);p.setPen(QColor("#6dff00"));p.setFont(fittedFont(text,numeric,line*.46));p.drawText(numeric,Qt::AlignCenter,text);
            QRectF bar(left,top+(line-barHeight)/2,right-left,barHeight);
            QLinearGradient gradient(bar.topLeft(),bar.topRight());gradient.setColorAt(0,QColor("#36d6eb"));gradient.setColorAt(.5,QColor("#0784ee"));gradient.setColorAt(1,QColor("#0635e8"));p.fillRect(bar,gradient);
            p.setPen(QColor("#82888e"));p.drawRect(bar);
            for(int i=0;i<=8;++i){const double x=bar.left()+i*bar.width()/8;p.drawLine(QPointF(x,bar.top()),QPointF(x,bar.bottom()));
                if(row==1){const QString tick=QLocale().toString(.6+i*.1,'f',2);QRectF box(x-23,bar.bottom()+3,46,22);p.setFont(fittedFont(tick,box,qMin(15,width()/37),false));p.setPen(QColor("#dedede"));p.drawText(box,Qt::AlignCenter,tick);p.setPen(QColor("#82888e"));}}
            if(std::isfinite(values[row])){const double x=bar.left()+qBound(0.0,(values[row]-.6)/.8,1.0)*bar.width();
                p.setPen(QPen(QColor("#ffdd22"),3));p.drawLine(QPointF(x,bar.top()-5),QPointF(x,bar.bottom()+5));
                p.fillRect(QRectF(x-4,bar.top()-4,8,bar.height()*.65),QColor("#ffdd22"));}
        }
    }
};
}
double ecuTargetLambda(const QByteArray &aux,const QByteArray &runtime){
    if(aux.size()!=256||aux[0]!=1||aux[68]!=1||!(SensorCalibrationCodec::u16(aux,70)&4)
        ||runtime.size()!=101||quint8(runtime[0])!=0||!(quint8(runtime[39])&2))return Missing;
    int x[3],y[3];for(int i=0;i<3;++i){x[i]=SensorCalibrationCodec::u16(aux,114+2*i);y[i]=SensorCalibrationCodec::u16(aux,120+2*i);
        if(x[i]<1000||x[i]>50000||y[i]<650||y[i]>1300||(i&&x[i]<=x[i-1]))return Missing;}
    const quint32 map=(quint32(SensorCalibrationCodec::u16(runtime,45))<<16)|quint32(SensorCalibrationCodec::u16(runtime,47));
    if(map>500000)return Missing;
    const int load=int(map/10);if(load<=x[0])return y[0]/1000.0;if(load>=x[2])return y[2]/1000.0;
    const int i=load<x[1]?0:1;return (y[i]+qint64(y[i+1]-y[i])*(load-x[i])/(x[i+1]-x[i]))/1000.0;
}
TuneDisplayWidget::TuneDisplayWidget(QWidget *parent):QWidget(parent){
    setObjectName(QStringLiteral("TuneDisplay"));setMinimumSize(420,320);setWindowTitle(tr("Tune Display"));
    channels_.fill(-1);readings_.fill(Missing);
    const QStringList saved=QSettings().value(QStringLiteral("tuneDisplay/channels")).toStringList();
    if(saved.size()==9){std::array<int,9> parsed;bool valid=true;for(int i=0;i<9;++i){bool ok=false;parsed[i]=saved[i].toInt(&ok);valid&=ok&&parsed[i]>=-1&&parsed[i]<=7;}if(valid)channels_=parsed;}
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(3,3,3,3);layout->setSpacing(3);
    bars_=new LambdaBars(this);bars_->setFixedHeight(125);layout->addWidget(bars_);
    auto *grid=new QGridLayout;grid->setSpacing(1);layout->addLayout(grid,1);
    for(int i=0;i<9;++i){auto *cell=new Readout(this);cell->setObjectName(QStringLiteral("tuneReadout%1").arg(i));cell->choose=[this,i]{chooseChannel(i);};cells_[i]=cell;grid->addWidget(cell,i/3,i%3);}
    for(int i=0;i<3;++i){grid->setRowStretch(i,1);grid->setColumnStretch(i,1);}
    auto *timer=new QTimer(this);timer->setInterval(250);connect(timer,&QTimer::timeout,this,[this]{if(sampleAge_.isValid()&&sampleAge_.elapsed()>2000)clearReadings();});timer->start();refresh();
}
void TuneDisplayWidget::setConnected(bool connected){connected_=connected;if(!connected)clearReadings();}
void TuneDisplayWidget::setReadings(const std::array<double,LogChannelCount> &values,double target){
    if(!connected_)return;
    for(int i=0;i<7;++i)readings_[i]=std::isfinite(values[i])?values[i]:Missing;
    readings_[7]=std::isfinite(target)&&target>0?target:Missing;sampleAge_.start();refresh();
}
void TuneDisplayWidget::clearReadings(){readings_.fill(Missing);sampleAge_.invalidate();refresh();}
int TuneDisplayWidget::channel(int slot)const{return slot>=0&&slot<9?channels_[slot]:-1;}
double TuneDisplayWidget::reading(int channel)const{return channel>=0&&channel<8?readings_[channel]:Missing;}
bool TuneDisplayWidget::assignChannel(int slot,int channel){
    if(slot<0||slot>=9||channel< -1||channel>7)return false;channels_[slot]=channel;
    QStringList saved;for(int c:channels_)saved<<QString::number(c);QSettings().setValue(QStringLiteral("tuneDisplay/channels"),saved);refresh();return true;
}
void TuneDisplayWidget::chooseChannel(int slot){
    QMenu menu(this);for(int channel=-1;channel<8;++channel){auto *action=menu.addAction(title(channel));action->setCheckable(true);action->setChecked(channels_[slot]==channel);connect(action,&QAction::triggered,this,[this,slot,channel]{assignChannel(slot,channel);});}
    menu.exec(cells_[slot]->mapToGlobal(QPoint(5,5)));
}
void TuneDisplayWidget::refresh(){
    auto *bars=static_cast<LambdaBars*>(bars_);bars->measured=readings_[3];bars->target=readings_[7];bars->update();
    for(int i=0;i<9;++i){auto *cell=static_cast<Readout*>(cells_[i]);cell->label=title(channels_[i]);cell->value=number(channels_[i],reading(channels_[i]));cell->setAccessibleName(cell->label);cell->setAccessibleDescription(cell->value);cell->update();}
}
void TuneDisplayWidget::resizeEvent(QResizeEvent *event){QWidget::resizeEvent(event);bars_->setFixedHeight(qBound(105,int(height()*.24),160));}
}
