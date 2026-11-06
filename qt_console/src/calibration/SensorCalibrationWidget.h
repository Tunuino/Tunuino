#pragma once
#include "../protocol/SensorCalibrationCodec.h"
#include <QWidget>
#include <QElapsedTimer>
#include <functional>
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
namespace tunuino {
class SensorCalibrationWidget final : public QWidget {
public:
    explicit SensorCalibrationWidget(SensorKind kind,QWidget *parent=nullptr);
    SensorKind kind() const {return kind_;}
    void load(const QByteArray &header,const QByteArray &aux,const QByteArray &identity,quint32 generation);
    void setAvailable(bool available);
    void updateRaw(const QByteArray &runtime);
    void saved(){edited_=false;}
    void save();
    std::function<void(SensorKind,const QByteArray&)> writeRequested;
private:
    bool hasSample() const;
    void chooseInput();
    void refresh();
    void edit();
    void setInput(int channel);
    SensorCalibration configuration() const;
    QString inputName(int channel) const;
    SensorKind kind_;
    QByteArray header_,aux_,identity_,runtime_;
    quint32 generation_=0;
    QElapsedTimer sampleAge_;
    QComboBox *input_;
    QComboBox *preset_=nullptr;
    std::array<QDoubleSpinBox*,3> volts_{},values_{};
    std::array<QPushButton*,3> capture_{};
    QPushButton *save_;
    QLabel *status_,*live_;
    bool available_=false,loading_=false,edited_=false;
};
}
