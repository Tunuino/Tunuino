#pragma once
#include "SensorCalibrationCodec.h"
#include "MapPageCodec.h"
namespace tunuino {
class VvtCodec {
public:
    static bool supported(const QByteArray &id){return id.size()==32&&(quint8(id[17])&1)&&((quint8(id[18])&192)==192);}
    static bool tableSupported(const QByteArray &id){return supported(id)&&(quint8(id[17])&2);}
    static bool advancedSupported(const QByteArray &id){return tableSupported(id)&&(quint8(id[17])&4);}
    static quint32 u32(const QByteArray &p,int at){return quint32(SensorCalibrationCodec::u16(p,at))<<16|quint32(SensorCalibrationCodec::u16(p,at+2));}
    static bool extension(const QByteArray &p){
        if(p.size()!=256||quint8(p[0])>2)return false;
        if(p[0]!=2||!p[64])return p.mid(64)==QByteArray(192,0);
        if((p[64]<1||p[64]>3)||(p[64]==1&&p[123])||quint8(p[123])>(p[64]==3?2:1)||quint8(p[65])>1||quint8(p[66])>16||quint8(p[67])>1||quint8(p[110])>1||p[111]||p[92]||p[93]||(p[64]==3?p.mid(142)!=QByteArray(114,0):p.mid(124)!=QByteArray(132,0)))return false;
        if(p[116]){const int n=quint8(p[116]),m=quint8(p[117]);if(n<4||n>120||m<1||m>=n/2||quint8(p[118])>quint8(p[119])||quint8(p[119])>=n-m||quint8(p[120])>1||quint8(p[121])>3||quint8(p[122])>3)return false;}else if(p.mid(117,6)!=QByteArray(6,0))return false;
        const auto u=[&](int at){return SensorCalibrationCodec::u16(p,at);};const auto s=[&](int at){return int(qint16(u(at)));};
        if(p[64]==3){if(quint8(p[124])>1||quint8(p[125])>1||quint8(p[128])>1||p[129]||u(126)>9500||s(132)<-10000||s(132)>0||s(134)<0||s(134)>10000||s(136)<-10000||s(136)>0||s(138)<0||s(138)>10000||u(140)>2000
            ||(p[125]&&(u(126)<u(70)||u(126)>u(72)))||(p[128]&&(s(130)<s(86)||s(130)>s(88))))return false;}
        return u(68)>=10&&u(68)<=300&&u(70)<u(72)&&u(72)<=9500&&u(74)>=u(70)&&u(74)<=u(72)
            &&u(76)<=20000&&u(78)<=20000&&u(80)<=5000&&(u(76)||u(78))&&u32(p,82)<72000
            &&s(86)>=-9000&&s(88)<=9000&&s(86)<s(88)&&s(88)-s(86)<=9000&&s(90)>=s(86)&&s(90)<=s(88)
            &&u(94)>=500&&u(94)<=12000&&u(96)<=u(94)-100&&s(98)>=0&&s(98)<=12000&&u(100)<=2000
            &&u(102)<=60000&&u(104)>=20&&u(104)<=1000&&u(106)>=100&&u(106)<=5000&&u(108)>=1&&u(108)<=2000
            &&u(114)<=u(112)&&u(112)<=20000&&(!p[110]||u(114)>0)&&(!p[65]||p[66]);
    }
    // Caller initializes an absent VTEC prefix before adding the VVT extension.
    static QByteArray defaults(QByteArray p){
        if(p.size()!=256||(p[0]!=1&&p[0]!=2))return {};
        p[0]=2;if(p[64])return p;p[64]=1;
        const int offsets[]={68,70,72,74,76,78,80,86,88,90,94,96,98,100,102,104,106,108,112,114};
        const int values[]={100,2000,8000,5000,1000,0,0,0,3000,0,1500,100,7000,500,0,250,1000,500,2000,1500};
        for(int i=0;i<20;++i)SensorCalibrationCodec::put(p,offsets[i],values[i]);return p;
    }
    static QByteArray advanced(QByteArray p,bool fresh=false){
        p=defaults(p);if(p.isEmpty()||p[64]==3)return p;p[64]=3;
        SensorCalibrationCodec::put(p,132,fresh?-500:-10000);SensorCalibrationCodec::put(p,134,fresh?500:10000);
        SensorCalibrationCodec::put(p,136,fresh?-1500:-10000);SensorCalibrationCodec::put(p,138,fresh?1500:10000);SensorCalibrationCodec::put(p,140,fresh?50:0);return p;
    }
    static void bind(QByteArray &p,const QByteArray &h){
        if(p.size()!=256||h.size()!=256)return;
        const int offsets[]={3,4,153,154,155};for(int i=0;i<5;++i)p[116+i]=h[offsets[i]];p[121]=h[5]==1?h[9]:char(0);p[122]=h[98]==1?h[99]:char(0);
    }
    static bool tableAllowed(const QByteArray &control,const QByteArray &h,const QByteArray &table,QString &error){
        if(!extension(control)){error="Invalid VVT control page.";return false;}
        if(control[0]!=2||!control[123])return true;
        if(h.size()!=256||!h[6]||!MapPageCodec::valid(7,table)||table==QByteArray(576,0)){error="Write a native VVT target table and calibrate MAP before selecting RPM/MAP mode.";return false;}
        const int low=qint16(SensorCalibrationCodec::u16(control,86)),high=qint16(SensorCalibrationCodec::u16(control,88));
        for(int i=0;i<256;++i){const int v=qint16(SensorCalibrationCodec::u16(table,64+2*i));if(v<low||v>high){error="Every VVT table target must be inside the configured CAM 1 travel.";return false;}}return true;
    }
    static bool allowed(const QByteArray &p,const QByteArray &h,const QByteArray &a,QString &error){
        if(!extension(p)){error="Invalid VVT limits, target or controller values.";return false;}if(p[0]!=2||!p[65])return true;
        if(h.size()!=256||a.size()!=256||quint8(h[128])<1||quint8(h[128])>3||quint8(h[129])<1||quint8(h[129])>12||!h[2]||!h[8]){error="Configure an engine profile, trigger and coolant calibration before enabling VVT.";return false;}
        auto expected=p;bind(expected,h);if(p.mid(116,7)!=expected.mid(116,7)){error="CAM 1 or wheel configuration changed. Measure/review and write CAM1 parameters before enabling VVT.";return false;}
        const auto u=[&](int at){return SensorCalibrationCodec::u16(p,at);};const int wheelSlots=quint8(h[3]),missing=quint8(h[4]),first=quint8(h[153]),last=quint8(h[154]),phase=quint8(h[155]);
        if(wheelSlots<=missing||first>last||last>=wheelSlots-missing||phase>1){error="Invalid CAM 1 synchronization window.";return false;}
        const qint64 low=qint64(phase)*36000+qint64(first)*36000/wheelSlots;
        const qint64 high=qint64(phase)*36000+qint64(last+1+(last==wheelSlots-missing-1?missing:0))*36000/wheelSlots-1;
        if(qint64(u32(p,82))-qint16(u(88))<low||qint64(u32(p,82))-qint16(u(86))>high){error="The full VVT travel must remain inside the existing CAM 1 tooth window and the same revolution. Measure the base edge and configure the actual synchronization window first.";return false;}
        if(u(94)>SensorCalibrationCodec::u16(h,96)){error="VVT activation RPM exceeds the engine acquisition limit.";return false;}
        const int channel=quint8(p[66]);bool conflict=false;
        for(int i=0;i<quint8(h[129]);++i)conflict|=quint8(h[160+8*i])+1==channel;
        for(int i=150;i<=152;++i)conflict|=quint8(h[i])==channel;
        conflict|=(a[1]!=0&&quint8(a[2])==channel)||(a[3]!=0&&quint8(a[4])==channel)||(p[1]!=0&&quint8(p[2])==channel);
        if(conflict){error="VVT output is occupied by another function. Choose a free output explicitly.";return false;}
        if(p[110]&&(h[38]!=1||!h[48])){error="Calibrate oil pressure before requiring it for VVT.";return false;}return true;
    }
};
}
