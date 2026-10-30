#include "SerialSession.h"
#include "../protocol/FrameCodec.h"

namespace tunuino {
SerialSession::SerialSession(QObject *parent) : QObject(parent) {
    timeout_.setSingleShot(true);
    connect(&timeout_, &QTimer::timeout, this, &SerialSession::onTimeout);
    connect(&port_, &QSerialPort::readyRead, this, &SerialSession::onReadyRead);
    connect(&port_, &QSerialPort::bytesWritten, this, &SerialSession::onBytesWritten);
    connect(&port_, &QSerialPort::errorOccurred, this, &SerialSession::onError);
}
bool SerialSession::open(const QString &name, qint32 baud) {
    close();
    port_.setPortName(name);
    port_.setBaudRate(baud);
    port_.setDataBits(QSerialPort::Data8);
    port_.setParity(QSerialPort::NoParity);
    port_.setStopBits(QSerialPort::OneStop);
    port_.setFlowControl(QSerialPort::NoFlowControl);
    if (!port_.open(QIODevice::ReadWrite)) {
        emit exchangeFailed(port_.errorString());
        return false;
    }
    emit connectionChanged(true);
    return true;
}
void SerialSession::close() {
    timeout_.stop();
    busy_ = false;
    incoming_.clear();
    outgoing_.clear();
    sent_ = 0;
    const bool wasOpen = port_.isOpen();
    if (wasOpen) port_.close();
    if (wasOpen) emit connectionChanged(false);
}
bool SerialSession::isOpen() const { return port_.isOpen(); }
bool SerialSession::exchange(const QByteArray &body, int timeoutMs) {
    if (!isOpen() || busy_ || timeoutMs < 1 || timeoutMs > 60000) return false;
    outgoing_ = FrameCodec::encode(body);
    if (outgoing_.isEmpty()) return false;
    incoming_.clear();
    sent_ = 0;
    busy_ = true;
    timeout_.start(timeoutMs);
    pumpWrite();
    return busy_;
}
void SerialSession::pumpWrite() {
    if (!busy_ || sent_ >= outgoing_.size()) return;
    const qint64 written = port_.write(outgoing_.constData() + sent_, outgoing_.size() - sent_);
    if (written < 0) { fail(port_.errorString()); return; }
    sent_ += written;
}
void SerialSession::onBytesWritten(qint64) { pumpWrite(); }
void SerialSession::onReadyRead() {
    if (!busy_) { port_.readAll(); return; }
    incoming_.append(port_.readAll());
    if (incoming_.size() < 2) return;
    const int count = (int(quint8(incoming_[0])) << 8) | int(quint8(incoming_[1]));
    if (count < 1 || count > FrameCodec::MaxBody) { fail(QStringLiteral("Invalid response length")); return; }
    const int total = count + 6;
    if (incoming_.size() > total) { fail(QStringLiteral("Unexpected trailing response bytes")); return; }
    if (incoming_.size() != total) return;
    auto decoded = FrameCodec::decode(incoming_);
    if (!decoded.ok()) { fail(decoded.error); return; }
    timeout_.stop();
    busy_ = false;
    incoming_.clear();
    outgoing_.clear();
    emit responseReceived(decoded.body);
}
void SerialSession::onError(QSerialPort::SerialPortError error) {
    if (error != QSerialPort::NoError && error != QSerialPort::NotOpenError) {
        const QString reason = port_.errorString();
        if (busy_) fail(reason);
        else if (port_.isOpen()) { close(); emit exchangeFailed(reason); }
    }
}
void SerialSession::onTimeout() { fail(QStringLiteral("ECU response timeout")); }
void SerialSession::fail(const QString &reason) {
    close(); // Failed exchange invalidates session, matching the legacy transport.
    emit exchangeFailed(reason);
}
}
