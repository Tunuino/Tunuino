#pragma once
#include "../protocol/SensorCalibrationCodec.h"
#include "../protocol/MapPageCodec.h"
#include <QStringList>
#include <QVector>
#include <array>
namespace tunuino {
struct ToolSnapshot {
    QByteArray identity;
    quint32 generation=0;
    std::array<QByteArray,5> pages;
    bool valid() const {return identity.size()==32&&generation&&pages[0].size()==256&&pages[3].size()==256
        &&(pages[4].isEmpty()||MapPageCodec::valid(5,pages[4]))&&MapPageCodec::valid(2,pages[1])&&MapPageCodec::valid(3,pages[2]);}
};
struct Assignment {QString id,contact;QStringList functions;bool conflict=false;};
inline QVector<Assignment> inputAssignments(const ToolSnapshot &s){
    QVector<Assignment> rows;if(!s.valid())return rows;
    const int av[]={13,24,14,25,15,26,16,27,17,28,18},at[]={19,30,20,31};
    for(int i=0;i<11;++i)rows.append({QString("AV%1").arg(i+1),QString("J103 / %1").arg(av[i]),{},false});
    rows.append({"ECU supply","Internal divider",{"ECU power detection"},false});
    for(int i=0;i<4;++i)rows.append({QString("AT%1").arg(i+1),QString("J103 / %1").arg(at[i]),{},false});
    const auto assign=[&](int channel,bool temp,const QString &name){if(channel>0&&channel<=(temp?4:11))rows[temp?11+channel:channel-1].functions<<name;};
    const auto &h=s.pages[0],&a=s.pages[3];assign(quint8(h[6]),false,"MAP");assign(quint8(h[7]),false,"Lambda");assign(quint8(h[8]),true,"CLT");
    if(a[0]==1)assign(quint8(a[5]),false,"TPS");
    if(a[68]==1){assign(quint8(a[72]),true,"IAT");assign(quint8(a[73]),false,"Battery");const int flags=SensorCalibrationCodec::u16(a,70);if(flags&128)assign(quint8(a[74]),false,"Launch");if(flags&256)assign(quint8(a[75]),false,"Flex fuel");}
    rows.append({"Crank","J101 / 5 (+), 13 (-)",h[2]==1?QStringList{"Trigger"}:QStringList{},false});
    rows.append({"Cam","J101 / 4 (+), 12 (-)",quint8(h[128])>=1&&quint8(h[128])<=3&&h[156]==0?QStringList{"Phase"}:QStringList{},false});
    for(auto &row:rows)row.conflict=row.functions.size()>1;return rows;
}
inline QVector<Assignment> outputAssignments(const ToolSnapshot &s){
    QVector<Assignment> rows;if(!s.valid())return rows;
    // Proteus v0.7 PCB 383558bf, J102 terminal nets LS1..16 and IGN1..12.
    const int ls[]={3,15,4,16,5,6,7,19,8,20,9,21,10,11,12,23},ign[]={35,34,22,33,32,31,30,29,28,27,26,25};
    for(int i=0;i<28;++i)rows.append({QString(i<16?"LS%1":"IGN%1").arg(i<16?i+1:i-15),QString("J102 / %1").arg(i<16?ls[i]:ign[i-16]),{},false});
    const auto &h=s.pages[0],&a=s.pages[3];const int format=quint8(h[128]),count=quint8(h[129]);
    if(format>=1&&format<=3&&count>=1&&count<=12){
        std::array<QVector<int>,12> coils;
        for(int i=0;i<count;++i){const int at=160+8*i,injector=quint8(h[at]),coil=quint8(h[at+1]);if(injector<16)rows[injector].functions<<QString("Injector %1").arg(i+1);if(coil<12)coils[coil]<<i;}
        const auto angle=[&](int cylinder){const int at=162+8*cylinder;return (quint32(SensorCalibrationCodec::u16(h,at))<<16)|quint32(SensorCalibrationCodec::u16(h,at+2));};
        for(int i=0;i<12;++i){const auto &pair=coils[i];if(format==3&&pair.size()==2&&std::abs(qint64(angle(pair[0]))-qint64(angle(pair[1])))==36000)
                rows[16+i].functions<<QString("Ignition %1 / %2").arg(pair[0]+1).arg(pair[1]+1);
            else for(int cylinder:pair)rows[16+i].functions<<QString("Ignition %1").arg(cylinder+1);}
        const QString roles[]={"Fuel pump","Fan","Main relay"};for(int i=0;i<3;++i){const int ch=quint8(h[150+i]);if(ch>0&&ch<=16)rows[ch-1].functions<<roles[i];}
    }
    if(a[0]==1){if(a[1]!=0&&quint8(a[2])>0&&quint8(a[2])<=16)rows[quint8(a[2])-1].functions<<"Idle";if(a[3]!=0&&quint8(a[4])>0&&quint8(a[4])<=16)rows[quint8(a[4])-1].functions<<"Boost";}
    for(auto &row:rows)row.conflict=row.functions.size()>1;return rows;
}
}
