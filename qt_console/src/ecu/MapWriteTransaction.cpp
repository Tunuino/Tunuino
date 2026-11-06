#include "MapWriteTransaction.h"
#include "EcuPowerState.h"
#include "../protocol/MapPageCodec.h"
#include <utility>

namespace tunuino {
namespace {
constexpr int Chunk=64;
int pageSize(int page){return page==1 || page==4?256:576;}
QByteArray outputQuery(){return QByteArray("TUNUINO:X",9)+QByteArray(1,0);}
QByteArray runtimeQuery(){QByteArray q(5,0);q[0]='O';q[4]=100;return q;}
quint32 nextGeneration(quint32 generation){return generation==0xffffffffu?1:generation+1;}
}
quint32 MapWriteTransaction::u32(const QByteArray &b,int at) {
    return (quint32(quint8(b[at]))<<24)|(quint32(quint8(b[at+1]))<<16)
        |(quint32(quint8(b[at+2]))<<8)|quint32(quint8(b[at+3]));
}
void MapWriteTransaction::put16(QByteArray &b,int at,quint16 v){b[at]=char(v>>8);b[at+1]=char(v);}
QByteArray MapWriteTransaction::pageCommand(char command,int page){QByteArray q(3,0);q[0]=command;put16(q,1,quint16(page));return q;}
QByteArray MapWriteTransaction::readCommand(int page,int offset,int count) {
    QByteArray q(7,0);q[0]='r';put16(q,1,quint16(page));put16(q,3,quint16(offset));put16(q,5,quint16(count));return q;
}
quint32 MapWriteTransaction::crc32(const QByteArray &bytes) {
    quint32 crc=0xffffffffu;
    for(char byte:bytes){crc^=quint8(byte);for(int i=0;i<8;++i)crc=(crc>>1)^((crc&1u)?0xedb88320u:0u);}
    return ~crc;
}
bool MapWriteTransaction::start(int page,const QByteArray &desired,const QByteArray &baseMap,
    const QByteArray &identity,quint32 generation) {
    if(busy() || !MapPageCodec::valid(page,desired) || !MapPageCodec::valid(page,baseMap))return false;
    sensor_=-1;return begin(page,desired,baseMap,identity,generation);
}
bool MapWriteTransaction::startSensor(SensorKind kind,const QByteArray &desired,const QByteArray &base,
    const QByteArray &identity,quint32 generation) {
    if(busy() || !SensorCalibrationCodec::validKind(kind) || desired.size()!=256 || base.size()!=256)return false;
    sensor_=int(kind);return begin(SensorCalibrationCodec::page(kind),desired,base,identity,generation);
}
bool MapWriteTransaction::begin(int page,const QByteArray &desired,const QByteArray &baseMap,
    const QByteArray &identity,quint32 generation) {
    if(busy() || !send || !savePrevious || !completed || generation==0
        || !EcuPowerState::supportedIdentity(identity) || !(quint8(identity[19])&8))return false;
    page_=page;desired_=desired;baseMap_=baseMap;identity_=identity;generation_=resultGeneration_=generation;
    burnRequested_=writeRequested_=identityMatched_=false;base_={};expected_={};read_={};
    issue(Phase::Identity,QByteArray("TUNUINO?"));return true;
}
void MapWriteTransaction::issue(Phase phase,const QByteArray &request,int timeout) {
    phase_=phase;
    if(!send(request,timeout) && busy())fail(QStringLiteral("Unable to send ECU request."));
}
void MapWriteTransaction::finish(bool success,const QString &message) {
    const bool uncertain=!success && burnRequested_;phase_=Phase::Idle;
    completed(success,uncertain,message);
}
void MapWriteTransaction::fail(const QString &message) {
    finish(false,burnRequested_?message+QStringLiteral(" Permanent save was requested. Re-read the ECU before any new write; do not repeat the save automatically.")
        :message+(writeRequested_?QStringLiteral(" No permanent save was requested. Temporary changes may remain; reset the stopped ECU and re-read before retrying.")
            :QStringLiteral(" No map write was requested.")));
}
void MapWriteTransaction::transportFailed(const QString &reason){if(busy())fail(reason);}
bool MapWriteTransaction::validation(const QByteArray &b,bool dirty,bool persisted,quint32 generation,bool checkCrc) const {
    return b.size()==16 && quint8(b[0])==0 && quint8(b[1])==1 && quint8(b[2])==unsigned(dirty) && quint8(b[3])<=1
        && (!persisted || quint8(b[3])==1) && u32(b,4)==0 && u32(b,8)==generation
        && (!checkCrc || u32(b,12)==crc32(expected_[0].isEmpty()?base_[0]:expected_[0]));
}
bool MapWriteTransaction::outputsIdle(const QByteArray &b) {
    return b.size()==24 && quint8(b[0])==0 && quint8(b[1])==1
        && (quint8(b[2])==0 || quint8(b[2])==3) && u32(b,4)==0 && u32(b,8)==0;
}
bool MapWriteTransaction::runtime(const QByteArray &b,quint32 generation,bool dirty) const {
    if(b.size()!=101 || quint8(b[0])!=0 || b.mid(1,31)!=identity_.mid(1,31)
        || !(quint8(b[40])&1) || bool(quint8(b[40])&2)!=dirty || ((quint8(b[32])&15)!=0 && (quint8(b[32])&15)!=3)
        || u32(b,41)!=0 || u32(b,97)!=0 || u32(b,57)!=generation)return false;
    const unsigned raw=(unsigned(quint8(b[83]))<<8)|unsigned(quint8(b[84]));
    return raw<4095 && raw*(3.3*9.2/4095.0)>=6.0;
}
void MapWriteTransaction::beginRead(Phase phase){phase_=phase;read_={};readPage_=1;offset_=0;requestRead();}
void MapWriteTransaction::requestRead() {
    issue(phase_,readCommand(readPage_,offset_,qMin(Chunk,pageSize(readPage_)-offset_)));
}
void MapWriteTransaction::requestWrite() {
    const int count=qMin(Chunk,int(desired_.size())-offset_);
    QByteArray q=readCommand(page_,offset_,count);q[0]='w';q+=desired_.mid(offset_,count);writeRequested_=true;issue(Phase::Write,q);
}
void MapWriteTransaction::receive(const QByteArray &b) {
    if(!busy())return;
    if(b.isEmpty() || quint8(b[0])!=0){fail(QStringLiteral("ECU rejected the operation."));return;}
    const auto reject=[this]{fail(QStringLiteral("ECU state, identity or readback changed; write stopped."));};
    switch(phase_) {
    case Phase::Identity:
        if(b!=identity_){reject();return;}
        identityMatched_=true;
        issue(Phase::ValidateBase,QByteArray("TUNUINO:V"));return;
    case Phase::ValidateBase:
        if(b.size()==16 && quint8(b[1])==1 && quint8(b[2])!=0) {
            fail(QStringLiteral("ECU has uncommitted temporary changes. Reset the stopped ECU and re-read before writing."));return;
        }
        if(!validation(b,false,false,generation_,false)){reject();return;}
        issue(Phase::OutputsBase,outputQuery());return;
    case Phase::OutputsBase:
        if(!outputsIdle(b)){fail(QStringLiteral("Stop the engine and disarm outputs before writing a map."));return;}
        issue(Phase::RuntimeBase,runtimeQuery());return;
    case Phase::RuntimeBase:
        if(!runtime(b,generation_)){reject();return;}beginRead(Phase::ReadBase);return;
    case Phase::ReadBase:case Phase::ReadStage:case Phase::ReadPersisted: {
        const Phase reading=phase_;const int count=qMin(Chunk,pageSize(readPage_)-offset_);
        if(b.size()!=count+1){reject();return;}
        read_[size_t(readPage_-1)]+=b.mid(1);offset_+=count;
        if(offset_==pageSize(readPage_)){++readPage_;offset_=0;}
        if(readPage_<=4){requestRead();return;}
        if(reading==Phase::ReadBase) {
            base_=read_;
            if(base_[size_t(page_-1)]!=baseMap_ || quint8(base_[0][0])!=0 || quint8(base_[0][1])!=2
                || !MapPageCodec::valid(2,base_[1]) || !MapPageCodec::valid(3,base_[2])){reject();return;}
            if(sensor_>=0){QString error;
                if(!SensorCalibrationCodec::permitted(SensorKind(sensor_),baseMap_,desired_,base_[0],base_[3],error)){fail(error);return;}}
            issue(Phase::ConfirmBase,QByteArray("TUNUINO:V"));
        } else {
            if(read_!=expected_){fail(QStringLiteral("ECU readback differs from the requested map or an untouched configuration page."));return;}
            issue(reading==Phase::ReadStage?Phase::ValidateStage:Phase::ValidatePersisted,QByteArray("TUNUINO:V"));
        }
        return;
    }
    case Phase::ConfirmBase:
        if(!validation(b,false,false,generation_,true)){reject();return;}
        if(desired_==baseMap_){finish(true,QStringLiteral("No changes to write."));return;}
        if(!savePrevious(identity_,generation_,base_)){fail(QStringLiteral("Could not preserve the current ECU configuration. No write started."));return;}
        expected_=base_;expected_[size_t(page_-1)]=desired_;
        issue(Phase::OutputsStage,outputQuery());return;
    case Phase::OutputsStage:
        if(!outputsIdle(b)){reject();return;}issue(Phase::RuntimeStage,runtimeQuery());return;
    case Phase::RuntimeStage:
        if(!runtime(b,generation_)){reject();return;}offset_=0;requestWrite();return;
    case Phase::Write:
        if(b.size()!=1){reject();return;}
        offset_+=qMin(Chunk,int(desired_.size())-offset_);
        if(offset_<desired_.size())requestWrite();else beginRead(Phase::ReadStage);return;
    case Phase::ValidateStage:
        if(!validation(b,true,false,generation_,true)){reject();return;}
        issue(Phase::OutputsBurn,outputQuery());return;
    case Phase::OutputsBurn:
        if(!outputsIdle(b)){reject();return;}issue(Phase::RuntimeBurn,runtimeQuery());return;
    case Phase::RuntimeBurn:
        if(!runtime(b,generation_,true)){reject();return;}
        burnRequested_=true;issue(Phase::Burn,pageCommand('b',page_),15000);return;
    case Phase::Burn:
        if(b.size()!=1){reject();return;}beginRead(Phase::ReadPersisted);return;
    case Phase::ValidatePersisted:
        if(!validation(b,false,true,nextGeneration(generation_),true)){reject();return;}
        resultGeneration_=nextGeneration(generation_);
        issue(Phase::RuntimeFinal,runtimeQuery());return;
    case Phase::RuntimeFinal:
        if(!runtime(b,nextGeneration(generation_))){reject();return;}
        issue(Phase::OutputsFinal,outputQuery());return;
    case Phase::OutputsFinal:
        if(!outputsIdle(b)){reject();return;}
        finish(true,QStringLiteral("Calibration saved to ECU and all configuration pages verified. Outputs remain disarmed."));return;
    case Phase::Idle:return;
    }
}
}
