#pragma once
#include <QObject>
#include <QJsonObject>
#include <QTableWidget>
#include <functional>
namespace tunuino {
class MapProject final:public QObject {
public:
    explicit MapProject(QWidget *window);
    std::function<QJsonObject(QString &)> capture;
    std::function<bool(const QJsonObject &,QString &)> apply;
    std::function<bool()> canUse;
    std::function<bool(const QJsonObject &,QString &)> applyEdits;
    bool save(const QString &path,QString &error);
    bool open(const QString &path,QString &error);
    void saveAs();void openDialog();
    static QByteArray encode(const QJsonObject &payload,QString &error);
    static bool decode(const QByteArray &bytes,QJsonObject &payload,QString &error);
    static QJsonObject tableState(QTableWidget *table,QTableWidget *rpm);
    static bool tableState(QTableWidget *table,QTableWidget *rpm,const QJsonObject &,double low,double high,bool apply,QString &error);
    static QJsonObject defaultPages();
    static QJsonObject vvtTableState(const QByteArray &page={});
    static QString defaultDirectory();
private:
    QWidget *window_;QString path_;
};
}
