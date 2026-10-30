#pragma once
#include <QObject>
#include <QByteArray>
#include <QQueue>
#include <QSerialPort>
#include <QTimer>

namespace tunuino {
// Event-driven serial request/response session. All methods run on the owning Qt thread.
class SerialSession final : public QObject {
    Q_OBJECT
public:
    explicit SerialSession(QObject *parent = nullptr);
    bool open(const QString &portName, qint32 baudRate = 115200);
    void close();
    bool isOpen() const;
    bool exchange(const QByteArray &body, int timeoutMs = 1000);
signals:
    void responseReceived(const QByteArray &body);
    void exchangeFailed(const QString &reason);
    void connectionChanged(bool connected);
private slots:
    void onReadyRead();
    void onBytesWritten(qint64 count);
    void onError(QSerialPort::SerialPortError error);
    void onTimeout();
private:
    void pumpWrite();
    void fail(const QString &reason);
    QSerialPort port_;
    QTimer timeout_;
    QByteArray incoming_;
    QByteArray outgoing_;
    qsizetype sent_ = 0;
    bool busy_ = false;
};
}
