#include "AutoConnection.h"
#include <QSet>
#include <algorithm>
#include <utility>

namespace tunuino {
AutoConnection::AutoConnection(Hooks hooks, QObject *parent)
    : QObject(parent), hooks_(std::move(hooks)) {
    timer_.setInterval(750);
    connect(&timer_, &QTimer::timeout, this, [this] { scan(clock_.elapsed()); });
}
void AutoConnection::start() {
    clock_.start(); timer_.start();
    QTimer::singleShot(0,this,[this] { scan(clock_.elapsed()); });
}
void AutoConnection::scan(qint64 nowMs) {
    if (hooks_.suspended()) {
        if (hooks_.isOpen()) hooks_.close();
        return;
    }
    if (hooks_.isOpen()) return;
    auto ports = hooks_.ports();
    std::sort(ports.begin(),ports.end(),[](const AutoPort &a,const AutoPort &b) { return a.name < b.name; });
    QSet<QString> present;
    for (const auto &port : ports) present.insert(port.name);
    for (auto it=retryAfter_.begin();it!=retryAfter_.end();) {
        if (!present.contains(it.key())) it=retryAfter_.erase(it); else ++it;
    }
    for (const auto &port : ports) {
        // Matches the board USB descriptor. Never send identity requests to
        // unrelated serial devices or to the ROM DFU bootloader (0483:DF11).
        if (port.vendor!=0xCAFE || port.product!=0x4001 || port.name.isEmpty()) continue;
        if (retryAfter_.value(port.name,0)>nowMs) continue;
        retryAfter_.insert(port.name,nowMs+3000);
        (void)hooks_.begin(port.name);
        return; // One outstanding identity exchange at a time.
    }
}
}
