#pragma once
#include "ProteusPinoutData.h"
#include "ConnectorPhotoWidget.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QHeaderView>
#include <QLabel>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolTip>
#include <QVBoxLayout>
#include <functional>

namespace tunuino {
inline QString proteusContact(int connector, int pin) {
    return QString("J10%1/%2").arg(connector + 1).arg(pin);
}
inline bool proteusFixedPin(const QString &type) {
    return type=="12v" || type=="5v" || type=="gnd" || type=="agnd" || type=="can" || type=="knock";
}
inline QColor proteusPinColor(const QString &type) { return connectorPinColor(type); }
class ProteusConnectorPhoto final : public ConnectorPhotoWidget {
public:
    explicit ProteusConnectorPhoto(int connector, QWidget *parent = nullptr)
        : ConnectorPhotoWidget(proteusPins(connector),
            ":/proteus/" + QStringList{"black23","black35","white35"}.value(connector) + ".jpg",
            QString("J10%1/").arg(connector+1), QString("proteusPhoto%1").arg(connector+1), parent) {}
};

class ProteusPinoutWidget final : public QWidget {
public:
    std::function<void(const QString &)> contactSelected;
    explicit ProteusPinoutWidget(QWidget *parent = nullptr) : QWidget(parent) {
        setObjectName("proteusPinout");
        auto *layout = new QVBoxLayout(this); layout->setContentsMargins(0,0,0,0);
        auto *heading = new QLabel(tr("Proteus connector pinouts"), this);
        heading->setStyleSheet("font-weight:bold"); layout->addWidget(heading);
        auto *orientation = new QLabel(tr("ECU mating face — latch at the top. Click a pin to see its connection."), this);
        orientation->setWordWrap(true); layout->addWidget(orientation);
        tabs_ = new QTabWidget(this); tabs_->setObjectName("proteusConnectors");
        const QStringList titles{"J101 · Black23", "J102 · Black35", "J103 · White35"};
        for (int c=0; c<3; ++c) {
            photos_[c] = new ProteusConnectorPhoto(c, tabs_);
            photos_[c]->selected = [this,c](int pin) { selectPin(c,pin); };
            tabs_->addTab(photos_[c], titles[c]);
        }
        layout->addWidget(tabs_, 3);
        auto *legend = new QLabel(tr("Yellow: selected connection   • Blue dot: assigned function\nPin border: electrical signal type"), this);
        legend->setWordWrap(true); layout->addWidget(legend);
        details_ = new QLabel(tr("Select a function on the left or a connector pin."), this);
        details_->setObjectName("proteusPinDetail"); details_->setWordWrap(true); details_->setTextFormat(Qt::PlainText);
        layout->addWidget(details_);
        table_ = new QTableWidget(0,4,this); table_->setObjectName("proteusPinTable");
        table_->setHorizontalHeaderLabels({tr("Pin"), tr("Signal"), tr("Function"), tr("Assigned / state")});
        table_->verticalHeader()->hide(); table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table_->setSelectionBehavior(QAbstractItemView::SelectRows); table_->setSelectionMode(QAbstractItemView::SingleSelection);
        table_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
        table_->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
        table_->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Stretch);
        table_->horizontalHeader()->setSectionResizeMode(3,QHeaderView::Interactive);
        table_->setColumnWidth(3,140);table_->setWordWrap(true);table_->setTextElideMode(Qt::ElideNone);
        connect(table_->horizontalHeader(),&QHeaderView::sectionResized,this,[this]{table_->resizeRowsToContents();});
        layout->addWidget(table_, 2);
        auto *credits = new QPushButton(tr("Photo credits / license"),this);
        credits->setFlat(true); layout->addWidget(credits);
        connect(credits,&QPushButton::clicked,this,[this] {
            QDialog dialog(this); dialog.setWindowTitle(tr("Connector photographs")); dialog.resize(650,450);
            auto *layout = new QVBoxLayout(&dialog); auto *text = new QPlainTextEdit(&dialog); text->setReadOnly(true);
            QFile file(":/proteus/PHOTO-LICENSE.txt"); if(file.open(QIODevice::ReadOnly)) text->setPlainText(QString::fromUtf8(file.readAll()));
            layout->addWidget(text); auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close,&dialog);
            layout->addWidget(buttons); connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject); dialog.exec();
        });
        connect(tabs_,&QTabWidget::currentChanged,this,[this] {
            const QString prefix=QString("J10%1/").arg(tabs_->currentIndex()+1);
            bool relevant=false; for(const auto &pin:highlights_) relevant|=pin.startsWith(prefix);
            if(!relevant) highlights_.clear();
            rebuildTable(); updatePhotos(); updateDetail();
        });
        connect(table_,&QTableWidget::cellClicked,this,[this](int row,int) { selectPin(tabs_->currentIndex(),row+1); });
        rebuildTable();
    }
    void setDedicatedFunctions(const QMap<QString,QString> &functions) {
        if(dedicated_==functions) return;
        dedicated_=functions; updatePhotos(); updateTable(); updateDetail();
    }
    void setAssignments(const QMap<QString,QStringList> &assignments) {
        if(assignments_ == assignments) return;
        assignments_ = assignments; updatePhotos(); updateTable(); updateDetail();
    }
    void highlight(const QStringList &contacts, bool follow = true) {
        highlights_ = contacts;
        if(follow && !contacts.isEmpty()) {
            for(int c=0;c<3;++c) if(contacts.first().startsWith(QString("J10%1/").arg(c+1))) tabs_->setCurrentIndex(c);
        }
        updatePhotos(); updateDetail();
    }
    void selectPin(int connector,int number) {
        const auto pins = proteusPins(connector);
        if(number<1||number>pins.size()) return;
        tabs_->setCurrentIndex(connector);
        const QString contact = proteusContact(connector,number);
        highlight({contact},false); table_->selectRow(number-1);
        table_->scrollToItem(table_->item(number-1,0));
        if(contactSelected) contactSelected(contact);
    }
