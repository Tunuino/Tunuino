#include "parameters/ParameterWidget.h"
#include "ecu/MapWriteTransaction.h"
#include <QApplication>
#include <QProcess>
#include <QQueue>
#include <QSet>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTest>
#include <QFile>
#include <QToolButton>
#include <iostream>
#include <cstdlib>
#include <algorithm>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed: "<<#x<<" line "<<__LINE__<<std::endl;std::exit(1);}}while(false)
using namespace tunuino;
using Codec=SensorCalibrationCodec;using Writer=MapWriteTransaction;
static quint32 u32(const QByteArray &b,int at){return (quint32(Codec::u16(b,at))<<16)|quint32(Codec::u16(b,at+2));}
static QByteArray read(int page,int at,int count){QByteArray q(7,0);q[0]='r';Codec::put(q,1,page);Codec::put(q,3,at);Codec::put(q,5,count);return q;}
int main(int argc,char **argv){QApplication app(argc,argv);
    if(!qEnvironmentVariable("TUNUINO_PARAMETER_STYLE").isEmpty()){QFile css(qEnvironmentVariable("TUNUINO_PARAMETER_STYLE"));CHECK(css.open(QIODevice::ReadOnly));app.setStyleSheet(QString::fromUtf8(css.readAll()));}
    const auto blocks=ParameterCatalog::blocks();CHECK(blocks.size()>=18);QSet<QString> keys;
    for(const auto &b:blocks){CHECK(!keys.contains(b.key));keys.insert(b.key);ParameterWidget widget(b);CHECK(widget.findChild<QTableWidget*>("parameters")->columnCount()==2);CHECK(!widget.findChild<QPushButton*>("parameterSave")->isEnabled());const auto mask=ParameterCatalog::ownedMask(b);CHECK(mask.size()==256);if(b.page==1)CHECK(mask[102]==0);
        for(const auto &f:b.fields){CHECK(f.offset>=0&&f.offset+f.width<=256);QByteArray data(256,0);CHECK(ParameterCatalog::put(data,f,f.initial));if(b.key!="ignition.firing")CHECK(ParameterCatalog::value(data,f)==f.initial);CHECK(!ParameterCatalog::put(data,f,f.maximum+1));}
    }
    // The guard is editable offline, but older firmware must never receive its extension.
    const auto guard=*std::find_if(blocks.begin(),blocks.end(),[](const auto &b){return b.key=="fuel.guard";});
    ParameterWidget guardWidget(guard);QByteArray guardHeader(256,0),guardIdentity(32,0);guardIdentity[19]=15;
    auto guardAux=Codec::featureDefaults(Codec::tpsDefaults());Codec::put(guardAux,70,1024);
    guardWidget.load(guardHeader,guardAux,guardIdentity,1);guardWidget.setAvailable(true);bool guardCalled=false;
    guardWidget.writeRequested=[&](int page,const QByteArray &desired,const QByteArray &mask){guardCalled=true;CHECK(page==4&&desired[240]==1&&Codec::u16(desired,70)==1024);for(int j=0;j<256;++j)CHECK(!((quint8(desired[j])^quint8(guardAux[j]))&~quint8(mask[j])));};
    CHECK(!guardWidget.findChild<QPushButton*>("parameterSave")->isEnabled());guardWidget.save();CHECK(!guardCalled);
    guardIdentity[19]=79;guardWidget.load(guardHeader,guardAux,guardIdentity,1);CHECK(guardWidget.findChild<QPushButton*>("parameterSave")->isEnabled());guardWidget.save();CHECK(guardCalled);
    if(!qEnvironmentVariable("TUNUINO_GUARD_PREVIEW").isEmpty()){guardWidget.resize(guardWidget.sizeHint());guardWidget.show();app.processEvents();CHECK(guardWidget.grab().save(qEnvironmentVariable("TUNUINO_GUARD_PREVIEW")));guardWidget.hide();}
    const auto executable=qEnvironmentVariable("TUNUINO_FIRMWARE_HARNESS");if(executable.isEmpty()){std::cout<<"Parameter catalog/UI bounds passed; native firmware harness not supplied\n";return 0;}
    QProcess process;process.start(executable,{"--engine-profile"});CHECK(process.waitForStarted(3000));
    const auto exchange=[&](const QByteArray &q){const auto line=q.toHex()+"\n";CHECK(process.write(line)==line.size());CHECK(process.waitForBytesWritten(3000));while(!process.canReadLine())CHECK(process.waitForReadyRead(3000));return QByteArray::fromHex(process.readLine().trimmed());};
    const auto identity=exchange("TUNUINO?");quint32 generation=u32(exchange("TUNUINO:V"),8);int operations=0;
    for(const auto &b:blocks){Writer::Pages base;for(int p=1;p<=4;++p)for(int at=0;at<(p==1||p==4?256:576);at+=64)base[p-1]+=exchange(read(p,at,64)).mid(1);
        ParameterWidget widget(b);widget.load(base[0],base[3],identity,generation);widget.setAvailable(true);
        if(b.key=="ignition.firing"){auto *first=widget.findChild<QComboBox*>("order0"),*last=widget.findChild<QComboBox*>("order3");CHECK(first&&last);const auto selected=first->currentIndex();first->setCurrentIndex(last->currentIndex());last->setCurrentIndex(selected);CHECK(widget.findChild<QDoubleSpinBox*>("cylinders")->isEnabled());}
        else {bool changed=false;for(const auto &f:b.fields){if(f.bit||!f.choices.isEmpty())continue;auto *spin=widget.findChild<QDoubleSpinBox*>(f.key);if(spin&&spin->value()+1/f.scale<=f.maximum){spin->setValue(spin->value()+1/f.scale);changed=true;break;}}
            if(!changed){auto *combo=widget.findChild<QComboBox*>("cylinder0");CHECK(combo);combo->addItem("Chosen spare output",4);combo->setCurrentIndex(2);}}
        if(b.key=="ignition.firing"&&!qEnvironmentVariable("TUNUINO_PARAMETER_PREVIEW").isEmpty()){widget.resize(widget.sizeHint());widget.show();app.processEvents();CHECK(widget.grab().save(qEnvironmentVariable("TUNUINO_PARAMETER_PREVIEW")));widget.findChild<QToolButton*>("engineProfileToggle")->setChecked(true);widget.resize(720,680);app.processEvents();CHECK(widget.grab().save(qEnvironmentVariable("TUNUINO_PARAMETER_PREVIEW")+"-advanced.png"));widget.hide();}
        bool invoked=false;widget.writeRequested=[&](int page,const QByteArray &desired,const QByteArray &mask){invoked=true;CHECK(page==b.page&&desired!=base[page-1]);
            for(int i=0;i<256;++i)CHECK(!((quint8(desired[i])^quint8(base[page-1][i]))&~quint8(mask[i])));
            Writer writer;QQueue<QByteArray> pending;bool success=false;int copies=0,burns=0;
            writer.send=[&](const QByteArray &q,int){CHECK(!q.startsWith("ARM")&&!q.startsWith("RUN"));pending.enqueue(q);return true;};
            writer.savePrevious=[&](const QByteArray &id,quint32 gen,const Writer::Pages &actual){CHECK(id==identity&&gen==generation&&actual==base);++copies;return true;};
            writer.completed=[&](bool ok,bool uncertain,const QString &msg){if(!ok)std::cerr<<b.key.toStdString()<<": "<<msg.toStdString()<<std::endl;CHECK(!uncertain);success=ok;};
            CHECK(writer.startParameters(page,desired,base[page-1],identity,generation,mask));while(writer.busy()){CHECK(!pending.isEmpty());const auto q=pending.dequeue();if(q[0]=='b')++burns;writer.receive(exchange(q));}
            CHECK(success&&copies==1&&burns==1);generation=writer.generation();++operations;
            for(int p=1;p<=4;++p){QByteArray actual;for(int at=0;at<(p==1||p==4?256:576);at+=64)actual+=exchange(read(p,at,64)).mid(1);CHECK(actual==(p==page?desired:base[p-1]));}
            auto illegal=base[0];illegal[102]=1;CHECK(!writer.startParameters(1,illegal,base[0],identity,generation,QByteArray(256,char(255))));illegal=desired;int unowned=0;while(unowned<256&&mask[unowned]!=0)++unowned;CHECK(unowned<256);illegal[unowned]^=1;CHECK(!writer.startParameters(page,illegal,base[page-1],identity,generation,mask));
        };
        widget.save();CHECK(invoked);
    }
    CHECK(operations==blocks.size());process.closeWriteChannel();CHECK(process.waitForFinished(3000)&&process.exitCode()==0);
    std::cout<<operations<<" parameter block writes against actual C dispatcher: one Burn each, full-page preservation and firing order independent of output wiring passed (host simulation)\n";
}
