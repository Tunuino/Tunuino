#pragma once
#include "ProteusPinoutData.h"
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
inline QColor proteusPinColor(const QString &type) {
    if (type == "av") return QColor("#eb8359");
    if (type == "at") return QColor("#56c8b7");
    if (type == "din" || type == "agnd" || type == "gnd") return QColor("#76c85d");
    if (type == "vr") return QColor("#de9054");
    if (type == "etb" || type == "can") return QColor("#50c9de");
    if (type == "ls") return QColor("#e786ca");
    if (type == "ign" || type == "hs") return QColor("#98a6ef");
    if (type == "12v" || type == "5v") return QColor("#e7cc4a");
    return QColor("#bcc4cc");
}
class ProteusConnectorPhoto final : public QWidget {
public:
    std::function<void(int)> selected;
    explicit ProteusConnectorPhoto(int connector, QWidget *parent = nullptr)
        : QWidget(parent), connector_(connector), pins_(proteusPins(connector)) {
        const QStringList names{"black23", "black35", "white35"};
        photo_.load(":/proteus/" + names[connector] + ".jpg");
        setMouseTracking(true);
        setMinimumHeight(130);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setObjectName(QString("proteusPhoto%1").arg(connector + 1));
    }
    bool hasPhoto() const { return !photo_.isNull(); }
    void setState(const QSet<int> &highlighted, const QSet<int> &assigned,
                  const QMap<int, QString> &functions) {
        if (highlighted_ == highlighted && assigned_ == assigned && functions_ == functions) return;
        highlighted_ = highlighted; assigned_ = assigned; functions_ = functions; update();
    }
    QSet<int> highlighted() const { return highlighted_; }
    QRectF imageRect() const {
        if (photo_.isNull()) return {};
        const QSizeF size = QSizeF(photo_.size()).scaled(QSizeF(width()-8, height()-8), Qt::KeepAspectRatio);
        return QRectF(QPointF((width()-size.width())/2, (height()-size.height())/2), size);
    }
    QPointF pinCenter(int number) const {
        const auto area = imageRect();
        for (const auto &pin : pins_) if (pin.number == number)
            return area.topLeft() + QPointF(pin.point.x()*area.width()/photo_.width(),
                                             pin.point.y()*area.height()/photo_.height());
        return {};
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this); painter.fillRect(rect(), QColor("#171b1e"));
        if (photo_.isNull()) return;
        painter.setRenderHint(QPainter::Antialiasing);
        const auto area = imageRect(); painter.drawPixmap(area, photo_, photo_.rect());
        const qreal radius = qBound(5.5, area.width()/52., 22.);
        QFont font = painter.font(); font.setBold(true); font.setPixelSize(qMax(8, int(radius*1.05))); painter.setFont(font);
        for (const auto &pin : pins_) {
            const auto center = pinCenter(pin.number);
            const bool chosen = highlighted_.contains(pin.number);
            painter.setPen(QPen(chosen ? QColor("#ffffff") : proteusPinColor(pin.type), chosen ? 3. : 2.));
            painter.setBrush(chosen ? QColor("#ffd65a") : QColor("#f4f6f7"));
            painter.drawEllipse(center, radius, radius);
            painter.setPen(QColor("#162027"));
            painter.drawText(QRectF(center.x()-radius, center.y()-radius, radius*2, radius*2),
                             Qt::AlignCenter, QString::number(pin.number));
            if (assigned_.contains(pin.number)) {
                painter.setPen(Qt::NoPen); painter.setBrush(QColor("#1c8fdb"));
                painter.drawEllipse(center + QPointF(radius*.72, -radius*.72), radius*.23, radius*.23);
            }
        }
    }
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) { const int pin = hit(event->position()); if (pin && selected) selected(pin); }
        QWidget::mousePressEvent(event);
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        const int number = hit(event->position());
        setCursor(number ? Qt::PointingHandCursor : Qt::ArrowCursor);
        if (number) for (const auto &pin : pins_) if (pin.number == number) {
            QString text = proteusContact(connector_, number) + " — " + pin.signal + "\n" + pin.description;
            if (functions_.contains(number)) text += "\n" + functions_[number];
            QToolTip::showText(event->globalPosition().toPoint(), text, this); return;
        }
        QToolTip::hideText();
    }
private:
    int hit(QPointF position) const {
        const qreal radius = qBound(7., imageRect().width()/45., 26.);
        for (const auto &pin : pins_) if (QLineF(position, pinCenter(pin.number)).length() <= radius) return pin.number;
        return 0;
    }
    int connector_;
    QVector<ProteusPin> pins_;
    QPixmap photo_;
    QSet<int> highlighted_, assigned_;
    QMap<int, QString> functions_;
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
        table_ = new QTableWidget(0,3,this); table_->setObjectName("proteusPinTable");
        table_->setHorizontalHeaderLabels({tr("Pin"), tr("Signal"), tr("Function / state")});
        table_->verticalHeader()->hide(); table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table_->setSelectionBehavior(QAbstractItemView::SelectRows); table_->setSelectionMode(QAbstractItemView::SingleSelection);
        table_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
        table_->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
        table_->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Stretch);
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
            table_->setItem(row,2,new QTableWidgetItem);
        }
        updateTable();
    }
    void updateTable() {
        for(int row=0;row<table_->rowCount();++row) {
            const auto functions=assignments_.value(proteusContact(tabs_->currentIndex(),row+1));
            const auto pin=proteusPins(tabs_->currentIndex())[row];
            const auto contact=proteusContact(tabs_->currentIndex(),row+1);
            const auto value=!functions.isEmpty()?functions.join("; "):dedicated_.contains(contact)?dedicated_.value(contact):proteusFixedPin(pin.type)?tr("Fixed hardware connection"):tr("No function selected");
            if(table_->item(row,2)->text()!=value) table_->item(row,2)->setText(value);
        }
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
