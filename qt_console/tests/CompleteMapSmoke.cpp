// Integration check using the real Console and its File menu.
// Offline only; settings/files are isolated in a temporary directory.
#include "MainWindow.h"
#include "project/MapProject.h"
#include "parameters/ParameterWidget.h"
#include "parameters/TriggerPresetDialog.h"
#include "calibration/SensorCurveWizard.h"
#include "calibration/SensorCalibrationWidget.h"
#include <QCheckBox>
#include <QComboBox>
#include <QListView>
#include <QScrollBar>
#include <QImage>
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
    auto *sensorTree=window.findChild<QTreeWidget*>("MainNavigation");CHECK(sensorTree);auto *sensorRoot=sensorTree->topLevelItem(0);CHECK(sensorRoot->text(0)=="Sensors and inputs");
    const QStringList expectedSensorGroups{"Analog inputs","Digital inputs","Oxygen Sensor","TPS, PPS","Switches","IAT, CLT","MAP, BARO","Pressure","Temperature","Other sensors","VSS and Gearbox"};CHECK(sensorRoot->childCount()==expectedSensorGroups.size());for(int i=0;i<expectedSensorGroups.size();++i)CHECK(sensorRoot->child(i)->text(0)==expectedSensorGroups[i]);
    CHECK(!(sensorRoot->child(1)->child(2)->flags()&Qt::ItemIsEnabled));CHECK(sensorRoot->child(7)->childCount()==12&&sensorRoot->child(8)->childCount()==11&&sensorRoot->child(10)->childCount()==9);
    const auto openSensor=[&](const QString &key){QTreeWidgetItemIterator sensorItem(sensorTree);while(*sensorItem){if((*sensorItem)->data(0,Qt::UserRole+1).toString()==key){sensorTree->itemClicked(*sensorItem,0);app.processEvents();return;}++sensorItem;}CHECK(false);};
    openSensor("inputs.analog");auto *inputWindow=window.findChild<QMdiArea*>()->activeSubWindow();CHECK(inputWindow&&inputWindow->windowTitle()=="Sensors and inputs - Analog inputs");inputWindow->close();
    openSensor("inputs.primary");auto *primaryInputWindow=window.findChild<QMdiArea*>()->activeSubWindow();CHECK(primaryInputWindow&&primaryInputWindow->windowTitle()=="Digital inputs - Primary trigger");auto *primaryFields=primaryInputWindow->widget()->findChild<QTableWidget*>("parameters");CHECK(primaryFields&&primaryFields->rowCount()==4&&primaryFields->columnCount()==2);primaryInputWindow->close();
    openSensor("inputs.cam1");auto *camInputWindow=window.findChild<QMdiArea*>()->activeSubWindow();CHECK(camInputWindow&&camInputWindow->windowTitle()=="Digital inputs - CAM 1");auto *camFields=camInputWindow->widget()->findChild<QTableWidget*>("parameters");CHECK(camFields&&camFields->rowCount()==3&&camFields->columnCount()==2);camInputWindow->close();
    auto *iat=dynamic_cast<tunuino::SensorCalibrationWidget*>(window.findChild<QWidget*>("sensorIAT"));CHECK(iat);const auto beforeWizard=project->capture(error);const QJsonValue oldInput=iat->editorState()["input"];
    openSensor("sensorwizard:IAT");auto *sensorWizard=dynamic_cast<tunuino::SensorCurveWizard*>(window.findChild<QDialog*>("iatSensorWizard"));CHECK(sensorWizard&&sensorWizard->isVisible()&&sensorWizard->isModal());auto *sensorOk=sensorWizard->findChild<QPushButton*>("sensorWizardOk");CHECK(sensorOk&&!sensorOk->isEnabled());
    for(int i=0;i<3;++i)sensorWizard->findChild<QDoubleSpinBox*>(QString("sensorWizardValue%1").arg(i))->setValue(i==1?3950:2500);CHECK(sensorOk->isEnabled());sensorOk->click();CHECK(!sensorWizard->isVisible());CHECK(iat->editorState()["input"]==oldInput);CHECK(project->capture(error)["parameters"]==beforeWizard["parameters"]&&project->capture(error)["pages"]==beforeWizard["pages"]);
    const auto prepared=project->capture(error);openSensor("sensorwizard:IAT");sensorWizard->reject();CHECK(project->capture(error)==prepared);
    openSensor("sensorwizard:MAP");auto *pressureWizard=dynamic_cast<tunuino::SensorCurveWizard*>(window.findChild<QDialog*>("mapSensorWizard"));CHECK(pressureWizard&&pressureWizard->isVisible());pressureWizard->findChild<QDoubleSpinBox*>("sensorWizardValue0")->setValue(.5);pressureWizard->findChild<QDoubleSpinBox*>("sensorWizardValue2")->setValue(4.5);pressureWizard->findChild<QDoubleSpinBox*>("sensorWizardValue3")->setValue(300);pressureWizard->findChild<QPushButton*>("sensorWizardOk")->click();CHECK(!pressureWizard->isVisible());
    CHECK(project->capture(error)["sensors"].toObject()["MAP"].toObject()["volts"].toArray()==QJsonArray({.5,4.5}));
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
    auto *engines=presets->findChild<QComboBox*>("predefinedTriggers");auto *ok=presets->findChild<QPushButton*>("triggerPresetOk");CHECK(engines&&ok&&engines->count()==tunuino::TriggerPresets::catalog().size());
    // Exercise the actual popup: populated model data alone did not catch
    // the pale-on-pale menu delegate that hid every engine name.
    engines->showPopup();app.processEvents();auto *list=engines->view();CHECK(list->isVisible());
    for(int row=0;row<engines->count();++row)CHECK(list->model()->index(row,0).data().toString()==tunuino::TriggerPresets::catalog()[row].name);
    const auto second=list->model()->index(1,0);const auto rowRect=list->visualRect(second).adjusted(8,2,-8,-2);CHECK(!rowRect.isEmpty());
    const auto popupImage=list->viewport()->grab().toImage().scaled(list->viewport()->size());int darkText=0;const auto pixels=rowRect.intersected(popupImage.rect());
    for(int y=pixels.top();y<=pixels.bottom();++y)for(int x=pixels.left();x<=pixels.right();++x){const auto color=popupImage.pixelColor(x,y);if(color.red()<100&&color.green()<100&&color.blue()<100)++darkText;}
    CHECK(darkText>40);CHECK(list->verticalScrollBar()->maximum()>0);QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualRect(second).center());app.processEvents();CHECK(engines->currentIndex()==1&&!list->isVisible());
    engines->showPopup();app.processEvents();const auto lastEngine=list->model()->index(engines->count()-1,0);list->scrollTo(lastEngine,QAbstractItemView::PositionAtBottom);app.processEvents();CHECK(list->viewport()->rect().contains(list->visualRect(lastEngine).center()));QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualRect(lastEngine).center());app.processEvents();CHECK(engines->currentIndex()==engines->count()-1&&!list->isVisible());
    const auto beforePreset=project->capture(error);engines->setCurrentIndex(5);CHECK(!ok->isEnabled());CHECK(project->capture(error)==beforePreset);
    presets->findChild<QPushButton*>("triggerPresetCancel")->click();CHECK(!presets->isVisible()&&project->capture(error)==beforePreset);
    presets->open();engines->setCurrentIndex(1);presets->findChild<QCheckBox*>("triggerApply3")->setChecked(false);
    QJsonObject expected;CHECK(tunuino::TriggerPresets::apply(beforePreset,1,tunuino::TriggerPresets::Primary,expected,error));ok->click();CHECK(!presets->isVisible());CHECK(project->capture(error)==expected);
    presets->open();engines->setCurrentIndex(0);presets->findChild<QCheckBox*>("triggerApply0")->setChecked(false);
    const auto beforeOrder=project->capture(error);CHECK(tunuino::TriggerPresets::apply(beforeOrder,0,tunuino::TriggerPresets::Firing,expected,error));ok->click();CHECK(project->capture(error)==expected);
    const auto engine=expected["parameters"].toObject()["ignition.firing"].toObject()["engineProfile"].toObject();CHECK(engine["order"].toArray()==QJsonArray({0,4,2,5,1,3}));CHECK(expected["pages"]==beforePreset["pages"]);
    const auto afterPreset=project->capture(error);CHECK(!tunuino::TriggerPresets::apply(afterPreset,0,tunuino::TriggerPresets::Secondary,expected,error));CHECK(project->capture(error)==afterPreset);
    const auto beforePressureWizard=project->capture(error);openSensor("sensorwizard:MAP");pressureWizard->findChild<QComboBox*>("pressureWizardTarget")->setCurrentIndex(2);pressureWizard->findChild<QDoubleSpinBox*>("sensorWizardValue3")->setValue(1000);pressureWizard->findChild<QPushButton*>("sensorWizardOk")->click();CHECK(!pressureWizard->isVisible());const auto pressureDraft=project->capture(error);CHECK(pressureDraft["pages"]==beforePressureWizard["pages"]&&pressureDraft["parameters"]==beforePressureWizard["parameters"]);CHECK(pressureDraft["sensors"].toObject()["Oil pressure"].toObject()["values"].toArray()==QJsonArray({0,1000}));
    auto legacy=pressureDraft;auto six=legacy["sensors"].toObject();six.remove("Fuel pressure");six.remove("Oil pressure");legacy["sensors"]=six;CHECK(project->apply(legacy,error));const auto upgraded=project->capture(error);auto eight=upgraded["sensors"].toObject();CHECK(eight.size()==8);CHECK(eight["Fuel pressure"].toObject()["input"]==0&&eight["Oil pressure"].toObject()["input"]==0);eight.remove("Fuel pressure");eight.remove("Oil pressure");CHECK(eight==six&&upgraded["pages"]==legacy["pages"]);
    auto oldInputs=project->capture(error);auto oldParameters=oldInputs["parameters"].toObject();oldParameters.remove("inputs.analog");oldParameters.remove("inputs.primary");oldParameters.remove("inputs.cam1");oldInputs["parameters"]=oldParameters;CHECK(project->apply(oldInputs,error));CHECK(project->capture(error)["parameters"].toObject().contains("inputs.analog"));CHECK(project->capture(error)["parameters"].toObject().contains("inputs.primary"));CHECK(project->capture(error)["parameters"].toObject().contains("inputs.cam1"));
    auto badInputs=project->capture(error);auto badPages=badInputs["pages"].toObject();auto badHeader=QByteArray::fromHex(badPages["header"].toString().toLatin1());badHeader[70]=2;badPages["header"]=QString::fromLatin1(badHeader.toHex());badInputs["pages"]=badPages;CHECK(!project->apply(badInputs,error));
    auto badPrimaryProject=project->capture(error);auto primaryPages=badPrimaryProject["pages"].toObject();auto primaryBytes=QByteArray::fromHex(primaryPages["header"].toString().toLatin1());primaryBytes[5]=2;primaryPages["header"]=QString::fromLatin1(primaryBytes.toHex());badPrimaryProject["pages"]=primaryPages;CHECK(!project->apply(badPrimaryProject,error));
    auto badCamProject=project->capture(error);auto camPages=badCamProject["pages"].toObject();auto camBytes=QByteArray::fromHex(camPages["header"].toString().toLatin1());camBytes[98]=2;camPages["header"]=QString::fromLatin1(camBytes.toHex());badCamProject["pages"]=camPages;CHECK(!project->apply(badCamProject,error));
    std::cout<<"Real File/Save Project as from Firing order: full 6-cylinder map round-trip, all three tables, parameters, comment, atomic rejection and complete-project dialog and caption maximize/minimize/close/reopen passed offline\n"<<std::flush;
    std::_Exit(0);
}
