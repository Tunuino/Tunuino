#include "GraphLogWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QFileDialog>
#include <QFile>
#include <QSaveFile>
#include <QMessageBox>
#include <QShortcut>
#include <QToolBar>
#include <QAction>
#include <QTabWidget>
#include <QTabBar>
#include <QScrollArea>
#include <QMenu>
#include <QSettings>
#include <QSignalBlocker>
#include <QInputDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QStyle>
#include <algorithm>
#include <functional>

namespace tunuino {
namespace {
QColor channelColor(int channel) {
    static const QColor colors[]={QColor(230,154,50),QColor(116,210,130),QColor(100,180,255),
        QColor(100,220,195),QColor(255,115,105),QColor(205,155,245),QColor(230,215,100)};
    return channel>=0 && channel<LogChannelCount?colors[channel]:QColor(Qt::gray);
}
}
class LogPlot final : public QWidget {
public:
    explicit LogPlot(const std::vector<LogSample> &samples,LogViewState &view,
                     const LogChartConfig &config,bool dots,QWidget *parent)
        : QWidget(parent),samples_(samples),view_(view),config_(config),dots_(dots) {
        setMinimumHeight(155);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
        setToolTip(tr("Pause display to inspect samples. Right-click to configure channels and axes."));
    }
    std::function<void()> inspected;
    std::function<void(const QPoint&)> menuRequested;
    std::function<void()> configureRequested;
protected:
    void contextMenuEvent(QContextMenuEvent *event) override {
        if (menuRequested) menuRequested(event->globalPos());
    }
    void mouseDoubleClickEvent(QMouseEvent *) override { if (configureRequested) configureRequested(); }
    void mousePressEvent(QMouseEvent *event) override { if (event->button()==Qt::LeftButton) inspect(event); }
    void mouseMoveEvent(QMouseEvent *event) override { if (event->buttons()&Qt::LeftButton) inspect(event); }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);p.fillRect(rect(),QColor(31,33,37));
        const QRectF area(82,32,std::max(1,width()-164),std::max(1,height()-65));
        const double begin=view_.end-view_.span;
        p.setPen(QColor(65,68,73));
        for (int i=0;i<=4;++i) {
            const double y=area.bottom()-area.height()*i/4;
            p.drawLine(QPointF(area.left(),y),QPointF(area.right(),y));
            const double x=area.left()+area.width()*i/4;
            p.drawLine(QPointF(x,area.top()),QPointF(x,area.bottom()));
        }
        p.setPen(QColor(220,220,222));
        for (int i=0;i<=4;++i) {
            const double x=area.left()+area.width()*i/4;
            p.drawText(QRectF(x-38,area.bottom()+5,76,20),Qt::AlignCenter,
                QString::number(begin+view_.span*i/4,'f',2)+" s");
        }
        bool anyValid=false;
        const auto first=std::lower_bound(samples_.begin(),samples_.end(),begin-1e-9,
            [](const LogSample &s,double t){return s.seconds<t;});
        for (int side=0;side<2;++side) {
            const auto &axis=config_.axes[side];const int channel=axis.channel;
            if (channel<0) continue;
            const QColor color=channelColor(channel);p.setPen(color);
            QString title=QString::fromLatin1(LogNames[channel])+" ["+LogUnits[channel]+"]";
            if (axis.autoscale) title+=QStringLiteral(" ▲▼");
            if (view_.cursor>=0 && view_.cursor<int(samples_.size())) {
                const double value=samples_[size_t(view_.cursor)].values[channel];
                title+="  "+(std::isfinite(value)?QString::number(value,'g',6):tr("Unavailable"));
            }
            p.drawText(QRectF(8+side*width()/2,5,width()/2-16,22),
                side?Qt::AlignRight:Qt::AlignLeft,title);
            double low=axis.minimum,high=axis.maximum;
            bool hasValid=false;
            if (axis.autoscale) {low=std::numeric_limits<double>::infinity();high=-low;}
            for(auto it=first;it!=samples_.end() && it->seconds<=view_.end;++it)
                if(std::isfinite(it->values[channel])) {
                    hasValid=true;
                    if(axis.autoscale) {low=std::min(low,it->values[channel]);high=std::max(high,it->values[channel]);}
                }
            anyValid|=hasValid;
            if(axis.autoscale && !hasValid) continue;
            if(axis.autoscale) {const double pad=std::max(0.01,(high-low)*.05);low-=pad;high+=pad;}
            for(int i=0;i<=4;++i)
                p.drawText(QRectF(side?area.right()+5:0,area.bottom()-area.height()*i/4-10,77,20),
                    (side?Qt::AlignLeft:Qt::AlignRight)|Qt::AlignVCenter,
                    QString::number(low+(high-low)*i/4,'g',5));
            auto point=[&](const LogSample &s){return QPointF(area.left()+(s.seconds-begin)/view_.span*area.width(),
                area.bottom()-(s.values[channel]-low)/(high-low)*area.height());};
            p.save();p.setClipRect(area);p.setRenderHint(QPainter::Antialiasing);
            p.setPen(QPen(color,1.5));QPainterPath path;bool joined=false;double previous=0;
            for(auto it=first;it!=samples_.end() && it->seconds<=view_.end;++it) {
                if(!std::isfinite(it->values[channel])) {joined=false;continue;}
                const auto pos=point(*it);
                if(dots_) p.drawEllipse(pos,2,2);
                else if(joined && it->seconds-previous<=2.0) path.lineTo(pos);
                else {path.moveTo(pos);p.drawEllipse(pos,1.2,1.2);}
                joined=true;previous=it->seconds;
            }
            if(!dots_)p.drawPath(path);
            p.restore();
        }
        if(!anyValid) {p.setPen(QColor(180,180,185));p.drawText(area,Qt::AlignCenter,tr("No valid samples for selected channels"));}
        if(view_.cursor>=0 && view_.cursor<int(samples_.size())) {
            const double x=area.left()+(samples_[size_t(view_.cursor)].seconds-begin)/view_.span*area.width();
            if(x>=area.left()-.1 && x<=area.right()+.1) {
                p.setPen(QPen(Qt::white,1,Qt::DashLine));
                p.drawLine(QPointF(x,area.top()),QPointF(x,area.bottom()));
            }
        }
    }
