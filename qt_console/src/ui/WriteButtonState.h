#pragma once
#include <QAbstractButton>

namespace tunuino {
inline void showPendingWrite(QAbstractButton *button, bool pending)
{
    if (button->property("ecuWritePending").toBool() == pending)
        return;
    button->setProperty("ecuWritePending", pending);
    button->setStyleSheet(pending
        ? QStringLiteral("QAbstractButton { background-color: #a32626; color: white; }")
        : QString{});
}
}
