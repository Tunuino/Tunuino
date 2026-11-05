#include "GraphLogWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QFileDialog>
#include <QFile>
#include <QSaveFile>
#include <QMessageBox>
#include <QShortcut>
#include <algorithm>
#include <functional>

namespace tunuino {
class LogPlot final : public QWidget {
public:
    explicit LogPlot(const std::vector<LogSample> &samples, QWidget *parent)
        : QWidget(parent), samples_(samples) { setMinimumHeight(200); }
    bool paused = false;
    bool dots = false;
    int channel = 0;
    double span = 30;
    double end = 30;
    int cursor = -1;
    std::function<void()> inspected;
    void follow() {
        if (!paused && !samples_.empty()) {
            end = std::max(span, samples_.back().seconds);
            cursor = int(samples_.size()) - 1;
        }
        update();
    }
    void zoom(double factor) { span = std::clamp(span * factor, 0.1, 1e7); update(); }
    void fit() {
        span = samples_.empty() ? 30 : std::max(0.1, samples_.back().seconds - samples_.front().seconds);
        end = samples_.empty() ? span : samples_.back().seconds;
        update();
    }
protected:
    void mousePressEvent(QMouseEvent *e) override { inspect(e); }
    void mouseMoveEvent(QMouseEvent *e) override { if (e->buttons() & Qt::LeftButton) inspect(e); }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.fillRect(rect(), QColor(31, 33, 37));
        const QRectF area(68, 30, std::max(1, width()-90), std::max(1, height()-68));
        p.setPen(QColor(75, 78, 83));
        for (int i = 0; i <= 4; ++i) {
            const double y = area.bottom()-area.height()*i/4;
            p.drawLine(QPointF(area.left(),y), QPointF(area.right(),y));
        }
        const double begin = end-span;
        double low = std::numeric_limits<double>::infinity(), high = -low;
        for (const auto &s : samples_) if (s.seconds >= begin && s.seconds <= end && std::isfinite(s.values[channel])) {
            low = std::min(low, s.values[channel]); high = std::max(high, s.values[channel]);
        }
        p.setPen(QColor(225, 226, 228));
        p.drawText(8, 20, QString::fromLatin1(LogNames[channel]) + " [" + LogUnits[channel] + "]");
        for (int i = 0; i <= 4; ++i) {
            const double x = area.left()+area.width()*i/4;
            p.drawText(QRectF(x-35, area.bottom()+8,70,20), Qt::AlignCenter, QString::number(begin+span*i/4,'f',2)+" s");
        }
        if (!std::isfinite(low)) {
            p.drawText(area, Qt::AlignCenter, tr("No valid samples for this channel"));
            return;
        }
        const double padding = std::max(0.01, (high-low)*0.05);
        low -= padding; high += padding;
        for (int i = 0; i <= 4; ++i)
            p.drawText(QRectF(0, area.bottom()-area.height()*i/4-10,62,20), Qt::AlignRight|Qt::AlignVCenter,
                       QString::number(low+(high-low)*i/4,'g',5));
        auto point = [&](const LogSample &s) {
            return QPointF(area.left()+(s.seconds-begin)/span*area.width(),
                           area.bottom()-(s.values[channel]-low)/(high-low)*area.height());
        };
        p.save(); p.setClipRect(area); p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor(230,154,50),1.5));
        QPainterPath path; bool joined = false; double previous = 0;
        for (const auto &s : samples_) {
            if (s.seconds < begin || s.seconds > end || !std::isfinite(s.values[channel])) { joined = false; continue; }
            const auto pos = point(s);
            if (dots) p.drawEllipse(pos,2,2);
            else if (joined && s.seconds-previous <= 2.0) path.lineTo(pos);
            else path.moveTo(pos);
            joined = true; previous = s.seconds;
        }
        if (!dots) p.drawPath(path);
        if (cursor >= 0 && cursor < int(samples_.size())) {
            const auto &s = samples_[size_t(cursor)];
            const double x = area.left()+(s.seconds-begin)/span*area.width();
            p.setPen(QPen(Qt::white,1,Qt::DashLine));
            p.drawLine(QPointF(x,area.top()),QPointF(x,area.bottom()));
        }
        p.restore();
    }
