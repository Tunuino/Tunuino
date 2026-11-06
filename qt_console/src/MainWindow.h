#pragma once
#include <QMainWindow>
class QLabel;
class QCloseEvent;
class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget *parent = nullptr);
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    QLabel *connectionStatus_;
};
