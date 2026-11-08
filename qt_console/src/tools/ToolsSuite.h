#pragma once
#include "ToolsData.h"
#include <QObject>
#include <QPointer>
#include <functional>
class QMainWindow;class QTreeWidget;class QMenu;class QAction;class QTableWidget;class QLabel;class QDialog;
namespace tunuino {
class ToolsSuite final:public QObject {
public:
    ToolsSuite(QMainWindow *window,QTreeWidget *navigation);
    void populate(QMenu *menu,QAction *graphLog,QAction *tuneDisplay);
    std::function<ToolSnapshot()> snapshot;
    std::function<void()> openAutotune,openScope,openOutputTest;
    std::function<void(int)> openServiceData;
    void refreshAssignments();
    std::function<void()> checkForUpdates;
    QMenu *monitorMenu=nullptr;
private:
    void assignments(bool outputs);
    void userNames(bool outputs);
    void customizeKeys();
    void strategies();
    void generalOptions();
    void restoreKeys();
    QString alias(const QString &id,const ToolSnapshot &base) const;
    QString darkStyle_;
    QMainWindow *window_;QTreeWidget *navigation_;
    QPointer<QDialog> inputs_,outputs_;
    QTableWidget *inputTable_=nullptr,*outputTable_=nullptr;
    QLabel *inputState_=nullptr,*outputState_=nullptr;
};
}
