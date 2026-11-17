#pragma once
#include "SensorUi.h"
#include "../tools/ScopeWidget.h"
#include "../tools/ServiceDataCodec.h"
#include "../protocol/Vvt2Codec.h"
#include <QTimer>
#include <QElapsedTimer>
#include <QPointer>
namespace tunuino {
class CamSignalWidget final:public QWidget {
public:
 ToolExchange exchange;std::function<ToolSnapshot()> snapshot;std::function<bool()> ready;
 explicit CamSignalWidget(bool second):second_(second){
  setObjectName(second?"cam2SignalMonitor":"cam1SignalMonitor");auto *layout=new QVBoxLayout(this);layout->addWidget(SensorUi::section(this,second?tr("CAM 2 - Signal and phase"):tr("CAM 1 - Signal and phase")));
  table_=new QTableWidget(4,2,this);SensorUi::grid(table_);const QStringList labels{tr("Crank synchronisation"),tr("Engine cycle synchronisation"),tr("Captured edge phase"),tr("Feedback age")};for(int i=0;i<4;++i){table_->setItem(i,0,new QTableWidgetItem(labels[i]));table_->setItem(i,1,new QTableWidgetItem("---"));}layout->addWidget(table_);state_=new QLabel(this);state_->setWordWrap(true);layout->addWidget(state_);
  auto *timer=new QTimer(this);timer->setInterval(250);connect(timer,&QTimer::timeout,this,[this]{poll();});timer->start();
 }
 QSize sizeHint()const override{return QSize(650,280);}
 void poll(){
  if(!isVisible())return;const auto base=snapshot?snapshot():ToolSnapshot{};
  if(!base.diagnosticValid()||!(second_?Vvt2Codec::supported(base.identity):VvtCodec::supported(base.identity))){clear(tr("Offline or phase acquisition firmware unavailable"));return;}
  if(age_.isValid()&&age_.elapsed()>1500)clear(tr("Stale ECU readings"));
  if(waiting_||!exchange||(ready&&!ready()))return;waiting_=true;QPointer<CamSignalWidget> self(this);QElapsedTimer requested;requested.start();
  if(!exchange(second_?"TUNUINO:G":"TUNUINO:H",[self,base,requested](const QByteArray &p){
   if(!self)return;self->waiting_=false;const auto current=self->snapshot?self->snapshot():ToolSnapshot{};
   if(current.identity!=base.identity||current.generation!=base.generation||requested.elapsed()>1500||p.size()!=32||p[0]||p[1]!=1||quint8(p[2])>7||p[3]||p[31]||ServiceDataCodec::u32(p,24)!=base.generation||SensorCalibrationCodec::u16(p,28)!=quint8(base.pages[0][3])||p[30]!=base.pages[0][4]){self->clear(tr("Invalid or previous-session phase response"));return;}
   self->age_.start();const auto edge=ServiceDataCodec::u64(p,8),sample=ServiceDataCodec::u64(p,16);const auto phase=ServiceDataCodec::u32(p,4);const bool valid=(quint8(p[2])&7)==7&&sample>=edge&&sample-edge<=1000000&&phase<72000;
   self->table_->item(0,1)->setText(p[2]&1?tr("Yes"):tr("No"));self->table_->item(1,1)->setText(p[2]&2?tr("Yes"):tr("No"));self->table_->item(2,1)->setText(valid?QString::number(phase/100.,'f',2)+" °":"---");self->table_->item(3,1)->setText(valid?QString::number((sample-edge)/1000.,'f',1)+" ms":"---");self->state_->setText(valid?tr("Measured edge relative to the synchronized 720° engine cycle. Generation %1.").arg(base.generation):tr("Waiting for synchronized, fresh cam feedback. Pulse detection is shown separately."));
  }))waiting_=false;
 }
private:
 void clear(const QString &message){for(int i=0;i<4;++i)table_->item(i,1)->setText("---");state_->setText(message);}
 bool second_,waiting_=false;QElapsedTimer age_;QTableWidget *table_;QLabel *state_;
};
}
