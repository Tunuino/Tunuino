#include "protocol/VtecCodec.h"
#include "parameters/ParameterWidget.h"
#include "ecu/MapWriteTransaction.h"
#include "calibration/VvtMonitor.h"
#include "protocol/Vvt2Codec.h"
#include "protocol/EtbCodec.h"
#include "dbw/EtbAutotune.h"
#include "calibration/CamSignalWidget.h"
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

static void nativeDual(){
 const auto exe=qEnvironmentVariable("TUNUINO_FIRMWARE_HARNESS");if(exe.isEmpty())return;
 QProcess process;process.start(exe,{"--dual-cam"});CHECK(process.waitForStarted(3000));
 const auto exchange=[&](const QByteArray &q){const auto line=q.toHex()+"\n";CHECK(process.write(line)==line.size());CHECK(process.waitForBytesWritten(3000));while(!process.canReadLine()){if(!process.waitForReadyRead(3000)){std::cerr<<"Dual firmware request failed: "<<q.toHex().constData()<<" exit "<<process.exitCode()<<" stderr "<<process.readAllStandardError().constData()<<std::endl;CHECK(false);}}return QByteArray::fromHex(process.readLine().trimmed());};
 const auto id=exchange("TUNUINO?");CHECK(Vvt2Codec::supported(id)&&Vvt2Codec::pulsesSupported(id));
 MapWriteTransaction::Pages base;for(int page=1;page<=11;++page)for(int at=0;at<(page==1||page==4||page==6||page==9?256:576);at+=64)base[page-1]+=exchange(read(page,at,64)).mid(1);
 CHECK(Vvt2Codec::format(base[8])&&MapPageCodec::valid(10,base[9])&&MapPageCodec::valid(11,base[10]));
 auto desired=Vvt2Codec::defaults();Vvt2Codec::bind(desired,base[0]);CHECK(Vvt2Codec::format(desired));
 const quint32 generation=ServiceDataCodec::u32(exchange("TUNUINO:V"),8);QQueue<QByteArray> requests;bool done=false,success=false;int snapshots=0;QString result;
 MapWriteTransaction transaction;transaction.send=[&](const QByteArray &q,int){requests.enqueue(q);return true;};transaction.savePrevious=[&](const QByteArray &identity,quint32 g,const MapWriteTransaction::Pages &pages){CHECK(identity==id&&g==generation&&pages==base);++snapshots;return true;};transaction.completed=[&](bool ok,bool uncertain,const QString &message){done=true;success=ok;result=message;CHECK(!uncertain);};
 CHECK(transaction.startParameters(9,desired,base[8],id,generation,QByteArray(256,char(255))));
 int limit=0;while(!done&&++limit<2000){CHECK(!requests.isEmpty());const auto request=requests.dequeue();transaction.receive(exchange(request));}if(!success)std::cerr<<result.toStdString()<<std::endl;CHECK(done&&success&&snapshots==1);
 CHECK(exchange(read(9,0,64)).mid(1)==desired.left(64));CHECK(exchange(read(6,0,64)).mid(1)==base[5].left(64));
 // Both native queries carry the new generation; no simulated phase is invented.
 const auto phase=exchange("TUNUINO:G"),pulses=exchange("TUNUINO:C");CHECK(phase.size()==32&&phase[2]==0&&ServiceDataCodec::u32(phase,24)==generation+1);CHECK(pulses.size()==28&&pulses[2]==0&&ServiceDataCodec::u32(pulses,20)==generation+1);
 // Both new signed target maps can be written through the same full readback transaction.
 for(int page:{10,11}){
  MapPageValues values;for(int i=0;i<16;++i){values.rpm[i]=500+500*i;values.load[i]=20+10*i;}for(auto &cell:values.cells)cell=page==10?12.34:-5.67;QString error;const auto target=MapPageCodec::encode(page,values,error);CHECK(target.size()==576);
  MapWriteTransaction::Pages previous;for(int p=1;p<=11;++p)for(int at=0;at<(p==1||p==4||p==6||p==9?256:576);at+=64)previous[p-1]+=exchange(read(p,at,64)).mid(1);
  done=false;success=false;const auto g=ServiceDataCodec::u32(exchange("TUNUINO:V"),8);
  transaction.savePrevious=[&](const QByteArray&,quint32 savedGeneration,const MapWriteTransaction::Pages &pages){CHECK(savedGeneration==g&&pages==previous);return true;};
  CHECK(transaction.start(page,target,previous[page-1],id,g));limit=0;while(!done&&++limit<2000){CHECK(!requests.isEmpty());transaction.receive(exchange(requests.dequeue()));}if(!success)std::cerr<<result.toStdString()<<std::endl;CHECK(done&&success);
  CHECK(exchange(read(page,64,64)).mid(1)==target.mid(64,64));
 }
 process.closeWriteChannel();CHECK(process.waitForFinished(3000)&&process.exitCode()==0);
}


