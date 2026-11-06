#pragma once
#include "../logging/LogData.h"
#include <QWidget>
#include <QByteArray>
#include <QElapsedTimer>
#include <array>
namespace tunuino {
// The target is taken only from the enabled target curve read from this ECU.
// It is independent of the local AFR Target editor and uses firmware integer units.
double ecuTargetLambda(const QByteArray &aux,const QByteArray &runtime);
class TuneDisplayWidget final : public QWidget {
public:
    explicit TuneDisplayWidget(QWidget *parent=nullptr);
    void setConnected(bool connected);
    void setReadings(const std::array<double,LogChannelCount> &readings,double target);
    bool assignChannel(int slot,int channel); // -1=None, 0..6=runtime, 7=target lambda.
    int channel(int slot) const;
    double reading(int channel) const;
    void clearReadings();
protected:
    void resizeEvent(QResizeEvent *event) override;
private:
    void chooseChannel(int slot);
    void refresh();
    std::array<int,9> channels_{};
    std::array<double,8> readings_{};
    std::array<QWidget*,9> cells_{};
    QWidget *bars_;
    bool connected_=false;
    QElapsedTimer sampleAge_;
};
}
