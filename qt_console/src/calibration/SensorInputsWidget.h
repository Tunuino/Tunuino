#pragma once
#include "SensorUi.h"
#include "../protocol/SensorCalibrationCodec.h"
#include "../ecu/EcuPowerState.h"
#include <QElapsedTimer>
#include <QTimer>
#include <QVBoxLayout>
#include <QMessageBox>
#include <functional>
namespace tunuino {
class SensorInputsWidget final:public QWidget {
public:
    enum Mode {Analog,Crank,Cam1,Temperature};
    std::function<void()> openScope;
    explicit SensorInputsWidget(Mode mode,QWidget *parent=nullptr):QWidget(parent),mode_(mode){
        setObjectName(QString("sensorInputs%1").arg(int(mode)));auto *layout=new QVBoxLayout(this);layout->setContentsMargins(4,4,4,4);layout->setSpacing(3);
        auto *bar=SensorUi::toolbar(this);if(mode==Crank||mode==Cam1){auto *scope=bar->addAction(CalibrationAppearance::icon(CalibrationAppearance::Curve),tr("Open Scope (F5)"));connect(scope,&QAction::triggered,this,[this]{if(openScope)openScope();});}
        auto *help=bar->addAction(CalibrationAppearance::icon(CalibrationAppearance::Help),tr("Help"));connect(help,&QAction::triggered,this,[this]{QMessageBox::information(this,title(),tr("Read-only values from the synchronized ECU. No inputs are reassigned here. Connector voltages use the ECU ADC reference and board divider. Values expire after two seconds or a session/generation change. Crank and CAM use fixed conditioned rising-edge acquisition; electrical polarity/sensitivity cannot be changed through the native protocol. The engine sync flag is shared, not a separate CAM level measurement."));});layout->addWidget(bar);layout->addWidget(SensorUi::section(this,title()));
        table_=new QTableWidget(mode==Analog?15:mode==Temperature?2:5,mode==Analog||mode==Temperature?4:2,this);table_->setObjectName("sensorInputReadings");SensorUi::grid(table_);
        if(mode==Analog||mode==Temperature){table_->horizontalHeader()->show();table_->setHorizontalHeaderLabels({tr("Input"),tr("J103 contact"),tr("Stored function"),tr("Live connector V")});}
        for(int row=0;row<table_->rowCount();++row)for(int column=0;column<table_->columnCount();++column)table_->setItem(row,column,new QTableWidgetItem("---"));
        layout->addWidget(table_,1);state_=new QLabel(tr("Offline — no live readings"),this);state_->setWordWrap(true);layout->addWidget(state_);
        auto *timer=new QTimer(this);timer->setInterval(250);connect(timer,&QTimer::timeout,this,[this]{refresh();});timer->start();refresh();
    }
    QString title()const{return mode_==Analog?tr("Analog inputs"):mode_==Temperature?tr("Standard temperature sensors"):mode_==Crank?tr("Digital inputs - Primary trigger"):tr("Digital inputs - CAM 1");}
    QSize sizeHint()const override{return QSize(mode_==Analog?780:680,mode_==Analog?610:mode_==Temperature?240:300);}
    void load(const QByteArray &header,const QByteArray &aux,const QByteArray &identity,quint32 generation){
        if(header.size()!=256||aux.size()!=256||!generation||!EcuPowerState::supportedIdentity(identity)){setAvailable(false);return;}
        if(identity!=identity_||generation!=generation_){runtime_.clear();age_.invalidate();}
        header_=header;aux_=aux;identity_=identity;generation_=generation;refresh();
    }
    void setAvailable(bool available){available_=available;if(!available){runtime_.clear();age_.invalidate();}refresh();}
    void updateRaw(const QByteArray &runtime){
        if(runtime.size()!=101||runtime[0]!=0||runtime.mid(1,31)!=identity_.mid(1,31)||u32(runtime,57)!=generation_){runtime_.clear();age_.invalidate();refresh();return;}
        runtime_=runtime;age_.start();refresh();
    }
private:
    static quint32 u32(const QByteArray &bytes,int at){return (quint32(SensorCalibrationCodec::u16(bytes,at))<<16)|quint32(SensorCalibrationCodec::u16(bytes,at+2));}
    void text(int row,int column,const QString &value){table_->item(row,column)->setText(value);}
    bool fresh()const{return available_&&header_.size()==256&&runtime_.size()==101&&age_.isValid()&&age_.elapsed()<=2000;}
    QString voltage(int index)const{if(!fresh()||!(quint8(runtime_[40])&1)||SensorCalibrationCodec::u16(header_,100)<2800||SensorCalibrationCodec::u16(header_,100)>3600)return "---";const int raw=SensorCalibrationCodec::u16(runtime_,61+2*index);return raw<=4095?QString::number(SensorCalibrationCodec::voltage(raw,header_),'f',4)+" V":QString("---");}
    void refresh(){
        static const int av[]={13,24,14,25,15,26,16,27,17,28,18},at[]={19,30,20,31};
        const bool loaded=header_.size()==256&&aux_.size()==256;
        if(mode_==Analog){
            QStringList assignments[15];if(loaded){for(auto kind:{SensorKind::Map,SensorKind::Clt,SensorKind::Lambda,SensorKind::Tps,SensorKind::Iat,SensorKind::Battery}){const auto c=SensorCalibrationCodec::decode(kind,header_,aux_);const bool thermal=SensorCalibrationCodec::temperature(kind);if(c.channel>0&&c.channel<=(thermal?4:11))assignments[(thermal?11:0)+c.channel-1]<<SensorCalibrationCodec::name(kind);}
                if(aux_[68]==1){const int flags=SensorCalibrationCodec::u16(aux_,70);for(int i=0;i<2;++i){const int channel=quint8(aux_[74+i]);if((flags&(i?256:128))&&channel>=1&&channel<=11)assignments[channel-1]<<(i?"Flex fuel":"Launch");}}}
            for(int row=0;row<15;++row){const bool thermal=row>=11;const int channel=thermal?row-11:row;text(row,0,QString(thermal?"AT%1":"AV%1").arg(channel+1));text(row,1,QString("Natural/white J103 / %1").arg(thermal?at[channel]:av[channel]));text(row,2,loaded?(assignments[row].isEmpty()?tr("Unassigned"):assignments[row].join(", ")):tr("Not read"));text(row,3,voltage(thermal?12+channel:channel));}
        }else if(mode_==Temperature){
            for(int row=0;row<2;++row){const auto kind=row?SensorKind::Clt:SensorKind::Iat;const auto c=loaded?SensorCalibrationCodec::decode(kind,header_,aux_):SensorCalibration{};text(row,0,SensorCalibrationCodec::name(kind));text(row,1,c.channel>=1&&c.channel<=4?QString("Natural/white J103 / %1 (AT%2)").arg(at[c.channel-1]).arg(c.channel):tr("Disabled / not read"));text(row,2,tr("Three-point calibration"));text(row,3,c.channel>=1&&c.channel<=4?voltage(11+c.channel):"---");}
        }else {
            const QStringList labels{tr("Connector"),tr("Acquisition edge"),tr("Trigger capture (stored)"),tr("Engine sync (shared flag)"),tr("Sensitivity")};for(int row=0;row<5;++row)text(row,0,labels[row]);
            text(0,1,mode_==Crank?"J101 / 5 (+), 13 (-)":"J101 / 4 (+), 12 (-)");text(1,1,tr("Conditioned rising edge — fixed"));text(2,1,loaded?(header_[2]!=0?tr("Enabled"):tr("Disabled")):tr("Not read"));text(3,1,fresh()?(quint8(runtime_[40])&8?tr("Synchronized"):tr("Not synchronized")):"---");text(4,1,tr("Hardware conditioning — no software curve"));
        }
        state_->setText(fresh()?tr("Read-only ECU generation %1. Assignments shown are stored ECU values.").arg(generation_):tr("Offline or stale — live values unavailable. Stored assignments are not changed."));
    }
    Mode mode_;QTableWidget *table_;QLabel *state_;QByteArray header_,aux_,identity_,runtime_;quint32 generation_=0;QElapsedTimer age_;bool available_=false;
};
}