static void etbUi(){
 ToolSnapshot source;source.identity=QByteArray(32,0);source.identity.replace(1,7,"TUNUINO");source.identity[8]=source.identity[9]=source.identity[10]=1;source.identity[12]=7;source.identity[14]=2;source.identity[17]=63;source.identity[18]=char(255);source.identity[19]=char(255);source.identity[20]=42;source.generation=4;
 source.pages[0]=QByteArray(256,0);Codec::put(source.pages[0],0,2);source.pages[3]=QByteArray(256,0);source.pages[5]=source.pages[8]=QByteArray(256,0);source.pages[11]=EtbCodec::defaults();source.pages[11][49]=1;source.pages[11][129]=2;
 MapPageValues grid;for(int i=0;i<16;++i){grid.rpm[i]=500+500*i;grid.load[i]=20+10*i;}QString error;for(int p:{2,3,5,7,8,10,11})source.pages[p-1]=MapPageCodec::encode(p,grid,error);CHECK(source.valid()&&EtbCodec::supported(source.identity));
 EtbAutotune tuner(0);tuner.snapshot=[&]{return source;};tuner.ready=[] {return true;};int writes=0,applications=0,starts=0;bool owned=false;int phase=0;
 tuner.busyChanged=[&](bool b){owned=b;};tuner.pidWriteEnabled=[] {return true;};tuner.pidPending=[] {return false;};tuner.writePid=[&]{++writes;};tuner.applyCandidate=[&](double p,double i,double d,QString&){CHECK(p==.7&&i==.2&&d==.04);++applications;return true;};
 tuner.exchange=[&](const QByteArray &q,std::function<void(const QByteArray&)> callback){CHECK(q.startsWith("TUNUINO:X")&&quint8(q[10])==0);int op=quint8(q[9]);if(op==13){++starts;phase=1;callback(QByteArray(1,0));}else if(op==15){phase=3;callback(QByteArray(1,0));}else if(op==14){phase=4;callback(QByteArray(1,0));}else {CHECK(op==12);QByteArray p(64,0);p[1]=1;p[4]=1;Codec::put(p,10,source.generation);Codec::put(p,12,4000);Codec::put(p,14,4000);Codec::put(p,20,2100);Codec::put(p,22,2900);p[28]=char(phase);Codec::put(p,32,700);Codec::put(p,34,200);Codec::put(p,36,40);p[48]=p[49]=p[50]=1;p[51]=3;callback(p);}return true;};
 tuner.show();QApplication::processEvents();tuner.start();CHECK(starts==1&&tuner.busy()&&owned&&writes==0&&applications==0);tuner.poll();CHECK(!tuner.busy()&&!owned&&writes==0&&applications==0);auto *apply=tuner.findChild<QPushButton*>("etbTuneApply");CHECK(apply&&apply->isEnabled());apply->click();CHECK(applications==1&&writes==0);
 tuner.start();tuner.poll();CHECK(apply->isEnabled());++source.generation;phase=0;tuner.poll();CHECK(!apply->isEnabled());apply->click();CHECK(applications==1&&writes==0);tuner.hide();
}
static void nativeEtb(){
 const auto exe=qEnvironmentVariable("TUNUINO_FIRMWARE_HARNESS");if(exe.isEmpty())return;QProcess process;process.start(exe,{"--etb"});CHECK(process.waitForStarted(3000));
 const auto exchange=[&](const QByteArray &q){const auto line=q.toHex()+"\n";CHECK(process.write(line)==line.size());CHECK(process.waitForBytesWritten(3000));while(!process.canReadLine())CHECK(process.waitForReadyRead(3000));return QByteArray::fromHex(process.readLine().trimmed());};
 const auto id=exchange("TUNUINO?");CHECK(EtbCodec::supported(id));
 const auto readAll=[&]{MapWriteTransaction::Pages pages;for(int p=1;p<=12;++p)for(int at=0;at<(p==1||p==4||p==6||p==9||p==12?256:576);at+=64)pages[p-1]+=exchange(read(p,at,64)).mid(1);return pages;};
 auto base=readAll();CHECK(EtbCodec::format(base[11]));auto desired=EtbCodec::defaults();for(int i=0;i<6;++i)desired[1+i]=char(i==2?0:i+1);desired[49]=1;desired[129]=2;QString error;CHECK(EtbCodec::allowed(desired,base[0],base[3],error));
 MapWriteTransaction transaction;QQueue<QByteArray> queue;bool complete=false,success=false;int snapshots=0;const auto generation=ServiceDataCodec::u32(exchange("TUNUINO:V"),8);
 transaction.send=[&](const QByteArray &q,int){queue.enqueue(q);return true;};transaction.savePrevious=[&](const QByteArray &identity,quint32 gen,const MapWriteTransaction::Pages &p){CHECK(identity==id&&gen==generation&&p==base);++snapshots;return true;};transaction.completed=[&](bool ok,bool uncertain,const QString &message){if(!ok)std::cerr<<message.toStdString()<<std::endl;CHECK(!uncertain);complete=true;success=ok;};
 CHECK(transaction.startParameters(12,desired,base[11],id,generation,QByteArray(256,char(255))));int limit=0;while(!complete&&++limit<3000){CHECK(!queue.isEmpty());transaction.receive(exchange(queue.dequeue()));}CHECK(success&&snapshots==1);const auto actual=readAll();for(int i=0;i<12;++i)CHECK(actual[i]==(i==11?desired:base[i]));
 auto old=id;old[17]=char(quint8(old[17])&~32);MapWriteTransaction unsupported;bool sent=false;unsupported.send=[&](const QByteArray&,int){sent=true;return true;};CHECK(!unsupported.startParameters(12,desired,base[11],old,generation,QByteArray(256,char(255)))&&!sent);
 auto conflict=desired;conflict[129]=1;CHECK(!EtbCodec::format(conflict));conflict=desired;conflict[2]=conflict[1];CHECK(!EtbCodec::format(conflict));
 auto block=ParameterCatalog::blocks();const auto found=std::find_if(block.begin(),block.end(),[](const auto &b){return b.key=="etb.2.pid";});CHECK(found!=block.end());ParameterWidget pid(*found);pid.load(base[0],base[3],id,generation+1,desired);pid.setAvailable(true);auto state=pid.editorState();auto values=state["values"].toObject();values["kp"]=.85;state["values"]=values;CHECK(pid.restoreEditor(state,true,error));QByteArray written;pid.writeRequested=[&](int p,const QByteArray &v,const QByteArray &){CHECK(p==12);written=v;};pid.save();CHECK(written.size()==256&&Codec::u16(written,134)==850&&written.mid(48,80)==desired.mid(48,80));
 process.closeWriteChannel();CHECK(process.waitForFinished(3000)&&process.exitCode()==0);
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
    const QJsonObject oldBlocks{{"vvt.position",QJsonObject{{"values",QJsonObject{{"reference",12.34},{"maximum",45}}}}},{"vvt.actuator",QJsonObject{{"values",QJsonObject{{"kp",2.5},{"ki",1.0},{"enabled",1}}}}},{"vvt.activation",QJsonObject{{"values",QJsonObject{{"runtime",12345}}}}}};const auto migrated=ParameterCatalog::migrateVvt(oldBlocks);CHECK(migrated["vvt.pid"].toObject()["values"].toObject()["iMax"].toDouble()==100&&migrated["vvt.pid"].toObject()["values"].toObject()["deadband"].toDouble()==0);CHECK(migrated.size()==11&&migrated["vvt.parameters"].toObject()["values"].toObject()["runtime"].toDouble()==12.345&&migrated["vvt.pid"].toObject()["values"].toObject()["kp"].toDouble()==2.5&&migrated["vvt.parameters"].toObject()["values"].toObject()["reference"].toDouble()==12.34);
    if(argc>1){setup.resize(620,620);setup.show();app.processEvents();setup.grab().save(QString::fromLocal8Bit(argv[1]));}
    etbUi();native();nativeMap();nativeMap(true);nativeDual();nativeEtb();std::cout<<"CAM parameters/PID layout, advanced codec, independent eight-page native writes, migration and conflicts passed.\n";
}