private:
    void inspect(QMouseEvent *event) {
        if(!view_.paused || samples_.empty())return;
        const double time=view_.end-view_.span+std::clamp((event->position().x()-82)/std::max(1,width()-164),0.0,1.0)*view_.span;
        auto it=std::lower_bound(samples_.begin(),samples_.end(),time,[](const LogSample &s,double t){return s.seconds<t;});
        if(it==samples_.end())--it;
        else if(it!=samples_.begin() && time-(it-1)->seconds<it->seconds-time)--it;
        view_.cursor=int(it-samples_.begin());if(inspected)inspected();
    }
    const std::vector<LogSample> &samples_;
    LogViewState &view_;
    LogChartConfig config_;
    bool dots_;
};

GraphLogWidget::GraphLogWidget(QWidget *parent,QSettings *settings) : QWidget(parent),
    layout_(defaultGraphLogLayout()),settings_(settings?settings:new QSettings("Tunuino","TunuinoConsole",this)) {
    decodeGraphLogLayout(settings_->value("logging/graphLayoutV1").toByteArray(),layout_);
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(4,4,4,4);
    auto *bar=new QToolBar(this);bar->setIconSize(QSize(18,18));
    const auto action=[&](const char *id,const QString &name,QStyle::StandardPixmap icon,auto callback) {
        auto *a=bar->addAction(style()->standardIcon(icon),name);a->setObjectName(QString::fromLatin1(id));
        connect(a,&QAction::triggered,this,callback);return a;
    };
    record_=action("logRecord",tr("Start logging"),QStyle::SP_MediaPlay,[this] {
        if(recording_) {recording_=false;refresh();return;}
        if(!connected_ || !confirmDiscard())return;
        clearLog();clock_.start();recording_=true;view_.paused=false;refresh();
    });
    record_->setEnabled(false);
    pause_=action("logPause",tr("Pause display"),QStyle::SP_MediaPause,[this] {
        view_.paused=!view_.paused;follow();refresh();
    });
    bar->addSeparator();
    action("logOpen",tr("Open log..."),QStyle::SP_DialogOpenButton,[this] {
        const QString path=QFileDialog::getOpenFileName(this,tr("Open log"),{},tr("Tunuino logs (*.csv)"));
        if(path.isEmpty())return;
        QFile file(path);
        if(!file.open(QIODevice::ReadOnly) || file.size()>16*1024*1024) {
            QMessageBox::warning(this,tr("Open log"),tr("Cannot read log or file exceeds 16 MB."));return;
        }
        std::vector<LogSample> parsed;std::string error;
        if(!decodeLogCsv(file.readAll().toStdString(),parsed,error)) {
            QMessageBox::warning(this,tr("Invalid log"),QString::fromStdString(error));return;
        }
        if(!confirmDiscard())return;
        recording_=false;samples_=std::move(parsed);dirty_=false;
        view_.paused=true;view_.cursor=0;fit();refresh();
    });
    action("logSave",tr("Save CSV..."),QStyle::SP_DialogSaveButton,[this] {
        if(samples_.empty())return;
        const QString path=QFileDialog::getSaveFileName(this,tr("Save log"),"tunuino-log.csv",tr("Tunuino logs (*.csv)"));
        if(path.isEmpty())return;
        const auto text=encodeLogCsv(samples_);QSaveFile file(path);
        if(!file.open(QIODevice::WriteOnly) || file.write(text.data(),qint64(text.size()))!=qint64(text.size()) || !file.commit()) {
            QMessageBox::warning(this,tr("Save log"),tr("Could not save log."));return;
        }
        dirty_=false;
    });
    action("logClear",tr("Clear log"),QStyle::SP_DialogResetButton,[this] {
        if(confirmDiscard()) {recording_=false;clearLog();refresh();}
    });
    bar->addSeparator();
    action("logZoomIn",tr("Zoom in (Q)"),QStyle::SP_ArrowUp,[this]{zoom(.5);});
    action("logZoomOut",tr("Zoom out (A)"),QStyle::SP_ArrowDown,[this]{zoom(2);});
    action("logFull",tr("Full log"),QStyle::SP_TitleBarMaxButton,[this]{fit();});
    action("logEarlier",tr("Earlier"),QStyle::SP_ArrowBack,[this] {
        if(view_.paused) {view_.end=std::max(view_.span,view_.end-view_.span/2);refresh();}
    });
    action("logLater",tr("Later"),QStyle::SP_ArrowForward,[this] {
        if(view_.paused && !samples_.empty()) {
            view_.end=std::min(std::max(view_.span,samples_.back().seconds),view_.end+view_.span/2);refresh();
        }
    });
    bar->addSeparator();
    style_=action("logStyle",tr("Lines / dots (Shift+S)"),QStyle::SP_FileDialogDetailedView,[this] {
        auto &tab=layout_[size_t(tabs_->currentIndex())];tab.dots=!tab.dots;saveLayout();rebuildPlots();refresh();
    });
    action("logConfigure",tr("Channels config"),QStyle::SP_FileDialogContentsView,[this]{configureChart(tabs_->currentIndex(),0);});
    layout->addWidget(bar);
    tabs_=new QTabWidget(this);tabs_->setObjectName("graphLogTabs");tabs_->setUsesScrollButtons(true);
    tabs_->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tabs_->tabBar(),&QTabBar::tabBarDoubleClicked,this,[this](int i){renameTab(i);});
    connect(tabs_->tabBar(),&QWidget::customContextMenuRequested,this,[this](const QPoint &pos) {
        const int index=tabs_->tabBar()->tabAt(pos);if(index<0)return;
        QMenu menu(this);auto *rename=menu.addAction(tr("Rename log tab..."));
        auto *add=menu.addAction(tr("Add graph"));add->setEnabled(layout_[size_t(index)].charts.size()<MaxGraphsPerTab);
        auto *chosen=menu.exec(tabs_->tabBar()->mapToGlobal(pos));
        if(chosen==rename)renameTab(index);
        else if(chosen==add) {LogChartConfig chart;chart.axes[0].channel=0;layout_[size_t(index)].charts.push_back(chart);saveLayout();rebuildPlots();}
    });
    connect(tabs_,&QTabWidget::currentChanged,this,[this](int index){if(index>=0 && status_)refresh();});
    layout->addWidget(tabs_,1);
    channels_=new QTableWidget(LogChannelCount,3,this);channels_->setObjectName("logCursorValues");
    channels_->setHorizontalHeaderLabels({tr("Channel"),tr("Cursor value"),tr("Unit")});
    channels_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    channels_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);channels_->setMaximumHeight(180);
    channels_->verticalHeader()->setDefaultSectionSize(22);
    for(int i=0;i<LogChannelCount;++i) {
        channels_->setItem(i,0,new QTableWidgetItem(QString::fromLatin1(LogNames[i])));
        channels_->item(i,0)->setForeground(channelColor(i));
        channels_->setItem(i,1,new QTableWidgetItem(tr("Unavailable")));
        channels_->setItem(i,2,new QTableWidgetItem(QString::fromLatin1(LogUnits[i])));
    }
    layout->addWidget(channels_);
    status_=new QLabel(this);status_->setWordWrap(true);layout->addWidget(status_);
    const auto shortcut=[&](const QString &key,auto callback) {
        auto *s=new QShortcut(QKeySequence(key),this);s->setContext(Qt::WidgetWithChildrenShortcut);
        connect(s,&QShortcut::activated,this,callback);
    };
    shortcut("Q",[this]{zoom(.5);});shortcut("A",[this]{zoom(2);});
    shortcut("Shift+S",[this]{style_->trigger();});
    rebuildPlots();refresh();
}