private:
    void rebuildTable() {
        const auto pins = proteusPins(tabs_->currentIndex()); table_->clearSelection(); table_->setCurrentCell(-1,-1); table_->setRowCount(pins.size());
        for(int row=0;row<pins.size();++row) {
            const auto &pin=pins[row];
            table_->setItem(row,0,new QTableWidgetItem(QString::number(pin.number)));
            table_->setItem(row,1,new QTableWidgetItem(pin.signal));
            table_->item(row,1)->setForeground(proteusPinColor(pin.type));
            table_->item(row,1)->setToolTip(pin.description);
            table_->setItem(row,2,new QTableWidgetItem(pin.description));
            table_->item(row,2)->setToolTip(pin.description);
            table_->setItem(row,3,new QTableWidgetItem);
        }
        updateTable(); table_->resizeRowsToContents();
    }
    void updateTable() {
        bool changed=false;
        for(int row=0;row<table_->rowCount();++row) {
            const auto functions=assignments_.value(proteusContact(tabs_->currentIndex(),row+1));
            const auto pin=proteusPins(tabs_->currentIndex())[row];
            const auto contact=proteusContact(tabs_->currentIndex(),row+1);
            const auto value=!functions.isEmpty()?functions.join("; "):dedicated_.contains(contact)?dedicated_.value(contact):proteusFixedPin(pin.type)?tr("Fixed hardware connection"):tr("No function selected");
            if(table_->item(row,3)->text()!=value) {table_->item(row,3)->setText(value);table_->item(row,3)->setToolTip(value);changed=true;}
        }
        if(changed) table_->resizeRowsToContents();
    }
    void updatePhotos() {
        for(int c=0;c<3;++c) {
            QSet<int> highlight,assigned; QMap<int,QString> functions;
            for(const auto &pin:proteusPins(c)) {
                const auto contact=proteusContact(c,pin.number);
                if(highlights_.contains(contact)) highlight.insert(pin.number);
                if(assignments_.contains(contact)){assigned.insert(pin.number);functions[pin.number]=assignments_[contact].join("; ");}
                else if(dedicated_.contains(contact)) functions[pin.number]=dedicated_.value(contact);
            }
            photos_[c]->setState(highlight,assigned,functions);
        }
    }
    void updateDetail() {
        QStringList lines;
        for(const auto &contact:highlights_) for(int c=0;c<3;++c) for(const auto &pin:proteusPins(c))
            if(contact==proteusContact(c,pin.number)) {
                const auto functions=assignments_.value(contact);
                lines<<QString("%1 — %2: %3\n%4").arg(contact,pin.signal,pin.description,
                       !functions.isEmpty()?functions.join("; "):dedicated_.contains(contact)?dedicated_.value(contact):proteusFixedPin(pin.type)?tr("Fixed hardware connection."):tr("No software function selected for this pin. This does not indicate whether a wire is physically connected."));
            }
        details_->setText(lines.isEmpty()?tr("Select a function on the left or a connector pin."):lines.join("\n"));
    }
    QTabWidget *tabs_;
    ProteusConnectorPhoto *photos_[3]{};
    QTableWidget *table_;
    QLabel *details_;
    QMap<QString,QStringList> assignments_;
    QMap<QString,QString> dedicated_;
    QStringList highlights_;
};
}
