#pragma once
#include "ConnectorPhotoWidget.h"
#include "UaefiPinoutData.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
namespace tunuino {
// Hardware reference only. No Proteus editor, ECU snapshot or write command is attached here.
class UaefiPinoutWidget final : public QWidget {
public:
    explicit UaefiPinoutWidget(QWidget *parent=nullptr):QWidget(parent) {
        setObjectName("uaefiPinout");setProperty("ecuBoard","UAefi");
        auto *layout=new QVBoxLayout(this);
        auto *heading=new QLabel(tr("UAefi F3 — connectors A–E"),this);heading->setStyleSheet("font-weight:bold");layout->addWidget(heading);
        auto *info=new QLabel(tr("Hardware pinout reference. Connector mating face, latch at the top. Component population depends on the assembled board."),this);
        info->setWordWrap(true);layout->addWidget(info);
        auto *splitter=new QSplitter(Qt::Vertical,this);splitter->setChildrenCollapsible(false);
        tabs_=new QTabWidget(splitter);tabs_->setObjectName("uaefiConnectors");
        for(int c=0;c<5;++c){const QString letter=QString(QChar('A'+c));
            photos_[c]=new ConnectorPhotoWidget(uaefiPins(c),":/uaefi/uaefi-"+letter+".jpg",letter,"uaefiPhoto"+letter,tabs_,true);
            photos_[c]->selected=[this,c](int pin){selectPin(c,pin);};
            tabs_->addTab(photos_[c],tr("%1 · %2").arg(letter,uaefiPcbConnector(c)));
        }
        table_=new QTableWidget(0,4,splitter);table_->setObjectName("uaefiPinTable");
        table_->setHorizontalHeaderLabels({tr("Pin"),tr("PCB contact"),tr("Signal"),tr("Function")});
        table_->setEditTriggers(QAbstractItemView::NoEditTriggers);table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        table_->setSelectionMode(QAbstractItemView::SingleSelection);table_->verticalHeader()->hide();
        for(int c=0;c<3;++c)table_->horizontalHeader()->setSectionResizeMode(c,QHeaderView::ResizeToContents);
        table_->horizontalHeader()->setSectionResizeMode(3,QHeaderView::Stretch);table_->setWordWrap(true);table_->setTextElideMode(Qt::ElideNone);
        connect(table_->horizontalHeader(),&QHeaderView::sectionResized,this,[this]{table_->resizeRowsToContents();});
        splitter->addWidget(tabs_);splitter->addWidget(table_);splitter->setSizes({300,260});layout->addWidget(splitter,1);
        detail_=new QLabel(tr("Select a connector pin to see its function."),this);detail_->setObjectName("uaefiPinDetail");detail_->setWordWrap(true);detail_->setTextFormat(Qt::PlainText);layout->addWidget(detail_);
        auto *credits=new QPushButton(tr("Photo credits / license"),this);credits->setFlat(true);layout->addWidget(credits);
        connect(credits,&QPushButton::clicked,this,[this]{QDialog dialog(this);dialog.setWindowTitle(tr("Connector photographs"));dialog.resize(650,450);auto *layout=new QVBoxLayout(&dialog);auto *text=new QPlainTextEdit(&dialog);text->setReadOnly(true);QFile file(":/uaefi/PHOTO-LICENSE.txt");if(file.open(QIODevice::ReadOnly))text->setPlainText(QString::fromUtf8(file.readAll()));layout->addWidget(text);auto *buttons=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);dialog.exec();});
        connect(tabs_,&QTabWidget::currentChanged,this,[this]{rebuild();});
        connect(table_,&QTableWidget::cellClicked,this,[this](int row,int){selectPin(tabs_->currentIndex(),row+1);});rebuild();
    }
    void selectPin(int connector,int number) {
        if(connector<0||connector>=5)return;const auto pins=uaefiPins(connector);if(number<1||number>pins.size())return;
        tabs_->setCurrentIndex(connector);const auto &pin=pins[number-1];
        photos_[connector]->setState({number},{},{});table_->selectRow(number-1);table_->scrollToItem(table_->item(number-1,0));
        detail_->setText(QString("%1%2 · %3/%2 — %4\n%5").arg(QString(QChar('A'+connector))).arg(number).arg(uaefiPcbConnector(connector),pin.signal,pin.description));
    }
private:
    void rebuild() {
        const int c=tabs_->currentIndex();const auto pins=uaefiPins(c);table_->setRowCount(pins.size());table_->clearSelection();table_->setCurrentCell(-1,-1);
        for(auto *photo:photos_)if(photo)photo->setState({}, {}, {});
        for(int row=0;row<pins.size();++row){const auto &pin=pins[row];const QStringList values{QString(QChar('A'+c))+QString::number(pin.number),uaefiPcbConnector(c)+"/"+QString::number(pin.number),pin.signal,pin.description};for(int col=0;col<4;++col){auto *item=new QTableWidgetItem(values[col]);item->setToolTip(values[col]);table_->setItem(row,col,item);}table_->item(row,2)->setForeground(connectorPinColor(pin.type));}
        table_->resizeRowsToContents();detail_->setText(tr("Select a connector pin to see its function."));
    }
    QTabWidget *tabs_;QTableWidget *table_;QLabel *detail_;ConnectorPhotoWidget *photos_[5]{};
};
}
