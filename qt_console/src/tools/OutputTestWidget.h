#pragma once
#include "ScopeWidget.h"
#include "ToolsData.h"
#include <QElapsedTimer>
class QComboBox;class QSpinBox;class QDoubleSpinBox;class QPushButton;class QLabel;
namespace tunuino {
class OutputTestWidget final:public QWidget {
public:
    explicit OutputTestWidget(QWidget *parent=nullptr);
    ToolExchange exchange;
    std::function<bool()> canStart;
    std::function<ToolSnapshot()> snapshot;
    std::function<void(bool)> busyChanged;
    bool busy()const{return phase_!=0;}
protected:void closeEvent(QCloseEvent *event)override;
private:
    void start();void pump();void receive(const QByteArray &);void stop(const QString&);
    QComboBox *output_;QSpinBox *cycles_,*off_;QDoubleSpinBox *on_;QPushButton *test_;QLabel *state_;
    ToolSnapshot base_;int phase_=0,channel_=0,remaining_=0,duration_=0,offMs_=0;bool waiting_=false;
    QElapsedTimer clock_;qint64 deadline_=0,next_=0;
};
}
