#include "MainWindow.h"
#include <QDockWidget>
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QHeaderView>
#include <QWidget>
#include <QVBoxLayout>
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("Tunuino Console — Qt 6");
    resize(1400, 850);
    auto *tabs = new QTabWidget(this);
    setCentralWidget(tabs);
    auto *maps = new QTableWidget(16, 16, tabs);
    maps->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    maps->verticalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    for (int row = 0; row < 16; ++row)
        for (int col = 0; col < 16; ++col) {
            auto *cell = new QTableWidgetItem();
            cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);
            maps->setItem(row, col, cell);
        }
    tabs->addTab(maps, tr("Fuel Map 16×16"));
    for (const auto &name : {tr("Ignition"), tr("Sensor Calibration"), tr("Datalogging"), tr("Firmware")}) {
        auto *label = new QLabel(tr("Module pending integration with ECU firmware"), tabs);
        label->setAlignment(Qt::AlignCenter);
        tabs->addTab(label, name);
    }
    auto *dock = new QDockWidget(tr("Engine Status"), this);
    dock->setObjectName("EngineStatusDock");
    connectionStatus_ = new QLabel(tr("ECU disconnected — no live data"), dock);
    connectionStatus_->setAlignment(Qt::AlignCenter);
    dock->setWidget(connectionStatus_);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    menuBar()->addMenu(tr("File"));
    menuBar()->addMenu(tr("ECU"));
    menuBar()->addMenu(tr("Settings"));
    statusBar()->showMessage(tr("Qt 6 interface prototype — ECU writes disabled"));
}
