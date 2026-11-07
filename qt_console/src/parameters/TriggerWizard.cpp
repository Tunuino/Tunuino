#include "TriggerWizard.h"
#include "../ecu/EcuPowerState.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QAbstractButton>
#include <QMessageBox>
#include <QAction>
#include <QCloseEvent>
#include <QMdiSubWindow>
#include <cmath>
namespace tunuino {
namespace {
quint32 u32(const QByteArray &b,int at){return (quint32(SensorCalibrationCodec::u16(b,at))<<16)|quint32(SensorCalibrationCodec::u16(b,at+2));}
QTableWidget *table(QWizardPage *page,const QStringList &names){auto *t=new QTableWidget(names.size(),2,page);t->horizontalHeader()->hide();t->verticalHeader()->hide();t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);t->verticalHeader()->setDefaultSectionSize(32);t->setEditTriggers(QAbstractItemView::NoEditTriggers);for(int i=0;i<names.size();++i)t->setItem(i,0,new QTableWidgetItem(names[i]));t->setStyleSheet("QTableWidget{background:#282828;color:#eeeeee;gridline-color:#707070;selection-background-color:#148bce;} QTableWidget QSpinBox,QTableWidget QDoubleSpinBox,QTableWidget QComboBox{background:#282828;color:#8de522;border:0;} QTableWidget QSpinBox:disabled,QTableWidget QDoubleSpinBox:disabled{color:#999999;} QCheckBox::indicator{width:14px;height:14px;border:2px solid #43cddd;} QCheckBox::indicator:checked{background:#43cddd;}");return t;}
QSpinBox *spin(QWidget *parent,const char *key,int low,int high){auto *s=new QSpinBox(parent);s->setObjectName(key);s->setRange(low,high);s->setKeyboardTracking(false);return s;}
QLabel *note(QWidget *parent,const QString &text){auto *label=new QLabel(text,parent);label->setWordWrap(true);return label;}
}
TriggerWizard::TriggerWizard(QWidget *parent):QWizard(parent){
    setObjectName("triggerWizard");setWindowFlags(Qt::Widget);setWizardStyle(QWizard::ClassicStyle);setOption(QWizard::HaveHelpButton);setButtonText(QWizard::FinishButton,tr("Write to ECU"));
    auto *primary=new QWizardPage(this);primary->setTitle(tr("Primary trigger"));auto *p=new QVBoxLayout(primary);
    p->addWidget(note(primary,tr("Configure the missing-tooth wheel used by the native firmware. Crank: J101 pins 5 (+) / 13 (-). Capture uses the conditioned input's rising edge; electrical sensor type and edge polarity are fixed by this firmware/board.")));
    auto *wheel=table(primary,{tr("Wheel pattern"),tr("Nominal teeth, including missing teeth"),tr("Missing teeth"),tr("Hard RPM limit"),tr("Enable trigger capture")});
    pattern_=new QComboBox(wheel);pattern_->setObjectName("triggerPattern");pattern_->addItems({tr("Current / custom"),"36-1","36-2","60-2","24-1","12-1"});teeth_=spin(wheel,"triggerTeeth",4,120);missing_=spin(wheel,"triggerMissing",1,59);rpm_=spin(wheel,"triggerMaximumRpm",1000,20000);rpm_->setSuffix(" rpm");capture_=new QCheckBox(wheel);capture_->setObjectName("triggerCapture");wheel->setCellWidget(0,1,pattern_);wheel->setCellWidget(1,1,teeth_);wheel->setCellWidget(2,1,missing_);wheel->setCellWidget(3,1,rpm_);wheel->setCellWidget(4,1,capture_);p->addWidget(wheel,1);setPage(0,primary);
    auto *phase=new QWizardPage(this);phase->setTitle(tr("Cylinder phase and reference angle"));auto *ph=new QVBoxLayout(phase);profileInfo_=note(phase,{});ph->addWidget(profileInfo_);
    ph->addWidget(note(phase,tr("Cam: J101 pins 4 (+) / 12 (-), one selected edge per 720°. Cam window indexes the last observed crank tooth; tooth zero is the first physical tooth after the gap. Tooth-zero angle is an absolute engine-cycle reference, not a sensor-to-TDC measurement made by this wizard.")));
    auto *sync=table(phase,{tr("Tooth-zero angle"),tr("First cam window tooth"),tr("Last cam window tooth"),tr("Invert phase")});angle_=new QDoubleSpinBox(sync);angle_->setObjectName("triggerToothZero");angle_->setRange(0,719.99);angle_->setDecimals(2);angle_->setSuffix(" °");angle_->setKeyboardTracking(false);first_=spin(sync,"triggerCamFirst",0,119);last_=spin(sync,"triggerCamLast",0,119);invert_=new QCheckBox(sync);invert_->setObjectName("triggerInvert");sync->setCellWidget(0,1,angle_);sync->setCellWidget(1,1,first_);sync->setCellWidget(2,1,last_);sync->setCellWidget(3,1,invert_);ph->addWidget(sync,1);
    auto *links=new QHBoxLayout;auto *firing=new QPushButton(tr("Open Firing order"),phase),*scope=new QPushButton(tr("Open Scope (F5)"),phase);firing->setObjectName("triggerOpenFiring");scope->setObjectName("triggerOpenScope");links->addWidget(firing);links->addWidget(scope);ph->addLayout(links);connect(firing,&QPushButton::clicked,this,[this]{if(openFiringOrder)openFiringOrder();});connect(scope,&QPushButton::clicked,this,[this]{if(openScope)openScope();});setPage(1,phase);
    auto *review=new QWizardPage(this);review->setTitle(tr("Review before writing"));auto *r=new QVBoxLayout(review);review_=note(review,{});review_->setObjectName("triggerReview");r->addWidget(review_);reviewTable_=table(review,{tr("Wheel"),tr("Hard RPM limit"),tr("Capture"),tr("Tooth-zero angle"),tr("First cam window tooth"),tr("Last cam window tooth"),tr("Invert phase")});reviewTable_->setObjectName("triggerReviewTable");r->addWidget(reviewTable_,1);evidence_=note(review,tr("No ECU Scope capture observed for this configuration."));evidence_->setObjectName("triggerScopeEvidence");r->addWidget(evidence_);r->addWidget(note(review,tr("Physical ignition timing still needs verification. Configure ignition angle lock in Ignition / Triggers / Primary trigger and disable it after timing-light calibration. Automatic TDC detection is unavailable. Cylinder/output assignments are preserved; outputs remain disarmed.")));state_=note(review,{});state_->setObjectName("triggerWizardStatus");r->addWidget(state_);setPage(2,review);
    connect(pattern_,&QComboBox::currentIndexChanged,this,[this](int index){if(loading_||index==0)return;const auto parts=pattern_->itemText(index).split('-');loading_=true;teeth_->setValue(parts[0].toInt());missing_->setValue(parts[1].toInt());loading_=false;edited();});
    for(auto *control:{teeth_,missing_,rpm_,first_,last_})connect(control,&QSpinBox::valueChanged,this,[this,control]{if(loading_)return;if(control==teeth_||control==missing_){loading_=true;pattern_->setCurrentIndex(0);loading_=false;}edited();});
    connect(angle_,&QDoubleSpinBox::valueChanged,this,[this]{edited();});connect(capture_,&QCheckBox::toggled,this,[this]{edited();});connect(invert_,&QCheckBox::toggled,this,[this]{edited();});connect(this,&QWizard::currentIdChanged,this,[this]{refresh();});
    connect(this,&QWizard::helpRequested,this,[this]{QMessageBox::information(this,tr("Trigger wizard"),tr("1. Read the ECU and choose its actual missing-tooth geometry.\n2. Review the existing cylinder profile, cam window and tooth-zero reference. Open Scope to inspect actual captured edges.\n3. Review and write with the engine stopped and outputs disarmed. The normal save path preserves all other configuration and creates a previous snapshot.\n\nPatterns without missing teeth, configurable electrical sensor type/polarity need additional firmware support. Ignition angle lock is configured in Primary trigger on supporting firmware."));});
    setContextMenuPolicy(Qt::ActionsContextMenu);auto *discard=new QAction(tr("Discard edits / use ECU values"),this);discard->setObjectName("triggerDiscard");addAction(discard);connect(discard,&QAction::triggered,this,[this]{this->discard();});
    loading_=true;teeth_->setValue(36);missing_->setValue(1);rpm_->setValue(12000);loading_=false;restart();refresh();
}
QJsonObject TriggerWizard::editorState()const{return {{"teeth",teeth_->value()},{"missing",missing_->value()},{"rpm",rpm_->value()},{"capture",capture_->isChecked()},{"angle",angle_->value()},{"first",first_->value()},{"last",last_->value()},{"invert",invert_->isChecked()}};}
bool TriggerWizard::restoreEditor(const QJsonObject &o,bool apply,QString &error){
    const auto number=[&](const char *key,double low,double high,double scale){const auto v=o[key];double n=v.toDouble();return v.isDouble()&&std::isfinite(n)&&n>=low&&n<=high&&std::abs(n*scale-std::round(n*scale))<1e-6;};
    if(!number("teeth",4,120,1)||!number("missing",1,59,1)||!number("rpm",1000,20000,1)||!number("angle",0,719.99,100)||!number("first",0,119,1)||!number("last",0,119,1)||!o["capture"].isBool()||!o["invert"].isBool()){error="Invalid trigger wizard editor.";return false;}if(!apply)return true;loading_=true;pattern_->setCurrentIndex(0);teeth_->setValue(o["teeth"].toInt());missing_->setValue(o["missing"].toInt());rpm_->setValue(o["rpm"].toInt());angle_->setValue(o["angle"].toDouble());first_->setValue(o["first"].toInt());last_->setValue(o["last"].toInt());capture_->setChecked(o["capture"].toBool());invert_->setChecked(o["invert"].toBool());loading_=false;dirty_=true;conflict_=false;scopeEvidence_.clear();restart();refresh();return true;
}
TriggerSetup TriggerWizard::settings()const{TriggerSetup s;s.capture=capture_->isChecked();s.teeth=teeth_->value();s.missing=missing_->value();s.maximumRpm=rpm_->value();s.camFirst=first_->value();s.camLast=last_->value();s.toothZero=angle_->value();s.inverted=invert_->isChecked();return s;}
void TriggerWizard::edited(){if(loading_)return;dirty_=true;refresh();}
void TriggerWizard::load(const QByteArray &h,const QByteArray &a,const QByteArray &identity,quint32 generation){
    if(h.size()!=256||a.size()!=256||!EcuPowerState::supportedIdentity(identity)||!generation){setAvailable(false);return;}
    const auto project=projectEdits_&&identity_!=identity?editorState():QJsonObject{};
    const bool keep=dirty_&&identity_==identity;const bool same=identity_==identity&&generation_==generation;
    if(keep&&header_!=h){const auto owned=TriggerSetupPlan::mask(TriggerSetupPlan::hasProfile(header_));for(int i=0;i<256;++i)if((quint8(header_[i])^quint8(h[i]))&quint8(owned[i]))conflict_=true;if(header_.mid(128)!=h.mid(128))conflict_=true;}
    header_=h;aux_=a;identity_=identity;generation_=generation;if(!same)scopeEvidence_.clear();
    if(!keep){const auto s=TriggerSetupPlan::read(h);loading_=true;pattern_->setCurrentIndex(0);teeth_->setValue(s.teeth);missing_->setValue(s.missing);rpm_->setValue(s.maximumRpm);capture_->setChecked(s.capture);angle_->setValue(s.toothZero);first_->setValue(s.camFirst);last_->setValue(s.camLast);invert_->setChecked(s.inverted);loading_=false;dirty_=conflict_=false;if(!same)restart();}
    if(!project.isEmpty()){QString ignored;restoreEditor(project,true,ignored);}
    refresh();
}
void TriggerWizard::loadOffline(const QByteArray &h,const QByteArray &a){if(h.size()!=256||a.size()!=256)return;header_=h;aux_=a;identity_.clear();generation_=0;available_=false;scopeEvidence_.clear();refresh();}
void TriggerWizard::setAvailable(bool on){if(available_==on)return;available_=on;refresh();}
void TriggerWizard::discard(){if(pending_||header_.size()!=256)return;dirty_=false;conflict_=false;load(header_,aux_,identity_,generation_);refresh();}
void TriggerWizard::refresh(){
    const bool profile=TriggerSetupPlan::hasProfile(header_);for(QWidget *w:QList<QWidget*>{angle_,first_,last_,invert_})w->setEnabled(profile&&!pending_);
    const QStringList modes{tr("Sequential"),tr("Semi-sequential / wasted COP"),tr("Batch / wasted COP"),tr("Semi-sequential / shared coils"),tr("Batch / shared coils")};
    profileInfo_->setText(profile?tr("ECU profile: %1 cylinders, %2. Cylinder order and connector assignments are preserved.").arg(quint8(header_[129])).arg(modes.value(quint8(header_[156]))):tr("No engine profile. Configure the primary wheel here, then create the profile in Firing order before editing cylinder phase."));
    QString error;const auto desired=TriggerSetupPlan::apply(header_,aux_,settings(),error);const auto old=TriggerSetupPlan::read(header_);
    review_->setText(tr("ECU generation %1. Saved value → proposed value").arg(generation_));
    const auto onOff=[this](bool on){return on?tr("On"):tr("Off");};
    const QStringList values{
        QString("%1-%2 → %3-%4").arg(old.teeth).arg(old.missing).arg(teeth_->value()).arg(missing_->value()),
        QString("%1 → %2 rpm").arg(old.maximumRpm).arg(rpm_->value()),onOff(old.capture)+" → "+onOff(capture_->isChecked()),
        profile?QString("%1° → %2°").arg(old.toothZero,0,'f',2).arg(angle_->value(),0,'f',2):tr("Requires engine profile"),
        profile?QString("%1 → %2").arg(old.camFirst).arg(first_->value()):tr("Requires engine profile"),
        profile?QString("%1 → %2").arg(old.camLast).arg(last_->value()):tr("Requires engine profile"),
        profile?onOff(old.inverted)+" → "+onOff(invert_->isChecked()):tr("Requires engine profile")};
    for(int i=0;i<values.size();++i){auto *value=new QTableWidgetItem(values[i]);value->setForeground(QColor("#8de522"));reviewTable_->setItem(i,1,value);}
    if(header_.size()==256&&header_[28]!=0)review_->setText(review_->text()+tr(" — IGNITION ANGLE LOCK is active in the saved ECU configuration."));
    evidence_->setText(scopeEvidence_.isEmpty()?tr("No ECU Scope capture observed for this configuration."):scopeEvidence_);
    const QString state=pending_?tr("Saving. Wait for verified readback; no automatic retries."):conflict_?tr("The ECU trigger/profile changed while editing. Right-click and discard local edits before writing."):!available_?tr("Connect and synchronize the ECU before writing."):desired.isEmpty()?error:tr("Ready to write reviewed trigger parameters. Physical timing verification remains pending.");state_->setText(state);
    if(auto *finish=button(QWizard::FinishButton))finish->setEnabled(currentId()==2&&available_&&!conflict_&&!pending_&&!desired.isEmpty());
    for(auto which:{QWizard::BackButton,QWizard::NextButton,QWizard::CancelButton})if(auto *b=button(which))b->setEnabled(!pending_&&(which!=QWizard::BackButton||currentId()>0));
}
bool TriggerWizard::validateCurrentPage(){QString error;if(currentId()==0&&!TriggerSetupPlan::primaryValid(settings(),error)){QMessageBox::warning(this,tr("Primary trigger"),error);return false;}if(currentId()==1&&TriggerSetupPlan::apply(header_,aux_,settings(),error).isEmpty()){QMessageBox::warning(this,tr("Trigger configuration"),error);return false;}return true;}
bool TriggerWizard::reviewReady()const{QString error;return currentId()==2&&available_&&!conflict_&&!pending_&&!TriggerSetupPlan::apply(header_,aux_,settings(),error).isEmpty();}
void TriggerWizard::save(){if(currentId()!=2||!available_||conflict_||pending_||!writeRequested){refresh();return;}QString error;const auto desired=TriggerSetupPlan::apply(header_,aux_,settings(),error);if(desired.isEmpty()){state_->setText(error);return;}if(writeRequested(1,desired,TriggerSetupPlan::mask(TriggerSetupPlan::hasProfile(header_)))){pending_=true;refresh();}}
void TriggerWizard::writeFinished(bool success,const QString &message){pending_=false;if(success){dirty_=false;conflict_=false;scopeEvidence_.clear();}refresh();state_->setText(message);}
bool TriggerWizard::observeScope(const ToolSnapshot &source,const QByteArray &h,const QByteArray &samples){
    if(!source.valid()||source.identity!=identity_||source.generation!=generation_||source.pages[0]!=header_||h.size()!=21||h[0]!=0||h[1]!=1||h[2]!=1||u32(h,5)!=generation_||!u32(h,9))return false;
    const int count=SensorCalibrationCodec::u16(h,3);if(count<1||count>256||samples.size()!=5*count||u32(samples,0)!=0)return false;int crank=0,cam=0;quint32 previous=0;
    for(int i=0;i<count;++i){const auto time=u32(samples,5*i);const auto kind=quint8(samples[5*i+4]);if((i&&time<previous)||(kind!=1&&kind!=2))return false;previous=time;if(kind==1)++crank;else ++cam;}
    scopeEvidence_=tr("Last ECU Scope capture: %1 crank / %2 cam edges over %3 ms, generation %4, ECU wheel %5-%6. This capture describes the saved ECU geometry; draft edits are not measured until written.").arg(crank).arg(cam).arg(previous/1000.0,0,'f',2).arg(generation_).arg(quint8(source.pages[0][3])).arg(quint8(source.pages[0][4]));refresh();return true;
}
void TriggerWizard::reject(){if(pending_){refresh();return;}if(auto *window=qobject_cast<QMdiSubWindow*>(parentWidget()))window->close();else hide();}
void TriggerWizard::closeEvent(QCloseEvent *event){if(pending_){event->ignore();return;}event->accept();}
}
