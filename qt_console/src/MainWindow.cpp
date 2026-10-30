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
#include <QTreeWidget>
#include <QStackedWidget>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QGroupBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSplitter>
#include <QAbstractItemView>
#include <QComboBox>
#include <QCheckBox>
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
    for (const auto &name : {tr("Ignition"), tr("Datalogging"), tr("Firmware")}) {
        auto *label = new QLabel(tr("Module pending integration with ECU firmware"), tabs);
        label->setAlignment(Qt::AlignCenter);
        tabs->addTab(label, name);
    }
    // Calibration navigation is independent of the legacy Java UI.
    auto *calibrationPage = new QWidget(tabs);
    auto *calibrationLayout = new QHBoxLayout(calibrationPage);
    auto *navigation = new QTreeWidget(calibrationPage);
    navigation->setHeaderHidden(true);
    navigation->setMinimumWidth(230);
    navigation->setMaximumWidth(380);
    navigation->setUniformRowHeights(true);
    auto *pages = new QStackedWidget(calibrationPage);
    auto *leftPanel = new QWidget(calibrationPage);
    auto *leftLayout = new QVBoxLayout(leftPanel);
    auto *search = new QLineEdit(leftPanel);
    search->setPlaceholderText(tr("Search settings..."));
    search->setClearButtonEnabled(true);
    leftLayout->addWidget(search);
    leftLayout->addWidget(navigation, 1);
    struct Category { const char *title; const char *items[8]; };
    const Category categories[] = {
        {"Sensor Calibration", {"TPS", "MAP", "CLT", "IAT", "Lambda", "Analog Inputs", nullptr}},
        {"Fuel System", {"Injector Dead Time", "Fuel Pressure", "Injectors", nullptr}},
        {"Ignition System", {"Coils", "Dwell", "Trigger", nullptr}},
        {"Engine Control", {"Idle Control", "Boost Control", "Launch Control", nullptr}},
        {"Input / Output", {"Inputs", "Outputs", nullptr}}
    };
    for (const auto &category : categories) {
        auto *parentItem = new QTreeWidgetItem(navigation, {tr(category.title)});
        for (const char *name : category.items) {
            if (!name) break;
            auto *item = new QTreeWidgetItem(parentItem, {tr(name)});
            auto *page = new QWidget(pages);
            auto *form = new QFormLayout(page);
            auto *title = new QLabel(QStringLiteral("<h2>%1</h2>").arg(tr(name).toHtmlEscaped()), page);
            form->addRow(title);
            auto *info = new QLabel(tr("Configuration preview only. ECU read/write and automatic calibration are not connected."), page);
            info->setWordWrap(true);
            form->addRow(info);
            if (QString::fromLatin1(name) == QStringLiteral("TPS")) {
                auto *minVoltage = new QDoubleSpinBox(page);
                auto *maxVoltage = new QDoubleSpinBox(page);
                for (auto *spin : {minVoltage, maxVoltage}) {
                    spin->setRange(0.0, 5.0);
                    spin->setDecimals(3);
                    spin->setSuffix(tr(" V"));
                }
                maxVoltage->setValue(5.0);
                form->addRow(tr("Closed throttle reference"), minVoltage);
                form->addRow(tr("Wide open throttle reference"), maxVoltage);
                auto *rangeStatus = new QLabel(tr("Reference values only — no sensor measurement"), page);
                form->addRow(rangeStatus);
                auto validate = [minVoltage, maxVoltage, rangeStatus]() {
                    const bool valid = maxVoltage->value() - minVoltage->value() >= 0.2;
                    rangeStatus->setText(valid ? QObject::tr("Valid reference range (not calibrated)") : QObject::tr("Invalid: open throttle must exceed closed throttle by 0.2 V"));
                    rangeStatus->setStyleSheet(valid ? QString() : QStringLiteral("color: #c0392b;"));
                };
                connect(minVoltage, QOverload<double>::of(&QDoubleSpinBox::valueChanged), page, [validate](double) { validate(); });
                connect(maxVoltage, QOverload<double>::of(&QDoubleSpinBox::valueChanged), page, [validate](double) { validate(); });
                validate();
                auto *calibrate = new QPushButton(tr("Auto Calibrate (not connected)"), page);
                calibrate->setEnabled(false);
                form->addRow(calibrate);
            }
            if (QString::fromLatin1(name) == QStringLiteral("Lambda")) {
                auto *brand = new QComboBox(page);
                brand->addItems({tr("Select wideband manufacturer"), "AEM", "Innovate Motorsports", "14point7", "PLX Devices", tr("Other / Custom")});
                auto *model = new QComboBox(page);
                auto *input = new QComboBox(page);
                input->addItems({tr("Select analog input"), "Analog 1", "Analog 2", "Analog 3", "Analog 4"});
                auto *vLow = new QDoubleSpinBox(page);
                auto *vHigh = new QDoubleSpinBox(page);
                auto *afrLow = new QDoubleSpinBox(page);
                auto *afrHigh = new QDoubleSpinBox(page);
                for (auto *v : {vLow, vHigh}) { v->setRange(0, 5); v->setDecimals(3); v->setSuffix(" V"); }
                for (auto *a : {afrLow, afrHigh}) { a->setRange(5, 30); a->setDecimals(2); a->setSuffix(" AFR"); }
                auto *note = new QLabel(page);
                note->setWordWrap(true);
                form->addRow(tr("Wideband manufacturer"), brand);
                form->addRow(tr("Controller / model"), model);
                form->addRow(tr("ECU input"), input);
                form->addRow(tr("Voltage at first calibration point"), vLow);
                form->addRow(tr("AFR at first calibration point"), afrLow);
                form->addRow(tr("Voltage at second calibration point"), vHigh);
                form->addRow(tr("AFR at second calibration point"), afrHigh);
                form->addRow(note);
                auto update = [=]() {
                    const bool custom = brand->currentIndex() == brand->count() - 1;
                    model->clear();
                    if (brand->currentIndex() == 0) model->addItem(tr("Select a manufacturer first"));
                    else if (custom) model->addItem(tr("Custom analog output"));
                    else model->addItem(tr("Exact model / transfer curve not yet verified"));
                    for (auto *field : {vLow, vHigh, afrLow, afrHigh}) field->setEnabled(custom);
                    note->setText(custom
                        ? tr("Enter the two voltage/AFR points from your controller manual. No ECU write is available yet.")
                        : tr("Manufacturer selection is a UI placeholder. No voltage-to-AFR preset is loaded; exact model and datasheet are required."));
                };
                connect(brand, QOverload<int>::of(&QComboBox::currentIndexChanged), page, [update](int) { update(); });
                update();
                auto *save = new QPushButton(tr("Apply to ECU (not connected)"), page);
                save->setEnabled(false);
                form->addRow(save);
            }
            if (QString::fromLatin1(name) == QStringLiteral("MAP") ||
                QString::fromLatin1(name) == QStringLiteral("CLT") ||
                QString::fromLatin1(name) == QStringLiteral("IAT")) {
                auto *sensor = new QComboBox(page);
                sensor->addItems({tr("Select sensor type"), tr("OEM / Manufacturer curve"), tr("Custom calibration")});
                form->addRow(tr("Sensor profile"), sensor);
                auto *help = new QLabel(tr("Choose the exact sensor and calibration curve before applying values to the ECU."), page);
                help->setWordWrap(true);
                form->addRow(help);
            }
            if (QString::fromLatin1(name) == QStringLiteral("Injector Dead Time")) {
                auto *table = new QTableWidget(5, 2, page);
                table->setHorizontalHeaderLabels({tr("Battery voltage (V)"), tr("Dead Time (ms)")});
                const QStringList volts = {"8", "10", "12", "14", "16"};
                for (int i = 0; i < volts.size(); ++i) {
                    auto *voltage = new QTableWidgetItem(volts[i]);
                    voltage->setFlags(voltage->flags() & ~Qt::ItemIsEditable);
                    table->setItem(i, 0, voltage);
                    auto *deadTime = new QTableWidgetItem(tr("Not configured"));
                    deadTime->setFlags(deadTime->flags() & ~Qt::ItemIsEditable);
                    table->setItem(i, 1, deadTime);
                }
                table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
                table->setMinimumHeight(190);
                table->setToolTip(tr("Reference voltage rows only. Dead Time requires injector manufacturer data."));
                form->addRow(table);
            }
            pages->addWidget(page);
            item->setData(0, Qt::UserRole, pages->count() - 1);
        }
        parentItem->setExpanded(true);
    }
    connect(navigation, &QTreeWidget::currentItemChanged, pages,
        [pages](QTreeWidgetItem *current, QTreeWidgetItem *) {
            if (current && current->data(0, Qt::UserRole).isValid())
                pages->setCurrentIndex(current->data(0, Qt::UserRole).toInt());
        });
    connect(search, &QLineEdit::textChanged, navigation, [navigation](const QString &query) {
        for (int i = 0; i < navigation->topLevelItemCount(); ++i) {
            auto *category = navigation->topLevelItem(i);
            const bool parentMatch = category->text(0).contains(query, Qt::CaseInsensitive);
            bool visibleChild = false;
            for (int j = 0; j < category->childCount(); ++j) {
                auto *child = category->child(j);
                const bool show = parentMatch || child->text(0).contains(query, Qt::CaseInsensitive);
                child->setHidden(!show);
                visibleChild |= show;
            }
            category->setHidden(!visibleChild);
            if (!query.isEmpty() && visibleChild) category->setExpanded(true);
        }
    });
    auto *splitter = new QSplitter(Qt::Horizontal, calibrationPage);
    splitter->addWidget(leftPanel);
    splitter->addWidget(pages);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({280, 900});
    calibrationLayout->addWidget(splitter);
    tabs->addTab(calibrationPage, tr("Calibration & Setup"));
    if (auto *first = navigation->topLevelItem(0)) navigation->setCurrentItem(first->child(0));
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
