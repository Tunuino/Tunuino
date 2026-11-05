#pragma once
#include <QByteArray>
#include <QtGlobal>
#include <limits>

namespace tunuino {
// Proteus v0.7 hardware 383558bf7ca1c53ad8bbd8fdc9dd3a4187013c46:
// mcu.kicad_sch + proteus.kicad_pcb: R1503=82k, R1504=10k,
// 12V_RAW -> divider -> U1501 pad 43 / ADC1_IN7. board_io.c places
// ADC1_IN7 at raw[11]; runtime exposes it at offset 60+2*11.
// This measures external ECU supply, never USB presence or engine RPM.
class EcuPowerState {
public:
    static bool supportedIdentity(const QByteArray &body) {
        if (body.size()!=32 || quint8(body[0])!=0 || body.mid(1,7)!="TUNUINO"
            || quint8(body[8])!=1 || quint8(body[9])!=1 || quint8(body[10])!=1
            || quint8(body[11])!=0 || quint8(body[12])!=7) return false;
        bool nonzero=false, nonff=false;
        for(int i=20;i<32;++i) { nonzero |= quint8(body[i])!=0; nonff |= quint8(body[i])!=255; }
        return nonzero && nonff;
    }
    void reset() { online_=false; firstPowered_=-1; lastSample_=-1; voltage_=std::numeric_limits<double>::quiet_NaN(); }
    bool update(const QByteArray &response, qint64 nowMs) {
        if (response.size()!=101 || quint8(response[0])!=0 || !(quint8(response[40])&1)) {
            reset(); return false;
        }
        const unsigned raw=(unsigned(quint8(response[83]))<<8)|unsigned(quint8(response[84]));
        if (raw>=4095) { reset(); return false; } // Saturated ADC is not a voltage measurement.
        const double volts=raw*(3.3*9.2/4095.0);
        if (lastSample_>=0 && (nowMs<lastSample_ || (!online_ && nowMs-lastSample_>2000))) reset();
        lastSample_=nowMs; voltage_=volts;
        // Hysteresis keeps normal cranking voltage dips from toggling the UI.
        // Two valid readings >=250 ms apart are required to enter online.
        if (volts<=4.0) { online_=false; firstPowered_=-1; }
        else if (!online_) {
            if (volts<6.0) firstPowered_=-1;
            else if (firstPowered_<0) firstPowered_=nowMs;
            else if (nowMs-firstPowered_>=250) online_=true;
        }
        return online_;
    }
    bool online() const { return online_; }
    double voltage() const { return voltage_; }
private:
    bool online_=false;
    qint64 firstPowered_=-1;
    qint64 lastSample_=-1;
    double voltage_=std::numeric_limits<double>::quiet_NaN();
};
}
