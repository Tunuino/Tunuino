#pragma once
#include "SensorUi.h"
#include "../protocol/SensorCalibrationCodec.h"
#include <QDialog>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QJsonArray>
#include <functional>
namespace tunuino {
class SensorCurveWizard final:public QDialog {
public:
    std::function<bool(SensorKind,const QJsonObject&,QString&)> prepare;
    static bool ntc(double r25,double beta,double pullup,double excitation,const std::array<double,3> &temperatures,QJsonObject &curve,QString &error){
        error.clear();if(!std::isfinite(r25)||!std::isfinite(beta)||!std::isfinite(pullup)||!std::isfinite(excitation)||r25<1||r25>1e7||beta<100||beta>10000||pullup<1||pullup>1e7||excitation<.1||excitation>6){error="Enter the actual R25, Beta, pull-up and excitation voltage.";return false;}
        for(int i=0;i<3;++i)if(!std::isfinite(temperatures[i])||temperatures[i]<-40||temperatures[i]>150||(i&&temperatures[i]<=temperatures[i-1])){error="Use three increasing temperatures between -40 and 150 degrees C.";return false;}
        QJsonArray volts,values;double previous=-1;for(int i=2;i>=0;--i){const double resistance=r25*std::exp(beta*(1/(temperatures[i]+273.15)-1/298.15));const double voltage=std::round(excitation*resistance/(pullup+resistance)*10000)/10000;
            if(!std::isfinite(voltage)||voltage<=previous){error="The generated voltage points are not distinct at native precision.";return false;}previous=voltage;volts.append(voltage);values.append(std::round(temperatures[i]*100)/100);}
        curve={{"volts",volts},{"values",values}};return true;
    }
    static bool linear(double lowV,double lowKpa,double highV,double highKpa,QJsonObject &curve,QString &error,double maximum=500){
        error.clear();if(!std::isfinite(lowV)||!std::isfinite(highV)||!std::isfinite(lowKpa)||!std::isfinite(highKpa)||lowV<0||highV>6||lowV>=highV||lowKpa<0||highKpa>maximum||lowKpa>=highKpa){error="Enter increasing voltage/pressure points within the selected sensor range.";return false;}
        curve={{"volts",QJsonArray{lowV,highV}},{"values",QJsonArray{lowKpa,highKpa}}};return true;
    }
    explicit SensorCurveWizard(SensorKind kind,bool selectTemperature=false,QWidget *parent=nullptr,bool selectPressure=false):QDialog(parent),kind_(kind){
        setObjectName(kind==SensorKind::Map?"mapSensorWizard":selectTemperature?"temperatureSensorWizard":kind==SensorKind::Iat?"iatSensorWizard":"cltSensorWizard");setWindowTitle(selectPressure?tr("Pressure sensor wizard"):kind==SensorKind::Map?tr("MAP pressure sensor wizard"):selectTemperature?tr("Temperature sensor wizard"):tr("%1 sensor wizard").arg(SensorCalibrationCodec::name(kind)));setWindowModality(Qt::ApplicationModal);setWindowFlags(Qt::Dialog|Qt::WindowTitleHint|Qt::WindowCloseButtonHint);
        auto *layout=new QVBoxLayout(this);layout->setContentsMargins(8,8,8,8);layout->setSpacing(4);layout->addWidget(SensorUi::section(this,windowTitle()));
        auto *table=new QTableWidget(kind==SensorKind::Map?(selectPressure?5:4):selectTemperature?8:7,2,this);table->setObjectName("sensorWizardFields");SensorUi::grid(table);int row=0;
        if(selectPressure){table->setItem(row,0,new QTableWidgetItem(tr("Target sensor")));pressureTarget_=new QComboBox(table);pressureTarget_->setObjectName("pressureWizardTarget");pressureTarget_->addItems({"MAP","Fuel pressure","Oil pressure"});SensorUi::choices(pressureTarget_);table->setCellWidget(row++,1,pressureTarget_);connect(pressureTarget_,&QComboBox::currentIndexChanged,this,[this]{refresh();});}
        if(selectTemperature){table->setItem(row,0,new QTableWidgetItem(tr("Target sensor")));target_=new QComboBox(table);target_->addItems({"IAT","CLT"});target_->setObjectName("temperatureWizardTarget");SensorUi::choices(target_);table->setCellWidget(row++,1,target_);}
        const auto field=[&](const QString &label,const QString &unit,double max,double initial,double minimum=0){table->setItem(row,0,new QTableWidgetItem(label));auto *spin=new QDoubleSpinBox(table);spin->setObjectName(QString("sensorWizardValue%1").arg(fields_.size()));spin->setRange(minimum,max);spin->setDecimals(unit==" V"?4:2);spin->setSuffix(unit);spin->setValue(initial);spin->setKeyboardTracking(false);table->setCellWidget(row++,1,spin);fields_.append(spin);connect(spin,&QDoubleSpinBox::valueChanged,this,[this]{refresh();});};
        if(kind==SensorKind::Map){field(tr("Lower voltage")," V",6,0);field(tr("Lower absolute pressure")," kPa",selectPressure?2000:500,0);field(tr("Upper voltage")," V",6,5);field(tr("Upper absolute pressure")," kPa",selectPressure?2000:500,0);}
        else {field(tr("Resistance at 25 °C")," ohm",1e7,0);field(tr("Beta")," K",10000,0);field(tr("Pull-up resistance")," ohm",1e7,0);field(tr("Excitation voltage")," V",6,5);field(tr("Temperature 1")," °C",150,-20,-40);field(tr("Temperature 2")," °C",150,40,-40);field(tr("Temperature 3")," °C",150,100,-40);}
        table->setMinimumHeight(table->rowCount()*30+2);layout->addWidget(table);auto *notice=new QLabel(kind==SensorKind::Map?tr("Use the installed sensor datasheet and pressure reference. MAP uses absolute kPa; fuel/oil use the calibrated sensor reference. Calibration does not enable pressure protection."):tr("Custom NTC model: enter the installed sensor and actual pull-up data. The ECU uses a three-point linear approximation, not the full Beta equation."),this);notice->setWordWrap(true);layout->addWidget(notice);state_=new QLabel(this);state_->setWordWrap(true);layout->addWidget(state_);
        buttons_=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,this);buttons_->button(QDialogButtonBox::Ok)->setObjectName("sensorWizardOk");layout->addWidget(buttons_);connect(buttons_,&QDialogButtonBox::rejected,this,&QDialog::reject);connect(buttons_,&QDialogButtonBox::accepted,this,[this]{QJsonObject curve;QString error;if(!result(curve,error)||!prepare)return;const auto kind=selectedKind();if(!prepare(kind,curve,error)){state_->setText(error);return;}accept();});resize(640,kind==SensorKind::Map?290:430);refresh();
    }
private:
    SensorKind selectedKind()const{if(pressureTarget_)return pressureTarget_->currentIndex()==1?SensorKind::FuelPressure:pressureTarget_->currentIndex()==2?SensorKind::OilPressure:SensorKind::Map;return target_?(target_->currentIndex()?SensorKind::Clt:SensorKind::Iat):kind_;}
    bool result(QJsonObject &curve,QString &error)const{if(SensorCalibrationCodec::pressure(selectedKind())){for(int i:{1,3})if(std::abs(fields_[i]->value()*10-std::round(fields_[i]->value()*10))>1e-6){error=tr("Fuel/oil calibration points use 0.1 kPa precision.");return false;}}if(kind_==SensorKind::Map)return linear(fields_[0]->value(),fields_[1]->value(),fields_[2]->value(),fields_[3]->value(),curve,error,SensorCalibrationCodec::maximum(selectedKind()));return ntc(fields_[0]->value(),fields_[1]->value(),fields_[2]->value(),fields_[3]->value(),{fields_[4]->value(),fields_[5]->value(),fields_[6]->value()},curve,error);}
    void refresh(){if(!buttons_)return;QJsonObject curve;QString error;const bool valid=result(curve,error);buttons_->button(QDialogButtonBox::Ok)->setEnabled(valid);state_->setText(valid?tr("OK prepares the local calibration. Input assignment is preserved; no ECU write."):error);}
    SensorKind kind_;QComboBox *pressureTarget_=nullptr;QVector<QDoubleSpinBox*> fields_;QComboBox *target_=nullptr;QDialogButtonBox *buttons_=nullptr;QLabel *state_=nullptr;
};
}
