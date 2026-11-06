#include "OutputTestWidget.h"
#include <QFormLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QTimer>
#include <QCloseEvent>
namespace tunuino {
namespace {quint32 u32(const QByteArray &b,int i){return quint32(SensorCalibrationCodec::u16(b,i))<<16|quint32(SensorCalibrationCodec::u16(b,i+2));}}
OutputTestWidget::OutputTestWidget(QWidget *parent):QWidget(parent){auto *layout=new QFormLayout(this);setObjectName("outputTest");auto *note=new QLabel(tr("Test physical output pulses with the engine stopped. Confirm the selected wiring before testing. ECU timer limits: LS 20 ms, IGN 5 ms. Cancelling stops further cycles; the current bounded pulse ends in the ECU."),this);note->setWordWrap(true);layout->addRow(note);
    output_=new QComboBox(this);
    // Contacts are the same audited J102 mapping as the assignment inspector.
    const int pins[]={3,15,4,16,5,6,7,19,8,20,9,21,10,11,12,23,35,34,22,33,32,31,30,29,28,27,26,25};
    for(int i=0;i<28;++i)output_->addItem(QString("%1 — J102 / %2").arg(i<16?QString("LS%1").arg(i+1):QString("IGN%1").arg(i-15)).arg(pins[i]),i);
    cycles_=new QSpinBox(this);cycles_->setRange(1,100);cycles_->setValue(1);on_=new QDoubleSpinBox(this);on_->setRange(.1,20);on_->setDecimals(2);on_->setValue(1);on_->setSuffix(" ms");off_=new QSpinBox(this);off_->setRange(150,10000);off_->setValue(500);off_->setSuffix(" ms");
    layout->addRow(tr("Output"),output_);layout->addRow(tr("Number of cycles"),cycles_);layout->addRow(tr("On time"),on_);layout->addRow(tr("Off time"),off_);test_=new QPushButton(tr("Test"),this);test_->setObjectName("outputTestStart");auto *cancel=new QPushButton(tr("Cancel further cycles"),this);layout->addRow(test_,cancel);state_=new QLabel(tr("No output is active from this tool."),this);state_->setWordWrap(true);layout->addRow(state_);
    connect(output_,&QComboBox::currentIndexChanged,this,[this]{on_->setMaximum(output_->currentData().toInt()<16?20:5);});connect(test_,&QPushButton::clicked,this,[this]{start();});connect(cancel,&QPushButton::clicked,this,[this]{if(busy())stop(tr("Further cycles cancelled. Current pulse remains bounded by the ECU timer."));});auto *timer=new QTimer(this);timer->setInterval(25);connect(timer,&QTimer::timeout,this,[this]{pump();});timer->start();clock_.start();
}
void OutputTestWidget::start(){if(busy()||!snapshot)return;if(canStart&&!canStart()){state_->setText(tr("Another ECU operation is in progress."));return;}base_=snapshot();if(!base_.valid()){state_->setText(tr("Connect the ECU and wait for complete synchronization."));return;}
    if(QMessageBox::question(this,tr("Test outputs"),tr("Pulse %1? Verify the wiring and keep the engine stopped.").arg(output_->currentText()),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
    if(snapshot().identity!=base_.identity||snapshot().generation!=base_.generation)return;
    channel_=output_->currentData().toInt();duration_=int(std::round(on_->value()*1000));remaining_=cycles_->value();offMs_=off_->value();phase_=1;deadline_=clock_.elapsed()+3000;test_->setEnabled(false);output_->setEnabled(false);cycles_->setEnabled(false);on_->setEnabled(false);off_->setEnabled(false);if(busyChanged)busyChanged(true);pump();
}
void OutputTestWidget::pump(){if(!busy())return;if(!snapshot||snapshot().identity!=base_.identity||snapshot().generation!=base_.generation||clock_.elapsed()>deadline_){stop(tr("Test stopped: ECU changed, disconnected or timed out. No pulse is retried."));return;}
    if(phase_==6){if(clock_.elapsed()<next_)return;if(remaining_==0)phase_=7;else phase_=1;deadline_=clock_.elapsed()+3000;}
    if(waiting_||!exchange)return;QByteArray q;
    if(phase_==1)q="TUNUINO?";else if(phase_==2){q=QByteArray(5,0);q[0]='O';q[4]=100;}else if(phase_==3)q="TUNUINO:V";else if(phase_==4||phase_==7){q="TUNUINO:X";q.append(char(0));}else if(phase_==5){q="TUNUINO:X";QByteArray pulse(8,0);pulse[0]=4;pulse[1]=char(channel_);SensorCalibrationCodec::put(pulse,2,duration_);pulse.replace(4,4,"TEST");q+=pulse;}
    waiting_=true;if(!exchange(q,[this](const QByteArray &b){waiting_=false;receive(b);}))waiting_=false;
}
void OutputTestWidget::receive(const QByteArray &b){if(!busy())return;bool valid=false;
    if(phase_==1)valid=b==base_.identity;
    else if(phase_==2){valid=b.size()==101&&b[0]==0&&b.mid(1,31)==base_.identity.mid(1,31)&&u32(b,57)==base_.generation
        &&(quint8(b[40])&1)&&!(quint8(b[40])&2)&&!(quint8(b[39])&1)&&u32(b,41)==0&&u32(b,97)==0;
        if(valid){const int raw=SensorCalibrationCodec::u16(b,83);valid=raw<4095&&raw*(3.3*9.2/4095.0)>=6.0;}}
    else if(phase_==3)valid=b.size()==16&&b[0]==0&&b[1]==1&&b[2]==0&&u32(b,4)==0&&u32(b,8)==base_.generation;
    else if(phase_==4||phase_==7)valid=b.size()==24&&b[0]==0&&b[1]==1&&b[14]==1&&(b[2]==0||b[2]==3)&&!(quint8(b[3])&~2u)&&u32(b,4)==0&&u32(b,8)==0;
    else if(phase_==5)valid=b.size()==1&&b[0]==0;
    if(!valid){stop(tr("ECU refused the test or the engine/power/output checks failed. No pulse is retried."));return;}
    if(phase_==7){stop(tr("Test completed; final ECU state has no active or pending outputs."));return;}
    if(phase_==5){--remaining_;phase_=6;next_=clock_.elapsed()+offMs_;deadline_=next_+3000;state_->setText(tr("Pulse accepted by ECU; %1 cycles remaining.").arg(remaining_));}else{++phase_;deadline_=clock_.elapsed()+3000;}
}
void OutputTestWidget::stop(const QString &message){phase_=0;waiting_=false;test_->setEnabled(true);output_->setEnabled(true);cycles_->setEnabled(true);on_->setEnabled(true);off_->setEnabled(true);state_->setText(message);if(busyChanged)busyChanged(false);}
void OutputTestWidget::closeEvent(QCloseEvent *event){if(busy())stop(tr("Further cycles cancelled by closing the tool."));QWidget::closeEvent(event);}
}
