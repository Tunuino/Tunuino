#include "parameters/TriggerWizard.h"
#include "parameters/EngineProfileDraft.h"
#include "ecu/MapWriteTransaction.h"
#include "tools/ScopeWidget.h"
#include <QApplication>
#include <QProcess>
#include <QQueue>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QAction>
#include <QTimer>
#include <QTest>
#include <QFile>
#include <iostream>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed: "<<#x<<" line "<<__LINE__<<std::endl;std::exit(1);}}while(false)
using namespace tunuino;using Codec=SensorCalibrationCodec;using Writer=MapWriteTransaction;
static quint32 u32(const QByteArray &b,int at){return (quint32(Codec::u16(b,at))<<16)|quint32(Codec::u16(b,at+2));}
static void put32(QByteArray &b,int at,quint32 n){Codec::put(b,at,n>>16);Codec::put(b,at+2,n);}
static QByteArray read(int page,int at,int count){QByteArray q(7,0);q[0]='r';Codec::put(q,1,page);Codec::put(q,3,at);Codec::put(q,5,count);return q;}
static QByteArray uid(){QByteArray id(32,0);id.replace(1,7,"TUNUINO");id[8]=id[9]=id[10]=1;id[12]=7;id[19]=63;id[20]=42;return id;}
static void native(bool profile){
    const auto executable=qEnvironmentVariable("TUNUINO_FIRMWARE_HARNESS");if(executable.isEmpty())return;QProcess process;process.start(executable,profile?QStringList{"--engine-profile"}:QStringList{});CHECK(process.waitForStarted(3000));
    const auto exchange=[&](const QByteArray &q){const auto line=q.toHex()+"\n";CHECK(process.write(line)==line.size());CHECK(process.waitForBytesWritten(3000));while(!process.canReadLine())CHECK(process.waitForReadyRead(3000));return QByteArray::fromHex(process.readLine().trimmed());};
    const auto identity=exchange("TUNUINO?");const auto generation=u32(exchange("TUNUINO:V"),8);Writer::Pages base;for(int p=1;p<=4;++p)for(int at=0;at<(p==1||p==4?256:576);at+=64)base[p-1]+=exchange(read(p,at,64)).mid(1);
    TriggerWizard wizard;wizard.load(base[0],base[3],identity,generation);wizard.setAvailable(true);wizard.findChild<QComboBox*>("triggerPattern")->setCurrentIndex(3);wizard.findChild<QCheckBox*>("triggerCapture")->setChecked(true);wizard.findChild<QSpinBox*>("triggerMaximumRpm")->setValue(11000);
    if(profile){wizard.findChild<QSpinBox*>("triggerCamFirst")->setValue(10);wizard.findChild<QSpinBox*>("triggerCamLast")->setValue(12);wizard.findChild<QDoubleSpinBox*>("triggerToothZero")->setValue(123.45);wizard.findChild<QCheckBox*>("triggerInvert")->setChecked(true);}
    bool invoked=false;wizard.writeRequested=[&](int page,const QByteArray &desired,const QByteArray &mask){invoked=true;CHECK(page==1&&desired!=base[0]);CHECK(mask==TriggerSetupPlan::mask(profile));for(int i=0;i<256;++i)CHECK(!((quint8(desired[i])^quint8(base[0][i]))&~quint8(mask[i])));
        CHECK(desired[102]==base[0][102]);CHECK(desired.mid(150,3)==base[0].mid(150,3));CHECK(desired.mid(160)==base[0].mid(160));
        Writer writer;QQueue<QByteArray> pending;bool success=false;int copies=0,burns=0;writer.send=[&](const QByteArray &q,int){CHECK(!q.startsWith("ARM")&&!q.startsWith("RUN"));pending.enqueue(q);return true;};writer.savePrevious=[&](const QByteArray &id,quint32 gen,const Writer::Pages &pages){CHECK(id==identity&&gen==generation&&pages==base);++copies;return true;};writer.completed=[&](bool ok,bool uncertain,const QString &message){if(!ok)std::cerr<<message.toStdString()<<std::endl;CHECK(!uncertain);success=ok;};CHECK(writer.startParameters(1,desired,base[0],identity,generation,mask));
        while(writer.busy()){CHECK(!pending.isEmpty());const auto q=pending.dequeue();if(q[0]=='b')++burns;writer.receive(exchange(q));}CHECK(success&&copies==1&&burns==1);
        for(int p=1;p<=4;++p){QByteArray actual;for(int at=0;at<(p==1||p==4?256:576);at+=64)actual+=exchange(read(p,at,64)).mid(1);CHECK(actual==(p==1?desired:base[p-1]));}return true;};
    wizard.save();CHECK(!invoked);wizard.next();CHECK(wizard.currentId()==1);wizard.next();CHECK(wizard.currentId()==2);wizard.save();CHECK(invoked);wizard.writeFinished(true,"Verified");process.closeWriteChannel();CHECK(process.waitForFinished(3000)&&process.exitCode()==0);
}
int main(int argc,char **argv){QApplication app(argc,argv);const auto cssPath=qEnvironmentVariable("TUNUINO_PARAMETER_STYLE");if(!cssPath.isEmpty()){QFile css(cssPath);CHECK(css.open(QIODevice::ReadOnly));app.setStyleSheet(QString::fromUtf8(css.readAll()));}
    QByteArray header(256,0),aux(256,0);Codec::put(header,0,2);header[3]=36;header[4]=1;Codec::put(header,96,12000);EngineProfileDraft engine;engine.load(header,aux);QString error;for(int i=0;i<4;++i){CHECK(engine.assign(i,false,i,error));CHECK(engine.assign(i,true,i,error));}header=engine.build(error);CHECK(header.size()==256);
    auto s=TriggerSetupPlan::read(header);s.teeth=60;s.missing=2;s.maximumRpm=11000;s.capture=true;s.camFirst=10;s.camLast=12;s.toothZero=123.45;s.inverted=true;const auto desired=TriggerSetupPlan::apply(header,aux,s,error);CHECK(desired.size()==256&&desired[2]==1&&desired[3]==60&&desired[4]==2&&desired[153]==10&&desired[154]==12&&desired[155]==1&&u32(desired,146)==12345);
    auto bad=s;bad.missing=30;CHECK(TriggerSetupPlan::apply(header,aux,bad,error).isEmpty());bad=s;bad.camFirst=13;CHECK(TriggerSetupPlan::apply(header,aux,bad,error).isEmpty());bad=s;bad.toothZero=720;CHECK(TriggerSetupPlan::apply(header,aux,bad,error).isEmpty());bad=s;bad.toothZero=1.001;CHECK(TriggerSetupPlan::apply(header,aux,bad,error).isEmpty());
    auto limited=Codec::featureDefaults(Codec::tpsDefaults());Codec::put(limited,70,64);bad=s;bad.maximumRpm=6000;CHECK(TriggerSetupPlan::apply(header,limited,bad,error).isEmpty());
    TriggerWizard wizard;wizard.load(header,aux,uid(),42);wizard.setAvailable(true);int requests=0;wizard.writeRequested=[&](int,const QByteArray&,const QByteArray&){++requests;return false;};wizard.save();CHECK(requests==0);wizard.findChild<QComboBox*>("triggerPattern")->setCurrentIndex(3);wizard.next();wizard.next();CHECK(wizard.currentId()==2&&wizard.reviewReady());wizard.save();CHECK(requests==1&&!wizard.busy());
    wizard.writeRequested=[&](int,const QByteArray&,const QByteArray&){++requests;return true;};wizard.save();CHECK(wizard.busy());wizard.save();CHECK(requests==2);wizard.writeFinished(false,"Lost response; reread required");QTest::qWait(50);CHECK(requests==2);
    auto changed=header;changed[3]=60;changed[4]=2;wizard.load(changed,aux,uid(),43);wizard.setAvailable(true);CHECK(!wizard.reviewReady());wizard.save();CHECK(requests==2);wizard.findChild<QAction*>("triggerDiscard")->trigger();CHECK(wizard.reviewReady());
    ToolSnapshot source;source.identity=uid();source.generation=43;source.pages[0]=changed;source.pages[3]=aux;MapPageValues map;for(int i=0;i<16;++i){map.rpm[i]=500*i;map.load[i]=20*i;}map.cells.fill(50);source.pages[1]=MapPageCodec::encode(2,map,error);source.pages[2]=MapPageCodec::encode(3,map,error);CHECK(source.valid());
    QByteArray captureHeader(21,0),samples(10,0);captureHeader[1]=captureHeader[2]=1;Codec::put(captureHeader,3,2);put32(captureHeader,5,43);put32(captureHeader,9,7);samples[4]=1;put32(samples,5,1000);samples[9]=2;
    CHECK(wizard.observeScope(source,captureHeader,samples));CHECK(wizard.findChild<QLabel*>("triggerScopeEvidence")->text().contains("1 crank / 1 cam"));auto foreign=source;foreign.identity[20]=43;CHECK(!wizard.observeScope(foreign,captureHeader,samples));auto wrong=captureHeader;put32(wrong,5,44);CHECK(!wizard.observeScope(source,wrong,samples));
    ScopeWidget scope;scope.snapshot=[&]{return source;};scope.ready=[&]{return source.valid();};int actualCallbacks=0;scope.captured=[&](const ToolSnapshot &base,const QByteArray &h,const QByteArray &bytes){++actualCallbacks;CHECK(wizard.observeScope(base,h,bytes));};scope.exchange=[&](const QByteArray &q,std::function<void(const QByteArray&)> reply){const auto result=q[9]==0?captureHeader:captureHeader+samples;QTimer::singleShot(0,&scope,[reply,result]{reply(result);});return true;};QPushButton *capture=nullptr;for(auto *button:scope.findChildren<QPushButton*>())if(button->text()=="Capture")capture=button;CHECK(capture);capture->click();for(int i=0;i<100&&actualCallbacks==0;++i)QTest::qWait(20);CHECK(actualCallbacks==1);capture->click();source.identity[20]=43;QTest::qWait(150);CHECK(actualCallbacks==1);
    const auto preview=qEnvironmentVariable("TUNUINO_TRIGGER_PREVIEW");if(!preview.isEmpty()){wizard.resize(wizard.sizeHint());wizard.show();app.processEvents();CHECK(wizard.grab().save(preview));wizard.hide();}
    native(false);native(true);std::cout<<"Trigger wizard planning, review gating, conflicts, no retry, ECU-only Scope evidence and two native writes with all unrelated pages preserved passed (host simulation)\n";
}
