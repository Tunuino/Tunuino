#pragma once
#include "SensorCalibrationCodec.h"
namespace tunuino {
class VssCodec {
public:
 static bool supported(const QByteArray &id){return id.size()==32&&(quint8(id[18])&16);}
 static quint32 u32(const QByteArray &p,int at){return (quint32(SensorCalibrationCodec::u16(p,at))<<16)|quint32(SensorCalibrationCodec::u16(p,at+2));}
 static bool format(const QByteArray &h){if(h.size()!=256)return false;if(h.mid(82,14)==QByteArray(14,0))return true;return h[82]==1&&quint8(h[83])<=1&&quint8(h[84])<=1&&quint8(h[85])<=3&&u32(h,86)<=1000000&&SensorCalibrationCodec::u16(h,90)>=100&&SensorCalibrationCodec::u16(h,90)<=5000&&h.mid(92,4)==QByteArray(4,0);}
 static QByteArray defaults(QByteArray h){if(h.size()==256&&h.mid(82,14)==QByteArray(14,0)){h[82]=1;SensorCalibrationCodec::put(h,90,1000);}return h;}
 static bool reply(const QByteArray &p,quint32 generation){if(p.size()!=28||p[0]!=0||p[1]!=1||u32(p,20)!=generation)return false;const int flags=quint8(p[2]),reason=quint8(p[3]);if(flags==0)return reason>=1&&reason<=6;if((flags!=1&&flags!=3)||u32(p,12)<100)return false;return flags==3?reason==0&&u32(p,8)<=500000:reason==7&&u32(p,8)==0;}

};
}
