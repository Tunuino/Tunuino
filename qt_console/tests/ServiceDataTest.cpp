#include "tools/ServiceDataWidget.h"
#include <QApplication>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QTimer>
#include <QTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <iostream>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed: "<<#x<<" at "<<__LINE__<<std::endl;std::exit(1);}}while(false)
using namespace tunuino;
static ToolSnapshot base(){ToolSnapshot b;b.identity=QByteArray(32,0);b.identity.replace(1,7,"TUNUINO");b.identity[8]=1;b.identity[9]=1;b.identity[10]=1;b.identity[12]=7;b.identity[19]=63;b.identity[20]=42;b.generation=7;b.pages[0]=QByteArray(256,0);SensorCalibrationCodec::put(b.pages[0],0,2);MapPageValues m;for(int i=0;i<16;++i){m.rpm[i]=500*i;m.load[i]=20*i;}m.cells.fill(50);QString error;b.pages[1]=MapPageCodec::encode(2,m,error);b.pages[2]=MapPageCodec::encode(3,m,error);b.pages[3]=QByteArray(256,0);return b;}
int main(int argc,char **argv){QApplication app(argc,argv);CHECK(ServiceDataCodec::validVin("1HGBH41JXMN109186"));CHECK(!ServiceDataCodec::validVin("1HGBH41IXMN109186"));CHECK(!ServiceDataCodec::validComment(QString(65,'a')));CHECK(ServiceDataCodec::validComment(QString(64,QChar(0xe9))));CHECK(!ServiceDataCodec::validComment("line\nbreak"));
    CHECK(ServiceDataCodec::crc32("123456789")==0xcbf43926u);auto context=base();QByteArray current(512,0),frozen,header;current[0]=1;CHECK(ServiceDataCodec::valid(current));bool busy=false,dropAck=false,uncertain=false;int writes=0;quint32 token=0;
    ServiceDataWidget widget;widget.snapshot=[&]{return context;};widget.canStart=[&]{return !busy;};widget.busyChanged=[&](bool b){busy=b;};
    widget.exchange=[&](const QByteArray &q,std::function<void(const QByteArray&)> callback){CHECK(q.startsWith("TUNUINO:N"));QByteArray response;
        if(q[9]==0){frozen=current;header=QByteArray(21,0);header[1]=1;header[2]=uncertain?2:0;ServiceDataCodec::put32(header,4,ServiceDataCodec::u32(current,278));ServiceDataCodec::put32(header,8,context.generation);ServiceDataCodec::put32(header,12,++token);SensorCalibrationCodec::put(header,16,512);response=header;}
        else if(q[9]==1){const int at=SensorCalibrationCodec::u16(q,14),count=quint8(q[16]);response=header;SensorCalibrationCodec::put(response,18,at);response[20]=char(count);response+=frozen.mid(at,count);}
        else {CHECK(ServiceDataCodec::u32(q,10)==ServiceDataCodec::u32(current,278));++writes;
            if(q[9]==2){current[1]=q[14]?1:0;current.replace(2,17,QByteArray(17,0));if(q[14])current.replace(2,17,q.mid(15,17));}
            if(q[9]==3){const int n=SensorCalibrationCodec::u16(q,14);SensorCalibrationCodec::put(current,20,n);current.replace(22,256,QByteArray(256,0));current.replace(22,n,q.mid(16,n));}
            ServiceDataCodec::put32(current,278,ServiceDataCodec::u32(current,278)+1);response=QByteArray(5,0);ServiceDataCodec::put32(response,1,ServiceDataCodec::u32(current,278));if(dropAck){response.clear();dropAck=false;}}
        QTimer::singleShot(0,&widget,[callback,response]{callback(response);});return true;};
    widget.showPage(0);QTest::qWait(300);CHECK(!busy&&!widget.busy());auto *vin=widget.findChild<QLineEdit*>("vehicleVin"),*comment=widget.findChild<QLineEdit*>("projectComment");auto *save=widget.findChild<QPushButton*>("serviceSaveVin"),*read=widget.findChild<QPushButton*>("serviceRead");CHECK(vin&&comment&&save&&read&&save->isEnabled());
    QTest::keyClicks(comment,"Unsaved note");vin->setFocus();QTest::keyClicks(vin,"1HGBH41JXMN109186");save->click();QTest::qWait(350);CHECK(writes==1&&!busy&&vin->text()=="1HGBH41JXMN109186"&&comment->text()=="Unsaved note");CHECK(ServiceDataCodec::vin(current)==vin->text()&&ServiceDataCodec::comment(current).isEmpty());
    widget.findChild<QPushButton*>("serviceSaveComment")->click();QTest::qWait(350);CHECK(writes==2&&!busy&&ServiceDataCodec::comment(current)=="Unsaved note");
    const auto project=ServiceDataCodec::project(context,comment->text());const auto object=QJsonDocument::fromJson(project).object();CHECK(object["pages"].toArray().size()==4&&object["comment"]=="Unsaved note"&&object["sha256"].toString().size()==64&&object["crc32"].toString().size()==8);CHECK(!project.contains("1HGBH41JXMN109186")&&!object.contains("vin")&&!object.contains("service"));
    auto bad=context;bad.identity[9]=2;CHECK(ServiceDataCodec::project(bad,"test").isEmpty());
    vin->selectAll();QTest::keyClicks(vin,"WVWZZZ1JZXW000001");dropAck=true;save->click();QTest::qWait(300);CHECK(writes==3&&!save->isEnabled()&&!busy);QTest::qWait(300);CHECK(writes==3);read->click();QTest::qWait(300);CHECK(save->isEnabled()&&vin->text()=="WVWZZZ1JZXW000001");
    uncertain=true;read->click();QTest::qWait(300);CHECK(!save->isEnabled());uncertain=false;read->click();QTest::qWait(300);CHECK(save->isEnabled());
    auto corrupt=current;corrupt[332]=1;CHECK(!ServiceDataCodec::valid(corrupt));corrupt=current;SensorCalibrationCodec::put(corrupt,20,300);CHECK(!ServiceDataCodec::valid(corrupt));corrupt=current;corrupt[294]=1;ServiceDataCodec::put32(corrupt,282,20000001);CHECK(!ServiceDataCodec::valid(corrupt));
    context.identity[19]=31;widget.showPage(2);QTest::qWait(100);CHECK(!widget.busy());
    std::cout<<"Service UI snapshot/readback, independent fields, lost-ack handling, flash uncertainty and project VIN exclusion passed (synthetic transport)\n";
}
