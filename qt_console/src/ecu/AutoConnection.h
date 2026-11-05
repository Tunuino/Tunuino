#pragma once
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QMap>
#include <QString>
#include <QVector>
#include <functional>

namespace tunuino {
struct AutoPort {
    QString name;
    quint16 vendor = 0;
    quint16 product = 0;
};
// Discovery is independent of the transport and never marks an ECU online.
// A successful open must still pass identity and external-power validation.
class AutoConnection final : public QObject {
public:
    struct Hooks {
        std::function<QVector<AutoPort>()> ports;
        std::function<bool()> isOpen;
        std::function<bool()> suspended;
        std::function<bool(const QString &)> begin;
        std::function<void()> close;
    };
    explicit AutoConnection(Hooks hooks, QObject *parent = nullptr);
    void start();
    void scan(qint64 nowMs);
private:
    Hooks hooks_;
    QTimer timer_;
    QElapsedTimer clock_;
    QMap<QString,qint64> retryAfter_;
};
}
