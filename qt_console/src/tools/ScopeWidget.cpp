#include "ScopeWidget.h"
#include "../protocol/SensorCalibrationCodec.h"
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QDateTime>
#include <QFileDialog>
#include <QSaveFile>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
namespace tunuino {
namespace {
quint32 u32(const QByteArray &b,int i){return quint32(SensorCalibrationCodec::u16(b,i))<<16|quint32(SensorCalibrationCodec::u16(b,i+2));}
void put32(QByteArray &b,int i,quint32 n){SensorCalibrationCodec::put(b,i,n>>16);SensorCalibrationCodec::put(b,i+2,n);}
bool validSamples(const QByteArray &bytes){if(bytes.size()%5||bytes.size()>1280)return false;quint32 previous=0;for(int i=0;i<bytes.size();i+=5){const auto at=u32(bytes,i);if((i&&at<previous)||(bytes[i+4]!=1&&bytes[i+4]!=2))return false;previous=at;}return true;}
class ScopePlot final:public QWidget {
public:QByteArray samples;double zoom=1;
    explicit ScopePlot(QWidget *parent):QWidget(parent){setMinimumSize(420,240);}
protected:void paintEvent(QPaintEvent*)override{
    QPainter p(this);p.fillRect(rect(),QColor("#141619"));p.setPen(QColor("#d8dade"));
    if(samples.isEmpty()){p.drawText(rect(),Qt::AlignCenter,tr("No trigger capture"));return;}
    const double span=qMax(1.0,double(u32(samples,samples.size()-5)))/zoom;const QRectF plot(85,20,width()-105,height()-65);
    p.setPen(QColor("#44484c"));for(int i=0;i<=10;++i){double x=plot.left()+plot.width()*i/10;p.drawLine(QPointF(x,plot.top()),QPointF(x,plot.bottom()));p.setPen(QColor("#bfc3c8"));p.drawText(QRectF(x-25,plot.bottom()+5,50,20),Qt::AlignCenter,QString::number(span*i/10000,'f',1));p.setPen(QColor("#44484c"));}
    for(int channel=1;channel<=2;++channel){const double y=plot.top()+plot.height()*(channel==1?.3:.75);p.setPen(QColor("#bfc3c8"));p.drawText(QRectF(4,y-12,75,24),Qt::AlignVCenter,tr(channel==1?"Crank":"Cam"));p.setPen(QPen(channel==1?QColor("#42d5f0"):QColor("#ffc848"),1.5));p.drawLine(QPointF(plot.left(),y),QPointF(plot.right(),y));p.save();p.setClipRect(plot);for(int i=0;i<samples.size();i+=5)if(quint8(samples[i+4])==channel){const double x=plot.left()+plot.width()*u32(samples,i)/span;p.drawLine(QPointF(x,y),QPointF(x,y-35));}p.restore();}
    p.setPen(QColor("#bfc3c8"));p.drawText(QRectF(80,height()-24,width()-100,20),Qt::AlignCenter,tr("Captured rising edges — time from first edge (ms)"));
}};
}
ScopeWidget::ScopeWidget(QWidget *parent):QWidget(parent){auto *layout=new QVBoxLayout(this);auto *row=new QHBoxLayout;capture_=new QPushButton(tr("Capture"),this);auto *openButton=new QPushButton(tr("Open scope..."),this),*saveButton=new QPushButton(tr("Save scope..."),this),*zoomIn=new QPushButton(tr("Zoom in"),this),*zoomOut=new QPushButton(tr("Full capture"),this);for(auto *b:{capture_,openButton,saveButton,zoomIn,zoomOut})row->addWidget(b);layout->addLayout(row);plot_=new ScopePlot(this);layout->addWidget(plot_,1);status_=new QLabel(tr("Read-only crank/cam capture. Requires firmware with trigger Scope support."),this);status_->setWordWrap(true);layout->addWidget(status_);
    connect(capture_,&QPushButton::clicked,this,[this]{start();});connect(openButton,&QPushButton::clicked,this,[this]{open();});connect(saveButton,&QPushButton::clicked,this,[this]{save();});connect(zoomIn,&QPushButton::clicked,this,[this]{auto *p=static_cast<ScopePlot*>(plot_);p->zoom=qMin(16.0,p->zoom*2);p->update();});connect(zoomOut,&QPushButton::clicked,this,[this]{auto *p=static_cast<ScopePlot*>(plot_);p->zoom=1;p->update();});auto *timer=new QTimer(this);timer->setInterval(25);connect(timer,&QTimer::timeout,this,[this]{pump();});timer->start();}
void ScopeWidget::start(){if(busy())return;if(!ready||!ready()){status_->setText(tr("Connect a synchronized ECU with firmware Scope support; finish other ECU tools first."));return;}base_=snapshot?snapshot():ToolSnapshot{};if(!base_.valid()){status_->setText(tr("A synchronized ECU base is required for Scope capture."));return;}phase_=1;offset_=0;samples_.clear();deadline_=QDateTime::currentMSecsSinceEpoch()+5000;capture_->setEnabled(false);if(busyChanged)busyChanged(true);status_->setText(tr("Capturing trigger edges..."));pump();}
void ScopeWidget::pump(){if(!busy())return;const auto current=snapshot?snapshot():ToolSnapshot{};if(!current.valid()||current.identity!=base_.identity||current.generation!=base_.generation){finish(tr("ECU or generation changed; capture discarded."));return;}if(!ready||!ready()||QDateTime::currentMSecsSinceEpoch()>deadline_){finish(tr("Scope capture stopped: ECU unavailable or request timed out."));return;}if(waiting_||!exchange)return;QByteArray q("TUNUINO:S");if(phase_==1)q.append(char(0));else{QByteArray args(8,0);args[0]=1;put32(args,1,token_);SensorCalibrationCodec::put(args,5,offset_);args[7]=char(qMin(32,total_-offset_));q+=args;}waiting_=true;if(!exchange(q,[this](const QByteArray &b){waiting_=false;receive(b);}))waiting_=false;}
void ScopeWidget::receive(const QByteArray &b){if(!busy())return;if(b.size()<21||b[0]!=0||b[1]!=1||b[2]!=1){finish(tr("No valid capture. Check trigger configuration and firmware Scope support."));return;}
    if(phase_==1){total_=SensorCalibrationCodec::u16(b,3);generation_=u32(b,5);token_=u32(b,9);header_=b.left(21);if(!total_||total_>256||generation_!=base_.generation||!token_){finish(tr("Invalid capture header."));return;}phase_=2;}
    else{const int count=qMin(32,total_-offset_);if(b.left(21)!=header_||b.size()!=21+count*5){finish(tr("Capture changed while reading; try again."));return;}samples_+=b.mid(21);offset_+=count;if(offset_==total_){if(!validSamples(samples_)){finish(tr("Invalid edge sequence."));return;}auto *plot=static_cast<ScopePlot*>(plot_);plot->samples=samples_;plotGeneration_=generation_;plot->zoom=1;plot->update();finish(tr("%1 actual edges, ECU generation %2. Frozen capture; engine control was not changed.").arg(total_).arg(generation_));if(captured)captured(base_,header_,samples_);}}
}
void ScopeWidget::finish(const QString &message){phase_=0;waiting_=false;capture_->setEnabled(true);status_->setText(message);if(busyChanged)busyChanged(false);}
void ScopeWidget::save(){const auto &samples=static_cast<ScopePlot*>(plot_)->samples;if(samples.isEmpty())return;const auto path=QFileDialog::getSaveFileName(this,tr("Save scope"),{},tr("Tunuino scope (*.tnscope)"));if(path.isEmpty())return;QSaveFile file(path);const auto bytes=QJsonDocument(QJsonObject{{"format","tunuino-trigger-scope-v1"},{"edge","rising"},{"generation",double(plotGeneration_)},{"samples",QString::fromLatin1(samples.toHex())}}).toJson();if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())QMessageBox::warning(this,tr("Scope"),tr("Could not save the capture."));}
void ScopeWidget::open(){if(busy())return;const auto path=QFileDialog::getOpenFileName(this,tr("Open scope"),{},tr("Tunuino scope (*.tnscope)"));if(path.isEmpty())return;QFile file(path);if(!file.open(QIODevice::ReadOnly)||file.size()>16384)return;const auto object=QJsonDocument::fromJson(file.readAll()).object();const double generation=object["generation"].toDouble();const QString hex=object["samples"].toString();const auto samples=QByteArray::fromHex(hex.toLatin1());if(object["format"]!="tunuino-trigger-scope-v1"||object["edge"]!="rising"||!std::isfinite(generation)||generation<1||generation>4294967295.0||std::floor(generation)!=generation||samples.isEmpty()||hex.toLatin1()!=samples.toHex()||!validSamples(samples)){QMessageBox::warning(this,tr("Scope"),tr("Invalid scope file. Current capture was preserved."));return;}auto *plot=static_cast<ScopePlot*>(plot_);plot->samples=samples;plot->zoom=1;plot->update();generation_=quint32(generation);plotGeneration_=generation_;status_->setText(tr("Offline capture — no live ECU connection implied."));}
}
