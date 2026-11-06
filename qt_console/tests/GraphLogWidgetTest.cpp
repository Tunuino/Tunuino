#include "logging/GraphLogWidget.h"
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
#include <cstdlib>
#include <iostream>
#define CHECK(condition) do { if (!(condition)) { std::cerr << "Failed: " << #condition << " at " << __LINE__ << std::endl; std::exit(1); } } while (false)

int main(int argc,char **argv) {
    QApplication app(argc,argv);
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
    std::cout<<"Graph Log capture, pause, 12 tabs, shared cursor, dual axes, saved config, corrupt settings and rendering: passed\n";
}
