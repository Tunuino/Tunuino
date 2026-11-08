#pragma once
#include <QMainWindow>
#include "../ui/GeneralOptions.h"
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMenuBar>
#include <QMenu>
#include <QTabBar>
#include <QTreeWidget>
#include <QDockWidget>
#include <QPlainTextEdit>
#include <QTextBrowser>
#include <QDialog>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>
#include <QInputDialog>
#include <QStatusBar>
#include <QTimer>
#include <QPointer>
#include <QMessageBox>
#include <QApplication>
#include <QActionGroup>
#include <QLineEdit>
#include <QScrollBar>
#include <QSet>
#include <cmath>
#include <functional>
namespace tunuino {
// Layout persistence contains window identities/rectangles only, never ECU data.
class DesktopController final:public QObject {
public:
 std::function<QMdiSubWindow*(const QString&)> ensureWindow;
 std::function<QString()> contextHelp;
 std::function<QByteArray()> identitySupplier;
 DesktopController(QMainWindow *owner,QMdiArea *area,QDockWidget *treeDock,QTreeWidget *tree,QMenu *edit,QMenu *desktops,QMenu *windows,QMenu *help):QObject(owner),owner_(owner),area_(area),treeDock_(treeDock),tree_(tree){
  auto *deviceTimer=new QTimer(this);deviceTimer->setInterval(1000);connect(deviceTimer,&QTimer::timeout,this,[this]{if(!identitySupplier)return;const auto next=enabledOption("loadDesktops",true)?identitySupplier():QByteArray{};if(next==uid_)return;store();uid_=next;QString error;const auto saved=QSettings().value(storageKey()).toByteArray();if(!saved.isEmpty())applyDocument(saved,error);});deviceTimer->start();
  setObjectName("desktopController");auto *host=new QWidget(owner);auto *layout=new QVBoxLayout(host);layout->setContentsMargins(0,0,0,0);layout->setSpacing(0);tabs_=new QTabBar(host);tabs_->setObjectName("desktopTabs");tabs_->setExpanding(false);tabs_->setUsesScrollButtons(true);tabs_->setStyleSheet("QTabBar::tab{background:#666764;color:#efefea;padding:5px 20px;border:1px solid #777;}QTabBar::tab:selected{background:#078bda;color:white;}");layout->addWidget(tabs_);owner->takeCentralWidget();layout->addWidget(area);owner->setCentralWidget(host);
  const QStringList names{"logs","cranking","triggers","ignition","fuel","idle","boost","Extra 1","Extra 2","Extra 3","Extra 4","Extra 5","Extra 6","Extra 7","Extra 8","Extra 9","Extra 10","Extra 11"};for(const auto &name:names)states_.append(QJsonObject{{"name",name},{"windows",QJsonArray{}}});rebuildTabs();
  connect(tabs_,&QTabBar::currentChanged,this,[this](int index){if(!applying_)switchTo(index);});tabs_->setContextMenuPolicy(Qt::CustomContextMenu);connect(tabs_,&QTabBar::customContextMenuRequested,this,[this](const QPoint &point){QMenu menu;auto *rename=menu.addAction(tr("Rename active desktop"));if(menu.exec(tabs_->mapToGlobal(point))==rename){bool ok=false;auto name=QInputDialog::getText(owner_,tr("Rename active desktop"),tr("Name"),QLineEdit::Normal,states_[active_].toObject()["name"].toString(),&ok).trimmed();if(ok&&!name.isEmpty()&&name.size()<=64){auto state=states_[active_].toObject();state["name"]=name;states_[active_]=state;rebuildTabs();}}});
  eventDock_=new QDockWidget(tr("Event log"),owner);eventDock_->setObjectName("EventLogDock");events_=new QPlainTextEdit(eventDock_);events_->setReadOnly(true);events_->setMaximumBlockCount(2000);eventDock_->setWidget(events_);owner->addDockWidget(Qt::BottomDockWidgetArea,eventDock_);eventDock_->hide();connect(owner->statusBar(),&QStatusBar::messageChanged,this,[this](const QString &text){if(!text.isEmpty())events_->appendPlainText(text);});
  helpDock_=new QDockWidget(tr("Help"),owner);helpDock_->setObjectName("ContextHelpDock");context_=new QTextBrowser(helpDock_);helpDock_->setWidget(context_);owner->addDockWidget(Qt::RightDockWidgetArea,helpDock_);helpDock_->hide();
  add(edit,tr("Show/hide Tree view panel"),"editTree","F9",[this]{treeDock_->setVisible(treeDock_->isHidden());});add(edit,tr("Show/hide Event log"),"editEvents","Shift+F9",[this]{eventDock_->setVisible(eventDock_->isHidden());});
  add(desktops,tr("Restore desktops"),"desktopRestore",{},[this]{QString error;if(!restore(error))QMessageBox::information(owner_,tr("Restore desktops"),error);});add(desktops,tr("Store desktops"),"desktopStore",{},[this]{store();});desktops->addSeparator();
  add(desktops,tr("Open desktops template..."),"desktopOpen",{},[this]{auto path=QFileDialog::getOpenFileName(owner_,tr("Open desktops template"),{},tr("Tunuino desktops (*.tnlayout)"));if(path.isEmpty())return;QFile file(path);QString error;if(!file.open(QIODevice::ReadOnly)||file.size()>1024*1024||!applyDocument(file.readAll(),error))QMessageBox::warning(owner_,tr("Desktop template"),error.isEmpty()?tr("Could not read template."):error);});
  add(desktops,tr("Save desktops template..."),"desktopSave",{},[this]{auto path=QFileDialog::getSaveFileName(owner_,tr("Save desktops template"),{},tr("Tunuino desktops (*.tnlayout)"));if(path.isEmpty())return;if(!path.endsWith(".tnlayout",Qt::CaseInsensitive))path+=".tnlayout";QSaveFile file(path);auto bytes=document();if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())QMessageBox::warning(owner_,tr("Desktop template"),tr("Could not save template."));});desktops->addSeparator();
  add(desktops,tr("Next desktop"),"desktopNext","Ctrl+Shift+P",[this]{switchTo((active_+1)%states_.size());});add(desktops,tr("Previous desktop"),"desktopPrevious","Ctrl+Shift+O",[this]{switchTo((active_+states_.size()-1)%states_.size());});desktops->addSeparator();switchMenu_=desktops->addMenu(tr("Switch desktop to..."));rebuildSwitchMenu();
  auto *focus=add(desktops,tr("Switch Tree view panel / windows"),"desktopFocus","Tab",[this]{auto *f=QApplication::focusWidget();if(f&&(f==tree_||tree_->isAncestorOf(f))){if(auto *w=area_->activeSubWindow())w->widget()->setFocus();else area_->setFocus();}else{treeDock_->show();tree_->setFocus();}});focus->setShortcutContext(Qt::WindowShortcut);
  add(desktops,tr("Collapse all items in Tree view panel"),"desktopCollapse","Ctrl+\\",[this]{tree_->collapseAll();});
  add(windows,tr("Tile Horizontally"),"windowHorizontal",{},[this]{tile(true);});add(windows,tr("Tile Vertically"),"windowVertical",{},[this]{tile(false);});windows->addSeparator();add(windows,tr("Next"),"windowNext",{},[this]{area_->activateNextSubWindow();});add(windows,tr("Previous"),"windowPrevious",{},[this]{area_->activatePreviousSubWindow();});add(windows,tr("Close all windows"),"windowCloseAll","Ctrl+Shift+C",[this]{for(auto *w:area_->subWindowList())if(!w->isHidden())w->close();});
  add(help,tr("General help"),"generalHelp","F1",[this]{showHelp(false);});add(help,tr("Show pinout"),"showPinout",{},[this]{showHelp(true);});add(help,tr("Show/Hide help panel"),"contextHelp",{},[this]{helpDock_->setVisible(helpDock_->isHidden());refreshHelp();});
  connect(area_,&QMdiArea::subWindowActivated,this,[this](QMdiSubWindow*){refreshHelp();});connect(tree_,&QTreeWidget::currentItemChanged,this,[this]{refreshHelp();});connect(qApp,&QApplication::focusChanged,this,[this](QWidget*,QWidget*){if(!helpDock_->isHidden())refreshHelp();});
 }
 void initialize(){QString error;const auto data=QSettings().value(storageKey()).toByteArray();if(!data.isEmpty())applyDocument(data,error);}
 void store(){QSettings().setValue(storageKey(),document());}
 bool restore(QString &error){const auto data=QSettings().value(storageKey()).toByteArray();if(data.isEmpty()){error=tr("No stored desktops yet.");return false;}return applyDocument(data,error);}
 int currentDesktop()const{return active_;}
 QByteArray document(){capture();return QJsonDocument(QJsonObject{{"format","tunuino-desktops-v1"},{"active",active_},{"desktops",states_}}).toJson();}
 bool applyDocument(const QByteArray &bytes,QString &error){
  QJsonParseError parse;const auto doc=QJsonDocument::fromJson(bytes,&parse).object();const auto states=doc["desktops"].toArray();const int active=doc["active"].toInt(-1);if(parse.error!=QJsonParseError::NoError||doc["format"]!="tunuino-desktops-v1"||states.isEmpty()||states.size()>32||active<0||active>=states.size()){error=tr("Invalid desktop template.");return false;}
  for(const auto &value:states){const auto state=value.toObject();if(state["name"].toString().trimmed().isEmpty()||state["name"].toString().size()>64||!state["windows"].isArray()||state["windows"].toArray().size()>128){error=tr("Invalid desktop name or window list.");return false;}QSet<QString> ids;for(const auto &entry:state["windows"].toArray()){const auto w=entry.toObject();const auto id=w["id"].toString();if(id.isEmpty()||id.size()>256||ids.contains(id)){error=tr("Invalid or repeated window identity.");return false;}ids.insert(id);for(const auto &field:{"x","y","w","h"})if(!w[field].isDouble()||w[field].toDouble()!=w[field].toInt()||std::abs(w[field].toInt())>32768){error=tr("Invalid window rectangle.");return false;}if(w["w"].toInt()<80||w["h"].toInt()<50){error=tr("Invalid window size.");return false;}}}
  applying_=true;states_=states;active_=active;rebuildTabs();rebuildSwitchMenu();applying_=false;applyState();return true;
 }
 void switchTo(int index){if(index<0||index>=states_.size()||index==active_)return;capture();active_=index;applying_=true;tabs_->setCurrentIndex(index);applying_=false;applyState();}
private:
 QString storageKey()const{return uid_.isEmpty()?QString("workspace/desktops"):QString("workspace/ecu/")+QString::fromLatin1(uid_.toHex());}
 QAction *add(QMenu *menu,const QString &text,const QString &id,const QString &shortcut,std::function<void()> call){auto *a=menu->addAction(text);a->setObjectName(id);if(!shortcut.isEmpty())a->setShortcut(QKeySequence(shortcut));connect(a,&QAction::triggered,this,std::move(call));return a;}
 QString id(QMdiSubWindow *w){auto key=w->property("desktopWindowId").toString();if(key.isEmpty()){key=w->windowTitle()+"|"+w->widget()->objectName();w->setProperty("desktopWindowId",key);}return key;}
 void capture(){QJsonArray windows;for(auto *w:area_->subWindowList(QMdiArea::StackingOrder))if(!w->isHidden()){const auto rect=w->isMaximized()||w->isMinimized()?w->normalGeometry():w->geometry();windows.append(QJsonObject{{"id",id(w)},{"x",rect.x()},{"y",rect.y()},{"w",rect.width()},{"h",rect.height()},{"maximized",w->isMaximized()},{"minimized",w->isMinimized()}});}auto state=states_[active_].toObject();state["windows"]=windows;states_[active_]=state;}
 void applyState(){for(auto *w:area_->subWindowList())w->hide();for(const auto &entry:states_[active_].toObject()["windows"].toArray()){const auto saved=entry.toObject();QMdiSubWindow *selected=nullptr;for(auto *w:area_->subWindowList())if(id(w)==saved["id"].toString()){selected=w;break;}if(!selected&&ensureWindow)selected=ensureWindow(saved["id"].toString());if(!selected)continue;selected->showNormal();selected->setGeometry(saved["x"].toInt(),saved["y"].toInt(),saved["w"].toInt(),saved["h"].toInt());selected->setProperty("generalSizeApplied",true);selected->widget()->show();if(saved["maximized"].toBool())selected->showMaximized();else if(saved["minimized"].toBool())selected->showMinimized();selected->raise();area_->setActiveSubWindow(selected);}rebuildSwitchMenu();}
 void rebuildTabs(){const bool old=applying_;applying_=true;while(tabs_->count())tabs_->removeTab(0);for(const auto &state:states_)tabs_->addTab(state.toObject()["name"].toString());tabs_->setCurrentIndex(active_);applying_=old;rebuildSwitchMenu();}
 void rebuildSwitchMenu(){if(!switchMenu_)return;while(switchMenu_->actions().size()>states_.size()){auto *a=switchMenu_->actions().last();switchMenu_->removeAction(a);delete a;}for(int i=0;i<states_.size();++i){auto *a=i<switchMenu_->actions().size()?switchMenu_->actions()[i]:add(switchMenu_,{},QString("desktopSwitch%1").arg(i),i<10?QString("Ctrl+%1").arg((i+1)%10):QString{},[this,i]{switchTo(i);});a->setText(states_[i].toObject()["name"].toString());a->setCheckable(true);a->setChecked(i==active_);}}
 void tile(bool horizontal){QList<QMdiSubWindow*> windows;for(auto *w:area_->subWindowList())if(!w->isHidden())windows<<w;const auto r=area_->viewport()->rect();for(int i=0;i<windows.size();++i){windows[i]->showNormal();windows[i]->setGeometry(horizontal?QRect(0,r.height()*i/windows.size(),r.width(),r.height()*(i+1)/windows.size()-r.height()*i/windows.size()):QRect(r.width()*i/windows.size(),0,r.width()*(i+1)/windows.size()-r.width()*i/windows.size(),r.height()));}}
 void refreshHelp(){context_->setPlainText(contextHelp?contextHelp():tr("Select a parameter window to see its help."));}
 void showHelp(bool pinout){if(!helpDialog_){helpDialog_=new QDialog(owner_);helpDialog_->setAttribute(Qt::WA_DeleteOnClose);helpDialog_->setWindowTitle(tr("Tunuino help"));helpDialog_->resize(780,620);auto *layout=new QVBoxLayout(helpDialog_);auto *browser=new QTextBrowser(helpDialog_);browser->setObjectName("generalHelpBrowser");QString html=tr("<h1>Tunuino Console</h1><h2>Projects</h2><p>File / Open Project and Save Project as store the complete local project. Opening does not write to the ECU. ECU / Write to ECU (F2) writes the active supported editor after validation.</p><h2>Workspace</h2><p>Open windows from the tree. Desktops keep their own visible windows and sizes. Store/Restore saves the arrangement; templates contain layout only. Window commands arrange or close the current desktop. Edit Undo/Redo applies to the active map table; Show undo list displays its retained history.</p><h2>Updates</h2><p>Help / About contains Console updates and channel selection.</p><h2 id='pinout'>Proteus v0.7 supported signal pinout</h2><p>Signal contacts used by the Console are listed below. This is not the complete power/ground/board wiring diagram.</p><table border='1' cellpadding='5'><tr><th>Signal</th><th>Connector / contact</th></tr>");const int av[]={13,24,14,25,15,26,16,27,17,28,18},at[]={19,30,20,31},ls[]={3,15,4,16,5,6,7,19,8,20,9,21,10,11,12,23},ign[]={35,34,22,33,32,31,30,29,28,27,26,25};for(int i=0;i<11;++i)html+=QString("<tr><td>AV%1</td><td>J103 / %2</td></tr>").arg(i+1).arg(av[i]);for(int i=0;i<4;++i)html+=QString("<tr><td>AT%1</td><td>J103 / %2</td></tr>").arg(i+1).arg(at[i]);for(int i=0;i<16;++i)html+=QString("<tr><td>LS%1</td><td>J102 / %2</td></tr>").arg(i+1).arg(ls[i]);for(int i=0;i<12;++i)html+=QString("<tr><td>IGN%1 (logic)</td><td>J102 / %2</td></tr>").arg(i+1).arg(ign[i]);html+=tr("<tr><td>Primary trigger</td><td>J101 / 5 (+), 13 (-)</td></tr><tr><td>CAM 1</td><td>J101 / 4 (+), 12 (-)</td></tr><tr><td>Digital 1 / VSS</td><td>J101 / 10</td></tr><tr><td>Digital 2 / CAM 2</td><td>J101 / 1</td></tr></table>");browser->setHtml(html);layout->addWidget(browser);}helpDialog_->show();helpDialog_->raise();auto *browser=helpDialog_->findChild<QTextBrowser*>();if(pinout)browser->scrollToAnchor("pinout");else browser->verticalScrollBar()->setValue(0);}
 QByteArray uid_;QMainWindow *owner_;QMdiArea *area_;QDockWidget *treeDock_;QTreeWidget *tree_;QTabBar *tabs_;QMenu *switchMenu_=nullptr;QJsonArray states_;int active_=0;bool applying_=false;QDockWidget *eventDock_,*helpDock_;QPlainTextEdit *events_;QTextBrowser *context_;QPointer<QDialog> helpDialog_;
};
}
