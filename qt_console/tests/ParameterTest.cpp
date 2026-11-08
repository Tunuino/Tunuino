#include "parameters/ParameterWidget.h"
#include "ecu/MapWriteTransaction.h"
#include <QApplication>
#include <QProcess>
#include <QQueue>
#include <QSet>
#include <QComboBox>
#include <QCheckBox>
#include <QAction>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QTest>
#include <QFile>
#include <QToolButton>
#include <iostream>
#include <cstdlib>
#include <algorithm>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed: "<<#x<<" line "<<__LINE__<<std::endl;std::exit(1);}}while(false)
using namespace tunuino;
using Codec=SensorCalibrationCodec;using Writer=MapWriteTransaction;
static quint32 u32(const QByteArray &b,int at){return (quint32(Codec::u16(b,at))<<16)|quint32(Codec::u16(b,at+2));}
static QByteArray read(int page,int at,int count){QByteArray q(7,0);q[0]='r';Codec::put(q,1,page);Codec::put(q,3,at);Codec::put(q,5,count);return q;}
int main(int argc,char **argv){QApplication app(argc,argv);
    if(!qEnvironmentVariable("TUNUINO_PARAMETER_STYLE").isEmpty()){QFile css(qEnvironmentVariable("TUNUINO_PARAMETER_STYLE"));CHECK(css.open(QIODevice::ReadOnly));app.setStyleSheet(QString::fromUtf8(css.readAll()));}
    const auto blocks=ParameterCatalog::blocks();CHECK(blocks.size()>=18);QSet<QString> keys;
    for(const auto &b:blocks){CHECK(!keys.contains(b.key));keys.insert(b.key);ParameterWidget widget(b);CHECK(widget.findChild<QTableWidget*>("parameters")->columnCount()==2);CHECK(!widget.findChild<QPushButton*>("parameterSave")->isEnabled());const auto mask=ParameterCatalog::ownedMask(b);CHECK(mask.size()==256);if(b.page==1)CHECK(mask[102]==0);
        for(const auto &f:b.fields){CHECK(f.offset>=0&&f.offset+f.width<=256);QByteArray data(256,0);CHECK(ParameterCatalog::put(data,f,f.initial));if(b.key!="ignition.firing")CHECK(ParameterCatalog::value(data,f)==f.initial);CHECK(!ParameterCatalog::put(data,f,f.maximum+1));}
    }
    const auto analog=*std::find_if(blocks.begin(),blocks.end(),[](const auto &b){return b.key=="inputs.analog";});ParameterWidget inputs(analog);CHECK(analog.fields.size()==24);auto *grid=inputs.findChild<QTableWidget*>("parameters");CHECK(grid->rowCount()==24&&!grid->horizontalHeader()->isVisible());
    const QStringList filters{"500Hz (no filter)","50Hz","25Hz","10Hz","5Hz","2Hz","1Hz"};for(int i=0;i<11;++i){auto *filter=inputs.findChild<QComboBox*>(QString("filter%1").arg(i));CHECK(filter&&filter->count()==7);for(int j=0;j<7;++j)CHECK(filter->itemText(j)==filters[j]);CHECK(!inputs.findChild<QComboBox*>(QString("pull%1").arg(i))->isEnabled());}CHECK(inputs.findChild<QCheckBox*>("CLTPullup")->isChecked()&&!inputs.findChild<QCheckBox*>("CLTPullup")->isEnabled());
    QByteArray inputHeader(256,0),inputAux(256,0),inputId(32,0);inputId[19]=15;inputs.load(inputHeader,inputAux,inputId,1);inputs.setAvailable(true);bool inputWrite=false;inputs.writeRequested=[&](int page,const QByteArray &desired,const QByteArray &mask){inputWrite=true;CHECK(page==1&&desired[70]==1&&desired[71]==6);for(int i=0;i<256;++i)CHECK(i>=70&&i<=81?mask[i]==char(255):mask[i]==0);};inputs.findChild<QComboBox*>("filter0")->setCurrentIndex(6);inputs.save();CHECK(!inputWrite);inputId[18]=2;inputs.load(inputHeader,inputAux,inputId,1);inputs.findChild<QComboBox*>("filter0")->setCurrentIndex(6);inputs.save();CHECK(inputWrite);
    auto inputState=inputs.editorState();auto inputValues=inputState["values"].toObject();inputValues["CLTPullup"]=0;inputState["values"]=inputValues;QString inputError;CHECK(!inputs.restoreEditor(inputState,true,inputError));auto desiredInput=inputHeader;desiredInput[70]=1;desiredInput[71]=6;QByteArray inputMask(256,0);for(int i=70;i<=81;++i)inputMask[i]=char(255);Writer oldInputWriter;bool sentInput=false;oldInputWriter.send=[&](const QByteArray &,int){sentInput=true;return true;};auto oldInputId=inputId;oldInputId[18]=0;CHECK(!oldInputWriter.startParameters(1,desiredInput,inputHeader,oldInputId,1,inputMask)&&!sentInput);
    auto externalInput=inputHeader;externalInput[70]=1;externalInput[71]=1;inputs.load(externalInput,inputAux,inputId,2);CHECK(!inputs.findChild<QPushButton*>("parameterSave")->isEnabled());inputWrite=false;inputs.save();CHECK(!inputWrite);inputs.findChild<QAction*>("primaryDiscard")->trigger();CHECK(inputs.findChild<QComboBox*>("filter0")->currentIndex()==1);
    if(!qEnvironmentVariable("TUNUINO_ANALOG_PREVIEW").isEmpty()){inputs.resize(inputs.sizeHint());inputs.show();app.processEvents();CHECK(inputs.grab().save(qEnvironmentVariable("TUNUINO_ANALOG_PREVIEW")));auto *popup=inputs.findChild<QComboBox*>("filter0");popup->showPopup();app.processEvents();CHECK(popup->view()->grab().save(qEnvironmentVariable("TUNUINO_ANALOG_PREVIEW")+"-popup.png"));popup->hidePopup();inputs.hide();}
    const auto primaryInput=*std::find_if(blocks.begin(),blocks.end(),[](const auto &b){return b.key=="inputs.primary";});CHECK(primaryInput.fields.size()==4);ParameterWidget digital(primaryInput);const QStringList levels{"None","Low","Medium","High"};auto *edgeChoice=digital.findChild<QComboBox*>("inputFilter");CHECK(edgeChoice&&edgeChoice->count()==4);for(int i=0;i<4;++i)CHECK(edgeChoice->itemText(i)==levels[i]);for(auto key:{"sensorType","adaptiveThreshold","pullupPulldown"}){auto *choice=digital.findChild<QComboBox*>(key);CHECK(choice&&choice->currentIndex()==0);for(int i=1;i<choice->count();++i)CHECK(!(choice->model()->flags(choice->model()->index(i,0))&Qt::ItemIsEnabled));}
    QByteArray primaryHeader(256,0),primaryAux(256,0),primaryId(32,0);primaryId[19]=15;digital.load(primaryHeader,primaryAux,primaryId,1);digital.setAvailable(true);bool primaryWritten=false;digital.writeRequested=[&](int page,const QByteArray &desired,const QByteArray &mask){primaryWritten=true;CHECK(page==1&&desired[5]==1&&desired[9]==3);for(int i=0;i<256;++i)CHECK(i==5||i==9?mask[i]==char(255):mask[i]==0);};edgeChoice->setCurrentIndex(3);digital.save();CHECK(!primaryWritten);primaryId[18]=4;digital.load(primaryHeader,primaryAux,primaryId,1);edgeChoice->setCurrentIndex(3);digital.save();CHECK(primaryWritten);
    auto immutable=digital.editorState();auto immutableValues=immutable["values"].toObject();immutableValues["sensorType"]=1;immutable["values"]=immutableValues;QString primaryError;CHECK(!digital.restoreEditor(immutable,true,primaryError));
    auto changedPrimary=primaryHeader;changedPrimary[5]=1;changedPrimary[9]=1;digital.load(changedPrimary,primaryAux,primaryId,2);CHECK(!digital.findChild<QPushButton*>("parameterSave")->isEnabled());primaryWritten=false;digital.save();CHECK(!primaryWritten);digital.findChild<QAction*>("primaryDiscard")->trigger();CHECK(edgeChoice->currentIndex()==1);
    auto unsupportedPrimary=primaryId;unsupportedPrimary[18]=0;auto wantedPrimary=primaryHeader;wantedPrimary[5]=1;wantedPrimary[9]=2;QByteArray primaryMask(256,0);primaryMask[5]=primaryMask[9]=char(255);Writer primaryWriter;bool primarySent=false;primaryWriter.send=[&](const QByteArray&,int){primarySent=true;return true;};CHECK(!primaryWriter.startParameters(1,wantedPrimary,primaryHeader,unsupportedPrimary,1,primaryMask)&&!primarySent);
    if(!qEnvironmentVariable("TUNUINO_PRIMARY_INPUT_PREVIEW").isEmpty()){digital.resize(digital.sizeHint());digital.show();app.processEvents();CHECK(digital.grab().save(qEnvironmentVariable("TUNUINO_PRIMARY_INPUT_PREVIEW")));edgeChoice->showPopup();app.processEvents();CHECK(edgeChoice->view()->grab().save(qEnvironmentVariable("TUNUINO_PRIMARY_INPUT_PREVIEW")+"-popup.png"));edgeChoice->hidePopup();digital.hide();}
    const auto camBlock=*std::find_if(blocks.begin(),blocks.end(),[](const auto &b){return b.key=="inputs.cam1";});ParameterWidget camPanel(camBlock);CHECK(camBlock.fields.size()==3&&!camPanel.findChild<QComboBox*>("adaptiveThreshold"));auto *camType=camPanel.findChild<QComboBox*>("sensorType");CHECK(camType->count()==4&&camType->itemText(3).startsWith("VR Sensor sensitivity table"));for(int i=1;i<4;++i)CHECK(!(camType->model()->flags(camType->model()->index(i,0))&Qt::ItemIsEnabled));auto *camLevel=camPanel.findChild<QComboBox*>("inputFilter");for(int i=0;i<4;++i)CHECK(camLevel->itemText(i)==levels[i]);
    QByteArray camHeader(256,0),camAux(256,0),camId(32,0);camId[19]=15;camPanel.load(camHeader,camAux,camId,1);camPanel.setAvailable(true);bool camWritten=false;camPanel.writeRequested=[&](int page,const QByteArray &desired,const QByteArray &mask){camWritten=true;CHECK(page==1&&desired[98]==1&&desired[99]==3);for(int i=0;i<256;++i)CHECK(i==98||i==99?mask[i]==char(255):mask[i]==0);};camLevel->setCurrentIndex(3);camPanel.save();CHECK(!camWritten);camId[18]=8;camPanel.load(camHeader,camAux,camId,1);camLevel->setCurrentIndex(3);camPanel.save();CHECK(camWritten);
    auto oldCamId=camId;oldCamId[18]=0;auto wantedCam=camHeader;wantedCam[98]=1;wantedCam[99]=2;QByteArray camMask(256,0);camMask[98]=camMask[99]=char(255);Writer camWriter;bool camSent=false;camWriter.send=[&](const QByteArray&,int){camSent=true;return true;};CHECK(!camWriter.startParameters(1,wantedCam,camHeader,oldCamId,1,camMask)&&!camSent);
    auto camChanged=camHeader;camChanged[98]=1;camChanged[99]=1;camPanel.load(camChanged,camAux,camId,2);CHECK(!camPanel.findChild<QPushButton*>("parameterSave")->isEnabled());camWritten=false;camPanel.save();CHECK(!camWritten);camPanel.findChild<QAction*>("primaryDiscard")->trigger();CHECK(camLevel->currentIndex()==1);
    if(!qEnvironmentVariable("TUNUINO_CAM_INPUT_PREVIEW").isEmpty()){camPanel.resize(camPanel.sizeHint());camPanel.show();app.processEvents();CHECK(camPanel.grab().save(qEnvironmentVariable("TUNUINO_CAM_INPUT_PREVIEW")));camLevel->showPopup();app.processEvents();CHECK(camLevel->view()->grab().save(qEnvironmentVariable("TUNUINO_CAM_INPUT_PREVIEW")+"-popup.png"));camLevel->hidePopup();camPanel.hide();}
    // The guard is editable offline, but older firmware must never receive its extension.
    const auto guard=*std::find_if(blocks.begin(),blocks.end(),[](const auto &b){return b.key=="fuel.guard";});
    ParameterWidget guardWidget(guard);QByteArray guardHeader(256,0),guardIdentity(32,0);guardIdentity[19]=15;
    auto guardAux=Codec::featureDefaults(Codec::tpsDefaults());Codec::put(guardAux,70,1024);
    guardWidget.load(guardHeader,guardAux,guardIdentity,1);guardWidget.setAvailable(true);bool guardCalled=false;
    guardWidget.writeRequested=[&](int page,const QByteArray &desired,const QByteArray &mask){guardCalled=true;CHECK(page==4&&desired[240]==1&&Codec::u16(desired,70)==1024);for(int j=0;j<256;++j)CHECK(!((quint8(desired[j])^quint8(guardAux[j]))&~quint8(mask[j])));};
    CHECK(!guardWidget.findChild<QPushButton*>("parameterSave")->isEnabled());guardWidget.save();CHECK(!guardCalled);
    guardIdentity[19]=79;guardWidget.load(guardHeader,guardAux,guardIdentity,1);CHECK(guardWidget.findChild<QPushButton*>("parameterSave")->isEnabled());guardWidget.save();CHECK(guardCalled);
    if(!qEnvironmentVariable("TUNUINO_GUARD_PREVIEW").isEmpty()){guardWidget.resize(guardWidget.sizeHint());guardWidget.show();app.processEvents();CHECK(guardWidget.grab().save(qEnvironmentVariable("TUNUINO_GUARD_PREVIEW")));guardWidget.hide();}
    const auto primary=*std::find_if(blocks.begin(),blocks.end(),[](const auto &b){return b.key=="ignition.primary";});
    ParameterWidget oldPrimary(primary);oldPrimary.load(guardHeader,guardAux,guardIdentity,1);oldPrimary.setAvailable(true);CHECK(!oldPrimary.findChild<QCheckBox*>("angleLock")->isEnabled());bool oldWrite=false;
    oldPrimary.writeRequested=[&](int page,const QByteArray &desired,const QByteArray &){oldWrite=true;CHECK(page==1&&desired.mid(27,5)==guardHeader.mid(27,5));};oldPrimary.save();CHECK(oldWrite);oldWrite=false;oldPrimary.findChild<QDoubleSpinBox*>("lockedAngle")->setValue(10);oldPrimary.save();CHECK(!oldWrite);
    auto lockId=guardIdentity;lockId[19]=char(207);ParameterWidget conflictPrimary(primary);auto lockHeader=guardHeader;lockHeader[3]=36;lockHeader[4]=1;Codec::put(lockHeader,96,12000);conflictPrimary.load(lockHeader,guardAux,lockId,1);conflictPrimary.setAvailable(true);conflictPrimary.findChild<QDoubleSpinBox*>("teeth")->setValue(60);
    auto changedLock=lockHeader;changedLock[27]=1;Codec::put(changedLock,30,1000);conflictPrimary.load(changedLock,guardAux,lockId,2);CHECK(!conflictPrimary.findChild<QPushButton*>("parameterSave")->isEnabled());bool conflictWrite=false;conflictPrimary.writeRequested=[&](int,const QByteArray&,const QByteArray&){conflictWrite=true;};conflictPrimary.save();CHECK(!conflictWrite);conflictPrimary.findChild<QAction*>("primaryDiscard")->trigger();CHECK(conflictPrimary.findChild<QPushButton*>("parameterSave")->isEnabled());CHECK(conflictPrimary.findChild<QDoubleSpinBox*>("lockedAngle")->value()==10);
    const auto executable=qEnvironmentVariable("TUNUINO_FIRMWARE_HARNESS");if(executable.isEmpty()){std::cout<<"Parameter catalog/UI bounds passed; native firmware harness not supplied\n";return 0;}
    QProcess process;process.start(executable,{"--engine-profile"});CHECK(process.waitForStarted(3000));
    const auto exchange=[&](const QByteArray &q){const auto line=q.toHex()+"\n";CHECK(process.write(line)==line.size());CHECK(process.waitForBytesWritten(3000));while(!process.canReadLine())CHECK(process.waitForReadyRead(3000));return QByteArray::fromHex(process.readLine().trimmed());};
    const auto identity=exchange("TUNUINO?");quint32 generation=u32(exchange("TUNUINO:V"),8);int operations=0;
    for(const auto &b:blocks){Writer::Pages base;for(int p=1;p<=4;++p)for(int at=0;at<(p==1||p==4?256:576);at+=64)base[p-1]+=exchange(read(p,at,64)).mid(1);
        ParameterWidget widget(b);widget.load(base[0],base[3],identity,generation);widget.setAvailable(true);
        if(b.key=="ignition.firing"){auto *first=widget.findChild<QComboBox*>("order0"),*last=widget.findChild<QComboBox*>("order3");CHECK(first&&last);const auto selected=first->currentIndex();first->setCurrentIndex(last->currentIndex());last->setCurrentIndex(selected);CHECK(widget.findChild<QDoubleSpinBox*>("cylinders")->isEnabled());}
        else if(b.key=="inputs.cam1")widget.findChild<QComboBox*>("inputFilter")->setCurrentIndex(3);
        else if(b.key=="inputs.primary")widget.findChild<QComboBox*>("inputFilter")->setCurrentIndex(2);
        else if(b.key=="inputs.analog")widget.findChild<QComboBox*>("filter0")->setCurrentIndex(6);
        else {bool changed=false;for(const auto &f:b.fields){if(f.bit||!f.choices.isEmpty())continue;auto *spin=widget.findChild<QDoubleSpinBox*>(f.key);if(spin&&spin->value()+1/f.scale<=f.maximum){spin->setValue(spin->value()+1/f.scale);changed=true;break;}}
            if(!changed){auto *combo=widget.findChild<QComboBox*>("cylinder0");CHECK(combo);combo->addItem("Chosen spare output",4);combo->setCurrentIndex(2);}}
        if(b.key=="ignition.firing"&&!qEnvironmentVariable("TUNUINO_PARAMETER_PREVIEW").isEmpty()){widget.resize(widget.sizeHint());widget.show();app.processEvents();CHECK(widget.grab().save(qEnvironmentVariable("TUNUINO_PARAMETER_PREVIEW")));widget.findChild<QToolButton*>("engineProfileToggle")->setChecked(true);widget.resize(720,680);app.processEvents();CHECK(widget.grab().save(qEnvironmentVariable("TUNUINO_PARAMETER_PREVIEW")+"-advanced.png"));widget.hide();}
        if(b.key=="ignition.primary"){widget.findChild<QCheckBox*>("capture")->setChecked(true);widget.findChild<QCheckBox*>("angleLock")->setChecked(true);widget.findChild<QDoubleSpinBox*>("lockedAngle")->setValue(10);}
        bool invoked=false;widget.writeRequested=[&](int page,const QByteArray &desired,const QByteArray &mask){invoked=true;CHECK(page==b.page&&desired!=base[page-1]);
            for(int i=0;i<256;++i)CHECK(!((quint8(desired[i])^quint8(base[page-1][i]))&~quint8(mask[i])));
            Writer writer;QQueue<QByteArray> pending;bool success=false;int copies=0,burns=0;
            writer.send=[&](const QByteArray &q,int){CHECK(!q.startsWith("ARM")&&!q.startsWith("RUN"));pending.enqueue(q);return true;};
            writer.savePrevious=[&](const QByteArray &id,quint32 gen,const Writer::Pages &actual){CHECK(id==identity&&gen==generation&&actual==base);++copies;return true;};
            writer.completed=[&](bool ok,bool uncertain,const QString &msg){if(!ok)std::cerr<<b.key.toStdString()<<": "<<msg.toStdString()<<std::endl;CHECK(!uncertain);success=ok;};
            CHECK(writer.startParameters(page,desired,base[page-1],identity,generation,mask));while(writer.busy()){CHECK(!pending.isEmpty());const auto q=pending.dequeue();if(q[0]=='b')++burns;writer.receive(exchange(q));}
            CHECK(success&&copies==1&&burns==1);generation=writer.generation();++operations;
            for(int p=1;p<=4;++p){QByteArray actual;for(int at=0;at<(p==1||p==4?256:576);at+=64)actual+=exchange(read(p,at,64)).mid(1);CHECK(actual==(p==page?desired:base[p-1]));}
            auto illegal=base[0];illegal[102]=1;CHECK(!writer.startParameters(1,illegal,base[0],identity,generation,QByteArray(256,char(255))));illegal=desired;int unowned=0;while(unowned<256&&mask[unowned]!=0)++unowned;CHECK(unowned<256);illegal[unowned]^=1;CHECK(!writer.startParameters(page,illegal,base[page-1],identity,generation,mask));
        };
        widget.save();CHECK(invoked);
        if(b.key=="ignition.primary"){
            for(int p=1;p<=4;++p){base[p-1].clear();for(int at=0;at<(p==1||p==4?256:576);at+=64)base[p-1]+=exchange(read(p,at,64)).mid(1);}
            CHECK(base[0][27]==1&&base[0][28]==1&&Codec::u16(base[0],30)==1000);widget.saved();widget.load(base[0],base[3],identity,generation);invoked=false;
            if(!qEnvironmentVariable("TUNUINO_LOCK_PREVIEW").isEmpty()){widget.resize(widget.sizeHint());widget.show();app.processEvents();CHECK(widget.grab().save(qEnvironmentVariable("TUNUINO_LOCK_PREVIEW")));widget.hide();}
            widget.findChild<QCheckBox*>("angleLock")->setChecked(false);widget.save();CHECK(invoked);
        }
    }
    CHECK(operations==blocks.size()+1);process.closeWriteChannel();CHECK(process.waitForFinished(3000)&&process.exitCode()==0);
    std::cout<<operations<<" parameter block writes against actual C dispatcher: one Burn each, full-page preservation and firing order independent of output wiring passed (host simulation)\n";
}
