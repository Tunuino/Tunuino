#pragma once
#include "../calibration/SensorUi.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QSettings>
#include <QPainter>
#include <QVBoxLayout>
#include <functional>
namespace tunuino {
inline int option(const char *key,int fallback){return QSettings().value(QString("general/")+key,fallback).toInt();}
inline bool enabledOption(const char *key,bool fallback){return QSettings().value(QString("general/")+key,fallback).toBool();}
inline double displayValue(int channel,double value){if(option("units",0)==0)return value;if(channel==2)return value*.1450377377;if(channel==4||channel==5)return value*1.8+32;return value;}
inline QString displayUnit(int channel,const QString &metric){if(option("units",0)==0)return metric;return channel==2?"psi":channel==4||channel==5?"°F":metric;}
inline QColor tablePalette(double level,int scheme){if(scheme==1)return QColor::fromHsvF(.78-.18*level,.72,.86);if(scheme==2)return level<.5?QColor(0,int(level*510),int(255-level*510)):QColor(int((level-.5)*510),int(255-(level-.5)*510),0);return QColor::fromHsvF(.66*(1-level),.72,.86);}
class OptionCheck final:public QCheckBox {
public:using QCheckBox::QCheckBox;
protected:void paintEvent(QPaintEvent*)override{QPainter p(this);const QRect r(4,(height()-16)/2,16,16);p.setPen(isEnabled()?QColor("#8bff00"):QColor("#8b8f89"));p.setBrush(isChecked()?QColor("#eeeeeb"):QColor("#242628"));p.drawRect(r);if(isChecked()){p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(isEnabled()?QColor("#555854"):QColor("#999c96"),2));p.drawLine(r.topLeft()+QPoint(3,8),r.topLeft()+QPoint(7,12));p.drawLine(r.topLeft()+QPoint(7,12),r.topLeft()+QPoint(13,3));}}
};
class GeneralOptionsDialog final:public QDialog {
public:
 explicit GeneralOptionsDialog(QWidget *parent=nullptr):QDialog(parent){setObjectName("generalOptionsDialog");setWindowTitle(tr("General Options"));auto *layout=new QVBoxLayout(this);layout->setContentsMargins(5,5,5,5);table_=new QTableWidget(0,2,this);table_->setObjectName("generalOptionsTable");SensorUi::grid(table_);table_->setStyleSheet("QTableWidget{background:#242628;color:#8bff00;gridline-color:#737976;}QDoubleSpinBox,QComboBox{background:transparent;color:#8bff00;border:0;padding:0 4px;}QComboBox:disabled,QDoubleSpinBox:disabled{color:#93989a;}QCheckBox{padding-left:4px;}QCheckBox::indicator{width:14px;height:14px;border:1px solid #8bff00;background:transparent;}QCheckBox::indicator:checked{background:#8bff00;}");layout->addWidget(table_);
  section(tr("General Options"));choice("appearance",tr("Appearance"),{tr("Standard"),tr("Dark mode")},1);choice("units",tr("Units"),{tr("Metric"),tr("Imperial")},0)->setToolTip(tr("Display units in gauges and Graph log. Calibration editors retain their explicitly labelled ECU units."));number("panelSize",tr("Parameters panel size"),210,800,297);flag("toolbar",tr("Show toolbar"),true);flag("modified",tr("Mark modified cells"),true);flag("loadDesktops",tr("Load ECU desktops"),true);
  section(tr("2D Tables"));choice("table2Size",tr("New 2D table size"),{tr("Small"),tr("Medium"),tr("Big")},1);choice("curveColor",tr("Tables color"),{tr("Red"),tr("Blue"),tr("Green"),tr("Amber")},0);
  section(tr("3D Tables"));choice("scheme",tr("Color scheme"),{tr("Standard"),tr("Blue-Violet"),tr("Blue-Green-Red")},0);flag("square",tr("Display square tables"),false);choice("table3Appearance",tr("New 3D table appearance"),{tr("Table and graph"),tr("Only table")},1);choice("table3Size",tr("New 3D table size"),{tr("Small"),tr("Medium"),tr("Big")},2);flag("cubic",tr("Cubic 3D graph"),false);
  section(tr("Logging"));flag("stopPaused",tr("Stop logging when paused"),false);flag("autoSave",tr("Auto save logs"),false);flag("systemTime",tr("Display system time"),false);flag("minMarks",tr("Display min values marks"),true);flag("maxMarks",tr("Display max values marks"),true);
  section(tr("Gauges"));choice("gaugeSize",tr("Default gauge size"),{tr("Small"),tr("Medium"),tr("Big")},1);choice("gaugeType",tr("Default gauge type"),{tr("Round gauge"),tr("Value"),tr("Bar")},0);choice("gaugeStyle",tr("Round gauge style"),{tr("Standard"),tr("Deep Ocean"),tr("Amber Gold")},0);
  section(tr("Other"));flag("noExitAsk",tr("Don't ask at app exit"),true);
  auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,this);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,this,[this]{for(const auto &write:writes_)write();accept();});connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);resize(590,860);
 }
private:
 int row(const QString &label){int r=table_->rowCount();table_->insertRow(r);table_->setItem(r,0,new QTableWidgetItem(label));table_->setRowHeight(r,27);return r;}
 void section(const QString &title){int r=row(title);table_->setSpan(r,0,1,2);table_->item(r,0)->setForeground(QColor("#f0f0e8"));table_->item(r,0)->setBackground(QColor("#666764"));}
 QComboBox *choice(const char *key,const QString &label,const QStringList &items,int initial){auto *w=new QComboBox(table_);w->setObjectName(key);w->addItems(items);if(QString(key)=="curveColor"){const QColor colors[]={QColor("#f63b18"),QColor("#197bff"),QColor("#45bd26"),QColor("#e4a92a")};for(int i=0;i<4;++i){QPixmap icon(18,18);icon.fill(colors[i]);w->setItemIcon(i,QIcon(icon));}}w->setCurrentIndex(qBound(0,option(key,initial),int(items.size())-1));SensorUi::choices(w);table_->setCellWidget(row(label),1,w);const QString path=QString("general/")+key;writes_.append([w,path]{if(w->isEnabled())QSettings().setValue(path,w->currentIndex());});return w;}
 QWidget *number(const char *key,const QString &label,int low,int high,int initial){auto *w=new QDoubleSpinBox(table_);w->setObjectName(key);w->setDecimals(0);w->setRange(low,high);w->setButtonSymbols(QAbstractSpinBox::NoButtons);w->setValue(option(key,initial));table_->setCellWidget(row(label),1,w);const QString path=QString("general/")+key;writes_.append([w,path]{if(w->isEnabled())QSettings().setValue(path,w->value());});return w;}
 QCheckBox *flag(const char *key,const QString &label,bool initial){auto *w=new OptionCheck(table_);w->setObjectName(key);w->setChecked(enabledOption(key,initial));table_->setCellWidget(row(label),1,w);const QString path=QString("general/")+key;writes_.append([w,path]{if(w->isEnabled())QSettings().setValue(path,w->isChecked());});return w;}
 QTableWidget *table_;QVector<std::function<void()>> writes_;
};
}
