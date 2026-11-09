#pragma once
#include "DbwSignals.h"
#include "../calibration/SensorUi.h"
#include <QDoubleSpinBox>
#include <QVBoxLayout>
#include <QTimer>
#include <QMessageBox>
#include <QInputDialog>
#include <QSignalBlocker>
#include <memory>

namespace tunuino {
class DbwWidget final:public QWidget {
public:
    std::function<void()> preparePrimaryTps;
    explicit DbwWidget(std::shared_ptr<DbwSignals> model,bool monitor=false):model_(std::move(model)),monitor_(monitor){
        setObjectName(monitor?"dbwSignalMonitor":"dbwSignalSetup");auto *layout=new QVBoxLayout(this);layout->setContentsMargins(4,4,4,4);layout->setSpacing(3);
        auto *bar=SensorUi::toolbar(this);auto *help=bar->addAction(CalibrationAppearance::icon(CalibrationAppearance::Help),tr("Help"));
        connect(help,&QAction::triggered,this,[this]{QMessageBox::information(this,tr("DBW signal commissioning"),tr("Configure independent main/check inputs for the pedal and throttle. Capture their actual 0% and 100% positions with the engine stopped and all outputs disarmed. Inverted signals are supported. Each signal needs at least 1 V of travel. Do not force a powered throttle.\n\nThis window diagnoses sensor signals only. The firmware has no DBW H-bridge driver, automatic motor calibration or redundant-sensor safety controller. Project calibration is local; only the primary TPS can be prepared in its existing ECU sensor editor."));});layout->addWidget(bar);
        layout->addWidget(SensorUi::section(this,monitor?tr("DBW - Signal diagnostics"):tr("DBW - APS / TPS calibration")));
        table_=new QTableWidget(4,4,this);table_->setObjectName(monitor?"dbwDiagnostics":"dbwCalibration");SensorUi::grid(table_);table_->horizontalHeader()->show();table_->setHorizontalHeaderLabels(monitor?QStringList{tr("Signal"),tr("Voltage"),tr("Position"),tr("Status")}:QStringList{tr("Signal"),tr("Input"),tr("0% voltage"),tr("100% voltage")});
        const QStringList names{tr("APS main"),tr("APS check"),tr("TPS main"),tr("TPS check")};
        for(int i=0;i<4;++i){table_->setItem(i,0,new QTableWidgetItem(names[i]));if(monitor){for(int col=1;col<4;++col)table_->setItem(i,col,new QTableWidgetItem("---"));continue;}
            auto *input=new QComboBox(table_);input->setObjectName(QString("dbwInput%1").arg(i));input->addItem(tr("Disabled"),0);input->addItem(tr("Customise..."),-1);SensorUi::choices(input);table_->setCellWidget(i,1,input);inputs_[i]=input;
            connect(input,&QComboBox::currentIndexChanged,this,[this,i,input](int){if(loading_)return;int channel=input->currentData().toInt();if(channel==-1){QStringList choices{tr("Disabled")};const int contacts[]={13,24,14,25,15,26,16,27,17,28,18};for(int ch=0;ch<11;++ch)choices<<QString("AV%1 - J103 / %2").arg(ch+1).arg(contacts[ch]);bool ok=false;const auto chosen=QInputDialog::getItem(this,tr("Customise signal input"),tr("Select the actual wired input"),choices,model_->tracks[i].channel,false,&ok);if(!ok){sync();return;}channel=choices.indexOf(chosen);}if(channel!=model_->tracks[i].channel){model_->tracks[i]={channel,0,0};model_->invalidate();model_->resetCapture(i/2);}sync();});
            for(int end=0;end<2;++end){auto *spin=new QDoubleSpinBox(table_);spin->setObjectName(QString("dbwVoltage%1_%2").arg(i).arg(end));spin->setRange(0,6);spin->setDecimals(4);spin->setSuffix(" V");spin->setButtonSymbols(QAbstractSpinBox::NoButtons);spin->setKeyboardTracking(false);table_->setCellWidget(i,2+end,spin);endpoints_[i][end]=spin;
                connect(spin,&QDoubleSpinBox::valueChanged,this,[this,i,end](double v){if(loading_)return;model_->resetCapture(i/2);if(end)model_->tracks[i].full=v;else model_->tracks[i].rest=v;});}
        }layout->addWidget(table_,1);
        if(!monitor){auto *captureBar=SensorUi::toolbar(this);pair_=new QComboBox(captureBar);pair_->addItems({tr("Pedal APS"),tr("Throttle TPS")});SensorUi::choices(pair_);captureBar->addWidget(pair_);
            for(bool full:{false,true}){auto *action=captureBar->addAction(style()->standardIcon(full?QStyle::SP_ArrowUp:QStyle::SP_ArrowDown),full?tr("Capture 100%"):tr("Capture 0%"));action->setObjectName(full?"dbwCaptureFull":"dbwCaptureRest");captures_[full?1:0]=action;connect(action,&QAction::triggered,this,[this,full]{QString error;if(!model_->capture(pair_->currentIndex(),full,error))message_=error;else message_=tr("Measured endpoints captured locally. Review both signals.");sync();});}
            layout->addWidget(captureBar);auto *tolerance=new QDoubleSpinBox(this);tolerance_=tolerance;tolerance->setObjectName("dbwDisagreement");tolerance->setRange(1,20);tolerance->setSuffix(tr(" % maximum disagreement"));tolerance->setValue(model_->disagreement);tolerance->setToolTip(tr("Console diagnostic threshold; not an ECU protection."));layout->addWidget(tolerance);connect(tolerance,&QDoubleSpinBox::valueChanged,this,[this](double v){if(!loading_)model_->disagreement=v;});
            auto *prepare=bar->addAction(CalibrationAppearance::icon(CalibrationAppearance::Parameters),tr("Prepare primary TPS sensor"));prepare->setObjectName("dbwPrepareTps");connect(prepare,&QAction::triggered,this,[this]{if(preparePrimaryTps)preparePrimaryTps();});
        }
        state_=new QLabel(this);state_->setObjectName("dbwSignalStatus");state_->setWordWrap(true);layout->addWidget(state_);
        auto *notice=new QLabel(tr("Signal commissioning only. DBW motor control and automatic H-bridge tuning are unavailable. APS/check calibrations remain in the project; they are not written to ECU."),this);notice->setWordWrap(true);layout->addWidget(notice);
        auto *timer=new QTimer(this);timer->setInterval(200);connect(timer,&QTimer::timeout,this,[this]{if(isVisible())refresh();});timer->start();sync();
    }
    QSize sizeHint()const override{return QSize(840,monitor_?310:420);}
    void sync(){loading_=true;if(!monitor_){for(int i=0;i<4;++i){{auto *input=inputs_[i];QSignalBlocker block(input);input->clear();const int ch=model_->tracks[i].channel;const int contacts[]={13,24,14,25,15,26,16,27,17,28,18};if(ch)input->addItem(QString("Current: AV%1 - J103 / %2").arg(ch).arg(contacts[ch-1]),ch);input->addItem(tr("Disabled"),0);input->addItem(tr("Customise..."),-1);input->setCurrentIndex(0);}endpoints_[i][0]->setValue(model_->tracks[i].rest);endpoints_[i][1]->setValue(model_->tracks[i].full);}tolerance_->setValue(model_->disagreement);}loading_=false;refresh();}
    void refresh(){
        if(monitor_)for(int i=0;i<4;++i){double v,p;const bool valid=model_->position(i,p);table_->item(i,1)->setText(model_->voltage(i,v)?QString::number(v,'f',4)+" V":"---");table_->item(i,2)->setText(valid?QString::number(p,'f',1)+" %":"---");table_->item(i,3)->setText(!model_->fresh()?tr("No sample"):!model_->calibrated(i)?tr("Uncalibrated"):valid?tr("In range"):tr("Invalid"));}
        else for(auto *action:captures_)action->setEnabled(model_->stationary()&&model_->assignmentError().isEmpty());
        state_->setText((message_.isEmpty()?QString{}:message_+"\n")+tr("APS: %1\nTPS: %2").arg(model_->diagnostic(0),model_->diagnostic(1)));
    }
private:
    std::shared_ptr<DbwSignals> model_;bool monitor_,loading_=false;QString message_;
    QTableWidget *table_;QLabel *state_;QComboBox *pair_=nullptr;QDoubleSpinBox *tolerance_=nullptr;
    std::array<QComboBox*,4> inputs_{};std::array<std::array<QDoubleSpinBox*,2>,4> endpoints_{};std::array<QAction*,2> captures_{};
};
}
