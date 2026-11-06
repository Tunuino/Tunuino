#include "SensorCalibrationWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QInputDialog>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QTimer>
#include <QTableWidget>
#include <QHeaderView>
namespace tunuino {
SensorCalibrationWidget::SensorCalibrationWidget(SensorKind kind,QWidget *parent):QWidget(parent),kind_(kind){
    setObjectName(QStringLiteral("sensor%1").arg(SensorCalibrationCodec::name(kind)));
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);
    auto *row=new QHBoxLayout;input_=new QComboBox(this);input_->setObjectName("sensorInput");input_->addItem(tr("Disabled"),0);
    auto *custom=new QPushButton(tr("Customise input..."),this);custom->setObjectName("sensorCustomise");row->addWidget(input_,1);row->addWidget(custom);layout->addLayout(row);
    connect(custom,&QPushButton::clicked,this,[this]{chooseInput();});
    connect(input_,&QComboBox::currentIndexChanged,this,[this]{edit();});
    if(kind_==SensorKind::Lambda){
        preset_=new QComboBox(this);preset_->setObjectName("sensorPreset");
        preset_->addItems({tr("Current / custom curve"),"AEM X-Series 30-0300","14point7 Spartan 3 v2 (green output)","Innovate LC-2 (factory output 1)","PLX SM-AFR Gen2"});
        layout->addWidget(preset_);
        connect(preset_,&QComboBox::currentIndexChanged,this,[this](int index){
            if(!index)return;loading_=true;
            const double low[]={0,.5,0,0,0},high[]={0,4.5,5,5,5};
            const double first[]={0,.4990+.1621*.5,.68,.5,10/14.7},second[]={0,.4990+.1621*4.5,1.36,22.39/14.7,20/14.7};
            volts_[0]->setValue(low[index]);volts_[1]->setValue(high[index]);
            values_[0]->setValue(first[index]);values_[1]->setValue(second[index]);loading_=false;edit();
        });
        auto *note=new QLabel(tr("Check the exact controller and configured analog output. Selecting a profile does not select an ECU input or save it."),this);note->setWordWrap(true);layout->addWidget(note);
    }
    const int points=kind_==SensorKind::Clt?3:2;
    auto *table=new QTableWidget(points,kind_==SensorKind::Tps?2:3,this);table->setObjectName("sensorPoints");
    table->setHorizontalHeaderLabels(kind_==SensorKind::Tps?QStringList{tr("Voltage at connector"),tr("Measure")}:
        QStringList{tr("Voltage at connector"),kind_==SensorKind::Map?tr("MAP kPa"):kind_==SensorKind::Clt?tr("Temperature °C"):tr("Lambda"),tr("Measure")});
    QStringList rows;
    for(int i=0;i<points;++i){
        rows<< (kind_==SensorKind::Tps?(i?tr("Throttle open"):tr("Throttle closed")):tr("Point %1").arg(i+1));
        volts_[i]=new QDoubleSpinBox(table);volts_[i]->setObjectName(QStringLiteral("sensorVoltage%1").arg(i));volts_[i]->setRange(0,6);volts_[i]->setDecimals(4);volts_[i]->setSuffix(" V");
        table->setCellWidget(i,0,volts_[i]);connect(volts_[i],QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this]{edit();});
        if(kind_!=SensorKind::Tps){
            values_[i]=new QDoubleSpinBox(table);values_[i]->setObjectName(QStringLiteral("sensorValue%1").arg(i));
            values_[i]->setRange(kind_==SensorKind::Clt?-40:0,kind_==SensorKind::Map?500:kind_==SensorKind::Clt?150:5);
            values_[i]->setDecimals(kind_==SensorKind::Lambda?3:2);table->setCellWidget(i,1,values_[i]);
            connect(values_[i],QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this]{edit();});
        }
        capture_[i]=new QPushButton(tr("Capture"),table);capture_[i]->setObjectName(QStringLiteral("sensorCapture%1").arg(i));table->setCellWidget(i,kind_==SensorKind::Tps?1:2,capture_[i]);
        connect(capture_[i],&QPushButton::clicked,this,[this,i]{
            const int ch=input_->currentData().toInt();
            if(!hasSample())return;
            const int index=kind_==SensorKind::Clt?11+ch:ch-1;
            const int raw=SensorCalibrationCodec::u16(runtime_,61+2*index);
            volts_[i]->setValue(SensorCalibrationCodec::voltage(raw,header_));
        });
    }
    table->setVerticalHeaderLabels(rows);table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);table->setMaximumHeight(points==3?185:145);layout->addWidget(table);
    live_=new QLabel(tr("No live sample"),this);live_->setObjectName("sensorLive");layout->addWidget(live_);
    status_=new QLabel(tr("Connect the ECU to read its calibration."),this);status_->setObjectName("sensorStatus");status_->setWordWrap(true);layout->addWidget(status_);
    save_=new QPushButton(tr("Write to ECU"),this);save_->setObjectName("sensorSave");layout->addWidget(save_,0,Qt::AlignLeft);
    connect(save_,&QPushButton::clicked,this,[this]{save();});
    auto *timer=new QTimer(this);timer->setInterval(500);connect(timer,&QTimer::timeout,this,[this]{refresh();});timer->start();refresh();
}
QString SensorCalibrationWidget::inputName(int channel)const{
    if(!channel)return tr("Disabled");
    // Proteus v0.7 PCB 383558bf: J103, TE 1-776231-2, natural housing.
    // AV sequence maps to raw[0..10]; AT1..4 maps to raw[12..15].
    static const int av[]={13,24,14,25,15,26,16,27,17,28,18},at[]={19,30,20,31};
    const bool temperature=kind_==SensorKind::Clt;
    return tr("%1 — Natural/white J103, pin %2 (%3%4)").arg(SensorCalibrationCodec::name(kind_)).arg(temperature?at[channel-1]:av[channel-1]).arg(temperature?"AT":"AV").arg(channel);
}
void SensorCalibrationWidget::setInput(int channel){
    QSignalBlocker blocker(input_);input_->clear();input_->addItem(tr("Disabled"),0);
    if(channel>0){input_->addItem(inputName(channel),channel);input_->setCurrentIndex(1);}
}
void SensorCalibrationWidget::chooseInput(){
    QStringList choices;choices<<tr("Disabled");const int count=kind_==SensorKind::Clt?4:11;
    for(int i=1;i<=count;++i)choices<<inputName(i);
    bool ok=false;const QString chosen=QInputDialog::getItem(this,tr("Customise sensor input"),tr("Proteus v0.7 connector contact"),choices,input_->currentData().toInt(),false,&ok);
    if(ok){setInput(choices.indexOf(chosen));edit();}
}
void SensorCalibrationWidget::load(const QByteArray &header,const QByteArray &aux,const QByteArray &identity,quint32 generation){
    if(header.size()!=256||aux.size()!=256||identity.size()!=32||!generation)return;
    const bool keep=edited_&&identity_==identity;
    if(identity_!=identity){runtime_.clear();sampleAge_.invalidate();}
    header_=header;aux_=aux;identity_=identity;generation_=generation;
    if(!keep){
        loading_=true;const auto c=SensorCalibrationCodec::decode(kind_,header,aux);
        if(c.channel<0||c.channel>(kind_==SensorKind::Clt?4:11)){loading_=false;available_=false;refresh();return;}
        setInput(c.channel);if(preset_){QSignalBlocker blocker(preset_);preset_->setCurrentIndex(0);}
        for(int i=0;i<(kind_==SensorKind::Clt?3:2);++i){volts_[i]->setValue(SensorCalibrationCodec::voltage(c.raw[i],header));if(values_[i])values_[i]->setValue(c.values[i]);}
        edited_=false;loading_=false;
    }
    refresh();
}
void SensorCalibrationWidget::setAvailable(bool available){available_=available;refresh();}
void SensorCalibrationWidget::updateRaw(const QByteArray &runtime){
    if(runtime.size()!=101||quint8(runtime[0])!=0||runtime.mid(1,31)!=identity_.mid(1,31))return;
    runtime_=runtime;sampleAge_.start();refresh();
}
SensorCalibration SensorCalibrationWidget::configuration()const{
    SensorCalibration c;c.channel=input_->currentData().toInt();
    for(int i=0;i<(kind_==SensorKind::Clt?3:2);++i){c.raw[i]=SensorCalibrationCodec::raw(volts_[i]->value(),header_);if(values_[i])c.values[i]=values_[i]->value();}return c;
}
void SensorCalibrationWidget::edit(){if(loading_)return;edited_=true;refresh();}
void SensorCalibrationWidget::refresh(){
    const bool loaded=header_.size()==256&&aux_.size()==256&&!identity_.isEmpty()&&generation_;
    QString error;QByteArray desired;
    if(loaded){desired=SensorCalibrationCodec::apply(kind_,configuration(),header_,aux_,error);
        if(!desired.isEmpty()&&!SensorCalibrationCodec::permitted(kind_,kind_==SensorKind::Tps?aux_:header_,desired,header_,aux_,error))desired.clear();}
    save_->setEnabled(available_&&loaded&&!desired.isEmpty());
    status_->setText(!loaded?tr("Connect the ECU to read its calibration."):!error.isEmpty()?error:edited_?tr("Unsaved calibration edits. Other ECU settings will be preserved."):tr("Calibration read from ECU."));
    const int channel=input_->currentData().toInt();
    const bool fresh=hasSample();
    for(auto *capture:capture_)if(capture)capture->setEnabled(fresh);
    if(fresh){const int index=kind_==SensorKind::Clt?11+channel:channel-1;const int raw=SensorCalibrationCodec::u16(runtime_,61+2*index);
        live_->setText(tr("Live selected input: %1 V").arg(SensorCalibrationCodec::voltage(raw,header_),0,'f',4));}
    else live_->setText(tr("No fresh ADC sample for the selected input"));
}
bool SensorCalibrationWidget::hasSample()const{
    if(!available_||!input_->currentData().toInt()||header_.size()!=256||runtime_.size()!=101||!sampleAge_.isValid()
        ||sampleAge_.elapsed()>2000||!(quint8(runtime_[40])&1)||runtime_.mid(1,31)!=identity_.mid(1,31))return false;
    const quint32 generation=(quint32(SensorCalibrationCodec::u16(runtime_,57))<<16)|quint32(SensorCalibrationCodec::u16(runtime_,59));
    const int channel=input_->currentData().toInt(),index=kind_==SensorKind::Clt?11+channel:channel-1;
    const int reference=SensorCalibrationCodec::u16(header_,100);
    return generation==generation_&&reference>=2800&&reference<=3600&&SensorCalibrationCodec::u16(runtime_,61+2*index)<=4095;
}
void SensorCalibrationWidget::save(){
    if(!available_||!writeRequested)return;QString error;
    const auto desired=SensorCalibrationCodec::apply(kind_,configuration(),header_,aux_,error);
    if(desired.isEmpty()||!SensorCalibrationCodec::permitted(kind_,kind_==SensorKind::Tps?aux_:header_,desired,header_,aux_,error)){
        QMessageBox::warning(this,tr("Sensor calibration"),error);return;}
    writeRequested(kind_,desired);
}
}