private:
    void inspect(QMouseEvent *e) {
        if (!paused || samples_.empty()) return;
        const double time = end-span + std::clamp((e->position().x()-68)/std::max(1,width()-90),0.0,1.0)*span;
        auto it = std::lower_bound(samples_.begin(),samples_.end(),time,[](const LogSample &s,double t){ return s.seconds < t; });
        if (it == samples_.end()) --it;
        else if (it != samples_.begin() && time-(it-1)->seconds < it->seconds-time) --it;
        cursor = int(it-samples_.begin());
        if (inspected) inspected();
        update();
    }
    const std::vector<LogSample> &samples_;
};

GraphLogWidget::GraphLogWidget(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    auto *bar = new QHBoxLayout;
    auto button = [&](const QString &name, auto callback) {
        auto *b = new QPushButton(name,this); bar->addWidget(b);
        connect(b,&QPushButton::clicked,this,callback); return b;
    };
    record_ = button(tr("Start logging"),[this] {
        if (recording_) { recording_ = false; refresh(); return; }
        if (!connected_ || !confirmDiscard()) return;
        clearLog(); clock_.start(); recording_ = true; plot_->paused = false; refresh();
    });
    record_->setEnabled(false);
    pause_ = button(tr("Pause display"),[this] {
        plot_->paused = !plot_->paused; plot_->follow(); refresh();
    });
    button(tr("Open log..."),[this] {
        const QString path = QFileDialog::getOpenFileName(this,tr("Open log"),{},tr("Tunuino logs (*.csv)"));
        if (path.isEmpty()) return;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly) || file.size() > 16*1024*1024) {
            QMessageBox::warning(this,tr("Open log"),tr("Cannot read log or file exceeds 16 MB.")); return;
        }
        std::vector<LogSample> parsed; std::string error;
        if (!decodeLogCsv(file.readAll().toStdString(),parsed,error)) {
            QMessageBox::warning(this,tr("Invalid log"),QString::fromStdString(error)); return;
        }
        if (!confirmDiscard()) return;
        recording_ = false; samples_ = std::move(parsed); dirty_ = false;
        plot_->paused = true; plot_->cursor = 0; plot_->fit(); refresh();
    });
    button(tr("Save CSV..."),[this] {
        if (samples_.empty()) return;
        const QString path = QFileDialog::getSaveFileName(this,tr("Save log"),"tunuino-log.csv",tr("Tunuino logs (*.csv)"));
        if (path.isEmpty()) return;
        const auto text = encodeLogCsv(samples_); QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(text.data(),qint64(text.size())) != qint64(text.size()) || !file.commit()) {
            QMessageBox::warning(this,tr("Save log"),tr("Could not save log.")); return;
        }
        dirty_ = false;
    });
    button(tr("Clear"),[this] { if (confirmDiscard()) { recording_ = false; clearLog(); refresh(); } });
    layout->addLayout(bar);
    auto *viewBar = new QHBoxLayout;
    auto *channel = new QComboBox(this);
    for (int i = 0; i < LogChannelCount; ++i) channel->addItem(QString::fromLatin1(LogNames[i])+" ["+LogUnits[i]+"]");
    viewBar->addWidget(channel);
    plot_ = new LogPlot(samples_,this);
    connect(channel,&QComboBox::currentIndexChanged,this,[this](int index){ plot_->channel=index; plot_->update(); });
    const auto control = [&](const QString &name, auto callback) {
        auto *b = new QPushButton(name,this); viewBar->addWidget(b); connect(b,&QPushButton::clicked,this,callback);
    };
    control(tr("Zoom in"),[this]{ plot_->zoom(0.5); });
    control(tr("Zoom out"),[this]{ plot_->zoom(2); });
    control(tr("Full log"),[this]{ plot_->fit(); });
    control(tr("Earlier"),[this]{ if (plot_->paused) { plot_->end=std::max(plot_->span,plot_->end-plot_->span/2); plot_->update(); } });
    control(tr("Later"),[this]{ if (plot_->paused && !samples_.empty()) { plot_->end=std::min(std::max(plot_->span,samples_.back().seconds),plot_->end+plot_->span/2); plot_->update(); } });
    auto *style = new QComboBox(this); style->addItems({tr("Lines"),tr("Dots")}); viewBar->addWidget(style);
    connect(style,&QComboBox::currentIndexChanged,this,[this](int index){plot_->dots=index==1;plot_->update();});
    layout->addLayout(viewBar); layout->addWidget(plot_,1);
    channels_ = new QTableWidget(LogChannelCount,3,this);
    channels_->setHorizontalHeaderLabels({tr("Channel"),tr("Cursor value"),tr("Unit")});
    channels_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    channels_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    channels_->setMaximumHeight(230);
    for (int i=0;i<LogChannelCount;++i) {
        channels_->setItem(i,0,new QTableWidgetItem(QString::fromLatin1(LogNames[i])));
        channels_->setItem(i,1,new QTableWidgetItem(tr("Unavailable")));
        channels_->setItem(i,2,new QTableWidgetItem(QString::fromLatin1(LogUnits[i])));
    }
    layout->addWidget(channels_);
    status_ = new QLabel(this); status_->setWordWrap(true); layout->addWidget(status_);
    plot_->inspected = [this]{ refresh(); };
    refresh();
}
void GraphLogWidget::setConnected(bool connected) {
    connected_ = connected;
    if (!connected) { recording_ = false; plot_->paused = true; }
    refresh();
}
void GraphLogWidget::appendRuntime(const std::array<double,LogChannelCount> &values) {
    if (!connected_ || !recording_) return;
    if (samples_.size() >= MaxLogSamples) { recording_ = false; plot_->paused = true; refresh(); return; }
    LogSample sample; sample.seconds=clock_.nsecsElapsed()/1e9; sample.values=values;
    if (!samples_.empty() && sample.seconds <= samples_.back().seconds) return;
    samples_.push_back(sample); dirty_=true; plot_->follow(); refresh();
}
void GraphLogWidget::clearLog() {
    samples_.clear(); dirty_=false; plot_->cursor=-1; plot_->span=30; plot_->end=30; plot_->update();
}
bool GraphLogWidget::confirmDiscard() {
    return !dirty_ || QMessageBox::question(this,tr("Unsaved log"),tr("Discard the unsaved log?"),QMessageBox::Discard|QMessageBox::Cancel,QMessageBox::Cancel)==QMessageBox::Discard;
}
void GraphLogWidget::refresh() {
    record_->setEnabled(connected_);
    record_->setText(recording_?tr("Stop logging"):tr("Start logging"));
    pause_->setText(plot_->paused?tr("Resume display"):tr("Pause display"));
    const int index=plot_->cursor;
    for (int i=0;i<LogChannelCount;++i) {
        const double value=index>=0 && index<int(samples_.size()) ? samples_[size_t(index)].values[i] : std::numeric_limits<double>::quiet_NaN();
        channels_->item(i,1)->setText(std::isfinite(value)?QString::number(value,'g',8):tr("Unavailable"));
    }
    const QString cursor=index>=0 && index<int(samples_.size()) ? QString::number(samples_[size_t(index)].seconds,'f',3) : QStringLiteral("—");
    status_->setText(tr("%1 | %2 samples | C=%3 s | %4 | ECU polling: 500 ms nominal; actual receive timestamps. TPS, IAT and Battery unavailable in current protocol.")
        .arg(recording_?tr("Recording"):tr("Stopped")).arg(qulonglong(samples_.size())).arg(cursor)
        .arg(plot_->paused?tr("Display paused — collection continues while recording"):tr("Live display")));
    if (samples_.size()>=MaxLogSamples) status_->setText(tr("Log full: recording stopped at 100000 samples. Save CSV before starting a new log."));
    plot_->update();
}
}
