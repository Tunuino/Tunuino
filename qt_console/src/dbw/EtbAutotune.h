#pragma once
#include "../tools/ScopeWidget.h"
#include "../tools/ServiceDataCodec.h"
#include "../protocol/EtbCodec.h"
#include "../ui/WriteButtonState.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QElapsedTimer>
#include <QPointer>
#include <QHideEvent>
#include <QPainter>
namespace tunuino {
class EtbTunePlot final:public QWidget {
public:
 QVector<QPointF> samples;explicit EtbTunePlot(QWidget *parent):QWidget(parent){setMinimumHeight(150);}
 void append(double target,double measured){samples.append({target,measured});if(samples.size()>200)samples.removeFirst();update();}
protected:
 void paintEvent(QPaintEvent*)override{QPainter p(this);p.fillRect(rect(),QColor("#171b1e"));const QRectF area=rect().adjusted(38,14,-12,-25);p.setPen(QColor("#596169"));for(int i=0;i<=4;++i){double y=area.bottom()-area.height()*i/4;p.drawLine(QPointF(area.left(),y),QPointF(area.right(),y));p.drawText(QRectF(0,y-8,32,16),Qt::AlignRight,QString::number(i*25));}for(int track=0;track<2;++track){p.setPen(QPen(track?QColor("#2dd7e6"):QColor("#edb547"),2));for(int i=1;i<samples.size();++i){const auto value=[&](int n){return track?samples[n].y():samples[n].x();};p.drawLine(QPointF(area.left()+area.width()*(i-1)/199,area.bottom()-area.height()*value(i-1)/100),QPointF(area.left()+area.width()*i/199,area.bottom()-area.height()*value(i)/100));}}p.setPen(QColor("#dddddd"));p.drawText(QRectF(35,height()-22,width()-45,20),Qt::AlignCenter,tr("Target (yellow) / measured position (cyan), % — actual ECU samples"));}
};
class EtbAutotune final:public QWidget {
public:
 ToolExchange exchange;std::function<ToolSnapshot()> snapshot;std::function<bool()> ready;
 std::function<void(bool)> busyChanged;std::function<bool()> configurationPending;
 std::function<bool(double,double,double,QString&)> applyCandidate;
 std::function<void()> writePid;std::function<bool()> pidWriteEnabled;std::function<bool()> pidPending;
 explicit EtbAutotune(int channel):channel_(channel){
  setObjectName(QString("etb%1Autotune").arg(channel+1));auto *layout=new QVBoxLayout(this);
  auto *section=new QLabel(tr("ETB %1 — PID Autotune").arg(channel+1),this);section->setStyleSheet("background:#666764;color:white;font-weight:bold;padding:5px");layout->addWidget(section);
  auto *instructions=new QLabel(tr("Uses stored ECU sensor calibrations and H-bridge assignment. Connect the throttle supply, stop the engine, disarm engine outputs and release the pedal. Start moves only this throttle around a 40% test point. Abort stops the experiment. Apply prepares a PID proposal locally; Write to ECU saves it explicitly."),this);instructions->setWordWrap(true);layout->addWidget(instructions);
  readings_=new QLabel(tr("No ECU readings"),this);readings_->setObjectName("etbReadings");layout->addWidget(readings_);plot_=new EtbTunePlot(this);layout->addWidget(plot_,1);
  status_=new QLabel(tr("Ready to read the ECU"),this);status_->setObjectName("etbTuneStatus");status_->setWordWrap(true);layout->addWidget(status_);
  auto *row=new QHBoxLayout;start_=new QPushButton(tr("Start Autotune"),this);start_->setObjectName("etbTuneStart");abort_=new QPushButton(tr("Abort"),this);abort_->setObjectName("etbTuneAbort");apply_=new QPushButton(tr("Apply PID proposal"),this);apply_->setObjectName("etbTuneApply");write_=new QPushButton(tr("Write to ECU"),this);write_->setObjectName("etbTuneWrite");for(auto *button:{start_,abort_,apply_,write_})row->addWidget(button);layout->addLayout(row);
  connect(start_,&QPushButton::clicked,this,[this]{start();});connect(abort_,&QPushButton::clicked,this,[this]{abort();});connect(apply_,&QPushButton::clicked,this,[this]{apply();});connect(write_,&QPushButton::clicked,this,[this]{if(writePid)writePid();});
  auto *timer=new QTimer(this);timer->setInterval(150);connect(timer,&QTimer::timeout,this,[this]{poll();});timer->start();refresh();
 }
 QSize sizeHint()const override{return QSize(820,480);}
 bool busy()const{return active_||starting_;}
 void start(){
  if(busy()||waiting_||!exchange||(ready&&!ready()))return;if(configurationPending&&configurationPending()){status_->setText(tr("Write or discard pending ETB/input changes before Autotune."));return;}const auto base=snapshot?snapshot():ToolSnapshot{};if(!base.valid()||!EtbCodec::supported(base.identity)||base.pages[11].size()!=256||!base.pages[11][EtbCodec::block(channel_)+1]){status_->setText(tr("Write the sensor calibrations and H-bridge assignment before starting."));return;}
  base_=base;candidate_.clear();starting_=true;cancel_=false;dismissed_=false;plot_->samples.clear();if(busyChanged)busyChanged(true);refresh();
  const auto q=command(13)+QByteArray("ETB!",4);QPointer<EtbAutotune> self(this);waiting_=true;
  if(!exchange(q,[self](const QByteArray &p){if(!self)return;self->waiting_=false;self->starting_=false;if(p!=QByteArray(1,0)){self->finish(tr("ECU refused the experiment or communication failed. Check stopped engine, disarmed outputs, pedal, calibrated tracks and supply."));return;}self->active_=true;self->status_->setText(tr("Autotune running on the ECU. Do not move the pedal."));self->refresh();})){waiting_=false;finish(tr("ECU busy. No Autotune start was sent."));}
 }
 void abort(){if(!busy()){if(candidate_.isEmpty())return;candidate_.clear();dismissed_=true;status_->setText(tr("PID proposal discarded. No ECU write."));refresh();return;}cancel_=true;status_->setText(tr("Stopping the experiment..."));poll();}
 void poll(){
  refresh();if(!isVisible()&&!busy())return;if(waiting_||!exchange||(ready&&!ready()))return;
  const auto current=snapshot?snapshot():ToolSnapshot{};
  if(!current.valid()||!EtbCodec::supported(current.identity)){if(busy())finish(tr("ECU communication lost. Heartbeats stopped; the native experiment lease shuts the bridge down."));else status_->setText(tr("Offline or ETB firmware unavailable"));return;}
  if(busy()&&(current.identity!=base_.identity||current.generation!=base_.generation||current.pages[11]!=base_.pages[11])){cancel_=true;finish(tr("ECU configuration changed. Experiment lease released."));return;}
  if(!busy()&&!candidate_.isEmpty()&&(current.identity!=base_.identity||current.generation!=base_.generation||current.pages[11]!=base_.pages[11])){candidate_.clear();dismissed_=true;status_->setText(tr("ECU changed. Obtain a new proposal."));}
  if(!busy())base_=current;
  if(active_){QPointer<EtbAutotune> self(this);waiting_=true;const int operation=cancel_?14:15;
   if(!exchange(command(operation),[self](const QByteArray &p){if(!self)return;self->waiting_=false;if(p!=QByteArray(1,0)){self->finish(tr("Experiment stopped or the ECU rejected the lease. No automatic restart."));return;}self->read();})){waiting_=false;}
  }else read();
 }
protected:
 void hideEvent(QHideEvent *event)override{abort();QWidget::hideEvent(event);}
private:
 QByteArray command(int op)const{QByteArray q("TUNUINO:X",9);q+=char(op);q+=char(channel_);if(op!=12){QByteArray generation(4,0);SensorCalibrationCodec::put(generation,0,base_.generation>>16);SensorCalibrationCodec::put(generation,2,base_.generation);q+=generation;}return q;}
 void read(){
  if(waiting_||!exchange)return;waiting_=true;QPointer<EtbAutotune> self(this);const auto source=base_;QElapsedTimer request;request.start();
  if(!exchange(command(12),[self,source,request](const QByteArray &p){
   if(!self)return;self->waiting_=false;const auto current=self->snapshot?self->snapshot():ToolSnapshot{};
   if(current.identity!=source.identity||current.generation!=source.generation||request.elapsed()>1500||!EtbCodec::reply(p,self->channel_,source.generation)){if(self->busy())self->finish(tr("Invalid or old ETB response. Heartbeats stopped."));else self->status_->setText(tr("No current ETB state"));return;}
   const auto u=[&](int at){return SensorCalibrationCodec::u16(p,at);};const bool measured=p[49]&&(quint8(p[51])&2);
   self->readings_->setText(tr("Target %1% | Position %2% | Motor command %3% | TPS main %4 V / check %5 V").arg(u(12)/100.,0,'f',2).arg(measured?QString::number(u(14)/100.,'f',2):QString("---")).arg(qint16(u(16))/100.,0,'f',2).arg(u(20)/1000.,0,'f',3).arg(u(22)/1000.,0,'f',3));
   if(measured)self->plot_->append(u(12)/100.,u(14)/100.);
   const int phase=quint8(p[28]);if(phase==3&&!self->dismissed_&&u(32)>0&&u(32)<=20000&&u(34)<=20000&&u(36)<=5000){self->candidate_=p;self->status_->setText(tr("Measured proposal: P %1, I %2, D %3. Review and Apply; nothing has been written.").arg(u(32)/1000.,0,'f',3).arg(u(34)/1000.,0,'f',3).arg(u(36)/1000.,0,'f',3));if(self->busy())self->release();}
   else if(self->active_&&(phase==4||phase==5||phase==0)){self->finish(phase==4?tr("Autotune aborted. Motor disabled; no PID change."):tr("Autotune stopped without a valid proposal. Check signal agreement, movement and supply."));}
   else if(self->active_)self->status_->setText(phase==1?tr("Approaching the test position..."):tr("Measuring oscillations: %1 / 6 cycles").arg(quint8(p[30])));
   self->refresh();
  })){waiting_=false;}
 }
 void release(){bool old=busy();active_=starting_=false;cancel_=false;if(old&&busyChanged)busyChanged(false);refresh();}
 void finish(const QString &message){candidate_.clear();release();status_->setText(message);}
 void apply(){
  if(candidate_.isEmpty()||busy()||!applyCandidate)return;const auto current=snapshot?snapshot():ToolSnapshot{};
  if(current.identity!=base_.identity||current.generation!=base_.generation||current.pages[11]!=base_.pages[11]){finish(tr("ECU changed. Obtain a new proposal."));return;}
  QString error;if(!applyCandidate(SensorCalibrationCodec::u16(candidate_,32)/1000.,SensorCalibrationCodec::u16(candidate_,34)/1000.,SensorCalibrationCodec::u16(candidate_,36)/1000.,error)){status_->setText(error);return;}
  status_->setText(tr("PID proposal prepared locally. Review it in PID and use Write to ECU."));candidate_.clear();dismissed_=true;refresh();
 }
 void refresh(){const auto base=snapshot?snapshot():ToolSnapshot{};const bool online=base.valid()&&EtbCodec::supported(base.identity);start_->setEnabled(online&&!busy()&&!waiting_&&(!ready||ready())&&(!configurationPending||!configurationPending()));abort_->setEnabled(busy()||!candidate_.isEmpty());abort_->setText(busy()?tr("Abort"):tr("Discard proposal"));apply_->setEnabled(online&&!busy()&&!candidate_.isEmpty());write_->setEnabled(!busy()&&pidWriteEnabled&&pidWriteEnabled());showPendingWrite(write_,pidPending&&pidPending());}
 int channel_;bool active_=false,starting_=false,waiting_=false,cancel_=false,dismissed_=false;ToolSnapshot base_;QByteArray candidate_;QLabel *readings_,*status_;EtbTunePlot *plot_;QPushButton *start_,*abort_,*apply_,*write_;
};
}