void GraphLogWidget::rebuildPlots() {
    const int current=std::max(0,tabs_->currentIndex());
    QSignalBlocker blocker(tabs_);plots_.clear();
    while(tabs_->count()) {auto *page=tabs_->widget(0);tabs_->removeTab(0);page->deleteLater();}
    for(int tabIndex=0;tabIndex<GraphLogTabCount;++tabIndex) {
        auto *scroll=new QScrollArea(tabs_);scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);
        auto *page=new QWidget(scroll);auto *layout=new QVBoxLayout(page);layout->setContentsMargins(0,0,0,0);layout->setSpacing(3);
        const auto &tab=layout_[size_t(tabIndex)];
        for(int chartIndex=0;chartIndex<int(tab.charts.size());++chartIndex) {
            auto *plot=new LogPlot(samples_,view_,tab.charts[size_t(chartIndex)],tab.dots,page);
            plot->setObjectName(QStringLiteral("logPlot_%1_%2").arg(tabIndex).arg(chartIndex));
            plot->inspected=[this]{refresh();};
            plot->configureRequested=[this,tabIndex,chartIndex]{configureChart(tabIndex,chartIndex);};
            plot->menuRequested=[this,tabIndex,chartIndex](const QPoint &pos) {
                QMenu menu(this);auto *setup=menu.addAction(tr("Setup channels..."));
                auto *add=menu.addAction(tr("Add graph"));add->setEnabled(layout_[size_t(tabIndex)].charts.size()<MaxGraphsPerTab);
                auto *remove=menu.addAction(tr("Remove graph"));remove->setEnabled(layout_[size_t(tabIndex)].charts.size()>1);
                menu.addSeparator();auto *lines=menu.addAction(tr("Change line style"));
                auto *full=menu.addAction(tr("Zoom full out"));
                const auto *chosen=menu.exec(pos);
                if(chosen==setup)configureChart(tabIndex,chartIndex);
                else if(chosen==full)fit();
                else if(chosen==lines) {layout_[size_t(tabIndex)].dots=!layout_[size_t(tabIndex)].dots;saveLayout();rebuildPlots();refresh();}
                else if(chosen==add) {LogChartConfig chart;chart.axes[0].channel=0;layout_[size_t(tabIndex)].charts.push_back(chart);saveLayout();rebuildPlots();}
                else if(chosen==remove) {auto &charts=layout_[size_t(tabIndex)].charts;charts.erase(charts.begin()+chartIndex);saveLayout();rebuildPlots();}
            };
            layout->addWidget(plot,1);plots_.push_back(plot);
        }
        scroll->setWidget(page);tabs_->addTab(scroll,tab.name);
    }
    tabs_->setCurrentIndex(current);
}
void GraphLogWidget::saveLayout() {
    settings_->setValue("logging/graphLayoutV1",encodeGraphLogLayout(layout_));settings_->sync();
}
void GraphLogWidget::renameTab(int tab) {
    if(tab<0 || tab>=GraphLogTabCount)return;
    bool ok=false;
    const QString name=QInputDialog::getText(this,tr("Rename log tab"),tr("Name (up to 48 characters)"),
        QLineEdit::Normal,layout_[size_t(tab)].name,&ok).trimmed();
    if(!ok || name.isEmpty())return;
    if(name.size()>48) {QMessageBox::warning(this,tr("Tab name"),tr("Use up to 48 characters."));return;}
    layout_[size_t(tab)].name=name;tabs_->setTabText(tab,name);saveLayout();
}
void GraphLogWidget::configureChart(int tab,int chartIndex) {
    QDialog dialog(this);dialog.setWindowTitle(tr("Channels config"));dialog.setObjectName("logChannelConfig");
    auto *layout=new QVBoxLayout(&dialog);
    std::array<QComboBox*,2> channels;std::array<QCheckBox*,2> autos;
    std::array<QDoubleSpinBox*,2> minima,maxima;
    const auto old=layout_[size_t(tab)].charts[size_t(chartIndex)];
    for(int side=0;side<2;++side) {
        auto *group=new QGroupBox(side?tr("Right axis"):tr("Left axis"),&dialog);auto *form=new QFormLayout(group);
        auto *channel=new QComboBox(group);channels[side]=channel;channel->setObjectName(QStringLiteral("axisChannel%1").arg(side));
        channel->addItem(tr("Hidden"),-1);
        for(int i=0;i<LogChannelCount;++i)channel->addItem(QString::fromLatin1(LogNames[i])+" ["+LogUnits[i]+"]",i);
        channel->setCurrentIndex(old.axes[side].channel+1);form->addRow(tr("Log channel"),channel);
        auto *autoscale=new QCheckBox(tr("Autoscale"),group);autos[side]=autoscale;
        autoscale->setObjectName(QStringLiteral("axisAutoscale%1").arg(side));autoscale->setChecked(old.axes[side].autoscale);
        form->addRow(autoscale);
        auto *minimum=new QDoubleSpinBox(group);minima[side]=minimum;
        auto *maximum=new QDoubleSpinBox(group);maxima[side]=maximum;
        minimum->setObjectName(QStringLiteral("axisMinimum%1").arg(side));maximum->setObjectName(QStringLiteral("axisMaximum%1").arg(side));
        for(auto *spin:{minimum,maximum}) {spin->setRange(-1e9,1e9);spin->setDecimals(4);spin->setEnabled(!autoscale->isChecked());}
        minimum->setValue(old.axes[side].minimum);maximum->setValue(old.axes[side].maximum);
        form->addRow(tr("Min value"),minimum);form->addRow(tr("Max value"),maximum);
        connect(autoscale,&QCheckBox::toggled,&dialog,[=](bool checked){minimum->setEnabled(!checked);maximum->setEnabled(!checked);});
        layout->addWidget(group);
    }
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,[&] {
        LogChartConfig next;
        for(int side=0;side<2;++side) {
            next.axes[side]={channels[side]->currentData().toInt(),autos[side]->isChecked(),minima[side]->value(),maxima[side]->value()};
            if(next.axes[side].minimum>=next.axes[side].maximum) {
                QMessageBox::warning(&dialog,tr("Axis range"),tr("Min value must be smaller than Max value."));return;
            }
        }
        if(next.axes[0].channel>=0 && next.axes[0].channel==next.axes[1].channel) {
            QMessageBox::warning(&dialog,tr("Channels"),tr("Choose different channels for the two axes."));return;
        }
        layout_[size_t(tab)].charts[size_t(chartIndex)]=next;dialog.accept();
    });
    if(dialog.exec()==QDialog::Accepted) {saveLayout();rebuildPlots();refresh();}
}
void GraphLogWidget::setConnected(bool connected) {
    connected_=connected;if(!connected){recording_=false;view_.paused=true;}refresh();
}
void GraphLogWidget::appendRuntime(const std::array<double,LogChannelCount> &values) {
    if(!connected_ || !recording_)return;
    if(samples_.size()>=MaxLogSamples) {recording_=false;view_.paused=true;refresh();return;}
    LogSample sample;sample.seconds=clock_.nsecsElapsed()/1e9;sample.values=values;
    if(!samples_.empty() && sample.seconds<=samples_.back().seconds)return;
    samples_.push_back(sample);dirty_=true;follow();refresh();
}
void GraphLogWidget::follow() {
    if(!view_.paused && !samples_.empty()) {view_.end=std::max(view_.span,samples_.back().seconds);view_.cursor=int(samples_.size())-1;}
}
void GraphLogWidget::fit() {
    view_.span=samples_.empty()?30:std::max(.1,samples_.back().seconds-samples_.front().seconds);
    view_.end=samples_.empty()?view_.span:samples_.back().seconds;refresh();
}
void GraphLogWidget::zoom(double factor) {view_.span=std::clamp(view_.span*factor,.1,1e7);refresh();}
void GraphLogWidget::clearLog() {samples_.clear();dirty_=false;view_.cursor=-1;view_.span=30;view_.end=30;}
bool GraphLogWidget::confirmDiscard() {
    return !dirty_ || QMessageBox::question(this,tr("Unsaved log"),tr("Discard the unsaved log?"),
        QMessageBox::Discard|QMessageBox::Cancel,QMessageBox::Cancel)==QMessageBox::Discard;
}
void GraphLogWidget::refresh() {
    record_->setEnabled(connected_);record_->setText(recording_?tr("Stop logging"):tr("Start logging"));
    pause_->setText(view_.paused?tr("Resume display"):tr("Pause display"));
    const int index=view_.cursor;
    for(int i=0;i<LogChannelCount;++i) {
        const double value=index>=0 && index<int(samples_.size())?samples_[size_t(index)].values[i]:std::numeric_limits<double>::quiet_NaN();
        channels_->item(i,1)->setText(std::isfinite(value)?QString::number(value,'g',8):tr("Unavailable"));
    }
    const QString cursor=index>=0 && index<int(samples_.size())?QString::number(samples_[size_t(index)].seconds,'f',3):QStringLiteral("—");
    status_->setText(tr("%1 | %2 samples | C=%3 s | %4 | ECU polling: 500 ms nominal; actual receive timestamps. TPS, IAT and Battery unavailable in current protocol.")
        .arg(recording_?tr("Recording"):tr("Stopped")).arg(qulonglong(samples_.size())).arg(cursor)
        .arg(view_.paused?tr("Display paused — collection continues while recording"):tr("Live display")));
    if(samples_.size()>=MaxLogSamples)status_->setText(tr("Log full: recording stopped at 100000 samples. Save CSV before starting a new log."));
    for(auto *plot:plots_)if(plot->isVisible())plot->update();
}
}
