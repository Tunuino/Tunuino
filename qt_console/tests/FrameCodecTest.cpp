#include <QtTest>
#include "../src/protocol/FrameCodec.h"
#include "../src/protocol/ConfigProtocol.h"
class FrameCodecTest : public QObject {
    Q_OBJECT
private slots:
    void roundTrip() {
        const QByteArray body("TUNUINO?");
        const auto packet = tunuino::FrameCodec::encode(body);
        QCOMPARE(packet.size(), body.size() + 6);
        QCOMPARE(tunuino::FrameCodec::decode(packet).body, body);
    }
    void configPageReadRequest() {
        const QByteArray request = tunuino::ConfigProtocol::readPage(1, 0x1234, 256);
        QCOMPARE(request.toHex(), QByteArray("72000112340100"));
        QCOMPARE(tunuino::FrameCodec::decode(tunuino::FrameCodec::encode(request)).body, request);
        QVERIFY(tunuino::ConfigProtocol::readPage(0, 0, 1).isEmpty());
        QVERIFY(!tunuino::ConfigProtocol::readPage(5, 0, 1).isEmpty());
        QVERIFY(!tunuino::ConfigProtocol::readPage(6, 0, 1).isEmpty());
        QVERIFY(tunuino::ConfigProtocol::readPage(7, 0, 1).isEmpty());
        QVERIFY(tunuino::ConfigProtocol::readPage(1, 0, 0).isEmpty());
        QVERIFY(tunuino::ConfigProtocol::readPage(1, 0, 257).isEmpty());
    }
    void configPageReadResponse() {
        QByteArray payload(256, char(0x5a));
        QByteArray result(1, char(0));
        result += payload;
        QByteArray parsed;
        QVERIFY(tunuino::ConfigProtocol::parseReadResponse(result, 256, &parsed));
        QCOMPARE(parsed, payload);
        result[0] = char(0x83);
        QVERIFY(!tunuino::ConfigProtocol::parseReadResponse(result, 256, &parsed));
        QVERIFY(!tunuino::ConfigProtocol::parseReadResponse(result, 255, &parsed));
        QVERIFY(!tunuino::ConfigProtocol::parseReadResponse(result, 256, nullptr));
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
