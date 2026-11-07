#pragma once
#include "EngineProfileDraft.h"
#include <QJsonArray>

namespace tunuino {
// Preset facts are independent of ECU wiring and of the native tooth-zero reference.
// Sources: https://support.haltech.com/portal/en/kb/articles/2jz-1jz-engine
// https://support.haltech.com/portal/en/kb/articles/2jz-vvt-i-trigger
// https://support.haltech.com/portal/en/kb/articles/m52-engine
struct TriggerPreset {
    QString name;
    int teeth=0,missing=0;
    QVector<int> order;
    QString limitation;
};
class TriggerPresets {
public:
    enum Group {Primary=1,Secondary=2,Cam2=4,Firing=8,IgnitionOutputs=16,InjectorPhase=32,Dwell=64,VvtOffsets=128};
    static const QVector<TriggerPreset> &catalog(){
        static const QVector<TriggerPreset> entries{
            {"2JZ VVTi",36,2,{1,5,3,6,2,4},"Primary wheel and firing order only. The OEM three-tooth cam decoder, timing reference and VVT offsets are not configured."},
            {"BMW E36 M52B20",60,2,{1,5,3,6,2,4},"Primary wheel and firing order only. Early OEM frequency-type cam sensors require replacement/adaptation; cam decoding and timing reference are not configured."},
            {"BMW E46 N42B18",0,0,{},"Engine preset not validated for the native decoder."},
            {"BMW E46 M54B30",0,0,{},"Engine preset not validated for the native decoder."},
            {"FORD FOCUS MK2 ST225 B5254",0,0,{},"Engine preset not validated for the native decoder."},
            {"LEXUS IS200 1G-FE",0,0,{},"Engine preset not validated for the native decoder."},
            {"LOTUS ELISE CUP 2ZR-FE",0,0,{},"Engine preset not validated for the native decoder."},
            {"MAZDA MX-5 NC",0,0,{},"Engine preset not validated for the native decoder."},
            {"MINI COOPER R50 R53",0,0,{},"Engine preset not validated for the native decoder."},
            {"NISSAN VQ35",0,0,{},"The OEM multiple-gap crank/cam pattern needs a dedicated decoder."},
            {"VW R32 BUB BFH",0,0,{},"Engine preset not validated for the native decoder."}
        };return entries;
    }
    static int supported(int index){if(index<0||index>=catalog().size())return 0;const auto &p=catalog()[index];return (p.teeth?Primary:0)|(!p.order.isEmpty()?Firing:0);}
    // Produces a local editor draft only. Never changes base pages or sends ECU commands.
    static bool apply(const QJsonObject &current,int index,int groups,QJsonObject &result,QString &error){
        error.clear();if(!groups||(groups&~supported(index))){error="Select at least one supported preset group.";return false;}
        const auto &preset=catalog()[index];auto next=current;auto parameters=next["parameters"].toObject();
        if(groups&Primary){
            auto primary=parameters["ignition.primary"].toObject();auto values=primary["values"].toObject();
            auto trigger=next["trigger"].toObject();
            if(!values.contains("teeth")||!values.contains("missing")||!trigger.contains("teeth")||!trigger.contains("missing")){error="Missing primary trigger editor.";return false;}
            values["teeth"]=preset.teeth;values["missing"]=preset.missing;primary["values"]=values;parameters["ignition.primary"]=primary;
            trigger["teeth"]=preset.teeth;trigger["missing"]=preset.missing;next["trigger"]=trigger;
        }
        if(groups&Firing){
            auto firing=parameters["ignition.firing"].toObject();auto values=firing["values"].toObject();
            const auto pages=current["pages"].toObject();EngineProfileDraft draft;
            draft.load(QByteArray::fromHex(pages["header"].toString().toLatin1()),QByteArray::fromHex(pages["aux"].toString().toLatin1()));
            if(!firing["engineProfile"].isObject()||!draft.import(firing["engineProfile"].toObject(),error,true))return false;
            // Existing output assignments and event angles survive when count is unchanged.
            // A new cylinder remains explicitly unassigned, as in Firing order.
            draft.resize(preset.order.size());for(int i=0;i<preset.order.size();++i)draft.setOrder(i,preset.order[i]-1);
            values["cylinders"]=draft.count();for(int i=0;i<12;++i)values[QString("order%1").arg(i)]=i<draft.count()?draft.order[i]+1:0;
            firing["values"]=values;firing["engineProfile"]=draft.json();parameters["ignition.firing"]=firing;
        }
        next["parameters"]=parameters;result=next;return true;
    }
};
}
