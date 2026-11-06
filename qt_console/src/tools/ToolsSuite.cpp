#include "ToolsSuite.h"
#include <QMainWindow>
#include <QApplication>
#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QKeySequenceEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QTreeWidget>
#include <QSettings>
#include <QMessageBox>
#include <QTimer>
#include <QClipboard>
#include <QPushButton>
#include <QCryptographicHash>
#include <QSet>
namespace tunuino {
namespace {
QString lightStyle(){return QStringLiteral("QWidget { background: #ededed; color: #202020; } QLineEdit, QSpinBox, QDoubleSpinBox, QTableView, QTreeView { background: white; color: #202020; } QMenu::item:selected { background: #9fc7ed; } QHeaderView::section { background: #d7d7d7; color: #202020; }");}
struct KeyAction {QString id,title;QAction *action;};
QVector<KeyAction> keyActions(QMainWindow *window){
    QVector<KeyAction> result;
    std::function<void(QMenu*,QString)> walk=[&](QMenu *menu,QString prefix){for(auto *a:menu->actions()){
        if(a->isSeparator())continue;QString text=a->text();text.remove('&');const QString id=prefix+"/"+text;
        if(a->menu())walk(a->menu(),id);else result.append({id,id.mid(1),a});}};
    for(auto *a:window->menuBar()->actions())if(a->menu())walk(a->menu(),"/"+a->text());return result;
}
QString keyId(const QString &id){return "tools/keys/"+QString::fromLatin1(QCryptographicHash::hash(id.toUtf8(),QCryptographicHash::Sha256).toHex());}
QString namesKey(const ToolSnapshot &base,const QString &id){return "tools/names/"+QString::fromLatin1(base.identity.mid(20,12).toHex())+"/"+id;}
}
ToolsSuite::ToolsSuite(QMainWindow *window,QTreeWidget *navigation):QObject(window),window_(window),navigation_(navigation){
    darkStyle_=qApp->styleSheet();
    auto *timer=new QTimer(this);timer->setInterval(1000);connect(timer,&QTimer::timeout,this,[this]{refreshAssignments();});timer->start();
    QTimer::singleShot(0,this,[this]{restoreKeys();QSettings settings;if(!settings.value("tools/darkMode",true).toBool())qApp->setStyleSheet(lightStyle());const int size=settings.value("tools/fontSize",QApplication::font().pointSize()).toInt();if(size>=8&&size<=18){auto f=QApplication::font();f.setPointSize(size);QApplication::setFont(f);}
        for(int i=0;i<navigation_->topLevelItemCount();++i){auto *item=navigation_->topLevelItem(i);item->setHidden(!settings.value("tools/strategies/"+item->text(0),true).toBool());}});
}
void ToolsSuite::populate(QMenu *menu,QAction *graph,QAction *tune){
    menu->setToolTipsVisible(true);menu->removeAction(graph);menu->removeAction(tune);
    const auto unsupported=[&](const QString &name,const QString &reason){auto *a=menu->addAction(name);a->setEnabled(false);a->setToolTip(reason);};
    unsupported(tr("Device password"),tr("ECU firmware has no persistent password/access-control protocol."));
    unsupported(tr("DBW calibration tool"),tr("Requires a DBW controller, redundant TPS/PPS acquisition and an H-bridge driver in the ECU firmware."));
    unsupported(tr("Calibrate WBO circuit"),tr("The supported ECU firmware reads an external wideband controller; it has no internal WBO circuit calibration driver."));
    auto *test=menu->addAction(tr("Test outputs"));connect(test,&QAction::triggered,this,[this]{if(openOutputTest)openOutputTest();});
    unsupported(tr("Diff. oil bleed"),tr("Requires the vehicle differential pump/CAN service protocol, which is not implemented in this firmware."));
    auto *autotune=menu->addAction(tr("Autotune"));connect(autotune,&QAction::triggered,this,[this]{if(openAutotune)openAutotune();});
    menu->addSeparator();
    menu->addAction(tr("Logbook"),this,[this]{if(openServiceData)openServiceData(2);});
    unsupported(tr("User defined CAN stream"),tr("Requires CAN transport and persistent frame mappings in the ECU firmware."));
    unsupported(tr("Set EDL-1 datalogger time"),tr("No EDL-1 transport or clock protocol is implemented for this ECU."));
    menu->addAction(tr("Show assigned outputs"),this,[this]{assignments(true);});menu->addAction(tr("Show assigned inputs"),this,[this]{assignments(false);});
    auto *names=menu->addMenu(tr("User names..."));names->addAction(tr("Outputs"),this,[this]{userNames(true);});names->addAction(tr("Analog inputs"),this,[this]{userNames(false);});
    menu->addSeparator();auto *scope=menu->addAction(tr("Scope"));scope->setShortcut(QKeySequence("F5"));connect(scope,&QAction::triggered,this,[this]{if(openScope)openScope();});
    menu->addAction(graph);menu->addAction(tune);menu->addSeparator();
    menu->addAction(tr("Customize keys"),this,[this]{customizeKeys();});menu->addAction(tr("General options"),this,[this]{generalOptions();});menu->addAction(tr("Available strategies"),this,[this]{strategies();});
    menu->addSeparator();unsupported(tr("Decode VW coding..."),tr("Vehicle-specific CAN coding definitions and capture transport are not implemented."));
    menu->addSeparator();menu->addAction(tr("Vehicle VIN..."),this,[this]{if(openServiceData)openServiceData(0);});menu->addAction(tr("Project comment..."),this,[this]{if(openServiceData)openServiceData(1);});
}
QString ToolsSuite::alias(const QString &id,const ToolSnapshot &base)const{return QSettings().value(namesKey(base,id)).toString().left(16);}
void ToolsSuite::assignments(bool outputs){
    auto &dialog=outputs?outputs_:inputs_;if(dialog){dialog->show();dialog->raise();refreshAssignments();return;}
    dialog=new QDialog(window_);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowTitle(outputs?tr("Assigned outputs"):tr("Assigned inputs"));dialog->resize(760,620);
    auto *layout=new QVBoxLayout(dialog);auto *state=new QLabel(dialog);state->setWordWrap(true);layout->addWidget(state);
    auto *table=new QTableWidget(dialog);table->setColumnCount(4);table->setHorizontalHeaderLabels({tr("Channel"),tr("Contact"),tr("User name"),tr("Assigned function")});table->setEditTriggers(QAbstractItemView::NoEditTriggers);table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);layout->addWidget(table,1);
    auto *copy=new QPushButton(tr("Copy list"),dialog);layout->addWidget(copy);connect(copy,&QPushButton::clicked,dialog,[table]{QString text;for(int r=0;r<table->rowCount();++r){QStringList cells;for(int c=0;c<4;++c)cells<<table->item(r,c)->text();text+=cells.join('\t')+'\n';}QApplication::clipboard()->setText(text);});
    if(outputs){outputTable_=table;outputState_=state;}else{inputTable_=table;inputState_=state;}refreshAssignments();dialog->show();
}
void ToolsSuite::refreshAssignments(){
    if(!snapshot)return;const auto base=snapshot();
    for(bool output:{false,true}){auto dialog=output?outputs_:inputs_;if(!dialog||!dialog->isVisible())continue;auto *table=output?outputTable_:inputTable_;auto *state=output?outputState_:inputState_;
        const auto rows=output?outputAssignments(base):inputAssignments(base);
        QByteArray fingerprint=base.identity+QByteArray::number(base.generation)+base.pages[0]+base.pages[3];for(const auto &row:rows)fingerprint+=row.id.toUtf8()+alias(row.id,base).toUtf8();
        fingerprint=QCryptographicHash::hash(fingerprint,QCryptographicHash::Sha256);if(table->property("assignmentFingerprint").toByteArray()==fingerprint)continue;table->setProperty("assignmentFingerprint",fingerprint);
        state->setText(base.valid()?tr("ECU generation %1. Green: assigned. Yellow: unused. Red: conflicting assignments.").arg(base.generation):tr("Connect the ECU and wait for automatic synchronization. No current assignments are available."));table->setRowCount(rows.size());
        for(int r=0;r<rows.size();++r){const auto &row=rows[r];const QStringList text{row.id,row.contact,alias(row.id,base),row.functions.isEmpty()?tr("Unused"):row.functions.join(", ")};for(int c=0;c<4;++c){auto *item=new QTableWidgetItem(text[c]);item->setBackground(QColor(row.conflict?"#713535":row.functions.isEmpty()?"#665d2f":"#2e583c"));table->setItem(r,c,item);}}}
}
void ToolsSuite::userNames(bool outputs){
    const auto base=snapshot?snapshot():ToolSnapshot{};if(!base.valid()){QMessageBox::information(window_,tr("User names"),tr("Connect the ECU and wait for a complete configuration read."));return;}
    QDialog dialog(window_);dialog.setWindowTitle(outputs?tr("User names — outputs"):tr("User names — analog inputs"));dialog.resize(620,600);auto *layout=new QVBoxLayout(&dialog);
    auto *note=new QLabel(tr("Names are kept on this computer for this ECU identity. They label the assignment lists; they are not ECU calibration data."),&dialog);note->setWordWrap(true);layout->addWidget(note);
    const auto rows=outputs?outputAssignments(base):inputAssignments(base);auto *table=new QTableWidget(rows.size(),2,&dialog);table->setHorizontalHeaderLabels({tr("Channel / contact"),tr("Name (16 characters)")});table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);layout->addWidget(table);
    QVector<QLineEdit*> edits;for(int i=0;i<rows.size();++i){auto *item=new QTableWidgetItem(rows[i].id+" — "+rows[i].contact);item->setFlags(item->flags()&~Qt::ItemIsEditable);table->setItem(i,0,item);auto *edit=new QLineEdit(alias(rows[i].id,base),table);edit->setMaxLength(16);edits<<edit;table->setCellWidget(i,1,edit);}
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()==QDialog::Accepted){QSettings settings;for(int i=0;i<rows.size();++i)settings.setValue(namesKey(base,rows[i].id),edits[i]->text().trimmed());refreshAssignments();}
}
void ToolsSuite::restoreKeys(){
    const auto actions=keyActions(window_);for(const auto &item:actions)if(!item.action->property("tunuinoDefaultShortcut").isValid())item.action->setProperty("tunuinoDefaultShortcut",item.action->shortcut());QSettings settings;QSet<QString> seen;QVector<QKeySequence> requested;
    for(const auto &item:actions){const auto saved=settings.value(keyId(item.id));const auto key=saved.isValid()?QKeySequence(saved.toString(),QKeySequence::PortableText):item.action->shortcut();const auto text=key.toString(QKeySequence::PortableText);if(!text.isEmpty()&&seen.contains(text))return;if(!text.isEmpty())seen.insert(text);requested<<key;}
    for(int i=0;i<actions.size();++i)actions[i].action->setShortcut(requested[i]);
}
void ToolsSuite::customizeKeys(){
    const auto actions=keyActions(window_);QDialog dialog(window_);dialog.setWindowTitle(tr("Customize keys"));dialog.resize(720,600);auto *layout=new QVBoxLayout(&dialog);auto *table=new QTableWidget(actions.size(),2,&dialog);table->setHorizontalHeaderLabels({tr("Function"),tr("Key combination")});table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);layout->addWidget(table);
    QVector<QKeySequenceEdit*> edits;for(int i=0;i<actions.size();++i){auto *item=new QTableWidgetItem(actions[i].title);item->setFlags(item->flags()&~Qt::ItemIsEditable);table->setItem(i,0,item);auto *edit=new QKeySequenceEdit(actions[i].action->shortcut(),table);edits<<edit;table->setCellWidget(i,1,edit);}
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Close|QDialogButtonBox::RestoreDefaults,&dialog);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults),&QPushButton::clicked,&dialog,[&]{for(int i=0;i<actions.size();++i)edits[i]->setKeySequence(actions[i].action->property("tunuinoDefaultShortcut").value<QKeySequence>());});
    connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,[&]{QSet<QString> seen;for(auto *edit:edits){const auto key=edit->keySequence().toString(QKeySequence::PortableText);if(!key.isEmpty()&&seen.contains(key)){QMessageBox::warning(&dialog,tr("Customize keys"),tr("The same key combination is assigned to more than one function."));return;}if(!key.isEmpty())seen.insert(key);}QSettings settings;for(int i=0;i<actions.size();++i){const auto key=edits[i]->keySequence();actions[i].action->setShortcut(key);settings.setValue(keyId(actions[i].id),key.toString(QKeySequence::PortableText));}dialog.accept();});dialog.exec();
}
void ToolsSuite::strategies(){
    QDialog dialog(window_);dialog.setWindowTitle(tr("Available strategies"));auto *layout=new QVBoxLayout(&dialog);auto *note=new QLabel(tr("Choose the groups visible in the navigation tree. This changes the workspace only; it does not enable ECU strategies."),&dialog);note->setWordWrap(true);layout->addWidget(note);QVector<QCheckBox*> boxes;
    for(int i=0;i<navigation_->topLevelItemCount();++i){auto *item=navigation_->topLevelItem(i);auto *box=new QCheckBox(item->text(0),&dialog);box->setChecked(!item->isHidden());boxes<<box;layout->addWidget(box);}
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()==QDialog::Accepted){QSettings settings;for(int i=0;i<boxes.size();++i){auto *item=navigation_->topLevelItem(i);item->setHidden(!boxes[i]->isChecked());settings.setValue("tools/strategies/"+item->text(0),boxes[i]->isChecked());}}
}
void ToolsSuite::generalOptions(){
    QDialog dialog(window_);dialog.setWindowTitle(tr("General options"));auto *layout=new QFormLayout(&dialog);auto *dark=new QCheckBox(tr("Dark mode"),&dialog);dark->setChecked(QSettings().value("tools/darkMode",true).toBool());layout->addRow(dark);auto *size=new QSpinBox(&dialog);size->setRange(8,18);size->setValue(QApplication::font().pointSize());layout->addRow(tr("Interface font size"),size);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);layout->addRow(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()==QDialog::Accepted){auto font=QApplication::font();font.setPointSize(size->value());QApplication::setFont(font);QSettings().setValue("tools/fontSize",size->value());QSettings().setValue("tools/darkMode",dark->isChecked());qApp->setStyleSheet(dark->isChecked()?darkStyle_:lightStyle());}
}
}
