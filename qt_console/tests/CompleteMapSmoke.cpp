// Integration check using the real Console and its File menu.
// Offline only; settings/files are isolated in a temporary directory.
#include "MainWindow.h"
#include "project/MapProject.h"
#include "parameters/ParameterWidget.h"
#include "parameters/TriggerPresetDialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QApplication>
#include <QStyleFactory>
#include <QTemporaryDir>
#include <QSettings>
#include <QDoubleSpinBox>
#include <QTreeWidget>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QJsonArray>
#include <QFile>
#include <QAction>
#include <QFileDialog>
#include <QTimer>
#include <QTest>
#include "ui/CalibrationAppearance.h"
#include <iostream>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed "<<#x<<" line "<<__LINE__<<": "<<error.toStdString()<<std::endl;std::exit(1);}}while(false)
int main(int argc,char **argv){QApplication app(argc,argv);app.setStyle(QStyleFactory::create("Fusion"));QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);QTemporaryDir temp;app.setOrganizationName("TunuinoTest");app.setApplicationName("CompleteMap");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());QString error;
    MainWindow window;window.show();app.processEvents();auto *project=dynamic_cast<tunuino::MapProject*>(window.findChild<QObject*>("mapProject"));CHECK(project);
    auto *firing=dynamic_cast<tunuino::ParameterWidget*>(window.findChild<QWidget*>("ignition.firing"));CHECK(firing);firing->findChild<QDoubleSpinBox*>("cylinders")->setValue(6);
    auto *tree=window.findChild<QTreeWidget*>("MainNavigation");CHECK(tree);QTreeWidgetItemIterator it(tree);while(*it){if((*it)->data(0,Qt::UserRole+1).toString()=="ignition.firing"){tree->itemClicked(*it,0);break;}++it;}app.processEvents();CHECK(window.findChild<QMdiArea*>()->activeSubWindow()->windowTitle()=="Ignition - Firing order");
    auto data=project->capture(error);CHECK(!data.isEmpty());auto tables=data["tables"].toObject();for(const auto &name:{"fuel","ignition","afr"}){auto t=tables[name].toObject();auto rows=t["cells"].toArray(),row=rows[0].toArray();row[0]=QString(name)=="fuel"?123.45:QString(name)=="ignition"?-12.34:12.8;rows[0]=row;t["cells"]=rows;tables[name]=t;}data["tables"]=tables;
    auto parameters=data["parameters"].toObject();auto general=parameters["fuel.general"].toObject(),values=general["values"].toObject();values["requiredFuel"]=6.75;general["values"]=values;parameters["fuel.general"]=general;data["parameters"]=parameters;data["service"]=QJsonObject{{"comment","Full six-cylinder map"}};
    CHECK(project->apply(data,error));CHECK(project->capture(error)==data);const auto path=temp.path()+"/whole.tnproj";CHECK(project->save(path,error));QFile file(path);CHECK(file.open(QIODevice::ReadOnly));const auto saved=file.readAll();file.close();
    firing->findChild<QDoubleSpinBox*>("cylinders")->setValue(2);CHECK(project->capture(error)!=data);CHECK(project->open(path,error));CHECK(firing->findChild<QDoubleSpinBox*>("cylinders")->value()==6);CHECK(project->capture(error)==data);
    auto bad=data;auto sensors=bad["sensors"].toObject(),tps=sensors["TPS"].toObject();tps["input"]=99;sensors["TPS"]=tps;bad["sensors"]=sensors;const auto corrupt=temp.path()+"/invalid.tnproj";QFile invalid(corrupt);CHECK(invalid.open(QIODevice::WriteOnly));invalid.write(tunuino::MapProject::encode(bad,error));invalid.close();CHECK(!project->open(corrupt,error));CHECK(project->capture(error)==data);
    auto *save=window.findChild<QAction*>("fileSaveMap");CHECK(save);CHECK(save->text()=="Save Project as...");CHECK(window.findChild<QAction*>("fileOpenMap")->text()=="Open Project...");CHECK(save->shortcut()==QKeySequence::Save);bool dialogChecked=false;
    QTimer::singleShot(100,[&]{for(auto *widget:QApplication::topLevelWidgets())if(auto *dialog=qobject_cast<QFileDialog*>(widget)){CHECK(dialog->nameFilters()==QStringList{"Tunuino project (*.tnproj)"});CHECK(dialog->defaultSuffix()=="tnproj");CHECK(dialog->selectedFiles().value(0).endsWith(".tnproj"));CHECK(!dialog->selectedFiles().value(0).contains("fuel.json"));dialogChecked=true;dialog->reject();}});save->trigger();CHECK(dialogChecked);
    auto *mdi=window.findChild<QMdiArea*>();QMdiSubWindow *sub=nullptr;for(auto *candidate:mdi->subWindowList())if(candidate->widget()==firing)sub=candidate;CHECK(sub);sub->showNormal();mdi->setActiveSubWindow(sub);app.processEvents();
    const auto captionClick=[&](QStyle::SubControl control){mdi->setActiveSubWindow(sub);sub->raise();QTest::qWait(40);sub->repaint();QStyleOptionTitleBar option;option.initFrom(sub);const int height=sub->style()->pixelMetric(QStyle::PM_TitleBarHeight,&option,sub);option.rect=QRect(4,4,sub->width()-8,height);option.titleBarFlags=sub->windowFlags();option.titleBarState=sub->windowState();option.subControls=QStyle::SC_All;const auto rect=sub->style()->subControlRect(QStyle::CC_TitleBar,&option,control,sub);CHECK(!rect.isEmpty());mdi->setActiveSubWindow(sub);QTest::mouseMove(sub,QPoint(40,height+40));app.processEvents();QTest::mouseMove(sub,rect.center()+QPoint(-1,0));app.processEvents();QTest::mouseMove(sub,rect.center());app.processEvents();QTest::mouseClick(sub,Qt::LeftButton,Qt::NoModifier,rect.center());QTest::qWait(40);app.processEvents();};
    captionClick(QStyle::SC_TitleBarMaxButton);CHECK(sub->isMaximized());sub->showNormal();app.processEvents();captionClick(QStyle::SC_TitleBarMinButton);CHECK(sub->isMinimized());sub->showNormal();app.processEvents();captionClick(QStyle::SC_TitleBarCloseButton);CHECK(!sub->isVisible());sub->showNormal();CHECK(firing->findChild<QDoubleSpinBox*>("cylinders")->value()==6);
    auto *presets=dynamic_cast<tunuino::TriggerPresetDialog*>(window.findChild<QDialog*>("triggerPresetDialog"));CHECK(presets);
    QTreeWidgetItemIterator presetItem(tree);while(*presetItem){if((*presetItem)->data(0,Qt::UserRole+1).toString()=="ignition.wizard"){tree->itemClicked(*presetItem,0);break;}++presetItem;}
    app.processEvents();CHECK(presets->isVisible()&&presets->isModal());
    auto *engines=presets->findChild<QComboBox*>("predefinedTriggers");auto *ok=presets->findChild<QPushButton*>("triggerPresetOk");CHECK(engines&&ok&&engines->count()==11);
    const auto beforePreset=project->capture(error);engines->setCurrentIndex(2);CHECK(!ok->isEnabled());CHECK(project->capture(error)==beforePreset);
    presets->findChild<QPushButton*>("triggerPresetCancel")->click();CHECK(!presets->isVisible()&&project->capture(error)==beforePreset);
    presets->open();engines->setCurrentIndex(1);presets->findChild<QCheckBox*>("triggerApply3")->setChecked(false);
    QJsonObject expected;CHECK(tunuino::TriggerPresets::apply(beforePreset,1,tunuino::TriggerPresets::Primary,expected,error));ok->click();CHECK(!presets->isVisible());CHECK(project->capture(error)==expected);
    presets->open();engines->setCurrentIndex(0);presets->findChild<QCheckBox*>("triggerApply0")->setChecked(false);
    const auto beforeOrder=project->capture(error);CHECK(tunuino::TriggerPresets::apply(beforeOrder,0,tunuino::TriggerPresets::Firing,expected,error));ok->click();CHECK(project->capture(error)==expected);
    const auto engine=expected["parameters"].toObject()["ignition.firing"].toObject()["engineProfile"].toObject();CHECK(engine["order"].toArray()==QJsonArray({0,4,2,5,1,3}));CHECK(expected["pages"]==beforePreset["pages"]);
    const auto afterPreset=project->capture(error);CHECK(!tunuino::TriggerPresets::apply(afterPreset,0,tunuino::TriggerPresets::Secondary,expected,error));CHECK(project->capture(error)==afterPreset);
    std::cout<<"Real File/Save Project as from Firing order: full 6-cylinder map round-trip, all three tables, parameters, comment, atomic rejection and complete-project dialog and caption maximize/minimize/close/reopen passed offline\n"<<std::flush;
    std::_Exit(0);
}
