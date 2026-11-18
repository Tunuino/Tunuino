#include "calibration/IoConfigurationWidget.h"
// Integration check using the real Console and its File menu.
// Offline only; settings/files are isolated in a temporary directory.
#include "MainWindow.h"
#include "ecu/SerialSession.h"
#include "tools/ToolsSuite.h"
#include "tools/ScopeWidget.h"
#include "parameters/TriggerWizard.h"
#include "dbw/DbwWidget.h"
#include "workspace/DesktopController.h"
#include "calibration/SensitivityCurve.h"
#include "tools/AutotuneWidget.h"
#include "project/MapProject.h"
#include "project/ProjectComparator.h"
#include "parameters/ParameterWidget.h"
#include "parameters/TriggerPresetDialog.h"
#include "calibration/SensorCurveWizard.h"
#include "calibration/SensorCalibrationWidget.h"
#include <QCheckBox>
#include <QToolBar>
#include <QToolButton>
#include <QComboBox>
#include <QListView>
#include <QScrollBar>
#include <QImage>
#include <QLabel>
#include <QPushButton>
#include <QApplication>
#include <QSplitter>
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
#include <QElapsedTimer>
#include <QTest>
#include "ui/CalibrationAppearance.h"
#include <iostream>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed "<<#x<<" line "<<__LINE__<<": "<<error.toStdString()<<std::endl;std::exit(1);}}while(false)
int main(int argc,char **argv){QApplication app(argc,argv);app.setStyle(QStyleFactory::create("Fusion"));QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);QTemporaryDir temp;app.setOrganizationName("TunuinoTest");app.setApplicationName("CompleteMap");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());QString error;
    if(!qEnvironmentVariable("TUNUINO_MENU_STYLE").isEmpty()){QFile css(qEnvironmentVariable("TUNUINO_MENU_STYLE"));CHECK(css.open(QIODevice::ReadOnly));app.setStyleSheet(QString::fromUtf8(css.readAll()));}
    QElapsedTimer elapsed;elapsed.start();const auto checkpoint=[&](const char *stage){std::cout<<"CompleteMapSmoke ["<<elapsed.elapsed()<<" ms] "<<stage<<std::endl;};
    checkpoint("construct main window");
    MainWindow window;window.show();app.processEvents();auto *project=dynamic_cast<tunuino::MapProject*>(window.findChild<QObject*>("mapProject"));CHECK(project);
    if(!qEnvironmentVariable("TUNUINO_SERIAL_TEST_PORT").isEmpty()){
        auto *session=window.findChild<tunuino::SerialSession*>();CHECK(session);CHECK(session->open(qEnvironmentVariable("TUNUINO_SERIAL_TEST_PORT")));CHECK(session->exchange("TUNUINO?"));
        tunuino::ToolsSuite *tools=nullptr;for(auto *object:window.findChildren<QObject*>())if(auto *candidate=dynamic_cast<tunuino::ToolsSuite*>(object))tools=candidate;CHECK(tools);
        tunuino::ScopeWidget *scope=nullptr;for(auto *widget:window.findChildren<QWidget*>())if(auto *candidate=dynamic_cast<tunuino::ScopeWidget*>(widget))scope=candidate;CHECK(scope&&scope->snapshot);
        for(int n=0;n<500&&!scope->snapshot().diagnosticValid();++n)QTest::qWait(20);
        const auto base=scope->snapshot();CHECK(base.diagnosticValid()&&base.pages[0][128]==0);
        CHECK(tools->snapshot().valid()==qEnvironmentVariable("TUNUINO_SERIAL_FAULT").isEmpty());
        auto *wizard=dynamic_cast<tunuino::TriggerWizard*>(window.findChild<QWidget*>("triggerWizard"));CHECK(wizard);wizard->next();CHECK(wizard->currentId()==2&&wizard->reviewReady());
        auto localDraft=wizard->editorState();localDraft["capture"]=false;CHECK(wizard->restoreEditor(localDraft,true,error));wizard->keepProjectEdits(true);wizard->loadOffline(base.pages[0],base.pages[3]);CHECK(!wizard->reviewReady());
        QPushButton *capture=nullptr;for(auto *button:scope->findChildren<QPushButton*>())if(button->text()=="Capture")capture=button;CHECK(capture);capture->click();
        for(int n=0;n<200&&scope->busy();++n)QTest::qWait(20);CHECK(!scope->busy()&&scope->findChild<QLabel*>("triggerScopeStatus")->text().contains("No crank/cam edges"));
        CHECK(wizard->hasCurrentBase(base.pages[0],base.pages[3],base.identity,base.generation));CHECK(wizard->editorState()["capture"].toBool()==false);wizard->next();CHECK(wizard->reviewReady());
        const auto preview=qEnvironmentVariable("TUNUINO_TRIGGER_BOOTSTRAP_PREVIEW");if(!preview.isEmpty()){wizard->resize(720,540);app.processEvents();wizard->grab().save(preview+"-review.png");scope->resize(720,480);scope->grab().save(preview+"-scope.png");}
        session->close();std::cout<<"Real MainWindow serial bootstrap without engine profile passed.\n";return 0;
    }
    auto *sensorTree=window.findChild<QTreeWidget*>("MainNavigation");CHECK(sensorTree);CHECK(sensorTree->topLevelItemCount()==8);
    QStringList roots;for(int i=0;i<sensorTree->topLevelItemCount();++i)roots<<sensorTree->topLevelItem(i)->text(0);
    CHECK(roots==QStringList({"Inputs and outputs","Engine setup","Fueling","Ignition","VVT","ETB","Diagnostics","Logs"}));
    for(QTreeWidgetItemIterator item(sensorTree);*item;++item) CHECK((*item)->flags() & Qt::ItemIsEnabled);
    auto *ioRoot=sensorTree->topLevelItem(0);CHECK(ioRoot->childCount()==2&&!ioRoot->data(0,Qt::UserRole).isValid());
    CHECK(ioRoot->child(0)->text(0)=="ECU Proteus"&&ioRoot->child(0)->child(0)->text(0)=="Proteus Pinout");
    CHECK(ioRoot->child(1)->text(0)=="ECU UAefi"&&ioRoot->child(1)->child(0)->text(0)=="UAefi Pinout");

    const auto openSensor=[&](const QString &key){QTreeWidgetItemIterator sensorItem(sensorTree);while(*sensorItem){if((*sensorItem)->data(0,Qt::UserRole+1).toString()==key){sensorTree->itemClicked(*sensorItem,0);app.processEvents();return;}++sensorItem;}
        auto *central=dynamic_cast<tunuino::IoConfigurationWidget*>(window.findChild<QWidget*>("ioConfiguration"));
        CHECK(central && central->openConfiguration);central->openConfiguration(key);app.processEvents();};
    openSensor("io.configuration");
    CHECK(window.findChild<QMdiArea*>()->activeSubWindow()->widget()->objectName()=="ioConfiguration");
    CHECK(window.findChild<QMdiArea*>()->activeSubWindow()->windowTitle()=="Proteus Pinout");
    CHECK(window.findChild<QMdiArea*>()->activeSubWindow()->property("desktopWindowId")=="Inputs and outputs|ioConfiguration");
    const auto beforeUaefi=project->capture(error);
    auto *proteusSelector=window.findChild<QMdiArea*>()->activeSubWindow()->findChild<QToolButton*>("ecuPinoutSelector");
    CHECK(proteusSelector&&proteusSelector->text().contains("Proteus"));
    proteusSelector->menu()->findChild<QAction*>("pinoutSelectUAefi")->trigger();app.processEvents();
    auto *uaefiWindow=window.findChild<QMdiArea*>()->activeSubWindow();
    CHECK(uaefiWindow->windowTitle()=="UAefi Pinout"&&uaefiWindow->widget()->property("ecuBoard")=="UAefi");
    auto *uaefiTabs=uaefiWindow->findChild<QTabWidget*>("uaefiConnectors");auto *uaefiTable=uaefiWindow->findChild<QTableWidget*>("uaefiPinTable");
    CHECK(uaefiTabs&&uaefiTabs->count()==5&&uaefiTable&&!uaefiWindow->findChild<QTableWidget*>("ioAssignments"));
    const int counts[]={8,18,20,16,6};
    for(int c=0;c<5;++c){uaefiTabs->setCurrentIndex(c);app.processEvents();CHECK(uaefiTable->rowCount()==counts[c]);
        auto *photo=dynamic_cast<tunuino::ConnectorPhotoWidget*>(uaefiTabs->currentWidget());CHECK(photo&&photo->hasPhoto());
        QTest::mouseClick(photo,Qt::LeftButton,Qt::NoModifier,photo->pinCenter(1).toPoint());
        CHECK(uaefiWindow->findChild<QLabel*>("uaefiPinDetail")->text().contains(QString(QChar('A'+c))+"1"));
        CHECK(project->capture(error)==beforeUaefi); // Hardware browsing cannot edit or write the connected Proteus.
        app.processEvents();
        const auto preview=qEnvironmentVariable("TUNUINO_IO_PREVIEW");if(!preview.isEmpty())CHECK(uaefiWindow->grab().save(preview+QString("-UAefi-%1.png").arg(QChar('A'+c))));
    }
    const auto uaefiPreview=qEnvironmentVariable("TUNUINO_IO_PREVIEW");if(!uaefiPreview.isEmpty())CHECK(window.grab().save(uaefiPreview+"-ECU-navigation.png"));
    auto *uaefiSelector=uaefiWindow->findChild<QToolButton*>("ecuPinoutSelector");CHECK(uaefiSelector&&uaefiSelector->text().contains("UAefi"));
    uaefiSelector->menu()->findChild<QAction*>("pinoutSelectProteus")->trigger();app.processEvents();
    CHECK(uaefiWindow->isHidden());CHECK(window.findChild<QMdiArea*>()->activeSubWindow()->windowTitle()=="Proteus Pinout");
    CHECK(project->capture(error)==beforeUaefi);

    auto *ioTable=window.findChild<QTableWidget*>("ioAssignments");
    CHECK(ioTable && ioTable->rowCount() >= 13);
    CHECK(ioTable->item(0,0)->text() != "Ain 1 digital filter");
    auto *centralIo=dynamic_cast<tunuino::IoConfigurationWidget*>(window.findChild<QWidget*>("ioConfiguration"));
    auto *pinout=dynamic_cast<tunuino::ProteusPinoutWidget*>(window.findChild<QWidget*>("proteusPinout"));
    auto *connectorTabs=window.findChild<QTabWidget*>("proteusConnectors");
    auto *pinTable=window.findChild<QTableWidget*>("proteusPinTable");
    CHECK(centralIo&&pinout&&connectorTabs&&connectorTabs->count()==3&&pinTable);
    const auto beforePinBrowse=project->capture(error);
    for(int connector=0;connector<3;++connector){
        connectorTabs->setCurrentIndex(connector);app.processEvents();
        auto *photo=dynamic_cast<tunuino::ProteusConnectorPhoto*>(connectorTabs->currentWidget());
        CHECK(photo&&photo->hasPhoto());
        CHECK(pinTable->rowCount()==(connector==0?23:35));
        if(!qEnvironmentVariable("TUNUINO_IO_PREVIEW").isEmpty()) CHECK(centralIo->grab().save(qEnvironmentVariable("TUNUINO_IO_PREVIEW")+QString("-J10%1.png").arg(connector+1)));
        QTest::mouseClick(photo,Qt::LeftButton,Qt::NoModifier,photo->pinCenter(1).toPoint());
        CHECK(window.findChild<QLabel*>("proteusPinDetail")->text().contains(QString("J10%1/1").arg(connector+1)));
    }
    QTest::qWait(250);CHECK(project->capture(error)==beforePinBrowse);
    CHECK(window.findChild<QLabel*>("proteusPinDetail")->text().contains("J103/1"));
    connectorTabs->setCurrentIndex(0);app.processEvents();
    // A disabled capture still has its dedicated hardware contacts. Configure
    // edits that connection without navigating to trigger/strategy parameters.
    centralIo->refresh();pinout->selectPin(0,5);app.processEvents();
    CHECK(pinTable->item(4,3)->text().contains("Crankshaft sensor")&&pinTable->item(4,3)->text().contains("disabled"));
    const int crankRow=ioTable->currentRow();CHECK(ioTable->item(crankRow,0)->text()=="Crankshaft sensor");
    auto *primary=dynamic_cast<tunuino::ParameterWidget*>(window.findChild<QWidget*>("ignition.primary"));CHECK(primary);
    const auto primaryBefore=primary->editorState(),projectBeforePinDialog=project->capture(error);
    auto *ioActive=window.findChild<QMdiArea*>()->activeSubWindow();
    for(bool applyPin:{false,true}) {
        bool visited=false;
        QTimer::singleShot(0,&window,[&]{
            auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());CHECK(dialog&&dialog->objectName()=="ioPinConfiguration");
            CHECK(window.findChild<QMdiArea*>()->activeSubWindow()==ioActive);
            auto *choice=dialog->findChild<QComboBox*>("ioPinChoice");auto *detail=dialog->findChild<QLabel*>("ioPinConnectionDetail");
            CHECK(choice&&choice->count()==2&&detail&&detail->text().contains("J101/5")&&detail->text().contains("J101/13"));
            CHECK(!dialog->findChild<QPushButton*>("ioPinWrite")->isEnabled());
            choice->setCurrentIndex(1);
            CHECK(primary->editorState()==primaryBefore); // Choosing alone is not Apply or ECU write.
            if(applyPin){dialog->findChild<QPushButton*>("ioPinApply")->click();CHECK(primary->editorState()["values"].toObject()["capture"].toInt()==1);}
            if(!qEnvironmentVariable("TUNUINO_IO_PREVIEW").isEmpty())CHECK(dialog->grab().save(qEnvironmentVariable("TUNUINO_IO_PREVIEW")+(applyPin?"-pin-applied.png":"-pin-dialog.png")));
            visited=true;dialog->reject();
        });
        qobject_cast<QPushButton*>(ioTable->cellWidget(crankRow,2))->click();CHECK(visited);
        if(!applyPin)CHECK(project->capture(error)==projectBeforePinDialog);
    }
    CHECK(primary->restoreEditor(primaryBefore,true,error));centralIo->refresh();
    CHECK(project->capture(error)==projectBeforePinDialog);
    CHECK(pinTable->item(4,3)->text().contains("disabled"));
    pinout->selectPin(0,4);app.processEvents();
    const int camRow=ioTable->currentRow();CHECK(ioTable->item(camRow,0)->text()=="CAM 1 sensor");
    auto *camChoice=qobject_cast<QComboBox*>(ioTable->cellWidget(camRow,1));
    auto *crankChoice=qobject_cast<QComboBox*>(ioTable->cellWidget(crankRow,1));
    CHECK(camChoice&&camChoice->currentText()=="Disabled"&&camChoice->count()==2);
    CHECK(pinTable->item(3,3)->text().contains("CAM 1 sensor")&&pinTable->item(3,3)->text().contains("disabled"));
    CHECK(pinTable->item(11,3)->text().contains("disabled"));
    bool visitedCam=false;
    QTimer::singleShot(0,&window,[&]{
        auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());CHECK(dialog);
        auto *choice=dialog->findChild<QComboBox*>("ioPinChoice");
        CHECK(choice&&choice->currentText()=="Disabled");
        const auto detail=dialog->findChild<QLabel*>("ioPinConnectionDetail")->text();
        CHECK(detail.contains("shared")&&detail.contains("J101/4")&&detail.contains("J101/12"));
        choice->setCurrentIndex(1);CHECK(primary->editorState()==primaryBefore);
        dialog->findChild<QPushButton*>("ioPinApply")->click();visitedCam=true;dialog->reject();
    });
    qobject_cast<QPushButton*>(ioTable->cellWidget(camRow,2))->click();CHECK(visitedCam);
    CHECK(primary->editorState()["values"].toObject()["capture"].toInt()==1);
    centralIo->refresh();CHECK(camChoice->currentIndex()==1&&crankChoice->currentIndex()==1);
    CHECK(pinTable->item(3,3)->text().contains("CAM 1 sensor")&&!pinTable->item(3,3)->text().contains("disabled"));
    CHECK(primary->restoreEditor(primaryBefore,true,error));centralIo->refresh();
    CHECK(camChoice->currentText()=="Disabled"&&crankChoice->currentText()=="Disabled");
    CHECK(project->capture(error)==projectBeforePinDialog);

    if(!qEnvironmentVariable("TUNUINO_IO_PREVIEW").isEmpty()){
        auto *ioWindow=window.findChild<QMdiArea*>()->activeSubWindow();
        app.processEvents();
        CHECK(ioWindow->grab().save(qEnvironmentVariable("TUNUINO_IO_PREVIEW")));
        CHECK(window.grab().save(qEnvironmentVariable("TUNUINO_IO_PREVIEW")+"-workspace.png"));
    }
    openSensor("inputs.analog");auto *inputWindow=window.findChild<QMdiArea*>()->activeSubWindow();CHECK(inputWindow&&inputWindow->windowTitle()=="Sensors and inputs - Analog inputs");inputWindow->close();
    openSensor("inputs.primary");auto *primaryInputWindow=window.findChild<QMdiArea*>()->activeSubWindow();CHECK(primaryInputWindow&&primaryInputWindow->windowTitle()=="Digital inputs - Primary trigger");auto *primaryFields=primaryInputWindow->widget()->findChild<QTableWidget*>("parameters");CHECK(primaryFields&&primaryFields->rowCount()==4&&primaryFields->isRowHidden(0)&&primaryFields->isRowHidden(1)&&primaryFields->isRowHidden(2)&&!primaryFields->isRowHidden(3));primaryInputWindow->close();
    openSensor("inputs.cam1");auto *camInputWindow=window.findChild<QMdiArea*>()->activeSubWindow();CHECK(camInputWindow&&camInputWindow->windowTitle()=="Digital inputs - CAM 1");auto *camFields=camInputWindow->widget()->findChild<QTableWidget*>("parameters");CHECK(camFields&&camFields->rowCount()==3&&camFields->isRowHidden(0)&&camFields->isRowHidden(1)&&!camFields->isRowHidden(2));camInputWindow->close();
    openSensor("inputs.cam2");CHECK(window.findChild<QMdiArea*>()->activeSubWindow()->windowTitle()=="Digital inputs - CAM 2");
    auto *pressureAssignmentEditor=window.findChild<QWidget*>("inputs.pressure");CHECK(pressureAssignmentEditor&&pressureAssignmentEditor->isHidden());
    openSensor("inputs.vss");CHECK(window.findChild<QMdiArea*>()->activeSubWindow()->windowTitle()=="Digital inputs - VSS");openSensor("speed.vehicle");CHECK(window.findChild<QMdiArea*>()->activeSubWindow()->windowTitle()=="VSS and Gearbox - Vehicle speed");
    auto *iat=dynamic_cast<tunuino::SensorCalibrationWidget*>(window.findChild<QWidget*>("sensorIAT"));CHECK(iat);CHECK(iat->findChild<QTableWidget*>("sensorPoints")->isRowHidden(0));const auto beforeWizard=project->capture(error);const QJsonValue oldInput=iat->editorState()["input"];
    openSensor("sensorwizard:IAT");auto *sensorWizard=dynamic_cast<tunuino::SensorCurveWizard*>(window.findChild<QDialog*>("iatSensorWizard"));CHECK(sensorWizard&&sensorWizard->isVisible()&&sensorWizard->isModal());auto *sensorOk=sensorWizard->findChild<QPushButton*>("sensorWizardOk");CHECK(sensorOk&&!sensorOk->isEnabled());
    for(int i=0;i<3;++i)sensorWizard->findChild<QDoubleSpinBox*>(QString("sensorWizardValue%1").arg(i))->setValue(i==1?3950:2500);CHECK(sensorOk->isEnabled());sensorOk->click();CHECK(!sensorWizard->isVisible());CHECK(iat->editorState()["input"]==oldInput);CHECK(project->capture(error)["parameters"]==beforeWizard["parameters"]&&project->capture(error)["pages"]==beforeWizard["pages"]);
    const auto prepared=project->capture(error);openSensor("sensorwizard:IAT");sensorWizard->reject();CHECK(project->capture(error)==prepared);
    openSensor("sensorwizard:Pressure");auto *pressureWizard=dynamic_cast<tunuino::SensorCurveWizard*>(window.findChild<QDialog*>("pressureSensorWizard"));CHECK(pressureWizard&&pressureWizard->isVisible());pressureWizard->findChild<QDoubleSpinBox*>("sensorWizardValue0")->setValue(.5);pressureWizard->findChild<QDoubleSpinBox*>("sensorWizardValue2")->setValue(4.5);pressureWizard->findChild<QDoubleSpinBox*>("sensorWizardValue3")->setValue(300);pressureWizard->findChild<QPushButton*>("sensorWizardOk")->click();CHECK(!pressureWizard->isVisible());
    CHECK(project->capture(error)["sensors"].toObject()["Fuel pressure"].toObject()["volts"].toArray()==QJsonArray({.5,4.5}));
    auto *firing=dynamic_cast<tunuino::ParameterWidget*>(window.findChild<QWidget*>("ignition.firing"));CHECK(firing);CHECK(firing->findChild<QTableWidget*>("engineProfileAssignments")->isColumnHidden(1)&&firing->findChild<QTableWidget*>("engineProfileAssignments")->isColumnHidden(2));firing->findChild<QDoubleSpinBox*>("cylinders")->setValue(6);
    const auto profileBeforeConnection=firing->editorState();
    bool foundInjectorConnection=false;
    for(int row=0;row<ioTable->rowCount();++row)if(ioTable->item(row,0)->text()=="Injector — cylinder 1"){
        auto *connection=qobject_cast<QComboBox*>(ioTable->cellWidget(row,1));CHECK(connection);
        CHECK(connection->isEnabled()&&connection->itemText(0)=="No pin selected"); // Local wiring edits remain available offline.
        CHECK(!qobject_cast<QPushButton*>(ioTable->cellWidget(row,3))->isEnabled());
        connection->setCurrentIndex(1);
        CHECK(QMetaObject::invokeMethod(connection,"activated",Qt::DirectConnection,Q_ARG(int,1)));
        CHECK(firing->editorState()["engineProfile"].toObject()["cylinders"].toArray()[0].toObject()["injector"].toInt()==0);
        centralIo->refresh();ioTable->setCurrentCell(row,0);app.processEvents();
        CHECK(connectorTabs->currentIndex()==1);
        auto *outputPhoto=dynamic_cast<tunuino::ProteusConnectorPhoto*>(connectorTabs->currentWidget());
        CHECK(outputPhoto&&outputPhoto->highlighted().contains(3));
        CHECK(pinTable->item(2,3)->text().contains("Injector"));
        const auto beforePhotoClick=project->capture(error);
        QTest::mouseClick(outputPhoto,Qt::LeftButton,Qt::NoModifier,outputPhoto->pinCenter(3).toPoint());
        CHECK(ioTable->currentRow()==row&&project->capture(error)==beforePhotoClick);
        if(!qEnvironmentVariable("TUNUINO_IO_PREVIEW").isEmpty()) CHECK(centralIo->grab().save(qEnvironmentVariable("TUNUINO_IO_PREVIEW")+"-linked.png"));

        CHECK(firing->restoreEditor(profileBeforeConnection,true,error));
        foundInjectorConnection=true;
    }
    CHECK(foundInjectorConnection);

    auto *tree=window.findChild<QTreeWidget*>("MainNavigation");CHECK(tree);QTreeWidgetItemIterator it(tree);while(*it){if((*it)->data(0,Qt::UserRole+1).toString()=="ignition.firing"){tree->itemClicked(*it,0);break;}++it;}app.processEvents();CHECK(window.findChild<QMdiArea*>()->activeSubWindow()->windowTitle()=="Ignition - Firing order");
    auto data=project->capture(error);CHECK(!data.isEmpty());auto tables=data["tables"].toObject();for(const auto &name:{"fuel","ignition","afr"}){auto t=tables[name].toObject();auto rows=t["cells"].toArray(),row=rows[0].toArray();row[0]=QString(name)=="fuel"?123.45:QString(name)=="ignition"?-12.34:12.8;rows[0]=row;t["cells"]=rows;tables[name]=t;}data["tables"]=tables;
    auto parameters=data["parameters"].toObject();auto general=parameters["fuel.general"].toObject(),values=general["values"].toObject();values["requiredFuel"]=6.75;general["values"]=values;parameters["fuel.general"]=general;data["parameters"]=parameters;data["service"]=QJsonObject{{"comment","Full six-cylinder map"}};
    CHECK(project->apply(data,error));CHECK(project->capture(error)==data);const auto path=temp.path()+"/whole.tnproj";CHECK(project->save(path,error));QFile file(path);CHECK(file.open(QIODevice::ReadOnly));const auto saved=file.readAll();file.close();
    firing->findChild<QDoubleSpinBox*>("cylinders")->setValue(2);CHECK(project->capture(error)!=data);CHECK(project->open(path,error));CHECK(firing->findChild<QDoubleSpinBox*>("cylinders")->value()==6);CHECK(project->capture(error)==data);
    auto bad=data;auto sensors=bad["sensors"].toObject(),tps=sensors["TPS"].toObject();tps["input"]=99;sensors["TPS"]=tps;bad["sensors"]=sensors;const auto corrupt=temp.path()+"/invalid.tnproj";QFile invalid(corrupt);CHECK(invalid.open(QIODevice::WriteOnly));invalid.write(tunuino::MapProject::encode(bad,error));invalid.close();CHECK(!project->open(corrupt,error));CHECK(project->capture(error)==data);
    checkpoint("project round-trip complete; open save dialog");
    auto *save=window.findChild<QAction*>("fileSaveMap");CHECK(save);CHECK(save->text()=="Save Project as...");CHECK(window.findChild<QAction*>("fileOpenMap")->text()=="Open Project...");CHECK(save->shortcut()==QKeySequence::Save);bool dialogChecked=false;
    QTimer::singleShot(100,[&]{for(auto *widget:QApplication::topLevelWidgets())if(auto *dialog=qobject_cast<QFileDialog*>(widget)){CHECK(dialog->nameFilters()==QStringList{"Tunuino project (*.tnproj)"});CHECK(dialog->defaultSuffix()=="tnproj");CHECK(dialog->selectedFiles().value(0).endsWith(".tnproj"));CHECK(!dialog->selectedFiles().value(0).contains("fuel.json"));dialogChecked=true;dialog->reject();}});save->trigger();CHECK(dialogChecked);
    checkpoint("save dialog closed; caption controls");
    auto *mdi=window.findChild<QMdiArea*>();QMdiSubWindow *sub=nullptr;for(auto *candidate:mdi->subWindowList())if(candidate->widget()==firing)sub=candidate;CHECK(sub);sub->showNormal();mdi->setActiveSubWindow(sub);app.processEvents();
    const auto captionClick=[&](QStyle::SubControl control){mdi->setActiveSubWindow(sub);sub->raise();QTest::qWait(40);sub->repaint();QStyleOptionTitleBar option;option.initFrom(sub);const int height=sub->style()->pixelMetric(QStyle::PM_TitleBarHeight,&option,sub);option.rect=QRect(4,4,sub->width()-8,height);option.titleBarFlags=sub->windowFlags();option.titleBarState=sub->windowState();option.subControls=QStyle::SC_All;const auto rect=sub->style()->subControlRect(QStyle::CC_TitleBar,&option,control,sub);CHECK(!rect.isEmpty());mdi->setActiveSubWindow(sub);QTest::mouseMove(sub,QPoint(40,height+40));app.processEvents();QTest::mouseMove(sub,rect.center()+QPoint(-1,0));app.processEvents();QTest::mouseMove(sub,rect.center());app.processEvents();QTest::mouseClick(sub,Qt::LeftButton,Qt::NoModifier,rect.center());QTest::qWait(40);app.processEvents();};
    captionClick(QStyle::SC_TitleBarMaxButton);CHECK(sub->isMaximized());sub->showNormal();app.processEvents();captionClick(QStyle::SC_TitleBarMinButton);CHECK(sub->isMinimized());sub->showNormal();app.processEvents();captionClick(QStyle::SC_TitleBarCloseButton);CHECK(!sub->isVisible());sub->showNormal();CHECK(firing->findChild<QDoubleSpinBox*>("cylinders")->value()==6);
    checkpoint("trigger presets");
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
    const auto beforePressureWizard=project->capture(error);openSensor("sensorwizard:Pressure");pressureWizard->findChild<QComboBox*>("pressureWizardTarget")->setCurrentIndex(1);pressureWizard->findChild<QDoubleSpinBox*>("sensorWizardValue3")->setValue(1000);pressureWizard->findChild<QPushButton*>("sensorWizardOk")->click();CHECK(!pressureWizard->isVisible());const auto pressureDraft=project->capture(error);CHECK(pressureDraft["pages"]==beforePressureWizard["pages"]&&pressureDraft["parameters"]==beforePressureWizard["parameters"]);CHECK(pressureDraft["sensors"].toObject()["Oil pressure"].toObject()["values"].toArray()==QJsonArray({0,1000}));
    auto legacy=pressureDraft;auto six=legacy["sensors"].toObject();six.remove("Fuel pressure");six.remove("Oil pressure");legacy["sensors"]=six;CHECK(project->apply(legacy,error));const auto upgraded=project->capture(error);auto eight=upgraded["sensors"].toObject();CHECK(eight.size()==8);CHECK(eight["Fuel pressure"].toObject()["input"]==0&&eight["Oil pressure"].toObject()["input"]==0);eight.remove("Fuel pressure");eight.remove("Oil pressure");CHECK(eight==six&&upgraded["pages"]==legacy["pages"]);
    auto oldInputs=project->capture(error);auto oldParameters=oldInputs["parameters"].toObject();oldParameters.remove("inputs.analog");oldParameters.remove("inputs.primary");oldParameters.remove("inputs.cam1");oldParameters.remove("inputs.cam2");oldParameters.remove("inputs.pressure");oldParameters.remove("inputs.vss");oldParameters.remove("speed.vehicle");oldInputs["parameters"]=oldParameters;CHECK(project->apply(oldInputs,error));CHECK(project->capture(error)["parameters"].toObject().contains("inputs.analog"));CHECK(project->capture(error)["parameters"].toObject().contains("inputs.primary"));CHECK(project->capture(error)["parameters"].toObject().contains("inputs.cam1"));CHECK(project->capture(error)["parameters"].toObject().contains("inputs.cam2"));CHECK(project->capture(error)["parameters"].toObject().contains("inputs.pressure"));CHECK(project->capture(error)["parameters"].toObject().contains("inputs.vss"));CHECK(project->capture(error)["parameters"].toObject().contains("speed.vehicle"));
    auto badInputs=project->capture(error);auto badPages=badInputs["pages"].toObject();auto badHeader=QByteArray::fromHex(badPages["header"].toString().toLatin1());badHeader[70]=2;badPages["header"]=QString::fromLatin1(badHeader.toHex());badInputs["pages"]=badPages;CHECK(!project->apply(badInputs,error));
    auto badPrimaryProject=project->capture(error);auto primaryPages=badPrimaryProject["pages"].toObject();auto primaryBytes=QByteArray::fromHex(primaryPages["header"].toString().toLatin1());primaryBytes[5]=2;primaryPages["header"]=QString::fromLatin1(primaryBytes.toHex());badPrimaryProject["pages"]=primaryPages;CHECK(!project->apply(badPrimaryProject,error));
    auto badCamProject=project->capture(error);auto camPages=badCamProject["pages"].toObject();auto camBytes=QByteArray::fromHex(camPages["header"].toString().toLatin1());camBytes[98]=2;camPages["header"]=QString::fromLatin1(camBytes.toHex());badCamProject["pages"]=camPages;CHECK(!project->apply(badCamProject,error));
    auto badVssProject=project->capture(error);auto vssPages=badVssProject["pages"].toObject();auto vssBytes=QByteArray::fromHex(vssPages["header"].toString().toLatin1());vssBytes[82]=2;vssPages["header"]=QString::fromLatin1(vssBytes.toHex());badVssProject["pages"]=vssPages;CHECK(!project->apply(badVssProject,error));
    auto badCam2Project=project->capture(error);const auto beforeBadCam2=badCam2Project;auto cam2Pages=badCam2Project["pages"].toObject();auto cam2Bytes=QByteArray::fromHex(cam2Pages["header"].toString().toLatin1());cam2Bytes[57]=2;cam2Pages["header"]=QString::fromLatin1(cam2Bytes.toHex());badCam2Project["pages"]=cam2Pages;CHECK(!project->apply(badCam2Project,error));CHECK(project->capture(error)==beforeBadCam2);
    auto *dbwSetup=dynamic_cast<tunuino::DbwWidget*>(window.findChild<QWidget*>("dbwSignalSetup"));CHECK(dbwSetup && dbwSetup->isHidden());
    CHECK(dbwSetup->findChild<QComboBox*>("dbwInput0")->count()==2);CHECK(!dbwSetup->findChild<QAction*>("dbwCaptureRest")->isEnabled());
    const auto dbwOriginal=project->capture(error);auto withDbw=dbwOriginal;auto dbwState=withDbw["dbw"].toObject();auto dbwRows=dbwState["signals"].toArray();auto dbwRow=dbwRows[0].toObject();dbwRow["input"]=9;dbwRow["rest"]=.5;dbwRow["full"]=4.5;dbwRows[0]=dbwRow;dbwState["signals"]=dbwRows;withDbw["dbw"]=dbwState;
    CHECK(project->applyEdits(withDbw,error));CHECK(project->capture(error)["dbw"]==dbwState);CHECK(project->capture(error)["pages"]==dbwOriginal["pages"]);
    CHECK(dbwSetup->findChild<QComboBox*>("dbwInput0")->count()==3&&dbwSetup->findChild<QComboBox*>("dbwInput0")->currentData()==9);
    auto badDbw=withDbw;auto badDbwState=dbwState;badDbwState["disagreement"]=50;badDbw["dbw"]=badDbwState;CHECK(!project->apply(badDbw,error));CHECK(project->capture(error)["dbw"]==dbwState);
    CHECK(!project->applyEdits(badDbw,error));CHECK(project->capture(error)["dbw"]==dbwState);
    CHECK(project->save(temp.filePath("dbw.tnproj"),error));CHECK(project->applyEdits(dbwOriginal,error));CHECK(project->open(temp.filePath("dbw.tnproj"),error));CHECK(project->capture(error)["dbw"]==dbwState);
    auto legacyDbw=project->capture(error);legacyDbw.remove("dbw");CHECK(project->apply(legacyDbw,error));CHECK(project->capture(error)["dbw"]==tunuino::DbwSignals::defaults());
    CHECK(project->apply(dbwOriginal,error));
    openSensor("vtec.setup");auto *vtecPanel=dynamic_cast<tunuino::ParameterWidget*>(window.findChild<QMdiArea*>()->activeSubWindow()->widget());CHECK(vtecPanel&&vtecPanel->block().page==6);CHECK(!vtecPanel->findChild<QCheckBox*>("enabled")->isChecked());CHECK(vtecPanel->findChild<QComboBox*>("output")->count()==2);CHECK(vtecPanel->findChild<QTableWidget*>("parameters")->isRowHidden(1));
    const auto vtecBeforeAssignment=vtecPanel->editorState();
    bool foundVtecConnection=false;
    for(int row=0;row<ioTable->rowCount();++row)if(ioTable->item(row,0)->text()=="VTEC solenoid"){
        auto *connection=qobject_cast<QComboBox*>(ioTable->cellWidget(row,1));CHECK(connection);
        connection->setCurrentIndex(1);
        CHECK(QMetaObject::invokeMethod(connection,"activated",Qt::DirectConnection,Q_ARG(int,1)));
        CHECK(vtecPanel->editorState()["values"].toObject()["output"].toInt()==1);
        CHECK(vtecPanel->findChild<QTableWidget*>("parameters")->isRowHidden(1));
        CHECK(vtecPanel->restoreEditor(vtecBeforeAssignment,true,error));
        foundVtecConnection=true;
    }
    CHECK(foundVtecConnection);
    if(!qEnvironmentVariable("TUNUINO_VTEC_UI_PREVIEW").isEmpty()){
        app.processEvents();
        CHECK(vtecPanel->grab().save(qEnvironmentVariable("TUNUINO_VTEC_UI_PREVIEW")));
    }
    openSensor("vtec.monitor");CHECK(window.findChild<QMdiArea*>()->activeSubWindow()->widget()->objectName()=="vtecMonitor");
    const auto vtecBase=project->capture(error);auto legacyVtec=vtecBase;auto legacyVtecParams=legacyVtec["parameters"].toObject();legacyVtecParams.remove("vtec.setup");legacyVtec["parameters"]=legacyVtecParams;CHECK(project->apply(legacyVtec,error));CHECK(project->capture(error)["parameters"].toObject()["vtec.setup"].toObject()["values"].toObject()["enabled"]==0);
    CHECK(project->capture(error)["pages"]==vtecBase["pages"]);CHECK(project->apply(vtecBase,error));
    openSensor("vvt.parameters");CHECK(window.findChild<QMdiArea*>()->activeSubWindow()->widget()->findChild<QAction*>("vvtCaptureReference"));
    openSensor("vvt.parameters");auto *vvtActuator=dynamic_cast<tunuino::ParameterWidget*>(window.findChild<QMdiArea*>()->activeSubWindow()->widget());CHECK(vvtActuator&&vvtActuator->block().key=="vvt.parameters"&&vvtActuator->findChild<QComboBox*>("enabled")->currentIndex()==0);for(int row=0;row<vvtActuator->block().fields.size();++row)if(vvtActuator->block().fields[row].key=="output"){
        auto *grid=vvtActuator->findChild<QTableWidget*>("parameters");
        CHECK(grid->isRowHidden(row));
        vvtActuator->findChild<QToolButton*>("vvtAdvanced")->setChecked(true);
        CHECK(grid->isRowHidden(row));
    }
    openSensor("vvt.monitor");CHECK(window.findChild<QMdiArea*>()->activeSubWindow()->widget()->objectName()=="vvtMonitor");
    openSensor("vvt.table");auto *vvtTargetWindow=window.findChild<QMdiArea*>()->activeSubWindow();CHECK(vvtTargetWindow&&vvtTargetWindow->windowTitle()=="CAM 1 - CAM1 angle 1 [°]");
    auto *vvtCells=window.findChild<QTableWidget*>("vvtTargetTable");auto *vvtRpm=window.findChild<QTableWidget*>("vvtTargetRpm");CHECK(vvtCells&&vvtRpm&&vvtCells->rowCount()==16&&vvtCells->columnCount()==16);vvtCells->item(0,0)->setText("-12.34");vvtRpm->item(0,0)->setText("501");
    auto vvtDraft=project->capture(error);CHECK(vvtDraft["tables"].toObject()["vvt"].toObject()["cells"].toArray()[0].toArray()[0].toDouble()==-12.34);CHECK(project->apply(vvtDraft,error));CHECK(project->capture(error)==vvtDraft);
    auto oldVvtMap=vvtDraft;auto oldTables=oldVvtMap["tables"].toObject();oldTables.remove("vvt");oldTables.remove("vvt2");oldVvtMap["tables"]=oldTables;CHECK(project->apply(oldVvtMap,error));CHECK(project->capture(error)["tables"].toObject()["vvt"]==tunuino::MapProject::vvtTableState());
    CHECK(project->apply(vvtDraft,error));
    if(!qEnvironmentVariable("TUNUINO_VVT_MAP_PREVIEW").isEmpty()){vvtTargetWindow->resize(900,620);vvtTargetWindow->move(0,0);app.processEvents();vvtTargetWindow->grab().save(qEnvironmentVariable("TUNUINO_VVT_MAP_PREVIEW"));vvtTargetWindow->resize(620,450);app.processEvents();vvtTargetWindow->grab().save(qEnvironmentVariable("TUNUINO_VVT_MAP_PREVIEW")+"-small.png");}
    auto *vvtSplit=window.findChild<QSplitter*>("cam1Angle1Split");CHECK(vvtSplit&&vvtSplit->count()==2&&!vvtSplit->widget(0)->isHidden()&&!vvtSplit->widget(1)->isHidden());
    openSensor("vvt.table2");auto *secondWindow=window.findChild<QMdiArea*>()->activeSubWindow();CHECK(secondWindow->windowTitle()=="CAM 1 - CAM1 angle 2 [°]");auto *secondCells=window.findChild<QTableWidget*>("vvtTargetTable2");CHECK(secondCells&&secondCells!=vvtCells);secondCells->item(0,0)->setText("15.67");const auto twoTables=project->capture(error);CHECK(twoTables["tables"].toObject()["vvt2"].toObject()["cells"].toArray()[0].toArray()[0].toDouble()==15.67&&twoTables["tables"].toObject()["vvt"]==vvtDraft["tables"].toObject()["vvt"]);CHECK(project->apply(twoTables,error));
    openSensor("vvt.cam2.parameters");auto *cam2Control=dynamic_cast<tunuino::ParameterWidget*>(window.findChild<QMdiArea*>()->activeSubWindow()->widget());CHECK(cam2Control&&cam2Control->block().page==9);CHECK(cam2Control->findChild<QAction*>("vvtCaptureReference"));
    for(int row=0;row<cam2Control->block().fields.size();++row)if(cam2Control->block().fields[row].key=="output"){cam2Control->findChild<QToolButton*>("vvtAdvanced")->setChecked(true);CHECK(cam2Control->findChild<QTableWidget*>("parameters")->isRowHidden(row));}
    openSensor("vvt.cam2.pid");CHECK(window.findChild<QMdiArea*>()->activeSubWindow()->widget()->objectName()=="vvt.cam2.pid");
    for(const auto &key:{"vvt.cam2.table","vvt.cam2.table2"}){openSensor(key);CHECK(window.findChild<QMdiArea*>()->activeSubWindow()->windowTitle().startsWith("CAM 2 -"));}
    auto *cam2First=window.findChild<QTableWidget*>("cam2TargetTable"),*cam2Second=window.findChild<QTableWidget*>("cam2TargetTable2");CHECK(cam2First&&cam2Second&&cam2First!=cam2Second&&cam2First!=vvtCells);cam2First->item(0,0)->setText("18.25");cam2Second->item(0,0)->setText("-3.50");
    const auto fourCams=project->capture(error);CHECK(fourCams["tables"].toObject()["cam2Target1"].toObject()["cells"].toArray()[0].toArray()[0].toDouble()==18.25);CHECK(fourCams["tables"].toObject()["cam2Target2"].toObject()["cells"].toArray()[0].toArray()[0].toDouble()==-3.5);CHECK(project->apply(fourCams,error)&&project->capture(error)==fourCams);
    CHECK(project->save(temp.path()+"/dual-cam.tnproj",error));cam2First->item(0,0)->setText("0.00");CHECK(project->open(temp.path()+"/dual-cam.tnproj",error)&&project->capture(error)==fourCams);
    for(const auto &key:{"inputs.cam1.pulses","inputs.cam2.signal","vvt.cam2.monitor"}){openSensor(key);CHECK(window.findChild<QMdiArea*>()->activeSubWindow());}
    const auto cam2Preview=qEnvironmentVariable("TUNUINO_CAM2_UI_PREVIEW");if(!cam2Preview.isEmpty()){
        openSensor("vvt.cam2.parameters");auto *w=window.findChild<QMdiArea*>()->activeSubWindow();w->resize(680,660);app.processEvents();CHECK(w->grab().save(cam2Preview+"-parameters.png"));
        openSensor("vvt.cam2.pid");w=window.findChild<QMdiArea*>()->activeSubWindow();w->resize(620,390);app.processEvents();CHECK(w->grab().save(cam2Preview+"-pid.png"));
        openSensor("vvt.cam2.table");w=window.findChild<QMdiArea*>()->activeSubWindow();w->resize(900,540);app.processEvents();CHECK(w->grab().save(cam2Preview+"-table.png"));
        CHECK(window.grab().save(cam2Preview+"-workspace.png"));
    }
    for(int channel=1;channel<=2;++channel){const QString prefix=QString("etb.%1.").arg(channel);for(const auto &key:{"setup","sensors","pid","autotune"}){openSensor(prefix+key);CHECK(window.findChild<QMdiArea*>()->activeSubWindow()->windowTitle().startsWith(QString("ETB %1").arg(channel)));}
        openSensor(prefix+"setup");auto *panel=dynamic_cast<tunuino::ParameterWidget*>(window.findChild<QMdiArea*>()->activeSubWindow()->widget());CHECK(panel&&panel->block().page==12);for(int row=0;row<panel->block().fields.size();++row)if(panel->block().fields[row].key=="bridge")CHECK(panel->findChild<QTableWidget*>("parameters")->isRowHidden(row));
        openSensor(prefix+"sensors");panel=dynamic_cast<tunuino::ParameterWidget*>(window.findChild<QMdiArea*>()->activeSubWindow()->widget());CHECK(panel);for(int row=0;row<panel->block().fields.size();++row)if(panel->block().fields[row].key.startsWith("input"))CHECK(panel->findChild<QTableWidget*>("parameters")->isRowHidden(row));CHECK(panel->findChild<QAction*>("etbCaptureRest")&&panel->findChild<QAction*>("etbCaptureFull"));
        openSensor(prefix+"pid");panel=dynamic_cast<tunuino::ParameterWidget*>(window.findChild<QMdiArea*>()->activeSubWindow()->widget());panel->findChild<QDoubleSpinBox*>("kp")->setValue(channel==1?.65:.85);
    }
    const auto etbProject=project->capture(error);CHECK(etbProject["parameters"].toObject()["etb.1.pid"].toObject()["values"].toObject()["kp"].toDouble()==.65);CHECK(etbProject["parameters"].toObject()["etb.2.pid"].toObject()["values"].toObject()["kp"].toDouble()==.85);CHECK(project->save(temp.path()+"/etb.tnproj",error)&&project->open(temp.path()+"/etb.tnproj",error)&&project->capture(error)==etbProject);
    const auto etbPreview=qEnvironmentVariable("TUNUINO_ETB_UI_PREVIEW");if(!etbPreview.isEmpty()){
        for(const auto &key:{"setup","sensors","pid","autotune"}){openSensor(QString("etb.1.")+key);auto *w=window.findChild<QMdiArea*>()->activeSubWindow();w->resize(QString(key)=="autotune"?850:650,QString(key)=="autotune"?500:QString(key)=="setup"?590:420);app.processEvents();CHECK(w->grab().save(etbPreview+"-"+key+".png"));}
        for(int i=0;i<sensorTree->topLevelItemCount();++i){auto *root=sensorTree->topLevelItem(i);root->setExpanded(root->text(0)=="ETB");if(root->text(0)=="ETB")for(int j=0;j<root->childCount();++j)root->child(j)->setExpanded(true);}app.processEvents();CHECK(window.grab().save(etbPreview+"-workspace.png"));
    }
    if(!qEnvironmentVariable("TUNUINO_CAM_REFERENCE_PREVIEW").isEmpty()){openSensor("vvt.parameters");auto *w=window.findChild<QMdiArea*>()->activeSubWindow();w->resize(620,620);app.processEvents();w->grab().save(qEnvironmentVariable("TUNUINO_CAM_REFERENCE_PREVIEW")+"-parameters.png");openSensor("vvt.pid");w=window.findChild<QMdiArea*>()->activeSubWindow();w->resize(620,390);app.processEvents();w->grab().save(qEnvironmentVariable("TUNUINO_CAM_REFERENCE_PREVIEW")+"-pid.png");openSensor("vvt.table");vvtTargetWindow->resize(1000,540);app.processEvents();vvtTargetWindow->grab().save(qEnvironmentVariable("TUNUINO_CAM_REFERENCE_PREVIEW")+"-table.png");}
    const auto beforeVvt=project->capture(error);auto legacyVvt=beforeVvt;auto legacyVvtParams=legacyVvt["parameters"].toObject();for(const auto &key:legacyVvtParams.keys())if(key.startsWith("vvt."))legacyVvtParams.remove(key);legacyVvt["parameters"]=legacyVvtParams;CHECK(project->apply(legacyVvt,error));CHECK(project->capture(error)["pages"]==beforeVvt["pages"]);CHECK(project->capture(error)["parameters"].toObject()["vvt.parameters"].toObject()["values"].toObject()["enabled"]==0);CHECK(project->apply(beforeVvt,error));
    checkpoint("project validation complete; desktop and menus");
    // Reference menus operate on layout/history only; ECU/project data stays intact.
    QStringList menuNames;for(auto *action:window.menuBar()->actions())menuNames<<action->text();CHECK(menuNames==QStringList({"File","Edit","Desktops","Tools","Window","Help","ECU"}));
    const auto menuItems=[&](const QString &name){QStringList labels;auto *menu=window.findChild<QMenu*>(name);CHECK(menu);for(auto *action:menu->actions())if(!action->isSeparator())labels<<action->text();return labels;};
    CHECK(menuItems("EditMenu")==QStringList({"Undo","Redo","Show undo list..","Show/hide Tree view panel","Show/hide Event log"}));CHECK(menuItems("WindowMenu")==QStringList({"Tile Horizontally","Tile Vertically","Next","Previous","Close all windows"}));
    auto *desktops=dynamic_cast<tunuino::DesktopController*>(window.findChild<QObject*>("desktopController"));CHECK(desktops);const auto projectBeforeMenus=project->capture(error);const auto originalLayout=desktops->document();CHECK(window.findChild<QTabBar*>("desktopTabs")->count()==6);CHECK(window.findChild<QAction*>("desktopNext")->shortcut()==QKeySequence("Ctrl+Shift+P"));
    auto legacyDesktopDocument=QJsonDocument::fromJson(originalLayout).object();
    auto legacyDesktops=legacyDesktopDocument["desktops"].toArray();
    legacyDesktops.append(QJsonObject{{"name","Extra 1"},{"windows",QJsonArray{}}});
    auto retainedDesktop=legacyDesktops[0].toObject();
    CHECK(!retainedDesktop["windows"].toArray().isEmpty());
    retainedDesktop["name"]="Extra 2";
    legacyDesktops.append(retainedDesktop);
    legacyDesktopDocument["desktops"]=legacyDesktops;
    legacyDesktopDocument["active"]=7;
    CHECK(desktops->applyDocument(QJsonDocument(legacyDesktopDocument).toJson(),error));
    CHECK(window.findChild<QTabBar*>("desktopTabs")->count()==7 && desktops->currentDesktop()==6);
    CHECK(desktops->applyDocument(originalLayout,error));
    auto *desktopKey=window.findChild<QAction*>("desktopSwitch0");desktopKey->setShortcut(QKeySequence("Ctrl+Alt+1"));desktops->store();window.findChild<QAction*>("desktopNext")->trigger();CHECK(window.findChild<QAction*>("desktopSwitch0")==desktopKey&&desktopKey->shortcut()==QKeySequence("Ctrl+Alt+1"));CHECK(desktops->currentDesktop()==1);for(auto *w:mdi->subWindowList())CHECK(w->isHidden());openSensor("ignition.firing");CHECK(firing->findChild<QDoubleSpinBox*>("cylinders")->value()==6);auto *deskWindow=mdi->activeSubWindow();deskWindow->setGeometry(25,30,680,500);const auto deskRect=deskWindow->geometry();window.findChild<QAction*>("desktopPrevious")->trigger();CHECK(desktops->currentDesktop()==0);window.findChild<QAction*>("desktopNext")->trigger();CHECK(deskWindow->geometry()==deskRect&&!deskWindow->isHidden());CHECK(project->capture(error)==projectBeforeMenus);
    CHECK(desktops->restore(error));CHECK(desktops->currentDesktop()==0);auto brokenLayout=QJsonDocument::fromJson(originalLayout).object();brokenLayout["active"]=99;CHECK(!desktops->applyDocument(QJsonDocument(brokenLayout).toJson(),error));CHECK(desktops->currentDesktop()==0);
    openSensor("ignition.firing");openSensor("inputs.primary");window.findChild<QAction*>("windowHorizontal")->trigger();CHECK(mdi->activeSubWindow()->width()>300);window.findChild<QAction*>("windowVertical")->trigger();CHECK(mdi->activeSubWindow()->height()>100);window.findChild<QAction*>("windowCloseAll")->trigger();for(auto *w:mdi->subWindowList())CHECK(w->isHidden());CHECK(project->capture(error)==projectBeforeMenus);
    auto *navDock=window.findChild<QDockWidget*>("NavigationDock");CHECK(navDock&&!navDock->isHidden());window.findChild<QAction*>("editTree")->trigger();CHECK(navDock->isHidden());window.findChild<QAction*>("editTree")->trigger();CHECK(!navDock->isHidden());auto *events=window.findChild<QDockWidget*>("EventLogDock");window.findChild<QAction*>("editEvents")->trigger();CHECK(!events->isHidden());window.findChild<QAction*>("editEvents")->trigger();
    window.findChild<QAction*>("showPinout")->trigger();auto *helpBrowser=window.findChild<QTextBrowser*>("generalHelpBrowser");CHECK(helpBrowser&&helpBrowser->toPlainText().contains("J101 / 10")&&helpBrowser->toPlainText().contains("IGN12"));helpBrowser->window()->close();openSensor("ignition.firing");window.findChild<QAction*>("contextHelp")->trigger();CHECK(!window.findChild<QDockWidget*>("ContextHelpDock")->isHidden());window.findChild<QAction*>("contextHelp")->trigger();
    QMdiSubWindow *fuelWindow=nullptr;for(auto *w:mdi->subWindowList())if(w->windowTitle()=="Fuel Table")fuelWindow=w;CHECK(fuelWindow);fuelWindow->showNormal();mdi->setActiveSubWindow(fuelWindow);QTableWidget *fuelGrid=nullptr;for(auto *grid:fuelWindow->findChildren<QTableWidget*>())if(grid->rowCount()==16&&grid->columnCount()==16)fuelGrid=grid;CHECK(fuelGrid);const auto oldCell=fuelGrid->item(0,0)->text();fuelGrid->item(0,0)->setText("132.25");app.processEvents();QTest::qWait(180);auto *editUndo=window.findChild<QAction*>("editUndo");CHECK(editUndo&&editUndo->isEnabled());editUndo->trigger();CHECK(fuelGrid->item(0,0)->text()==oldCell);QTest::qWait(180);window.findChild<QAction*>("editRedo")->trigger();CHECK(fuelGrid->item(0,0)->text()=="132.25");window.findChild<QAction*>("editUndo")->trigger();window.findChild<QAction*>("editHistory")->trigger();CHECK(window.findChild<QDialog*>("undoHistoryDialog"));window.findChild<QDialog*>("undoHistoryDialog")->close();CHECK(project->capture(error)==projectBeforeMenus);CHECK(desktops->applyDocument(originalLayout,error));
    if(!qEnvironmentVariable("TUNUINO_MENU_PREVIEW").isEmpty()){for(auto *action:window.menuBar()->actions())if(QStringList({"Edit","Desktops","Window","Help"}).contains(action->text())){auto *menu=action->menu();menu->popup(window.menuBar()->mapToGlobal(window.menuBar()->actionGeometry(action).bottomLeft()));app.processEvents();CHECK(menu->grab().save(qEnvironmentVariable("TUNUINO_MENU_PREVIEW")+"-"+action->text()+".png"));menu->hide();}}
    checkpoint("CAM sensitivity and monitors");
    auto *sensitivity=dynamic_cast<tunuino::SensitivityCurve*>(window.findChild<QWidget*>("cam1Sensitivity"));CHECK(sensitivity && sensitivity->isHidden());auto curveProject=project->capture(error);const auto oldPages=curveProject["pages"];auto curve=sensitivity->state();auto volts=curve["volts"].toArray();volts[1]=.60;curve["volts"]=volts;CHECK(sensitivity->restore(curve,true,error));auto changedCurve=project->capture(error);CHECK(changedCurve["pages"]==oldPages&&changedCurve["curves"]!=curveProject["curves"]);CHECK(project->apply(curveProject,error));CHECK(sensitivity->state()["volts"].toArray()[1].toDouble()==.53);auto invalidCurve=curve;auto invalidRpm=curve["rpm"].toArray();invalidRpm[1]=750;invalidCurve["rpm"]=invalidRpm;CHECK(!sensitivity->restore(invalidCurve,true,error));
    openSensor("inputs.cam1.monitor");CHECK(mdi->activeSubWindow()->windowTitle()=="CAM 1 - Signal and phase");openSensor("inputs.pressure.monitor");CHECK(mdi->activeSubWindow()->windowTitle()=="Pressure sensors");
    auto *toolbar=window.findChild<QToolBar*>("MainToolbar");CHECK(toolbar&&toolbar->toolButtonStyle()==Qt::ToolButtonIconOnly&&toolbar->iconSize()==QSize(24,24));
    QStringList shortcuts;for(auto *action:toolbar->actions())if(!action->isSeparator()){
        shortcuts<<action->text();CHECK(!action->icon().isNull());
        auto *button=qobject_cast<QToolButton*>(toolbar->widgetForAction(action));
        CHECK(button&&button->defaultAction()==action&&button->toolButtonStyle()==Qt::ToolButtonIconOnly&&!button->toolTip().isEmpty());
        CHECK(button->sizeHint().width()<40);
        bool shared=false;for(auto *menuAction:window.menuBar()->actions())if(menuAction->menu()&&menuAction->menu()->actions().contains(action))shared=true;CHECK(shared);
    }
    CHECK(shortcuts==QStringList({"Write to ECU","Open Project...","Save Project as...","Restore desktops","Store desktops","Test outputs","Show assigned outputs","Show assigned inputs","Customize keys","Navigation sections...","Logbook","Autotune","General options","Scope","Graph Log","Tune Display"}));
    CHECK(!toolbar->actions().first()->isEnabled());
    if(!qEnvironmentVariable("TUNUINO_TOOLBAR_PREVIEW").isEmpty()){
        QToolBar preview;preview.setToolButtonStyle(toolbar->toolButtonStyle());preview.setIconSize(toolbar->iconSize());preview.setMovable(false);preview.setStyleSheet(toolbar->styleSheet());for(auto *action:toolbar->actions())preview.addAction(action);preview.resize(720,36);preview.show();app.processEvents();CHECK(preview.grab().save(qEnvironmentVariable("TUNUINO_TOOLBAR_PREVIEW")));
    }
    checkpoint("general options persistence");
    tunuino::GeneralOptionsDialog optionsDialog(&window);CHECK(!optionsDialog.findChild<QPushButton*>("Monitors and panels..."));CHECK(optionsDialog.findChild<QComboBox*>("appearance")&&optionsDialog.findChild<QComboBox*>("gaugeType"));CHECK(!optionsDialog.findChild<QComboBox*>("rtaAlgorithm")&&!optionsDialog.findChild<QCheckBox*>("loadY"));optionsDialog.findChild<QCheckBox*>("toolbar")->setChecked(false);optionsDialog.findChild<QComboBox*>("gaugeType")->setCurrentIndex(2);optionsDialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();CHECK(!tunuino::enabledOption("toolbar",true)&&tunuino::option("gaugeType",1)==2);QSettings().setValue("general/toolbar",true);QSettings().setValue("general/gaugeType",0);
    if(!qEnvironmentVariable("TUNUINO_OPTIONS_PREVIEW").isEmpty()){tunuino::GeneralOptionsDialog optionsPreview;optionsPreview.resize(590,1040);optionsPreview.show();app.processEvents();CHECK(optionsPreview.grab().save(qEnvironmentVariable("TUNUINO_OPTIONS_PREVIEW")+"-options.png"));optionsPreview.hide();tunuino::SensitivityCurve curvePreview;CHECK(curvePreview.restore(sensitivity->state(),true,error));curvePreview.resize(800,410);curvePreview.show();app.processEvents();CHECK(curvePreview.grab().save(qEnvironmentVariable("TUNUINO_OPTIONS_PREVIEW")+"-sensitivity.png"));}
    checkpoint("general options menu and toolbar");
    QAction *generalAction=nullptr;for(auto *action:window.findChildren<QAction*>())if(action->text()=="General options")generalAction=action;CHECK(generalAction);QTimer::singleShot(0,&window,[&]{auto *dialog=dynamic_cast<tunuino::GeneralOptionsDialog*>(QApplication::activeModalWidget());CHECK(dialog);dialog->findChild<QCheckBox*>("toolbar")->setChecked(false);dialog->findChild<QDoubleSpinBox*>("panelSize")->setValue(310);dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();});generalAction->trigger();CHECK(window.findChild<QToolBar*>("MainToolbar")->isHidden());QTimer::singleShot(0,&window,[&]{auto *dialog=dynamic_cast<tunuino::GeneralOptionsDialog*>(QApplication::activeModalWidget());CHECK(dialog);dialog->findChild<QCheckBox*>("toolbar")->setChecked(true);dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();});generalAction->trigger();CHECK(!window.findChild<QToolBar*>("MainToolbar")->isHidden());
    // Fueling always offers parameters first; Cancel and OK both reveal the review.
    auto *learner=window.findChild<QWidget*>("autotuneReview");CHECK(learner);QMdiSubWindow *learnerWindow=nullptr;for(auto *w:mdi->subWindowList())if(w->widget()==learner)learnerWindow=w;CHECK(learnerWindow);learnerWindow->hide();
    for(bool accept:{false,true}){QTimer::singleShot(0,&window,[&,accept]{auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());CHECK(dialog&&dialog->objectName()=="autotuneSetup");dialog->findChild<QDialogButtonBox*>()->button(accept?QDialogButtonBox::Ok:QDialogButtonBox::Cancel)->click();});QTreeWidgetItemIterator entry(tree);bool opened=false;while(*entry){if((*entry)->text(0)=="Autotune"&&(*entry)->parent()&&(*entry)->parent()->text(0)=="Fueling"){tree->itemClicked(*entry,0);opened=true;break;}++entry;}CHECK(opened&&learnerWindow->isVisible()&&mdi->activeSubWindow()==learnerWindow&&learnerWindow->windowTitle()=="Autotune - PROPOSED VE");learnerWindow->hide();}
    CHECK(project->capture(error)==curveProject);CHECK(!learner->findChild<QAction*>("autotuneApply100")->isEnabled());
    checkpoint("project comparator and analysis navigation");
    openSensor("analysis.text");CHECK(mdi->activeSubWindow()->windowTitle()=="Text Log");openSensor("analysis.scatter");CHECK(mdi->activeSubWindow()->windowTitle()=="Scatter Plot");
    window.findChild<QAction*>("fileCompareProject")->trigger();CHECK(mdi->activeSubWindow()->windowTitle()=="Project Comparator");auto *comparator=dynamic_cast<tunuino::ProjectComparator*>(window.findChild<QWidget*>("projectComparator"));CHECK(comparator);
    const auto compareBase=project->capture(error);auto source=compareBase;auto sourceTables=source["tables"].toObject(),sourceFuel=sourceTables["fuel"].toObject();auto sourceRows=sourceFuel["cells"].toArray(),sourceRow=sourceRows[0].toArray();sourceRow[0]=141.25;sourceRows[0]=sourceRow;sourceFuel["cells"]=sourceRows;sourceTables["fuel"]=sourceFuel;source["tables"]=sourceTables;source["service"]=QJsonObject{{"comment","Comparator import"}};auto sourceParams=source["parameters"].toObject(),sourceGeneral=sourceParams["fuel.general"].toObject(),sourceValues=sourceGeneral["values"].toObject();sourceValues["requiredFuel"]=7.;sourceGeneral["values"]=sourceValues;sourceParams["fuel.general"]=sourceGeneral;source["parameters"]=sourceParams;
    QFile compareFile(temp.path()+"/compare.tnproj");CHECK(compareFile.open(QIODevice::WriteOnly));compareFile.write(tunuino::MapProject::encode(source,error));compareFile.close();CHECK(comparator->load(compareFile.fileName(),error));auto *compTables=comparator->findChild<QTableWidget*>("compareTables"),*compVars=comparator->findChild<QTableWidget*>("compareVariables");CHECK(compTables->rowCount()==1&&compVars->rowCount()==2);compTables->selectRow(0);CHECK(comparator->applySelected(error));auto afterTable=project->capture(error);CHECK(afterTable["tables"]==source["tables"]&&afterTable["pages"]==compareBase["pages"]&&afterTable["parameters"]==compareBase["parameters"]&&afterTable["service"]==compareBase["service"]);
    compVars->selectAll();auto manual=afterTable;manual["service"]=QJsonObject{{"comment","Manual local edit"}};CHECK(project->applyEdits(manual,error));CHECK(!comparator->applySelected(error));CHECK(project->capture(error)==manual);CHECK(comparator->compare(error));compVars->selectAll();CHECK(comparator->applySelected(error));CHECK(project->capture(error)["parameters"]==source["parameters"]&&project->capture(error)["service"]==source["service"]&&project->capture(error)["pages"]==compareBase["pages"]);
    auto invalidSelection=project->capture(error);auto invalidTables=invalidSelection["tables"].toObject(),invalidFuel=invalidTables["fuel"].toObject();auto invalidRows=invalidFuel["cells"].toArray(),invalidRow=invalidRows[0].toArray();invalidRow[0]=9999;invalidRows[0]=invalidRow;invalidFuel["cells"]=invalidRows;invalidTables["fuel"]=invalidFuel;invalidSelection["tables"]=invalidTables;const auto beforeInvalid=project->capture(error);CHECK(!project->applyEdits(invalidSelection,error));CHECK(project->capture(error)==beforeInvalid);
    auto axisSource=source;auto axisTables=axisSource["tables"].toObject(),axisFuel=axisTables["fuel"].toObject();auto axisRpm=axisFuel["rpm"].toArray();axisRpm[0]=axisRpm[0].toDouble()+1;axisFuel["rpm"]=axisRpm;auto axisRows=axisFuel["cells"].toArray(),axisRow=axisRows[0].toArray();axisRow[0]=142.25;axisRows[0]=axisRow;axisFuel["cells"]=axisRows;axisTables["fuel"]=axisFuel;axisSource["tables"]=axisTables;CHECK(compareFile.open(QIODevice::WriteOnly|QIODevice::Truncate));compareFile.write(tunuino::MapProject::encode(axisSource,error));compareFile.close();CHECK(comparator->load(compareFile.fileName(),error));CHECK(compTables->rowCount()==2);for(int r=0;r<compTables->rowCount();++r)if(compTables->item(r,0)->text().contains("Cell"))compTables->selectRow(r);const auto beforeAxis=project->capture(error);CHECK(!comparator->applySelected(error));CHECK(project->capture(error)==beforeAxis);compTables->selectAll();CHECK(comparator->applySelected(error));CHECK(project->capture(error)["tables"]==axisSource["tables"]&&project->capture(error)["pages"]==compareBase["pages"]);
    CHECK(project->apply(compareBase,error));
    if(!qEnvironmentVariable("TUNUINO_ANALYSIS_PREVIEW").isEmpty()){tunuino::ProjectComparator preview(project);CHECK(preview.load(compareFile.fileName(),error));preview.resize(1000,500);preview.show();app.processEvents();CHECK(preview.grab().save(qEnvironmentVariable("TUNUINO_ANALYSIS_PREVIEW")+"-compare.png"));}
    checkpoint("render native map windows");
    for(const auto &title:{QString("Fuel Table"),QString("AFR Target")}){
        QTreeWidgetItemIterator entry(tree);bool opened=false;
        while(*entry){if((*entry)->text(0)==title){tree->itemClicked(*entry,0);opened=true;break;}++entry;}
        CHECK(opened);app.processEvents();auto *mapWindow=mdi->activeSubWindow();CHECK(mapWindow&&mapWindow->windowTitle()==title);
        mapWindow->resize(900,620);mapWindow->move(0,0);app.processEvents();CHECK(!mapWindow->grab().isNull());
        if(title=="AFR Target"){
            const auto state=project->capture(error)["tables"].toObject()["afr"].toObject();const auto points=state["rpm"].toArray();for(int i=1;i<points.size();++i)CHECK(points[i].toDouble()>points[i-1].toDouble());
            bool editableAxis=false;for(auto *grid:mapWindow->findChildren<QTableWidget*>())if(grid->rowCount()==1&&grid->columnCount()==16)editableAxis=grid->editTriggers().testFlag(QAbstractItemView::DoubleClicked);CHECK(editableAxis);
            if(!qEnvironmentVariable("TUNUINO_AFR_PREVIEW").isEmpty())CHECK(mapWindow->grab().save(qEnvironmentVariable("TUNUINO_AFR_PREVIEW")));
        }
        mapWindow->close();mapWindow->showNormal();app.processEvents();CHECK(!mapWindow->grab().isNull());mapWindow->hide();
    }
    checkpoint("USB-only runtime connection (simulated transport replies)");
    auto *session=window.findChild<tunuino::SerialSession*>();
    auto *badge=window.findChild<QLabel*>("EcuConnectionBadge");
    auto *connectionStatus=window.findChild<QLabel*>("EcuConnectionStatus");
    CHECK(session&&badge&&connectionStatus&&badge->text()=="OFFLINE");
    QByteArray identity(32,0);identity.replace(1,7,"TUNUINO");
    identity[8]=1;identity[9]=1;identity[10]=1;identity[12]=7;identity[20]=42;
    session->responseReceived(identity);
    CHECK(badge->text()=="OFFLINE"); // Identity alone cannot establish runtime communication.
    QByteArray usbRuntime(101,0);usbRuntime.replace(0,32,identity);usbRuntime[60]=1;
    session->responseReceived(usbRuntime); // ADC unavailable must not block communication.
    CHECK(badge->text()=="ONLINE"&&connectionStatus->text().contains("USB bench"));
    usbRuntime[40]=1; // ADC valid, external supply reads zero volts.
    session->responseReceived(usbRuntime);
    CHECK(badge->text()=="ONLINE"&&connectionStatus->text().contains("USB bench"));
    auto poweredRuntime=usbRuntime;const unsigned supplyRaw=1619;
    poweredRuntime[83]=char(supplyRaw>>8);poweredRuntime[84]=char(supplyRaw);
    session->responseReceived(poweredRuntime);QTest::qWait(300);session->responseReceived(poweredRuntime);
    CHECK(badge->text()=="ONLINE"&&!connectionStatus->text().contains("USB bench"));
    session->responseReceived(usbRuntime);
    CHECK(badge->text()=="ONLINE"&&connectionStatus->text().contains("USB bench"));
    session->connectionChanged(false);
    CHECK(badge->text()=="OFFLINE"); // Actual transport loss still disconnects the UI.
    checkpoint("all checks complete");
    std::cout<<"Real File/Save Project as from Firing order: full 6-cylinder map round-trip, all three tables, parameters, comment, atomic rejection and complete-project dialog and caption maximize/minimize/close/reopen passed offline\n"<<std::flush;
    std::_Exit(0);
}
