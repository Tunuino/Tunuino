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
    static QByteArray encode(int page,const MapPageValues &values,QString &error) {
        error.clear();
        if(page!=2 && page!=3) {error=QStringLiteral("Unsupported ECU map page.");return {};}
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
            if(!scaled(values.cells[i],100,page==2?0:-9000,page==2?30000:9000,cell)) {
                error=QStringLiteral("Invalid cell %1. Fuel: 0–300; Ignition: -90–90 degrees; up to two decimals.").arg(i+1);return {};
            }
            put(64+2*i,cell);
        }
        return bytes;
    }
    static bool valid(int page,const QByteArray &bytes) {
        if(bytes.size()!=576)return false;
        const auto u16=[&](int at){return (unsigned(quint8(bytes[at]))<<8)|unsigned(quint8(bytes[at+1]));};
        MapPageValues v;
        for(int i=0;i<16;++i){v.rpm[i]=u16(2*i);v.load[i]=u16(32+2*i)/100.0;}
        for(int i=0;i<256;++i)v.cells[i]=qint16(u16(64+2*i))/100.0;
        QString error;return encode(page,v,error)==bytes;
    }
};
}
