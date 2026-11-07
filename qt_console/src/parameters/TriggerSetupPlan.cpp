#include "TriggerSetupPlan.h"
#include "EngineProfileDraft.h"
#include "../protocol/SensorCalibrationCodec.h"
#include <cmath>
namespace tunuino {
bool TriggerSetupPlan::hasProfile(const QByteArray &h){return h.size()==256&&quint8(h[128])>=1&&quint8(h[128])<=3&&quint8(h[129])>=1&&quint8(h[129])<=12;}
TriggerSetup TriggerSetupPlan::read(const QByteArray &h){TriggerSetup s;if(h.size()!=256)return s;s.capture=h[2]!=0;s.teeth=quint8(h[3]);s.missing=quint8(h[4]);s.maximumRpm=SensorCalibrationCodec::u16(h,96);
    if(hasProfile(h)){s.toothZero=((quint32(SensorCalibrationCodec::u16(h,146))<<16)|quint32(SensorCalibrationCodec::u16(h,148)))/100.0;s.camFirst=quint8(h[153]);s.camLast=quint8(h[154]);s.inverted=h[155]!=0;}return s;}
bool TriggerSetupPlan::primaryValid(const TriggerSetup &s,QString &error){error.clear();if(s.teeth<4||s.teeth>120||s.missing<1||s.missing>=s.teeth/2){error="Use a missing-tooth wheel: 4–120 nominal teeth and fewer missing teeth than half the wheel.";return false;}if(s.maximumRpm<1000||s.maximumRpm>20000){error="Hard RPM limit must be 1000–20000 RPM.";return false;}return true;}
QByteArray TriggerSetupPlan::mask(bool profile){QByteArray m(256,0);m[2]=1;m[3]=m[4]=m[96]=m[97]=char(255);if(profile){for(int i=146;i<150;++i)m[i]=char(255);m[153]=m[154]=char(255);m[155]=1;}return m;}
QByteArray TriggerSetupPlan::apply(const QByteArray &h,const QByteArray &a,const TriggerSetup &s,QString &error){
    error.clear();if(h.size()!=256||a.size()!=256||SensorCalibrationCodec::u16(h,0)!=2){error="Read a supported, complete ECU configuration first.";return {};}
    if(!primaryValid(s,error))return {};
    if(a[68]==1&&(SensorCalibrationCodec::u16(a,70)&64)&&SensorCalibrationCodec::u16(a,158)>=s.maximumRpm){error="Hard RPM limit must exceed the configured soft fuel-cut limiter.";return {};}
    auto desired=h;desired[2]=s.capture?1:0;desired[3]=char(s.teeth);desired[4]=char(s.missing);SensorCalibrationCodec::put(desired,96,s.maximumRpm);
    if(hasProfile(h)){
        if(!std::isfinite(s.toothZero)||s.toothZero<0||s.toothZero>=720||std::abs(s.toothZero*100-std::round(s.toothZero*100))>1e-6||s.camFirst<0||s.camLast<s.camFirst||s.camLast>=s.teeth-s.missing){error="Check tooth-zero angle and the cam tooth window for the selected wheel.";return {};}
        const quint32 angle=quint32(std::lround(s.toothZero*100));SensorCalibrationCodec::put(desired,146,int(angle>>16));SensorCalibrationCodec::put(desired,148,int(angle));desired[153]=char(s.camFirst);desired[154]=char(s.camLast);desired[155]=s.inverted?1:0;
        EngineProfileDraft checked;checked.load(desired,a);if(checked.build(error).isEmpty())return {};
    }
    const auto owned=mask(hasProfile(h));for(int i=0;i<256;++i)if((quint8(desired[i])^quint8(h[i]))&~quint8(owned[i])){error="Unexpected change outside trigger parameters.";return {};}
    return desired;
}
}
