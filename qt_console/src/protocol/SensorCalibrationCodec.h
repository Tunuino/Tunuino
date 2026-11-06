#pragma once
#include <QByteArray>
#include <QString>
#include <array>
#include <cmath>
namespace tunuino {
enum class SensorKind { Map,Clt,Lambda,Tps };
struct SensorCalibration {
    int channel=0;
    std::array<int,3> raw{};
    std::array<double,3> values{};
};
class SensorCalibrationCodec {
public:
    static int page(SensorKind kind){return kind==SensorKind::Tps?4:1;}
    static QString name(SensorKind kind){switch(kind){case SensorKind::Map:return "MAP";case SensorKind::Clt:return "CLT";case SensorKind::Lambda:return "Lambda";case SensorKind::Tps:return "TPS";}return {};}
    static int u16(const QByteArray &p,int at){return (int(quint8(p[at]))<<8)|quint8(p[at+1]);}
    static void put(QByteArray &p,int at,int v){p[at]=char(unsigned(v)>>8);p[at+1]=char(v);}
    static SensorCalibration decode(SensorKind kind,const QByteArray &header,const QByteArray &aux) {
        SensorCalibration c;if(header.size()!=256 || aux.size()!=256)return c;
        if(kind==SensorKind::Tps){if(aux[0]==0)return c;c.channel=quint8(aux[5]);c.raw[0]=u16(aux,6);c.raw[1]=u16(aux,8);return c;}
        c.channel=quint8(header[kind==SensorKind::Map?6:kind==SensorKind::Lambda?7:8]);
        if(kind==SensorKind::Clt){for(int i=0;i<3;++i){c.raw[i]=u16(header,32+2*i);c.values[i]=qint16(u16(header,64+2*i))/100.0;}}
        else {const int at=kind==SensorKind::Map?10:18;for(int i=0;i<2;++i){c.raw[i]=u16(header,at+2*i);c.values[i]=u16(header,at+4+2*i)/(kind==SensorKind::Map?100.0:1000.0);}}
        return c;
    }
    static bool validate(SensorKind kind,const SensorCalibration &c,QString &error) {
        error.clear();const int points=kind==SensorKind::Clt?3:2;
        if(c.channel<0 || c.channel>(kind==SensorKind::Clt?4:11)){error="Invalid sensor input.";return false;}
        for(int i=0;i<points;++i)if(c.raw[i]<0 || c.raw[i]>4095){error="Calibration voltage exceeds ADC range.";return false;}
        if(kind==SensorKind::Tps){if(c.channel && std::abs(c.raw[1]-c.raw[0])<100){error="TPS closed/open points must differ by at least 100 ADC counts.";return false;}return true;}
        for(int i=1;i<points;++i)if(c.raw[i]<=c.raw[i-1]){error="Calibration voltage points must increase strictly.";return false;}
        for(int i=0;i<points;++i){const double v=c.values[i];const double scale=kind==SensorKind::Lambda?1000:100;
            const double low=kind==SensorKind::Clt?-40:0,high=kind==SensorKind::Map?500:kind==SensorKind::Clt?150:5;
            if(!std::isfinite(v)||v<low||v>high||std::abs(v*scale-std::round(v*scale))>1e-6){error="Invalid calibration value or precision.";return false;}}
        if(c.channel && kind==SensorKind::Clt && c.values[0]==c.values[1] && c.values[1]==c.values[2]){error="Temperature calibration must not be flat.";return false;}
        if(c.channel && kind!=SensorKind::Clt && (c.values[0]>=c.values[1] || (kind==SensorKind::Lambda && c.values[0]<=0))){error="The second calibration value must exceed the first.";return false;}
        return true;
    }
    static QByteArray tpsDefaults(){
        QByteArray a(256,0);a[0]=1;
        put(a,8,4095);put(a,10,100);put(a,12,30);put(a,16,10000);put(a,20,500);put(a,22,2000);
        const int t[]={-2000,4000,10000},r[]={1000,900,850};
        for(int i=0;i<3;++i){put(a,28+2*i,t[i]);put(a,34+2*i,r[i]);}
        put(a,46,400);put(a,50,9500);put(a,54,10000);put(a,56,20000);put(a,58,1000);put(a,60,1000);put(a,66,200);return a;
    }
    static QByteArray apply(SensorKind kind,const SensorCalibration &c,const QByteArray &header,const QByteArray &aux,QString &error){
        if(header.size()!=256||aux.size()!=256||!validate(kind,c,error))return {};
        QByteArray p=kind==SensorKind::Tps?aux:header;
        if(kind==SensorKind::Tps){if(p==QByteArray(256,0)){if(!c.channel)return p;p=tpsDefaults();}p[5]=char(c.channel);put(p,6,c.raw[0]);put(p,8,c.raw[1]);}
        else if(kind==SensorKind::Clt){p[8]=char(c.channel);for(int i=0;i<3;++i){put(p,32+2*i,c.raw[i]);put(p,64+2*i,int(std::round(c.values[i]*100)));}}
        else {const int at=kind==SensorKind::Map?10:18;p[kind==SensorKind::Map?6:7]=char(c.channel);
            for(int i=0;i<2;++i){put(p,at+2*i,c.raw[i]);put(p,at+4+2*i,int(std::round(c.values[i]*(kind==SensorKind::Map?100:1000))));}}
        return p;
    }
    static bool permitted(SensorKind kind,const QByteArray &base,const QByteArray &desired,const QByteArray &header,const QByteArray &aux,QString &error){
        if(int(kind)<0||int(kind)>3||base.size()!=256||desired.size()!=256||header.size()!=256||aux.size()!=256){error="Invalid sensor page.";return false;}
        const QByteArray h=kind==SensorKind::Tps?header:desired,a=kind==SensorKind::Tps?desired:aux;
        if(u16(h,0)!=2){error="Unsupported calibration schema.";return false;}
        const auto c=decode(kind,h,a);if(!validate(kind,c,error))return false;
        QByteArray allowedBase=base;
        if(kind==SensorKind::Tps && base==QByteArray(256,0) && desired[0]==1)allowedBase=tpsDefaults();
        for(int i=0;i<256;++i){
            const bool allowed=kind==SensorKind::Map?(i==6||(i>=10&&i<18)):
                kind==SensorKind::Lambda?(i==7||(i>=18&&i<26)):
                kind==SensorKind::Clt?(i==8||(i>=32&&i<38)||(i>=64&&i<70)):(i>=5&&i<10);
            if(!allowed && desired[i]!=allowedBase[i]){error="A sensor edit attempted to change unrelated ECU settings.";return false;}
        }
        if(!c.channel)return true;
        if(kind==SensorKind::Clt){if(quint8(a[68])==1&&(u16(a,70)&1)&&quint8(a[72])==c.channel){error="This temperature input is already assigned to IAT.";return false;}return true;}
        const int map=quint8(h[6]),lambda=quint8(h[7]),tps=quint8(a[0])==1?quint8(a[5]):0;
        if((kind!=SensorKind::Map&&c.channel==map)||(kind!=SensorKind::Lambda&&c.channel==lambda)||(kind!=SensorKind::Tps&&c.channel==tps)){error="This input is occupied by another sensor. Choose an unused input.";return false;}
        if(quint8(a[68])==1){const int flags=u16(a,70);const int channels[]={quint8(a[73]),quint8(a[74]),quint8(a[75])},masks[]={2|512,128,256};
            for(int i=0;i<3;++i)if((flags&masks[i])&&channels[i]==c.channel){error="This input is occupied by voltage, launch or flex-fuel acquisition.";return false;}}
        return true;
    }
    static bool tpsPercent(const QByteArray &aux,const QByteArray &runtime,double &value){
        if(aux.size()!=256||aux[0]!=1||runtime.size()!=101||!(quint8(runtime[40])&1))return false;
        const int channel=quint8(aux[5]);if(channel<1||channel>11)return false;
        const int closed=u16(aux,6),open=u16(aux,8),tolerance=u16(aux,66);if(closed==open||closed>4095||open>4095||tolerance>1000)return false;
        int sample=u16(runtime,61+2*(channel-1));if(sample>4095)return false;
        const int minimum=qMin(closed,open),maximum=qMax(closed,open),margin=(maximum-minimum)*tolerance/10000;
        if(sample<minimum-margin||sample>maximum+margin)return false;
        sample=qBound(minimum,sample,maximum);value=((sample-closed)*10000/(open-closed))/100.0;return true;
    }
    // Proteus v0.7 AV/AT input stages: 5.6k series + 10k to ground;
    // external connector voltage = ADC voltage * 1.56. Reference from page 1.
    static double voltage(int raw,const QByteArray &header){return header.size()==256?raw*(u16(header,100)/1000.0)*1.56/4095.0:0;}
    static int raw(double volts,const QByteArray &header){if(header.size()!=256||u16(header,100)<2800||u16(header,100)>3600||!std::isfinite(volts))return -1;return int(std::round(volts*4095.0/(u16(header,100)/1000.0*1.56)));}
};
}
