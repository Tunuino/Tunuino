#include "protocol/VtecCodec.h"
#include "parameters/ParameterWidget.h"
#include "ecu/MapWriteTransaction.h"
#include "calibration/VvtMonitor.h"
#include <QApplication>
#include <QQueue>
#include <QProcess>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QTest>
#include <iostream>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed: "<<#x<<" at "<<__LINE__<<std::endl;std::exit(1);}}while(false)
using namespace tunuino;
using Codec=SensorCalibrationCodec;
static QByteArray read(int page,int at,int count){QByteArray q(7,0);q[0]='r';Codec::put(q,1,page);Codec::put(q,3,at);Codec::put(q,5,count);return q;}
static void native(){
    const auto exe=qEnvironmentVariable("TUNUINO_FIRMWARE_HARNESS");if(exe.isEmpty())return;
    QProcess process;process.start(exe,{"--vvt"});CHECK(process.waitForStarted(3000));
    const auto exchange=[&](const QByteArray &q){const auto line=q.toHex()+"\n";CHECK(process.write(line)==line.size());CHECK(process.waitForBytesWritten(3000));while(!process.canReadLine())CHECK(process.waitForReadyRead(3000));return QByteArray::fromHex(process.readLine().trimmed());};
    const auto identity=exchange("TUNUINO?");CHECK(VvtCodec::supported(identity));
    const auto readAll=[&]{MapWriteTransaction::Pages p;for(int page=1;page<=6;++page)for(int at=0;at<(page==1||page==4||page==6?256:576);at+=64)p[size_t(page-1)]+=exchange(read(page,at,64)).mid(1);return p;};
    auto base=readAll();auto h=base[0],a=base[3];QString error;
    SensorCalibration sensor;sensor.channel=1;sensor.raw={0,4095,0};sensor.values={0,250,0};h=Codec::apply(SensorKind::Map,sensor,h,a,error);CHECK(h.size()==256);
    sensor.raw={0,2048,4095};sensor.values={-20,40,100};h=Codec::apply(SensorKind::Clt,sensor,h,a,error);CHECK(h.size()==256);h[2]=1;h[154]=12;
    sensor.channel=3;sensor.raw={300,3500,0};a=Codec::apply(SensorKind::Tps,sensor,h,a,error);CHECK(a.size()==256);
    auto prefix=VtecCodec::defaults();prefix[1]=1;prefix[2]=10;
    for(int page:{1,4,6}){const auto bytes=page==1?h:page==4?a:prefix;for(int at=0;at<256;at+=64){auto q=read(page,at,64);q[0]='w';q+=bytes.mid(at,64);CHECK(exchange(q)==QByteArray(1,0));}}
    CHECK(ServiceDataCodec::u32(exchange("TUNUINO:V"),4)==0);CHECK(exchange(QByteArray::fromHex("620001"))==QByteArray(1,0));
    base=readAll();const auto generation=ServiceDataCodec::u32(exchange("TUNUINO:V"),8);
    auto desired=VvtCodec::defaults(base[5]);desired[65]=1;desired[66]=11;Codec::put(desired,84,9000);Codec::put(desired,90,2000);VvtCodec::bind(desired,base[0]);CHECK(VtecCodec::allowed(desired,base[0],base[3],error)&&VvtCodec::allowed(desired,base[0],base[3],error));
    QByteArray mask(256,0);for(int i=0;i<123;++i)mask[i]=char(255);
    MapWriteTransaction writer;QQueue<QByteArray> pending;int copies=0,burns=0;bool success=false;
    writer.send=[&](const QByteArray &q,int){pending.enqueue(q);return true;};writer.savePrevious=[&](const QByteArray &id,quint32 gen,const MapWriteTransaction::Pages &pages){CHECK(id==identity&&gen==generation&&pages==base);++copies;return true;};
    writer.completed=[&](bool okay,bool uncertain,const QString &message){if(!okay)std::cerr<<message.toStdString()<<std::endl;CHECK(!uncertain);success=okay;};
    auto oldIdentity=identity;oldIdentity[17]=0;CHECK(!writer.startParameters(6,desired,base[5],oldIdentity,generation,mask));
    CHECK(writer.startParameters(6,desired,base[5],identity,generation,mask));while(writer.busy()){CHECK(!pending.isEmpty());const auto q=pending.dequeue();if(q[0]=='b')++burns;writer.receive(exchange(q));}
    CHECK(success&&copies==1&&burns==1);const auto actual=readAll();for(int i=0;i<6;++i)CHECK(actual[i]==(i==5?desired:base[i]));
    CHECK(actual[5].mid(1,27)==prefix.mid(1,27));auto collision=desired;collision[66]=10;CHECK(!VvtCodec::allowed(collision,actual[0],actual[3],error));auto filter=actual[0];filter[98]=1;filter[99]=1;CHECK(!VvtCodec::allowed(desired,filter,actual[3],error));auto narrow=actual[0];narrow[154]=0;CHECK(!VvtCodec::allowed(desired,narrow,actual[3],error));
    process.closeWriteChannel();CHECK(process.waitForFinished(3000)&&process.exitCode()==0);
    std::cout<<"VVT verified six-page write with VTEC preserved against real firmware passed (host simulation).\n";
}
static void nativeMap(bool advanced=false){
    const auto exe=qEnvironmentVariable("TUNUINO_FIRMWARE_HARNESS");if(exe.isEmpty())return;
    QProcess process;process.start(exe,{advanced?"--cam-control":"--vvt-map"});CHECK(process.waitForStarted(3000));
    const auto exchange=[&](const QByteArray &q){const auto line=q.toHex()+"\n";CHECK(process.write(line)==line.size());CHECK(process.waitForBytesWritten(3000));while(!process.canReadLine())CHECK(process.waitForReadyRead(3000));return QByteArray::fromHex(process.readLine().trimmed());};
    const auto identity=exchange("TUNUINO?");CHECK(VvtCodec::tableSupported(identity));
    const auto readAll=[&]{MapWriteTransaction::Pages p;for(int page=1;page<=(advanced?8:7);++page)for(int at=0;at<(page==1||page==4||page==6?256:576);at+=64)p[size_t(page-1)]+=exchange(read(page,at,64)).mid(1);return p;};
    auto base=readAll();auto h=base[0];h[6]=1;Codec::put(h,16,25000);
    for(int at=0;at<256;at+=64){auto q=read(1,at,64);q[0]='w';q+=h.mid(at,64);CHECK(exchange(q)==QByteArray(1,0));}CHECK(exchange(QByteArray::fromHex("620001"))==QByteArray(1,0));
    base=readAll();quint32 generation=ServiceDataCodec::u32(exchange("TUNUINO:V"),8);
    MapPageValues v;for(int i=0;i<16;++i){v.rpm[i]=500+500*i;v.load[i]=20+10*i;}v.cells.fill(20);QString error;const auto table=MapPageCodec::encode(7,v,error);CHECK(table.size()==576&&MapPageCodec::valid(7,table));
    MapWriteTransaction writer;QQueue<QByteArray> pending;int copies=0,burns=0;bool success=false;
    writer.send=[&](const QByteArray &q,int){pending.enqueue(q);return true;};writer.savePrevious=[&](const QByteArray &id,quint32 gen,const MapWriteTransaction::Pages &pages){CHECK(id==identity&&gen==generation&&pages==base);++copies;return true;};
    writer.completed=[&](bool okay,bool uncertain,const QString &message){if(!okay)std::cerr<<message.toStdString()<<std::endl;CHECK(!uncertain);success=okay;};
    const auto pump=[&]{int guard=0;while(writer.busy()){CHECK(!pending.isEmpty()&&++guard<1000);const auto q=pending.dequeue();if(q[0]=='b')++burns;writer.receive(exchange(q));}};
    const int targetPage=advanced?8:7;
    auto older=identity;older[17]=1;CHECK(!writer.start(targetPage,table,base[targetPage-1],older,generation));
    CHECK(writer.start(targetPage,table,base[targetPage-1],identity,generation));pump();CHECK(success&&copies==1&&burns==1);
    auto actual=readAll();for(int i=0;i<(advanced?8:7);++i)CHECK(actual[i]==(i==targetPage-1?table:base[i]));base=actual;generation=writer.generation();
    auto control=VvtCodec::defaults(VtecCodec::defaults());if(advanced)control=VvtCodec::advanced(control,true);else control[64]=2;control[123]=advanced?2:1;CHECK(VvtCodec::tableAllowed(control,base[0],base[targetPage-1],error));auto noMap=base[0];noMap[6]=0;CHECK(!VvtCodec::tableAllowed(control,noMap,base[targetPage-1],error));auto outside=table;Codec::put(outside,64,5000);CHECK(!VvtCodec::tableAllowed(control,base[0],outside,error));
    QByteArray mask(256,char(255));CHECK(writer.startParameters(6,control,base[5],identity,generation,mask));pump();CHECK(success&&copies==2&&burns==2);actual=readAll();for(int i=0;i<(advanced?8:7);++i)CHECK(actual[i]==(i==5?control:base[i]));
    base=actual;generation=writer.generation();success=true;CHECK(writer.start(targetPage,outside,base[targetPage-1],identity,generation));pump();CHECK(!success&&copies==2&&burns==2&&readAll()==base);
    process.closeWriteChannel();CHECK(process.waitForFinished(3000)&&process.exitCode()==0);
    std::cout<<"VVT target seven-page native write, source selection, backup and travel rejection passed (host simulation).\n";
}
int main(int argc,char **argv){
    QApplication app(argc,argv);QString error;const auto prefix=VtecCodec::defaults();const auto page=VvtCodec::defaults(prefix);CHECK(VtecCodec::format(page)&&VvtCodec::extension(page)&&page.mid(1,27)==prefix.mid(1,27));
    auto invalid=page;Codec::put(invalid,90,5000);CHECK(!VtecCodec::format(invalid));invalid=page;invalid[255]=1;CHECK(!VvtCodec::extension(invalid));
    ParameterBlock parameters,pid,vtec;for(const auto &block:ParameterCatalog::blocks()){if(block.key=="vvt.parameters")parameters=block;else if(block.key=="vvt.pid")pid=block;else if(block.key=="vtec.setup")vtec=block;}
    CHECK(parameters.page==6&&pid.fields.size()==8);QByteArray h(256,0),a(256,0),id(32,0);id[17]=7;id[18]=char(192);
    ParameterWidget setup(parameters);setup.load(h,a,id,1,prefix);setup.setAvailable(true);CHECK(setup.findChild<QAction*>("vvtCaptureReference"));CHECK(!setup.findChild<QDoubleSpinBox*>("oilTemp")->isEnabled());CHECK(!setup.findChild<QDoubleSpinBox*>("overriddenDc")->isEnabled());
    QByteArray written,owned;setup.writeRequested=[&](int p,const QByteArray &bytes,const QByteArray &mask){CHECK(p==6);written=bytes;owned=mask;};setup.save();CHECK(written==VvtCodec::advanced(page,true));
    setup.findChild<QDoubleSpinBox*>("reference")->setValue(90);setup.save();CHECK(VvtCodec::u32(written,82)==9000&&written.mid(1,27)==prefix.mid(1,27));
    auto stored=written;ParameterWidget controller(pid);controller.load(h,a,id,1,stored);controller.setAvailable(true);controller.findChild<QDoubleSpinBox*>("deadband")->setValue(.75);controller.findChild<QDoubleSpinBox*>("outputMax")->setValue(17);controller.writeRequested=[&](int,const QByteArray &bytes,const QByteArray &mask){written=bytes;CHECK(mask[140]&&mask[138]&&!mask[65]);};controller.save();CHECK(Codec::u16(written,140)==75&&Codec::u16(written,138)==1700&&written.mid(1,75)==stored.mid(1,75));
    ParameterWidget legacy(vtec);legacy.load(h,a,id,1,written);legacy.setAvailable(true);legacy.writeRequested=[&](int,const QByteArray &bytes,const QByteArray&){CHECK(bytes[0]==2&&bytes.mid(64)==written.mid(64));};legacy.save();
    setup.saved();setup.load(h,a,id,2,written);setup.findChild<QDoubleSpinBox*>("reference")->setValue(91);auto unrelated=written;Codec::put(unrelated,4,6000);setup.load(h,a,id,2,unrelated);written.clear();setup.save();CHECK(!written.isEmpty()&&VvtCodec::u32(written,82)==9100&&Codec::u16(written,4)==6000);
    auto conflict=written;Codec::put(conflict,84,9200);setup.load(h,a,id,3,conflict);written.clear();setup.save();CHECK(written.isEmpty());setup.findChild<QAction*>("primaryDiscard")->trigger();setup.save();CHECK(VvtCodec::u32(written,82)==9200);
    auto draft=setup.editorState();auto draftValues=draft["values"].toObject();draftValues["overrideDc"]=1;draftValues["overriddenDc"]=35;draftValues["overrideTarget"]=1;draftValues["overriddenTarget"]=10;CHECK(setup.restoreEditor(QJsonObject{{"values",draftValues}},true,error));setup.save();CHECK(written[125]==1&&written[128]==1&&Codec::u16(written,126)==3500&&Codec::u16(written,130)==1000);
    id[17]=0;setup.load(h,a,id,4,prefix);written.clear();setup.save();CHECK(written.isEmpty());
    const QJsonObject oldBlocks{{"vvt.position",QJsonObject{{"values",QJsonObject{{"reference",12.34},{"maximum",45}}}}},{"vvt.actuator",QJsonObject{{"values",QJsonObject{{"kp",2.5},{"ki",1.0},{"enabled",1}}}}},{"vvt.activation",QJsonObject{{"values",QJsonObject{{"runtime",12345}}}}}};const auto migrated=ParameterCatalog::migrateVvt(oldBlocks);CHECK(migrated["vvt.pid"].toObject()["values"].toObject()["iMax"].toDouble()==100&&migrated["vvt.pid"].toObject()["values"].toObject()["deadband"].toDouble()==0);CHECK(migrated.size()==2&&migrated["vvt.parameters"].toObject()["values"].toObject()["runtime"].toDouble()==12.345&&migrated["vvt.pid"].toObject()["values"].toObject()["kp"].toDouble()==2.5&&migrated["vvt.parameters"].toObject()["values"].toObject()["reference"].toDouble()==12.34);
    if(argc>1){setup.resize(620,620);setup.show();app.processEvents();setup.grab().save(QString::fromLocal8Bit(argv[1]));}
    native();nativeMap();nativeMap(true);std::cout<<"CAM parameters/PID layout, advanced codec, independent eight-page native writes, migration and conflicts passed.\n";
}
