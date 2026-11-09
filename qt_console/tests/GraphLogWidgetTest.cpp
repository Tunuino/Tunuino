#include "logging/GraphLogWidget.h"
#include "logging/TextLogWidget.h"
#include "logging/ScatterPlotWidget.h"
#include "ui/GeneralOptions.h"
#include <QDir>
#include <QFile>
#include <QApplication>
#include <QAction>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QTabBar>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QInputDialog>
#include <QMenu>
#include <QContextMenuEvent>
#include <QTest>
#include <QMouseEvent>
#include <cstdlib>
#include <iostream>
#define CHECK(condition) do { if (!(condition)) { std::cerr << "Failed: " << #condition << " at " << __LINE__ << std::endl; std::exit(1); } } while (false)

int main(int argc,char **argv) {
    QApplication app(argc,argv);
    if(!qEnvironmentVariable("TUNUINO_MENU_STYLE").isEmpty()){QFile css(qEnvironmentVariable("TUNUINO_MENU_STYLE"));CHECK(css.open(QIODevice::ReadOnly));app.setStyleSheet(QString::fromUtf8(css.readAll()));}
    auto original=tunuino::defaultGraphLogLayout(),decoded=original;
    original[5].name="Custom test";original[5].dots=true;
    original[5].charts[0].axes[0]={3,false,.7,1.5};
    CHECK(tunuino::decodeGraphLogLayout(tunuino::encodeGraphLogLayout(original),decoded));
    CHECK(decoded[5].name=="Custom test" && decoded[5].dots);
    CHECK(!decoded[5].charts[0].axes[0].autoscale && decoded[5].charts[0].axes[0].minimum==.7);
    const auto previous=tunuino::encodeGraphLogLayout(decoded);
    const auto reject=[&](tunuino::GraphLogLayout bad) {
        CHECK(!tunuino::decodeGraphLogLayout(tunuino::encodeGraphLogLayout(bad),decoded));
        CHECK(tunuino::encodeGraphLogLayout(decoded)==previous);
    };
    auto bad=original;bad[0].name=" ";reject(bad);
    bad=original;bad[0].charts.clear();reject(bad);
    bad=original;while(bad[0].charts.size()<5)bad[0].charts.push_back(bad[0].charts.front());reject(bad);
    bad=original;bad[0].charts[0].axes[1].channel=bad[0].charts[0].axes[0].channel;reject(bad);
    bad=original;bad[0].charts[0].axes[0].channel=7;reject(bad);
    bad=original;bad[0].charts[0].axes[0].maximum=bad[0].charts[0].axes[0].minimum;reject(bad);
    CHECK(!tunuino::decodeGraphLogLayout("{broken json",decoded));
    QTemporaryDir temp;CHECK(temp.isValid());
    QSettings settings(temp.path()+"/test.ini",QSettings::IniFormat);
    settings.setValue("logging/graphLayoutV1","invalid layout"); // Recover defaults without changing samples.
    tunuino::GraphLogWidget widget(nullptr,&settings);
    auto *tabs=widget.findChild<QTabWidget*>("graphLogTabs");
    CHECK(tabs && tabs->count()==12 && tabs->tabText(0)=="Basic");
    auto action=[&](const char *id) {
        auto *a=widget.findChild<QAction*>(id);CHECK(a);return a;
    };
    auto *table=widget.findChild<QTableWidget*>("logCursorValues");CHECK(table);
    CHECK(!action("logRecord")->isEnabled());
    widget.setConnected(true);action("logRecord")->trigger();
    std::array<double,tunuino::LogChannelCount> values;
    values.fill(std::numeric_limits<double>::quiet_NaN());
    values[0]=0;values[2]=101.3;values[3]=.98;values[4]=85;
    widget.appendRuntime(values);
    CHECK(table->item(0,1)->text()=="0");CHECK(table->item(1,1)->text()=="Unavailable");
    action("logPause")->trigger();QTest::qWait(30);
    values[0]=1500;widget.appendRuntime(values);
    CHECK(table->item(0,1)->text()=="0");
    tabs->setCurrentIndex(1);CHECK(table->item(0,1)->text()=="0");
    action("logPause")->trigger();CHECK(table->item(0,1)->text()=="1500");
    QTimer::singleShot(0,&widget,[&] {
        auto *dialog=widget.findChild<QDialog*>("logChannelConfig");CHECK(dialog);
        dialog->findChild<QComboBox*>("axisChannel0")->setCurrentIndex(4); // Lambda.
        dialog->findChild<QComboBox*>("axisChannel1")->setCurrentIndex(3); // MAP.
        dialog->findChild<QCheckBox*>("axisAutoscale0")->setChecked(false);
        dialog->findChild<QDoubleSpinBox*>("axisMinimum0")->setValue(.7);
        dialog->findChild<QDoubleSpinBox*>("axisMaximum0")->setValue(1.5);
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    });
    action("logConfigure")->trigger();
    tunuino::GraphLogLayout saved;
    CHECK(tunuino::decodeGraphLogLayout(settings.value("logging/graphLayoutV1").toByteArray(),saved));
    CHECK(saved[1].charts[0].axes[0].channel==3 && !saved[1].charts[0].axes[0].autoscale);
    CHECK(saved[1].charts[0].axes[0].minimum==.7 && saved[0].charts.size()==2);
    CHECK(table->item(0,1)->text()=="1500"); // Editing axes must not clear recorded data.
    action("logStyle")->trigger();
    QTimer::singleShot(0,&widget,[&] {
        auto *dialog=widget.findChild<QInputDialog*>();CHECK(dialog);
        dialog->setTextValue("Fuel test");dialog->accept();
    });
    CHECK(QMetaObject::invokeMethod(tabs->tabBar(),"tabBarDoubleClicked",Q_ARG(int,1)));
    CHECK(tabs->tabText(1)=="Fuel test");
    tunuino::GraphLogWidget restored(nullptr,&settings);
    auto *restoredTabs=restored.findChild<QTabWidget*>("graphLogTabs");CHECK(restoredTabs->tabText(1)=="Fuel test");
    CHECK(tunuino::decodeGraphLogLayout(settings.value("logging/graphLayoutV1").toByteArray(),saved));
    CHECK(saved[1].dots && !saved[0].dots && saved[1].charts[0].axes[0].maximum==1.5);
    for(int i=0;i<5;++i) {QTest::qWait(20);values[0]=1500+200*i;values[2]=100-3*i;values[3]=.98-.02*i;widget.appendRuntime(values);}
    widget.setConnected(false);CHECK(!action("logRecord")->isEnabled());
    const auto finalRpm=table->item(0,1)->text();
    values[0]=4000;widget.appendRuntime(values);CHECK(table->item(0,1)->text()==finalRpm);
    widget.resize(1000,700);widget.show();tabs->setCurrentIndex(0);action("logFull")->trigger();app.processEvents();
    CHECK(!widget.grab().isNull());
    // A click on either graph selects one timestamp for all chart legends and cursor values.
    auto *plot=tabs->widget(0)->findChild<QWidget*>("logPlot_0_0");CHECK(plot);
    QTest::mouseClick(plot,Qt::LeftButton,Qt::NoModifier,QPoint(82,60));
    CHECK(table->item(0,1)->text()=="0");
    const auto graphMenu=[&](const QString &label) {
        auto *currentPlot=tabs->widget(0)->findChild<QWidget*>("logPlot_0_0");CHECK(currentPlot);
        QTimer::singleShot(0,&widget,[&] {
            auto *menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());CHECK(menu);
            QAction *chosen=nullptr;
            for(auto *a:menu->actions())if(a->text()==label)chosen=a;
            CHECK(chosen && chosen->isEnabled());menu->setActiveAction(chosen);QTest::keyClick(menu,Qt::Key_Return);
        });
        QContextMenuEvent event(QContextMenuEvent::Mouse,QPoint(100,50),currentPlot->mapToGlobal(QPoint(100,50)));
        QApplication::sendEvent(currentPlot,&event);
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
    };
    graphMenu("Add graph");
    CHECK(tunuino::decodeGraphLogLayout(settings.value("logging/graphLayoutV1").toByteArray(),saved));
    CHECK(saved[0].charts.size()==3 && table->item(0,1)->text()=="0");
    graphMenu("Remove graph");
    CHECK(tunuino::decodeGraphLogLayout(settings.value("logging/graphLayoutV1").toByteArray(),saved));
    CHECK(saved[0].charts.size()==2 && table->item(0,1)->text()=="0");
    const QString screenshot=qEnvironmentVariable("TUNUINO_LOG_TEST_SCREENSHOT");
    if(!screenshot.isEmpty())CHECK(widget.grab().save(screenshot));
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());app.setOrganizationName("TunuinoOptionsTest");app.setApplicationName("Logging");QSettings().setValue("general/stopPaused",true);QSettings().setValue("general/autoSave",true);QSettings().setValue("general/logDirectory",temp.path()+"/automatic");QSettings().setValue("general/units",1);
    tunuino::GraphLogWidget controlled;controlled.setConnected(true);auto *record=controlled.findChild<QAction*>("logRecord");record->trigger();std::array<double,tunuino::LogChannelCount> optionsValues{};optionsValues[0]=1500;optionsValues[2]=100;optionsValues[4]=100;controlled.appendRuntime(optionsValues);auto *readings=controlled.findChild<QTableWidget*>("logCursorValues");CHECK(readings->item(2,2)->text()=="psi"&&readings->item(4,1)->text()=="212");controlled.findChild<QAction*>("logPause")->trigger();QTest::qWait(2);optionsValues[0]=2500;controlled.appendRuntime(optionsValues);controlled.findChild<QAction*>("logPause")->trigger();CHECK(readings->item(0,1)->text()=="1500");QTest::qWait(2);controlled.appendRuntime(optionsValues);record->trigger();const auto files=QDir(temp.path()+"/automatic").entryList({"*.csv"},QDir::Files);CHECK(files.size()==1);QFile autoLog(temp.path()+"/automatic/"+files[0]);CHECK(autoLog.open(QIODevice::ReadOnly));std::vector<tunuino::LogSample> savedSamples;std::string csvError;CHECK(tunuino::decodeLogCsv(autoLog.readAll().toStdString(),savedSamples,csvError)&&savedSamples.size()==2&&savedSamples[1].values[2]==100);CHECK(std::abs(tunuino::displayValue(2,100)-14.50377377)<.00001);QSettings().clear();
    // Shared real log data drives scatter filtering and text cursor/live transitions.
    QSettings analysisSettings(temp.path()+"/analysis.ini",QSettings::IniFormat);auto analysisLayout=tunuino::defaultGraphLogLayout();analysisLayout[0].charts[0].axes[0]={0,false,0,10000};analysisSettings.setValue("logging/graphLayoutV1",tunuino::encodeGraphLogLayout(analysisLayout));
    tunuino::GraphLogWidget analysis(nullptr,&analysisSettings);analysis.setConnected(true);analysis.findChild<QAction*>("logRecord")->trigger();
    for(int i=0;i<3;++i){std::array<double,7> sample{{1000.+i*1000,20.,40.+i*40,i==1?std::numeric_limits<double>::quiet_NaN():1.+i*.1,80,30,13.8}};QTest::qWait(2);analysis.appendRuntime(sample);}CHECK(analysis.samples().size()==3);
    tunuino::ScatterPlotWidget scatter(&analysis);CHECK(scatter.points().size()==2);CHECK(scatter.setConfiguration({{0,2,-1}},{{2,1,1}},4,false));CHECK(scatter.points().size()==3&&scatter.points()[1].x==1500&&scatter.points()[2].x==2500);CHECK(analysis.axisRange(0).first==0&&analysis.axisRange(0).second==10000);CHECK(!scatter.setConfiguration({{7,2,3}},{{1,1,1}},3,false));
    analysis.selectRange(analysis.samples()[1].seconds,analysis.samples()[2].seconds);scatter.rebuild();CHECK(scatter.points().size()==2&&scatter.points()[0].x==2000&&scatter.points()[1].x==2500);analysis.selectRange(-1,-1);scatter.rebuild();CHECK(scatter.points().size()==3);
    tunuino::TextLogWidget textLog;textLog.setReadings(analysis.samples()[2].values);auto *textValues=textLog.findChild<QTableWidget*>("textLogGroup0");CHECK(textValues->item(0,1)->text()=="3000");textLog.inspect(&analysis.samples()[0]);CHECK(textValues->item(0,1)->text()=="1000"&&textValues->item(0,1)->font().italic());textLog.disconnected();CHECK(textValues->item(0,1)->text()=="1000");textLog.inspect(nullptr);CHECK(textValues->item(0,1)->text()==QString::fromUtf8("—"));CHECK(!textValues->item(0,1)->font().italic());
    CHECK(textLog.addToCustom(0,2)&&textLog.addToCustom(0,0));CHECK(!textLog.addToCustom(3,0));tunuino::TextLogWidget textRestored;auto *custom=textRestored.findChild<QTableWidget*>("textLogGroup4");CHECK(custom->rowCount()==2&&custom->item(0,0)->text()=="MAP"&&custom->item(1,0)->text()=="RPM");
    textLog.setReadings(analysis.samples()[2].values);analysis.inspectionChanged=[&](const tunuino::LogSample *sample){textLog.inspect(sample);};analysis.resize(900,600);analysis.show();app.processEvents();auto *analysisPlot=analysis.findChild<QWidget*>("logPlot_0_0");CHECK(analysisPlot);QMouseEvent hover(QEvent::MouseMove,QPointF(82,60),Qt::NoButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(analysisPlot,&hover);CHECK(textValues->item(0,1)->text()=="1000");QEvent leave(QEvent::Leave);QApplication::sendEvent(analysisPlot,&leave);CHECK(textValues->item(0,1)->text()=="3000"&&!textValues->item(0,1)->font().italic());
    const auto preview=qEnvironmentVariable("TUNUINO_ANALYSIS_PREVIEW");if(!preview.isEmpty()){CHECK(scatter.setConfiguration({{0,2,3}},{{1,1,1}},5,false));scatter.resize(900,600);scatter.show();textLog.resize(700,420);textLog.show();app.processEvents();CHECK(scatter.grab().save(preview+"-scatter.png"));CHECK(textLog.grab().save(preview+"-text.png"));}
    analysis.inspectionChanged={};textLog.inspect(nullptr);textLog.setReadings(analysis.samples()[2].values);QTest::qWait(2300);CHECK(textValues->item(0,1)->text()==QString::fromUtf8("—"));
    std::cout<<"Graph Log capture, pause, 12 tabs, shared cursor, dual axes, saved config, corrupt settings and rendering: passed\n";
}
