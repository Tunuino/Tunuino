#include "tools/ToolsSuite.h"
#include "tools/ScopeWidget.h"
#include "tools/OutputTestWidget.h"
#include "tools/AutotuneWidget.h"
#include <QApplication>
#include <QMainWindow>
#include <QTreeWidget>
#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QPushButton>
#include <QMessageBox>
#include <QTimer>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QLabel>
#include <QFile>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QDialog>
#include <QTableWidget>
#include <QSplitter>
#include <iostream>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed: "<<#x<<" at "<<__LINE__<<std::endl;std::exit(1);}}while(false)
using namespace tunuino;
static void put32(QByteArray &b,int i,quint32 n){SensorCalibrationCodec::put(b,i,n>>16);SensorCalibrationCodec::put(b,i+2,n);}
static ToolSnapshot base(){ToolSnapshot s;s.identity=QByteArray(32,0);s.identity.replace(1,7,"TUNUINO");s.identity[8]=1;s.identity[9]=1;s.identity[10]=1;s.identity[12]=7;s.identity[19]=31;s.identity[20]=42;s.generation=7;s.pages[0]=QByteArray(256,0);SensorCalibrationCodec::put(s.pages[0],0,2);s.pages[0][6]=1;s.pages[0][7]=2;s.pages[0][8]=1;
    MapPageValues m;for(int i=0;i<16;++i){m.rpm[i]=i*500;m.load[i]=i*20;}m.cells.fill(50);QString error;s.pages[1]=MapPageCodec::encode(2,m,error);s.pages[2]=MapPageCodec::encode(3,m,error);s.pages[3]=SensorCalibrationCodec::initialAux(SensorKind::Iat,QByteArray(256,0));return s;}
