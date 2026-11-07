#pragma once
#include "EngineProfileDraft.h"
#include <QJsonArray>
#include <QStringList>

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
    QStringList sources;
};
class TriggerPresets {
public:
    enum Group {Primary=1,Secondary=2,Cam2=4,Firing=8,IgnitionOutputs=16,InjectorPhase=32,Dwell=64,VvtOffsets=128};
    static const QVector<TriggerPreset> &catalog(){
        static const QVector<TriggerPreset> entries{
            {"2JZ VVTi",36,2,{1,5,3,6,2,4},"36-2 crank-sensor variants only. OEM cam decoding, timing reference and VVT offsets are not configured.",{"https://support.haltech.com/portal/en/kb/articles/2jz-1jz-engine","https://support.haltech.com/portal/en/kb/articles/2jz-vvt-i-trigger"}},
            {"BMW E36 M52B20",60,2,{1,5,3,6,2,4},"Primary wheel and firing order only. Early frequency-type cam sensors need adaptation; cam decoding and timing reference are not configured.",{"https://support.haltech.com/portal/en/kb/articles/m52-engine"}},
            {"BMW E46 N42B18",0,0,{1,3,4,2},"Firing order only. Primary trigger, cam decoding, timing reference and Valvetronic settings are not configured.",{"https://workshop-manuals.com/bmw/3_series_e46/316i_n42_tour/3_technical_data/12__engine_electrical_system_%28n42%29/12__ignition_wires_spark_plugs/2_td__ignition_harness_spark_plugs_e46/"}},
            {"BMW E46 M54B30",60,2,{1,5,3,6,2,4},"Primary wheel and firing order only. Cam decoding, VANOS offsets and timing reference are not configured.",{"https://manuals.plus/m/076f9e1e05f1686beecceb8cfdf2d6b35d0ab37bfb5ed089ae94c8918a37aef6.pdf"}},
            {"FORD FOCUS MK2 ST225 B5254",0,0,{1,2,4,5,3},"Firing order for the 2.5 Duratec-ST HYDA only. Primary/cam triggers, timing reference and VVT offsets are not configured.",{"https://workshop-manuals.com/ford/focus_2004.75_07.2004/mechanical_repairs/3_powertrain/303_engine/303-01d_engine_2.5l_duratec-st_%28vi5%29/specifications/"}},
            {"LEXUS IS200 1G-FE",0,0,{},"Preset remains unavailable until primary-source engine data is confirmed.",{}},
            {"LOTUS ELISE CUP 2ZR-FE",36,2,{1,3,4,2},"2ZR-FE engine geometry and firing order only. Cam decoding, timing reference, VVT and supercharger controls are not configured.",{"https://pdfcoffee.com/fsm-workshop-manualpdf-pdf-free.html","https://charm.li/Toyota/2010/Corolla%20L4-1.8L%20%282ZR-FE%29/Repair%20and%20Diagnosis/Specifications/Mechanical%20Specifications/Firing%20Order/"}},
            {"MAZDA MX-5 NC LF-VE",0,0,{1,3,4,2},"Firing order only. The OEM 36-2-2-2 crank and cam patterns require dedicated decoders.",{"https://support.haltech.com/portal/en/kb/articles/mazda-lf-mzr-engine"}},
            {"MINI COOPER R50 R53",0,0,{1,3,4,2},"W10/W11 firing order only. Primary/cam triggers, timing reference and dwell are not configured.",{"https://charm.li/Mini/2003/Cooper%20%28R50%29%20L4-1.6L%20%28W10%29/Repair%20and%20Diagnosis/Maintenance/Tune-up%20and%20Engine%20Performance%20Checks/Firing%20Order/Specifications/","https://charm.li/Mini/2002/Cooper%20S%20%28R53%29%20L4-1.6L%20SC%20%28W11%29/Repair%20and%20Diagnosis/Powertrain%20Management/Ignition%20System/Description%20and%20Operation/"}},
            {"NISSAN VQ35DE 350Z / Skyline",0,0,{1,2,3,4,5,6},"Firing order only. The OEM multiple-gap crank/cam pattern requires dedicated decoders; VQ35HR is not covered.",{"https://support.haltech.com/portal/en/kb/articles/vq35de-engine"}},
            {"VW R32 BUB BFH",0,0,{1,5,3,6,2,4},"Firing order only. Primary/cam decoding, timing reference and VVT offsets are not configured.",{"https://www.billswebspace.com/R32TechnicalSpecifications.pdf","https://workshop-manuals.com/volkswagen/golf-mk5/power_unit/6-cylinder_injection_engine/technical_data/engine_data/sticky-snack.php"}},
            {"TOYOTA 1JZ VVTi",36,2,{1,5,3,6,2,4},"Primary wheel and firing order only. OEM three-tooth cam decoding, timing reference and VVT offsets are not configured.",{"https://support.haltech.com/portal/en/kb/articles/2jz-1jz-engine"}},
            {"TOYOTA 1NZ-FE",36,2,{1,3,4,2},"Primary wheel and firing order only. OEM three-tooth cam decoding, timing reference and VVT offsets are not configured.",{"https://support.haltech.com/portal/en/kb/articles/1nz-fe-engine"}},
            {"TOYOTA 2NZ-FE",36,2,{1,3,4,2},"Primary wheel and firing order only. OEM three-tooth cam decoding, timing reference and VVT offsets are not configured.",{"https://support.haltech.com/portal/en/kb/articles/1nz-fe-engine"}},
            {"TOYOTA 1AZ-FE",36,2,{1,3,4,2},"Primary wheel and firing order only. OEM three-tooth cam decoding, timing reference and VVT offsets are not configured.",{"https://support.haltech.com/portal/en/kb/articles/2az-fe-engine"}},
            {"TOYOTA 2AZ-FE",36,2,{1,3,4,2},"Primary wheel and firing order only. OEM three-tooth cam decoding, timing reference and VVT offsets are not configured.",{"https://support.haltech.com/portal/en/kb/articles/2az-fe-engine"}},
            {"TOYOTA 2ZZ-GE",36,2,{1,3,4,2},"Primary wheel and firing order only. OEM three-tooth cam decoding, timing reference and VVT offsets are not configured.",{"https://support.haltech.com/portal/en/kb/articles/2zz-ge-engine"}},
            {"TOYOTA 1UZ-FE VVTi (1998-on)",36,2,{1,8,4,3,6,5,7,2},"Primary wheel and firing order only. OEM three-tooth cam decoding, timing reference and VVT offsets are not configured. Early 12-tooth 1UZ variants are excluded.",{"https://support.haltech.com/portal/en/kb/articles/1uz-fe-engine"}},
            {"TOYOTA 2UZ-FE (1998-2005)",36,2,{1,8,4,3,6,5,7,2},"Primary wheel and firing order only. OEM three-tooth cam decoding, timing reference and VVT offsets are not configured. Early 12-tooth 1UZ variants are excluded.",{"https://support.haltech.com/portal/en/kb/articles/1uz-fe-engine"}},
            {"TOYOTA 2UZ-FE VVTi",36,2,{1,8,4,3,6,5,7,2},"Primary wheel and firing order only. OEM three-tooth cam decoding, timing reference and VVT offsets are not configured. Early 12-tooth 1UZ variants are excluded.",{"https://support.haltech.com/portal/en/kb/articles/1uz-fe-engine"}},
            {"TOYOTA 3UZ-FE VVTi",36,2,{1,8,4,3,6,5,7,2},"Primary wheel and firing order only. OEM three-tooth cam decoding, timing reference and VVT offsets are not configured. Early 12-tooth 1UZ variants are excluded.",{"https://support.haltech.com/portal/en/kb/articles/1uz-fe-engine"}},
            {"FORD ZETEC-E 2.0",36,1,{1,3,4,2},"Zetec-E family only, not Mazda MZR/Duratec-HE. Cam decoding, coil wiring and timing reference are not configured.",{"https://support.haltech.com/portal/en/kb/articles/duratec-zeta-2-0l-4-cyl"}},
            {"FORD BARRA TURBO BA-BF-FG",36,1,{1,5,3,6,2,4},"Turbo variants only. OEM 3+1 cam decoding, timing reference and VCT offsets are not configured.",{"https://support.haltech.com/portal/en/kb/articles/4-0l-barra-engine"}},
            {"GM LS2 GEN IV 58x",60,2,{1,8,7,2,6,5,4,3},"58x (60-2) crank variants only, not Gen III 24x. OEM four-tooth cam decoding, timing reference and dwell are not configured.",{"https://support.haltech.com/portal/en/kb/articles/ls2-ls3-gen-4-engine","https://support.haltech.com/portal/en/kb/articles/ls2-ls3-trigger"}},
            {"GM LS3 GEN IV 58x",60,2,{1,8,7,2,6,5,4,3},"58x (60-2) crank variants only, not Gen III 24x. OEM four-tooth cam decoding, timing reference and dwell are not configured.",{"https://support.haltech.com/portal/en/kb/articles/ls2-ls3-gen-4-engine","https://support.haltech.com/portal/en/kb/articles/ls2-ls3-trigger"}}
        };return entries;
    }
    static bool valid(const TriggerPreset &p){
        if(p.name.isEmpty()||p.limitation.isEmpty())return false;
        if(p.teeth){if(p.teeth<4||p.teeth>120||p.missing<1||p.missing>=p.teeth/2)return false;}else if(p.missing)return false;
        if(p.order.size()>12)return false;
        quint32 used=0;for(int cylinder:p.order){if(cylinder<1||cylinder>p.order.size()||(used&(1u<<cylinder)))return false;used|=1u<<cylinder;}
        if((p.teeth||!p.order.isEmpty())&&p.sources.isEmpty())return false;
        for(const auto &url:p.sources)if(!url.startsWith("https://")||url.contains("..."))return false;
        return true;
    }
    static int supported(int index){if(index<0||index>=catalog().size())return 0;const auto &p=catalog()[index];if(!valid(p))return 0;return (p.teeth?Primary:0)|(!p.order.isEmpty()?Firing:0);}
    static QString description(int index){
        if(index<0||index>=catalog().size())return {};
        const auto &p=catalog()[index];QStringList facts;
        if(p.teeth)facts<<QString("Primary: %1-%2.").arg(p.teeth).arg(p.missing);
        if(!p.order.isEmpty()){QStringList order;for(int cylinder:p.order)order<<QString::number(cylinder);facts<<QString("%1 cylinders; firing order: %2.").arg(p.order.size()).arg(order.join('-'));}
        facts<<p.limitation;return facts.join(' ');
    }
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
