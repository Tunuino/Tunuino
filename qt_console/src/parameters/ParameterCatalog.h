#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <QByteArray>
#include "../protocol/SensorCalibrationCodec.h"
namespace tunuino {
struct ParameterField {
    QString key,label,unit;
    int offset=0,width=2;
    bool signedValue=false;
    double scale=1,minimum=0,maximum=65535,initial=0;
    QStringList choices;
    int bit=0;
};
struct ParameterBlock { QString key,title,help;int page=1;QVector<ParameterField> fields; };
class ParameterCatalog {
public:
    static QVector<ParameterBlock> blocks();
    static double value(const QByteArray &page,const ParameterField &f);
    static bool put(QByteArray &page,const ParameterField &f,double value);
    static QByteArray ownedMask(const ParameterBlock &block);
};
}
