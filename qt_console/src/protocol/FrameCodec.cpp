#include "FrameCodec.h"
#include <QtGlobal>
namespace tunuino {
static quint32 crc32(const QByteArray &bytes) {
    quint32 crc = 0xffffffffu;
    for (const char byte : bytes) {
        crc ^= static_cast<quint8>(byte);
        for (int i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0u);
    }
    return ~crc;
}
static quint32 read32(const QByteArray &p, int offset) {
    return (quint32(quint8(p[offset])) << 24) | (quint32(quint8(p[offset+1])) << 16)
        | (quint32(quint8(p[offset+2])) << 8) | quint32(quint8(p[offset+3]));
}
QByteArray FrameCodec::encode(const QByteArray &body) {
    if (body.isEmpty() || body.size() > MaxBody) return {};
    QByteArray packet;
    packet.reserve(body.size() + 6);
    packet.append(char((body.size() >> 8) & 0xff));
    packet.append(char(body.size() & 0xff));
    packet.append(body);
    const quint32 crc = crc32(body);
    for (int shift = 24; shift >= 0; shift -= 8) packet.append(char((crc >> shift) & 0xff));
    return packet;
}
DecodeResult FrameCodec::decode(const QByteArray &packet) {
    if (packet.size() < 7) return {{}, QStringLiteral("Incomplete packet")};
    const int count = (int(quint8(packet[0])) << 8) | int(quint8(packet[1]));
    if (count < 1 || count > MaxBody) return {{}, QStringLiteral("Invalid body length")};
    if (packet.size() != count + 6) return {{}, QStringLiteral("Packet length mismatch")};
    const QByteArray body = packet.mid(2, count);
    if (read32(packet, 2 + count) != crc32(body)) return {{}, QStringLiteral("CRC mismatch")};
    return {body, {}};
}
}
