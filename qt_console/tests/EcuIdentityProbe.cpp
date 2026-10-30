#include "../src/ecu/SerialSession.h"
#include <QCoreApplication>
#include <QSerialPortInfo>
#include <QTimer>
#include <QTextStream>
#include <QCommandLineParser>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription("Tunuino ECU serial identity diagnostic (read-only)");
    parser.addHelpOption();
    QCommandLineOption portOption({"p", "port"}, "ECU serial port (e.g. COM4)", "port");
    QCommandLineOption baudOption({"b", "baud"}, "Serial baud rate", "baud", "115200");
    parser.addOption(portOption);
    parser.addOption(baudOption);
    parser.process(app);
    QTextStream out(stdout), err(stderr);
    if (!parser.isSet(portOption)) {
        out << "Available serial ports:\n";
        for (const auto &p : QSerialPortInfo::availablePorts())
            out << p.portName() << " | " << p.description() << " | VID:"
                << QString::number(p.vendorIdentifier(), 16) << " PID:"
                << QString::number(p.productIdentifier(), 16) << "\n";
        out << "Use --port COMx to run a read-only identity probe.\n";
        return 0;
    }
    bool baudOk = false;
    const int baud = parser.value(baudOption).toInt(&baudOk);
    if (!baudOk || baud < 1) { err << "Invalid baud rate\n"; return 2; }
    tunuino::SerialSession session;
    int exitCode = 3;
    QObject::connect(&session, &tunuino::SerialSession::exchangeFailed, &app,
        [&](const QString &reason) { err << "FAILED: " << reason << "\n"; exitCode = 4; app.quit(); });
    QObject::connect(&session, &tunuino::SerialSession::responseReceived, &app,
        [&](const QByteArray &response) {
            if (response.size() != 32 || response.mid(1, 7) != QByteArray("TUNUINO", 7)) {
                err << "FAILED: Unexpected identity response (" << response.size() << " bytes)\n";
                exitCode = 5;
            } else {
                out << "Identity OK | protocol=" << int(quint8(response[8]))
                    << " | board-family=" << int(quint8(response[9]))
                    << " | MCU=" << int(quint8(response[10]))
                    << " | firmware=" << int(quint8(response[13])) << "."
                    << int(quint8(response[14])) << "." << int(quint8(response[15])) << "\n";
                exitCode = 0;
            }
            app.quit();
        });
    if (!session.open(parser.value(portOption), baud)) return 3;
    // Existing protocol identity request. No write/calibration/flash commands.
    if (!session.exchange(QByteArray("TUNUINO?"), 3000)) {
        err << "FAILED: Could not send identity request\n";
        return 3;
    }
    return app.exec() == 0 ? exitCode : 6;
}
