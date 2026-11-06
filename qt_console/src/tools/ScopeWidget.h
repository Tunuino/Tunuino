#pragma once
#include <QWidget>
#include <QByteArray>
#include <functional>
#include "ToolsData.h"
class QLabel;class QPushButton;
namespace tunuino {
using ToolExchange=std::function<bool(const QByteArray&,std::function<void(const QByteArray&)>)>;
class ScopeWidget final:public QWidget {
public:
    explicit ScopeWidget(QWidget *parent=nullptr);
    ToolExchange exchange;
    std::function<ToolSnapshot()> snapshot;
    std::function<bool()> ready;
    std::function<void(bool)> busyChanged;
    bool busy()const{return phase_!=0;}
private:
    void start();void pump();void receive(const QByteArray&);void finish(const QString &);
    void save();void open();
    ToolSnapshot base_;
    int phase_=0,total_=0,offset_=0;bool waiting_=false;
    quint32 token_=0,generation_=0,plotGeneration_=0;
    QByteArray header_,samples_;
    QWidget *plot_;QLabel *status_;QPushButton *capture_;
    qint64 deadline_=0;
};
}
