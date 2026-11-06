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
namespace tunuino {
ParameterWidget::ParameterWidget(ParameterBlock block,QWidget *parent):QWidget(parent),block_(std::move(block)){
    setObjectName(block_.key);auto *layout=new QVBoxLayout(this);layout->setContentsMargins(4,4,4,4);layout->setSpacing(3);
    auto *toolbar=new QToolBar(this);toolbar->setObjectName("parameterToolbar");toolbar->setIconSize(QSize(20,20));
    auto *open=toolbar->addAction(style()->standardIcon(QStyle::SP_DialogOpenButton),tr("Open parameters"));auto *store=toolbar->addAction(style()->standardIcon(QStyle::SP_DialogSaveButton),tr("Save parameters"));auto *reset=toolbar->addAction(style()->standardIcon(QStyle::SP_BrowserReload),tr("Restore to defaults"));auto *help=toolbar->addAction(style()->standardIcon(QStyle::SP_MessageBoxQuestion),tr("Help"));layout->addWidget(toolbar);
    connect(open,&QAction::triggered,this,[this]{file(false);});connect(store,&QAction::triggered,this,[this]{file(true);});connect(reset,&QAction::triggered,this,[this]{defaults();});connect(help,&QAction::triggered,this,[this]{QMessageBox::information(this,block_.title,block_.help);});
    table_=new QTableWidget(block_.fields.size(),2,this);table_->setObjectName("parameters");table_->setHorizontalHeaderLabels({tr("Parameter"),tr("Value")});table_->verticalHeader()->hide();table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);table_->setSelectionMode(QAbstractItemView::SingleSelection);table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    for(int row=0;row<block_.fields.size();++row){const auto &f=block_.fields[row];table_->setItem(row,0,new QTableWidgetItem(f.label));
        if(f.choices.size()==2&&f.minimum==0&&f.maximum==1){auto *check=new QCheckBox(table_);check->setObjectName(f.key);table_->setCellWidget(row,1,check);connect(check,&QCheckBox::toggled,this,[this]{if(!loading_)edited_=true;});}
        else if(!f.choices.isEmpty()){auto *combo=new QComboBox(table_);combo->setObjectName(f.key);combo->addItems(f.choices);table_->setCellWidget(row,1,combo);connect(combo,&QComboBox::currentIndexChanged,this,[this,row,combo]{if(loading_)return;
            const auto &field=block_.fields[row];const bool custom=block_.key.endsWith(".outputs")||field.key=="input";
            if(custom&&combo->currentData().toInt()==-1){const int previous=combo->itemData(0).toInt();bool okay=false;const auto chosen=QInputDialog::getItem(this,tr("Customise connector assignment"),field.label,field.choices,previous,false,&okay);setValue(row,okay?field.choices.indexOf(chosen):previous);if(okay)edited_=true;}else edited_=true;
        });}
        else {auto *spin=new QDoubleSpinBox(table_);spin->setObjectName(f.key);spin->setRange(f.minimum,f.maximum);spin->setDecimals(f.scale==1000?3:f.scale==100?2:0);spin->setSingleStep(1/f.scale);spin->setSuffix(f.unit.isEmpty()?QString{}:" "+f.unit);table_->setCellWidget(row,1,spin);connect(spin,&QDoubleSpinBox::valueChanged,this,[this]{if(!loading_)edited_=true;});}
        setValue(row,f.initial);if(block_.key.endsWith(".outputs"))table_->setRowHidden(row,true);
    }
    table_->setStyleSheet("QTableWidget{background:#282828;color:#eeeeee;gridline-color:#707070;selection-background-color:#148bce;} QTableWidget QComboBox,QTableWidget QDoubleSpinBox{background:#282828;color:#8de522;border:0;padding-left:4px;} QTableWidget QComboBox:disabled,QTableWidget QDoubleSpinBox:disabled{color:#aaaaaa;}");
    table_->verticalHeader()->setDefaultSectionSize(30);layout->addWidget(table_,1);status_=new QLabel(tr("Offline editing. Connect and synchronize the ECU before writing."),this);status_->setObjectName("parameterStatus");status_->setWordWrap(true);layout->addWidget(status_);save_=new QPushButton(tr("Write to ECU"),this);save_->setObjectName("parameterSave");layout->addWidget(save_,0,Qt::AlignLeft);connect(save_,&QPushButton::clicked,this,[this]{save();});edited_=false;refresh();
}
double ParameterWidget::value(int row)const{auto *w=table_->cellWidget(row,1);if(auto *check=qobject_cast<QCheckBox*>(w))return check->isChecked()?1:0;if(auto *c=qobject_cast<QComboBox*>(w))return (block_.key.endsWith(".outputs")||block_.fields[row].key=="input")?c->currentData().toInt():c->currentIndex();return qobject_cast<QDoubleSpinBox*>(w)->value();}
void ParameterWidget::setValue(int row,double v){if(auto *check=qobject_cast<QCheckBox*>(table_->cellWidget(row,1))){check->setChecked(v!=0);return;}if(auto *c=qobject_cast<QComboBox*>(table_->cellWidget(row,1))){const auto &f=block_.fields[row];if(block_.key.endsWith(".outputs")||f.key=="input"){QSignalBlocker blocker(c);c->clear();c->addItem(f.choices.value(int(v)),int(v));c->addItem(tr("Customise..."),-1);c->setCurrentIndex(0);}else c->setCurrentIndex(int(v));}else qobject_cast<QDoubleSpinBox*>(table_->cellWidget(row,1))->setValue(v);}
void ParameterWidget::load(const QByteArray &h,const QByteArray &a,const QByteArray &identity,quint32 generation){if(h.size()!=256||a.size()!=256||identity.size()!=32||!generation)return;bool keep=edited_&&identity_==identity;base_=block_.page==1?h:a;identity_=identity;generation_=generation;
    if(!keep){loading_=true;const bool extension=(block_.page==4&&base_[68]==0)||(block_.key=="fuel.corrections"&&base_.mid(103,25)==QByteArray(25,0));for(int i=0;i<block_.fields.size();++i)setValue(i,extension?block_.fields[i].initial:ParameterCatalog::value(base_,block_.fields[i]));if(block_.key=="ignition.firing"){
        const int n=quint8(h[129]);QVector<QPair<double,int>> angles;for(int i=0;i<n&&i<12;++i)angles.append({ParameterCatalog::value(h,block_.fields[i+1]),i+1});std::sort(angles.begin(),angles.end());
        for(int i=0;i<12;++i)setValue(i+1,i<angles.size()?angles[i].second:0);
        table_->cellWidget(0,1)->setEnabled(false);
        for(int i=0;i<12;++i){table_->setRowHidden(i+1,i>=qMax(8,n));table_->cellWidget(i+1,1)->setEnabled(i<n);}
    }
    loading_=false;edited_=false;}if(block_.key.endsWith(".outputs"))for(int i=0;i<table_->rowCount();++i)table_->setRowHidden(i,i>=quint8(h[129]));refresh();}
