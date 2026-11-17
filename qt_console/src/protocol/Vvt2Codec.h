#pragma once
#include "VvtCodec.h"
#include "../ecu/EcuPowerState.h"
namespace tunuino {
class Vvt2Codec {
public:
    static bool supported(const QByteArray &id){return EcuPowerState::supportedIdentity(id)&&id[13]==0&&id[14]==2&&id[15]==0&&VvtCodec::advancedSupported(id)&&(quint8(id[17])&8)&&(quint8(id[17])&16)&&(quint8(id[18])&32);}
    static bool pulsesSupported(const QByteArray &id){return EcuPowerState::supportedIdentity(id)&&id[13]==0&&id[14]==2&&id[15]==0&&(quint8(id[17])&16);}
    static bool format(const QByteArray &p){return p.size()==256&&p.mid(1,63)==QByteArray(63,0)&&VvtCodec::extension(p);}
    static QByteArray defaults(QByteArray p=QByteArray(256,0)){if(!format(p))return {};if(!p[0])p[0]=2;return VvtCodec::advanced(VvtCodec::defaults(p),!p[64]);}
    static void bind(QByteArray &p,const QByteArray &h){if(p.size()!=256||h.size()!=256)return;p[116]=h[3];p[117]=h[4];p[121]=h[5]==1?h[9]:char(0);p[122]=h[57]==1?h[60]:char(0);}
    static bool allowed(const QByteArray &p,const QByteArray &h,const QByteArray &a,const QByteArray &first,QString &error){
        if(!format(p)){error="Invalid CAM 2 configuration.";return false;}if(!p[65])return true;
        if(h.size()!=256||a.size()!=256||first.size()!=256||h[57]!=1||h[58]!=1){error="Assign the CAM 2 sensor in Inputs and outputs before enabling VVT CAM 2.";return false;}
        auto expected=p;bind(expected,h);if(expected.mid(116,2)!=p.mid(116,2)||expected.mid(121,2)!=p.mid(121,2)){error="CAM 2 or crank acquisition changed. Review and write CAM 2 control again.";return false;}
        // Reuse controller checks with CAM 2's independent window and CAM 1's VTEC prefix.
        auto header=h,control=p;header[153]=p[118];header[154]=p[119];header[155]=p[120];header[98]=1;header[99]=p[122];control[1]=first[1];control[2]=first[2];
        if(first[65]&&first[66]==p[66]){error="The output is already assigned to VVT CAM 1.";return false;}
        if(!VvtCodec::allowed(control,header,a,error)){error.replace("CAM 1","CAM 2");return false;}return true;
    }
};
}
