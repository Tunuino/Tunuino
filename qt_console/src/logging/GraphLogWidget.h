#pragma once
#include "LogData.h"
#include <QWidget>
#include <QElapsedTimer>
class QLabel;
class QPushButton;
class QTableWidget;
class QComboBox;
namespace tunuino {
class LogPlot;
class GraphLogWidget final : public QWidget {
public:
    explicit GraphLogWidget(QWidget *parent = nullptr);
    void setConnected(bool connected);
    void appendRuntime(const std::array<double, LogChannelCount> &values);
private:
    void refresh();
    void clearLog();
    bool confirmDiscard();
    std::vector<LogSample> samples_;
    QElapsedTimer clock_;
    LogPlot *plot_;
    QLabel *status_;
    QPushButton *record_;
    QPushButton *pause_;
    QTableWidget *channels_;
    bool connected_ = false;
    bool recording_ = false;
    bool dirty_ = false;
};
}
