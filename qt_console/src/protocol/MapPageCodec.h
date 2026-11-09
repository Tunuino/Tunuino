#pragma once
#include <QByteArray>
#include <QString>
#include <array>
#include <cmath>

namespace tunuino {
struct MapPageValues {
    std::array<double,16> rpm{},load{}; // Load increases from index 0 to 15.
    std::array<double,256> cells{};    // Load-major, then RPM.
};
class MapPageCodec {
public:
    static bool afrSupported(const QByteArray &id){return id.size()==32&&(quint8(id[18])&64);}
    static QByteArray encode(int page,const MapPageValues &values,QString &error) {
        error.clear();
        if(page!=2 && page!=3 && page!=5) {error=QStringLiteral("Unsupported ECU map page.");return {};}
        QByteArray bytes(576,0);
        const auto put=[&](int offset,int value){bytes[offset]=char(unsigned(value)>>8);bytes[offset+1]=char(value);};
        const auto scaled=[&](double value,double scale,int low,int high,int &out) {
            if(!std::isfinite(value) || value*scale<low || value*scale>high)return false;
            const double rounded=std::round(value*scale);
            if(std::abs(value*scale-rounded)>1e-6)return false;
            out=int(rounded);return true;
        };
        int priorRpm=-1,priorLoad=-1;
        for(int i=0;i<16;++i) {
            int rpm,load;
            if(!scaled(values.rpm[i],1,0,20000,rpm) || !scaled(values.load[i],100,0,50000,load)
                || rpm<=priorRpm || load<=priorLoad) {
                error=QStringLiteral("Axes must increase strictly: RPM 0–20000 (whole numbers), MAP 0–500 kPa (up to two decimals).");return {};
            }
            put(2*i,rpm);put(32+2*i,load);priorRpm=rpm;priorLoad=load;
        }
        for(int i=0;i<256;++i) {
            int cell;
            if(page==5){const double lambda=values.cells[i]/14.7;if(!std::isfinite(lambda)||lambda<.65-1e-9||lambda>1.3+1e-9){error=QStringLiteral("AFR Target must be 9.555–19.11 (14.7 reference), equivalent to lambda 0.650–1.300.");return {};}cell=int(std::round(lambda*1000));}
            else if(!scaled(values.cells[i],100,page==2?0:-9000,page==2?30000:9000,cell)) {
                error=QStringLiteral("Invalid cell %1. Fuel: 0–300; Ignition: -90–90 degrees; up to two decimals.").arg(i+1);return {};
            }
            put(64+2*i,cell);
        }
        return bytes;
    }
    static bool valid(int page,const QByteArray &bytes) {
        if(bytes.size()!=576)return false;
        if(page==5&&bytes==QByteArray(576,0))return true;
        const auto u16=[&](int at){return (unsigned(quint8(bytes[at]))<<8)|unsigned(quint8(bytes[at+1]));};
        MapPageValues v;
        for(int i=0;i<16;++i){v.rpm[i]=u16(2*i);v.load[i]=u16(32+2*i)/100.0;}
        for(int i=0;i<256;++i)v.cells[i]=page==5?u16(64+2*i)*14.7/1000.:qint16(u16(64+2*i))/100.0;
        QString error;return encode(page,v,error)==bytes;
    }
};
}