static QPushButton *button(QWidget &w,const QString &name){for(auto *b:w.findChildren<QPushButton*>())if(b->text()==name)return b;std::cerr<<"Missing button "<<name.toStdString()<<std::endl;std::exit(1);}
int main(int argc,char **argv){QApplication app(argc,argv);QTemporaryDir prefs;CHECK(prefs.isValid());app.setOrganizationName("TunuinoTests");app.setApplicationName("ToolsTest");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,prefs.path());
    if(!qEnvironmentVariable("TUNUINO_MENU_STYLE").isEmpty()){QFile css(qEnvironmentVariable("TUNUINO_MENU_STYLE"));CHECK(css.open(QIODevice::ReadOnly));app.setStyleSheet(QString::fromUtf8(css.readAll()));}
    auto snapshot=base();CHECK(snapshot.valid());auto inputs=inputAssignments(snapshot);CHECK(inputs.size()==18&&inputs[0].functions==QStringList{"MAP"}&&inputs[12].functions==QStringList{"CLT"});CHECK(inputs[0].contact=="J103 / 13");
    snapshot.pages[0][7]=1;inputs=inputAssignments(snapshot);CHECK(inputs[0].conflict);snapshot=base();
    auto &h=snapshot.pages[0];h[128]=3;h[129]=2;h[160]=0;h[161]=0;h[168]=1;h[169]=0;put32(h,170,36000);
    auto outputs=outputAssignments(snapshot);CHECK(outputs.size()==28&&outputs[0].functions==QStringList{"Injector 1"}&&outputs[16].functions.size()==1&&!outputs[16].conflict&&outputs[16].contact=="J102 / 35");h[150]=1;outputs=outputAssignments(snapshot);CHECK(outputs[0].conflict);snapshot=base();
    QMainWindow main;auto *navigation=new QTreeWidget(&main);navigation->setColumnCount(1);new QTreeWidgetItem(navigation,{"Fueling"});new QTreeWidgetItem(navigation,{"Log"});auto *menu=main.menuBar()->addMenu("Tools");auto *graph=menu->addAction("Graph Log"),*tune=menu->addAction("Tune Display");ToolsSuite suite(&main,navigation);suite.snapshot=[&]{return snapshot;};suite.populate(menu,graph,tune);
    QStringList names;for(auto *a:menu->actions())if(!a->isSeparator())names<<a->text();CHECK(names==QStringList({"Device password","DBW calibration tool","Calibrate WBO circuit","Test outputs","Diff. oil bleed","Autotune","Logbook","User defined CAN stream","Set EDL-1 datalogger time","Show assigned outputs","Show assigned inputs","User names...","Scope","Graph Log","Tune Display","Customize keys","General options","Available strategies","Decode VW coding...","Vehicle VIN...","Project comment..."}));
    CHECK(menu->actions()[0]->isEnabled()==false&&!menu->actions()[0]->toolTip().isEmpty());
    snapshot.pages[0][2]=1;
    ScopeWidget scope;scope.snapshot=[&]{return snapshot;};bool scoped=false;scope.ready=[]{return true;};scope.busyChanged=[&](bool b){scoped=b;};QByteArray header(21,0);header[1]=1;header[2]=1;SensorCalibrationCodec::put(header,3,4);put32(header,5,7);put32(header,9,42);
    scope.exchange=[&](const QByteArray &q,std::function<void(const QByteArray&)> reply){CHECK(q.startsWith("TUNUINO:S"));QByteArray b=header;if(q.size()>10){b.resize(41);for(int i=0;i<4;++i){put32(b,21+5*i,i*1000);b[25+5*i]=i%2?2:1;}}QTimer::singleShot(0,&scope,[reply,b]{reply(b);});return true;};button(scope,"Capture")->click();CHECK(scoped);QTest::qWait(300);CHECK(!scoped&&!scope.busy());
    const auto completeSnapshot=snapshot;snapshot.pages[1].clear();snapshot.pages[2].clear();CHECK(!snapshot.valid()&&snapshot.diagnosticValid());button(scope,"Capture")->click();QTest::qWait(200);CHECK(!scope.busy());snapshot=completeSnapshot;
    auto emptyHeader=header;emptyHeader[2]=0;SensorCalibrationCodec::put(emptyHeader,3,0);scope.exchange=[&](const QByteArray &,std::function<void(const QByteArray&)> reply){QTimer::singleShot(0,&scope,[reply,emptyHeader]{reply(emptyHeader);});return true;};button(scope,"Capture")->click();QTest::qWait(100);CHECK(!scope.busy()&&scope.findChild<QLabel*>("triggerScopeStatus")->text().contains("No crank/cam edges"));
    scope.exchange=[&](const QByteArray &q,std::function<void(const QByteArray&)> reply){QByteArray b=header;if(q.size()>10){b.resize(41);for(int i=0;i<4;++i){put32(b,21+5*i,i*1000);b[25+5*i]=i%2?2:1;}}QTimer::singleShot(0,&scope,[reply,b]{reply(b);});return true;};
    put32(header,5,8);button(scope,"Capture")->click();QTest::qWait(100);CHECK(!scope.busy());bool badGeneration=false;for(auto *label:scope.findChildren<QLabel*>())badGeneration|=label->text().contains("Invalid capture header");CHECK(badGeneration);put32(header,5,7);
    // Output test validates identity, stopped runtime, supply, generation and idle outputs before every pulse.
    OutputTestWidget output;output.snapshot=[&]{return snapshot;};output.canStart=[]{return true;};int pulses=0;
    output.exchange=[&](const QByteArray &q,std::function<void(const QByteArray&)> reply){QByteArray b;
        if(q=="TUNUINO?")b=snapshot.identity;
        else if(q[0]=='O'){b=QByteArray(101,0);b.replace(1,31,snapshot.identity.mid(1));b[40]=1;put32(b,57,7);SensorCalibrationCodec::put(b,83,1619);}
        else if(q=="TUNUINO:V"){b=QByteArray(16,0);b[1]=1;b[3]=1;put32(b,8,7);}
        else if(q==QByteArray("TUNUINO:X")+char(0)){b=QByteArray(24,0);b[1]=1;b[14]=1;}
        else {CHECK(q.size()==17&&q.startsWith("TUNUINO:X")&&q[9]==4&&q.mid(13)=="TEST");++pulses;b=QByteArray(1,0);}
        QTimer::singleShot(0,&output,[reply,b]{reply(b);});return true;};
    QTimer::singleShot(0,&output,[]{auto *dialog=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());CHECK(dialog);dialog->button(QMessageBox::Yes)->click();});button(output,"Test")->click();QTest::qWait(1000);CHECK(pulses==1&&!output.busy());
    // No pulse is sent if the runtime reports a running engine.
    output.exchange=[&](const QByteArray &q,std::function<void(const QByteArray&)> reply){QByteArray b=q=="TUNUINO?"?snapshot.identity:QByteArray(101,0);if(q[0]=='O'){b.replace(1,31,snapshot.identity.mid(1));b[39]=1;b[40]=1;put32(b,41,1000000);put32(b,57,7);SensorCalibrationCodec::put(b,83,1619);}QTimer::singleShot(0,&output,[reply,b]{reply(b);});return true;};
    QTimer::singleShot(0,&output,[]{qobject_cast<QMessageBox*>(QApplication::activeModalWidget())->button(QMessageBox::Yes)->click();});button(output,"Test")->click();QTest::qWait(300);CHECK(!output.busy()&&pulses==1);
    if(!qEnvironmentVariable("TUNUINO_AUTOTUNE_PREVIEW").isEmpty())QFile::remove(qEnvironmentVariable("TUNUINO_AUTOTUNE_PREVIEW")+"-setup.png");
    AutotuneWidget autotune;autotune.resize(1100,600);autotune.show();
    const auto setup=[&](bool accept,int delay){QTimer::singleShot(0,&autotune,[&,accept,delay]{auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());CHECK(dialog&&dialog->objectName()=="autotuneSetup");CHECK(dialog->findChild<QTableWidget*>("autotuneSetupFields")->rowCount()==8);CHECK(!dialog->findChild<QDoubleSpinBox*>("veTable")->isEnabled());if(!qEnvironmentVariable("TUNUINO_AUTOTUNE_PREVIEW").isEmpty()&&!QFile::exists(qEnvironmentVariable("TUNUINO_AUTOTUNE_PREVIEW")+"-setup.png")){CHECK(dialog->grab().save(qEnvironmentVariable("TUNUINO_AUTOTUNE_PREVIEW")+"-setup.png"));}dialog->findChild<QDoubleSpinBox*>("delay")->setValue(delay);dialog->findChild<QDialogButtonBox*>()->button(accept?QDialogButtonBox::Ok:QDialogButtonBox::Cancel)->click();});autotune.configure();};
    setup(false,900);CHECK(!QSettings().contains("autotune/delay"));setup(true,2000);CHECK(QSettings().value("autotune/delay").toInt()==2000);
    auto *review=autotune.findChild<QTableWidget*>("autotuneTable");CHECK(review&&review->rowCount()==16&&review->item(0,0)->data(Qt::UserRole).toBool());CHECK(review->item(0,0)->text()=="40.0");
    auto *split=autotune.findChild<QSplitter*>("autotuneSplitter");CHECK(split&&split->orientation()==Qt::Horizontal);autotune.findChild<QAction*>("autotuneTableOnly")->trigger();CHECK(autotune.findChild<QWidget*>("autotuneSurface")->isHidden());autotune.findChild<QAction*>("autotuneSplitHorizontal")->trigger();CHECK(!autotune.findChild<QWidget*>("autotuneSurface")->isHidden());
