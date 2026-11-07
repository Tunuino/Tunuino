#pragma once
#include <QJsonObject>
#include "TriggerSetupPlan.h"
#include "../tools/ToolsData.h"
#include <QWizard>
#include <functional>
class QTableWidget;class QSpinBox;class QDoubleSpinBox;class QComboBox;class QCheckBox;class QLabel;class QCloseEvent;
namespace tunuino {
class TriggerWizard final:public QWizard {
public:
    QJsonObject editorState()const;
    bool restoreEditor(const QJsonObject &,bool apply,QString &error);
    void keepProjectEdits(bool on){projectEdits_=on;}
    explicit TriggerWizard(QWidget *parent=nullptr);
    void load(const QByteArray &header,const QByteArray &aux,const QByteArray &identity,quint32 generation);
    void setAvailable(bool available);
    void loadOffline(const QByteArray &header,const QByteArray &aux);
    void save();
    void writeFinished(bool success,const QString &message);
    bool observeScope(const ToolSnapshot &source,const QByteArray &header,const QByteArray &samples);
    bool busy()const{return pending_;}
    bool reviewReady()const;
    std::function<bool(int,const QByteArray&,const QByteArray&)> writeRequested;
    std::function<void()> openScope,openFiringOrder;
    QSize sizeHint()const override{return QSize(720,540);}
protected:
    void accept()override{save();}
    void reject()override;
    bool validateCurrentPage()override;
    void closeEvent(QCloseEvent *event)override;
private:
    bool projectEdits_=false;
    TriggerSetup settings()const;
    void edited();
    void refresh();
    void discard();
    QComboBox *pattern_;
    QSpinBox *teeth_,*missing_,*rpm_,*first_,*last_;
    QDoubleSpinBox *angle_;
    QCheckBox *capture_,*invert_;
    QLabel *profileInfo_,*review_,*state_,*evidence_;
    QTableWidget *reviewTable_;
    QByteArray header_,aux_,identity_;
    quint32 generation_=0;
    bool available_=false,dirty_=false,conflict_=false,pending_=false,loading_=false;
    QString scopeEvidence_;
};
}
