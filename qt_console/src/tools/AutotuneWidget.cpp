#include "AutotuneWidget.h"
#include "../calibration/SensorUi.h"
#include "../ui/ShortcutIcons.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSplitter>
#include <QActionGroup>
#include <QMenu>
#include <QSettings>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QStyledItemDelegate>
#include <QSignalBlocker>
#include <QScrollBar>
#include <algorithm>
namespace tunuino {
namespace {
class LoadTitle final:public QWidget {
public:explicit LoadTitle(QWidget *parent):QWidget(parent){setFixedWidth(18);}
protected:void paintEvent(QPaintEvent*)override{QPainter p(this);p.setPen(QColor("#e5e8dc"));p.setFont(QFont("Segoe UI",8));p.drawText(rect(),Qt::AlignCenter,tr("M\nA\nP\n\nk\nP\na"));}
};
class RpmAxis final:public QWidget {
public:RpmAxis(QTableWidget *table,QWidget *parent):QWidget(parent),table_(table){setObjectName("autotuneRpmAxis");setFixedHeight(23);connect(table->horizontalScrollBar(),&QScrollBar::valueChanged,this,[this]{update();});connect(table->horizontalHeader(),&QHeaderView::geometriesChanged,this,[this]{update();});}
protected:void paintEvent(QPaintEvent*)override{QPainter p(this);p.fillRect(rect(),QColor("#252829"));p.setFont(QFont("Segoe UI",8));p.setClipRect(QRect(table_->x()+table_->viewport()->x(),0,table_->viewport()->width(),height()));for(int c=0;c<16;++c){QRect r(table_->x()+table_->viewport()->x()+table_->columnViewportPosition(c),1,table_->columnWidth(c),20);p.fillRect(r,QColor("#4c5150"));p.setPen(QColor("#858b80"));p.drawRect(r);p.setPen(QColor("#e5e8dc"));auto *i=table_->horizontalHeaderItem(c);if(i)p.drawText(r,Qt::AlignCenter,i->text());}}
private:QTableWidget *table_;
};
class CoverageDelegate final:public QStyledItemDelegate {
public:using QStyledItemDelegate::QStyledItemDelegate;
 void paint(QPainter *p,const QStyleOptionViewItem &o,const QModelIndex &i)const override{
  p->save();p->setClipRect(o.rect);const bool selected=o.state&QStyle::State_Selected;
  p->fillRect(o.rect,selected?QColor("#078bda"):i.data(Qt::BackgroundRole).value<QBrush>());
  if(i.data(Qt::UserRole).toBool()){p->setPen(QPen(QColor(30,40,30,100),1));for(int x=o.rect.left()-o.rect.height();x<o.rect.right();x+=6)p->drawLine(x,o.rect.bottom(),x+o.rect.height(),o.rect.top());}
  QFont font=o.font;const double point=font.pointSizeF()>0?font.pointSizeF():9;const double ratio=(o.rect.width()-2)/QFontMetricsF(font).horizontalAdvance("300.0");font.setPointSizeF(qBound(5.,point*ratio,9.));p->setFont(font);p->setPen(selected?Qt::white:QColor("#18231b"));p->drawText(o.rect.adjusted(1,0,-1,0),Qt::AlignCenter,i.data().toString());p->restore();
 }
};
QIcon textIcon(const QString &text,const QColor &color){QPixmap pix(26,26);pix.fill(Qt::transparent);QPainter p(&pix);p.setPen(color);p.setFont(QFont("Segoe UI",12,QFont::Bold));p.drawText(pix.rect(),Qt::AlignCenter,text);return QIcon(pix);}
}
class AutotuneSurface final:public QWidget {
public:
 explicit AutotuneSurface(QTableWidget *table,QWidget *parent):QWidget(parent),table_(table){setObjectName("autotuneSurface");setMinimumSize(140,120);setCursor(Qt::OpenHandCursor);}
 bool follow=false;QString unit="VE";
protected:
 void mousePressEvent(QMouseEvent *e)override{if(e->button()==Qt::LeftButton){drag_=true;last_=e->position();setCursor(Qt::ClosedHandCursor);}}
 void mouseReleaseEvent(QMouseEvent*)override{drag_=false;setCursor(Qt::OpenHandCursor);}
 void mouseMoveEvent(QMouseEvent *e)override{if(drag_){const auto d=e->position()-last_;yaw_+=d.x()*.012;pitch_=qBound(.1,pitch_+d.y()*.008,1.5);last_=e->position();update();}}
 void wheelEvent(QWheelEvent *e)override{zoom_=qBound(.4,zoom_*(e->angleDelta().y()>0?1.12:1/1.12),2.5);update();e->accept();}
 void mouseDoubleClickEvent(QMouseEvent*)override{yaw_=.65;pitch_=.65;zoom_=1;update();}
 void paintEvent(QPaintEvent*)override{
  QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.fillRect(rect(),QColor("#202326"));
  bool known[16][16];double values[16][16],low=1e30,high=-1e30;bool any=false;
  for(int r=0;r<16;++r)for(int c=0;c<16;++c){auto *i=table_->item(r,c);bool ok=false;const double v=i?i->text().toDouble(&ok):0;known[r][c]=ok;values[r][c]=ok?v:0;if(ok){low=(std::min)(low,v);high=(std::max)(high,v);any=true;}}
  if(!any){p.setPen(QColor("#c5c9c3"));p.drawText(rect(),Qt::AlignCenter,tr("No measured data for this view"));return;}
  const double span=(std::max)(1.,high-low),cy=std::cos(yaw_),sy=std::sin(yaw_),cp=std::cos(pitch_),sp=std::sin(pitch_);
  struct V{QPointF point;double depth;};V vertices[16][16];double left=1e30,right=-1e30,top=1e30,bottom=-1e30;
  for(int r=0;r<16;++r)for(int c=0;c<16;++c){const double x=(c-7.5)/7.5,y=(r-7.5)/7.5,z=((known[r][c]?values[r][c]:low)-low)/span*.9;const double rx=x*cy-y*sy,ry=x*sy+y*cy;vertices[r][c]={QPointF(rx,ry*cp-z*sp),ry*sp+z*cp};auto q=vertices[r][c].point;left=(std::min)(left,q.x());right=(std::max)(right,q.x());top=(std::min)(top,q.y());bottom=(std::max)(bottom,q.y());}
  const QRectF area=QRectF(rect()).adjusted(28,28,-28,-44);const double scale=(std::min)(area.width()/(right-left),area.height()/(bottom-top))*.88*zoom_;const QPointF origin((left+right)/2,(top+bottom)/2);
  const auto screen=[&](int r,int c){return area.center()+(vertices[r][c].point-origin)*scale;};
  struct Face{int r,c;double depth;};QVector<Face> faces;for(int r=0;r<15;++r)for(int c=0;c<15;++c)faces.append({r,c,(vertices[r][c].depth+vertices[r+1][c].depth+vertices[r][c+1].depth+vertices[r+1][c+1].depth)/4});std::sort(faces.begin(),faces.end(),[](const Face&a,const Face&b){return a.depth<b.depth;});
  p.save();p.setClipRect(area);for(const auto &f:faces){if(!known[f.r][f.c]||!known[f.r+1][f.c]||!known[f.r][f.c+1]||!known[f.r+1][f.c+1])continue;const double level=qBound(0.,(values[f.r][f.c]-low)/span,1.);p.setPen(QPen(QColor("#244e20"),.6));p.setBrush(QColor::fromHsvF(.33-level*.25,.78,.85));p.drawPolygon(QPolygonF{screen(f.r,f.c),screen(f.r+1,f.c),screen(f.r+1,f.c+1),screen(f.r,f.c+1)});}
  p.setPen(Qt::NoPen);p.setBrush(QColor("#fff08d"));for(int r=0;r<16;++r)for(int c=0;c<16;++c)if(known[r][c]&&!table_->item(r,c)->data(Qt::UserRole).toBool())p.drawEllipse(screen(r,c),2.5,2.5);
  const int row=table_->currentRow(),col=table_->currentColumn();if(follow&&row>=0&&col>=0){p.setPen(QPen(QColor("#fff28a"),1.6));for(int n=0;n<15;++n){p.drawLine(screen(row,n),screen(row,n+1));p.drawLine(screen(n,col),screen(n+1,col));}p.setBrush(Qt::white);p.drawEllipse(screen(row,col),3,3);}
  p.restore();p.setPen(QPen(QColor("#dfe3d9"),1.2));p.drawLine(screen(15,0),screen(15,15));p.drawLine(screen(15,0),screen(0,0));p.setFont(QFont("Segoe UI",8));
  for(int c:{0,5,10,15})if(auto *label=table_->horizontalHeaderItem(c))p.drawText(QRectF(screen(15,c)+QPointF(-15,4),QSizeF(40,18)),Qt::AlignCenter,label->text());
  for(int r:{0,5,10,15})if(auto *label=table_->verticalHeaderItem(r))p.drawText(QRectF(screen(r,0)+QPointF(-42,-8),QSizeF(42,18)),Qt::AlignLeft,label->text());
  p.setPen(QColor("#dde5de"));p.drawText(QRect(8,4,width()-16,22),Qt::AlignLeft,unit);p.drawText(QRect(8,height()-35,width()-16,30),Qt::AlignCenter,tr("RPM (rpm)  /  MAP kPa"));
 }
private:QTableWidget *table_;bool drag_=false;QPointF last_;double yaw_=.65,pitch_=.65,zoom_=1;
};
AutotuneWidget::AutotuneWidget(QWidget *parent):QWidget(parent){
 setObjectName("autotuneReview");setStyleSheet("QWidget#autotuneReview{background:#252829;}");setWindowTitle(tr("Autotune - PROPOSED VE"));setMinimumSize(340,240);
 QSettings prefs;settings_.rpmMin=prefs.value("autotune/rpmMin",500).toDouble();settings_.rpmMax=prefs.value("autotune/rpmMax",15000).toDouble();settings_.tpsMin=prefs.value("autotune/tpsMin",0).toDouble();settings_.tpsMax=prefs.value("autotune/tpsMax",100).toDouble();settings_.mapMin=prefs.value("autotune/mapMin",20).toDouble();settings_.mapMax=prefs.value("autotune/mapMax",400).toDouble();settings_.delay=qBound(0,prefs.value("autotune/delay",500).toInt(),10000);
 const auto valid=[](double a,double b,double max){return std::isfinite(a)&&std::isfinite(b)&&a>=0&&a<b&&b<=max;};if(!valid(settings_.rpmMin,settings_.rpmMax,20000)||!valid(settings_.mapMin,settings_.mapMax,500)||!valid(settings_.tpsMin,settings_.tpsMax,100))settings_=Settings{};
 auto *layout=new QVBoxLayout(this);layout->setContentsMargins(4,4,4,4);layout->setSpacing(3);auto *bar=SensorUi::toolbar(this);bar->setObjectName("autotuneToolbar");layout->addWidget(bar);
 splitter_=new QSplitter(Qt::Horizontal,this);splitter_->setObjectName("autotuneSplitter");tablePane_=new QWidget(splitter_);tablePane_->setMinimumSize(130,120);auto *grid=new QVBoxLayout(tablePane_);grid->setContentsMargins(0,0,0,0);grid->setSpacing(2);
 table_=new QTableWidget(16,16,tablePane_);table_->setObjectName("autotuneTable");table_->setMinimumSize(100,80);table_->setEditTriggers(QAbstractItemView::NoEditTriggers);table_->setSelectionMode(QAbstractItemView::ExtendedSelection);table_->setItemDelegate(new CoverageDelegate(table_));table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);table_->horizontalHeader()->setMinimumSectionSize(22);table_->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);table_->verticalHeader()->setMinimumSectionSize(12);table_->setStyleSheet("QTableWidget{background:#252829;color:#18231b;gridline-color:#737d70;font:9pt 'Segoe UI';}QHeaderView::section{background:#4c5150;color:#e5e8dc;padding:1px;border:1px solid #858b80;font:8pt 'Segoe UI';}");table_->setLayoutDirection(Qt::LeftToRight);
 // Match the Fuel Table: load rises on the left, RPM increases along the bottom.
 table_->horizontalHeader()->hide();auto *tableRow=new QHBoxLayout;tableRow->setContentsMargins(0,0,0,0);tableRow->setSpacing(1);tableRow->addWidget(new LoadTitle(tablePane_));tableRow->addWidget(table_,1);grid->addLayout(tableRow,1);grid->addWidget(new RpmAxis(table_,tablePane_));auto *axes=new QLabel(tr("RPM"),tablePane_);axes->setAlignment(Qt::AlignCenter);grid->addWidget(axes);surface_=new AutotuneSurface(table_,splitter_);splitter_->setStretchFactor(0,1);splitter_->setStretchFactor(1,1);splitter_->setSizes({500,500});layout->addWidget(splitter_,1);
 state_=new QLabel(this);state_->setObjectName("autotuneState");state_->setWordWrap(true);state_->setStyleSheet("color:#d7d9ce;font:8pt 'Segoe UI';");layout->addWidget(state_);
 const auto action=[&](const QString &id,const QString &tip,const QIcon &icon,const std::function<void()> &fn){auto *a=bar->addAction(icon,tip);a->setObjectName(id);a->setToolTip(tip);connect(a,&QAction::triggered,this,[fn]{fn();});return a;};
 const auto view=[&](int type){tablePane_->setVisible(type!=1);surface_->setVisible(type!=0);if(type>=2)splitter_->setOrientation(type==2?Qt::Vertical:Qt::Horizontal);};
 action("autotuneTableOnly",tr("Only 3D table"),style()->standardIcon(QStyle::SP_FileDialogDetailedView),[=]{view(0);});action("autotuneGraphOnly",tr("Only 3D graph"),shortcutIcon(ShortcutGlyph::Graph),[=]{view(1);});action("autotuneSplitVertical",tr("Split vertically"),textIcon(QString::fromUtf8("Ⅱ"),Qt::white),[=]{view(2);});action("autotuneSplitHorizontal",tr("Split horizontally"),textIcon("=",Qt::white),[=]{view(3);});bar->addSeparator();
 auto *follow=action("autotuneFollow",tr("Follow cursor"),textIcon("+",Qt::white),[this]{surface_->follow=!surface_->follow;surface_->update();});follow->setCheckable(true);
 configure_=action("autotuneRefresh",tr("Refresh autotune / parameters"),style()->standardIcon(QStyle::SP_BrowserReload),[this]{configure();});bar->addSeparator();auto *group=new QActionGroup(this);const QString labels[]={tr("PROPOSED VE"),tr("ACTUAL LAMBDA"),tr("NUMBER OF SAMPLES"),tr("LAMBDA DIFFERENCE"),tr("VE DIFFERENCE")};const QString glyphs[]={"VE",QString::fromUtf8("λ"),QString::fromUtf8("Σ"),QString::fromUtf8("Δλ"),"ΔVE"};
 for(int i=0;i<5;++i){auto *a=action(QString("autotuneMode%1").arg(i),labels[i],textIcon(glyphs[i],i==1||i==3?QColor("#ef5b50"):QColor("#e2e8d2")),[this,i,labels]{mode_=i;setWindowTitle(tr("Autotune - ")+labels[i]);refresh();});a->setCheckable(true);a->setChecked(i==0);group->addAction(a);}
 bar->addSeparator();action("autotuneHelp",tr("Help"),CalibrationAppearance::icon(CalibrationAppearance::Help),[this]{QMessageBox::information(this,tr("Autotune"),tr("Collect verified ECU data with a prepared lambda target, stop, then review. Hatched cells have fewer than five accepted samples. Right-click selected cells: Apply selection 100% (S) or 50% (Alt+S), to the local Fuel Table only. No ECU write occurs here. Proposed correction retains 50% strength and ±10% per review. CLT must be 70–130 °C. Only native VE table 1 is supported; closed-loop and transient corrections must be disabled. Arbitrary CSV logs cannot establish calibration provenance."));});
 bar->addSeparator();start_=action("autotuneStart",tr("Start collection"),textIcon(QString::fromUtf8("▶"),Qt::white),[this]{start();});stop_=action("autotuneStop",tr("Stop collection"),textIcon(QString::fromUtf8("■"),Qt::white),[this]{stop();});
 apply100_=new QAction(tr("Apply selection 100%"),this);apply100_->setObjectName("autotuneApply100");apply100_->setShortcut(QKeySequence("S"));apply100_->setShortcutContext(Qt::WidgetWithChildrenShortcut);addAction(apply100_);connect(apply100_,&QAction::triggered,this,[this]{apply(100);});apply50_=new QAction(tr("Apply selection 50%"),this);apply50_->setObjectName("autotuneApply50");apply50_->setShortcut(QKeySequence("Alt+S"));apply50_->setShortcutContext(Qt::WidgetWithChildrenShortcut);addAction(apply50_);connect(apply50_,&QAction::triggered,this,[this]{apply(50);});table_->setContextMenuPolicy(Qt::CustomContextMenu);connect(table_,&QWidget::customContextMenuRequested,this,[this](QPoint pos){QMenu menu(this);menu.addAction(apply100_);menu.addAction(apply50_);menu.exec(table_->viewport()->mapToGlobal(pos));});
 connect(table_,&QTableWidget::itemSelectionChanged,this,[this]{QVector<int> cells;for(auto *item:table_->selectedItems())cells<<(15-item->row())*16+item->column();if(selectionChanged)selectionChanged(cells);surface_->update();bool eligible=false;for(int i:cells)eligible|=counts_[i]>=5;apply100_->setEnabled(!collecting_&&!invalidated_&&base_.valid()&&eligible&&!proposal().isEmpty());apply50_->setEnabled(apply100_->isEnabled());});connect(table_,&QTableWidget::currentCellChanged,this,[this]{surface_->update();});refresh();
}
void AutotuneWidget::configure(){
 if(collecting_){state_->setText(tr("Stop collection before changing Autotune parameters."));return;}
 QDialog dialog(this);dialog.setObjectName("autotuneSetup");dialog.setWindowTitle(tr("Autotune"));auto *layout=new QVBoxLayout(&dialog);layout->setContentsMargins(5,5,5,5);layout->addWidget(SensorUi::section(&dialog,tr("Autotune")));auto *fields=new QTableWidget(8,2,&dialog);fields->setObjectName("autotuneSetupFields");SensorUi::grid(fields);fields->setStyleSheet(fields->styleSheet()+"QTableWidget{color:#8bff00;}");layout->addWidget(fields);
 const QString names[]={tr("Minimum RPM"),tr("Maximum RPM"),tr("Minimum TPS(%)"),tr("Maximum TPS(%)"),tr("Minimum MAP(kPa)"),tr("Maximum MAP(kPa)"),tr("Transients delay(ms)"),tr("VE table")};const char *ids[]={"rpmMin","rpmMax","tpsMin","tpsMax","mapMin","mapMax","delay","veTable"};const double values[]={settings_.rpmMin,settings_.rpmMax,settings_.tpsMin,settings_.tpsMax,settings_.mapMin,settings_.mapMax,double(settings_.delay),1};QVector<QDoubleSpinBox*> edits;
 for(int i=0;i<8;++i){fields->setItem(i,0,new QTableWidgetItem(names[i]));fields->item(i,0)->setForeground(QColor("#8bff00"));auto *edit=new QDoubleSpinBox(fields);edit->setObjectName(ids[i]);edit->setDecimals(i==2||i==3||i==4||i==5?1:0);edit->setRange(0,i<2?20000:i<4?100:i<6?500:i==6?10000:1);edit->setValue(values[i]);edit->setButtonSymbols(QAbstractSpinBox::NoButtons);if(i==7){edit->setEnabled(false);edit->setToolTip(tr("Only native VE table 1 is implemented."));}fields->setCellWidget(i,1,edit);edits<<edit;}
 auto *error=new QLabel(&dialog);error->setWordWrap(true);layout->addWidget(error);auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{for(int i:{0,2,4})if(edits[i]->value()>=edits[i+1]->value()){error->setText(tr("Each minimum must be below its maximum."));return;}dialog.accept();});dialog.resize(420,350);
 if(dialog.exec()==QDialog::Accepted){settings_={edits[0]->value(),edits[1]->value(),edits[2]->value(),edits[3]->value(),edits[4]->value(),edits[5]->value(),int(edits[6]->value())};QSettings prefs;for(int i=0;i<7;++i)prefs.setValue(QString("autotune/")+ids[i],edits[i]->value());reprocess();}
 review();
}
void AutotuneWidget::review(){refresh();}
QByteArray AutotuneWidget::displayPage()const{if(base_.valid()&&!invalidated_)return base_.pages[1];auto page=previewPage?previewPage():QByteArray{};if(MapPageCodec::valid(2,page))return page;MapPageValues m;for(int i=0;i<16;++i){m.rpm[i]=500+i*500;m.load[i]=20+i*20;}m.cells.fill(40);QString error;return MapPageCodec::encode(2,m,error);}
void AutotuneWidget::start(){if(collecting_||!snapshot)return;if(canStart&&!canStart()){state_->setText(tr("Another ECU operation is in progress."));return;}const auto base=snapshot();if(!base.valid()){state_->setText(tr("Connect the ECU and wait for a complete configuration read."));return;}
 const int flags=SensorCalibrationCodec::u16(base.pages[3],70);if(base.pages[3][68]!=1||!(flags&4)||(flags&~7)||base.pages[0][103]!=0){state_->setText(tr("Requires an enabled ECU lambda target and no closed-loop/transient/trim/fuel corrections. Their effects are not silently treated as VE error."));return;}
 base_=base;appliedPage_=base.pages[1];samples_.clear();counts_.fill(0);ratios_.fill(0);lambdas_.fill(0);targets_.fill(0);accepted_=rejected_=0;previousTime_=lastSample_=-1;invalidated_=false;collecting_=true;clock_.start();if(busyChanged)busyChanged(true);refresh();
}
void AutotuneWidget::stop(){if(!collecting_)return;collecting_=false;if(busyChanged)busyChanged(false);refresh();}
void AutotuneWidget::disconnected(){if(base_.valid())invalidated_=true;stop();refresh();}
void AutotuneWidget::sample(const std::array<double,LogChannelCount> &v,double target){
 if(!collecting_)return;const auto current=snapshot?snapshot():ToolSnapshot{};if(!current.valid()||current.identity!=base_.identity||current.generation!=base_.generation||current.pages!=base_.pages){disconnected();return;}const double now=clock_.elapsed()/1000.;if(lastSample_>=0&&now-lastSample_<.1)return;lastSample_=now;if(samples_.size()>=100000){stop();state_->setText(tr("Collection stopped at 100000 samples. Review or start a new collection."));return;}samples_.append({now,target,v});process(samples_.last());refresh();
}
void AutotuneWidget::process(const Sample &s){const auto &v=s.values;bool valid=std::isfinite(s.target)&&s.target>=.65&&s.target<=1.3;for(int i:{0,1,2,3,4})valid&=std::isfinite(v[i]);valid&=v[0]>=settings_.rpmMin&&v[0]<=settings_.rpmMax&&v[1]>=settings_.tpsMin&&v[1]<=settings_.tpsMax&&v[2]>=settings_.mapMin&&v[2]<=settings_.mapMax&&v[3]>=.65&&v[3]<=1.5&&v[4]>=70&&v[4]<=130;
 const bool transient=previousTime_<0||s.time-previousTime_>2||!valid||std::abs(v[0]-previous_[0])>150||std::abs(v[1]-previous_[1])>2||std::abs(v[2]-previous_[2])>5;previous_=v;previousTime_=s.time;if(transient)stableSince_=s.time;if(!valid||s.time-stableSince_<settings_.delay/1000.){++rejected_;return;}
 const auto &page=base_.pages[1];if(v[0]<SensorCalibrationCodec::u16(page,0)||v[0]>SensorCalibrationCodec::u16(page,30)||v[2]<SensorCalibrationCodec::u16(page,32)/100.||v[2]>SensorCalibrationCodec::u16(page,62)/100.){++rejected_;return;}int rpm=0,load=0;double rd=1e20,ld=1e20;for(int i=0;i<16;++i){const double dr=std::abs(v[0]-SensorCalibrationCodec::u16(page,2*i)),dl=std::abs(v[2]-SensorCalibrationCodec::u16(page,32+2*i)/100.);if(dr<rd){rd=dr;rpm=i;}if(dl<ld){ld=dl;load=i;}}
 const int cell=load*16+rpm;ratios_[cell]+=v[3]/s.target;lambdas_[cell]+=v[3];targets_[cell]+=s.target;++counts_[cell];++accepted_;
}
void AutotuneWidget::reprocess(){if(!base_.valid()||invalidated_)return;counts_.fill(0);ratios_.fill(0);lambdas_.fill(0);targets_.fill(0);accepted_=rejected_=0;previousTime_=-1;for(const auto &s:samples_)process(s);}
QByteArray AutotuneWidget::proposal()const{if(!base_.valid()||invalidated_)return {};auto bytes=base_.pages[1];bool changed=false;for(int i=0;i<256;++i)if(counts_[i]>=5){const double factor=qBound(.9,1+(ratios_[i]/counts_[i]-1)*.5,1.1);const int old=SensorCalibrationCodec::u16(bytes,64+2*i);if(old<=0)continue;const int next=qBound(1,int(std::round(old*factor)),30000);SensorCalibrationCodec::put(bytes,64+2*i,next);changed|=next!=old;}return changed?bytes:QByteArray{};}
void AutotuneWidget::refresh(){
 start_->setEnabled(!collecting_);stop_->setEnabled(collecting_);configure_->setEnabled(!collecting_);const auto next=proposal(),base=displayPage();
 state_->setText(invalidated_?tr("ECU/base changed. Proposal cannot be applied; start a new collection."):!base_.valid()?tr("Local preview only — no verified ECU collection. Hatched cells have no calculated proposal. Only native VE table 1."):tr("%1 accepted, %2 rejected. Delay %3 ms; CLT 70–130 °C; five samples/cell; 50% correction strength, limited to ±10%. %4").arg(accepted_).arg(rejected_).arg(settings_.delay).arg(collecting_?tr("Collecting — application disabled."):tr("Select cells and right-click to apply locally.")));
 QVector<QPair<int,int>> selection;for(auto *item:table_->selectedItems())selection.append({item->row(),item->column()});const int currentRow=table_->currentRow(),currentCol=table_->currentColumn();QSignalBlocker block(table_);table_->setUpdatesEnabled(false);
 for(int i=0;i<16;++i)table_->setVerticalHeaderItem(15-i,new QTableWidgetItem(QString::number(SensorCalibrationCodec::u16(base,32+2*i)/100.)));
 for(int i=0;i<16;++i)table_->setHorizontalHeaderItem(i,new QTableWidgetItem(QString::number(SensorCalibrationCodec::u16(base,2*i))));
 for(int load=0;load<16;++load)for(int rpm=0;rpm<16;++rpm){const int cell=load*16+rpm;const bool measured=counts_[cell]>0&&base_.valid()&&!invalidated_;double value=0;bool known=true;const double old=SensorCalibrationCodec::u16(base,64+2*cell)/100.;if(mode_==0)value=next.isEmpty()?old:SensorCalibrationCodec::u16(next,64+2*cell)/100.;else if(mode_==2)value=measured?counts_[cell]:0;else if(!measured)known=false;else if(mode_==1)value=lambdas_[cell]/counts_[cell];else if(mode_==3)value=(lambdas_[cell]-targets_[cell])/counts_[cell];else value=next.isEmpty()?0:SensorCalibrationCodec::u16(next,64+2*cell)/100.-old;
  if(mode_==4&&!measured)known=false;auto *item=table_->item(15-load,rpm);if(!item){item=new QTableWidgetItem;table_->setItem(15-load,rpm,item);}item->setText(known?QString::number(value,'f',mode_==2?0:mode_==1||mode_==3?3:1):QStringLiteral("—"));item->setTextAlignment(Qt::AlignCenter);item->setData(Qt::UserRole,counts_[cell]<5||invalidated_);item->setBackground(known?QColor::fromHsvF(.33-qBound(0.,mode_==0?value/150.:mode_==2?value/50.:.4,.9)*.3,.45,.86):QColor("#9ba99c"));item->setToolTip(tr("RPM %1; MAP %2 kPa; base VE %3; accepted samples %4").arg(SensorCalibrationCodec::u16(base,2*rpm)).arg(SensorCalibrationCodec::u16(base,32+2*load)/100.).arg(old).arg(counts_[cell]));
 }
 table_->clearSelection();for(const auto &cell:selection)table_->item(cell.first,cell.second)->setSelected(true);if(currentRow>=0)table_->setCurrentCell(currentRow,currentCol,QItemSelectionModel::NoUpdate);table_->setUpdatesEnabled(true);surface_->unit=mode_==0?tr("VE"):mode_==1?tr("Lambda"):mode_==2?tr("Samples"):mode_==3?tr("Lambda difference"):tr("VE difference");surface_->update();bool eligible=false;for(auto *item:table_->selectedItems())eligible|=counts_[(15-item->row())*16+item->column()]>=5;apply100_->setEnabled(!collecting_&&!invalidated_&&!next.isEmpty()&&eligible);apply50_->setEnabled(apply100_->isEnabled());
}
void AutotuneWidget::apply(int percent){
 if(collecting_||invalidated_||!snapshot||!applyToEditor)return;const auto current=snapshot();if(!current.valid()||current.identity!=base_.identity||current.generation!=base_.generation||current.pages!=base_.pages){disconnected();return;}const auto next=proposal();if(next.isEmpty())return;auto desired=appliedPage_;bool changed=false;for(auto *item:table_->selectedItems()){const int cell=(15-item->row())*16+item->column();if(counts_[cell]<5)continue;const int at=64+2*cell,old=SensorCalibrationCodec::u16(desired,at),target=SensorCalibrationCodec::u16(next,at);const int value=int(std::round(old+(target-old)*percent/100.));SensorCalibrationCodec::put(desired,at,value);changed|=old!=value;}if(!changed)return;
 if(QMessageBox::question(this,tr("Autotune"),tr("Apply the selected proposal cells at %1% to the local Fuel Table? This does not write the ECU.").arg(percent),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
 if(applyToEditor(base_,appliedPage_,desired)){appliedPage_=desired;state_->setText(tr("Selected cells applied to local Fuel Table. Review before a separate ECU write."));}else state_->setText(tr("Fuel Table or ECU changed. Local edits were preserved."));
}
}
