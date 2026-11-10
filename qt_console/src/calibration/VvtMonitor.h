#pragma once
#include "SensorUi.h"
#include "../tools/ScopeWidget.h"
#include "../tools/ServiceDataCodec.h"
#include <QVBoxLayout>
#include <QTimer>
#include <QElapsedTimer>
#include <QPointer>
namespace tunuino {
class VvtMonitor final:public QWidget {
public:
    ToolExchange exchange;std::function<ToolSnapshot()> snapshot;std::function<bool()> ready;
    explicit VvtMonitor(){
        setObjectName("vvtMonitor");auto *layout=new QVBoxLayout(this);layout->setContentsMargins(4,4,4,4);layout->addWidget(SensorUi::section(this,tr("VVT - CAM 1 - ECU state")));
        table_=new QTableWidget(8,2,this);table_->setObjectName("vvtReadings");SensorUi::grid(table_);
        const QStringList names{tr("Fixed target advance"),tr("Measured advance"),tr("Solenoid duty"),tr("State"),tr("Engine runtime"),tr("Feedback age"),tr("Oil pressure"),tr("Retained VVT fault")};
        for(int i=0;i<8;++i){table_->setItem(i,0,new QTableWidgetItem(names[i]));table_->setItem(i,1,new QTableWidgetItem("---"));}layout->addWidget(table_);
        state_=new QLabel(tr("CAM 1 selected edge only. This monitor cannot arm outputs or change the target."),this);state_->setWordWrap(true);layout->addWidget(state_);
        auto *timer=new QTimer(this);timer->setInterval(200);connect(timer,&QTimer::timeout,this,[this]{poll();});timer->start();
    }
    QSize sizeHint()const override{return QSize(720,380);}
    void poll(){
        if(!isVisible())return;const auto base=snapshot?snapshot():ToolSnapshot{};
        if(!base.valid()||!VvtCodec::supported(base.identity)){clear(tr("Offline or VVT firmware unavailable"));return;}
        if(source_.identity!=base.identity||source_.generation!=base.generation){clear(tr("Waiting for current ECU state"));source_=base;}
        if(age_.isValid()&&age_.elapsed()>1500)clear(tr("Stale ECU state"));
        if(waiting_||!exchange||(ready&&!ready()))return;waiting_=true;QPointer<VvtMonitor> self(this);QElapsedTimer requested;requested.start();
        if(!exchange(QByteArray("TUNUINO:X",9)+char(10),[self,base,requested](const QByteArray &p){
            if(!self)return;self->waiting_=false;const auto current=self->snapshot?self->snapshot():ToolSnapshot{};
            if(current.identity!=base.identity||current.generation!=base.generation||requested.elapsed()>1500||p.size()!=40||p[0]!=0||p[1]!=1||quint8(p[2])>1||quint8(p[3])>1||quint8(p[4])>11||quint8(p[5])>1||quint8(p[6])>16||p[7]!=1||ServiceDataCodec::u32(p,8)!=base.generation||quint8(p[18])>1||p[19]||quint8(p[36])>1||p.mid(37)!=QByteArray(3,0)||SensorCalibrationCodec::u16(p,16)>9500||(p[3]&&(!p[2]||p[4]||p[5]||!p[18]))){self->clear(tr("Invalid or previous-session VVT state"));return;}
            static const QStringList reasons{tr("Active"),tr("Disabled"),tr("Disarmed"),tr("Engine not running"),tr("RPM gate"),tr("Coolant gate / unavailable"),tr("Minimum engine runtime"),tr("Oil pressure gate / loss"),tr("Invalid or stale CAM feedback"),tr("Measured position outside configured travel"),tr("Persistent tracking error"),tr("Retained fault: disarm/rearm explicitly")};
            self->age_.start();self->table_->item(0,1)->setText(QString::number(qint16(SensorCalibrationCodec::u16(p,12))/100.0,'f',2)+" °");
            self->table_->item(1,1)->setText(p[18]?QString::number(qint16(SensorCalibrationCodec::u16(p,14))/100.0,'f',2)+" °":"---");
            self->table_->item(2,1)->setText(QString::number(SensorCalibrationCodec::u16(p,16)/100.0,'f',2)+" %");self->table_->item(3,1)->setText(reasons.value(quint8(p[4])));
            self->table_->item(4,1)->setText(QString::number(ServiceDataCodec::u32(p,20))+" ms");self->table_->item(5,1)->setText(p[18]?QString::number(ServiceDataCodec::u32(p,28)/1000.0,'f',1)+" ms":"---");
            self->table_->item(6,1)->setText(p[36]?QString::number(ServiceDataCodec::u32(p,32)/1000.0,'f',1)+" kPa":"---");self->table_->item(7,1)->setText(p[5]?tr("Yes"):tr("No"));
            self->state_->setText(tr("Native CAM 1 feedback and PWM command. Generation %1. No physical solenoid confirmation.").arg(base.generation));
        })){waiting_=false;}
    }
private:
    void clear(const QString &message){for(int i=0;i<8;++i)table_->item(i,1)->setText("---");state_->setText(message);}
    QTableWidget *table_;QLabel *state_;QElapsedTimer age_;bool waiting_=false;ToolSnapshot source_;
};
}
