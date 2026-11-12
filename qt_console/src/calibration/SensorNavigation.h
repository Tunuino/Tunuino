#pragma once
#include "../ui/CalibrationAppearance.h"
#include <QTreeWidget>
namespace tunuino {
inline void populateSensorNavigation(QTreeWidgetItem *root){
    using namespace CalibrationAppearance;
    const auto group=[](QTreeWidgetItem *parent,const QString &name){auto *item=new QTreeWidgetItem(parent,{name});item->setIcon(0,icon(Group));return item;};
    const auto page=[](QTreeWidgetItem *parent,const QString &name,const QString &key,Glyph glyph=Parameters){auto *item=new QTreeWidgetItem(parent,{name});item->setIcon(0,icon(glyph));item->setData(0,Qt::UserRole,100);item->setData(0,Qt::UserRole+1,key);return item;};
    page(root,"Analog inputs","inputs.analog");
    page(root,"Analog input monitor","inputs.monitor");
    auto *digital=group(root,"Digital inputs");
    page(digital,"Primary trigger","inputs.primary");
    page(digital,"CAM 1","inputs.cam1");
    page(digital,"CAM 2","inputs.cam2");
    page(digital,"VSS","inputs.vss");
    page(digital,"Primary trigger monitor","inputs.crank");
    page(digital,"CAM 1 monitor","inputs.cam1.monitor");
    page(digital,"CAM 2 monitor","inputs.cam2.monitor");
    page(root,"Wideband oxygen sensor","sensor:Lambda");
    page(root,"TPS","sensor:TPS");
    auto *ntc=group(root,"IAT, CLT");
    page(ntc,"IAT sensor wizard","sensorwizard:IAT",Wizard);
    page(ntc,"CLT sensor wizard","sensorwizard:CLT",Wizard);
    page(ntc,"IAT calibration","sensor:IAT",Curve);
    page(ntc,"CLT calibration","sensor:CLT",Curve);
    page(root,"MAP calibration","sensor:MAP",Curve);
    auto *pressure=group(root,"Pressure");
    page(pressure,"Sensor wizard","sensorwizard:Pressure",Wizard);
    page(pressure,"Parameters","inputs.pressure");
    page(pressure,"Fuel pressure cal.","sensor:Fuel pressure",Curve);
    page(pressure,"Oil pressure cal.","sensor:Oil pressure",Curve);
    page(pressure,"Pressure monitor","inputs.pressure.monitor");
    auto *temperature=group(root,"Temperature");
    page(temperature,"Sensor wizard","sensorwizard:Temperature",Wizard);
    page(temperature,"Standard sensors","inputs.temperature");
    page(root,"Battery","sensor:Battery");
    auto *vss=group(root,"Vehicle speed");
    page(vss,"Parameters","speed.vehicle");
    page(vss,"Vehicle speed monitor","speed.monitor");
}
}
