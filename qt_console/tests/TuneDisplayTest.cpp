#include "display/TuneDisplayWidget.h"
#include "protocol/SensorCalibrationCodec.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QSettings>
#include <QTimer>
#include <QMenu>
#include <QContextMenuEvent>
#include <QTest>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <iostream>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed: "<<#x<<" at "<<__LINE__<<std::endl;std::exit(1);}}while(false)
using Codec=tunuino::SensorCalibrationCodec;
static void put32(QByteArray &bytes,int at,quint32 n){Codec::put(bytes,at,n>>16);Codec::put(bytes,at+2,n);}
int main(int argc,char **argv){
    QApplication app(argc,argv);QTemporaryDir temp;CHECK(temp.isValid());
    app.setOrganizationName("TunuinoTests");app.setApplicationName("TuneDisplayTest");
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,temp.path());
    QByteArray aux=Codec::initialAux(tunuino::SensorKind::Iat,QByteArray(256,0)),runtime(101,0);
    Codec::put(aux,70,4);for(int i=0;i<3;++i)Codec::put(aux,120+2*i,900+i*100);
    runtime[39]=2;put32(runtime,45,60000);
    CHECK(tunuino::ecuTargetLambda(aux,runtime)==.95);
    put32(runtime,45,0);CHECK(tunuino::ecuTargetLambda(aux,runtime)==.9);
    put32(runtime,45,500000);CHECK(tunuino::ecuTargetLambda(aux,runtime)==1.1);
    put32(runtime,45,500001);CHECK(std::isnan(tunuino::ecuTargetLambda(aux,runtime)));
    put32(runtime,45,100000);CHECK(tunuino::ecuTargetLambda(aux,runtime)==1.0);
    Codec::put(aux,70,0);CHECK(std::isnan(tunuino::ecuTargetLambda(aux,runtime)));
    Codec::put(aux,70,4);runtime[39]=0;CHECK(std::isnan(tunuino::ecuTargetLambda(aux,runtime)));
    runtime[39]=2;auto invalid=aux;Codec::put(invalid,116,2000);CHECK(std::isnan(tunuino::ecuTargetLambda(invalid,runtime)));
    invalid=aux;Codec::put(invalid,122,1400);CHECK(std::isnan(tunuino::ecuTargetLambda(invalid,runtime)));
    CHECK(std::isnan(tunuino::ecuTargetLambda(QByteArray{},runtime)));
    tunuino::TuneDisplayWidget widget;widget.resize(780,520);widget.show();app.processEvents();
    for(int i=0;i<9;++i)CHECK(widget.channel(i)==-1);
    std::array<double,tunuino::LogChannelCount> values={2500,24.5,101.3,.98,86,31.5,13.92};
    widget.setReadings(values,.95);CHECK(std::isnan(widget.reading(0))); // Offline values cannot appear live.
    widget.setConnected(true);widget.setReadings(values,.95);CHECK(widget.reading(0)==2500&&widget.reading(3)==.98&&widget.reading(7)==.95);
    for(int i=0;i<8;++i)CHECK(widget.assignChannel(i,i));CHECK(widget.assignChannel(8,0));
    CHECK(!widget.assignChannel(0,8)&&widget.channel(0)==0);CHECK(!widget.assignChannel(9,1));
    {tunuino::TuneDisplayWidget reopened;for(int i=0;i<8;++i)CHECK(reopened.channel(i)==i);CHECK(reopened.channel(8)==0);}
    auto *cell=widget.findChild<QWidget*>("tuneReadout0");CHECK(cell);
    QTimer::singleShot(0,&widget,[&]{auto *menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());CHECK(menu&&menu->actions().size()==9);menu->actions()[3]->trigger();menu->close();});
    QTest::mouseDClick(cell,Qt::LeftButton);CHECK(widget.channel(0)==2);
    values[3]=std::numeric_limits<double>::infinity();widget.setReadings(values,std::numeric_limits<double>::quiet_NaN());CHECK(std::isnan(widget.reading(3))&&std::isnan(widget.reading(7)));
    widget.setConnected(false);for(int i=0;i<8;++i)CHECK(std::isnan(widget.reading(i)));CHECK(widget.channel(0)==2);
    widget.setConnected(true);values[3]=.98;widget.setReadings(values,.95);
    QTest::qWait(2350);for(int i=0;i<8;++i)CHECK(std::isnan(widget.reading(i))); // Stale telemetry clears markers and digits.
    widget.resize(420,320);app.processEvents();CHECK(!widget.grab().isNull());
    widget.resize(960,680);widget.setReadings(values,.95);app.processEvents();
    if(qEnvironmentVariableIsSet("TUNUINO_TUNE_SCREENSHOT"))CHECK(widget.grab().save(qEnvironmentVariable("TUNUINO_TUNE_SCREENSHOT")));
    // MDI close/reopen keeps the selected channels and a single widget instance.
    QMdiArea area;auto *mdiWidget=new tunuino::TuneDisplayWidget;auto *window=area.addSubWindow(mdiWidget);window->setAttribute(Qt::WA_DeleteOnClose,false);area.show();window->show();CHECK(mdiWidget->assignChannel(0,4));window->close();window->show();mdiWidget->show();CHECK(mdiWidget->channel(0)==4);
    QSettings settings;QStringList corrupt;for(int i=0;i<9;++i)corrupt<<"0";corrupt[8]="8";settings.setValue("tuneDisplay/channels",corrupt);
    {tunuino::TuneDisplayWidget recovered;for(int i=0;i<9;++i)CHECK(recovered.channel(i)==-1);}
    std::cout<<"Tune Display: target curve, validity, nine channels, persistence, selection, stale data, MDI reopening and rendering passed (synthetic data)\n";
}