void ParameterWidget::setAvailable(bool available){available_=available;refresh();}
void ParameterWidget::refresh(){const bool profile=block_.page!=1||block_.key=="ignition.primary"||(base_.size()==256&&quint8(base_[128])>=1&&quint8(base_[128])<=3);save_->setEnabled(available_&&base_.size()==256&&profile);if(!available_)status_->setText(tr("Offline or ECU operation in progress. Edits are retained."));else if(!profile)status_->setText(tr("A configured engine profile is required; injector/coil assignments are not generated automatically."));else status_->setText(tr("Synchronized ECU data. Writing affects only this parameter block; other window edits are separate."));}
void ParameterWidget::save(){if(!available_||base_.size()!=256||!writeRequested)return;auto desired=base_;
    if(block_.page==4&&desired[68]==0)desired=SensorCalibrationCodec::featureDefaults(SensorCalibrationCodec::initialAux(SensorKind::Iat,desired));
    auto mask=ParameterCatalog::ownedMask(block_);
    // Initializing an absent extension is explicit only on Write; existing blocks
    // are always preserved bit-for-bit outside this block's fields.
    if(block_.page==4&&base_[68]==0)for(int i=0;i<256;++i)mask[i]=char(quint8(mask[i])|(quint8(desired[i])^quint8(base_[i])));
    if(block_.key=="fuel.corrections"&&base_.mid(103,25)==QByteArray(25,0)){
        SensorCalibrationCodec::put(desired,104,400);SensorCalibrationCodec::put(desired,106,10000);SensorCalibrationCodec::put(desired,108,10000);
        for(int i=104;i<112;++i)mask[i]=char(quint8(mask[i])|(quint8(desired[i])^quint8(base_[i])));
    }
    if(block_.key=="ignition.firing"){
        if(value(0)!=quint8(base_[129])){status_->setText(tr("Changing cylinder count requires a complete engine-profile configuration and is not supported here."));return;}
        const int count=quint8(base_[129]);if(count<1||count>12)return;QVector<quint32> angles;for(int i=0;i<count;++i)angles.append(quint32(ParameterCatalog::value(base_,block_.fields[i+1])));std::sort(angles.begin(),angles.end());
        unsigned seen=0;for(int i=0;i<count;++i){const int cylinder=int(value(i+1))-1;if(cylinder<0||cylinder>=count||(seen&(1u<<cylinder))){status_->setText(tr("Each configured cylinder must appear exactly once in the firing order."));return;}seen|=1u<<cylinder;ParameterCatalog::put(desired,block_.fields[cylinder+1],angles[i]);}
        ParameterCatalog::put(desired,block_.fields.last(),value(block_.fields.size()-1));writeRequested(block_.page,desired,mask);return;
    }
    for(int i=0;i<block_.fields.size();++i)if(!ParameterCatalog::put(desired,block_.fields[i],value(i))){status_->setText(tr("Invalid parameter value."));return;}
    if(block_.page==1&&block_.key!="ignition.primary"&&quint8(base_[128])==0){status_->setText(tr("A configured engine profile is required. This window does not invent injector/coil assignments."));return;}
    writeRequested(block_.page,desired,mask);
}
void ParameterWidget::defaults(){if(QMessageBox::question(this,tr("Restore block defaults"),tr("Replace this window's values with inactive reference defaults? Nothing is sent to the ECU until Write to ECU."))!=QMessageBox::Yes)return;loading_=true;for(int i=0;i<block_.fields.size();++i)setValue(i,block_.fields[i].initial);if(block_.key=="ignition.firing"&&base_.size()==256){const int n=quint8(base_[129]);setValue(0,n);for(int i=0;i<12;++i)setValue(i+1,i<n?i+1:0);}loading_=false;edited_=true;}
void ParameterWidget::file(bool writing){const auto path=writing?QFileDialog::getSaveFileName(this,tr("Save parameters"),{},tr("Tunuino parameters (*.tnparams)")):QFileDialog::getOpenFileName(this,tr("Open parameters"),{},tr("Tunuino parameters (*.tnparams)"));if(path.isEmpty())return;
    if(writing){QJsonObject values;for(int i=0;i<block_.fields.size();++i)values[block_.fields[i].key]=value(i);const QJsonObject doc{{"format","tunuino-parameters-v1"},{"block",block_.key},{"values",values}};QSaveFile file(path);const auto bytes=QJsonDocument(doc).toJson();if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())QMessageBox::warning(this,tr("Parameters"),tr("Could not save parameters."));return;}
    QFile file(path);if(!file.open(QIODevice::ReadOnly)||file.size()>65536)return;QJsonParseError error;const auto doc=QJsonDocument::fromJson(file.readAll(),&error).object();const auto values=doc["values"].toObject();bool valid=error.error==QJsonParseError::NoError&&doc["format"]=="tunuino-parameters-v1"&&doc["block"]==block_.key&&values.size()==block_.fields.size();QByteArray sample(256,0);
    for(const auto &f:block_.fields)valid&=values[f.key].isDouble()&&ParameterCatalog::put(sample,f,values[f.key].toDouble())&&(f.choices.isEmpty()||(values[f.key].toDouble()>=0&&values[f.key].toDouble()<f.choices.size()&&std::floor(values[f.key].toDouble())==values[f.key].toDouble()));if(!valid){QMessageBox::warning(this,tr("Parameters"),tr("File format, block or parameter values do not match this window."));return;}loading_=true;for(int i=0;i<block_.fields.size();++i)setValue(i,values[block_.fields[i].key].toDouble());loading_=false;edited_=true;
}
}
