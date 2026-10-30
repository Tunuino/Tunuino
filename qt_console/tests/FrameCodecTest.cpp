#include <QtTest>
#include "../src/protocol/FrameCodec.h"
class FrameCodecTest : public QObject {
    Q_OBJECT
private slots:
    void roundTrip() {
        const QByteArray body("TUNUINO?");
        const auto packet = tunuino::FrameCodec::encode(body);
        QCOMPARE(packet.size(), body.size() + 6);
        QCOMPARE(tunuino::FrameCodec::decode(packet).body, body);
    }
    void rejectsCorruption() {
        QByteArray packet = tunuino::FrameCodec::encode("TUNUINO?");
        packet[3] = char(packet[3] ^ 1);
        QVERIFY(!tunuino::FrameCodec::decode(packet).ok());
    }
    void rejectsInvalidLength() {
        QVERIFY(tunuino::FrameCodec::encode({}).isEmpty());
        QVERIFY(tunuino::FrameCodec::encode(QByteArray(4097, 'a')).isEmpty());
    }
};
QTEST_MAIN(FrameCodecTest)
#include "FrameCodecTest.moc"
