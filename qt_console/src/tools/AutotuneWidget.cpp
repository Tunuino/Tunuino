#include "AutotuneWidget.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QMessageBox>
namespace tunuino {
AutotuneWidget::AutotuneWidget(QWidget *parent):QWidget(parent){auto *layout=new QVBoxLayout(this);auto *note=new QLabel(tr("Collect steady engine data, stop collection, review the proposed VE table, then apply it to the local Fuel Table. This tool never writes while collecting. Permanent ECU writes still require stopped engine and disarmed outputs."),this);note->setWordWrap(true);layout->addWidget(note);auto *ranges=new QGridLayout;
    const auto range=[&](int row,const QString &title,double minimum,double maximum,double initialMin,double initialMax,QDoubleSpinBox *&low,QDoubleSpinBox *&high){ranges->addWidget(new QLabel(title,this),row,0);low=new QDoubleSpinBox(this);high=new QDoubleSpinBox(this);for(auto *s:{low,high})s->setRange(minimum,maximum);low->setValue(initialMin);high->setValue(initialMax);ranges->addWidget(low,row,1);ranges->addWidget(high,row,2);};
    range(0,tr("RPM min / max"),0,20000,800,7000,rpmMin_,rpmMax_);range(1,tr("MAP kPa min / max"),0,500,20,250,mapMin_,mapMax_);range(2,tr("TPS % min / max"),0,100,0,100,tpsMin_,tpsMax_);layout->addLayout(ranges);
    auto *row=new QHBoxLayout;start_=new QPushButton(tr("Start collection"),this);auto *stopButton=new QPushButton(tr("Stop collection"),this);apply_=new QPushButton(tr("Apply proposal to Fuel Table"),this);row->addWidget(start_);row->addWidget(stopButton);row->addWidget(apply_);layout->addLayout(row);
    table_=new QTableWidget(16,16,this);table_->setEditTriggers(QAbstractItemView::NoEditTriggers);table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);layout->addWidget(table_,1);state_=new QLabel(this);state_->setWordWrap(true);layout->addWidget(state_);
    connect(start_,&QPushButton::clicked,this,[this]{start();});connect(stopButton,&QPushButton::clicked,this,[this]{stop();});connect(apply_,&QPushButton::clicked,this,[this]{apply();});refresh();
}
void AutotuneWidget::start(){if(collecting_||!snapshot)return;if(canStart&&!canStart()){state_->setText(tr("Another ECU operation is in progress."));return;}const auto base=snapshot();if(!base.valid()){state_->setText(tr("Connect the ECU and wait for a complete configuration read."));return;}
    if(rpmMin_->value()>=rpmMax_->value()||mapMin_->value()>=mapMax_->value()||tpsMin_->value()>=tpsMax_->value()){state_->setText(tr("Each minimum must be below its maximum."));return;}
    const int flags=SensorCalibrationCodec::u16(base.pages[3],70);
    if(base.pages[3][68]!=1||!(flags&4)||(flags&~7)||base.pages[0][103]!=0){state_->setText(tr("This implementation requires an enabled ECU lambda target and no closed-loop/transient/trim/fuel-correction strategies. Their effects are not silently treated as VE error."));return;}
    base_=base;counts_.fill(0);ratios_.fill(0);accepted_=rejected_=0;previousTime_=-1;invalidated_=false;collecting_=true;clock_.start();start_->setEnabled(false);for(auto *s:{rpmMin_,rpmMax_,mapMin_,mapMax_,tpsMin_,tpsMax_})s->setEnabled(false);if(busyChanged)busyChanged(true);refresh();
}
void AutotuneWidget::stop(){if(!collecting_)return;collecting_=false;start_->setEnabled(true);for(auto *s:{rpmMin_,rpmMax_,mapMin_,mapMax_,tpsMin_,tpsMax_})s->setEnabled(true);if(busyChanged)busyChanged(false);refresh();}
void AutotuneWidget::disconnected(){if(base_.valid())invalidated_=true;stop();refresh();}
void AutotuneWidget::sample(const std::array<double,LogChannelCount> &v,double target){
    if(!collecting_)return;const auto current=snapshot?snapshot():ToolSnapshot{};
    if(!current.valid()||current.identity!=base_.identity||current.generation!=base_.generation||current.pages!=base_.pages){disconnected();return;}
    const double now=clock_.elapsed()/1000.0;
    if(previousTime_>=0&&now-previousTime_<.1)return;
    bool valid=std::isfinite(target)&&target>=.65&&target<=1.3;
    for(int i:{0,1,2,3,4})valid&=std::isfinite(v[i]);
    valid&=v[0]>=rpmMin_->value()&&v[0]<=rpmMax_->value()&&v[1]>=tpsMin_->value()&&v[1]<=tpsMax_->value()&&v[2]>=mapMin_->value()&&v[2]<=mapMax_->value()&&v[3]>=.65&&v[3]<=1.5&&v[4]>=70&&v[4]<=130;
    const bool transient=previousTime_<0||now-previousTime_>2||!valid||std::abs(v[0]-previous_[0])>150||std::abs(v[1]-previous_[1])>2||std::abs(v[2]-previous_[2])>5;
    previous_=v;previousTime_=now;if(transient)stableSince_=now;
    if(!valid||now-stableSince_<2){++rejected_;refresh();return;}
    const auto &page=base_.pages[1];int rpm=0,load=0;double rpmDistance=1e20,loadDistance=1e20;
    const double firstRpm=SensorCalibrationCodec::u16(page,0),lastRpm=SensorCalibrationCodec::u16(page,30),firstLoad=SensorCalibrationCodec::u16(page,32)/100.0,lastLoad=SensorCalibrationCodec::u16(page,62)/100.0;
    if(v[0]<firstRpm||v[0]>lastRpm||v[2]<firstLoad||v[2]>lastLoad){++rejected_;refresh();return;}
    for(int i=0;i<16;++i){const double dr=std::abs(v[0]-SensorCalibrationCodec::u16(page,2*i)),dl=std::abs(v[2]-SensorCalibrationCodec::u16(page,32+2*i)/100.0);if(dr<rpmDistance){rpmDistance=dr;rpm=i;}if(dl<loadDistance){loadDistance=dl;load=i;}}
    const int cell=load*16+rpm;if(counts_[cell]<100000){ratios_[cell]+=v[3]/target;++counts_[cell];++accepted_;}refresh();
}
QByteArray AutotuneWidget::proposal()const{if(!base_.valid()||invalidated_)return {};auto bytes=base_.pages[1];bool changed=false;
    for(int i=0;i<256;++i)if(counts_[i]>=5){const double ratio=ratios_[i]/counts_[i];const double factor=qBound(.9,1+(ratio-1)*.5,1.1);const int old=SensorCalibrationCodec::u16(bytes,64+2*i);if(old<=0)continue;const int next=qBound(1,int(std::round(old*factor)),30000);SensorCalibrationCodec::put(bytes,64+2*i,next);changed|=next!=old;}
    return changed?bytes:QByteArray{};
}
void AutotuneWidget::refresh(){const auto next=proposal();apply_->setEnabled(!collecting_&&!invalidated_&&!next.isEmpty());
    state_->setText(invalidated_?tr("ECU/base changed. Proposal cannot be applied; start a new collection."):tr("%1 accepted, %2 rejected. Warm CLT 70–130 °C; two seconds stable; five samples per cell; 50% correction strength, limited to ±10% per review.").arg(accepted_).arg(rejected_));
    if(!base_.valid())return;const auto &page=next.isEmpty()?base_.pages[1]:next;
    for(int i=0;i<16;++i){table_->setHorizontalHeaderItem(i,new QTableWidgetItem(QString::number(SensorCalibrationCodec::u16(page,2*i))));table_->setVerticalHeaderItem(15-i,new QTableWidgetItem(QString::number(SensorCalibrationCodec::u16(page,32+2*i)/100.0)));}
    for(int i=0;i<256;++i){const int old=SensorCalibrationCodec::u16(base_.pages[1],64+2*i),value=SensorCalibrationCodec::u16(page,64+2*i);auto *item=new QTableWidgetItem(QString::number(value/100.0,'f',2));item->setToolTip(tr("Base %1; accepted samples %2").arg(old/100.0).arg(counts_[i]));if(value!=old)item->setBackground(QColor("#3b6440"));table_->setItem(15-i/16,i%16,item);}
}
void AutotuneWidget::apply(){if(collecting_||invalidated_||!snapshot||!applyToEditor)return;const auto current=snapshot();if(current.identity!=base_.identity||current.generation!=base_.generation||current.pages!=base_.pages){disconnected();return;}const auto next=proposal();if(next.isEmpty())return;
    if(QMessageBox::question(this,tr("Autotune"),tr("Apply the reviewed proposal to the local Fuel Table? This does not write the ECU. Use Write to ECU after stopping the engine."),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)==QMessageBox::Yes){if(applyToEditor(base_,next)){state_->setText(tr("Proposal applied to local Fuel Table. Review before a separate ECU write."));apply_->setEnabled(false);}else state_->setText(tr("Fuel Table or ECU changed. Local edits were preserved."));}}
}
