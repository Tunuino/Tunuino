#pragma once
#include "../ui/CalibrationAppearance.h"
#include <QTreeWidget>
namespace tunuino {
inline void populateSensorNavigation(QTreeWidgetItem *root){
    using namespace CalibrationAppearance;
    const auto group=[](QTreeWidgetItem *parent,const QString &name){auto *item=new QTreeWidgetItem(parent,{name});item->setIcon(0,icon(Group));return item;};
    const auto page=[](QTreeWidgetItem *parent,const QString &name,const QString &key,Glyph glyph=Parameters){auto *item=new QTreeWidgetItem(parent,{name});item->setIcon(0,icon(glyph));item->setData(0,Qt::UserRole,100);item->setData(0,Qt::UserRole+1,key);return item;};
    const auto missing=[](QTreeWidgetItem *parent,const QString &name,const QString &reason,Glyph glyph=Parameters){auto *item=new QTreeWidgetItem(parent,{name});item->setIcon(0,icon(glyph));item->setFlags(item->flags()&~Qt::ItemIsEnabled);item->setForeground(0,QColor("#858585"));item->setToolTip(0,reason);return item;};
    const QString unsupported="Not implemented in the native ECU configuration/runtime protocol. No operational control is available.";
    page(root,"Analog inputs","inputs.analog");
    auto *digital=group(root,"Digital inputs");page(digital,"Primary trigger","inputs.primary");page(digital,"CAM 1","inputs.cam1");
    page(digital,"CAM 2","inputs.cam2");page(digital,"VSS","inputs.vss");
    missing(digital,"CAM 1 sensitivity","The conditioned capture input has no software sensitivity curve.",Curve);missing(digital,"CAM 2 sensitivity","The Hall/optical Schmitt input has no programmable VR sensitivity curve.",Curve);
    auto *oxygen=group(root,"Oxygen Sensor");page(oxygen,"Parameters","sensor:Lambda");
    auto *throttle=group(root,"TPS, PPS");page(throttle,"TPS","sensor:TPS");missing(throttle,"PPS",unsupported);
    missing(throttle,"PPS check voltage vs pedal pos.",unsupported,Curve);missing(throttle,"TPS check voltage vs throttle pos.","Independent throttle plausibility curves are not implemented; TPS calibration remains available.",Curve);
    auto *switches=group(root,"Switches");for(const auto &name:{"Clutch, brake","Built-in switches","User switches","MUX Switch","Latching switch"})missing(switches,name,unsupported);
    missing(switches,"Rotary switches",unsupported,Group);
    auto *ntc=group(root,"IAT, CLT");page(ntc,"IAT sensor wizard","sensorwizard:IAT",Wizard);page(ntc,"CLT sensor wizard","sensorwizard:CLT",Wizard);missing(ntc,"Failsafe","Sensor failure behaviour is fixed in the native protections; configurable substitution values are not implemented.");
    page(ntc,"IAT calibration","sensor:IAT",Curve);page(ntc,"CLT calibration","sensor:CLT",Curve);
    auto *map=group(root,"MAP, BARO");page(map,"Parameters","sensor:MAP");missing(map,"BARO calibration",unsupported,Curve);page(map,"MAP calibration","sensor:MAP",Curve);
    auto *pressure=group(root,"Pressure");page(pressure,"Sensor wizard","sensorwizard:MAP",Wizard);page(pressure,"Parameters","inputs.pressure");
    for(const auto &name:{"AC pressure cal.","Back pressure cal.","Coolant pressure cal.","Crankcase pressure cal.","Diff. pressure cal.","Fuel pressure cal.","Nitrous pressure cal.","Oil pressure cal.","Pre throttle boost cal.","Wastegate dome pressure cal."}){if(QString(name)=="Fuel pressure cal.")page(pressure,name,"sensor:Fuel pressure",Curve);else if(QString(name)=="Oil pressure cal.")page(pressure,name,"sensor:Oil pressure",Curve);else missing(pressure,name,unsupported,Curve);}
    auto *temperature=group(root,"Temperature");page(temperature,"Sensor wizard","sensorwizard:Temperature",Wizard);page(temperature,"Standard sensors","inputs.temperature");missing(temperature,"Custom sensors",unsupported);
    for(const auto &name:{"AC evap temp cal.","Cylinder head temp. cal.","Engine oil temp. cal.","Fuel temp cal.","Custom temp. cal 1","Custom temp. cal 2","Custom temp. cal 3","Custom temp. cal 4"})missing(temperature,name,unsupported,Curve);
    auto *other=group(root,"Other sensors");for(const auto &name:{"Parameters","EWG position sensor","Turboshaft speed"})missing(other,name,unsupported);missing(other,"Fuel level cal.",unsupported,Curve);page(other,"Battery","sensor:Battery");
    auto *vss=group(root,"VSS and Gearbox");for(const auto &name:{"Gearbox","Vehicle speed","Wheels setup","Driven axle","Non-driven axle","Paddle shifters"}){if(QString(name)=="Vehicle speed")page(vss,name,"speed.vehicle");else missing(vss,name,unsupported);}
    missing(vss,"Gear position sensor",unsupported,Group);missing(vss,"Gear lever",unsupported,Group);missing(vss,"Vss and gears",unsupported);
}
}
