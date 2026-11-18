#pragma once
#include "ConnectorPin.h"
#include <QWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QSet>
#include <QMap>
#include <QToolTip>
#include <functional>
#include <utility>
namespace tunuino {
inline QColor connectorPinColor(const QString &type) {
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
class ConnectorPhotoWidget : public QWidget {
public:
    std::function<void(int)> selected;
    ConnectorPhotoWidget(QVector<ConnectorPin> pins, const QString &photo, QString contactPrefix,
                         const QString &objectName, QWidget *parent = nullptr, bool labelPrefix = false)
        : QWidget(parent), pins_(std::move(pins)), prefix_(std::move(contactPrefix)), labelPrefix_(labelPrefix) {
        photo_.load(photo);
        setMouseTracking(true);
        setMinimumHeight(130);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setObjectName(objectName);
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
            painter.setPen(QPen(chosen ? QColor("#ffffff") : connectorPinColor(pin.type), chosen ? 3. : 2.));
            painter.setBrush(chosen ? QColor("#ffd65a") : QColor("#f4f6f7"));
            painter.drawEllipse(center, radius, radius);
            painter.setPen(QColor("#162027"));
            painter.drawText(QRectF(center.x()-radius, center.y()-radius, radius*2, radius*2),
                             Qt::AlignCenter, (labelPrefix_ ? prefix_ : QString{}) + QString::number(pin.number));
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
            QString text = prefix_ + QString::number(number) + " — " + pin.signal + "\n" + pin.description;
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
    QVector<ConnectorPin> pins_;
    QString prefix_;
    bool labelPrefix_ = false;
    QPixmap photo_;
    QSet<int> highlighted_, assigned_;
    QMap<int, QString> functions_;
};

}
