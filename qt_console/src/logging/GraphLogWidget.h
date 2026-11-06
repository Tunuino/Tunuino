#pragma once
#include "LogData.h"
#include "GraphLogLayout.h"
#include <QWidget>
#include <QElapsedTimer>
class QAction;
class QLabel;
class QSettings;
class QTableWidget;
class QTabWidget;
namespace tunuino {
class LogPlot;
struct LogViewState {
    bool paused=false;
    double span=30;
    double end=30;
    int cursor=-1;
};
class GraphLogWidget final : public QWidget {
public:
    explicit GraphLogWidget(QWidget *parent=nullptr,QSettings *settings=nullptr);
    void setConnected(bool connected);
    void appendRuntime(const std::array<double,LogChannelCount> &values);
private:
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
