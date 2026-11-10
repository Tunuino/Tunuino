#pragma once
#include "SensorCalibrationCodec.h"
#include "VvtCodec.h"
namespace tunuino {
class VtecCodec {
public:
    static bool supported(const QByteArray &id){return id.size()==32&&((quint8(id[18])&192)==192);}
    static QByteArray defaults(){QByteArray p(256,0);p[0]=1;const int offsets[]={4,6,8,10,12,14,16,18,20,22,24,26};const int values[]={5000,4800,8000,4000,3500,6000,5500,7000,11000,10000,2000,1500};for(int i=0;i<12;++i)SensorCalibrationCodec::put(p,offsets[i],values[i]);return p;}
    static bool format(const QByteArray &p){
        if(p.size()!=256)return false;if(p==QByteArray(256,0))return true;
        if((p[0]!=1&&p[0]!=2)||quint8(p[1])>1||quint8(p[2])>16||quint8(p[3])>1||p.mid(28,p[0]==2?36:228)!=QByteArray(p[0]==2?36:228,0)||!VvtCodec::extension(p))return false;
        const auto u=[&](int at){return SensorCalibrationCodec::u16(p,at);};
        return u(6)>=500&&u(6)<u(4)&&u(4)<u(8)&&u(8)<=20000&&u(12)<=u(10)&&u(10)<=10000
            &&u(16)>=1000&&u(16)<=u(14)&&u(14)<=50000&&u(18)<u(20)&&u(20)<=15000
            &&u(22)<=60000&&u(26)<=u(24)&&u(24)<=20000&&(!p[3]||u(26)>0)&&(!p[1]||p[2]);
    }
    static bool allowed(const QByteArray &p,const QByteArray &h,const QByteArray &a,QString &error){
        if(!format(p)){error="Invalid VTEC limits or hysteresis. Nothing was written.";return false;}
        if(p[1]==0)return true;
        if(h.size()!=256||a.size()!=256||quint8(h[128])<1||quint8(h[128])>3||quint8(h[129])<1||quint8(h[129])>12||!h[2]||!h[6]||!h[8]||a[0]!=1||!a[5]){error="Configure engine outputs, trigger, MAP, coolant and TPS before enabling VTEC.";return false;}
        if(SensorCalibrationCodec::u16(p,8)>SensorCalibrationCodec::u16(h,96)){error="VTEC maximum RPM exceeds the engine acquisition limit.";return false;}
        const int channel=quint8(p[2]);bool conflict=false;
        for(int i=0;i<quint8(h[129]);++i)conflict|=quint8(h[160+8*i])+1==channel;
        for(int i=150;i<=152;++i)conflict|=quint8(h[i])==channel;
        conflict|=(a[1]!=0&&quint8(a[2])==channel)||(a[3]!=0&&quint8(a[4])==channel);
        conflict|=p[0]==2&&p[65]!=0&&quint8(p[66])==channel;
        if(conflict){error="The selected VTEC output is occupied. Choose a free output explicitly.";return false;}
        if(p[3]&&(h[38]!=1||!h[48])){error="Calibrate the existing oil pressure input before requiring oil pressure for VTEC.";return false;}
        return true;
    }
};
}
