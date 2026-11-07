#include "SensorCalibrationWidget.h"
#include "SensorUi.h"
#include <QFileDialog>
#include <QSaveFile>
#include <QFile>
#include <QJsonDocument>
#include <QAction>
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
#include <QJsonArray>
#include <cmath>
namespace tunuino {
SensorCalibrationWidget::SensorCalibrationWidget(SensorKind kind,QWidget *parent):QWidget(parent),kind_(kind){
    setObjectName(QStringLiteral("sensor%1").arg(SensorCalibrationCodec::name(kind)));
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(4,4,4,4);layout->setSpacing(3);
    auto *bar=SensorUi::toolbar(this);bar->setObjectName("sensorToolbar");
    auto *open=bar->addAction(CalibrationAppearance::icon(CalibrationAppearance::Open),tr("Open calibration"));auto *store=bar->addAction(CalibrationAppearance::icon(CalibrationAppearance::Save),tr("Save calibration"));restore_=bar->addAction(CalibrationAppearance::icon(CalibrationAppearance::Defaults),tr("Restore last loaded calibration"));bar->addSeparator();auto *help=bar->addAction(CalibrationAppearance::icon(CalibrationAppearance::Help),tr("Help"));
    connect(open,&QAction::triggered,this,[this]{file(false);});connect(store,&QAction::triggered,this,[this]{file(true);});connect(restore_,&QAction::triggered,this,[this]{if(header_.size()!=256||aux_.size()!=256)return;edited_=false;projectEdits_=false;load(header_,aux_,identity_,generation_);});connect(help,&QAction::triggered,this,[this]{QMessageBox::information(this,SensorCalibrationCodec::name(kind_),tr("Choose the installed input through Customise. Enter measured voltage/calibration points or capture a fresh sample from the selected input. Toolbar files contain this sensor only; File / Save Project as stores the whole project. Write to ECU is available through ECU / F2 or the table context menu. Opening files, restoring values or editing never arms outputs."));});
    layout->addWidget(bar);layout->addWidget(SensorUi::section(this,SensorCalibrationCodec::name(kind_)));
    const int points=SensorCalibrationCodec::points(kind_);table_=new QTableWidget(1+(kind_==SensorKind::Lambda?1:0)+points*(kind_==SensorKind::Tps?1:2),2,this);table_->setObjectName("sensorPoints");SensorUi::grid(table_);int row=0;
    table_->setItem(row,0,new QTableWidgetItem(tr("Input")));auto *inputRow=new QWidget(table_);auto *inputLayout=new QHBoxLayout(inputRow);inputLayout->setContentsMargins(0,0,0,0);inputLayout->setSpacing(2);input_=new QComboBox(inputRow);input_->setObjectName("sensorInput");input_->addItem(tr("Disabled"),0);SensorUi::choices(input_);auto *custom=new QPushButton(tr("Customise..."),inputRow);custom->setObjectName("sensorCustomise");inputLayout->addWidget(input_,1);inputLayout->addWidget(custom);table_->setCellWidget(row++,1,inputRow);connect(custom,&QPushButton::clicked,this,[this]{chooseInput();});connect(input_,&QComboBox::currentIndexChanged,this,[this]{edit();});
    if(kind_==SensorKind::Lambda){
        table_->setItem(row,0,new QTableWidgetItem(tr("Controller")));preset_=new QComboBox(table_);preset_->setObjectName("sensorPreset");preset_->addItems({tr("Current / custom curve"),"AEM X-Series 30-0300","14point7 Spartan 3 v2 (green output)","Innovate LC-2 (factory output 1)","PLX SM-AFR Gen2"});SensorUi::choices(preset_);table_->setCellWidget(row++,1,preset_);
        preset_->setToolTip(tr("Check the exact controller and configured analog output. A profile changes the curve only; it does not choose an input or write the ECU."));connect(preset_,&QComboBox::currentIndexChanged,this,[this](int index){if(!index)return;loading_=true;const double low[]={0,.5,0,0,0},high[]={0,4.5,5,5,5};const double first[]={0,.4990+.1621*.5,.68,.5,10/14.7},second[]={0,.4990+.1621*4.5,1.36,22.39/14.7,20/14.7};volts_[0]->setValue(low[index]);volts_[1]->setValue(high[index]);values_[0]->setValue(first[index]);values_[1]->setValue(second[index]);loading_=false;edit();});
    }
    for(int i=0;i<points;++i){
        table_->setItem(row,0,new QTableWidgetItem(kind_==SensorKind::Tps?(i?tr("Throttle open"):tr("Throttle closed")):tr("Point %1 voltage").arg(i+1)));
        auto *voltageRow=new QWidget(table_);auto *line=new QHBoxLayout(voltageRow);line->setContentsMargins(0,0,0,0);line->setSpacing(2);volts_[i]=new QDoubleSpinBox(voltageRow);volts_[i]->setObjectName(QStringLiteral("sensorVoltage%1").arg(i));volts_[i]->setRange(0,6);volts_[i]->setDecimals(4);volts_[i]->setSuffix(" V");volts_[i]->setKeyboardTracking(false);volts_[i]->setButtonSymbols(QAbstractSpinBox::NoButtons);line->addWidget(volts_[i],1);capture_[i]=new QPushButton(tr("Capture"),voltageRow);capture_[i]->setObjectName(QStringLiteral("sensorCapture%1").arg(i));line->addWidget(capture_[i]);table_->setCellWidget(row++,1,voltageRow);connect(volts_[i],&QDoubleSpinBox::valueChanged,this,[this]{edit();});
        connect(capture_[i],&QPushButton::clicked,this,[this,i]{if(!hasSample())return;const int ch=input_->currentData().toInt(),index=SensorCalibrationCodec::temperature(kind_)?11+ch:ch-1;const int raw=SensorCalibrationCodec::u16(runtime_,61+2*index);volts_[i]->setValue(SensorCalibrationCodec::voltage(raw,header_));});
        if(kind_!=SensorKind::Tps){table_->setItem(row,0,new QTableWidgetItem(tr("Point %1 %2").arg(i+1).arg(kind_==SensorKind::Map?tr("pressure"):SensorCalibrationCodec::temperature(kind_)?tr("temperature"):kind_==SensorKind::Battery?tr("battery voltage"):tr("lambda"))));values_[i]=new QDoubleSpinBox(table_);values_[i]->setObjectName(QStringLiteral("sensorValue%1").arg(i));values_[i]->setRange(SensorCalibrationCodec::temperature(kind_)?-40:0,kind_==SensorKind::Map?500:SensorCalibrationCodec::temperature(kind_)?150:kind_==SensorKind::Battery?24:5);values_[i]->setDecimals(kind_==SensorKind::Lambda?3:2);values_[i]->setSuffix(kind_==SensorKind::Map?" kPa":SensorCalibrationCodec::temperature(kind_)?" °C":kind_==SensorKind::Battery?" V":"");values_[i]->setKeyboardTracking(false);values_[i]->setButtonSymbols(QAbstractSpinBox::NoButtons);table_->setCellWidget(row++,1,values_[i]);connect(values_[i],&QDoubleSpinBox::valueChanged,this,[this]{edit();});}
    }
    layout->addWidget(table_,1);
    if(kind_==SensorKind::Battery){auto *notice=new QLabel(tr("Use a suitable external divider. Never apply battery voltage directly to an AV input. Calibration does not enable voltage compensation or protection."),this);notice->setWordWrap(true);layout->addWidget(notice);}
    live_=new QLabel(tr("No live sample"),this);live_->setObjectName("sensorLive");layout->addWidget(live_);status_=new QLabel(this);status_->setObjectName("sensorStatus");status_->setWordWrap(true);layout->addWidget(status_);
    save_=new QPushButton(tr("Write to ECU"),this);save_->setObjectName("sensorSave");layout->addWidget(save_,0,Qt::AlignLeft);connect(save_,&QPushButton::clicked,this,[this]{save();});save_->hide();
    table_->setContextMenuPolicy(Qt::ActionsContextMenu);writeContext_=new QAction(tr("Write to ECU (F2)"),table_);writeContext_->setObjectName("sensorWriteContext");table_->addAction(writeContext_);connect(writeContext_,&QAction::triggered,this,[this]{save();});
    auto *timer=new QTimer(this);timer->setInterval(500);connect(timer,&QTimer::timeout,this,[this]{refresh();});timer->start();refresh();
}
QSize SensorCalibrationWidget::sizeHint()const{return QSize(780,table_->rowCount()*30+(kind_==SensorKind::Battery?180:140));}
void SensorCalibrationWidget::file(bool writing){
    const auto path=writing?QFileDialog::getSaveFileName(this,tr("Save sensor calibration"),{},tr("Tunuino sensor calibration (*.tnsensor)")):QFileDialog::getOpenFileName(this,tr("Open sensor calibration"),{},tr("Tunuino sensor calibration (*.tnsensor)"));if(path.isEmpty())return;
    if(writing){QSaveFile file(path);const auto bytes=QJsonDocument(QJsonObject{{"format","tunuino-sensor-v1"},{"sensor",SensorCalibrationCodec::name(kind_)},{"editor",editorState()}}).toJson();if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())QMessageBox::warning(this,tr("Sensor calibration"),tr("Could not save the calibration."));return;}
    QFile file(path);if(!file.open(QIODevice::ReadOnly)||file.size()>65536)return;QJsonParseError parse;const auto doc=QJsonDocument::fromJson(file.readAll(),&parse).object();QString error;
    if(parse.error!=QJsonParseError::NoError||doc["format"]!="tunuino-sensor-v1"||doc["sensor"]!=SensorCalibrationCodec::name(kind_)||!restoreEditor(doc["editor"].toObject(),false,error)){QMessageBox::warning(this,tr("Sensor calibration"),error.isEmpty()?tr("The file does not match this sensor."):error);return;}
    restoreEditor(doc["editor"].toObject(),true,error);projectEdits_=true;
}
QJsonObject SensorCalibrationWidget::editorState()const{QJsonArray volts,values;for(int i=0;i<SensorCalibrationCodec::points(kind_);++i){volts.append(volts_[i]->value());values.append(values_[i]?values_[i]->value():0);}return {{"input",input_->currentData().toInt()},{"volts",volts},{"values",values}};}
bool SensorCalibrationWidget::restoreEditor(const QJsonObject &o,bool apply,QString &error){
    const auto volts=o["volts"].toArray(),values=o["values"].toArray();const auto input=o["input"].toDouble(-1);bool valid=o["input"].isDouble()&&input==std::floor(input)&&input>=0&&input<=(SensorCalibrationCodec::temperature(kind_)?4:11)&&volts.size()==SensorCalibrationCodec::points(kind_)&&values.size()==volts.size();
    for(int i=0;valid&&i<volts.size();++i){valid=volts[i].isDouble()&&std::isfinite(volts[i].toDouble())&&volts[i].toDouble()>=volts_[i]->minimum()&&volts[i].toDouble()<=volts_[i]->maximum()&&values[i].isDouble()&&std::isfinite(values[i].toDouble());if(values_[i])valid=valid&&values[i].toDouble()>=values_[i]->minimum()&&values[i].toDouble()<=values_[i]->maximum();}
    if(!valid){error="Invalid sensor editor: "+SensorCalibrationCodec::name(kind_);return false;}if(!apply)return true;loading_=true;setInput(int(input));if(preset_)preset_->setCurrentIndex(0);for(int i=0;i<volts.size();++i){volts_[i]->setValue(volts[i].toDouble());if(values_[i])values_[i]->setValue(values[i].toDouble());}loading_=false;edited_=true;runtime_.clear();sampleAge_.invalidate();refresh();return true;
}
QString SensorCalibrationWidget::inputName(int channel)const{
    if(!channel)return tr("Disabled");
    // Proteus v0.7 PCB 383558bf: J103, TE 1-776231-2, natural housing.
    // AV sequence maps to raw[0..10]; AT1..4 maps to raw[12..15].
    static const int av[]={13,24,14,25,15,26,16,27,17,28,18},at[]={19,30,20,31};
    const bool temperature=SensorCalibrationCodec::temperature(kind_);
    return tr("%1 — Natural/white J103, pin %2 (%3%4)").arg(SensorCalibrationCodec::name(kind_)).arg(temperature?at[channel-1]:av[channel-1]).arg(temperature?"AT":"AV").arg(channel);
}
void SensorCalibrationWidget::setInput(int channel){
    QSignalBlocker blocker(input_);input_->clear();input_->addItem(tr("Disabled"),0);
    if(channel>0){input_->addItem(inputName(channel),channel);input_->setCurrentIndex(1);}
}
void SensorCalibrationWidget::chooseInput(){
    QStringList choices;choices<<tr("Disabled");const int count=SensorCalibrationCodec::temperature(kind_)?4:11;
    for(int i=1;i<=count;++i)choices<<inputName(i);
    bool ok=false;const QString chosen=QInputDialog::getItem(this,tr("Customise sensor input"),tr("Proteus v0.7 connector contact"),choices,input_->currentData().toInt(),false,&ok);
    if(ok){setInput(choices.indexOf(chosen));edit();}
}
void SensorCalibrationWidget::load(const QByteArray &header,const QByteArray &aux,const QByteArray &identity,quint32 generation){
    if(header.size()!=256||aux.size()!=256||identity.size()!=32||!generation)return;
    const auto project=projectEdits_&&identity_!=identity?editorState():QJsonObject{};
    const bool keep=edited_&&identity_==identity;
    if(identity_!=identity){runtime_.clear();sampleAge_.invalidate();}
    header_=header;aux_=aux;identity_=identity;generation_=generation;
    if(!keep){
        loading_=true;const auto c=SensorCalibrationCodec::decode(kind_,header,aux);
        if(c.channel<0||c.channel>(SensorCalibrationCodec::temperature(kind_)?4:11)){loading_=false;available_=false;refresh();return;}
        setInput(c.channel);if(preset_){QSignalBlocker blocker(preset_);preset_->setCurrentIndex(0);}
        for(int i=0;i<(SensorCalibrationCodec::points(kind_));++i){volts_[i]->setValue(SensorCalibrationCodec::voltage(c.raw[i],header));if(values_[i])values_[i]->setValue(c.values[i]);}
        edited_=false;loading_=false;
    }
    if(!project.isEmpty()){QString ignored;restoreEditor(project,true,ignored);}
    refresh();
}
void SensorCalibrationWidget::setAvailable(bool available){available_=available;refresh();}
void SensorCalibrationWidget::updateRaw(const QByteArray &runtime){
    if(runtime.size()!=101||quint8(runtime[0])!=0||runtime.mid(1,31)!=identity_.mid(1,31))return;
    runtime_=runtime;sampleAge_.start();refresh();
}
SensorCalibration SensorCalibrationWidget::configuration()const{
    SensorCalibration c;c.channel=input_->currentData().toInt();
    for(int i=0;i<(SensorCalibrationCodec::points(kind_));++i){c.raw[i]=SensorCalibrationCodec::raw(volts_[i]->value(),header_);if(values_[i])c.values[i]=values_[i]->value();}return c;
}
void SensorCalibrationWidget::edit(){if(loading_)return;edited_=true;refresh();}
void SensorCalibrationWidget::refresh(){
    const bool loaded=header_.size()==256&&aux_.size()==256&&!identity_.isEmpty()&&generation_;
    QString error;QByteArray desired;
    if(loaded){desired=SensorCalibrationCodec::apply(kind_,configuration(),header_,aux_,error);
        if(!desired.isEmpty()&&!SensorCalibrationCodec::permitted(kind_,SensorCalibrationCodec::auxiliary(kind_)?aux_:header_,desired,header_,aux_,error))desired.clear();}
    save_->setEnabled(available_&&loaded&&!desired.isEmpty());writeContext_->setEnabled(save_->isEnabled());restore_->setEnabled(loaded);
    status_->setText(!loaded?tr("Connect the ECU to read its calibration."):!error.isEmpty()?error:edited_?tr("Unsaved calibration edits. Other ECU settings will be preserved."):((kind_==SensorKind::Iat||kind_==SensorKind::Battery)&&aux_[68]!=1)?tr("No calibration stored. Select an input and enter measured points."):tr("Calibration read from ECU."));
    const int channel=input_->currentData().toInt();
    const bool fresh=hasSample();
    for(auto *capture:capture_)if(capture)capture->setEnabled(fresh);
    if(fresh){const int index=SensorCalibrationCodec::temperature(kind_)?11+channel:channel-1;const int raw=SensorCalibrationCodec::u16(runtime_,61+2*index);
        live_->setText(tr("Live selected input: %1 V").arg(SensorCalibrationCodec::voltage(raw,header_),0,'f',4));}
    else live_->setText(tr("No fresh ADC sample for the selected input"));
}
bool SensorCalibrationWidget::hasSample()const{
    if(!available_||!input_->currentData().toInt()||header_.size()!=256||runtime_.size()!=101||!sampleAge_.isValid()
        ||sampleAge_.elapsed()>2000||!(quint8(runtime_[40])&1)||runtime_.mid(1,31)!=identity_.mid(1,31))return false;
    const quint32 generation=(quint32(SensorCalibrationCodec::u16(runtime_,57))<<16)|quint32(SensorCalibrationCodec::u16(runtime_,59));
    const int channel=input_->currentData().toInt(),index=SensorCalibrationCodec::temperature(kind_)?11+channel:channel-1;
    const int reference=SensorCalibrationCodec::u16(header_,100);
    return generation==generation_&&reference>=2800&&reference<=3600&&SensorCalibrationCodec::u16(runtime_,61+2*index)<=4095;
}
void SensorCalibrationWidget::save(){
    if(!available_||!writeRequested)return;QString error;
    const auto desired=SensorCalibrationCodec::apply(kind_,configuration(),header_,aux_,error);
    if(desired.isEmpty()||!SensorCalibrationCodec::permitted(kind_,SensorCalibrationCodec::auxiliary(kind_)?aux_:header_,desired,header_,aux_,error)){
        QMessageBox::warning(this,tr("Sensor calibration"),error);return;}
    writeRequested(kind_,desired);
}
}
