#pragma once
#include "SensorUi.h"
#include "../protocol/VssCodec.h"
#include "../protocol/Cam2Codec.h"
#include "../tools/ScopeWidget.h"
#include <QTimer>
#include <QElapsedTimer>
#include <QVBoxLayout>
#include <QPointer>
namespace tunuino {
class VssMonitor final:public QWidget {
public:
 enum class Source{Vss,Cam2};
 ToolExchange exchange;std::function<ToolSnapshot()> snapshot;std::function<bool()> ready;
 explicit VssMonitor(QWidget *parent=nullptr,Source source=Source::Vss):QWidget(parent),cam2_(source==Source::Cam2){setObjectName(cam2_?"cam2Monitor":"vssMonitor");auto *layout=new QVBoxLayout(this);layout->setContentsMargins(4,4,4,4);auto *bar=SensorUi::toolbar(this);auto *refresh=bar->addAction(CalibrationAppearance::icon(CalibrationAppearance::Defaults),cam2_?tr("Read CAM 2"):tr("Read VSS"));connect(refresh,&QAction::triggered,this,[this]{poll();});layout->addWidget(bar);layout->addWidget(SensorUi::section(this,cam2_?tr("CAM 2 - Live pulses"):tr("Vehicle speed - Live readings")));table_=new QTableWidget(3,2,this);SensorUi::grid(table_);const QStringList labels{tr("Frequency"),cam2_?tr("Period"):tr("Vehicle speed"),tr("Accepted pulses")};for(int i=0;i<3;++i){table_->setItem(i,0,new QTableWidgetItem(labels[i]));table_->setItem(i,1,new QTableWidgetItem("---"));}layout->addWidget(table_);state_=new QLabel(tr("Offline"),this);state_->setWordWrap(true);layout->addWidget(state_);auto *timer=new QTimer(this);timer->setInterval(250);connect(timer,&QTimer::timeout,this,[this]{poll();});timer->start();}
 QSize sizeHint()const override{return QSize(650,240);}
 void poll(){if(!isVisible())return;const auto current=snapshot?snapshot():ToolSnapshot{};if(!current.valid()||!(cam2_?Cam2Codec::supported(current.identity):VssCodec::supported(current.identity))){clear(tr("Offline or input firmware support unavailable"));return;}if(current.identity!=base_.identity||current.generation!=base_.generation){base_=current;clear(tr("Waiting for current ECU readings"));}if(age_.isValid()&&age_.elapsed()>2000)clear(tr("Stale readings"));if(waiting_||(ready&&!ready())||!exchange)return;const auto expected=current;QPointer<VssMonitor> self(this);waiting_=true;if(!exchange(cam2_?"TUNUINO:J":"TUNUINO:U",[self,expected](const QByteArray &body){if(!self)return;self->waiting_=false;const auto now=self->snapshot?self->snapshot():ToolSnapshot{};if(now.identity!=expected.identity||now.generation!=expected.generation||!(self->cam2_?Cam2Codec::reply(body,expected.generation):VssCodec::reply(body,expected.generation))){self->clear(tr("Invalid or previous-session input response"));return;}self->age_.start();if(body[2]==0){static const QStringList reasons{tr("OK"),tr("Disabled"),tr("Waiting for two pulses"),tr("No fresh pulses; stopped shaft and disconnected wiring cannot be distinguished"),tr("Frequency or speed outside native limits"),tr("Clock invalid"),tr("ECU configuration changed or dirty"),tr("Calibrate Speed ratio to enable km/h")};self->clear(reasons.value(quint8(body[3]),tr("Unavailable")));return;}self->table_->item(0,1)->setText(QString::number(VssCodec::u32(body,4)/1000.0,'f',3)+" Hz");self->table_->item(1,1)->setText(self->cam2_?QString::number(VssCodec::u32(body,12)/1000.0,'f',3)+" ms":(quint8(body[2])&2)?QString::number(VssCodec::u32(body,8)/1000.0,'f',3)+" km/h":QString("---"));self->table_->item(2,1)->setText(QString::number(VssCodec::u32(body,24)));self->state_->setText(self->cam2_?tr("Input pulses only. CAM 2 phase decoding and VVT control are unavailable."):(quint8(body[2])&2)?tr("Stored ECU configuration, generation %1").arg(expected.generation):tr("Frequency available. Calibrate Speed ratio to enable km/h."));})){waiting_=false;}}
private:
 void clear(const QString &text){for(int i=0;i<3;++i)table_->item(i,1)->setText("---");state_->setText(text);}
 bool cam2_;QTableWidget *table_;QLabel *state_;ToolSnapshot base_;QElapsedTimer age_;bool waiting_=false;
};
}
