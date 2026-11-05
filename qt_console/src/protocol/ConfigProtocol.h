#pragma once
#include <QByteArray>
#include <QtGlobal>

namespace tunuino {
// Wire-level helpers for the firmware's existing paged configuration protocol.
// This module does not activate outputs, write settings, or claim persistence.
class ConfigProtocol final {
public:
    static constexpr quint16 ConfigPage = 1;
    static constexpr quint16 MaxChunk = 256;
    static QByteArray readPage(quint16 page, quint16 offset, quint16 count) {
        if (page < 1 || page > 4 || count == 0 || count > MaxChunk) return {};
        QByteArray request(7, char(0));
        request[0] = 'r';
        put16(request, 1, page);
        put16(request, 3, offset);
        put16(request, 5, count);
        return request;
    }
    static bool parseReadResponse(const QByteArray &body, quint16 expectedCount,
                                  QByteArray *payload) {
        if (!payload || expectedCount == 0 || expectedCount > MaxChunk
            || body.size() != int(expectedCount) + 1
            || quint8(body[0]) != 0) return false;
        *payload = body.mid(1);
        return true;
    }
private:
    static void put16(QByteArray &bytes, int offset, quint16 value) {
        bytes[offset] = char(value >> 8);
        bytes[offset + 1] = char(value & 0xff);
    }
};
}
