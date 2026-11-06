#pragma once
#include "ParameterCatalog.h"
#include <QWidget>
#include <functional>
class QTableWidget;class QLabel;class QPushButton;
namespace tunuino {
class ParameterWidget final:public QWidget {
public:
    explicit ParameterWidget(ParameterBlock block,QWidget *parent=nullptr);
    const ParameterBlock &block()const{return block_;}
    void load(const QByteArray &header,const QByteArray &aux,const QByteArray &identity,quint32 generation);
    void setAvailable(bool available);
    void saved(){edited_=false;}
    void save();
    std::function<void(int,const QByteArray&,const QByteArray&)> writeRequested;
private:
    void refresh();
    double value(int row)const;
    void setValue(int row,double value);
    void file(bool writing);
    void defaults();
    ParameterBlock block_;QByteArray base_,identity_;quint32 generation_=0;
    QTableWidget *table_;QLabel *status_;QPushButton *save_;
    bool available_=false,edited_=false,loading_=false;
};
}
