#include "protocol/VtecCodec.h"
#include "parameters/ParameterWidget.h"
#include "ecu/MapWriteTransaction.h"
#include "calibration/VvtMonitor.h"
#include <QApplication>
#include <QQueue>
#include <QProcess>
#include <QDoubleSpinBox>
#include <QCheckBox>
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
int main(int argc,char **argv){
    QApplication app(argc,argv);QString error;const auto prefix=VtecCodec::defaults();const auto page=VvtCodec::defaults(prefix);CHECK(VtecCodec::format(page)&&VvtCodec::extension(page)&&page.mid(1,27)==prefix.mid(1,27));
    auto invalid=page;Codec::put(invalid,90,5000);CHECK(!VtecCodec::format(invalid));invalid=page;invalid[255]=1;CHECK(!VvtCodec::extension(invalid));
    ParameterBlock position,actuator,target,vtec;for(const auto &block:ParameterCatalog::blocks()){if(block.key=="vvt.position")position=block;else if(block.key=="vvt.actuator")actuator=block;else if(block.key=="vvt.target")target=block;else if(block.key=="vtec.setup")vtec=block;}
    CHECK(position.page==6&&actuator.fields.size()==12);QByteArray h(256,0),a(256,0),id(32,0);id[17]=1;id[18]=char(192);
    ParameterWidget setup(actuator);setup.load(h,a,id,1,prefix);setup.setAvailable(true);QByteArray written;setup.writeRequested=[&](int p,const QByteArray &bytes,const QByteArray&){CHECK(p==6);written=bytes;};setup.save();CHECK(written==page);
    ParameterWidget reference(position);reference.load(h,a,id,1,page);reference.setAvailable(true);CHECK(reference.findChild<QAction*>("vvtCaptureReference"));reference.findChild<QDoubleSpinBox*>("reference")->setValue(90);reference.writeRequested=[&](int,const QByteArray &bytes,const QByteArray&){written=bytes;};reference.save();CHECK(VvtCodec::u32(written,82)==9000&&written.mid(1,27)==prefix.mid(1,27));
    ParameterWidget legacy(vtec);legacy.load(h,a,id,1,written);legacy.setAvailable(true);legacy.writeRequested=[&](int,const QByteArray &bytes,const QByteArray&){CHECK(bytes[0]==2&&bytes.mid(64)==written.mid(64));};legacy.save();
    reference.saved();reference.load(h,a,id,2,written);
    // Independent page-6 edits rebase; edits to owned fields require discard.
    reference.findChild<QDoubleSpinBox*>("reference")->setValue(91);auto unrelated=written;Codec::put(unrelated,4,6000);reference.load(h,a,id,2,unrelated);written.clear();reference.save();CHECK(!written.isEmpty()&&VvtCodec::u32(written,82)==9100&&Codec::u16(written,4)==6000);
    auto conflict=written;Codec::put(conflict,84,9200);reference.load(h,a,id,3,conflict);written.clear();reference.save();CHECK(written.isEmpty());reference.findChild<QAction*>("primaryDiscard")->trigger();reference.save();CHECK(VvtCodec::u32(written,82)==9200);
    id[17]=0;setup.load(h,a,id,4,prefix);written.clear();setup.save();CHECK(written.isEmpty());
    if(argc>1){setup.resize(720,610);setup.show();app.processEvents();setup.grab().save(QString::fromLocal8Bit(argv[1]));}
    native();std::cout<<"VVT codec, controller editor, shared-page preservation, conflicts and old firmware checks passed.\n";
}
