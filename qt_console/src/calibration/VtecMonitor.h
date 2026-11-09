#pragma once
#include "SensorUi.h"
#include "../tools/ScopeWidget.h"
#include "../tools/ServiceDataCodec.h"
#include <QTimer>
#include <QVBoxLayout>
#include <QElapsedTimer>
#include <QPointer>
namespace tunuino {
class VtecMonitor final:public QWidget {
public:
    ToolExchange exchange;std::function<ToolSnapshot()> snapshot;std::function<bool()> ready;
    explicit VtecMonitor(){setObjectName("vtecMonitor");auto *layout=new QVBoxLayout(this);layout->setContentsMargins(4,4,4,4);layout->addWidget(SensorUi::section(this,tr("VTEC - ECU state")));table_=new QTableWidget(5,2,this);SensorUi::grid(table_);const QStringList labels{tr("Solenoid command"),tr("State"),tr("Engine runtime"),tr("Oil pressure"),tr("Retained VTEC fault")};for(int i=0;i<5;++i){table_->setItem(i,0,new QTableWidgetItem(labels[i]));table_->setItem(i,1,new QTableWidgetItem("---"));}layout->addWidget(table_);state_=new QLabel(tr("ECU state only; no manual actuation. Solenoid command is not physical spool-valve confirmation."),this);state_->setWordWrap(true);layout->addWidget(state_);auto *timer=new QTimer(this);timer->setInterval(250);connect(timer,&QTimer::timeout,this,[this]{poll();});timer->start();}
    QSize sizeHint()const override{return QSize(720,280);}
    void poll(){
        if(!isVisible())return;const auto base=snapshot?snapshot():ToolSnapshot{};
        if(!base.valid()||!VtecCodec::supported(base.identity)){clear(tr("Offline or supporting VTEC firmware unavailable"));return;}
        if(source_.identity!=base.identity||source_.generation!=base.generation){clear(tr("Waiting for current ECU state"));source_=base;}
        if(age_.isValid()&&age_.elapsed()>1500)clear(tr("Stale ECU state"));
        if(waiting_||!exchange||(ready&&!ready()))return;waiting_=true;QPointer<VtecMonitor> self(this);
        if(!exchange(QByteArray("TUNUINO:X",9)+char(9),[self,base](const QByteArray &p){if(!self)return;self->waiting_=false;const auto current=self->snapshot?self->snapshot():ToolSnapshot{};
            if(current.identity!=base.identity||current.generation!=base.generation||p.size()!=24||p[0]!=0||p[1]!=1||quint8(p[2])>1||quint8(p[3])>1||quint8(p[4])>11||quint8(p[5])>1||quint8(p[6])>16||p[7]!=1||quint8(p[20])>1||p.mid(21)!=QByteArray(3,0)||(p[3]&&(!p[2]||p[4]||p[5]))||ServiceDataCodec::u32(p,12)!=base.generation){self->clear(tr("Invalid or previous-session VTEC state"));return;}
            static const QStringList reasons{tr("Active"),tr("Disabled"),tr("Disarmed"),tr("Engine not running"),tr("Coolant temperature gate"),tr("Waiting for minimum engine runtime"),tr("RPM gate"),tr("TPS gate"),tr("MAP gate"),tr("Oil pressure gate / loss"),tr("Required sensor unavailable"),tr("Retained fault: disarm and rearm explicitly")};
            self->age_.start();self->table_->item(0,1)->setText(p[3]?tr("On"):tr("Off"));self->table_->item(1,1)->setText(reasons.value(quint8(p[4])));self->table_->item(2,1)->setText(QString::number(ServiceDataCodec::u32(p,8))+" ms");self->table_->item(3,1)->setText(p[20]?QString::number(ServiceDataCodec::u32(p,16)/1000.0,'f',1)+" kPa":"---");self->table_->item(4,1)->setText(p[5]?tr("Yes"):tr("No"));self->state_->setText(tr("ECU command only; no spool-valve feedback. Generation %1.").arg(base.generation));
        })){waiting_=false;}
    }
private:
    void clear(const QString &text){for(int i=0;i<5;++i)table_->item(i,1)->setText("---");state_->setText(text);}
    QTableWidget *table_;QLabel *state_;QElapsedTimer age_;bool waiting_=false;ToolSnapshot source_;
};
}
