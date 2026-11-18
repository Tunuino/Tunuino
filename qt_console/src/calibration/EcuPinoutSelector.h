#pragma once
#include <QMenu>
#include <QToolButton>
#include <functional>
namespace tunuino {
inline QToolButton *ecuPinoutSelector(QWidget *parent, const QString &board,
                                    const std::function<void(const QString &)> &select) {
    auto *button=new QToolButton(parent);button->setObjectName("ecuPinoutSelector");
    button->setText(QObject::tr("ECU pinout: %1").arg(board));
    button->setToolTip(QObject::tr("Choose the pinout to view. This does not change the connected ECU or its configuration."));
    button->setPopupMode(QToolButton::InstantPopup);button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    auto *menu=new QMenu(button);button->setMenu(menu);
    for(const auto &name:{QString("Proteus"),QString("UAefi")}) {
        auto *action=menu->addAction(name=="UAefi"?QObject::tr("UAefi — pinout reference"):QObject::tr("Proteus"));
        action->setObjectName("pinoutSelect"+name);action->setCheckable(true);action->setChecked(name==board);
        QObject::connect(action,&QAction::triggered,button,[select,name]{if(select)select(name);});
    }
    return button;
}
}
