#include "ParameterWidget.h"
#include <QVBoxLayout>
#include <QToolBar>
#include <QTableWidget>
#include <QHeaderView>
#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <algorithm>
#include <QInputDialog>
#include <QSignalBlocker>
#include <QEvent>
#include <QGroupBox>
#include <QScrollArea>
#include <QFormLayout>
#include <QToolButton>
#include <QAction>
#include <QTimer>
#include <QSet>
#include <cmath>
namespace tunuino {
ParameterWidget::ParameterWidget(ParameterBlock block,QWidget *parent):QWidget(parent),block_(std::move(block)){
    setObjectName(block_.key);auto *layout=new QVBoxLayout(this);layout->setContentsMargins(4,4,4,4);layout->setSpacing(3);
    auto *toolbar=new QToolBar(this);toolbar->setObjectName("parameterToolbar");toolbar->setIconSize(QSize(20,20));
    auto *open=toolbar->addAction(style()->standardIcon(QStyle::SP_DialogOpenButton),tr("Open parameters"));auto *store=toolbar->addAction(style()->standardIcon(QStyle::SP_DialogSaveButton),tr("Save parameters"));auto *reset=toolbar->addAction(style()->standardIcon(QStyle::SP_BrowserReload),tr("Restore to defaults"));auto *help=toolbar->addAction(style()->standardIcon(QStyle::SP_MessageBoxQuestion),tr("Help"));toolbar->setStyleSheet("QToolBar{background:#1b1d1f;border:1px solid #555;padding:2px;spacing:2px;}QToolButton{padding:2px;border:0;}QToolButton:hover{background:#0078d7;}");layout->addWidget(toolbar);
    auto *section=new QLabel(block_.title.section(" - ",-1),this);section->setObjectName("parameterSection");section->setStyleSheet("background:#4b4b4b;color:#eeeeee;font-weight:bold;padding:4px;border:1px solid #777;");layout->addWidget(section);
    connect(open,&QAction::triggered,this,[this]{file(false);});connect(store,&QAction::triggered,this,[this]{file(true);});connect(reset,&QAction::triggered,this,[this]{defaults();});connect(help,&QAction::triggered,this,[this]{QMessageBox::information(this,block_.title,block_.help);});
    table_=new QTableWidget(block_.fields.size(),2,this);table_->setObjectName("parameters");table_->horizontalHeader()->hide();table_->verticalHeader()->hide();table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);table_->setSelectionMode(QAbstractItemView::SingleSelection);table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    for(int row=0;row<block_.fields.size();++row){const auto &f=block_.fields[row];table_->setItem(row,0,new QTableWidgetItem(f.label));
        if(f.choices.size()==2&&f.minimum==0&&f.maximum==1){table_->item(row,0)->setForeground(QColor("#43cddd"));auto *check=new QCheckBox(table_);check->setObjectName(f.key);table_->setCellWidget(row,1,check);connect(check,&QCheckBox::toggled,this,[this]{if(!loading_)edited_=true;});}
        else if(!f.choices.isEmpty()){auto *combo=new QComboBox(table_);combo->setObjectName(f.key);combo->addItems(f.choices);table_->setCellWidget(row,1,combo);connect(combo,&QComboBox::currentIndexChanged,this,[this,row,combo]{if(loading_)return;
            const auto &field=block_.fields[row];const bool custom=block_.key.endsWith(".outputs")||field.key=="input";
            if(custom&&combo->currentData().toInt()==-1){const int previous=combo->itemData(0).toInt();bool okay=false;const auto chosen=QInputDialog::getItem(this,tr("Customise connector assignment"),field.label,field.choices,previous,false,&okay);setValue(row,okay?field.choices.indexOf(chosen):previous);if(okay)edited_=true;}else edited_=true;
        });}
        else {auto *spin=new QDoubleSpinBox(table_);spin->setKeyboardTracking(false);spin->setObjectName(f.key);spin->setRange(f.minimum,f.maximum);spin->setDecimals(f.scale==1000?3:f.scale==100?2:0);spin->setSingleStep(1/f.scale);spin->setSuffix(f.unit.isEmpty()?QString{}:" "+f.unit);table_->setCellWidget(row,1,spin);connect(spin,&QDoubleSpinBox::valueChanged,this,[this]{if(!loading_)edited_=true;});}
        table_->cellWidget(row,1)->setProperty("parameterRow",row);table_->cellWidget(row,1)->installEventFilter(this);for(auto *child:table_->cellWidget(row,1)->findChildren<QWidget*>()){child->setProperty("parameterRow",row);child->installEventFilter(this);}
        if(block_.key=="ignition.firing"&&row>=1&&row<=12)table_->item(row,0)->setForeground(QColor("#8de522"));setValue(row,f.initial);if(block_.key.endsWith(".outputs"))table_->setRowHidden(row,true);
    }
    table_->setStyleSheet("QTableWidget{background:#282828;color:#eeeeee;gridline-color:#707070;selection-background-color:#148bce;} QTableWidget QComboBox,QTableWidget QDoubleSpinBox{background:#282828;color:#8de522;border:0;padding-left:4px;} QTableWidget QComboBox:disabled,QTableWidget QDoubleSpinBox:disabled{color:#aaaaaa;} QTableWidget QDoubleSpinBox:focus{background:#eeeeee;color:#202020;} QTableWidget QCheckBox{background:transparent;} QTableWidget QCheckBox::indicator{width:14px;height:14px;border:2px solid #43cddd;} QTableWidget QCheckBox::indicator:checked{background:#43cddd;}");
    table_->verticalHeader()->setDefaultSectionSize(28);layout->addWidget(table_,1);if(block_.key=="ignition.firing")setupProfile();status_=new QLabel(tr("Offline editing. Connect and synchronize the ECU before writing."),this);status_->setObjectName("parameterStatus");status_->setWordWrap(true);layout->addWidget(status_);save_=new QPushButton(tr("Write to ECU"),this);save_->setObjectName("parameterSave");layout->addWidget(save_,0,Qt::AlignLeft);connect(save_,&QPushButton::clicked,this,[this]{save();});edited_=false;refresh();
}
double ParameterWidget::value(int row)const{auto *w=table_->cellWidget(row,1);if(auto *check=qobject_cast<QCheckBox*>(w))return check->isChecked()?1:0;if(auto *c=qobject_cast<QComboBox*>(w))return (block_.key.endsWith(".outputs")||block_.fields[row].key=="input")?c->currentData().toInt():c->currentIndex();return qobject_cast<QDoubleSpinBox*>(w)->value();}
void ParameterWidget::setValue(int row,double v){if(auto *check=qobject_cast<QCheckBox*>(table_->cellWidget(row,1))){check->setChecked(v!=0);return;}if(auto *c=qobject_cast<QComboBox*>(table_->cellWidget(row,1))){const auto &f=block_.fields[row];if(block_.key.endsWith(".outputs")||f.key=="input"){QSignalBlocker blocker(c);c->clear();c->addItem(f.choices.value(int(v)),int(v));c->addItem(tr("Customise..."),-1);c->setCurrentIndex(0);}else c->setCurrentIndex(int(v));}else qobject_cast<QDoubleSpinBox*>(table_->cellWidget(row,1))->setValue(v);}
void ParameterWidget::load(const QByteArray &h,const QByteArray &a,const QByteArray &identity,quint32 generation){
    if(h.size()!=256||a.size()!=256||identity.size()!=32||!generation)return;
    const bool keep=edited_&&identity_==identity;
    if(profile_){
        if(keep&&(!profileHeader_.isEmpty())&&profileHeader_.mid(128)!=h.mid(128))profileConflict_=true;
        profileHeader_=h;profileAux_=a;
        if(!keep){profile_->load(h,a);profileConflict_=false;edited_=false;}
        else if(!profileConflict_)profile_->rebase(h,a);
        base_=h;identity_=identity;generation_=generation;syncProfile();refresh();return;
    }
    base_=block_.page==1?h:a;profileHeader_=h;profileAux_=a;identity_=identity;generation_=generation;
    if(!keep){loading_=true;const bool extension=(block_.key=="fuel.guard"&&base_[240]==0)||(block_.page==4&&base_[68]==0)||(block_.key=="fuel.corrections"&&base_.mid(103,25)==QByteArray(25,0));for(int i=0;i<block_.fields.size();++i)setValue(i,extension?block_.fields[i].initial:ParameterCatalog::value(base_,block_.fields[i]));loading_=false;edited_=false;}
    if(block_.key.endsWith(".outputs")){
        const int count=quint8(h[129]);for(int i=0;i<table_->rowCount();++i){table_->setRowHidden(i,i>=count);table_->item(i,0)->setText(tr("Cylinder %1").arg(i+1));}
        if(block_.key=="coils.outputs"&&h[128]==3){QSet<int> seen;for(int i=0;i<count;++i){const int coil=quint8(h[161+8*i]);if(seen.contains(coil)){table_->setRowHidden(i,true);continue;}seen.insert(coil);for(int j=i+1;j<count;++j)if(h[161+8*j]==h[161+8*i])table_->item(i,0)->setText(tr("Cylinders %1 / %2").arg(i+1).arg(j+1));}}
    }refresh();
}
void ParameterWidget::setAvailable(bool available){if(available_==available)return;available_=available;refresh();}
void ParameterWidget::refresh(){const bool profile=profile_.has_value()||block_.page!=1||block_.key=="ignition.primary"||(base_.size()==256&&quint8(base_[128])>=1&&quint8(base_[128])<=3);const bool supported=block_.key!="fuel.guard"||(identity_.size()==32&&(quint8(identity_[19])&64));save_->setEnabled(available_&&base_.size()==256&&profile&&!profileConflict_&&supported);if(!available_)status_->setText(tr("Offline or ECU operation in progress. Edits are retained."));else if(!supported)status_->setText(tr("This ECU firmware does not support Lambda guard. Update firmware before writing this block."));else if(profileConflict_)status_->setText(tr("ECU profile or assignments changed while editing. Right-click the table and discard local edits before writing."));else if(!profile)status_->setText(tr("A configured engine profile is required; injector/coil assignments are not generated automatically."));else status_->setText(tr("Synchronized ECU data. Writing affects only this parameter block; other window edits are separate."));}
void ParameterWidget::save(){if(block_.key=="fuel.guard"&&(identity_.size()!=32||!(quint8(identity_[19])&64))){refresh();return;}if(!available_||base_.size()!=256||!writeRequested)return;auto desired=base_;
    if(block_.page==4&&desired[68]==0)desired=SensorCalibrationCodec::featureDefaults(SensorCalibrationCodec::initialAux(SensorKind::Iat,desired));
    auto mask=ParameterCatalog::ownedMask(block_);
    // Initializing an absent extension is explicit only on Write; existing blocks
    // are always preserved bit-for-bit outside this block's fields.
    if(block_.page==4&&base_[68]==0)for(int i=0;i<256;++i)mask[i]=char(quint8(mask[i])|(quint8(desired[i])^quint8(base_[i])));
    if(block_.key=="fuel.guard"){desired[240]=1;mask[240]=char(255);}
    if(block_.key=="fuel.corrections"&&base_.mid(103,25)==QByteArray(25,0)){
        SensorCalibrationCodec::put(desired,104,400);SensorCalibrationCodec::put(desired,106,10000);SensorCalibrationCodec::put(desired,108,10000);
        for(int i=104;i<112;++i)mask[i]=char(quint8(mask[i])|(quint8(desired[i])^quint8(base_[i])));
    }
    if(profile_){
        if(profileConflict_){refresh();return;}QString error;const auto header=profile_->build(error);if(header.isEmpty()){status_->setText(error);return;}
        QByteArray owned(256,0);for(int i=128;i<256;++i)owned[i]=char(255);writeRequested(1,header,owned);return;
    }
    if(block_.key=="coils.outputs"&&base_[128]==3){
        for(int i=0;i<quint8(base_[129]);++i)if(!table_->isRowHidden(i)){const int port=int(value(i));desired[161+8*i]=char(port);for(int j=i+1;j<quint8(base_[129]);++j)if(base_[161+8*j]==base_[161+8*i])desired[161+8*j]=char(port);}
        EngineProfileDraft checked;checked.load(desired,profileAux_);QString error;if(checked.build(error).isEmpty()){status_->setText(error);return;}writeRequested(1,desired,mask);return;
    }
    for(int i=0;i<block_.fields.size();++i)if(!(block_.key.endsWith(".outputs")&&i>=quint8(base_[129]))&&!ParameterCatalog::put(desired,block_.fields[i],value(i))){status_->setText(tr("Invalid parameter value."));return;}
    if(block_.page==1&&block_.key!="ignition.primary"&&quint8(base_[128])==0){status_->setText(tr("A configured engine profile is required. This window does not invent injector/coil assignments."));return;}
    if(block_.page==1&&quint8(desired[128])>=1&&quint8(desired[128])<=3){EngineProfileDraft checked;checked.load(desired,profileAux_);QString error;if(checked.build(error).isEmpty()){status_->setText(error);return;}}
    writeRequested(block_.page,desired,mask);
}
void ParameterWidget::defaults(){
    if(QMessageBox::question(this,tr("Restore block defaults"),tr("Replace this window's values with reference defaults? Nothing is sent to the ECU until Write to ECU."))!=QMessageBox::Yes)return;
    if(profile_){for(int i=0;i<profile_->count();++i)profile_->setOrder(i,i);profile_->inverted=false;
        // Reset only firing order/phase; preserve wiring, fuel, dwell and trigger geometry.
        syncProfile();edited_=true;return;}
    loading_=true;for(int i=0;i<block_.fields.size();++i)setValue(i,block_.fields[i].initial);loading_=false;edited_=true;
}
void ParameterWidget::file(bool writing){const auto path=writing?QFileDialog::getSaveFileName(this,tr("Save parameters"),{},tr("Tunuino parameters (*.tnparams)")):QFileDialog::getOpenFileName(this,tr("Open parameters"),{},tr("Tunuino parameters (*.tnparams)"));if(path.isEmpty())return;
    if(writing){QJsonObject values;for(int i=0;i<block_.fields.size();++i)values[block_.fields[i].key]=value(i);QJsonObject doc{{"format","tunuino-parameters-v1"},{"block",block_.key},{"values",values}};if(profile_)doc["engineProfile"]=profile_->json();QSaveFile file(path);const auto bytes=QJsonDocument(doc).toJson();if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())QMessageBox::warning(this,tr("Parameters"),tr("Could not save parameters."));return;}
    QFile file(path);if(!file.open(QIODevice::ReadOnly)||file.size()>65536)return;QJsonParseError error;const auto doc=QJsonDocument::fromJson(file.readAll(),&error).object();const auto values=doc["values"].toObject();bool valid=error.error==QJsonParseError::NoError&&doc["format"]=="tunuino-parameters-v1"&&doc["block"]==block_.key&&values.size()==block_.fields.size();QByteArray sample(256,0);
    for(const auto &f:block_.fields)valid&=values[f.key].isDouble()&&ParameterCatalog::put(sample,f,values[f.key].toDouble())&&(f.choices.isEmpty()||(values[f.key].toDouble()>=0&&values[f.key].toDouble()<f.choices.size()&&std::floor(values[f.key].toDouble())==values[f.key].toDouble()));if(!valid){QMessageBox::warning(this,tr("Parameters"),tr("File format, block or parameter values do not match this window."));return;}if(profile_){QString error;auto draft=*profile_;if(doc.contains("engineProfile")){if(!draft.import(doc["engineProfile"].toObject(),error)){QMessageBox::warning(this,tr("Engine profile"),error);return;}}
        else {draft.resize(values["cylinders"].toInt());for(int i=0;i<draft.count();++i)draft.setOrder(i,values[QString("order%1").arg(i)].toInt()-1);draft.inverted=values["invert"].toInt()!=0;}
        *profile_=draft;syncProfile();edited_=true;return;}
    loading_=true;for(int i=0;i<block_.fields.size();++i)setValue(i,values[block_.fields[i].key].toDouble());loading_=false;edited_=true;
}
QSize ParameterWidget::sizeHint()const{int rows=0;for(int i=0;i<table_->rowCount();++i)if(!table_->isRowHidden(i))++rows;return QSize(650,qBound(200,rows*28+(profile_?155:125),610));}
bool ParameterWidget::eventFilter(QObject *object,QEvent *event){
    if(event->type()==QEvent::FocusIn&&object->property("parameterRow").isValid())table_->setCurrentCell(object->property("parameterRow").toInt(),0);
    return QWidget::eventFilter(object,event);
}
void ParameterWidget::setupProfile(){
    profile_.emplace();profile_->load({},{});
    auto *layout=static_cast<QVBoxLayout*>(this->layout());
    auto *toggle=new QToolButton(this);toggle->setObjectName("engineProfileToggle");toggle->setText(tr("Advanced"));toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);toggle->setArrowType(Qt::RightArrow);toggle->setCheckable(true);layout->addWidget(toggle,0,Qt::AlignLeft);
    advanced_=new QGroupBox(tr("Engine profile and connector assignments"),this);advanced_->setObjectName("engineProfileAdvanced");auto *form=new QFormLayout(advanced_);
    auto *note=new QLabel(tr("A cylinder-count change proposes evenly spaced firing angles. Verify the actual engine angles below. New cylinders have no outputs assigned. All editing is local until Write to ECU."),advanced_);note->setWordWrap(true);form->addRow(note);
    operation_=new QComboBox(advanced_);operation_->setObjectName("engineOperation");operation_->addItems({tr("Sequential / individual coils"),tr("Semi-sequential / wasted COP"),tr("Batch / wasted COP"),tr("Semi-sequential / shared coils"),tr("Batch / shared coils")});form->addRow(tr("Injection / ignition mode"),operation_);
    connect(operation_,&QComboBox::currentIndexChanged,this,[this](int index){if(!loading_){profile_->operation=index;edited_=true;syncProfile();}});
    const QStringList labels{tr("Required fuel ms"),tr("Dwell ms"),tr("Injector dead time ms"),tr("Maximum injection time ms"),tr("Tooth zero angle °"),tr("First cam window tooth"),tr("Last cam window tooth")};
    const double low[]={.1,.1,0,.1,0,0,0},high[]={30,5,5,20,719.99,119,119};
    for(int i=0;i<labels.size();++i){auto *spin=new QDoubleSpinBox(advanced_);spin->setObjectName(QString("engineProfileValue%1").arg(i));spin->setRange(low[i],high[i]);spin->setDecimals(i<4?3:i==4?2:0);spin->setSingleStep(i<4?.001:i==4?.01:1);profileValues_<<spin;form->addRow(labels[i],spin);
        connect(spin,&QDoubleSpinBox::valueChanged,this,[this,i](double value){if(loading_)return;const int n=int(std::lround(value*(i<4?1000:i==4?100:1)));switch(i){case 0:profile_->requiredFuel=n;break;case 1:profile_->dwell=n;break;case 2:profile_->deadTime=n;break;case 3:profile_->maximumInjection=n;break;case 4:profile_->toothZero=quint32(n);break;case 5:profile_->camFirst=n;break;case 6:profile_->camLast=n;break;}edited_=true;});
    }
    assignments_=new QTableWidget(0,4,advanced_);assignments_->setObjectName("engineProfileAssignments");assignments_->setHorizontalHeaderLabels({tr("Cylinder"),tr("Injector"),tr("Ignition"),tr("TDC °")});assignments_->verticalHeader()->hide();assignments_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);assignments_->setEditTriggers(QAbstractItemView::NoEditTriggers);assignments_->setMinimumHeight(180);form->addRow(assignments_);auto *scroll=new QScrollArea(this);scroll->setObjectName("engineProfileScroll");scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);scroll->setMinimumHeight(120);scroll->setMaximumHeight(300);scroll->setWidget(advanced_);layout->addWidget(scroll);scroll->hide();
    connect(toggle,&QToolButton::toggled,this,[scroll,toggle](bool shown){scroll->setVisible(shown);toggle->setArrowType(shown?Qt::DownArrow:Qt::RightArrow);});
    connect(qobject_cast<QDoubleSpinBox*>(table_->cellWidget(0,1)),&QDoubleSpinBox::valueChanged,this,[this](double n){if(loading_)return;profile_->resize(int(n));edited_=true;syncProfile();});
    for(int i=0;i<12;++i)connect(qobject_cast<QComboBox*>(table_->cellWidget(i+1,1)),&QComboBox::currentIndexChanged,this,[this,i](int cylinder){if(loading_||i>=profile_->count())return;profile_->setOrder(i,cylinder-1);edited_=true;syncProfile();});
    connect(qobject_cast<QCheckBox*>(table_->cellWidget(13,1)),&QCheckBox::toggled,this,[this](bool on){if(!loading_){profile_->inverted=on;edited_=true;}});
    table_->setContextMenuPolicy(Qt::ActionsContextMenu);auto *discard=new QAction(tr("Discard local edits / use ECU values"),table_);table_->addAction(discard);connect(discard,&QAction::triggered,this,[this]{discardProfileEdits();});syncProfile();
}
void ParameterWidget::discardProfileEdits(){if(profileHeader_.size()!=256)return;profile_->load(profileHeader_,profileAux_);profileConflict_=false;edited_=false;syncProfile();refresh();}
void ParameterWidget::syncProfile(){
    if(!profile_)return;loading_=true;const int count=profile_->count();setValue(0,count);table_->cellWidget(0,1)->setEnabled(true);
    for(int i=0;i<12;++i){setValue(i+1,i<count?profile_->order.value(i,-1)+1:0);table_->setRowHidden(i+1,i>=qMax(8,count));table_->cellWidget(i+1,1)->setEnabled(i<count);}setValue(13,profile_->inverted?1:0);
    operation_->setCurrentIndex(profile_->operation);const double v[]={profile_->requiredFuel/1000.0,profile_->dwell/1000.0,profile_->deadTime/1000.0,profile_->maximumInjection/1000.0,profile_->toothZero/100.0,double(profile_->camFirst),double(profile_->camLast)};for(int i=0;i<profileValues_.size();++i)profileValues_[i]->setValue(v[i]);
    assignments_->setRowCount(count);const int ls[]={3,15,4,16,5,6,7,19,8,20,9,21,10,11,12,23},ign[]={35,34,22,33,32,31,30,29,28,27,26,25};
    for(int i=0;i<count;++i){assignments_->setItem(i,0,new QTableWidgetItem(tr("Cylinder %1").arg(i+1)));
        for(int column=1;column<=2;++column){const bool spark=column==2;const int channel=spark?profile_->cylinders[i].coil:profile_->cylinders[i].injector;auto *combo=new QComboBox(assignments_);combo->setObjectName(QString("engine%1%2").arg(spark?"Coil":"Injector").arg(i));combo->addItem(channel<0?tr("Not assigned"):QString("%1%2 - J102 / %3").arg(spark?"IGN":"LS").arg(channel+1).arg(spark?ign[channel]:ls[channel]));combo->addItem(tr("Customise..."));assignments_->setCellWidget(i,column,combo);
            connect(combo,&QComboBox::activated,this,[this,i,spark](int index){if(index!=1)return;QStringList choices{tr("Not assigned")};const int pinsLs[]={3,15,4,16,5,6,7,19,8,20,9,21,10,11,12,23},pinsIgn[]={35,34,22,33,32,31,30,29,28,27,26,25};for(int port=0;port<(spark?12:16);++port)choices<<QString("%1%2 - J102 / %3").arg(spark?"IGN":"LS").arg(port+1).arg(spark?pinsIgn[port]:pinsLs[port]);bool okay=false;const auto choice=QInputDialog::getItem(this,tr("Customise output"),tr("Cylinder %1").arg(i+1),choices,(spark?profile_->cylinders[i].coil:profile_->cylinders[i].injector)+1,false,&okay);if(okay){QString error;if(!profile_->assign(i,spark,choices.indexOf(choice)-1,error))QMessageBox::warning(this,tr("Output assignment"),error);else edited_=true;}QTimer::singleShot(0,this,[this]{syncProfile();});});
        }
        auto *tdc=new QDoubleSpinBox(assignments_);tdc->setObjectName(QString("engineTdc%1").arg(i));tdc->setRange(0,719.99);tdc->setDecimals(2);tdc->setValue(profile_->cylinders[i].tdc/100.0);assignments_->setCellWidget(i,3,tdc);connect(tdc,&QDoubleSpinBox::editingFinished,this,[this,i,tdc]{if(!loading_){profile_->setTdc(i,quint32(std::lround(tdc->value()*100)));edited_=true;QTimer::singleShot(0,this,[this]{syncProfile();});}});
    }
    loading_=false;
    if(status_&&save_)refresh();
}

}
