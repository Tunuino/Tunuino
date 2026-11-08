#pragma once
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
namespace tunuino {
// Original vector artwork for the main application shortcuts.
enum class ShortcutGlyph { Write, Open, Save, RestoreDesktop, StoreDesktop, Test,
    Outputs, Inputs, Keyboard, Strategies, Logbook, Autotune, Options, Scope, Graph, Tune };
inline QIcon shortcutIcon(ShortcutGlyph kind){
 QIcon icon;
 for(int size:{16,24,32,48,64}){
  QPixmap pix(size,size);pix.fill(Qt::transparent);QPainter p(&pix);p.setRenderHint(QPainter::Antialiasing);p.scale(size/32.,size/32.);
  const QColor white("#eef1df"),edge("#aeb5aa"),blue("#35b5ec"),gold("#eab54a");
  p.setPen(QPen(white,1.7,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.setBrush(Qt::NoBrush);
  switch(kind){
  case ShortcutGlyph::Write:
   p.setBrush(QColor("#667568"));p.drawRect(QRectF(8,8,16,16));for(int i=10;i<25;i+=4){p.drawLine(i,4,i,8);p.drawLine(i,24,i,28);p.drawLine(4,i,8,i);p.drawLine(24,i,28,i);}p.setPen(QPen(gold,2));p.drawLine(16,10,16,21);p.drawLine(12,17,16,21);p.drawLine(20,17,16,21);break;
  case ShortcutGlyph::Open:{
   p.setBrush(QColor("#3e5559"));QPainterPath folder;folder.moveTo(3,27);folder.lineTo(3,6);folder.lineTo(12,6);folder.lineTo(16,10);folder.lineTo(27,10);folder.lineTo(27,15);p.drawPath(folder);p.drawPolygon(QPolygonF{{3,27},{7,15},{30,15},{26,27}});break;}
  case ShortcutGlyph::Save:
   p.setBrush(QColor("#4b5b60"));p.drawRect(QRectF(5,3,23,26));p.setBrush(white);p.drawRect(QRectF(10,4,12,9));p.setBrush(QColor("#536567"));p.drawRect(QRectF(17,5,3,6));p.setBrush(Qt::NoBrush);p.drawRect(QRectF(10,18,13,10));break;
  case ShortcutGlyph::RestoreDesktop:case ShortcutGlyph::StoreDesktop:{
   p.drawRoundedRect(QRectF(3,4,24,18),1,1);p.drawLine(4,9,26,9);p.drawLine(10,26,20,26);p.drawLine(15,22,15,26);p.setBrush(QColor("#6d7977"));p.drawEllipse(QRectF(19,17,11,11));p.setPen(QPen(gold,1.7));const bool load=kind==ShortcutGlyph::RestoreDesktop;p.drawLine(24,19,24,26);p.drawLine(21,load?23:22,24,load?26:19);p.drawLine(27,load?23:22,24,load?26:19);break;}
  case ShortcutGlyph::Test:
   p.setPen(QPen(QColor("#b3d9a5"),3.5));p.drawLine(5,15,13,24);p.drawLine(13,24,27,8);break;
  case ShortcutGlyph::Outputs:case ShortcutGlyph::Inputs:
   for(int i=0;i<3;++i){p.setPen(QPen(kind==ShortcutGlyph::Inputs?blue:white,1.5));p.setBrush(kind==ShortcutGlyph::Inputs?blue:white);p.drawRect(QRectF(4,5+i*9,5,4));p.drawRoundedRect(QRectF(14,5+i*9,13,4),1,1);}break;
  case ShortcutGlyph::Keyboard:
   p.setBrush(QColor("#5d6967"));p.drawRoundedRect(QRectF(3,12,26,16),1,1);p.drawPolyline(QPolygonF{{21,12},{21,8},{27,6},{27,3}});p.setPen(QPen(white,1.4));for(int y=16;y<=20;y+=4)for(int x=7;x<=25;x+=4)p.drawPoint(x,y);p.drawLine(8,24,24,24);break;
  case ShortcutGlyph::Strategies:
   p.setPen(QPen(gold,1.5));p.setBrush(QColor("#d3942c"));p.drawRect(QRectF(3,4,9,9));p.drawRect(QRectF(20,8,9,9));p.drawRect(QRectF(20,22,9,8));p.drawLine(12,9,16,9);p.drawLine(16,9,16,26);p.drawLine(16,12,20,12);p.drawLine(16,26,20,26);break;
  case ShortcutGlyph::Logbook:
   p.setPen(QPen(edge,1.5));p.setBrush(QColor("#626d68"));p.drawRect(QRectF(6,3,21,26));p.drawLine(10,3,10,29);for(int y=8;y<27;y+=4)p.drawLine(14,y,23,y);break;
  case ShortcutGlyph::Autotune:
   p.setPen(QPen(edge,1.8));for(int y=8;y<=24;y+=8)p.drawLine(3,y,29,y);p.setBrush(QColor("#657c73"));p.drawRect(QRectF(8,4,4,8));p.drawRect(QRectF(21,12,4,8));p.drawRect(QRectF(13,20,4,8));break;
  case ShortcutGlyph::Options:
   p.setPen(QPen(gold,3));p.drawLine(5,5,25,26);p.setPen(QPen(white,3.4));p.drawLine(6,26,23,9);p.setPen(QPen(white,2));p.drawPolyline(QPolygonF{{23,9},{20,3},{26,7},{30,5},{29,12},{23,14}});p.drawEllipse(QRectF(3,24,4,4));break;
  case ShortcutGlyph::Scope:
   p.setPen(QPen(white,2.4));p.drawPolyline(QPolygonF{{3,18},{8,18},{8,7},{16,7},{16,25},{24,25},{24,13},{29,13}});break;
  case ShortcutGlyph::Graph:
   p.setPen(QPen(edge,1));for(int y=7;y<27;y+=6)p.drawLine(5,y,29,y);p.setPen(QPen(white,1.8));p.drawLine(4,3,4,28);p.drawLine(4,28,30,28);p.setPen(QPen(gold,2));p.drawPolyline(QPolygonF{{6,23},{11,15},{16,18},{22,9},{28,6}});p.setPen(QPen(blue,1.8));p.drawPolyline(QPolygonF{{6,16},{12,20},{18,12},{24,16},{28,13}});break;
  case ShortcutGlyph::Tune:
   p.setBrush(QColor("#598d71"));p.drawRect(QRectF(3,4,26,25));for(int y=10;y<29;y+=6)p.drawLine(3,y,29,y);for(int x=11;x<28;x+=9)p.drawLine(x,4,x,29);p.setPen(QPen(gold,2));p.drawLine(5,14,9,14);p.drawLine(14,20,18,20);p.drawLine(23,26,27,26);break;
  }
  p.end();icon.addPixmap(pix);
 }
 return icon;
}
}
