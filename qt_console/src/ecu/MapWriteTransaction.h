#pragma once
#include <QByteArray>
#include <QObject>
#include <QString>
#include <array>
#include <functional>
#include "../protocol/SensorCalibrationCodec.h"

namespace tunuino {
// A single explicit write operation owns the session until completion. Requests
// are never retried, especially Burn. No ARM, RUN, STOP or output-test commands.
class MapWriteTransaction : public QObject {
public:
    explicit MapWriteTransaction(QObject *parent=nullptr) : QObject(parent) {}
    using Pages=std::array<QByteArray,6>;
    std::function<bool(const QByteArray&,int)> send;
    std::function<bool(const QByteArray&,quint32,const Pages&)> savePrevious;
    std::function<void(bool,bool,const QString&)> completed; // success, uncertain permanent save, message.
    bool start(int page,const QByteArray &desired,const QByteArray &baseMap,
               const QByteArray &identity,quint32 generation);
    bool startSensor(SensorKind kind,const QByteArray &desired,const QByteArray &base,
        const QByteArray &identity,quint32 generation);
    bool startParameters(int page,const QByteArray &desired,const QByteArray &base,
        const QByteArray &identity,quint32 generation,const QByteArray &mask);
    void receive(const QByteArray &body);
    void transportFailed(const QString &reason);
    bool busy() const {return phase_!=Phase::Idle;}
    bool burnRequested() const {return burnRequested_;}
    bool writeRequested() const {return writeRequested_;}
    bool identityMatched() const {return identityMatched_;}
    quint32 generation() const {return resultGeneration_;}
    static quint32 crc32(const QByteArray &bytes);
private:
    enum class Phase {Idle,Identity,ValidateBase,OutputsBase,RuntimeBase,ReadBase,ConfirmBase,
        OutputsStage,RuntimeStage,Write,ReadStage,ValidateStage,OutputsBurn,RuntimeBurn,
        Burn,ReadPersisted,ValidatePersisted,RuntimeFinal,OutputsFinal};
    bool begin(int page,const QByteArray &desired,const QByteArray &base,const QByteArray &identity,quint32 generation);
    void issue(Phase phase,const QByteArray &request,int timeout=3000);
    void finish(bool success,const QString &message);
    void fail(const QString &message);
    void beginRead(Phase phase);
    void requestRead();
    void requestWrite();
    bool validation(const QByteArray &body,bool dirty,bool persisted,quint32 generation,bool checkCrc) const;
    bool runtime(const QByteArray &body,quint32 generation,bool dirty=false) const;
    static bool outputsIdle(const QByteArray &body);
    static quint32 u32(const QByteArray &bytes,int offset);
    static void put16(QByteArray &bytes,int offset,quint16 value);
    static QByteArray pageCommand(char command,int page);
    static QByteArray readCommand(int page,int offset,int count);
    Phase phase_=Phase::Idle;
    int pageCount_=4;
    int page_=0,offset_=0,readPage_=0,sensor_=-1;
    quint32 generation_=0,resultGeneration_=0;
    bool burnRequested_=false,writeRequested_=false,identityMatched_=false;
    QByteArray desired_,baseMap_,identity_;
    Pages base_,expected_,read_;
};
}
