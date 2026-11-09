#include "ecu/MapWriteTransaction.h"
#include "protocol/MapPageCodec.h"
#include <QCoreApplication>
#include <QQueue>
#include <QProcess>
#include <cstdlib>
#include <iostream>
#define CHECK(condition) do {if(!(condition)){std::cerr<<"Failed: "<<#condition<<" at "<<__LINE__<<std::endl;std::exit(1);}}while(false)
using Writer=tunuino::MapWriteTransaction;
static quint32 u32(const QByteArray &b,int at){return (quint32(quint8(b[at]))<<24)|(quint32(quint8(b[at+1]))<<16)|(quint32(quint8(b[at+2]))<<8)|quint8(b[at+3]);}
static int u16(const QByteArray &b,int at){return (int(quint8(b[at]))<<8)|quint8(b[at+1]);}
static void put32(QByteArray &b,int at,quint32 v){for(int i=0;i<4;++i)b[at+i]=char(v>>(24-8*i));}
static QByteArray read(int page,int at,int count){QByteArray q(7,0);q[0]='r';q[2]=char(page);q[3]=char(at>>8);q[4]=char(at);q[5]=char(count>>8);q[6]=char(count);return q;}
struct FakeEcu {
    Writer::Pages pages,active;
    QByteArray identity=QByteArray(32,0);
    quint32 generation=5;
    int writes=0,burns=0,runtimes=0;
    bool dirty=false,persisted=true,armed=false,badIdentity=false,badValidation=false,
        badCrc=false,corruptStage=false,corruptPermanent=false,failBurn=false,notPersisted=false,
        losePowerBeforeBurn=false,changedIdentityBeforeBurn=false,engineStartsBeforeBurn=false;
    int rejectWrite=0;
    FakeEcu() {
        identity.replace(1,7,"TUNUINO");identity[8]=1;identity[9]=1;identity[10]=1;identity[12]=7;identity[14]=2;identity[19]=15;identity[20]=42;
        tunuino::MapPageValues v;
        for(int i=0;i<16;++i){v.rpm[i]=500+400*i;v.load[i]=20+10*i;}
        QString error;v.cells.fill(50);pages[1]=tunuino::MapPageCodec::encode(2,v,error);
        v.cells.fill(10);pages[2]=tunuino::MapPageCodec::encode(3,v,error);
        pages[0]=QByteArray(256,0);pages[0][1]=2;pages[3]=QByteArray(256,char(37));active=pages;
    }
    QByteArray reply(const QByteArray &q) {
        if(q=="TUNUINO?"){auto b=identity;if(badIdentity)b[20]=43;return b;}
        if(q=="TUNUINO:V") {
            QByteArray b(16,0);b[1]=1;b[2]=dirty;b[3]=persisted;
            put32(b,4,badValidation?64:0);put32(b,8,generation);
            put32(b,12,Writer::crc32(pages[0])^(badCrc?1:0));return b;
        }
        if(q==QByteArray("TUNUINO:X",9)+QByteArray(1,0)) {
            QByteArray b(24,0);b[1]=1;b[2]=armed?1:0;put32(b,4,armed?1:0);return b;
        }
        if(q[0]=='O') {
            ++runtimes;QByteArray b(101,0);b.replace(1,31,identity.mid(1));b[40]=char(1|(dirty?2:0)|(persisted?4:0));
            put32(b,57,generation);b[83]=6;b[84]=83; // 12 V, ADC 1619.
            if(runtimes==3) {
                if(losePowerBeforeBurn){b[83]=0;b[84]=0;}
                if(changedIdentityBeforeBurn)b[20]=43;
                if(engineStartsBeforeBurn)put32(b,41,1000000);
            }
            return b;
        }
        const int page=u16(q,1)-1;CHECK(page>=0 && page<4);
        if(q[0]=='r') {
            auto b=QByteArray(1,0)+pages[size_t(page)].mid(u16(q,3),u16(q,5));
            if(page==3 && u16(q,3)==0 && ((corruptStage && writes && !burns)||(corruptPermanent && burns)))b[1]=char(quint8(b[1])^1);
            return b;
        }
        if(q[0]=='w') {
            ++writes;if(rejectWrite==writes)return QByteArray(1,char(0x84));
            CHECK(page==1 || page==2);CHECK(q.size()==7+u16(q,5));
            pages[size_t(page)].replace(u16(q,3),u16(q,5),q.mid(7));dirty=pages!=active;return QByteArray(1,0);
        }
        CHECK(q[0]=='b');CHECK(++burns==1);
        if(failBurn)return QByteArray(1,char(0x80));
        active=pages;dirty=false;persisted=!notPersisted;if(++generation==0)generation=1;return QByteArray(1,0);
    }
};
struct Run {
    FakeEcu ecu;Writer writer;QQueue<QByteArray> queue;
    bool done=false,success=false,uncertain=false,copyFails=false,dropBurn=false,staleBase=false,staleGeneration=false;
    int copies=0,completions=0;
    QString message;Writer::Pages copied;
    Run() {
        writer.send=[&](const QByteArray &q,int timeout){CHECK(timeout>=3000);queue.enqueue(q);return true;};
        writer.savePrevious=[&](const QByteArray &id,quint32 gen,const Writer::Pages &pages){CHECK(id==ecu.identity && gen==ecu.generation && ecu.writes==0);++copies;copied=pages;return !copyFails;};
        writer.completed=[&](bool ok,bool unknown,const QString &text){CHECK(++completions==1);done=true;success=ok;uncertain=unknown;message=text;};
    }
    void run(bool unchanged=false,int page=2) {
        const auto base=ecu.pages[size_t(page-1)];auto desired=base;
        if(!unchanged){desired[64]=page==2?char(0x17):char(0xfe);desired[65]=page==2?char(0x70):char(0x0c);}
        if(staleBase){ecu.pages[size_t(page-1)][67]=char(quint8(ecu.pages[size_t(page-1)][67])^1);ecu.active=ecu.pages;}
        CHECK(writer.start(page,desired,base,ecu.identity,ecu.generation-(staleGeneration?1:0)));
        CHECK(!writer.start(page,desired,base,ecu.identity,ecu.generation));
        int count=0;
        while(writer.busy()) {
            CHECK(!queue.isEmpty() && ++count<200);const auto q=queue.dequeue();
            CHECK(q[0]!='D');if(q.startsWith("TUNUINO:X"))CHECK(q.size()==10 && q[9]==0);
            const auto response=ecu.reply(q);
            if(dropBurn && q[0]=='b')writer.transportFailed("Lost Burn response");else writer.receive(response);
        }
        CHECK(done && queue.isEmpty());
        writer.receive(QByteArray(1,0));CHECK(completions==1);
        if(success && !unchanged){CHECK(ecu.pages[size_t(page-1)]==desired);CHECK(ecu.pages[0]==copied[0] && ecu.pages[3]==copied[3]);CHECK(ecu.pages[size_t(page==2?2:1)]==copied[size_t(page==2?2:1)]);}
    }
};
static void nativeFirmware(bool afr=false) {
    const QString executable=qEnvironmentVariable("TUNUINO_FIRMWARE_HARNESS");
    if(executable.isEmpty())return;
    QProcess process;process.start(executable,afr?QStringList{"--afr"}:QStringList{});CHECK(process.waitForStarted(3000));
    const auto exchange=[&](const QByteArray &q) {
        const auto line=q.toHex()+"\n";CHECK(process.write(line)==line.size());CHECK(process.waitForBytesWritten(3000));
        while(!process.canReadLine())CHECK(process.waitForReadyRead(3000));
        return QByteArray::fromHex(process.readLine().trimmed());
    };
    const auto id=exchange("TUNUINO?");const auto validation=exchange("TUNUINO:V");
    CHECK(id.size()==32 && validation.size()==16 && u32(validation,4)==0);
    Writer::Pages before;
    for(int p=1;p<=(afr?5:4);++p)for(int at=0;at<(p==1 || p==4?256:576);at+=64)before[size_t(p-1)]+=exchange(read(p,at,64)).mid(1);
    quint32 generation=u32(validation,8);int copies=0,totalBurns=0;
    for(int index=0;index<(afr?3:2);++index) {
        const int page=afr?(index==0?5:index+1):index+2;
        auto desired=before[size_t(page-1)];
        if(page==5){tunuino::MapPageValues v;for(int i=0;i<16;++i){v.rpm[i]=500+400*i;v.load[i]=20+10*i;}v.cells.fill(14.7);QString error;desired=tunuino::MapPageCodec::encode(5,v,error);CHECK(!desired.isEmpty());}
        if(page!=5){desired[64]=page==2?char(0x13):char(0xfe);desired[65]=page==2?char(0x88):char(0x0c);}
        Writer writer;QQueue<QByteArray> pending;bool success=false;int burns=0;
        writer.send=[&](const QByteArray &q,int){pending.enqueue(q);return true;};
        writer.savePrevious=[&](const QByteArray &identity,quint32 gen,const Writer::Pages &pages){CHECK(identity==id && gen==generation && pages==before);++copies;return true;};
        writer.completed=[&](bool ok,bool uncertain,const QString &message){if(!ok)std::cerr<<message.toStdString()<<std::endl;CHECK(!uncertain);success=ok;};
        CHECK(writer.start(page,desired,before[size_t(page-1)],id,generation));
        while(writer.busy()){CHECK(!pending.isEmpty());const auto q=pending.dequeue();if(q[0]=='b')++burns;writer.receive(exchange(q));}
        CHECK(success && burns==1);CHECK(writer.generation()==generation+1);
        before[size_t(page-1)]=desired;generation=writer.generation();totalBurns+=burns;
    }
    CHECK(copies==(afr?3:2) && totalBurns==(afr?3:2));
    process.closeWriteChannel();CHECK(process.waitForFinished(3000) && process.exitCode()==0);
    std::cout<<"Real firmware dispatcher, page validation, persistence callback and generation: passed (host simulation)\n";
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    CHECK(Writer::crc32("123456789")==0xcbf43926u);
    {Run r;r.run();CHECK(r.success && !r.uncertain && r.ecu.writes==9 && r.ecu.burns==1 && r.copies==1);}
    {Run r;r.run(false,3);CHECK(r.success && r.ecu.burns==1);}
    {Run r;r.run(true);CHECK(r.success && r.ecu.writes==0 && r.ecu.burns==0 && r.copies==0);}
    {Run r;r.ecu.generation=0xffffffffu;r.run();CHECK(r.success && r.writer.generation()==1);}
    {Run r;r.staleBase=true;r.run();CHECK(!r.success && r.ecu.writes==0);}
    {Run r;r.staleGeneration=true;r.run();CHECK(!r.success && r.ecu.writes==0);}
    {Run r;r.ecu.dirty=true;r.run();CHECK(!r.success && r.ecu.writes==0);}
    {Run r;r.ecu.armed=true;r.run();CHECK(!r.success && r.ecu.writes==0 && r.ecu.burns==0);}
    {Run r;r.ecu.badIdentity=true;r.run();CHECK(!r.success && r.ecu.writes==0);}
    {Run r;r.ecu.badValidation=true;r.run();CHECK(!r.success && r.ecu.writes==0);}
    {Run r;r.ecu.badCrc=true;r.run();CHECK(!r.success && r.ecu.writes==0);}
    {Run r;r.copyFails=true;r.run();CHECK(!r.success && r.ecu.writes==0);}
    {Run r;r.ecu.rejectWrite=4;r.run();CHECK(!r.success && !r.uncertain && r.ecu.burns==0);}
    {Run r;r.ecu.corruptStage=true;r.run();CHECK(!r.success && r.ecu.burns==0);}
    {Run r;r.ecu.losePowerBeforeBurn=true;r.run();CHECK(!r.success && r.ecu.burns==0);}
    {Run r;r.ecu.changedIdentityBeforeBurn=true;r.run();CHECK(!r.success && r.ecu.burns==0);}
    {Run r;r.ecu.engineStartsBeforeBurn=true;r.run();CHECK(!r.success && r.ecu.burns==0);}
    {Run r;r.ecu.failBurn=true;r.run();CHECK(!r.success && r.uncertain && r.ecu.burns==1);}
    {Run r;r.dropBurn=true;r.run();CHECK(!r.success && r.uncertain && r.ecu.burns==1);}
    {Run r;r.ecu.corruptPermanent=true;r.run();CHECK(!r.success && r.uncertain && r.ecu.burns==1);}
    {Run r;r.ecu.notPersisted=true;r.run();CHECK(!r.success && r.uncertain && r.ecu.burns==1);}
    FakeEcu ecu;Writer invalid;invalid.send=[](const QByteArray&,int){return true;};invalid.savePrevious=[](const QByteArray&,quint32,const Writer::Pages&){return true;};invalid.completed=[](bool,bool,const QString&){};
    CHECK(!invalid.start(4,ecu.pages[1],ecu.pages[1],ecu.identity,5));
    CHECK(!invalid.start(2,QByteArray(576,0),ecu.pages[1],ecu.identity,5));
    tunuino::MapPageValues badMap;
    for(int i=0;i<16;++i){badMap.rpm[i]=500+400*i;badMap.load[i]=20+10*i;}badMap.cells.fill(-5);
    QString codecError;CHECK(!tunuino::MapPageCodec::encode(3,badMap,codecError).isEmpty());
    CHECK(tunuino::MapPageCodec::encode(2,badMap,codecError).isEmpty());
    badMap.cells.fill(0);badMap.rpm[1]=badMap.rpm[0];CHECK(tunuino::MapPageCodec::encode(2,badMap,codecError).isEmpty());
    badMap.rpm[1]=900;badMap.cells[0]=1.001;CHECK(tunuino::MapPageCodec::encode(2,badMap,codecError).isEmpty());
    badMap.cells.fill(14.7);CHECK(!tunuino::MapPageCodec::encode(5,badMap,codecError).isEmpty());
    const auto afrPage=tunuino::MapPageCodec::encode(5,badMap,codecError);CHECK(tunuino::MapPageCodec::valid(5,afrPage));
    CHECK(!invalid.start(5,afrPage,QByteArray(576,0),ecu.identity,5));
    badMap.cells[0]=9;CHECK(tunuino::MapPageCodec::encode(5,badMap,codecError).isEmpty());
    badMap.cells[0]=20;CHECK(tunuino::MapPageCodec::encode(5,badMap,codecError).isEmpty());
    nativeFirmware();nativeFirmware(true);
    std::cout<<"Fuel/Ignition write, clean base, unchanged pages, readback, save failure and no Burn retry: passed\n";
}
