#pragma once
#include "ScopeWidget.h"
#include "ServiceDataCodec.h"
#include <QElapsedTimer>
class QLabel;class QLineEdit;class QPushButton;class QTabWidget;class QTableWidget;
namespace tunuino {
class ServiceDataWidget final:public QWidget {
public:
    explicit ServiceDataWidget(QWidget *parent=nullptr);
    ToolExchange exchange;
    std::function<ToolSnapshot()> snapshot;
    std::function<bool()> canStart;
    std::function<void(bool)> busyChanged;
    void showPage(int index);
    bool busy()const{return phase_!=0;}
protected:void closeEvent(QCloseEvent *event)override;
private:
    void read();void startWrite(int op);void pump();void receive(const QByteArray &);
    void finish(const QString &,bool unknown=false);void display();void exportProject();
    ToolSnapshot base_;QByteArray header_,data_,request_;
    int phase_=0,offset_=0,writeOp_=0;bool waiting_=false,unknown_=false,edited_=false,vinEdited_=false,commentEdited_=false;
    QString expected_;QElapsedTimer clock_;qint64 deadline_=0;
    QTabWidget *tabs_;QLineEdit *vin_,*comment_;QTableWidget *book_;QLabel *state_;
    QPushButton *read_,*saveVin_,*saveComment_,*checkpoint_,*reset_,*export_;
};
}
