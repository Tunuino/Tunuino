#include "MapProject.h"
#include "../dbw/DbwSignals.h"
#include "../tools/ServiceDataCodec.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QSaveFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QMessageBox>
#include <QApplication>
#include <cmath>
namespace tunuino {
namespace {
QByteArray content(const QJsonObject &o){return QJsonDocument(o).toJson(QJsonDocument::Compact);}
QString crc(const QByteArray &b){return QStringLiteral("%1").arg(ServiceDataCodec::crc32(b),8,16,QLatin1Char('0'));}
}
MapProject::MapProject(QWidget *window):QObject(window),window_(window){setObjectName("mapProject");}
QJsonObject MapProject::vvtTableState(const QByteArray &page){
    QJsonArray rpm,load,cells;const bool stored=MapPageCodec::valid(7,page)&&page!=QByteArray(576,0);
    for(int i=0;i<16;++i){rpm.append(stored?int(SensorCalibrationCodec::u16(page,2*i)):500+500*i);load.append(stored?SensorCalibrationCodec::u16(page,32+2*(15-i))/100.:170-10*i);QJsonArray row;for(int c=0;c<16;++c)row.append(stored?qint16(SensorCalibrationCodec::u16(page,64+2*((15-i)*16+c)))/100.:0.);cells.append(row);}return {{"rpm",rpm},{"load",load},{"cells",cells}};
}
QJsonObject MapProject::defaultPages(){
    QByteArray h(256,0);SensorCalibrationCodec::put(h,0,2);h[3]=36;h[4]=1;h[26]=3;
    for(int at:{12,20})SensorCalibrationCodec::put(h,at,4095);SensorCalibrationCodec::put(h,34,2048);SensorCalibrationCodec::put(h,36,4095);SensorCalibrationCodec::put(h,96,12000);SensorCalibrationCodec::put(h,100,3300);
    MapPageValues v;for(int i=0;i<16;++i){v.rpm[i]=500+500*i;v.load[i]=20+10*i;}QString error;
    return {{"header",QString::fromLatin1(h.toHex())},{"fuel",QString::fromLatin1(MapPageCodec::encode(2,v,error).toHex())},{"ignition",QString::fromLatin1(MapPageCodec::encode(3,v,error).toHex())},{"aux",QString::fromLatin1(QByteArray(256,0).toHex())}};
}
QByteArray MapProject::encode(const QJsonObject &payload,QString &error){
    const auto bytes=content(payload);QJsonObject envelope{{"format","tunuino-project-v2"},{"payload",payload},{"crc32",crc(bytes)},{"sha256",QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex())}};
    const auto encoded=QJsonDocument(envelope).toJson(QJsonDocument::Indented);QJsonObject checked;if(!decode(encoded,checked,error))return {};return encoded;
}
bool MapProject::decode(const QByteArray &bytes,QJsonObject &payload,QString &error){
    error.clear();const auto fail=[&](const char *message){error=message;return false;};if(bytes.size()>2*1024*1024)return fail("Project file is too large.");
    QJsonParseError parse;const auto doc=QJsonDocument::fromJson(bytes,&parse);if(parse.error!=QJsonParseError::NoError||!doc.isObject())return fail("Invalid project file.");const auto o=doc.object();
    if(o["format"]!="tunuino-project-v2"||!o["payload"].isObject())return fail("This is not a complete Tunuino project. Use the table toolbar to import individual tables.");
    const auto p=o["payload"].toObject();const auto raw=content(p);
    if(o["crc32"]!=crc(raw)||o["sha256"]!=QString::fromLatin1(QCryptographicHash::hash(raw,QCryptographicHash::Sha256).toHex()))return fail("Project checksum does not match; nothing was loaded.");
    if(p["target"].toObject()!=QJsonObject{{"family",1},{"mcu",1},{"hardwareRevision",7},{"schema",2}})return fail("Unsupported ECU target or configuration format.");
    const auto pages=p["pages"].toObject();QStringList names{"header","fuel","ignition","aux"};if(pages.contains("afr"))names<<"afr";if(pages.contains("controls")){if(!pages.contains("afr"))return fail("VTEC requires the complete AFR and control page set.");names<<"controls";}if(pages.contains("vvt")){if(!pages.contains("controls"))return fail("VVT table requires the complete control page set.");names<<"vvt";}if(pages.contains("vvt2")){if(!pages.contains("vvt"))return fail("Second CAM table requires angle 1 and complete pages.");names<<"vvt2";}if(pages.size()!=names.size())return fail("Project must contain all four configuration pages.");
    for(int i=0;i<names.size();++i){const auto text=pages[names[i]].toString();const auto b=QByteArray::fromHex(text.toLatin1());if(text!=QString::fromLatin1(b.toHex())||b.size()!=(i==0||i==3||i==5?256:576)||(i==0&&SensorCalibrationCodec::u16(b,0)!=2)||(i==1&&!MapPageCodec::valid(2,b))||(i==2&&!MapPageCodec::valid(3,b))||(i==4&&!MapPageCodec::valid(5,b))||(i==5&&!VtecCodec::format(b))||(i==6&&!MapPageCodec::valid(7,b))||(i==7&&!MapPageCodec::valid(8,b)))return fail("Invalid project configuration page.");}
    for(const auto &key:{"tables","parameters","sensors","trigger","service"})if(!p[key].isObject())return fail("Incomplete project editor data.");
    if(p.contains("dbw")){DbwSignals dbw;if(!p["dbw"].isObject()||!dbw.restore(p["dbw"].toObject(),false,error))return false;}
    const auto tables=p["tables"].toObject();if(!tables.contains("fuel")||!tables.contains("ignition")||!tables.contains("afr")||tables.size()<(3)||tables.size()>5||(tables.size()>=4&&!tables.contains("vvt"))||(tables.size()==5&&!tables.contains("vvt2")))return fail("Fuel, Ignition and AFR Target tables are required; VVT is optional for legacy projects.");payload=p;return true;
}
bool MapProject::save(const QString &path,QString &error){
    if(canUse&&!canUse()){error="Wait for the active ECU operation to finish.";return false;}if(!capture){error="Project capture is unavailable.";return false;}
    if(auto *focus=QApplication::focusWidget())focus->clearFocus();
    const auto p=capture(error);if(p.isEmpty())return false;const auto bytes=encode(p,error);if(bytes.isEmpty())return false;
    QSaveFile file(path);if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit()){error="Could not save the complete project.";return false;}path_=path;QSettings().setValue("maps/directory",QFileInfo(path).absolutePath());return true;
}
bool MapProject::open(const QString &path,QString &error){
    if(canUse&&!canUse()){error="Wait for the active ECU operation to finish.";return false;}QFile file(path);if(!file.open(QIODevice::ReadOnly)||file.size()>2*1024*1024){error="Could not read the project file.";return false;}QJsonObject p;if(!decode(file.readAll(),p,error)||!apply||!apply(p,error))return false;path_=path;QSettings().setValue("maps/directory",QFileInfo(path).absolutePath());return true;
}
QString MapProject::defaultDirectory(){const auto saved=QSettings().value("maps/directory").toString();if(!saved.isEmpty()&&QDir(saved).exists())return saved;const auto dir=QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)+"/Tunuino/Maps";QDir().mkpath(dir);return dir;}
void MapProject::saveAs(){QFileDialog dialog(window_,tr("Save Project"),path_.isEmpty()?defaultDirectory()+"/Untitled.tnproj":path_,tr("Tunuino project (*.tnproj)"));dialog.setAcceptMode(QFileDialog::AcceptSave);dialog.setDefaultSuffix("tnproj");if(dialog.exec()!=QDialog::Accepted)return;QString error;if(!save(dialog.selectedFiles().value(0),error))QMessageBox::warning(window_,tr("Save Project"),error);}
void MapProject::openDialog(){const auto path=QFileDialog::getOpenFileName(window_,tr("Open Project"),defaultDirectory(),tr("Tunuino project (*.tnproj)"));if(path.isEmpty())return;QString error;if(!open(path,error))QMessageBox::warning(window_,tr("Open Project"),error);}
QJsonObject MapProject::tableState(QTableWidget *t,QTableWidget *rpm){QJsonArray x,y,rows;for(int i=0;i<16;++i){bool ok;double n=rpm->item(0,i)->text().toDouble(&ok);x.append(ok?QJsonValue(n):QJsonValue());n=t->verticalHeaderItem(i)->text().toDouble(&ok);y.append(ok?QJsonValue(n):QJsonValue());QJsonArray row;for(int j=0;j<16;++j){n=t->item(i,j)->text().toDouble(&ok);row.append(ok?QJsonValue(n):QJsonValue());}rows.append(row);}return {{"rpm",x},{"load",y},{"cells",rows}};}
bool MapProject::tableState(QTableWidget *t,QTableWidget *rpm,const QJsonObject &o,double low,double high,bool apply,QString &error){
    const auto x=o["rpm"].toArray(),y=o["load"].toArray(),rows=o["cells"].toArray();const auto number=[](const QJsonValue &v,double a,double b){return v.isDouble()&&std::isfinite(v.toDouble())&&v.toDouble()>=a&&v.toDouble()<=b;};
    bool valid=x.size()==16&&y.size()==16&&rows.size()==16;for(int i=0;valid&&i<16;++i){valid=number(x[i],0,20000)&&number(y[i],0,500)&&rows[i].isArray()&&rows[i].toArray().size()==16;for(int j=0;valid&&j<16;++j)valid=number(rows[i].toArray()[j],low,high);}
    if(!valid){error="Invalid table axes, dimensions or values. Nothing was loaded.";return false;}if(!apply)return true;
    for(int i=0;i<16;++i){rpm->item(0,i)->setText(QString::number(x[i].toDouble(),'g',12));t->setHorizontalHeaderItem(i,new QTableWidgetItem(QString::number(x[i].toDouble(),'g',12)));t->setVerticalHeaderItem(i,new QTableWidgetItem(QString::number(y[i].toDouble(),'g',12)));for(int j=0;j<16;++j)t->item(i,j)->setText(QString::number(rows[i].toArray()[j].toDouble(),'g',12));}return true;
}
}
