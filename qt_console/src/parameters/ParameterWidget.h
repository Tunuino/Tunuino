#pragma once
#include <QJsonObject>
#include "ParameterCatalog.h"
#include "EngineProfileDraft.h"
#include <optional>
#include <QWidget>
#include <functional>
class QAction;class QTableWidget;class QLabel;class QPushButton;class QComboBox;class QDoubleSpinBox;class QGroupBox;
namespace tunuino {
class ParameterWidget final:public QWidget {
public:
    QJsonObject editorState()const;
    bool restoreEditor(const QJsonObject &,bool apply,QString &error);
    void keepProjectEdits(bool on){projectEdits_=on;}
    explicit ParameterWidget(ParameterBlock block,QWidget *parent=nullptr);
    QSize sizeHint()const override;
    const ParameterBlock &block()const{return block_;}
    void load(const QByteArray &header,const QByteArray &aux,const QByteArray &identity,quint32 generation,const QByteArray &controls={});
    void setAvailable(bool available);
    void saved(){edited_=false;}
    void save();
    std::function<void()> captureCamReference;
    std::function<void(int,const QByteArray&,const QByteArray&)> writeRequested;
private:
    bool projectEdits_=false;
    void refresh();
    void status(const QString &text,bool important=true);
    QAction *writeContext_=nullptr;
    double value(int row)const;
    void setValue(int row,double value);
    void file(bool writing);
    void defaults();
    void setupProfile();
    void syncProfile();
    void discardProfileEdits();
    bool eventFilter(QObject *watched,QEvent *event)override;
    std::optional<EngineProfileDraft> profile_;
    QByteArray profileHeader_,profileAux_;
    bool profileConflict_=false;
    QTableWidget *assignments_=nullptr;
    QGroupBox *advanced_=nullptr;
    QComboBox *operation_=nullptr;
    QVector<QDoubleSpinBox*> profileValues_;
    ParameterBlock block_;QByteArray base_,identity_;quint32 generation_=0;
    QTableWidget *table_=nullptr;QLabel *status_=nullptr;QPushButton *save_=nullptr;
    bool available_=false,edited_=false,loading_=false;
};
}
