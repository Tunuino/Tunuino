#pragma once
#include "../tools/ToolsData.h"
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <cmath>

namespace tunuino {
// Commissioning only: this model never computes a motor command or arms outputs.
class DbwSignals {
public:
    struct Signal { int channel=0; double rest=0,full=0; };
    std::array<Signal,4> tracks{}; // APS main/check, TPS main/check.
    double disagreement=5;
    ToolSnapshot base;
    bool available=false;
    QByteArray runtime;
    QElapsedTimer age;
    static QJsonObject defaults(){return DbwSignals{}.state();}
    QJsonObject state()const{
        QJsonArray a;for(const auto &s:tracks)a.append(QJsonObject{{"input",s.channel},{"rest",s.rest},{"full",s.full}});
        return {{"format",1},{"signals",a},{"disagreement",disagreement}};
    }
    bool restore(const QJsonObject &o,bool apply,QString &error){
        const auto a=o["signals"].toArray();const auto tolerance=o["disagreement"];
        bool valid=o.keys()==QStringList({"disagreement","format","signals"})&&o["format"]==1&&a.size()==4&&tolerance.isDouble()&&std::isfinite(tolerance.toDouble())&&tolerance.toDouble()>=1&&tolerance.toDouble()<=20;
        std::array<Signal,4> next{};
        for(int i=0;valid&&i<4;++i){const auto s=a[i].toObject();const double c=s["input"].toDouble(-1),lo=s["rest"].toDouble(-1),hi=s["full"].toDouble(-1);
            valid=s.keys()==QStringList({"full","input","rest"})&&s["input"].isDouble()&&std::isfinite(c)&&c==std::floor(c)&&c>=0&&c<=11&&s["rest"].isDouble()&&s["full"].isDouble()&&std::isfinite(lo)&&std::isfinite(hi)&&lo>=0&&lo<=6&&hi>=0&&hi<=6;
            next[i]={int(valid?c:0),lo,hi};
        }
        if(!valid){error="Invalid DBW signal calibration. Nothing was loaded.";return false;}
        if(apply){resetCapture(0);resetCapture(1);tracks=next;disagreement=tolerance.toDouble();invalidate();}error.clear();return true;
    }
    void invalidate(){runtime.clear();age.invalidate();}
    void observe(const ToolSnapshot &snapshot,const QByteArray &body){
        if(base.identity!=snapshot.identity||base.generation!=snapshot.generation){invalidate();resetCapture(0);resetCapture(1);}base=snapshot;
        if(!available||!base.valid()||body.size()!=101||quint8(body[0])!=0||body.mid(1,31)!=base.identity.mid(1,31)
            ||u32(body,57)!=base.generation||!(quint8(body[40])&1)||(quint8(body[40])&2)){invalidate();return;}
        const int reference=SensorCalibrationCodec::u16(base.pages[0],100);
        if(reference<2800||reference>3600){invalidate();return;}runtime=body;age.start();
    }
    bool fresh()const{return available&&base.valid()&&runtime.size()==101&&age.isValid()&&age.elapsed()<=1500;}
    bool stationary()const{return fresh()&&quint8(runtime[32])==0&&u32(runtime,41)==0&&u32(runtime,97)==0;}
    static quint32 u32(const QByteArray &b,int at){return (quint32(SensorCalibrationCodec::u16(b,at))<<16)|quint32(SensorCalibrationCodec::u16(b,at+2));}
    bool voltage(int row,double &v)const{
        if(!fresh()||row<0||row>=4||tracks[row].channel<1||tracks[row].channel>11)return false;
        const int raw=SensorCalibrationCodec::u16(runtime,61+2*(tracks[row].channel-1));
        // Saturation/shorts are not usable calibration measurements.
        if(raw<=0||raw>=4095)return false;v=SensorCalibrationCodec::voltage(raw,base.pages[0]);return true;
    }
    QString assignmentError()const{
        QSet<int> assigned;
        for(int i=0;i<4;++i){const int channel=tracks[i].channel;if(!channel)return "Assign four independent AV inputs.";if(assigned.contains(channel))return "Main and check signals require independent inputs.";assigned.insert(channel);
            if(base.valid()){
                // Existing primary TPS can be reused only by the TPS main signal.
                for(const auto &role:inputAssignments(base).value(channel-1).functions)if(!(i==2&&role=="TPS"))return QString("AV%1 is occupied by %2.").arg(channel).arg(role);
                const auto &h=base.pages[0];if(h[38]==1&&(quint8(h[39])==channel||quint8(h[48])==channel))return QString("AV%1 is occupied by a pressure sensor.").arg(channel);
            }
        }return {};
    }
    bool calibrated(int row)const{return row>=0&&row<4&&tracks[row].channel&&std::abs(tracks[row].full-tracks[row].rest)>=1.0;}
    bool position(int row,double &percent)const{
        double v;if(!calibrated(row)||!voltage(row,v))return false;const auto &s=tracks[row];percent=100*(v-s.rest)/(s.full-s.rest);return std::isfinite(percent)&&percent>=-2&&percent<=102;
    }
    QString diagnostic(int pair)const{
        const auto conflict=assignmentError();if(!conflict.isEmpty())return conflict;
        if(!fresh())return "No fresh ECU sample.";
        const int row=pair*2;double a,b;
        if(!calibrated(row)||!calibrated(row+1))return "Calibrate both signals: minimum span 1 V.";
        if(!position(row,a)||!position(row+1,b))return "Signal outside calibration range or ADC invalid.";
        if(std::abs(a-b)>disagreement)return "Main / check disagreement.";
        return "Signals agree (diagnostic only).";
    }
    void resetCapture(int pair){if(pair>=0&&pair<2){captureIdentity_[pair].clear();captureGeneration_[pair]=0;}}
    bool capture(int pair,bool full,QString &error){
        error=assignmentError();if(!error.isEmpty())return false;
        if(pair<0||pair>1||!stationary()){error="Capture requires a fresh sample, stopped engine and disarmed outputs.";return false;}
        if(full&&(captureIdentity_[pair]!=base.identity||captureGeneration_[pair]!=base.generation)){error="Capture 0% first in the current ECU session.";return false;}
        double a,b;if(!voltage(pair*2,a)||!voltage(pair*2+1,b)){error="Both signals must have valid, non-saturated ADC readings.";return false;}
        if(full){tracks[pair*2].full=a;tracks[pair*2+1].full=b;}else{tracks[pair*2].rest=a;tracks[pair*2+1].rest=b;tracks[pair*2].full=a;tracks[pair*2+1].full=b;captureIdentity_[pair]=base.identity;captureGeneration_[pair]=base.generation;}return true;
    }
private:
    std::array<QByteArray,2> captureIdentity_{};
    std::array<quint32,2> captureGeneration_{};
};
}
