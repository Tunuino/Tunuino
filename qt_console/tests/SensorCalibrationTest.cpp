#include "calibration/SensorCalibrationWidget.h"
#include "calibration/SensorInputsWidget.h"
#include "calibration/SensorCurveWizard.h"
#include "ecu/MapWriteTransaction.h"
#include <QApplication>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QProcess>
#include <QQueue>
#include <QTest>
#include <iostream>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed: "<<#x<<" at "<<__LINE__<<std::endl;std::exit(1);}}while(false)
using Codec=tunuino::SensorCalibrationCodec;using Kind=tunuino::SensorKind;using Writer=tunuino::MapWriteTransaction;
static quint32 u32(const QByteArray &b,int at){return (quint32(Codec::u16(b,at))<<16)|quint32(Codec::u16(b,at+2));}
static void put32(QByteArray &b,int at,quint32 v){Codec::put(b,at,int(v>>16));Codec::put(b,at+2,int(v));}
static QByteArray read(int page,int at,int count){QByteArray q(7,0);q[0]='r';Codec::put(q,1,page);Codec::put(q,3,at);Codec::put(q,5,count);return q;}
static tunuino::SensorCalibration calibration(Kind kind){
    tunuino::SensorCalibration c;c.channel=kind==Kind::Lambda||kind==Kind::Iat?2:kind==Kind::Tps?3:kind==Kind::Battery?4:1;
    c.raw={0,2000,3977};c.values={120,50,-20};
    if(kind==Kind::Map){c.raw={0,3977,0};c.values={0,400,0};}
    if(kind==Kind::Lambda){c.raw={0,3977,0};c.values={.68,1.36,0};}
    if(kind==Kind::Battery){c.raw={300,3500,0};c.values={5,20,0};}
    if(kind==Kind::Tps)c.raw={300,3500,0};return c;
}
static void nativeFirmware(bool batteryFirst){
    const QString executable=qEnvironmentVariable("TUNUINO_FIRMWARE_HARNESS");if(executable.isEmpty())return;
    QProcess process;process.start(executable,QStringList{});CHECK(process.waitForStarted(3000));
    const auto exchange=[&](const QByteArray &q){const auto line=q.toHex()+"\n";CHECK(process.write(line)==line.size());CHECK(process.waitForBytesWritten(3000));while(!process.canReadLine())CHECK(process.waitForReadyRead(3000));return QByteArray::fromHex(process.readLine().trimmed());};
    const auto identity=exchange("TUNUINO?");quint32 generation=u32(exchange("TUNUINO:V"),8);int totalBurns=0;
    const std::array<Kind,6> order=batteryFirst?std::array<Kind,6>{Kind::Battery,Kind::Iat,Kind::Map,Kind::Clt,Kind::Lambda,Kind::Tps}:
        std::array<Kind,6>{Kind::Iat,Kind::Battery,Kind::Map,Kind::Clt,Kind::Lambda,Kind::Tps};
    for(const Kind kind:order){
        Writer::Pages base;
        for(int p=1;p<=4;++p)for(int at=0;at<(p==1||p==4?256:576);at+=64)base[size_t(p-1)]+=exchange(read(p,at,64)).mid(1);
        QString error;const auto desired=Codec::apply(kind,calibration(kind),base[0],base[3],error);CHECK(!desired.isEmpty());
        const int page=Codec::page(kind);CHECK(Codec::permitted(kind,base[size_t(page-1)],desired,base[0],base[3],error));
        Writer writer;QQueue<QByteArray> pending;bool success=false;int copies=0,burns=0;
        writer.send=[&](const QByteArray &q,int){pending.enqueue(q);return true;};
        writer.savePrevious=[&](const QByteArray &id,quint32 gen,const Writer::Pages &pages){CHECK(id==identity&&gen==generation&&pages==base);++copies;return true;};
        writer.completed=[&](bool ok,bool uncertain,const QString &message){if(!ok)std::cerr<<message.toStdString()<<std::endl;CHECK(!uncertain);success=ok;};
        CHECK(writer.startSensor(kind,desired,base[size_t(page-1)],identity,generation));
        while(writer.busy()){CHECK(!pending.isEmpty());const auto q=pending.dequeue();if(q[0]=='b')++burns;writer.receive(exchange(q));}
        CHECK(success&&copies==1&&burns==1&&writer.generation()==generation+1);generation=writer.generation();totalBurns+=burns;
        for(int p=1;p<=4;++p){QByteArray actual;for(int at=0;at<(p==1||p==4?256:576);at+=64)actual+=exchange(read(p,at,64)).mid(1);CHECK(actual==(p==page?desired:base[size_t(p-1)]));}
    }
    CHECK(totalBurns==6);process.closeWriteChannel();CHECK(process.waitForFinished(3000)&&process.exitCode()==0);
    std::cout<<"Six sensor writes against real firmware, all pages preserved: passed (host simulation)\n";
}
int main(int argc,char **argv){
    QApplication app(argc,argv);QByteArray header(256,char(0x55)),aux(256,0);Codec::put(header,0,2);Codec::put(header,100,3300);header[6]=1;header[7]=2;header[8]=1;
    QString error;
    for(const Kind kind:{Kind::Map,Kind::Clt,Kind::Lambda,Kind::Tps,Kind::Iat,Kind::Battery}){
        const auto c=calibration(kind);auto desired=Codec::apply(kind,c,header,aux,error);CHECK(!desired.isEmpty());
        CHECK(Codec::permitted(kind,Codec::auxiliary(kind)?aux:header,desired,header,aux,error));
        const auto roundTrip=Codec::decode(kind,Codec::auxiliary(kind)?header:desired,Codec::auxiliary(kind)?desired:aux);
        CHECK(roundTrip.channel==c.channel&&roundTrip.raw==c.raw);
        if(kind!=Kind::Tps)for(int i=0;i<(Codec::points(kind));++i)CHECK(roundTrip.values[i]==c.values[i]);
        desired[200]=char(quint8(desired[200])^1);CHECK(!Codec::permitted(kind,Codec::auxiliary(kind)?aux:header,desired,header,aux,error));
    }
    auto conflict=calibration(Kind::Lambda);conflict.channel=1;auto desired=Codec::apply(Kind::Lambda,conflict,header,aux,error);CHECK(!Codec::permitted(Kind::Lambda,header,desired,header,aux,error));
    auto temps=calibration(Kind::Clt);auto iat=aux;iat[68]=1;Codec::put(iat,70,1);iat[72]=1;
    desired=Codec::apply(Kind::Clt,temps,header,iat,error);CHECK(!Codec::permitted(Kind::Clt,header,desired,header,iat,error));
    auto reverse=calibration(Kind::Tps);std::swap(reverse.raw[0],reverse.raw[1]);CHECK(Codec::validate(Kind::Tps,reverse,error));
    auto tps=Codec::apply(Kind::Tps,reverse,header,aux,error);QByteArray live(101,0);live[40]=1;Codec::put(live,65,1900);double percent=0;CHECK(Codec::tpsPercent(tps,live,percent)&&percent==50);
    Codec::put(live,65,4095);CHECK(!Codec::tpsPercent(tps,live,percent));live[40]=0;CHECK(!Codec::tpsPercent(tps,live,percent));
    for(int raw:{0,300,1500,2048,3977,4095})CHECK(Codec::raw(Codec::voltage(raw,header),header)==raw);
    auto bad=calibration(Kind::Map);bad.raw[1]=0;CHECK(!Codec::validate(Kind::Map,bad,error));bad=calibration(Kind::Tps);bad.raw[1]=bad.raw[0]+20;CHECK(!Codec::validate(Kind::Tps,bad,error));
    header=QByteArray(256,0);Codec::put(header,0,2);Codec::put(header,100,3300);
    header=Codec::apply(Kind::Map,calibration(Kind::Map),header,aux,error);CHECK(!header.isEmpty());
    QByteArray identity(32,0);identity.replace(1,7,"TUNUINO");identity[8]=1;identity[9]=1;identity[10]=1;identity[12]=7;identity[19]=15;identity[20]=42;
    tunuino::SensorCalibrationWidget widget(Kind::Map);widget.load(header,aux,identity,42);widget.setAvailable(true);
    auto *capture=widget.findChild<QPushButton*>("sensorCapture0");auto *volts=widget.findChild<QDoubleSpinBox*>("sensorVoltage0");CHECK(capture&&!capture->isEnabled());
    live=QByteArray(101,0);live.replace(1,31,identity.mid(1));live[40]=1;put32(live,57,42);Codec::put(live,61,1500);
    widget.updateRaw(live);CHECK(capture->isEnabled());capture->click();CHECK(std::abs(volts->value()-Codec::voltage(1500,header))<.0001);
    int requested=0;widget.writeRequested=[&](Kind kind,const QByteArray &page){CHECK(kind==Kind::Map&&Codec::u16(page,10)==1500&&page[102]==header[102]);++requested;};widget.save();CHECK(requested==1);
    widget.load(header,aux,identity,43);CHECK(!capture->isEnabled());CHECK(std::abs(volts->value()-Codec::voltage(1500,header))<.0001); // Unsaved edit preserved, old generation rejected.
    put32(live,57,43);widget.updateRaw(live);CHECK(capture->isEnabled());
    live[40]=0;widget.updateRaw(live);CHECK(!capture->isEnabled());live[40]=1;widget.updateRaw(live);
    tunuino::SensorInputsWidget inputs(tunuino::SensorInputsWidget::Analog);inputs.load(header,aux,identity,43);inputs.setAvailable(true);inputs.updateRaw(live);auto *inputGrid=inputs.findChild<QTableWidget*>("sensorInputReadings");CHECK(inputGrid&&inputGrid->rowCount()==15);CHECK(inputGrid->item(0,1)->text().contains("13")&&inputGrid->item(0,2)->text()=="MAP");CHECK(inputGrid->item(0,3)->text()==QString::number(Codec::voltage(1500,header),'f',4)+" V");
    auto wrongSample=live;wrongSample[21]=char(quint8(wrongSample[21])+1);inputs.updateRaw(wrongSample);CHECK(inputGrid->item(0,3)->text()=="---");inputs.updateRaw(live);wrongSample=live;put32(wrongSample,57,44);inputs.updateRaw(wrongSample);CHECK(inputGrid->item(0,3)->text()=="---");inputs.updateRaw(live);
    QTest::qWait(2101);inputs.setAvailable(true);CHECK(inputGrid->item(0,3)->text()=="---");const double old=volts->value();capture->click();CHECK(volts->value()==old);
    auto other=identity;other[20]=43;widget.load(header,aux,other,43);CHECK(!capture->isEnabled());
    widget.setAvailable(false);CHECK(!widget.findChild<QPushButton*>("sensorSave")->isEnabled());
    // A stored channel is reserved even when its compensation is disabled.
    auto features=Codec::initialAux(Kind::Iat,aux);
    auto iatCalibration=calibration(Kind::Iat),batteryCalibration=calibration(Kind::Battery);
    features=Codec::apply(Kind::Iat,iatCalibration,header,features,error);
    features=Codec::apply(Kind::Battery,batteryCalibration,header,features,error);
    CHECK(Codec::u16(features,70)==0&&features[72]==2&&features[73]==4);
    auto occupied=calibration(Kind::Map);occupied.channel=4;
    auto changed=Codec::apply(Kind::Map,occupied,header,features,error);
    CHECK(!Codec::permitted(Kind::Map,header,changed,header,features,error));
    auto active=features;Codec::put(active,70,1|2|512);
    for(auto kind:{Kind::Iat,Kind::Battery}){
        auto disabled=Codec::decode(kind,header,active);disabled.channel=0;
        changed=Codec::apply(kind,disabled,header,active,error);
        CHECK(!Codec::permitted(kind,active,changed,header,active,error));
        auto edited=Codec::decode(kind,header,active);edited.values[0]+=.01;
        changed=Codec::apply(kind,edited,header,active,error);
        CHECK(Codec::permitted(kind,active,changed,header,active,error));
        CHECK(Codec::u16(changed,70)==(1|2|512));
        changed[70]=char(quint8(changed[70])^1);CHECK(!Codec::permitted(kind,active,changed,header,active,error));
    }
    live=QByteArray(101,0);live[40]=1;Codec::put(live,87,2000);Codec::put(live,67,1900);
    double measured=0;
    CHECK(Codec::auxiliaryReading(Kind::Iat,features,live,measured)&&measured==50);
    CHECK(Codec::auxiliaryReading(Kind::Battery,features,live,measured)&&measured==12.5);
    Codec::put(live,67,3501);CHECK(!Codec::auxiliaryReading(Kind::Battery,features,live,measured));
    live[40]=0;CHECK(!Codec::auxiliaryReading(Kind::Iat,features,live,measured));
    // Each auxiliary panel captures its own selected ADC channel, not another sensor.
    for(auto kind:{Kind::Iat,Kind::Battery}){
        tunuino::SensorCalibrationWidget panel(kind);panel.load(header,features,identity,42);panel.setAvailable(true);
        live=QByteArray(101,0);live.replace(1,31,identity.mid(1));live[40]=1;put32(live,57,42);
        const int offset=kind==Kind::Iat?87:67;Codec::put(live,offset,1500);panel.updateRaw(live);
        auto *button=panel.findChild<QPushButton*>("sensorCapture0");CHECK(button&&button->isEnabled());button->click();
        auto *field=panel.findChild<QDoubleSpinBox*>("sensorVoltage0");CHECK(Codec::raw(field->value(),header)==1500);
        int writes=0;panel.writeRequested=[&](Kind actual,const QByteArray &page){CHECK(actual==kind&&Codec::u16(page,kind==Kind::Iat?76:88)==1500);++writes;};
        panel.save();CHECK(writes==1);
        if(qEnvironmentVariableIsSet("TUNUINO_SENSOR_TEST_SCREENSHOT")){panel.resize(900,420);panel.show();app.processEvents();panel.grab().save(qEnvironmentVariable("TUNUINO_SENSOR_TEST_SCREENSHOT")+Codec::name(kind)+".png");}
    }
    QJsonObject curve{{"untouched",true}};const auto originalCurve=curve;
    CHECK(!tunuino::SensorCurveWizard::ntc(0,3950,2500,5,{0,25,100},curve,error));CHECK(curve==originalCurve);
    CHECK(tunuino::SensorCurveWizard::ntc(2500,3950,2500,5,{0,25,100},curve,error));CHECK(curve["volts"].toArray()[1].toDouble()==2.5);CHECK(curve["values"].toArray()==QJsonArray({100,25,0}));CHECK(curve["volts"].toArray()[0].toDouble()<2.5&&curve["volts"].toArray()[2].toDouble()>2.5);
    CHECK(!tunuino::SensorCurveWizard::ntc(2500,3950,2500,5,{25,25,100},curve,error));CHECK(!tunuino::SensorCurveWizard::linear(4.5,0,.5,300,curve,error));CHECK(tunuino::SensorCurveWizard::linear(.5,0,4.5,300,curve,error));CHECK(curve["volts"].toArray()==QJsonArray({.5,4.5}));
    tunuino::SensorInputsWidget digital(tunuino::SensorInputsWidget::Cam1);digital.load(header,aux,identity,43);digital.setAvailable(true);put32(live,57,43);live[40]=9;digital.updateRaw(live);auto *digitalGrid=digital.findChild<QTableWidget*>("sensorInputReadings");CHECK(digitalGrid->item(0,1)->text().contains("4 (+), 12 (-)"));CHECK(digitalGrid->item(3,1)->text()=="Synchronized");digital.setAvailable(false);CHECK(digitalGrid->item(3,1)->text()=="---");
    tunuino::SensorInputsWidget temperatures(tunuino::SensorInputsWidget::Temperature);temperatures.load(header,features,identity,43);temperatures.setAvailable(true);Codec::put(live,87,1500);temperatures.updateRaw(live);auto *thermalGrid=temperatures.findChild<QTableWidget*>("sensorInputReadings");CHECK(thermalGrid->item(0,1)->text().contains("30 (AT2)"));CHECK(thermalGrid->item(0,3)->text()==QString::number(Codec::voltage(1500,header),'f',4)+" V");
    nativeFirmware(false);nativeFirmware(true);std::cout<<"Sensor encoding, input conflicts, reserved settings, reverse TPS, real ADC capture and stale samples: passed\n";
}
