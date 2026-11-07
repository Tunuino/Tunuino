#include "EngineProfileDraft.h"
#include "../protocol/SensorCalibrationCodec.h"
#include <QJsonArray>
#include <algorithm>
#include <cmath>
namespace tunuino {
namespace {
int u16(const QByteArray &p,int at){return SensorCalibrationCodec::u16(p,at);}
quint32 u32(const QByteArray &p,int at){return (quint32(u16(p,at))<<16)|quint32(u16(p,at+2));}
void put(QByteArray &p,int at,quint32 value,int width=2){for(int i=width-1;i>=0;--i){p[at+i]=char(value);value>>=8;}}
}
void EngineProfileDraft::load(const QByteArray &header,const QByteArray &aux){
    header_=header;aux_=aux;retained_.clear();cylinders.clear();order.clear();profile_=QByteArray(128,0);
    operation=0;inverted=false;requiredFuel=5000;dwell=2000;deadTime=1000;maximumInjection=15000;toothZero=0;camFirst=camLast=0;
    // Inactive reference constants; no output assignments are proposed.
    put(profile_,12,9000);put(profile_,14,8500);
    if(header.size()==256&&quint8(header[128])>=1&&quint8(header[128])<=3&&quint8(header[129])>=1&&quint8(header[129])<=12){
        profile_=header.mid(128);requiredFuel=u16(profile_,2);dwell=u16(profile_,4);deadTime=u16(profile_,6);maximumInjection=u16(profile_,8);toothZero=u32(profile_,18);
        camFirst=quint8(profile_[25]);camLast=quint8(profile_[26]);inverted=profile_[27]!=0;operation=quint8(profile_[28]);
        for(int i=0;i<quint8(profile_[1]);++i)cylinders.append({quint8(profile_[32+8*i]),quint8(profile_[33+8*i]),u32(profile_,34+8*i)});
        sortOrder();retained_=cylinders;
    }else resize(4);
}
void EngineProfileDraft::sortOrder(){order.clear();for(int i=0;i<count();++i)order<<i;std::sort(order.begin(),order.end(),[this](int a,int b){return cylinders[a].tdc<cylinders[b].tdc;});}
bool EngineProfileDraft::resize(int n){
    if(n<1||n>12)return false;if(n==count())return true;
    QVector<int> next;for(int cylinder:order)if(cylinder>=0&&cylinder<n&&!next.contains(cylinder))next<<cylinder;
    for(int i=0;i<n;++i)if(!next.contains(i))next<<i;
    retained_.resize(qMax(qsizetype(12),retained_.size()));for(int i=0;i<count();++i)retained_[i]=cylinders[i];const int previous=count();cylinders.resize(n);for(int i=previous;i<n;++i)cylinders[i]=retained_[i];order=next;
    // A cylinder-count change explicitly proposes even firing; the angles remain editable.
    for(int i=0;i<n;++i)cylinders[order[i]].tdc=quint32(std::lround(72000.0*i/n));
    return true;
}
bool EngineProfileDraft::permutation()const{if(order.size()!=count())return false;quint32 used=0;for(int c:order){if(c<0||c>=count()||(used&(1u<<c)))return false;used|=1u<<c;}return true;}
void EngineProfileDraft::setOrder(int event,int cylinder){
    if(event<0||event>=order.size())return;QVector<quint32> angles;for(const auto &c:cylinders)angles<<c.tdc;std::sort(angles.begin(),angles.end());
    order[event]=cylinder;if(permutation())for(int i=0;i<count();++i)cylinders[order[i]].tdc=angles[i];
}
void EngineProfileDraft::setTdc(int cylinder,quint32 angle){if(cylinder<0||cylinder>=count()||angle>=72000)return;cylinders[cylinder].tdc=angle;sortOrder();}
int EngineProfileDraft::coilPartner(int cylinder)const{
    if(operation<3||cylinder<0||cylinder>=count()||cylinders[cylinder].coil<0)return -1;
    for(int i=0;i<count();++i)if(i!=cylinder&&cylinders[i].coil==cylinders[cylinder].coil&&std::abs(qint64(cylinders[i].tdc)-qint64(cylinders[cylinder].tdc))==36000)return i;
    return -1;
}
bool EngineProfileDraft::assign(int cylinder,bool ignition,int channel,QString &error){
    error.clear();if(cylinder<0||cylinder>=count()||channel< -1||channel>=(ignition?12:16)){error="Invalid cylinder or output.";return false;}
    const int partner=ignition?coilPartner(cylinder):-1;
    if(channel>=0){
        if(!ignition){for(int at:{22,23,24})if(quint8(profile_[at])==channel+1){error="Output is assigned to the pump, fan or main relay.";return false;}
            if(aux_.size()==256&&aux_[0]==1&&((aux_[1]!=0&&quint8(aux_[2])==channel+1)||(aux_[3]!=0&&quint8(aux_[4])==channel+1))){error="Output is assigned to idle or boost control.";return false;}}
        int sharers=0;for(int i=0;i<count();++i)if(i!=cylinder&&i!=partner&&(ignition?cylinders[i].coil:cylinders[i].injector)==channel){
            if(!ignition||operation<3||partner>=0||std::abs(qint64(cylinders[i].tdc)-qint64(cylinders[cylinder].tdc))!=36000||++sharers>1){error="Output is already assigned to another cylinder.";return false;}}
    }
    if(ignition){cylinders[cylinder].coil=channel;if(partner>=0)cylinders[partner].coil=channel;}else cylinders[cylinder].injector=channel;
    return true;
}
bool EngineProfileDraft::validate(QString &error)const{
    const auto fail=[&](const char *message){error=message;return false;};error.clear();
    if(header_.size()!=256||aux_.size()!=256||u16(header_,0)!=2)return fail("Read a supported ECU configuration before writing.");
    if(count()<1||count()>12||operation<0||operation>4||!permutation())return fail("Each cylinder must occur exactly once in the firing order.");
    if(operation>=3&&count()%2)return fail("Shared coils require an even cylinder count.");
    if(requiredFuel<100||requiredFuel>30000||dwell<100||dwell>5000||deadTime<0||deadTime>5000||maximumInjection<100||maximumInjection>20000||deadTime>=maximumInjection)return fail("Fuel, dwell or injector-time limits are invalid.");
    const int wheelSlots=quint8(header_[3]),missing=quint8(header_[4]);
    if(wheelSlots<4||wheelSlots>120||missing<1||missing>=wheelSlots/2||camFirst<0||camLast<camFirst||camLast>=wheelSlots-missing||toothZero>=72000)return fail("Trigger geometry is invalid. Check teeth, missing teeth and cam window.");
    for(int i=1;i<order.size();++i)if(cylinders[order[i-1]].tdc>=cylinders[order[i]].tdc)return fail("Firing order and cylinder TDC angles disagree.");
    quint32 used=0;
    for(int at:{22,23,24}){const int port=quint8(profile_[at]);if(port){if(port>16||(used&(1u<<(port-1))))return fail("Auxiliary output assignments conflict.");used|=1u<<(port-1);}}
    if(aux_[0]==1){if((aux_[1]!=0&&quint8(aux_[2])>16)||(aux_[3]!=0&&quint8(aux_[4])>16))return fail("Invalid auxiliary output assignment.");if(aux_[1]!=0&&quint8(aux_[2])>0)used|=1u<<(quint8(aux_[2])-1);if(aux_[3]!=0&&quint8(aux_[4])>0)used|=1u<<(quint8(aux_[4])-1);}
    for(int i=0;i<count();++i){const auto &c=cylinders[i];if(c.injector<0||c.injector>=16||c.coil<0||c.coil>=12)return fail("Assign an injector and ignition output to every configured cylinder.");
        if(used&(1u<<c.injector))return fail("Injector output conflicts with another cylinder or auxiliary function.");used|=1u<<c.injector;
        if(c.tdc>=72000)return fail("Cylinder TDC angle must be below 720 degrees.");
        int partners=0;
        for(int j=0;j<count();++j)if(i!=j){if(cylinders[j].tdc==c.tdc)return fail("Cylinder TDC angles must be distinct.");if(cylinders[j].coil==c.coil){
            if(operation<3||std::abs(qint64(c.tdc)-qint64(cylinders[j].tdc))!=36000)return fail("Shared coils require exactly two cylinders 360 degrees apart.");
            ++partners;
            if(aux_[68]==1&&(u16(aux_,70)&1024)&&u16(aux_,216+2*i)!=u16(aux_,216+2*j))return fail("Cylinders sharing a coil must have identical ignition trim.");}}
        if((operation>=3&&partners!=1)||(operation<3&&partners))return fail("Ignition output assignments do not match the selected mode.");
    }
    return true;
}
QByteArray EngineProfileDraft::build(QString &error)const{
    if(!validate(error))return {};auto header=header_;auto p=profile_;p[0]=operation==0?1:operation<3?2:3;p[1]=char(count());
    put(p,2,requiredFuel);put(p,4,dwell);put(p,6,deadTime);put(p,8,maximumInjection);put(p,18,toothZero,4);p[25]=char(camFirst);p[26]=char(camLast);p[27]=inverted?1:0;p[28]=char(operation);
    for(int i=0;i<12;++i){for(int j=0;j<8;++j)p[32+8*i+j]=0;if(i<count()){p[32+8*i]=char(cylinders[i].injector);p[33+8*i]=char(cylinders[i].coil);put(p,34+8*i,cylinders[i].tdc,4);}}
    header.replace(128,128,p);return header;
}
QJsonObject EngineProfileDraft::json()const{
    QJsonArray rows,events;for(const auto &c:cylinders)rows.append(QJsonObject{{"injector",c.injector},{"coil",c.coil},{"tdc",int(c.tdc)}});for(int c:order)events.append(c);
    return {{"operation",operation},{"inverted",inverted},{"requiredFuel",requiredFuel},{"dwell",dwell},{"deadTime",deadTime},{"maximumInjection",maximumInjection},{"toothZero",int(toothZero)},{"camFirst",camFirst},{"camLast",camLast},{"cylinders",rows},{"order",events}};
}
bool EngineProfileDraft::import(const QJsonObject &o,QString &error,bool editorDraft){
    auto copy=*this;const auto integer=[&](const QJsonObject &object,const char *key,int low,int high,int &out){const auto v=object[key];const double n=v.toDouble(-1e20);if(!v.isDouble()||n<low||n>high||n!=std::floor(n))return false;out=int(n);return true;};
    int angle=0;if(!integer(o,"operation",0,4,copy.operation)||!o["inverted"].isBool()||!integer(o,"requiredFuel",100,30000,copy.requiredFuel)||!integer(o,"dwell",100,5000,copy.dwell)||!integer(o,"deadTime",0,5000,copy.deadTime)||!integer(o,"maximumInjection",100,20000,copy.maximumInjection)||!integer(o,"toothZero",0,71999,angle)||!integer(o,"camFirst",0,119,copy.camFirst)||!integer(o,"camLast",0,119,copy.camLast)){error="Invalid engine-profile parameters.";return false;}
    copy.inverted=o["inverted"].toBool();copy.toothZero=quint32(angle);const auto rows=o["cylinders"].toArray(),events=o["order"].toArray();if(rows.size()<1||rows.size()>12||events.size()!=rows.size()){error="Invalid cylinder count or order.";return false;}
    copy.cylinders.clear();copy.order.clear();for(const auto &value:rows){const auto row=value.toObject();Cylinder c;int tdc=0;if(!integer(row,"injector",-1,15,c.injector)||!integer(row,"coil",-1,11,c.coil)||!integer(row,"tdc",0,71999,tdc)){error="Invalid cylinder assignment.";return false;}c.tdc=quint32(tdc);copy.cylinders<<c;}
    for(const auto &value:events){const double n=value.toDouble(-1e20);if(!value.isDouble()||n<(editorDraft?-1:0)||n>=rows.size()||n!=std::floor(n)){error="Invalid firing order.";return false;}copy.order<<int(n);}
    if(!editorDraft&&!copy.permutation()){error="Each cylinder must occur once.";return false;}for(int i=1;!editorDraft&&i<copy.order.size();++i)if(copy.cylinders[copy.order[i-1]].tdc>=copy.cylinders[copy.order[i]].tdc){error="Firing order and TDC angles disagree.";return false;}copy.retained_=copy.cylinders;*this=copy;return true;
}
}
