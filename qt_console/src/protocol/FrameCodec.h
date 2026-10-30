#pragma once
#include <QByteArray>
#include <QString>
namespace tunuino {
struct DecodeResult { QByteArray body; QString error; bool ok() const { return error.isEmpty(); } };
class FrameCodec {
public:
    static constexpr int MaxBody = 4096;
    static QByteArray encode(const QByteArray &body);
    static DecodeResult decode(const QByteArray &packet);
};
}
