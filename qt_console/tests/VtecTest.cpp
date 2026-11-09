#include "protocol/VtecCodec.h"
#include "parameters/ParameterWidget.h"
#include "ecu/MapWriteTransaction.h"
#include "calibration/VtecMonitor.h"
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
    QProcess process;process.start(exe,{"--vtec"});CHECK(process.waitForStarted(3000));
    const auto exchange=[&](const QByteArray &q){const auto line=q.toHex()+"\n";CHECK(process.write(line)==line.size());CHECK(process.waitForBytesWritten(3000));while(!process.canReadLine())CHECK(process.waitForReadyRead(3000));return QByteArray::fromHex(process.readLine().trimmed());};
    const auto identity=exchange("TUNUINO?");CHECK(VtecCodec::supported(identity));
    const auto readAll=[&]{MapWriteTransaction::Pages p;for(int page=1;page<=6;++page)for(int at=0;at<(page==1||page==4||page==6?256:576);at+=64)p[size_t(page-1)]+=exchange(read(page,at,64)).mid(1);return p;};
    auto base=readAll();auto h=base[0],a=base[3];QString error;
    SensorCalibration sensor;sensor.channel=1;sensor.raw={0,4095,0};sensor.values={0,250,0};h=Codec::apply(SensorKind::Map,sensor,h,a,error);CHECK(h.size()==256);
    sensor.raw={0,2048,4095};sensor.values={-20,40,100};h=Codec::apply(SensorKind::Clt,sensor,h,a,error);CHECK(h.size()==256);h[2]=1;
    sensor.channel=3;sensor.raw={300,3500,0};a=Codec::apply(SensorKind::Tps,sensor,h,a,error);CHECK(a.size()==256);
    for(int page:{1,4}){const auto bytes=page==1?h:a;for(int at=0;at<256;at+=64){auto q=read(page,at,64);q[0]='w';q+=bytes.mid(at,64);CHECK(exchange(q)==QByteArray(1,0));}}
    CHECK(ServiceDataCodec::u32(exchange("TUNUINO:V"),4)==0);CHECK(exchange(QByteArray::fromHex("620001"))==QByteArray(1,0));
    base=readAll();const auto generation=ServiceDataCodec::u32(exchange("TUNUINO:V"),8);
    auto desired=VtecCodec::defaults();desired[1]=1;desired[2]=10;CHECK(VtecCodec::allowed(desired,base[0],base[3],error));
    QByteArray mask(256,0);for(int i=0;i<28;++i)mask[i]=char(255);
    MapWriteTransaction writer;QQueue<QByteArray> pending;int copies=0,burns=0;bool success=false;
    writer.send=[&](const QByteArray &q,int){pending.enqueue(q);return true;};writer.savePrevious=[&](const QByteArray &id,quint32 gen,const MapWriteTransaction::Pages &pages){CHECK(id==identity&&gen==generation&&pages==base);++copies;return true;};
    writer.completed=[&](bool okay,bool uncertain,const QString &message){if(!okay)std::cerr<<message.toStdString()<<std::endl;CHECK(!uncertain);success=okay;};
    auto oldIdentity=identity;oldIdentity[18]=char(quint8(oldIdentity[18])&127);CHECK(!writer.startParameters(6,desired,base[5],oldIdentity,generation,mask));
    CHECK(writer.startParameters(6,desired,base[5],identity,generation,mask));while(writer.busy()){CHECK(!pending.isEmpty());const auto q=pending.dequeue();if(q[0]=='b')++burns;writer.receive(exchange(q));}
    CHECK(success&&copies==1&&burns==1);const auto actual=readAll();for(int i=0;i<6;++i)CHECK(actual[i]==(i==5?desired:base[i]));
    auto collision=desired;collision[2]=1;CHECK(!VtecCodec::allowed(collision,actual[0],actual[3],error));
    process.closeWriteChannel();CHECK(process.waitForFinished(3000)&&process.exitCode()==0);
    std::cout<<"VTEC verified six-page write against real firmware passed (host simulation).\n";
}
int main(int argc,char **argv){
    QApplication app(argc,argv);QString error;auto page=VtecCodec::defaults();CHECK(VtecCodec::format(page)&&VtecCodec::format(QByteArray(256,0)));auto invalid=page;Codec::put(invalid,6,5000);CHECK(!VtecCodec::format(invalid));invalid=page;invalid[255]=1;CHECK(!VtecCodec::format(invalid));
    ParameterBlock block;for(const auto &b:ParameterCatalog::blocks())if(b.key=="vtec.setup")block=b;CHECK(block.page==6&&block.fields.size()==15);
    ParameterWidget panel(block);QByteArray h(256,0),a(256,0),id(32,0);id[18]=char(192);panel.load(h,a,id,1,QByteArray(256,0));panel.setAvailable(true);QByteArray written,mask;panel.writeRequested=[&](int p,const QByteArray &bytes,const QByteArray &owned){CHECK(p==6);written=bytes;mask=owned;};panel.save();CHECK(written==page&&mask[0]==char(255));
    auto changed=page;Codec::put(changed,4,6000);panel.findChild<QDoubleSpinBox*>("rpmOn")->setValue(5500);panel.load(h,a,id,2,changed);written.clear();panel.save();CHECK(written.isEmpty());auto *discard=panel.findChild<QAction*>("primaryDiscard");CHECK(discard);discard->trigger();panel.save();CHECK(!written.isEmpty()&&Codec::u16(written,4)==6000);
    id[18]=64;panel.load(h,a,id,3);written.clear();panel.save();CHECK(written.isEmpty());
    panel.show();if(argc>1){panel.resize(720,610);app.processEvents();panel.grab().save(QString::fromLocal8Bit(argv[1]));}
    native();std::cout<<"VTEC codec, editor, conflicts and firmware capability checks passed.\n";
}
