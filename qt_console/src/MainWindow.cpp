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
#include <QSerialPortInfo>
#include <QAction>
#include <QListWidget>
#include <QTimer>
#include <QSharedPointer>
#include <QVector>
#include "ecu/SerialSession.h"
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("Tunuino Console — Qt 6");
    resize(1400, 850);
    auto *tabs = new QTabWidget(this);
    setCentralWidget(tabs);
    auto *navigationDock = new QDockWidget(tr("TUNUINO  |  Navigation"), this);
    navigationDock->setObjectName("NavigationDock");
    auto *navigationList = new QListWidget(navigationDock);
    navigationList->setMinimumWidth(195);
    navigationList->setMaximumWidth(340);
    navigationDock->setMinimumWidth(190);
    navigationList->addItems({tr("Fuel Table"), tr("Ignition Table"), tr("Datalogging"),
                              tr("Firmware"), tr("Calibration & Setup")});
    navigationDock->setWidget(navigationList);
    addDockWidget(Qt::LeftDockWidgetArea, navigationDock);
    connect(navigationList, &QListWidget::currentRowChanged, tabs, &QTabWidget::setCurrentIndex);
    connect(tabs, &QTabWidget::currentChanged, navigationList, [navigationList](int index) { navigationList->setCurrentRow(index); });
    navigationList->setCurrentRow(0);

    // Compact calibration workspace with explicit read-only state.
    auto *fuelPage = new QWidget(tabs);
    auto *fuelLayout = new QVBoxLayout(fuelPage);
    fuelLayout->setContentsMargins(10, 10, 10, 10);
    fuelLayout->setSpacing(8);
    auto *fuelToolbar = new QWidget(fuelPage);
    auto *fuelToolbarLayout = new QHBoxLayout(fuelToolbar);
    fuelToolbarLayout->setContentsMargins(0, 0, 0, 0);
    auto *fuelHeading = new QLabel(tr("FUEL TABLE  |  16 x 16"), fuelToolbar);
    fuelHeading->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11pt; color: #e69a32;"));
    fuelToolbarLayout->addWidget(fuelHeading);
    fuelToolbarLayout->addStretch();
    auto *fuelState = new QLabel(tr("READ ONLY  -  Awaiting ECU map protocol"), fuelToolbar);
    fuelState->setStyleSheet(QStringLiteral("color: #bdbfc1;"));
    fuelToolbarLayout->addWidget(fuelState);
    fuelLayout->addWidget(fuelToolbar);
    auto *fuelActions = new QWidget(fuelPage);
    auto *fuelActionsLayout = new QHBoxLayout(fuelActions);
    fuelActionsLayout->setContentsMargins(0, 0, 0, 0);
    auto *fuelRead = new QPushButton(tr("Read from ECU"), fuelActions);
    auto *fuelSave = new QPushButton(tr("Write to ECU"), fuelActions);
    fuelRead->setEnabled(false);
    fuelSave->setEnabled(false);
    fuelRead->setToolTip(tr("Requires validated ECU map-read protocol."));
    fuelSave->setToolTip(tr("Requires validated ECU map-write protocol and safety checks."));
    fuelActionsLayout->addWidget(fuelRead);
    fuelActionsLayout->addWidget(fuelSave);
    fuelActionsLayout->addStretch();
    fuelLayout->addWidget(fuelActions);
    auto *maps = new QTableWidget(16, 16, fuelPage);
    maps->setAlternatingRowColors(true);
    maps->setSelectionMode(QAbstractItemView::ExtendedSelection);
    maps->setSelectionBehavior(QAbstractItemView::SelectItems);
    maps->setCornerButtonEnabled(false);
    maps->setToolTip(tr("Map values are unavailable until the ECU table-read protocol is implemented."));
    maps->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    maps->horizontalHeader()->setDefaultSectionSize(62);
    maps->horizontalHeader()->setMinimumSectionSize(48);
    maps->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    maps->verticalHeader()->setDefaultSectionSize(28);
    maps->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    maps->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    maps->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    for (int row = 0; row < 16; ++row)
        for (int col = 0; col < 16; ++col) {
            auto *cell = new QTableWidgetItem();
            cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);
            maps->setItem(row, col, cell);
        }
    fuelLayout->addWidget(maps, 1);
    auto *fuelFooter = new QLabel(tr("Values are intentionally blank until read from ECU. Editing and writing remain disabled."), fuelPage);
    fuelFooter->setStyleSheet(QStringLiteral("color: #a9aaad;"));
    fuelLayout->addWidget(fuelFooter);
    tabs->addTab(fuelPage, tr("Fuel Table"));
    auto *ignitionPage = new QWidget(tabs);
    auto *ignitionLayout = new QVBoxLayout(ignitionPage);
    auto *ignitionHeading = new QLabel(tr("IGNITION TABLE  |  16 x 16"), ignitionPage);
    ignitionHeading->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11pt; color: #e69a32;"));
    ignitionLayout->addWidget(ignitionHeading);
    auto *ignitionActions = new QWidget(ignitionPage);
    auto *ignitionActionsLayout = new QHBoxLayout(ignitionActions);
    ignitionActionsLayout->setContentsMargins(0, 0, 0, 0);
    auto *ignitionRead = new QPushButton(tr("Read from ECU"), ignitionActions);
    auto *ignitionSave = new QPushButton(tr("Write to ECU"), ignitionActions);
    ignitionRead->setEnabled(false);
    ignitionSave->setEnabled(false);
    ignitionActionsLayout->addWidget(ignitionRead);
    ignitionActionsLayout->addWidget(ignitionSave);
    ignitionActionsLayout->addStretch();
    ignitionLayout->addWidget(ignitionActions);
    auto *ignitionMap = new QTableWidget(16, 16, ignitionPage);
    ignitionMap->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ignitionMap->horizontalHeader()->setDefaultSectionSize(62);
    ignitionMap->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ignitionMap->verticalHeader()->setDefaultSectionSize(28);
    ignitionMap->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    ignitionMap->setAlternatingRowColors(true);
    for (int row = 0; row < 16; ++row)
        for (int col = 0; col < 16; ++col) {
            auto *cell = new QTableWidgetItem();
            cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);
            ignitionMap->setItem(row, col, cell);
        }
    ignitionLayout->addWidget(ignitionMap, 1);
    auto *ignitionFooter = new QLabel(tr("Ignition values unavailable. ECU read/write protocol not yet integrated."), ignitionPage);
    ignitionFooter->setStyleSheet(QStringLiteral("color: #a9aaad;"));
    ignitionLayout->addWidget(ignitionFooter);
    tabs->addTab(ignitionPage, tr("Ignition Table"));
    auto *loggingPage = new QWidget(tabs);
    auto *loggingLayout = new QVBoxLayout(loggingPage);
    auto *loggingHeading = new QLabel(tr("DATALOGGING  |  Live channels"), loggingPage);
    loggingHeading->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11pt; color: #e69a32;"));
    loggingLayout->addWidget(loggingHeading);
    auto *loggingInfo = new QLabel(tr("No live data stream available. ECU telemetry protocol must be implemented before recording."), loggingPage);
    loggingInfo->setWordWrap(true);
    loggingLayout->addWidget(loggingInfo);
    auto *loggingChannels = new QTableWidget(7, 3, loggingPage);
    loggingChannels->setHorizontalHeaderLabels({tr("Channel"), tr("Value"), tr("Unit")});
    const char *channelNames[] = {"RPM", "TPS", "MAP", "Lambda", "CLT", "IAT", "Battery"};
    const char *channelUnits[] = {"rpm", "%", "kPa", "lambda", "deg C", "deg C", "V"};
    for (int i = 0; i < 7; ++i) {
        auto *channel = new QTableWidgetItem(QString::fromLatin1(channelNames[i]));
        auto *value = new QTableWidgetItem(tr("Unavailable"));
        auto *unit = new QTableWidgetItem(QString::fromLatin1(channelUnits[i]));
        for (auto *item : {channel, value, unit}) item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        loggingChannels->setItem(i, 0, channel);
        loggingChannels->setItem(i, 1, value);
        loggingChannels->setItem(i, 2, unit);
    }
    loggingChannels->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    loggingChannels->setMaximumHeight(300);
    loggingLayout->addWidget(loggingChannels);
    auto *loggingStart = new QPushButton(tr("Start logging"), loggingPage);
    loggingStart->setEnabled(false);
    loggingStart->setToolTip(tr("Requires verified ECU telemetry and timestamp handling."));
    loggingLayout->addWidget(loggingStart, 0, Qt::AlignLeft);
    loggingLayout->addStretch();
    tabs->addTab(loggingPage, tr("Datalogging"));

    auto *firmwarePage = new QWidget(tabs);
    auto *firmwareLayout = new QVBoxLayout(firmwarePage);
    auto *firmwareHeading = new QLabel(tr("FIRMWARE  |  Device management"), firmwarePage);
    firmwareHeading->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11pt; color: #e69a32;"));
    firmwareLayout->addWidget(firmwareHeading);
    auto *firmwareInfo = new QLabel(tr("USB serial identity is supported. Firmware flashing and recovery are not yet implemented in this Qt application."), firmwarePage);
    firmwareInfo->setWordWrap(true);
    firmwareLayout->addWidget(firmwareInfo);
    auto *firmwareChecklist = new QFormLayout();
    firmwareChecklist->addRow(tr("Identity"), new QLabel(tr("Available when connected"), firmwarePage));
    firmwareChecklist->addRow(tr("USB DFU flashing"), new QLabel(tr("Not implemented"), firmwarePage));
    firmwareChecklist->addRow(tr("Image compatibility check"), new QLabel(tr("Not implemented"), firmwarePage));
    firmwareChecklist->addRow(tr("Post-flash verification"), new QLabel(tr("Not implemented"), firmwarePage));
    firmwareLayout->addLayout(firmwareChecklist);
    auto *firmwareFlash = new QPushButton(tr("Flash firmware (not available)"), firmwarePage);
    firmwareFlash->setEnabled(false);
    firmwareLayout->addWidget(firmwareFlash, 0, Qt::AlignLeft);
    firmwareLayout->addStretch();
    tabs->addTab(firmwarePage, tr("Firmware"));
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
        {"Engine Control", {"Idle Control", "Boost Control", "Launch Control", "ETB / Drive by Wire", nullptr}},
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
                input->addItem(tr("Select analog input"));
                for (int i = 1; i <= 8; ++i) input->addItem(tr("Analog Input %1").arg(i));
                auto *vLow = new QDoubleSpinBox(page);
                auto *vHigh = new QDoubleSpinBox(page);
                auto *afrLow = new QDoubleSpinBox(page);
                auto *afrHigh = new QDoubleSpinBox(page);
                for (auto *v : {vLow, vHigh}) { v->setRange(0, 5); v->setDecimals(3); v->setSuffix(" V"); }
                for (auto *a : {afrLow, afrHigh}) { a->setRange(5, 30); a->setDecimals(2); a->setSuffix(" AFR"); }
                auto *note = new QLabel(page);
                note->setWordWrap(true);
                auto *curveStatus = new QLabel(tr("Calibration: not configured"), page);
                curveStatus->setWordWrap(true);
                form->addRow(tr("Wideband manufacturer"), brand);
                form->addRow(tr("Controller / model"), model);
                form->addRow(tr("ECU input"), input);
                form->addRow(tr("Voltage at first calibration point"), vLow);
                form->addRow(tr("AFR at first calibration point"), afrLow);
                form->addRow(tr("Voltage at second calibration point"), vHigh);
                form->addRow(tr("AFR at second calibration point"), afrHigh);
                form->addRow(note);
                form->addRow(tr("Calibration status"), curveStatus);
                auto update = [=]() {
                    const bool custom = brand->currentIndex() == brand->count() - 1;
                    const bool aem = brand->currentIndex() == 1 && model->currentIndex() == 0;
                    const bool lc2 = brand->currentIndex() == 2 && model->currentIndex() == 0;
                    const bool spartan2 = brand->currentIndex() == 3 && model->currentIndex() == 0;
                    const bool spartan3v2 = brand->currentIndex() == 3 && model->currentIndex() == 1;
                    const bool plxGen2 = brand->currentIndex() == 4 && model->currentIndex() == 0;
                    for (auto *field : {vLow, vHigh, afrLow, afrHigh})
                        field->setEnabled(custom);
                    if (aem) {
                        vLow->setValue(0.5); afrLow->setValue(8.5);
                        vHigh->setValue(4.5); afrHigh->setValue(18.0);
                        note->setText(tr("Verified AEM 30-0300: AFR = 2.3750 * V + 7.3125. Below 0.50 V = not ready; above 4.50 V = sensor error. Check exact part number. Source: documents.aemelectronics.com/techlibrary_30-0300.pdf"));
                    } else if (lc2) {
                        vLow->setValue(0.0); afrLow->setValue(7.35);
                        vHigh->setValue(5.0); afrHigh->setValue(22.39);
                        note->setText(tr("Verified Innovate LC-2 factory Analog Output 1 (yellow), gasoline AFR: 0 V = 7.35, 5 V = 22.39. Programmable outputs may differ. Source: innovatemotorsports.com LC-2 manual."));
                    } else if (spartan2 || spartan3v2 || plxGen2) {
                        vLow->setValue(0.0); afrLow->setValue(10.0);
                        vHigh->setValue(5.0); afrHigh->setValue(20.0);
                        note->setText(spartan2
                            ? tr("14point7 Spartan 2 green linear output: 0–5 V = 10–20 gasoline AFR. Startup 1.66 V / 3.33 V are calibration signals, not live AFR. Confirm hardware revision.")
                            : spartan3v2
                            ? tr("14point7 Spartan 3 v2 GREEN high-performance output, factory default: 0–5 V = 0.68–1.36 lambda (approximately 10–20 gasoline AFR). Programmable scaling may differ. Do not use the brown status output.")
                            : tr("PLX SM-AFR Gen2 wideband analog output: 0–5 V corresponds to approximately 10–20 gasoline AFR. Verify Gen2 hardware; other generations are not covered."));
                    } else {
                        note->setText(custom
                            ? tr("Enter two points from your controller manual. Confirm the configured analog output and AFR fuel reference.")
                            : tr("No verified analog scaling for this exact model. Do not use another controller's curve."));
                    }
                    const bool valid = custom && vLow->value() < vHigh->value() && afrLow->value() != afrHigh->value();
                    const bool preset = aem || lc2 || spartan2 || spartan3v2 || plxGen2;
                    curveStatus->setText(preset ? tr("Manufacturer profile loaded for review (not saved)") :
                        valid ? tr("Custom two-point curve: valid for review (not saved)") : tr("Calibration: not configured / invalid points"));
                    curveStatus->setStyleSheet(preset || valid ? QStringLiteral("color: #15803d;") : QStringLiteral("color: #b45309;"));
                };
                connect(brand, QOverload<int>::of(&QComboBox::currentIndexChanged), page, [=](int) {
                    model->clear();
                    if (brand->currentIndex() == 0) model->addItem(tr("Select manufacturer first"));
                    else if (brand->currentIndex() == 1) model->addItems({"AEM X-Series 30-0300 (verified)", "Other AEM model (unverified)"});
                    else if (brand->currentIndex() == 2) model->addItems({"Innovate LC-2 Output 1 (verified)", "Other Innovate model/output (unverified)"});
                    else if (brand->currentIndex() == 3) model->addItems({"14point7 Spartan 2 (linear output)", "14point7 Spartan 3 v2 (factory green output)", "Other Spartan variant (unverified)"});
                    else if (brand->currentIndex() == 4) model->addItems({"PLX SM-AFR Gen2 (verified analog output)", "Other PLX generation (unverified)"});
                    else model->addItem(tr("Custom analog output"));
                    update();
                });
                connect(model, QOverload<int>::of(&QComboBox::currentIndexChanged), page, [update](int) { update(); });
                for (auto *field : {vLow, vHigh, afrLow, afrHigh})
                    connect(field, QOverload<double>::of(&QDoubleSpinBox::valueChanged), page, [update](double) { update(); });
                model->addItem(tr("Select manufacturer first"));
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
                auto *input = new QComboBox(page);
                input->addItem(tr("Select input"));
                for (int i = 1; i <= 8; ++i) input->addItem(tr("Analog Input %1").arg(i));
                form->addRow(tr("ECU analog input"), input);
                auto *curve = new QComboBox(page);
                if (QString::fromLatin1(name) == QStringLiteral("MAP")) {
                    curve->addItems({tr("Select MAP sensor calibration"), tr("Manufacturer-specific curve"), tr("Custom 2-point voltage / kPa")});
                } else {
                    curve->addItems({tr("Select temperature calibration"), tr("Manufacturer thermistor curve"), tr("Custom resistance / temperature table")});
                }
                form->addRow(tr("Calibration curve"), curve);
                auto *help = new QLabel(tr("Choose the exact sensor and calibration curve before applying values to the ECU. No generic transfer curve is assumed."), page);
                help->setWordWrap(true);
                form->addRow(help);
                auto *apply = new QPushButton(tr("Apply to ECU (not connected)"), page);
                apply->setEnabled(false);
                form->addRow(apply);
            }
            if (QString::fromLatin1(name) == QStringLiteral("Idle Control") ||
                QString::fromLatin1(name) == QStringLiteral("Boost Control") ||
                QString::fromLatin1(name) == QStringLiteral("Launch Control")) {
                const QString modeName = QString::fromLatin1(name);
                auto *enabled = new QCheckBox(tr("Enable strategy (preview only)"), page);
                enabled->setChecked(false);
                auto *mode = new QComboBox(page);
                auto *rpm = new QDoubleSpinBox(page);
                rpm->setRange(0, 12000);
                rpm->setDecimals(0);
                rpm->setSingleStep(50);
                rpm->setSuffix(tr(" rpm"));
                auto *threshold = new QDoubleSpinBox(page);
                auto *hysteresis = new QDoubleSpinBox(page);
                auto *output = new QComboBox(page);
                output->addItems({tr("Not assigned"), "PWM 1", "PWM 2", "PWM 3"});
                auto *description = new QLabel(page);
                description->setWordWrap(true);
                if (modeName == QStringLiteral("Idle Control")) {
                    mode->addItems({tr("Disabled"), tr("Open Loop"), tr("Closed Loop PID")});
                    rpm->setValue(850);
                    threshold->setRange(0, 100);
                    threshold->setSuffix(" %");
                    threshold->setValue(20);
                    hysteresis->setRange(0, 50);
                    hysteresis->setSuffix(" %");
                    hysteresis->setValue(3);
                    form->addRow(tr("Idle mode"), mode);
                    form->addRow(tr("Target Idle RPM (reference)"), rpm);
                    form->addRow(tr("Base valve duty (reference)"), threshold);
                    form->addRow(tr("Duty limit / hysteresis (reference)"), hysteresis);
                    form->addRow(tr("Idle valve output"), output);
                    description->setText(tr("Target RPM is a starting reference, not a verified engine setting. PID tuning and coolant-temperature tables are not connected."));
                } else if (modeName == QStringLiteral("Boost Control")) {
                    mode->addItems({tr("Disabled"), tr("Open Loop Duty"), tr("Closed Loop Target")});
                    threshold->setRange(0, 400);
                    threshold->setSuffix(" kPa abs");
                    threshold->setValue(100);
                    hysteresis->setRange(0, 100);
                    hysteresis->setSuffix(" %");
                    hysteresis->setValue(0);
                    form->addRow(tr("Boost mode"), mode);
                    form->addRow(tr("Target MAP (reference)"), threshold);
                    form->addRow(tr("Solenoid duty (reference)"), hysteresis);
                    form->addRow(tr("Boost solenoid output"), output);
                    description->setText(tr("100 kPa absolute is approximately atmospheric pressure, NOT a safe boost target. Overboost protection and control maps must be configured before use."));
                } else {
                    mode->addItems({tr("Disabled"), tr("Clutch switch"), tr("Digital input")});
                    rpm->setValue(0);
                    threshold->setRange(0, 100);
                    threshold->setSuffix(" % TPS");
                    threshold->setValue(0);
                    hysteresis->setRange(0, 30);
                    hysteresis->setSuffix(" s");
                    hysteresis->setDecimals(1);
                    form->addRow(tr("Activation source"), mode);
                    form->addRow(tr("Launch RPM (unset)"), rpm);
                    form->addRow(tr("Minimum TPS (unset)"), threshold);
                    form->addRow(tr("Maximum activation duration (unset)"), hysteresis);
                    description->setText(tr("Launch limits are intentionally unset. Input wiring, RPM cut strategy and safety conditions must be validated before activation."));
                }
                form->addRow(enabled);
                form->addRow(description);
                auto *apply = new QPushButton(tr("Apply to ECU (not connected)"), page);
                apply->setEnabled(false);
                form->addRow(apply);
                // Preview controls must never imply that a live ECU strategy is enabled.
                enabled->setEnabled(false);
            }
            if (QString::fromLatin1(name) == QStringLiteral("ETB / Drive by Wire")) {
                auto *mode = new QComboBox(page);
                mode->addItems({tr("Disabled (default)"), tr("ETB / Drive by Wire (requires verified ECU support)")});
                auto *pedal = new QComboBox(page);
                pedal->addItems({tr("Select pedal sensor"), "APP 1 + APP 2"});
                auto *throttle = new QComboBox(page);
                throttle->addItems({tr("Select throttle sensor"), "TPS 1 + TPS 2"});
                auto *driver = new QComboBox(page);
                driver->addItems({tr("Select motor driver"), tr("Not configured")});
                auto *pidP = new QDoubleSpinBox(page);
                auto *pidI = new QDoubleSpinBox(page);
                auto *pidD = new QDoubleSpinBox(page);
                for (auto *spin : {pidP, pidI, pidD}) {
                    spin->setRange(0.0, 1000.0);
                    spin->setDecimals(3);
                    spin->setSingleStep(0.01);
                    spin->setValue(0.0);
                }
                auto *step1 = new QPushButton(tr("1. Calibrate pedal released / pressed"), page);
                auto *step2 = new QPushButton(tr("2. Calibrate throttle closed / open"), page);
                auto *step3 = new QPushButton(tr("3. Verify redundant sensors and motor direction"), page);
                auto *step4 = new QPushButton(tr("4. Validate safety limits and save"), page);
                for (auto *button : {step1, step2, step3, step4}) button->setEnabled(false);
                auto *warning = new QLabel(tr("SAFETY: Electronic throttle actuation is locked. Calibration requires redundant APP/TPS plausibility checks, motor current and position limits, watchdog/fault shutdown, and a verified ECU driver. Never run an actuator sweep with the engine running."), page);
                warning->setWordWrap(true);
                warning->setStyleSheet(QStringLiteral("color: #b45309;"));
                form->addRow(tr("ETB control"), mode);
                form->addRow(tr("Accelerator pedal"), pedal);
                form->addRow(tr("Throttle position"), throttle);
                form->addRow(tr("H-bridge motor driver"), driver);
                form->addRow(tr("PID P (unset)"), pidP);
                form->addRow(tr("PID I (unset)"), pidI);
                form->addRow(tr("PID D (unset)"), pidD);
                form->addRow(step1);
                form->addRow(step2);
                form->addRow(step3);
                form->addRow(step4);
                form->addRow(warning);
                auto *diagnostics = new QGroupBox(tr("ETB Safety Diagnostics"), page);
                auto *diagnosticsLayout = new QFormLayout(diagnostics);
                auto *status = new QLabel(tr("NOT MONITORED — ECU disconnected"), diagnostics);
                status->setStyleSheet(QStringLiteral("font-weight: bold; color: #b45309;"));
                auto *appStatus = new QLabel(tr("APP1 / APP2: no live data"), diagnostics);
                auto *tpsStatus = new QLabel(tr("TPS1 / TPS2: no live data"), diagnostics);
                auto *motorStatus = new QLabel(tr("Motor driver: not verified"), diagnostics);
                auto *faultStatus = new QLabel(tr("Fault handling: not verified"), diagnostics);
                diagnosticsLayout->addRow(tr("ETB status"), status);
                diagnosticsLayout->addRow(tr("Pedal correlation"), appStatus);
                diagnosticsLayout->addRow(tr("Throttle correlation"), tpsStatus);
                diagnosticsLayout->addRow(tr("Motor and current limits"), motorStatus);
                diagnosticsLayout->addRow(tr("Fail-safe / watchdog"), faultStatus);
                form->addRow(diagnostics);
                auto *safetyInfo = new QLabel(tr("When live telemetry is implemented, mismatched redundant sensors, out-of-range voltage, motor overcurrent, actuator timeout or watchdog faults must produce a clear alarm and trigger the firmware fail-safe. The console cannot replace ECU-side protection."), page);
                safetyInfo->setWordWrap(true);
                form->addRow(safetyInfo);
                // No ECU capabilities or live feedback are available yet. Keep the controls
                // non-operational to avoid implying that drive-by-wire is safe to enable.
                for (auto *control : {mode, pedal, throttle, driver}) control->setEnabled(false);
                for (auto *spin : {pidP, pidI, pidD}) spin->setEnabled(false);
            }
            if (QString::fromLatin1(name) == QStringLiteral("Inputs") ||
                QString::fromLatin1(name) == QStringLiteral("Outputs")) {
                const bool isOutput = QString::fromLatin1(name) == QStringLiteral("Outputs");
                auto *channel = new QComboBox(page);
                auto *function = new QComboBox(page);
                auto *polarity = new QComboBox(page);
                auto *help = new QLabel(page);
                help->setWordWrap(true);
                if (isOutput) {
                    channel->addItem(tr("Select output"));
                    for (int i = 1; i <= 12; ++i) channel->addItem(tr("Output %1").arg(i));
                    function->addItems({tr("Unassigned"), tr("Injector 1"), tr("Injector 2"), tr("Injector 3"),
                        tr("Injector 4"), tr("Injector 5"), tr("Injector 6"), tr("Injector 7"), tr("Injector 8"),
                        tr("Ignition 1"), tr("Ignition 2"), tr("Ignition 3"), tr("Ignition 4"),
                        tr("Ignition 5"), tr("Ignition 6"), tr("Ignition 7"), tr("Ignition 8"),
                        tr("Fuel Pump"), tr("Cooling Fan 1"), tr("Cooling Fan 2"),
                        tr("Idle Valve"), tr("Boost Solenoid"), tr("Tachometer")});
                    polarity->addItems({tr("Active High"), tr("Active Low")});
                    help->setText(tr("Choose an output and its function. The physical pin and available drivers depend on the detected ECU board; no pin is assigned automatically."));
                } else {
                    channel->addItem(tr("Select input"));
                    for (int i = 1; i <= 12; ++i) channel->addItem(tr("Digital Input %1").arg(i));
                    for (int i = 1; i <= 8; ++i) channel->addItem(tr("Analog Input %1").arg(i));
                    function->addItems({tr("Unassigned"), tr("Clutch Switch"), tr("Brake Switch"),
                        tr("Launch Enable"), tr("Flex Fuel"), tr("Oil Pressure"),
                        tr("Fuel Pressure"), tr("External MAP"), tr("Wideband 1"),
                        tr("Wideband 2"), tr("Switchable Map")});
                    polarity->addItems({tr("Active High"), tr("Active Low")});
                    help->setText(tr("Choose a physical input and function. Analog/digital compatibility and pin mapping require ECU board validation."));
                }
                form->addRow(isOutput ? tr("Output channel") : tr("Input channel"), channel);
                form->addRow(tr("Assigned function"), function);
                form->addRow(tr("Signal polarity"), polarity);
                form->addRow(help);
                auto *apply = new QPushButton(tr("Apply to ECU (not connected)"), page);
                apply->setEnabled(false);
                form->addRow(apply);
                auto update = [=]() {
                    const bool selected = channel->currentIndex() > 0;
                    function->setEnabled(selected);
                    polarity->setEnabled(selected);
                    if (!selected) function->setCurrentIndex(0);
                };
                connect(channel, QOverload<int>::of(&QComboBox::currentIndexChanged), page,
                    [update](int) { update(); });
                update();
            }
            if (QString::fromLatin1(name) == QStringLiteral("Coils") ||
                QString::fromLatin1(name) == QStringLiteral("Dwell") ||
                QString::fromLatin1(name) == QStringLiteral("Trigger") ||
                QString::fromLatin1(name) == QStringLiteral("Injectors") ||
                QString::fromLatin1(name) == QStringLiteral("Fuel Pressure")) {
                const QString section = QString::fromLatin1(name);
                auto *profile = new QComboBox(page);
                auto *channel = new QComboBox(page);
                auto *note = new QLabel(page);
                note->setWordWrap(true);
                if (section == QStringLiteral("Coils")) {
                    profile->addItems({tr("Select coil type"), tr("Smart coil (built-in igniter)"), tr("Dumb coil (external igniter)")});
                    channel->addItem(tr("Select ignition channel"));
                    for (int i = 1; i <= 8; ++i) channel->addItem(tr("Ignition %1").arg(i));
                    note->setText(tr("Coil type and driver compatibility must be verified before enabling outputs."));
                } else if (section == QStringLiteral("Dwell")) {
                    profile->addItems({tr("Select dwell strategy"), tr("Battery voltage compensation table"), tr("Fixed dwell (reference only)")});
                    channel->addItem(tr("Select ignition channel"));
                    for (int i = 1; i <= 8; ++i) channel->addItem(tr("Ignition %1").arg(i));
                    note->setText(tr("Dwell values require the coil manufacturer's data. No default firing dwell is applied."));
                } else if (section == QStringLiteral("Trigger")) {
                    profile->addItems({tr("Select trigger pattern"), tr("Missing tooth"), tr("Dual wheel"), tr("Cam + crank"), tr("Custom / verify with oscilloscope")});
                    channel->addItems({tr("Select trigger input"), "Crank", "Cam 1", "Cam 2"});
                    note->setText(tr("Trigger polarity, tooth count and sync angle require verification before engine operation."));
                } else if (section == QStringLiteral("Injectors")) {
                    profile->addItems({tr("Select injector type"), tr("High impedance"), tr("Low impedance (driver required)")});
                    channel->addItem(tr("Select injector channel"));
                    for (int i = 1; i <= 8; ++i) channel->addItem(tr("Injector %1").arg(i));
                    note->setText(tr("Injector impedance, flow rate and driver compatibility must be confirmed."));
                } else {
                    profile->addItems({tr("Select pressure sensor"), tr("Analog 0–5 V"), tr("Digital / CAN (requires support)"), tr("Custom calibration")});
                    channel->addItem(tr("Select analog input"));
                    for (int i = 1; i <= 8; ++i) channel->addItem(tr("Analog Input %1").arg(i));
                    note->setText(tr("Choose the exact sensor pressure range and transfer curve before calibration."));
                }
                form->addRow(tr("Configuration"), profile);
                form->addRow(tr("Channel / Input"), channel);
                form->addRow(note);
                auto *apply = new QPushButton(tr("Apply to ECU (not connected)"), page);
                apply->setEnabled(false);
                form->addRow(apply);
            }
            if (QString::fromLatin1(name) == QStringLiteral("Analog Inputs")) {
                auto *channel = new QComboBox(page);
                channel->addItem(tr("Select analog input"));
                for (int i = 1; i <= 8; ++i) channel->addItem(tr("Analog Input %1").arg(i));
                auto *sensor = new QComboBox(page);
                sensor->addItems({tr("Unassigned"), tr("Oil Pressure"), tr("Fuel Pressure"),
                    tr("Wideband 1"), tr("Wideband 2"), tr("External MAP"),
                    tr("Oil Temperature"), tr("Exhaust Pressure"), tr("Custom Sensor")});
                auto *profile = new QComboBox(page);
                profile->addItems({tr("Select sensor profile"), tr("0–5 V linear (requires range)"),
                    tr("Manufacturer curve"), tr("Custom two-point calibration")});
                auto *note = new QLabel(tr("Select the channel, sensor and its verified calibration curve. A generic 0–5 V input does not define engineering units."), page);
                note->setWordWrap(true);
                form->addRow(tr("Input channel"), channel);
                form->addRow(tr("Sensor function"), sensor);
                form->addRow(tr("Calibration profile"), profile);
                form->addRow(note);
                auto *apply = new QPushButton(tr("Apply to ECU (not connected)"), page);
                apply->setEnabled(false);
                form->addRow(apply);
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
    auto *statusPanel = new QWidget(dock);
    auto *statusLayout = new QVBoxLayout(statusPanel);
    statusLayout->setContentsMargins(10, 12, 10, 12);
    statusLayout->setSpacing(8);
    auto *readingsTitle = new QLabel(tr("LIVE ENGINE DATA"), statusPanel);
    readingsTitle->setStyleSheet(QStringLiteral("font-weight: bold; color: #e69a32;"));
    statusLayout->addWidget(readingsTitle);
    auto *readings = new QFormLayout();
    readings->setLabelAlignment(Qt::AlignLeft);
    for (const char *parameter : {"RPM", "TPS", "MAP", "Lambda", "CLT", "IAT", "Battery"}) {
        auto *value = new QLabel(tr("No telemetry"), statusPanel);
        value->setStyleSheet(QStringLiteral("color: #a9aaad;"));
        readings->addRow(QString::fromLatin1(parameter), value);
    }
    statusLayout->addLayout(readings);
    auto *identityTitle = new QLabel(tr("ECU CONNECTION"), statusPanel);
    identityTitle->setStyleSheet(QStringLiteral("font-weight: bold; color: #e69a32;"));
    statusLayout->addSpacing(12);
    statusLayout->addWidget(identityTitle);
    connectionStatus_ = new QLabel(tr("ECU disconnected"), statusPanel);
    connectionStatus_->setWordWrap(true);
    connectionStatus_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    statusLayout->addWidget(connectionStatus_);
    statusLayout->addStretch(1);
    dock->setWidget(statusPanel);
    dock->setMinimumWidth(245);
    dock->setMaximumWidth(400);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    auto *session = new tunuino::SerialSession(this);
    // The firmware runtime command is read-only: O + offset(U16 BE) + count(U16 BE).
    auto *liveTimer = new QTimer(this);
    liveTimer->setInterval(500);
    auto identityVerified = QSharedPointer<bool>::create(false);
    auto requestLive = [session, identityVerified]() {
        if (!*identityVerified || !session->isOpen()) return;
        QByteArray command(5, char(0));
        command[0] = 'O';
        command[4] = char(100);
        (void)session->exchange(command, 1500);
    };
    connect(liveTimer, &QTimer::timeout, this, requestLive);
    auto *liveValues = statusPanel->findChildren<QLabel *>();
    // The first seven value labels follow the LIVE ENGINE DATA heading.
    auto liveReadings = QSharedPointer<QVector<QLabel *>>::create();
    for (int i = 0; i < 7 && i + 1 < liveValues.size(); ++i)
        liveReadings->append(liveValues.at(i + 1));
    auto *fileMenu = menuBar()->addMenu(tr("File"));
    fileMenu->addAction(tr("Exit"), this, &QWidget::close);
    auto *ecuMenu = menuBar()->addMenu(tr("ECU"));
    auto *connectMenu = ecuMenu->addMenu(tr("Connect to ECU"));
    auto *disconnectAction = ecuMenu->addAction(tr("Disconnect"));
    disconnectAction->setEnabled(false);
    auto *settingsMenu = menuBar()->addMenu(tr("Settings"));
    auto *refreshAction = settingsMenu->addAction(tr("Refresh serial ports"));
    auto *firmwareMenu = menuBar()->addMenu(tr("Firmware"));
    auto *dfuInfo = firmwareMenu->addAction(tr("USB DFU status / instructions"));
    connect(dfuInfo, &QAction::triggered, this, [this]() {
        QMessageBox::information(this, tr("Firmware / DFU"),
            tr("Firmware flashing is not implemented in this Qt console. Use the validated legacy flashing tool. Do not flash an unverified image."));
    });
    auto refreshPorts = [=]() {
        connectMenu->clear();
        const auto ports = QSerialPortInfo::availablePorts();
        if (ports.isEmpty()) {
            auto *none = connectMenu->addAction(tr("No serial ports detected"));
            none->setEnabled(false);
        }
        for (const auto &port : ports) {
            const QString name = port.portName();
            auto *action = connectMenu->addAction(name + QStringLiteral(" — ") + port.description());
            connect(action, &QAction::triggered, this, [=]() {
                liveTimer->stop();
                *identityVerified = false;
                session->close();
                connectionStatus_->setText(tr("Connecting to %1...").arg(name));
                if (!session->open(name, 115200)) return;
                if (!session->exchange(QByteArray("TUNUINO?"), 3000)) {
                    session->close();
                    connectionStatus_->setText(tr("Identity request failed"));
                }
            });
        }
    };
    connect(refreshAction, &QAction::triggered, this, refreshPorts);
    connect(connectMenu, &QMenu::aboutToShow, this, refreshPorts);
    connect(disconnectAction, &QAction::triggered, this, [=]() {
        liveTimer->stop();
        *identityVerified = false;
        session->close();
        connectionStatus_->setText(tr("ECU disconnected"));
    });
    connect(session, &tunuino::SerialSession::connectionChanged, this, [=](bool connected) {
        disconnectAction->setEnabled(connected);
        if (!connected) { liveTimer->stop(); *identityVerified = false; connectionStatus_->setText(tr("ECU disconnected")); }
    });
    connect(session, &tunuino::SerialSession::exchangeFailed, this, [=](const QString &error) {
        liveTimer->stop();
        *identityVerified = false;
        connectionStatus_->setText(tr("ECU communication failed: %1").arg(error));
        statusBar()->showMessage(error, 10000);
    });
    connect(session, &tunuino::SerialSession::responseReceived, this, [=](const QByteArray &data) {
        if (*identityVerified) {
            // Status + 100-byte runtime record; never treat invalid channels as zero readings.
            if (data.size() != 101 || quint8(data[0]) != 0) {
                liveTimer->stop();
                statusBar()->showMessage(tr("ECU runtime read unsupported or invalid; polling stopped."));
                return;
            }
            const auto u32 = [&data](int at) -> quint32 {
                return (quint32(quint8(data[at])) << 24) | (quint32(quint8(data[at+1])) << 16)
                    | (quint32(quint8(data[at+2])) << 8) | quint32(quint8(data[at+3]));
            };
            const quint8 mask = quint8(data[1 + 38]);
            const auto setReading = [liveReadings](int index, bool valid, const QString &value) {
                if (index < liveReadings->size()) liveReadings->at(index)->setText(valid ? value : QObject::tr("Unavailable"));
            };
            setReading(0, mask & 1, QString::number(u32(1 + 40) / 1000.0, 'f', 0) + QStringLiteral(" rpm"));
            setReading(2, mask & 2, QString::number(u32(1 + 44) / 1000.0, 'f', 1) + QStringLiteral(" kPa"));
            setReading(3, mask & 4, QString::number(u32(1 + 48) / 1000000.0, 'f', 3));
            setReading(4, mask & 8, QString::number(qint32(u32(1 + 52)) / 1000.0, 'f', 1) + QStringLiteral(" C"));
            return;
        }
        if (data.size() != 32 || data.mid(1, 7) != QByteArray("TUNUINO", 7)) {
            connectionStatus_->setText(tr("Invalid ECU identity response"));
            session->close();
            return;
        }
        const int protocol = quint8(data[8]);
        const int board = quint8(data[9]);
        const int mcu = quint8(data[10]);
        const QString version = QStringLiteral("%1.%2.%3").arg(quint8(data[13])).arg(quint8(data[14])).arg(quint8(data[15]));
        connectionStatus_->setText(tr("ECU connected | Protocol %1 | Board %2 | MCU %3 | Firmware %4")
            .arg(protocol).arg(board).arg(mcu).arg(version));
        *identityVerified = true;
        liveTimer->start();
        statusBar()->showMessage(tr("ECU identity verified. Read-only runtime polling active; configuration writes disabled."));
    });
    refreshPorts();
    statusBar()->showMessage(tr("TUNUINO  |  ECU connection ready  |  Map read/write unavailable until firmware integration"));
}
