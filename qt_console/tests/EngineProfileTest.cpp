#include "parameters/EngineProfileDraft.h"
#include "parameters/ParameterWidget.h"
#include "ecu/MapWriteTransaction.h"
#include <QApplication>
#include <QProcess>
#include <QQueue>
#include <QJsonArray>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QToolButton>
#include <QScrollArea>
#include <QLabel>
#include <QTest>
#include <QTimer>
#include <QMessageBox>
#include <QToolBar>
#include <iostream>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed: "<<#x<<" line "<<__LINE__<<std::endl;std::exit(1);}}while(false)
using namespace tunuino;using Codec=SensorCalibrationCodec;using Writer=MapWriteTransaction;
static quint32 u32(const QByteArray &b,int at){return (quint32(Codec::u16(b,at))<<16)|quint32(Codec::u16(b,at+2));}
static QByteArray read(int page,int at,int count){QByteArray q(7,0);q[0]='r';Codec::put(q,1,page);Codec::put(q,3,at);Codec::put(q,5,count);return q;}
int main(int argc,char **argv){QApplication app(argc,argv);QByteArray header(256,0),aux(256,0);Codec::put(header,0,2);header[3]=36;header[4]=1;Codec::put(header,96,12000);QString error;EngineProfileDraft draft;draft.load(header,aux);
    CHECK(draft.count()==4&&draft.build(error).isEmpty());for(int i=0;i<4;++i){CHECK(draft.cylinders[i].injector==-1&&draft.cylinders[i].coil==-1);CHECK(draft.assign(i,false,i,error)&&draft.assign(i,true,i,error));}
    auto built=draft.build(error);CHECK(built.size()==256&&built.left(128)==header.left(128)&&built[128]==1&&built[129]==4);
    CHECK(!draft.assign(1,false,0,error)&&draft.cylinders[1].injector==1);CHECK(draft.resize(6));CHECK(draft.cylinders[0].injector==0&&draft.cylinders[4].injector==-1&&draft.cylinders[5].coil==-1&&draft.build(error).isEmpty());
    for(int i=4;i<6;++i){CHECK(draft.assign(i,false,i,error));CHECK(draft.assign(i,true,i,error));}CHECK(!draft.build(error).isEmpty());CHECK(draft.resize(2)&&draft.resize(6));CHECK(draft.cylinders[5].injector==5);
    draft.resize(4);draft.operation=3;CHECK(draft.assign(2,true,0,error)&&draft.assign(3,true,1,error));CHECK(!draft.build(error).isEmpty());CHECK(draft.assign(0,true,5,error));CHECK(draft.cylinders[0].coil==5&&draft.cylinders[2].coil==5);CHECK(!draft.assign(1,true,5,error));
    draft.resize(3);CHECK(draft.build(error).isEmpty());draft.resize(4);draft.operation=0;for(int i=0;i<4;++i)draft.cylinders[i].coil=i;draft.setTdc(1,17500);CHECK(draft.cylinders[1].tdc==17500);draft.setOrder(0,1);draft.setOrder(1,0);CHECK(draft.cylinders[1].tdc==0&&draft.cylinders[0].tdc==17500);
    EngineProfileDraft imported;imported.load(header,aux);CHECK(imported.import(draft.json(),error)&&imported.build(error)==draft.build(error));auto invalid=draft.json();invalid["operation"]=9;CHECK(!imported.import(invalid,error));invalid=draft.json();auto events=invalid["order"].toArray();events[0]=events[1];invalid["order"]=events;CHECK(!imported.import(invalid,error));
    auto pumpHeader=built;pumpHeader[150]=1;EngineProfileDraft conflict;conflict.load(pumpHeader,aux);CHECK(!conflict.assign(1,false,0,error));
    EngineProfileDraft paired;paired.load(built,aux);paired.operation=3;CHECK(paired.assign(2,true,0,error)&&paired.assign(3,true,1,error));const auto pairHeader=paired.build(error);CHECK(!pairHeader.isEmpty());
    ParameterBlock coilBlock;for(const auto &block:ParameterCatalog::blocks())if(block.key=="coils.outputs")coilBlock=block;ParameterWidget coils(coilBlock);coils.load(pairHeader,aux,QByteArray(32,42),1);coils.setAvailable(true);auto *coilsTable=coils.findChild<QTableWidget*>("parameters");CHECK(coilsTable->isRowHidden(2)&&coilsTable->isRowHidden(3));auto *selection=coils.findChild<QComboBox*>("cylinder0");CHECK(selection);selection->addItem("Spare IGN6",5);selection->setCurrentIndex(2);bool pairWritten=false;coils.writeRequested=[&](int,const QByteArray &result,const QByteArray&){pairWritten=true;CHECK(result[161]==5&&result[177]==5&&result[169]==1&&result[185]==1);};coils.save();CHECK(pairWritten);
    ParameterBlock firing;for(const auto &block:ParameterCatalog::blocks())if(block.key=="ignition.firing")firing=block;CHECK(!firing.key.isEmpty());ParameterWidget widget(firing);QByteArray identity(32,0);identity[20]=42;widget.load(built,aux,identity,2);widget.setAvailable(true);
    CHECK(widget.findChild<QTableWidget*>("parameters")->horizontalHeader()->isHidden());auto *count=widget.findChild<QDoubleSpinBox*>("cylinders");CHECK(count&&count->isEnabled());count->setValue(6);CHECK(widget.findChild<QTableWidget*>("engineProfileAssignments")->rowCount()==6);CHECK(widget.findChild<QComboBox*>("engineInjector4")->currentText()=="Not assigned");bool emitted=false;widget.writeRequested=[&](int,const QByteArray&,const QByteArray&){emitted=true;};widget.save();CHECK(!emitted);const auto message=widget.findChild<QLabel*>("parameterStatus")->text();widget.setAvailable(true);CHECK(widget.findChild<QLabel*>("parameterStatus")->text()==message);
    // Firing-order defaults must not reset hidden fuel or coil calibration.
    auto calibrated=built;Codec::put(calibrated,130,6200);Codec::put(calibrated,132,3300);Codec::put(calibrated,134,400);Codec::put(calibrated,136,17000);calibrated[155]=1;
    ParameterWidget defaults(firing);defaults.load(calibrated,aux,identity,2);defaults.setAvailable(true);
    QTimer::singleShot(0,&defaults,[]{for(auto *window:QApplication::topLevelWidgets())if(auto *box=qobject_cast<QMessageBox*>(window))box->button(QMessageBox::Yes)->click();});
    defaults.findChild<QToolBar*>("parameterToolbar")->actions()[2]->trigger();bool defaultsWritten=false;defaults.writeRequested=[&](int,const QByteArray &result,const QByteArray&){defaultsWritten=true;CHECK(result.mid(130,8)==calibrated.mid(130,8)&&result[155]==0);};defaults.save();CHECK(defaultsWritten);
    // Unrelated auxiliary calibration updates can rebase the draft safely.
    const auto newAux=Codec::featureDefaults(Codec::tpsDefaults());widget.load(built,newAux,identity,3);CHECK(widget.findChild<QPushButton*>("parameterSave")->isEnabled());
    // A changed ECU profile cannot silently overwrite local drafts.
    auto changed=built;Codec::put(changed,130,6000);widget.load(changed,newAux,identity,4);CHECK(!widget.findChild<QPushButton*>("parameterSave")->isEnabled());
    const auto executable=qEnvironmentVariable("TUNUINO_FIRMWARE_HARNESS");if(executable.isEmpty()){std::cout<<"Profile draft creation, resize, explicit outputs, shared coils, nonuniform timing, JSON and UI conflict guards passed; firmware harness not supplied\n";return 0;}
    QProcess process;process.start(executable,QStringList{});CHECK(process.waitForStarted(3000));const auto exchange=[&](const QByteArray &q){const auto line=q.toHex()+"\n";CHECK(process.write(line)==line.size());CHECK(process.waitForBytesWritten(3000));while(!process.canReadLine())CHECK(process.waitForReadyRead(3000));return QByteArray::fromHex(process.readLine().trimmed());};
    const auto uid=exchange("TUNUINO?");quint32 generation=u32(exchange("TUNUINO:V"),8);int writes=0;
    for(int scenario=0;scenario<7;++scenario){Writer::Pages base;for(int p=1;p<=4;++p)for(int at=0;at<(p==1||p==4?256:576);at+=64)base[p-1]+=exchange(read(p,at,64)).mid(1);EngineProfileDraft model;model.load(base[0],base[3]);
        if(scenario==0){for(int i=0;i<4;++i){CHECK(model.assign(i,false,i,error));CHECK(model.assign(i,true,i,error));}}
        if(scenario==1){model.resize(6);for(int i=4;i<6;++i){CHECK(model.assign(i,false,i,error));CHECK(model.assign(i,true,i,error));}}
        if(scenario==2){model.operation=1;}
        if(scenario==3){model.operation=2;}
        if(scenario==4){model.resize(4);model.operation=3;CHECK(model.assign(2,true,0,error)&&model.assign(3,true,1,error));}
        if(scenario==5){CHECK(model.assign(0,true,5,error)&&model.cylinders[2].coil==5);}
        if(scenario==6){model.operation=4;model.inverted=true;}
        const auto desired=model.build(error);if(desired.isEmpty())std::cerr<<error.toStdString()<<std::endl;CHECK(desired.size()==256);CHECK(desired.left(128)==base[0].left(128));QByteArray mask(256,0);for(int i=128;i<256;++i)mask[i]=char(255);
        Writer writer;QQueue<QByteArray> pending;bool success=false;int copies=0,burns=0;
        writer.send=[&](const QByteArray &q,int){CHECK(!q.startsWith("ARM")&&!q.startsWith("RUN"));pending.enqueue(q);return true;};writer.savePrevious=[&](const QByteArray &id,quint32 gen,const Writer::Pages &pages){CHECK(id==uid&&gen==generation&&pages==base);++copies;return true;};writer.completed=[&](bool ok,bool uncertain,const QString &msg){if(!ok)std::cerr<<msg.toStdString()<<std::endl;CHECK(!uncertain);success=ok;};
        CHECK(writer.startParameters(1,desired,base[0],uid,generation,mask));while(writer.busy()){CHECK(!pending.isEmpty());const auto q=pending.dequeue();if(q[0]=='b')++burns;writer.receive(exchange(q));}CHECK(success&&copies==1&&burns==1);++writes;generation=writer.generation();
        for(int p=1;p<=4;++p){QByteArray actual;for(int at=0;at<(p==1||p==4?256:576);at+=64)actual+=exchange(read(p,at,64)).mid(1);CHECK(actual==(p==1?desired:base[p-1]));}
    }
    CHECK(writes==7);process.closeWriteChannel();CHECK(process.waitForFinished(3000)&&process.exitCode()==0);std::cout<<"Seven native firmware writes: create, resize, all five modes, shared-pair move and inverted phase; one Burn and all unrelated pages preserved (host simulation)\n";
}
