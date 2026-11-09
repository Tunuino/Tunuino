#include "project/MapProject.h"
#include "parameters/ParameterWidget.h"
#include "parameters/TriggerWizard.h"
#include "calibration/SensorCalibrationWidget.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDoubleSpinBox>
#include <QFile>
#include <QSettings>
#include <iostream>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed "<<#x<<" line "<<__LINE__<<std::endl;std::exit(1);}}while(false)
using namespace tunuino;
int main(int argc,char **argv){QApplication app(argc,argv);QTemporaryDir temp;QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());
    const auto pages=MapProject::defaultPages();auto header=QByteArray::fromHex(pages["header"].toString().toLatin1()),aux=QByteArray::fromHex(pages["aux"].toString().toLatin1());
    QJsonObject parameters;for(const auto &b:ParameterCatalog::blocks()){ParameterWidget editor(b);if(b.key=="ignition.firing"){editor.findChild<QDoubleSpinBox*>("cylinders")->setValue(6);}const auto state=editor.editorState();QString error;CHECK(editor.restoreEditor(state,false,error));ParameterWidget restored(b);CHECK(restored.restoreEditor(state,true,error));CHECK(restored.editorState()==state);parameters[b.key]=state;}
    QJsonObject sensors;for(auto kind:{SensorKind::Tps,SensorKind::Map,SensorKind::Clt,SensorKind::Iat,SensorKind::Lambda,SensorKind::Battery}){SensorCalibrationWidget editor(kind);editor.findChild<QDoubleSpinBox*>("sensorVoltage0")->setValue(1.2345);const auto state=editor.editorState();QString error;SensorCalibrationWidget restored(kind);CHECK(restored.restoreEditor(state,true,error));CHECK(restored.editorState()==state);sensors[SensorCalibrationCodec::name(kind)]=state;
        restored.keepProjectEdits(true);QByteArray identity(32,0);identity[20]=42;restored.load(header,aux,identity,1);CHECK(restored.editorState()==state);}
    for(const auto &b:ParameterCatalog::blocks())if(b.key=="fuel.target"){ParameterWidget editor(b);auto old=editor.editorState();auto values=old["values"].toObject();values.remove("tableTarget");old["values"]=values;QString error;CHECK(editor.restoreEditor(old,true,error));CHECK(editor.editorState()["values"].toObject()["tableTarget"].toInt()==0);}
    TriggerWizard trigger;QString error;const auto triggerState=trigger.editorState();CHECK(trigger.restoreEditor(triggerState,true,error));CHECK(!trigger.reviewReady());
    QTableWidget table(16,16),rpm(1,16);for(int i=0;i<16;++i){rpm.setItem(0,i,new QTableWidgetItem(QString::number(i*500)));table.setVerticalHeaderItem(i,new QTableWidgetItem(QString::number(200-i*10)));for(int j=0;j<16;++j)table.setItem(i,j,new QTableWidgetItem(QString::number(30+i+j)));}
    const auto tableState=MapProject::tableState(&table,&rpm);CHECK(MapProject::tableState(&table,&rpm,tableState,0,300,false,error));QJsonObject data{{"target",QJsonObject{{"family",1},{"mcu",1},{"hardwareRevision",7},{"schema",2}}},{"pages",pages},{"parameters",parameters},{"sensors",sensors},{"trigger",triggerState},{"service",QJsonObject{{"comment","Complete map"}}},{"tables",QJsonObject{{"fuel",tableState},{"ignition",tableState},{"afr",tableState}}}};
    const auto bytes=MapProject::encode(data,error);CHECK(!bytes.isEmpty());QJsonObject decoded;CHECK(MapProject::decode(bytes,decoded,error)&&decoded==data);auto wrong=bytes;wrong[wrong.indexOf("Complete map")]='X';CHECK(!MapProject::decode(wrong,decoded,error));auto incomplete=data;incomplete.remove("parameters");CHECK(MapProject::encode(incomplete,error).isEmpty());auto badTarget=data;badTarget["target"]=QJsonObject{};CHECK(MapProject::encode(badTarget,error).isEmpty());CHECK(!MapProject::decode("{\"format\":\"tunuino-map-v1\"}",decoded,error));
    MapProject project(nullptr);bool can=true;project.canUse=[&]{return can;};project.capture=[&](QString &){return data;};int applications=0;project.apply=[&](const QJsonObject &value,QString &){++applications;CHECK(value==data);return true;};const auto path=temp.path()+"/whole.tnproj";CHECK(project.save(path,error));CHECK(project.open(path,error)&&applications==1);can=false;CHECK(!project.open(path,error)&&!project.save(path,error)&&applications==1);can=true;
    QFile file(path);CHECK(file.open(QIODevice::ReadOnly));CHECK(file.readAll()==bytes);file.close();CHECK(!project.save(temp.path()+"/absent/map.tnproj",error));CHECK(project.open(path,error));
    auto badTable=tableState;auto rows=badTable["cells"].toArray();rows[0]=QJsonArray{1};badTable["cells"]=rows;const auto before=MapProject::tableState(&table,&rpm);CHECK(!MapProject::tableState(&table,&rpm,badTable,0,300,true,error));CHECK(MapProject::tableState(&table,&rpm)==before);
    auto native=data;auto nativePages=native["pages"].toObject();nativePages["afr"]=QString::fromLatin1(QByteArray(576,0).toHex());native["pages"]=nativePages;QJsonObject nativeDecoded;CHECK(MapProject::decode(MapProject::encode(native,error),nativeDecoded,error));CHECK(nativeDecoded==native);
    nativePages["afr"]=QString::fromLatin1(QByteArray(576,1).toHex());native["pages"]=nativePages;CHECK(!MapProject::decode(MapProject::encode(native,error),nativeDecoded,error));
    std::cout<<"Whole-map checksum, four pages, tables, all parameter/sensor drafts, six-cylinder offline profile, no ECU access, atomic file save and invalid input checks passed\n";
}
