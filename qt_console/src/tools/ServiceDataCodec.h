#pragma once
#include "ToolsData.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QCryptographicHash>
#include "../ecu/EcuPowerState.h"
namespace tunuino {
class ServiceDataCodec {
public:
    static quint32 u32(const QByteArray &b,int i){return quint32(SensorCalibrationCodec::u16(b,i))<<16|quint32(SensorCalibrationCodec::u16(b,i+2));}
    static quint64 u64(const QByteArray &b,int i){return quint64(u32(b,i))<<32|u32(b,i+4);}
    static void put32(QByteArray &b,int i,quint32 n){SensorCalibrationCodec::put(b,i,n>>16);SensorCalibrationCodec::put(b,i+2,n);}
    static bool validVin(const QString &vin){if(vin.isEmpty())return true;if(vin.size()!=17)return false;for(QChar c:vin)if(!((c>='0'&&c<='9')||(c>='A'&&c<='Z'&&c!='I'&&c!='O'&&c!='Q')))return false;return true;}
    static bool validComment(const QString &s){if(QString::fromUtf8(s.toUtf8())!=s||s.toUcs4().size()>64||s.toUtf8().size()>256)return false;for(uint c:s.toUcs4())if(c<32||(c>=127&&c<160)||(c>=0xd800&&c<=0xdfff)||c>0x10ffff)return false;return true;}
    static QString vin(const QByteArray &b){return b.size()==512&&b[1]==1?QString::fromLatin1(b.mid(2,17)):QString{};}
    static QString comment(const QByteArray &b){return b.size()==512?QString::fromUtf8(b.mid(22,SensorCalibrationCodec::u16(b,20))):QString{};}
    static bool valid(const QByteArray &b){
        if(b.size()!=512||b[0]!=1||quint8(b[1])>1||b[19]||b[295]||(quint8(b[294])&~15u)||b.mid(316,8)!=QByteArray(8,0)||b.mid(332)!=QByteArray(180,0))return false;
        if(b[1]==0&&b.mid(2,17)!=QByteArray(17,0))return false;
        if(!validVin(vin(b)))return false;const int length=SensorCalibrationCodec::u16(b,20);
        if(length>256||b.mid(22+length,256-length)!=QByteArray(256-length,0))return false;
        const auto text=comment(b);if(text.toUtf8()!=b.mid(22,length)||!validComment(text))return false;
        const int mask=quint8(b[294]);return (!(mask&1)||u32(b,282)<=20000000)&&(!(mask&2)||u32(b,286)<=500000)
            &&(!(mask&4)||(qint32(u32(b,290))>=-40000&&qint32(u32(b,290))<=150000))&&(!(mask&8)||(qint32(u32(b,296))>=-4000&&qint32(u32(b,296))<=15000));
    }
    static quint32 crc32(const QByteArray &bytes){quint32 crc=0xffffffffu;for(char byte:bytes){crc^=quint8(byte);for(int bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1u)?0xedb88320u:0u);}return ~crc;}
    static QByteArray project(const ToolSnapshot &base,const QString &comment){
        if(!base.valid()||!EcuPowerState::supportedIdentity(base.identity)||SensorCalibrationCodec::u16(base.pages[0],0)!=2||!validComment(comment))return {};
        QJsonArray pages;QByteArray content("Tunuino-project-v1");content+=base.identity.mid(20,12);QByteArray generation(4,0);put32(generation,0,base.generation);content+=generation;for(const auto &page:base.pages){pages.append(QString::fromLatin1(page.toHex()));content+=page;}content+=comment.toUtf8();
        const QJsonObject object{{"format","tunuino-project-v1"},{"target",QJsonObject{{"family",1},{"mcu",1},{"hardwareRevision",7},{"schema",2}}},{"sourceUid",QString::fromLatin1(base.identity.mid(20,12).toHex())},{"sourceGeneration",double(base.generation)},{"pages",pages},{"comment",comment},{"crc32",QStringLiteral("%1").arg(crc32(content),8,16,QLatin1Char('0'))},{"sha256",QString::fromLatin1(QCryptographicHash::hash(content,QCryptographicHash::Sha256).toHex())}};
        return QJsonDocument(object).toJson(QJsonDocument::Indented);
    }
};
}
