#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVector>
namespace tunuino {
// Editing model only. Construction, resizing and imports never touch the ECU.
class EngineProfileDraft {
public:
    struct Cylinder {int injector=-1,coil=-1;quint32 tdc=0;};
    void load(const QByteArray &header,const QByteArray &aux);
    void rebase(const QByteArray &header,const QByteArray &aux){header_=header;aux_=aux;}
    bool resize(int count);
    void setOrder(int event,int cylinder);
    void setTdc(int cylinder,quint32 angle);
    bool assign(int cylinder,bool ignition,int channel,QString &error);
    QByteArray build(QString &error)const;
    QJsonObject json()const;
    bool import(const QJsonObject &object,QString &error,bool editorDraft=false);
    int count()const{return cylinders.size();}
    QVector<Cylinder> cylinders;
    QVector<int> order;
    int operation=0;
    bool inverted=false;
    int requiredFuel=5000,dwell=2000,deadTime=1000,maximumInjection=15000;
    quint32 toothZero=0;
    int camFirst=0,camLast=0;
private:
    bool permutation()const;
    bool validate(QString &error)const;
    int coilPartner(int cylinder)const;
    void sortOrder();
    QByteArray header_,aux_,profile_;
    QVector<Cylinder> retained_;
};
}
