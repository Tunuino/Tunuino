#pragma once
#include "TriggerPresets.h"
#include <QDialog>
#include <QTableWidget>
#include <QHeaderView>
#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QAction>
#include <QPainter>
#include <QStyleOptionButton>
#include <functional>

namespace tunuino {
class PresetCheckBox final:public QCheckBox {
public:
    using QCheckBox::QCheckBox;
protected:
    void paintEvent(QPaintEvent *event)override{
        QCheckBox::paintEvent(event);if(!isChecked())return;
        QStyleOptionButton option;initStyleOption(&option);const auto r=style()->subElementRect(QStyle::SE_CheckBoxIndicator,&option,this);
        QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);painter.setPen(QPen(QColor("#756e43"),2));
        painter.drawLine(r.left()+3,r.center().y(),r.left()+7,r.bottom()-4);painter.drawLine(r.left()+7,r.bottom()-4,r.right()-3,r.top()+3);
    }
};
class TriggerPresetDialog final:public QDialog {
public:
    std::function<bool(int,int,QString&)> applyRequested;
    std::function<void()> openManual;
    explicit TriggerPresetDialog(QWidget *parent=nullptr):QDialog(parent){
        setObjectName("triggerPresetDialog");setWindowTitle(tr("Trigger wizard"));
        setWindowFlags(Qt::Dialog|Qt::WindowTitleHint|Qt::WindowCloseButtonHint);setWindowModality(Qt::ApplicationModal);
        auto *layout=new QVBoxLayout(this);layout->setContentsMargins(8,8,8,8);layout->setSpacing(8);
        auto *header=new QLabel(tr("Trigger wizard"),this);header->setStyleSheet("background:#686a66;color:#f1f1e7;font:600 10pt 'Segoe UI';padding:4px;");layout->addWidget(header);
        table_=new QTableWidget(9,2,this);table_->setObjectName("triggerPresetTable");table_->horizontalHeader()->hide();table_->verticalHeader()->hide();table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);table_->verticalHeader()->setDefaultSectionSize(30);table_->setEditTriggers(QAbstractItemView::NoEditTriggers);table_->setSelectionMode(QAbstractItemView::SingleSelection);table_->setSelectionBehavior(QAbstractItemView::SelectRows);table_->setFont(QFont("Segoe UI",10));
        table_->setStyleSheet("QTableWidget{font:10pt 'Segoe UI';background:#252829;color:#8bff00;gridline-color:#7b7d78;border:1px solid #c3cbc7;selection-background-color:#118ecb;selection-color:white;}QComboBox{font:10pt 'Segoe UI';background:transparent;color:#e2e6df;border:0;padding:1px;}QComboBox QAbstractItemView{background:#e7e7dc;color:#222222;selection-background-color:#118ecb;selection-color:white;}QCheckBox::indicator{width:16px;height:16px;border:1px solid #9da89c;background:#eeeeE5;}QCheckBox::indicator:checked{background:#eeeee5;}QCheckBox::indicator:disabled{background:#3a3d3a;border-color:#696c67;}");
        table_->setItem(0,0,new QTableWidgetItem(tr("Predefined triggers")));engines_=new QComboBox(table_);engines_->setObjectName("predefinedTriggers");for(const auto &preset:TriggerPresets::catalog()){engines_->addItem(preset.name);engines_->setItemData(engines_->count()-1,preset.limitation,Qt::ToolTipRole);}engines_->setMaxVisibleItems(11);table_->setCellWidget(0,1,engines_);
        const QStringList names{tr("Apply to primary trigger"),tr("Apply to secondary trigger"),tr("Apply to CAM#2 trigger"),tr("Apply to firing order"),tr("Apply to ignition outputs"),tr("Apply to injector phase"),tr("Apply to coil dwell time"),tr("Apply to VVTi CAMs offsets")};
        for(int i=0;i<8;++i){table_->setItem(i+1,0,new QTableWidgetItem(names[i]));auto *check=new PresetCheckBox(table_);check->setObjectName(QString("triggerApply%1").arg(i));checks_.append(check);table_->setCellWidget(i+1,1,check);connect(check,&QCheckBox::toggled,this,[this]{updateOk();});}
        table_->setMinimumHeight(272);layout->addWidget(table_);message_=new QLabel(this);message_->setObjectName("triggerPresetMessage");message_->setWordWrap(true);message_->setStyleSheet("color:#c2c7c4;font:9pt 'Segoe UI';");layout->addWidget(message_);
        buttons_=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,this);buttons_->button(QDialogButtonBox::Ok)->setObjectName("triggerPresetOk");buttons_->button(QDialogButtonBox::Cancel)->setObjectName("triggerPresetCancel");for(auto *button:buttons_->buttons())button->setMinimumWidth(110);layout->addWidget(buttons_);
        connect(buttons_,&QDialogButtonBox::accepted,this,[this]{apply();});connect(buttons_,&QDialogButtonBox::rejected,this,&QDialog::reject);connect(engines_,&QComboBox::currentIndexChanged,this,[this]{selectionChanged();});
        setContextMenuPolicy(Qt::ActionsContextMenu);auto *manual=new QAction(tr("Manual trigger setup..."),this);manual->setObjectName("triggerManualSetup");addAction(manual);connect(manual,&QAction::triggered,this,[this]{reject();if(openManual)openManual();});selectionChanged();resize(sizeHint());
    }
    QSize sizeHint()const override{return QSize(600,410);}
private:
    int groups()const{int result=0;for(int i=0;i<checks_.size();++i)if(checks_[i]->isEnabled()&&checks_[i]->isChecked())result|=1<<i;return result;}
    void updateOk(){if(buttons_)buttons_->button(QDialogButtonBox::Ok)->setEnabled(groups()!=0);}
    void selectionChanged(){const int supported=TriggerPresets::supported(engines_->currentIndex());for(int i=0;i<checks_.size();++i){const bool available=supported&(1<<i);checks_[i]->setEnabled(available);checks_[i]->setChecked(available);table_->item(i+1,0)->setForeground(QColor(available?"#8bff00":"#858b86"));const QString reason=i==0?tr("Wheel geometry only; keeps capture, RPM limit, polarity and timing reference."):i==3?tr("Sets cylinder count and firing order; keeps wiring. New cylinders remain unassigned."):tr("No validated preset data or native support for this group. Existing settings are preserved.");table_->item(i+1,0)->setToolTip(reason);checks_[i]->setToolTip(reason);}message_->setText(TriggerPresets::catalog()[engines_->currentIndex()].limitation+tr(" OK applies selected groups locally; no ECU write."));table_->setCurrentCell(0,0);updateOk();}
    void apply(){if(!groups()||!applyRequested)return;QString error;if(!applyRequested(engines_->currentIndex(),groups(),error)){message_->setText(error);return;}accept();}
    QTableWidget *table_=nullptr;QComboBox *engines_=nullptr;QVector<QCheckBox*> checks_;QDialogButtonBox *buttons_=nullptr;QLabel *message_=nullptr;
};
}
