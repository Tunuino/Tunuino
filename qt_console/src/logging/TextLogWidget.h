#pragma once
#include "LogData.h"
#include "../ui/GeneralOptions.h"
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QMenu>
#include <QMap>
#include <QShortcut>
#include <QTimer>
#include <QElapsedTimer>
#include <QVBoxLayout>
#include <QSettings>
#include <QLabel>
namespace tunuino {
class TextLogWidget final:public QWidget {
public:
 explicit TextLogWidget(QWidget *parent=nullptr):QWidget(parent){setObjectName("textLog");live_.values.fill(std::numeric_limits<double>::quiet_NaN());auto *layout=new QVBoxLayout(this);layout->setContentsMargins(4,4,4,4);tabs_=new QTabWidget(this);layout->addWidget(tabs_,1);groups_={{0,1,2,3,4,5,6},{0,1,2,3},{4,5},{6},{},{},{}};const QStringList names{tr("All channels"),tr("Fueling"),tr("Temperatures"),tr("Supply"),tr("Custom 1"),tr("Custom 2"),tr("Custom 3")};for(int i=4;i<7;++i){const auto stored=QSettings().value(QString("logging/textCustom%1").arg(i-3)).toList();for(const auto &v:stored){bool ok;int c=v.toInt(&ok);if(ok&&c>=0&&c<LogChannelCount&&!groups_[i].contains(c))groups_[i]<<c;}}for(int i=0;i<7;++i){auto *table=new QTableWidget(0,3,tabs_);table->setObjectName(QString("textLogGroup%1").arg(i));table->setHorizontalHeaderLabels({tr("Channel"),tr("Value"),tr("Unit")});table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);table->verticalHeader()->hide();table->setSelectionBehavior(QAbstractItemView::SelectRows);table->setEditTriggers(QAbstractItemView::NoEditTriggers);table->setContextMenuPolicy(Qt::CustomContextMenu);connect(table,&QWidget::customContextMenuRequested,this,[this,i,table](QPoint pos){const int row=table->rowAt(pos.y());QMenu menu(this);auto *add=menu.addMenu(tr("Add to Custom"));QMap<QAction*,int> targets;for(int n=4;n<7;++n)targets[add->addAction(tr("Custom %1").arg(n-3))]=n;auto *remove=menu.addAction(tr("Remove from Custom"));auto *up=menu.addAction(tr("Move up"));auto *down=menu.addAction(tr("Move down"));remove->setEnabled(i>=4&&row>=0);up->setEnabled(i>=4&&row>0);down->setEnabled(i>=4&&row>=0&&row+1<groups_[i].size());if(row<0)add->setEnabled(false);auto *chosen=menu.exec(table->viewport()->mapToGlobal(pos));if(row<0||!chosen)return;if(targets.contains(chosen))addToCustom(targets[chosen]-4,groups_[i][row]);else if(chosen==remove)groups_[i].removeAt(row);else if(chosen==up)groups_[i].swapItemsAt(row,row-1);else if(chosen==down)groups_[i].swapItemsAt(row,row+1);save();refresh();});if(i>=4)for(int direction:{-1,1}){auto *key=new QShortcut(QKeySequence(direction<0?"Alt+Up":"Alt+Down"),table);key->setContext(Qt::WidgetWithChildrenShortcut);connect(key,&QShortcut::activated,this,[this,i,table,direction]{const int row=table->currentRow(),next=row+direction;if(row<0||next<0||next>=groups_[i].size())return;groups_[i].swapItemsAt(row,next);save();refresh();table->selectRow(next);});}tables_<<table;tabs_->addTab(table,names[i]);}state_=new QLabel(this);layout->addWidget(state_);auto *timer=new QTimer(this);connect(timer,&QTimer::timeout,this,[this]{refresh();});timer->start(250);refresh();}
 bool addToCustom(int group,int channel){if(group<0||group>=3||channel<0||channel>=LogChannelCount)return false;auto &channels=groups_[group+4];if(!channels.contains(channel))channels<<channel;save();refresh();return true;}
 void setReadings(const std::array<double,LogChannelCount> &v){live_.values=v;age_.start();refresh();}
 void disconnected(){live_.values.fill(std::numeric_limits<double>::quiet_NaN());age_.invalidate();refresh();}
 void inspect(const LogSample *sample){cursor_=sample?*sample:LogSample{};inspecting_=sample!=nullptr;refresh();}
private:
 void save(){for(int i=4;i<7;++i){QVariantList list;for(int c:groups_[i])list<<c;QSettings().setValue(QString("logging/textCustom%1").arg(i-3),list);}}
 void refresh(){const bool fresh=age_.isValid()&&age_.elapsed()<=2000;for(int group=0;group<7;++group){auto *table=tables_[group];table->setRowCount(groups_[group].size());for(int r=0;r<groups_[group].size();++r){int c=groups_[group][r];double value=inspecting_?cursor_.values[c]:fresh?live_.values[c]:std::numeric_limits<double>::quiet_NaN();QStringList cells{LogNames[c],std::isfinite(value)?QString::number(displayValue(c,value),'g',8):QStringLiteral("—"),displayUnit(c,LogUnits[c])};for(int col=0;col<3;++col){auto *item=table->item(r,col);if(!item){item=new QTableWidgetItem;table->setItem(r,col,item);}item->setText(cells[col]);QFont font=table->font();font.setItalic(inspecting_);item->setFont(font);item->setForeground(QColor(inspecting_?"#39baff":"#8bff00"));}}}state_->setText(inspecting_?tr("Cursor: %1 s — recorded data").arg(cursor_.seconds,0,'f',3):fresh?tr("Live ECU data"):tr("Live data unavailable"));}
 QTabWidget *tabs_;QVector<QTableWidget*> tables_;QVector<QVector<int>> groups_;QLabel *state_;LogSample live_,cursor_;bool inspecting_=false;QElapsedTimer age_;
};
}
