#include "dbw/DbwWidget.h"
#include <QApplication>
#include <QTest>
#include <iostream>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed: "<<#x<<" at "<<__LINE__<<std::endl;std::exit(1);}}while(false)
using namespace tunuino;
using Codec=SensorCalibrationCodec;
static void put32(QByteArray &b,int at,quint32 n){Codec::put(b,at,int(n>>16));Codec::put(b,at+2,int(n));}
static ToolSnapshot snapshot(){
    ToolSnapshot s;s.identity=QByteArray(32,0);s.identity[19]=8;s.generation=7;
    s.pages[0]=QByteArray(256,0);Codec::put(s.pages[0],100,3300);s.pages[3]=QByteArray(256,0);
    MapPageValues v;for(int i=0;i<16;++i){v.rpm[i]=500+500*i;v.load[i]=20+10*i;}QString error;
    s.pages[1]=MapPageCodec::encode(2,v,error);s.pages[2]=MapPageCodec::encode(3,v,error);return s;
}
static QByteArray sample(const ToolSnapshot &s,int a=800,int b=3200){QByteArray body(101,0);body.replace(1,31,s.identity.mid(1,31));body[40]=5;put32(body,57,s.generation);for(int i=0;i<4;++i)Codec::put(body,61+2*i,i%2?b:a);return body;}
int main(int argc,char **argv){
    QApplication app(argc,argv);auto model=std::make_shared<DbwSignals>();auto base=snapshot();CHECK(base.valid());model->available=true;
    for(int i=0;i<4;++i)model->tracks[i].channel=i+1;
    QString error;CHECK(model->assignmentError().isEmpty());CHECK(!model->capture(0,false,error));
    model->observe(base,sample(base));CHECK(model->fresh()&&model->stationary());CHECK(model->capture(0,false,error));CHECK(!model->calibrated(0)&&!model->calibrated(1));CHECK(model->capture(1,false,error));
    model->observe(base,sample(base,3200,800));CHECK(model->capture(0,true,error));CHECK(model->capture(1,true,error));
    CHECK(model->calibrated(0)&&model->calibrated(1));CHECK(model->tracks[1].full<model->tracks[1].rest);
    model->observe(base,sample(base,2000,2000));double p=0;CHECK(model->position(0,p)&&std::abs(p-50)<.001);CHECK(model->position(1,p)&&std::abs(p-50)<.001);CHECK(model->diagnostic(0).contains("agree"));
    model->observe(base,sample(base,2600,2000));CHECK(model->diagnostic(0).contains("disagreement"));
    model->observe(base,sample(base,0,2000));CHECK(!model->position(0,p));CHECK(model->diagnostic(0).contains("invalid"));CHECK(!model->capture(0,false,error));
    model->tracks[1].channel=1;CHECK(!model->assignmentError().isEmpty());CHECK(!model->capture(0,true,error));model->tracks[1].channel=2;
    base.pages[0][6]=1;model->observe(base,sample(base));CHECK(model->assignmentError().contains("MAP"));base.pages[0][6]=0;
    base.pages[3][0]=1;base.pages[3][5]=3;model->observe(base,sample(base));CHECK(model->assignmentError().isEmpty());base.pages[3][5]=2;model->observe(base,sample(base));CHECK(!model->assignmentError().isEmpty());base.pages[3]=QByteArray(256,0);
    const auto saved=model->state();DbwSignals restored;CHECK(restored.restore(saved,true,error)&&restored.state()==saved);auto malformed=saved;malformed["disagreement"]=0;CHECK(!restored.restore(malformed,true,error)&&restored.state()==saved);
    auto rows=saved["signals"].toArray();auto row=rows[0].toObject();row["input"]=1.5;rows[0]=row;malformed=saved;malformed["signals"]=rows;CHECK(!restored.restore(malformed,true,error));
    for(int offset:{32,41,97}){auto body=sample(base);body[offset]=1;model->observe(base,body);CHECK(!model->stationary()&&!model->capture(0,false,error));}
    auto armed=sample(base);armed[32]=16;model->observe(base,armed);CHECK(!model->capture(0,false,error));
    auto body=sample(base);body[40]=7;model->observe(base,body);CHECK(!model->fresh());
    body=sample(base);put32(body,57,8);model->observe(base,body);CHECK(!model->fresh());
    body=sample(base);body[20]=1;model->observe(base,body);CHECK(!model->fresh());
    ++base.generation;model->observe(base,sample(base));CHECK(!model->capture(0,true,error));CHECK(error.contains("0% first"));
    model->observe(base,sample(base));CHECK(model->fresh());QTest::qWait(1550);CHECK(!model->fresh());model->available=false;CHECK(!model->capture(0,true,error));
    DbwWidget setup(model),monitor(model,true);setup.show();monitor.show();app.processEvents();CHECK(setup.findChild<QAction*>("dbwCaptureRest")&&!setup.findChild<QAction*>("dbwCaptureRest")->isEnabled());
    if(argc>1){setup.resize(setup.sizeHint());monitor.resize(monitor.sizeHint());setup.grab().save(QString::fromLocal8Bit(argv[1])+"-setup.png");monitor.grab().save(QString::fromLocal8Bit(argv[1])+"-monitor.png");}
    std::cout<<"DBW signal commissioning tests passed (simulated ADC; no motor control).\n";
}