autotune.snapshot=[&]{return snapshot;};autotune.canStart=[]{return true;};SensorCalibrationCodec::put(snapshot.pages[3],70,4);autotune.findChild<QAction*>("autotuneStart")->trigger();CHECK(autotune.collecting());
    std::array<double,7> values={2500,25,80,1.1,85,30,13.8};autotune.sample(values,1);QTest::qWait(2050);for(int i=0;i<6;++i){QTest::qWait(120);autotune.sample(values,1);} // Gap above 2s must reject transient/stale samples.
    CHECK(autotune.proposal().isEmpty());QTest::qWait(1000);autotune.sample(values,1);QTest::qWait(1050);for(int i=0;i<6;++i){QTest::qWait(120);autotune.sample(values,1);}
    autotune.findChild<QAction*>("autotuneStop")->trigger();CHECK(!autotune.collecting()&&!autotune.proposal().isEmpty());CHECK(SensorCalibrationCodec::u16(autotune.proposal(),64+2*(4*16+5))==5250);
    CHECK(review->horizontalHeaderItem(5)->text()=="2500"&&review->verticalHeaderItem(11)->text()=="80");
    CHECK(review->verticalHeaderItem(0)->text()=="300"&&review->verticalHeaderItem(15)->text()=="0");
    CHECK(review->item(10,5)->data(Qt::UserRole).toBool());
    CHECK(!review->item(11,5)->data(Qt::UserRole).toBool());
    autotune.findChild<QAction*>("autotuneMode1")->trigger();CHECK(autotune.windowTitle()=="Autotune - ACTUAL LAMBDA"&&review->item(11,5)->text()=="1.100"&&review->item(0,0)->text()==QString::fromUtf8("—"));
    autotune.findChild<QAction*>("autotuneMode2")->trigger();CHECK(review->item(11,5)->text().toUInt()>=5);
    autotune.findChild<QAction*>("autotuneMode3")->trigger();CHECK(review->item(11,5)->text()=="0.100");
    autotune.findChild<QAction*>("autotuneMode4")->trigger();CHECK(review->item(11,5)->text()=="2.5");
    autotune.findChild<QAction*>("autotuneMode0")->trigger();review->setCurrentCell(11,5);review->item(11,5)->setSelected(true);CHECK(autotune.findChild<QAction*>("autotuneApply100")->isEnabled());
    QByteArray local=snapshot.pages[1];int applications=0;autotune.applyToEditor=[&](const ToolSnapshot &b,const QByteArray &expected,const QByteArray &desired){CHECK(b.pages==snapshot.pages);if(expected!=local)return false;local=desired;++applications;return true;};
    const auto applySelected=[&](const char *name,bool accept){QTimer::singleShot(0,&autotune,[accept]{auto *dialog=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());CHECK(dialog);dialog->button(accept?QMessageBox::Yes:QMessageBox::Cancel)->click();});autotune.findChild<QAction*>(name)->trigger();};
    applySelected("autotuneApply50",false);CHECK(applications==0&&local==snapshot.pages[1]);applySelected("autotuneApply50",true);CHECK(applications==1&&SensorCalibrationCodec::u16(local,64+2*69)==5125);auto halfApplied=local;SensorCalibrationCodec::put(local,64,4999);auto editedLocal=local;applySelected("autotuneApply100",true);CHECK(applications==1&&local==editedLocal);local=halfApplied;applySelected("autotuneApply100",true);CHECK(applications==2&&SensorCalibrationCodec::u16(local,64+2*69)==5250);CHECK(local.left(64)==snapshot.pages[1].left(64)&&SensorCalibrationCodec::u16(local,64)==5000);
    setup(false,0);CHECK(!autotune.proposal().isEmpty());setup(true,10000);CHECK(autotune.proposal().isEmpty());setup(true,2000);CHECK(!autotune.proposal().isEmpty());
    if(!qEnvironmentVariable("TUNUINO_AUTOTUNE_PREVIEW").isEmpty()){app.processEvents();CHECK(autotune.grab().save(qEnvironmentVariable("TUNUINO_AUTOTUNE_PREVIEW")));}
    snapshot.generation=8;autotune.disconnected();CHECK(autotune.proposal().isEmpty());
    std::cout<<"Tools menu, assignment conflicts/shared coils, Scope chunks, output test gates and autotune provenance passed (host simulation)\n";
}
