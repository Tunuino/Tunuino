#pragma once
#include "Vvt2Codec.h"
#include <QSet>
namespace tunuino {
class EtbCodec {
public:
 static bool supported(const QByteArray &id){return Vvt2Codec::supported(id)&&(quint8(id[17])&32);}
 static int block(int channel){return 48+80*channel;}
 static QByteArray defaults(QByteArray p=QByteArray(256,0)){
  if(p.size()!=256)return {};if(p[0])return p;p[0]=1;SensorCalibrationCodec::put(p,32,500);
  for(int i=0;i<6;++i){SensorCalibrationCodec::put(p,8+4*i,i%2?4500:500);SensorCalibrationCodec::put(p,10+4*i,i%2?500:4500);}
  const int offsets[]={4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,38},values[]={1000,500,0,0,-2000,2000,6000,50,0,10000,5000,500,1000,500,10,1000};
  for(int channel=0;channel<2;++channel){const int b=block(channel);for(int j=0;j<16;++j)SensorCalibrationCodec::put(p,b+offsets[j],values[j]);p[b+36]=1;}return p;
 }
 static bool format(const QByteArray &p){
  if(p.size()!=256)return false;if(p==QByteArray(256,0))return true;
  if(p[0]!=1||p[3]||p[7]||p.mid(34,14)!=QByteArray(14,0)||p.mid(208)!=QByteArray(48,0))return false;
  QSet<int> inputs;for(int i=0;i<6;++i){const int ch=quint8(p[1+i]);if(ch>11||SensorCalibrationCodec::u16(p,8+4*i)>5000||SensorCalibrationCodec::u16(p,10+4*i)>5000||(ch&&inputs.contains(ch)))return false;if(ch)inputs.insert(ch);}
  const int tolerance=SensorCalibrationCodec::u16(p,32);if(tolerance<100||tolerance>1000)return false;
  for(int channel=0;channel<2;++channel){const int b=block(channel);const auto u=[&](int at){return SensorCalibrationCodec::u16(p,b+at);};const auto s=[&](int at){return int(qint16(u(at)));};
   if(quint8(p[b])>1||quint8(p[b+1])>2||quint8(p[b+2])>1||p[b+3]||quint8(p[b+36])>1||p[b+37]||p.mid(b+46,34)!=QByteArray(34,0)||p[b+34]||p[b+35]||p[b+44]||p[b+45])return false;
   if(u(4)<100||u(4)>5000||u(6)>20000||u(8)>20000||u(10)>5000||(!u(6)&&!u(8))||s(12)<-9500||s(12)>0||s(14)<0||s(14)>9500||u(16)<500||u(16)>9500||u(18)>200||u(20)>u(22)||u(22)>10000||u(24)<100||u(24)>50000||u(26)<100||u(26)>1000||u(28)<100||u(28)>3000||u(30)<100||u(30)>2000||u(32)<2||u(32)>20||u(38)>3000||qAbs(s(40))>u(16)||qAbs(s(42))>u(16))return false;
   if(p[b]){if(!p[b+1])return false;for(int i:{0,1,2+2*channel,3+2*channel}){const int low=SensorCalibrationCodec::u16(p,8+4*i),high=SensorCalibrationCodec::u16(p,10+4*i);if((i!=2&&!p[1+i])||low<125||low>4750||high<125||high>4750||qAbs(high-low)<1000)return false;}}
  }
  return !p[49]||!p[129]||(p[49]!=p[129]&&SensorCalibrationCodec::u16(p,52)==SensorCalibrationCodec::u16(p,132));
 }
 static bool allowed(const QByteArray &p,const QByteArray &h,const QByteArray &a,QString &error){
  if(!format(p)||h.size()!=256||a.size()!=256){error="Invalid ETB configuration, calibration, PID limits or shared PWM frequency.";return false;}if(!p[0])return true;
  for(int i=0;i<6;++i){const int ch=quint8(p[1+i]);if(!ch)continue;if(ch==quint8(h[6])||ch==quint8(h[7])||(h[38]==1&&(ch==quint8(h[39])||ch==quint8(h[48])))||(a[0]==1&&ch==quint8(a[5])&&i!=2)){error="An ETB input is occupied by another sensor.";return false;}
   const int flags=SensorCalibrationCodec::u16(a,70);if(a[68]==1&&(ch==quint8(a[73])||((flags&128)&&ch==quint8(a[74]))||((flags&256)&&ch==quint8(a[75])))){error="An ETB input is occupied by another function.";return false;}}
  if(p[48]&&(a[0]!=1||!a[5])){error="The primary ETB main must use the primary TPS input. Configure the TPS under Sensors and inputs before enabling ETB.";return false;}return true;
 }
 static bool primaryTps(const QByteArray &p,const QByteArray &a,const QByteArray &h,const QByteArray &runtime,double &percent){
  if(!format(p)||!p[0]||a.size()!=256||h.size()!=256||runtime.size()!=101||runtime[0]||!(quint8(runtime[40])&1)||(quint8(runtime[40])&2))return false;
  const int channel=p[48]?0:p[128]?1:-1;if(channel<0)return false;int positions[2];
  for(int track=0;track<2;++track){const int index=2+2*channel+track,input=index==2?(a[0]==1?quint8(a[5]):0):quint8(p[1+index]);if(input<1||input>11)return false;
   const int raw=SensorCalibrationCodec::u16(runtime,61+2*(input-1));if(raw>4095)return false;
   const int mv=int(quint64(raw)*SensorCalibrationCodec::u16(h,100)*156u/(4095u*100u));const int rest=SensorCalibrationCodec::u16(p,8+4*index),full=SensorCalibrationCodec::u16(p,10+4*index);if(mv<125||mv>4750||qAbs(full-rest)<1000)return false;
   int value=(mv-rest)*10000/(full-rest);if(value< -200||value>10200)return false;positions[track]=qBound(0,value,10000);
  }
  if(qAbs(positions[0]-positions[1])>SensorCalibrationCodec::u16(p,block(channel)+26))return false;percent=(positions[0]+positions[1])/200.;return true;
 }
 static bool reply(const QByteArray &p,int channel,quint32 generation){return p.size()==64&&p[0]==0&&p[1]==1&&quint8(p[2])==channel&&quint8(p[3])<=1&&quint8(p[4])<=2&&quint8(p[5])<=1&&quint8(p[6])<=1&&quint8(p[7])<=13&&VvtCodec::u32(p,8)==generation&&quint8(p[28])<=5&&quint8(p[48])==1&&quint8(p[49])<=1&&quint8(p[50])<=1&&quint8(p[51])<=3&&p.mid(52)==QByteArray(12,0);}
};
}
