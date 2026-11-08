#pragma once
#include "VssCodec.h"
namespace tunuino {
class Cam2Codec {
public:
 static bool supported(const QByteArray &id){return id.size()==32&&(quint8(id[18])&32);}
 static bool format(const QByteArray &h){if(h.size()!=256)return false;if(h.mid(57,7)==QByteArray(7,0))return true;return h[57]==1&&quint8(h[58])<=1&&quint8(h[59])<=1&&quint8(h[60])<=3&&SensorCalibrationCodec::u16(h,61)>=100&&SensorCalibrationCodec::u16(h,61)<=5000&&h[63]==0;}
 static QByteArray defaults(QByteArray h){if(h.size()==256&&h.mid(57,7)==QByteArray(7,0)){h[57]=1;SensorCalibrationCodec::put(h,61,1000);}return h;}
 static bool reply(const QByteArray &p,quint32 generation){if(p.size()!=28||p[0]!=0||p[1]!=1||VssCodec::u32(p,20)!=generation||VssCodec::u32(p,8)!=0)return false;const int flags=quint8(p[2]),reason=quint8(p[3]);return flags==0?(reason>=1&&reason<=6):flags==1&&reason==0&&VssCodec::u32(p,12)>=100&&VssCodec::u32(p,4)>0;}
};
}
