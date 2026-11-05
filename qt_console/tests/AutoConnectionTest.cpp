#include "ecu/AutoConnection.h"
#include "ecu/EcuPowerState.h"
#include <QCoreApplication>
#include <QStringList>
#include <cstdlib>
#include <cmath>
#include <iostream>
#define CHECK(condition) do { if (!(condition)) { std::cerr << "Failed: " << #condition << " at " << __LINE__ << std::endl; std::exit(1); } } while (false)

int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    QVector<tunuino::AutoPort> ports{{"COM1",0,0},{"COM2",0x0483,0xDF11},{"COM10",0xCAFE,0x4001},{"COM11",0xCAFE,0x4001}};
    QStringList attempts;
    bool opened=false, suspended=false, succeed=false;
    int closes=0;
    tunuino::AutoConnection connection({
        [&]{return ports;},[&]{return opened;},[&]{return suspended;},
        [&](const QString &name){attempts<<name;opened=succeed;return opened;},
        [&]{opened=false;++closes;}
    });
    connection.scan(0); CHECK(attempts==QStringList{"COM10"});
    connection.scan(750); CHECK(attempts.last()=="COM11");
    connection.scan(1500); CHECK(attempts.size()==2);
    succeed=true;
    connection.scan(3000); CHECK(attempts.size()==3 && opened);
    connection.scan(3750); CHECK(attempts.size()==3); // No probing a working transport.
    suspended=true; connection.scan(4500); CHECK(!opened && closes==1);
    connection.scan(5250); CHECK(attempts.size()==3);
    suspended=false; ports={{"COM18",0xCAFE,0x4001}};
    connection.scan(6000); CHECK(opened && attempts.last()=="COM18"); // Re-enumerated port.
    opened=false; succeed=false; ports.clear(); connection.scan(6750);
    ports={{"COM18",0xCAFE,0x4001}};
    connection.scan(7000); CHECK(attempts.size()==5); // Reinsertion clears old backoff.

    QByteArray identity(32,0); identity.replace(1,7,"TUNUINO");
    identity[8]=1;identity[9]=1;identity[10]=1;identity[12]=7;identity[20]=42;
    CHECK(tunuino::EcuPowerState::supportedIdentity(identity));
    auto invalid=identity;invalid[8]=2;CHECK(!tunuino::EcuPowerState::supportedIdentity(invalid));
    invalid=identity;invalid[12]=8;CHECK(!tunuino::EcuPowerState::supportedIdentity(invalid));
    invalid=identity;invalid[0]=1;CHECK(!tunuino::EcuPowerState::supportedIdentity(invalid));
    invalid=identity;invalid[20]=0;CHECK(!tunuino::EcuPowerState::supportedIdentity(invalid));
    tunuino::EcuPowerState power;
    const auto runtime=[](double volts,bool valid=true) {
        QByteArray body(101,0);body[40]=valid?1:0;
        const unsigned raw=unsigned(std::round(volts*4095.0/(3.3*9.2)));
        body[83]=char(raw>>8);body[84]=char(raw);return body;
    };
    CHECK(!power.update(runtime(0),0)); // USB alive, supply absent.
    CHECK(!power.update(runtime(12),500));
    CHECK(!power.update(runtime(12),600));
    CHECK(power.update(runtime(12),1000));
    CHECK(std::abs(power.voltage()-12)<0.01);
    CHECK(power.update(runtime(5),1500)); // Cranking dip: keep online.
    CHECK(power.update(runtime(12),5000)); // Reads can pause polling; fresh supply remains valid.
    CHECK(!power.update(runtime(0),5500)); // Key off even though USB still replies.
    CHECK(!power.update(runtime(12),6000));
    CHECK(power.update(runtime(12),6500)); // Key on again, no manual action.
    CHECK(!power.update(runtime(12,false),7000));
    CHECK(!power.update(runtime(12),7500));
    CHECK(!power.update(runtime(5),8000)); // Voltage in deadband cannot establish online.
    CHECK(!power.update(runtime(12),8500));
    CHECK(!power.update(runtime(12),11000)); // Stale confirmation cannot complete debounce.
    CHECK(power.update(runtime(12),11500));
    auto saturated=runtime(12);saturated[83]=15;saturated[84]=char(255);
    CHECK(!power.update(saturated,12000));
    CHECK(!power.update(runtime(12).left(100),12500));
    CHECK(!power.update(runtime(12),13000));
    CHECK(!power.update(runtime(12),100)); // Clock rollback requires new confirmation.
    power.reset(); CHECK(!power.online() && std::isnan(power.voltage()));
    std::cout<<"Automatic discovery, backoff, port changes, DFU isolation, identity and USB-only/key cycles: passed\n";
}
