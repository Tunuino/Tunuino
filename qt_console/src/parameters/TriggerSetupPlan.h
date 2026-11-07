#pragma once
#include <QByteArray>
#include <QString>
namespace tunuino {
struct TriggerSetup {
    bool capture=false,inverted=false;
    int teeth=36,missing=1,maximumRpm=12000,camFirst=0,camLast=0;
    double toothZero=0;
};
class TriggerSetupPlan {
public:
    static bool hasProfile(const QByteArray &header);
    static TriggerSetup read(const QByteArray &header);
    static bool primaryValid(const TriggerSetup &settings,QString &error);
    static QByteArray apply(const QByteArray &header,const QByteArray &aux,const TriggerSetup &settings,QString &error);
    static QByteArray mask(bool profile);
};
}
