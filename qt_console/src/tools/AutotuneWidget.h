#pragma once
#include "ToolsData.h"
#include "../logging/LogData.h"
#include <QWidget>
#include <QElapsedTimer>
#include <QVector>
#include <functional>
class QTableWidget;class QLabel;class QAction;class QSplitter;
namespace tunuino {
class AutotuneSurface;
class AutotuneWidget final:public QWidget {
public:
    explicit AutotuneWidget(QWidget *parent=nullptr);
    std::function<ToolSnapshot()> snapshot;
    std::function<QByteArray()> previewPage;
    std::function<bool()> canStart;
    std::function<bool(const ToolSnapshot&,const QByteArray&,const QByteArray&)> applyToEditor;
    std::function<void(const QVector<int>&)> selectionChanged;
    std::function<void(bool)> busyChanged;
    void configure();
    void review();
    void sample(const std::array<double,LogChannelCount>& values,double target);
    void disconnected();
    bool collecting()const{return collecting_;}
    QByteArray proposal()const;
private:
    struct Settings {double rpmMin=500,rpmMax=15000,tpsMin=0,tpsMax=100,mapMin=20,mapMax=400;int delay=500;};
    struct Sample {double time,target;std::array<double,LogChannelCount> values;};
    void start();void stop();void refresh();void apply(int percent);void process(const Sample&);void reprocess();
    QByteArray displayPage()const;
    ToolSnapshot base_;
    Settings settings_;
    QVector<Sample> samples_;
    QByteArray appliedPage_;
    std::array<unsigned,256> counts_{};
    std::array<double,256> ratios_{},lambdas_{},targets_{};
    std::array<double,LogChannelCount> previous_{};
    QElapsedTimer clock_;
    double previousTime_=-1,stableSince_=0,lastSample_=-1;
    bool collecting_=false,invalidated_=false;
    unsigned accepted_=0,rejected_=0;
    int mode_=0;
    QTableWidget *table_;QLabel *state_;QAction *start_,*stop_,*apply100_,*apply50_,*configure_;
    QSplitter *splitter_;QWidget *tablePane_;AutotuneSurface *surface_;
};
}
