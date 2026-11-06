#pragma once
#include "ToolsData.h"
#include "../logging/LogData.h"
#include <QWidget>
#include <QElapsedTimer>
#include <functional>
class QTableWidget;class QLabel;class QPushButton;class QDoubleSpinBox;
namespace tunuino {
class AutotuneWidget final:public QWidget {
public:
    explicit AutotuneWidget(QWidget *parent=nullptr);
    std::function<ToolSnapshot()> snapshot;
    std::function<bool()> canStart;
    std::function<bool(const ToolSnapshot&,const QByteArray&)> applyToEditor;
    std::function<void(bool)> busyChanged;
    void sample(const std::array<double,LogChannelCount>& values,double target);
    void disconnected();
    bool collecting()const{return collecting_;}
    QByteArray proposal()const;
private:
    void start();void stop();void refresh();void apply();
    ToolSnapshot base_;
    std::array<unsigned,256> counts_{};
    std::array<double,256> ratios_{};
    std::array<double,LogChannelCount> previous_{};
    QElapsedTimer clock_;
    double previousTime_=-1,stableSince_=0;
    bool collecting_=false,invalidated_=false;
    unsigned accepted_=0,rejected_=0;
    QTableWidget *table_;QLabel *state_;QPushButton *start_,*apply_;
    QDoubleSpinBox *rpmMin_,*rpmMax_,*mapMin_,*mapMax_,*tpsMin_,*tpsMax_;
};
}
