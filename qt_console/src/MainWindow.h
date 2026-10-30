#pragma once
#include <QMainWindow>
class QLabel;
class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget *parent = nullptr);
private:
    QLabel *connectionStatus_;
};
