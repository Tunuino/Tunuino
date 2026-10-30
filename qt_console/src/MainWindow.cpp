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
                    model->clear();
                    if (brand->currentIndex() == 0) model->addItem(tr("Select a manufacturer first"));
                    else if (custom) model->addItem(tr("Custom analog output"));
                    else if (brand->currentIndex() == 1) model->addItems({"AEM X-Series (verify part number)", "AEM UEGO (verify part number)"});
                    else if (brand->currentIndex() == 2) model->addItems({"Innovate LC-2", "Innovate MTX-L (verify generation)"});
                    else if (brand->currentIndex() == 3) model->addItems({"14point7 Spartan 2", "14point7 Spartan 3 (verify variant)"});
                    else if (brand->currentIndex() == 4) model->addItems({"PLX SM-AFR (verify generation)", "PLX DM-6 / SM-AFR (verify controller)"});
                    for (auto *field : {vLow, vHigh, afrLow, afrHigh}) field->setEnabled(custom);
                    note->setText(custom
                        ? tr("Enter two distinct voltage/AFR points from the controller manual. AFR scaling depends on fuel; confirm the controller output. ECU write is disabled.")
                        : tr("Model selection only. No voltage-to-AFR curve is assumed: verify the exact part number and analog output settings from its manual."));
                    const bool valid = custom && vLow->value() < vHigh->value() && afrLow->value() != afrHigh->value();
                    curveStatus->setText(valid ? tr("Custom two-point curve: valid for review (not saved)") : tr("Calibration: not configured / invalid points"));
                    curveStatus->setStyleSheet(valid ? QStringLiteral("color: #15803d;") : QStringLiteral("color: #b45309;"));
                };
                connect(brand, QOverload<int>::of(&QComboBox::currentIndexChanged), page, [update](int) { update(); });
                for (auto *field : {vLow, vHigh, afrLow, afrHigh})
                    connect(field, QOverload<double>::of(&QDoubleSpinBox::valueChanged), page, [update](double) { update(); });
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
    connectionStatus_ = new QLabel(tr("ECU disconnected — no live data"), dock);
    connectionStatus_->setAlignment(Qt::AlignCenter);
    dock->setWidget(connectionStatus_);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    menuBar()->addMenu(tr("File"));
    menuBar()->addMenu(tr("ECU"));
    menuBar()->addMenu(tr("Settings"));
    statusBar()->showMessage(tr("Qt 6 interface prototype — ECU writes disabled"));
}
