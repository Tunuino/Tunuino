#pragma once
#include "../ui/CalibrationAppearance.h"
#include <QAction>
#include <QComboBox>
#include <QListView>
#include <QStyledItemDelegate>
#include <QToolBar>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
namespace tunuino::SensorUi {
inline void choices(QComboBox *combo){auto *view=new QListView(combo);view->setObjectName("sensorChoices");combo->setView(view);view->setItemDelegate(new QStyledItemDelegate(view));view->setStyleSheet("QListView{background:#e7e7dc;color:#222;font:10pt 'Segoe UI';selection-background-color:#118ecb;selection-color:white;}QListView::item{background:#e7e7dc;color:#222;min-height:24px;padding:2px 5px;}QListView::item:selected{background:#118ecb;color:white;}");combo->setMaxVisibleItems(11);combo->setStyleSheet("QComboBox{combobox-popup:0;}");}
inline QToolBar *toolbar(QWidget *parent){auto *bar=new QToolBar(parent);bar->setIconSize(QSize(24,24));bar->setStyleSheet("QToolBar{background:#1d2022;border:1px solid #a0a19a;padding:1px;spacing:2px;}QToolButton{padding:1px;border:0;min-width:26px;min-height:26px;}QToolButton:hover{background:#0078d7;}");return bar;}
inline QLabel *section(QWidget *parent,const QString &title){auto *label=new QLabel(title,parent);label->setStyleSheet("background:#666764;color:#f0f0e8;font:600 10pt 'Segoe UI';padding:3px;border:1px solid #a4a59b;");return label;}
inline void grid(QTableWidget *table){table->horizontalHeader()->hide();table->verticalHeader()->hide();table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);table->verticalHeader()->setDefaultSectionSize(30);table->setEditTriggers(QAbstractItemView::NoEditTriggers);table->setStyleSheet("QTableWidget{font:10pt 'Segoe UI';background:#27292a;color:#25c7f5;gridline-color:#777b79;selection-background-color:#078bda;}QComboBox,QDoubleSpinBox{background:transparent;color:#8bff00;border:0;padding:0 4px;font:10pt 'Segoe UI';}QDoubleSpinBox:focus{background:#eceee8;color:#172126;}QComboBox{combobox-popup:0;}");}
}
