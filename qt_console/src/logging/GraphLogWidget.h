#pragma once
#include "LogData.h"
#include "GraphLogLayout.h"
#include <QWidget>
#include <QElapsedTimer>
#include <functional>
class QAction;
class QLabel;
class QSettings;
class QTableWidget;
class QTabWidget;
namespace tunuino {
class LogPlot;
struct LogViewState {
    qint64 epochMs=0;
    bool paused=false;
    bool inspecting=false;
    double selectionBegin=-1,selectionEnd=-1;
    double span=30;
    double end=30;
    int cursor=-1;
};
class GraphLogWidget final : public QWidget {
public:
    explicit GraphLogWidget(QWidget *parent=nullptr,QSettings *settings=nullptr);
    void setConnected(bool connected);
    void appendRuntime(const std::array<double,LogChannelCount> &values);
    const std::vector<LogSample> &samples()const{return samples_;}
    const LogViewState &view()const{return view_;}
    std::function<void(const LogSample*)> inspectionChanged;
    std::pair<double,double> axisRange(int channel)const;
    quint64 revision()const{return revision_;}
    void selectRange(double begin,double end);
private:
    quint64 revision_=0;
    void autoSave();
    void refresh();
    void clearLog();
    void follow();
    void fit();
    void zoom(double factor);
    void rebuildPlots();
    void saveLayout();
    void configureChart(int tab,int chart);
    void renameTab(int tab);
    bool confirmDiscard();
    std::vector<LogSample> samples_;
    QElapsedTimer clock_;
    LogViewState view_;
    GraphLogLayout layout_;
    QSettings *settings_=nullptr;
    QTabWidget *tabs_=nullptr;
    std::vector<LogPlot*> plots_;
    QLabel *status_=nullptr;
    QAction *record_=nullptr;
    QAction *pause_=nullptr;
    QAction *style_=nullptr;
    QTableWidget *channels_=nullptr;
    bool connected_=false;
    bool recording_=false;
    bool dirty_=false;
};
}
