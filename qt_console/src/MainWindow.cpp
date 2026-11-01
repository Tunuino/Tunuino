#include "MainWindow.h"
#include <QDockWidget>
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QTabBar>
#include <QHeaderView>
#include <QWidget>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QColor>
#include <QFont>
#include <QStackedWidget>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QSpinBox>
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
#include <QByteArray>
#include <QFileDialog>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QtEndian>
#include <QDir>
#include <QCoreApplication>
#include <QPlainTextEdit>
#include <QStringList>
#include <QMap>
#include <cmath>
#ifdef Q_OS_WIN
#include <windows.h>
#include <setupapi.h>
#include <winusb.h>
#include <usbiodef.h>
#endif
#include "ecu/SerialSession.h"
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("Tunuino Console — Qt 6");
    resize(1400, 850);
    auto *tabs = new QTabWidget(this);
    setCentralWidget(tabs);
    auto *navigationDock = new QDockWidget(tr("TUNUINO  |  Navigation"), this);
    navigationDock->setObjectName("NavigationDock");
    auto *navigationPanel = new QWidget(navigationDock);
    auto *navigationLayout = new QVBoxLayout(navigationPanel);
    navigationLayout->setContentsMargins(4, 4, 4, 4);
    navigationLayout->setSpacing(4);
    auto *navigationSearch = new QLineEdit(navigationPanel);
    navigationSearch->setPlaceholderText(tr("Search settings..."));
    navigationSearch->setClearButtonEnabled(true);
    navigationLayout->addWidget(navigationSearch);
    auto *navigationTree = new QTreeWidget(navigationPanel);
    navigationTree->setHeaderHidden(true);
    navigationTree->setIndentation(16);
    navigationTree->setMinimumWidth(200);
    navigationTree->setUniformRowHeights(true);
    navigationLayout->addWidget(navigationTree, 1);
    navigationDock->setMinimumWidth(210);
    navigationDock->setWidget(navigationPanel);
    addDockWidget(Qt::LeftDockWidgetArea, navigationDock);
    // Tab IDs are explicit: unsupported features are visible but never open dummy controls.
    const auto addCategory = [=](const QString &name, const QList<QPair<QString,int>> &children) {
        auto *category = new QTreeWidgetItem(navigationTree, QStringList(name));
        QFont font = category->font(0);
        font.setBold(true);
        category->setFont(0, font);
        for (const auto &child : children) {
            auto *item = new QTreeWidgetItem(category, QStringList(child.first));
            item->setData(0, Qt::UserRole, child.second);
            if (child.second < 0) {
                item->setForeground(0, QColor(QStringLiteral("#858585")));
                item->setToolTip(0, tr("Not implemented yet"));
            }
        }
        return category;
    };
    addCategory(tr("Sensors and inputs"), {{tr("TPS Calibration"),6},{tr("MAP Calibration"),6},{tr("Temperature sensors"),6},{tr("Lambda / O2"),6}});
    addCategory(tr("Engine start"), {{tr("Cranking"),-1},{tr("Afterstart"),-1}});
    addCategory(tr("Fueling"), {{tr("Fuel Table 16 x 16"),0},{tr("AFR Target 16 x 16"),2},{tr("Injector Dead Time"),6},{tr("Base Maps"),3}});
    addCategory(tr("Ignition"), {{tr("Ignition Table 16 x 16"),1},{tr("Trigger setup"),6}});
    addCategory(tr("Overrun"), {{tr("Fuel cut"),-1}});
    addCategory(tr("Knock sensors"), {{tr("Knock settings"),-1}});
    addCategory(tr("Idle"), {{tr("Idle control"),6}});
    addCategory(tr("Functions"), {{tr("General functions"),-1}});
    addCategory(tr("Outputs"), {{tr("Output configuration"),6}});
    addCategory(tr("Boost"), {{tr("Boost control"),6}});
    addCategory(tr("DBW"), {{tr("Electronic throttle"),6}});
    addCategory(tr("Traction control"), {{tr("Traction settings"),-1}});
    addCategory(tr("VVT"), {{tr("Cam timing"),-1}});
    addCategory(tr("Sport"), {{tr("Sport mode"),-1}});
    addCategory(tr("Nitrous"), {{tr("Nitrous control"),-1}});
    addCategory(tr("Tables switching"), {{tr("Table selection"),-1}});
    addCategory(tr("Engine protection"), {{tr("Protection settings"),-1}});
    addCategory(tr("Timers"), {{tr("Timers setup"),-1}});
    addCategory(tr("DSG Gearbox"), {{tr("Gearbox settings"),-1}});
    addCategory(tr("CAN, Serial"), {{tr("Communication settings"),6}});
    addCategory(tr("Log"), {{tr("Datalogging"),4}});
    addCategory(tr("Gauges"), {{tr("Runtime gauges"),-1}});
    connect(navigationTree, &QTreeWidget::itemClicked, tabs, [tabs](QTreeWidgetItem *item, int) {
        const int index = item->data(0, Qt::UserRole).toInt();
        if (item->parent() && index >= 0 && index < tabs->count())
            tabs->setCurrentIndex(index);
        else if (!item->parent()) item->setExpanded(!item->isExpanded());
    });
    connect(navigationSearch, &QLineEdit::textChanged, navigationTree, [navigationTree](const QString &query) {
        const QString term = query.trimmed();
        for (int i = 0; i < navigationTree->topLevelItemCount(); ++i) {
            auto *category = navigationTree->topLevelItem(i);
            const bool categoryMatch = category->text(0).contains(term, Qt::CaseInsensitive);
            bool any = categoryMatch;
            for (int j = 0; j < category->childCount(); ++j) {
                auto *child = category->child(j);
                const bool match = term.isEmpty() || categoryMatch || child->text(0).contains(term, Qt::CaseInsensitive);
                child->setHidden(!match);
                any |= match;
            }
            category->setHidden(!any);
            if (!term.isEmpty() && any) category->setExpanded(true);
        }
    });
    navigationTree->topLevelItem(2)->setExpanded(true);
    // Context inspector on the right; dockable and resizable independently of map grids.
    auto *inspectorDock = new QDockWidget(tr("TUNUINO  |  Configuration"), this);
    inspectorDock->setObjectName(QStringLiteral("ConfigurationInspectorDock"));
    inspectorDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    inspectorDock->setMinimumWidth(210);
    auto *inspectorBody = new QWidget(inspectorDock);
    auto *inspectorLayout = new QVBoxLayout(inspectorBody);
    auto *inspectorTitle = new QLabel(tr("Table information"), inspectorBody);
    inspectorTitle->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11pt; color: #e69a32;"));
    inspectorLayout->addWidget(inspectorTitle);
    auto *inspectorDescription = new QLabel(inspectorBody);
    inspectorDescription->setWordWrap(true);
    inspectorLayout->addWidget(inspectorDescription);
    auto *inspectorSafety = new QLabel(tr("ECU writes require a validated protocol and explicit user confirmation."), inspectorBody);
    inspectorSafety->setWordWrap(true);
    inspectorSafety->setStyleSheet(QStringLiteral("color: #b8b8b8;"));
    inspectorLayout->addWidget(inspectorSafety);
    inspectorLayout->addStretch(1);
    inspectorDock->setWidget(inspectorBody);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock);
    resizeDocks({navigationDock, inspectorDock}, {235, 260}, Qt::Horizontal);
    const QStringList inspectorNotes = {
        tr("Fuel / VE table. 16 x 16 grid. ECU values must be read and verified before use."),
        tr("Ignition advance table. 16 x 16 grid. Verify trigger configuration and timing before running an engine."),
        tr("AFR target table. Confirm sensor calibration and operating conditions."),
        tr("Offline base maps. Illustrative examples are not calibrated engine tunes."),
        tr("Datalogging. Runtime channels and capture status."),
        tr("Firmware management. Check board identity before flashing."),
        tr("Calibration and setup. Configure sensors and engine parameters.")
    };
    connect(tabs, &QTabWidget::currentChanged, inspectorDescription, [inspectorTitle, inspectorDescription, inspectorNotes, tabs](int index) {
        inspectorTitle->setText(tabs->tabText(index));
        inspectorDescription->setText(index >= 0 && index < inspectorNotes.size() ? inspectorNotes.at(index) : QString());
    });
    inspectorDescription->setText(inspectorNotes.first());

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
    // AFR Target is an editable OFFLINE template only. Firmware page storage is not
    // implemented for this table, so it cannot be written to the ECU.
    auto *afrPage = new QWidget(tabs);
    auto *afrLayout = new QVBoxLayout(afrPage);
    auto *afrTitle = new QLabel(tr("AFR TARGET  |  Generic reference  |  16 x 16"), afrPage);
    afrTitle->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11pt; color: #e69a32;"));
    afrLayout->addWidget(afrTitle);
    auto *afrWarning = new QLabel(tr("OFFLINE REFERENCE ONLY. Generic targets are not a validated engine tune and are never sent to the ECU."), afrPage);
    afrWarning->setWordWrap(true);
    afrWarning->setStyleSheet(QStringLiteral("color: #e6a750;"));
    afrLayout->addWidget(afrWarning);
    auto *afrMap = new QTableWidget(16, 16, afrPage);
    afrMap->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    afrMap->horizontalHeader()->setDefaultSectionSize(62);
    afrMap->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    afrMap->verticalHeader()->setDefaultSectionSize(28);
    afrMap->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    afrMap->setAlternatingRowColors(true);
    const int rpmAxis[16] = {500,750,1000,1250,1500,1750,2000,2500,3000,3500,4000,4500,5000,6000,7000,8000};
    const int loadAxis[16] = {20,30,40,50,60,70,80,90,100,110,120,130,140,160,180,200};
    QStringList rpmHeaders, loadHeaders;
    for (int i = 0; i < 16; ++i) {
        rpmHeaders.append(QString::number(rpmAxis[i]));
        loadHeaders.append(QString::number(loadAxis[i]));
    }
    afrMap->setHorizontalHeaderLabels(rpmHeaders);
    afrMap->setVerticalHeaderLabels(loadHeaders);
    for (int row = 0; row < 16; ++row) {
        for (int col = 0; col < 16; ++col) {
            // Stoichiometric example at low load; richer illustrative targets
            // under higher load. Never apply these values automatically.
            const double afr = loadAxis[row] <= 80 ? 14.7
                : loadAxis[row] <= 100 ? 13.8 : loadAxis[row] <= 140 ? 12.5 : 12.0;
            auto *cell = new QTableWidgetItem(QString::number(afr, 'f', 2));
            cell->setToolTip(tr("Generic gasoline AFR reference only; requires engine-specific validation."));
            afrMap->setItem(row, col, cell);
        }
    }
    afrLayout->addWidget(afrMap, 1);
    auto *afrFoot = new QLabel(tr("RPM (columns) / MAP kPa (rows). AFR assumes gasoline stoichiometric 14.7. No ECU AFR table protocol exists yet."), afrPage);
    afrFoot->setWordWrap(true);
    afrLayout->addWidget(afrFoot);
    tabs->addTab(afrPage, tr("AFR Target"));

    // User-selected external tune preview. No network access or ECU write path.
    auto *importPage = new QWidget(tabs);
    auto *importLayout = new QVBoxLayout(importPage);
    auto *importTitle = new QLabel(tr("BASE MAPS  |  Tunuino offline examples"), importPage);
    importTitle->setStyleSheet(QStringLiteral("font-weight: 700; color: #e69a32;"));
    importLayout->addWidget(importTitle);
    auto *importNotice = new QLabel(tr("Original example maps are offline only. Values are not validated and cannot be flashed."), importPage);
    importNotice->setWordWrap(true);
    importLayout->addWidget(importNotice);
    auto *presetBar = new QHBoxLayout();
    auto *presetLabel = new QLabel(tr("Original example profile"), importPage);
    auto *presetSelect = new QComboBox(importPage);
    presetSelect->addItem(tr("Naturally aspirated - illustrative"), 0);
    presetSelect->addItem(tr("Turbo - illustrative"), 1);
    auto *presetMap = new QComboBox(importPage);
    presetMap->addItem(tr("Fuel / VE"), 0);
    presetMap->addItem(tr("Ignition advance"), 1);
    presetMap->addItem(tr("AFR Target"), 2);
    auto *presetLoad = new QPushButton(tr("Preview example"), importPage);
    presetBar->addWidget(presetLabel);
    presetBar->addWidget(presetSelect);
    presetBar->addWidget(presetMap);
    presetBar->addWidget(presetLoad);
    importLayout->addLayout(presetBar);
    auto *saveExample = new QPushButton(tr("Save example as JSON"), importPage);
    auto *openExample = new QPushButton(tr("Open JSON"), importPage);
    presetBar->addWidget(openExample);
    presetBar->addWidget(saveExample);
    auto *importTable = new QTableWidget(importPage);
    importTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    importTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    importTable->horizontalHeader()->setDefaultSectionSize(65);
    importTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    importTable->verticalHeader()->setDefaultSectionSize(28);
    importTable->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    importLayout->addWidget(importTable, 1);
    auto *importStatus = new QLabel(tr("No local tune loaded"), importPage);
    auto editableExample = QSharedPointer<bool>::create(false);
    auto loadedExample = QSharedPointer<QJsonObject>::create();
    importStatus->setWordWrap(true);
    importLayout->addWidget(importStatus);
    connect(presetLoad, &QPushButton::clicked, this, [=]() {
        // These are original mathematical examples, not tested engine calibrations.
        // They cannot be sent to an ECU through this page.
        const bool turbo = presetSelect->currentData().toInt() == 1;
        const int kind = presetMap->currentData().toInt();
        const double rpm[16] = {500,750,1000,1250,1500,1750,2000,2500,3000,3500,4000,4500,5000,5500,6000,6500};
        const double mapNA[16] = {20,25,30,35,40,45,50,55,60,65,70,75,80,85,90,100};
        const double mapTurbo[16] = {20,30,40,50,60,70,80,90,100,110,120,130,145,160,180,200};
        const double *load = turbo ? mapTurbo : mapNA;
        *editableExample = true;
        *loadedExample = QJsonObject();
        importTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
        importTable->clear();
        importTable->setRowCount(16);
        importTable->setColumnCount(16);
        QStringList horizontal, vertical;
        for (int c = 0; c < 16; ++c) horizontal.append(QString::number(rpm[c]));
        for (int r = 0; r < 16; ++r) vertical.append(QString::number(load[r]));
        importTable->setHorizontalHeaderLabels(horizontal);
        importTable->setVerticalHeaderLabels(vertical);
        for (int r = 0; r < 16; ++r) {
            for (int c = 0; c < 16; ++c) {
                const double normalizedLoad = load[r] / 100.0;
                const double normalizedRpm = rpm[c] / 6500.0;
                double value = 0.0;
                if (kind == 0)
                    value = 35.0 + 42.0 * normalizedLoad + 9.0 * normalizedRpm;
                else if (kind == 1)
                    value = 12.0 + 18.0 * normalizedRpm - 8.0 * normalizedLoad - (turbo ? 8.0 * qMax(0.0, normalizedLoad - 1.0) : 0.0);
                else
                    value = 14.7 - 2.0 * qMax(0.0, normalizedLoad - 0.55) - (turbo ? 0.8 * qMax(0.0, normalizedLoad - 1.0) : 0.0);
                auto *cell = new QTableWidgetItem(QString::number(value, 'f', 2));
                cell->setToolTip(tr("Illustrative calculated value. Not validated for any engine."));
                importTable->setItem(r, c, cell);
            }
        }
        importStatus->setText(tr("EXAMPLE ONLY — %1 / %2. Columns: RPM. Rows: MAP (kPa). Not a safe tune; do not flash.").arg(presetSelect->currentText(), presetMap->currentText()));
    });
    connect(saveExample, &QPushButton::clicked, this, [=]() {
        if (!*editableExample || importTable->rowCount() != 16 || importTable->columnCount() != 16) {
            QMessageBox::information(this, tr("Base Maps"), tr("Preview an original example before saving."));
            return;
        }
        const QString filename = QFileDialog::getSaveFileName(this, tr("Save offline example"), QStringLiteral("tunuino-example.json"), tr("JSON files (*.json)"));
        if (filename.isEmpty()) return;
        QJsonObject document;
        document.insert(QStringLiteral("format"), QStringLiteral("tunuino-offline-example-v1"));
        document.insert(QStringLiteral("validated"), false);
        document.insert(QStringLiteral("ecuWritable"), false);
        document.insert(QStringLiteral("profile"), loadedExample->isEmpty() ? presetSelect->currentText() : loadedExample->value(QStringLiteral("profile")).toString());
        document.insert(QStringLiteral("map"), loadedExample->isEmpty() ? presetMap->currentText() : loadedExample->value(QStringLiteral("map")).toString());
        document.insert(QStringLiteral("rowAxis"), QStringLiteral("MAP_kPa"));
        document.insert(QStringLiteral("columnAxis"), QStringLiteral("RPM"));
        QJsonArray rpm, pressure, cells;
        for (int c = 0; c < 16; ++c) rpm.append(importTable->horizontalHeaderItem(c)->text().toDouble());
        for (int r = 0; r < 16; ++r) {
            pressure.append(importTable->verticalHeaderItem(r)->text().toDouble());
            QJsonArray row;
            for (int c = 0; c < 16; ++c) {
                const auto *cell = importTable->item(r,c);
                bool ok = false;
                const double value = cell ? cell->text().toDouble(&ok) : 0.0;
                if (!ok || !std::isfinite(value)) {
                    QMessageBox::warning(this, tr("Invalid map"), tr("All 256 cells must contain finite numbers."));
                    return;
                }
                row.append(value);
            }
            cells.append(row);
        }
        document.insert(QStringLiteral("rpm"), rpm);
        document.insert(QStringLiteral("mapKpa"), pressure);
        document.insert(QStringLiteral("values"), cells);
        QSaveFile output(filename);
        if (!output.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(this, tr("Save failed"), output.errorString());
            return;
        }
        const QByteArray payload = QJsonDocument(document).toJson(QJsonDocument::Indented);
        if (output.write(payload) != payload.size() || !output.commit()) {
            QMessageBox::warning(this, tr("Save failed"), output.errorString());
            return;
        }
        importStatus->setText(tr("Saved illustrative offline map to %1. Never flash without calibration.").arg(filename));
    });
    connect(openExample, &QPushButton::clicked, this, [=]() {
        const QString filename = QFileDialog::getOpenFileName(this, tr("Open Tunuino offline map"), QString(), tr("JSON files (*.json)"));
        if (filename.isEmpty()) return;
        QFile file(filename);
        if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) {
            QMessageBox::warning(this, tr("Open failed"), tr("File cannot be read or exceeds 1 MB."));
            return;
        }
        QJsonParseError error;
        const QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &error);
        const QJsonObject obj = json.object();
        const QJsonArray rpm = obj.value(QStringLiteral("rpm")).toArray();
        const QJsonArray pressure = obj.value(QStringLiteral("mapKpa")).toArray();
        const QJsonArray rows = obj.value(QStringLiteral("values")).toArray();
        if (error.error != QJsonParseError::NoError ||
            obj.value(QStringLiteral("format")).toString() != QLatin1String("tunuino-offline-example-v1") ||
            rpm.size() != 16 || pressure.size() != 16 || rows.size() != 16 ||
            obj.value(QStringLiteral("validated")).toBool(true) ||
            obj.value(QStringLiteral("ecuWritable")).toBool(true)) {
            QMessageBox::warning(this, tr("Invalid map"), tr("Not a supported offline example."));
            return;
        }
        QStringList h, v;
        QVector<double> numbers;
        bool valid = true;
        for (int i = 0; i < 16; ++i) {
            if (!rpm[i].isDouble() || !pressure[i].isDouble()) { valid = false; break; }
            h.append(QString::number(rpm[i].toDouble()));
            v.append(QString::number(pressure[i].toDouble()));
            const QJsonArray row = rows[i].toArray();
            if (row.size() != 16) { valid = false; break; }
            for (const auto &item : row) {
                if (!item.isDouble() || !std::isfinite(item.toDouble())) { valid = false; break; }
                numbers.append(item.toDouble());
            }
            if (!valid) break;
        }
        if (!valid || numbers.size() != 256) {
            QMessageBox::warning(this, tr("Invalid map"), tr("Axes and all 256 values must be numeric."));
            return;
        }
        *loadedExample = obj;
        *editableExample = true;
        importTable->clear();
        importTable->setRowCount(16);
        importTable->setColumnCount(16);
        importTable->setHorizontalHeaderLabels(h);
        importTable->setVerticalHeaderLabels(v);
        for (int r = 0; r < 16; ++r)
            for (int c = 0; c < 16; ++c)
                importTable->setItem(r, c, new QTableWidgetItem(QString::number(numbers[r * 16 + c], 'f', 2)));
        importTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
        importStatus->setText(tr("Offline example loaded: %1. Double-click to edit. Not validated; ECU write disabled.").arg(filename));
    });
    tabs->addTab(importPage, tr("Base Maps"));

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
    auto *firmwareInfo = new QLabel(tr("USB DFU device management. Windows bootloader detection is available without external tools. Direct flashing is under development and remains disabled."), firmwarePage);
    firmwareInfo->setWordWrap(true);
    firmwareLayout->addWidget(firmwareInfo);
    auto *dfuForm = new QFormLayout();
    auto *imagePath = new QLineEdit(firmwarePage);
    imagePath->setReadOnly(true);
    auto *browseImage = new QPushButton(tr("Select firmware .bin..."), firmwarePage);
    auto *imageRow = new QWidget(firmwarePage);
    auto *imageRowLayout = new QHBoxLayout(imageRow);
    imageRowLayout->setContentsMargins(0, 0, 0, 0);
    imageRowLayout->addWidget(imagePath, 1);
    imageRowLayout->addWidget(browseImage);
    dfuForm->addRow(tr("Firmware image"), imageRow);
    auto *dfuPort = new QComboBox(firmwarePage);
    dfuPort->setEditable(false);
    dfuForm->addRow(tr("USB DFU device"), dfuPort);
    firmwareLayout->addLayout(dfuForm);
    auto *dfuButtons = new QHBoxLayout();
    auto *detectDfu = new QPushButton(tr("Detect USB DFU"), firmwarePage);
    auto *flashDfu = new QPushButton(tr("Flash and verify (not available yet)"), firmwarePage);
    flashDfu->setEnabled(false);
    auto *inspectBin = new QPushButton(tr("Validate firmware .bin"), firmwarePage);
    dfuButtons->addWidget(detectDfu);
    dfuButtons->addWidget(inspectBin);
    auto *prepareFlash = new QPushButton(tr("Prepare flash (no write)"), firmwarePage);
    dfuButtons->addWidget(prepareFlash);
    dfuButtons->addWidget(flashDfu);
    dfuButtons->addStretch();
    firmwareLayout->addLayout(dfuButtons);
    auto *dfuStatus = new QLabel(tr("Select Detect USB DFU. Flashing is disabled until the native DFU protocol is validated."), firmwarePage);
    dfuStatus->setWordWrap(true);
    firmwareLayout->addWidget(dfuStatus);
    auto *dfuLog = new QPlainTextEdit(firmwarePage);
    dfuLog->setReadOnly(true);
    dfuLog->setMaximumBlockCount(500);
    firmwareLayout->addWidget(dfuLog, 1);
    connect(browseImage, &QPushButton::clicked, firmwarePage, [imagePath, firmwarePage]() {
        const QString file = QFileDialog::getOpenFileName(firmwarePage, QObject::tr("Select ECU firmware"), QString(), QObject::tr("Binary firmware (*.bin)"));
        if (!file.isEmpty()) imagePath->setText(file);
    });
    connect(prepareFlash, &QPushButton::clicked, firmwarePage, [=]() {
        QFile file(imagePath->text());
        if (!file.open(QIODevice::ReadOnly)) {
            dfuLog->appendPlainText(tr("Flash preparation blocked: select a firmware .bin first."));
            return;
        }
        const QByteArray bytes = file.readAll();
        if (bytes.size() < 8 || bytes.size() > 1024 * 1024) {
            dfuLog->appendPlainText(tr("Flash preparation blocked: invalid firmware size."));
            return;
        }
        const auto *raw = reinterpret_cast<const uchar *>(bytes.constData());
        const quint32 stack = qFromLittleEndian<quint32>(raw);
        const quint32 reset = qFromLittleEndian<quint32>(raw + 4);
        if (stack < 0x20000000u || stack >= 0x20030000u || stack % 8u != 0
                || !(reset & 1u) || (reset & ~quint32(1)) < 0x08000000u
                || (reset & ~quint32(1)) >= 0x08000000u + quint32(bytes.size())) {
            dfuLog->appendPlainText(tr("Flash preparation blocked: invalid vector table."));
            return;
        }
        // STM32 1 MiB single-bank sectors: 4x16 KiB, 1x64 KiB, 7x128 KiB.
        const quint32 sectorEnds[] = {16384,32768,49152,65536,131072,
            262144,393216,524288,655360,786432,917504,1048576};
        int sectors = 0;
        while (sectors < 12 && sectorEnds[sectors] < quint32(bytes.size()))
            ++sectors;
        ++sectors;
        dfuLog->appendPlainText(tr("Flash plan ONLY (no write): %1 bytes, sectors 0-%2, start 0x08000000")
            .arg(bytes.size()).arg(sectors - 1));
        const quint32 eraseBytes = sectorEnds[sectors - 1];
        const int transferBytes = 2048;
        const int writeBlocks = (bytes.size() + transferBytes - 1) / transferBytes;
        dfuLog->appendPlainText(tr("Erase footprint: %1 bytes; program blocks: %2 (max %3 bytes each); readback bytes: %4")
            .arg(eraseBytes).arg(writeBlocks).arg(transferBytes).arg(bytes.size()));
        dfuLog->appendPlainText(tr("Erase sector start addresses:"));
        for (int i = 0; i < sectors; ++i) {
            const quint32 offset = i == 0 ? 0 : sectorEnds[i - 1];
            dfuLog->appendPlainText(tr("  Sector %1: 0x%2")
                .arg(i).arg(0x08000000u + offset, 8, 16, QLatin1Char('0')));
        }
        dfuLog->appendPlainText(tr("Safety: save and verify a %1-byte sector backup before any erase.")
            .arg(eraseBytes));
        dfuLog->appendPlainText(tr("Firmware SHA256: %1")
            .arg(QString::fromLatin1(QCryptographicHash::hash(bytes,
                QCryptographicHash::Sha256).toHex())));
        dfuLog->appendPlainText(tr("Flash operation remains DISABLED. This plan does not prove board compatibility or backup completeness."));
    });
    connect(inspectBin, &QPushButton::clicked, firmwarePage, [=]() {
        const QString path = imagePath->text();
        QFile file(path);
        if (path.isEmpty() || !file.open(QIODevice::ReadOnly)) {
            dfuLog->appendPlainText(tr("BIN validation: select an accessible .bin file first."));
            return;
        }
        const qint64 size = file.size();
        if (size < 8 || size > 1024 * 1024) {
            dfuLog->appendPlainText(tr("BIN rejected: size %1 bytes (expected 8 to 1048576).").arg(size));
            return;
        }
        const QByteArray bytes = file.readAll();
        if (bytes.size() != size) {
            dfuLog->appendPlainText(tr("BIN rejected: incomplete file read."));
            return;
        }
        const quint32 stack = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(bytes.constData()));
        const quint32 reset = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(bytes.constData() + 4));
        const bool stackValid = stack >= 0x20000000u && stack < 0x20030000u && (stack % 8u == 0u);
        const bool resetValid = (reset & 1u) && (reset & ~quint32(1)) >= 0x08000000u
            && (reset & ~quint32(1)) < (0x08000000u + quint32(size));
        const QString digest = QString::fromLatin1(QCryptographicHash::hash(bytes,
            QCryptographicHash::Sha256).toHex());
        dfuLog->appendPlainText(tr("BIN preflight: %1 bytes, SHA256 %2").arg(size).arg(digest));
        dfuLog->appendPlainText(tr("Vector table: initial SP=0x%1, reset handler=0x%2")
            .arg(stack, 8, 16, QLatin1Char('0')).arg(reset, 8, 16, QLatin1Char('0')));
        dfuLog->appendPlainText(stackValid && resetValid
            ? tr("BIN vector preflight passed. This does NOT prove ECU compatibility; flashing remains disabled.")
            : tr("BIN vector preflight FAILED. Do not flash this image."));
    });
    connect(detectDfu, &QPushButton::clicked, firmwarePage, [=]() {
        dfuPort->clear();
        flashDfu->setEnabled(false);
        dfuLog->clear();
#ifdef Q_OS_WIN
        // Read-only native Windows enumeration: no external ST tool required.
        HDEVINFO devices = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
        if (devices == INVALID_HANDLE_VALUE) {
            dfuStatus->setText(tr("Windows USB enumeration failed."));
            return;
        }
        SP_DEVINFO_DATA deviceInfo{};
        deviceInfo.cbSize = sizeof(deviceInfo);
        for (DWORD index = 0; SetupDiEnumDeviceInfo(devices, index, &deviceInfo); ++index) {
            wchar_t hardwareIds[4096]{};
            if (!SetupDiGetDeviceRegistryPropertyW(devices, &deviceInfo, SPDRP_HARDWAREID,
                    nullptr, reinterpret_cast<PBYTE>(hardwareIds), sizeof(hardwareIds), nullptr))
                continue;
            const QString hardwareId = QString::fromWCharArray(hardwareIds).toUpper();
            // ST's ROM USB DFU bootloader has VID 0483 and PID DF11.
            if (!hardwareId.contains(QStringLiteral("VID_0483&PID_DF11"))) continue;
            wchar_t description[512]{};
            SetupDiGetDeviceRegistryPropertyW(devices, &deviceInfo, SPDRP_DEVICEDESC,
                nullptr, reinterpret_cast<PBYTE>(description), sizeof(description), nullptr);
            const QString name = QString::fromWCharArray(description);
            wchar_t service[512]{};
            const bool hasService = SetupDiGetDeviceRegistryPropertyW(devices, &deviceInfo,
                SPDRP_SERVICE, nullptr, reinterpret_cast<PBYTE>(service), sizeof(service), nullptr) != FALSE;
            wchar_t manufacturer[512]{};
            const bool hasManufacturer = SetupDiGetDeviceRegistryPropertyW(devices, &deviceInfo,
                SPDRP_MFG, nullptr, reinterpret_cast<PBYTE>(manufacturer), sizeof(manufacturer), nullptr) != FALSE;
            wchar_t instanceId[1024]{};
            const bool hasInstanceId = SetupDiGetDeviceInstanceIdW(devices, &deviceInfo,
                instanceId, DWORD(sizeof(instanceId) / sizeof(instanceId[0])), nullptr) != FALSE;
            const QString driverService = hasService ? QString::fromWCharArray(service) : tr("(unknown)");
            const QString driverManufacturer = hasManufacturer ? QString::fromWCharArray(manufacturer) : tr("(unknown)");
            const QString deviceInstance = hasInstanceId ? QString::fromWCharArray(instanceId) : hardwareId;
            dfuPort->addItem(QStringLiteral("STM32 BOOTLOADER — %1").arg(name), deviceInstance);
            dfuLog->appendPlainText(tr("Windows detected: %1 (%2)").arg(name, hardwareId));
            dfuLog->appendPlainText(tr("Driver service: %1").arg(driverService));
            dfuLog->appendPlainText(tr("Driver manufacturer: %1").arg(driverManufacturer));
            dfuLog->appendPlainText(tr("Device instance: %1").arg(deviceInstance));
            // Locate the WinUSB device interface associated with this exact PnP
            // instance. SetupDi device presence alone is not a usable USB handle.
            const GUID winUsbInterface = {0xA5DCBF10, 0x6530, 0x11D2,
                {0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED}};
            HDEVINFO interfaces = SetupDiGetClassDevsW(&winUsbInterface, nullptr, nullptr,
                DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
            bool interfaceFound = false;
            if (interfaces != INVALID_HANDLE_VALUE) {
                SP_DEVICE_INTERFACE_DATA iface{};
                iface.cbSize = sizeof(iface);
                for (DWORD n = 0; SetupDiEnumDeviceInterfaces(interfaces, nullptr,
                        &winUsbInterface, n, &iface); ++n) {
                    DWORD needed = 0;
                    SetupDiGetDeviceInterfaceDetailW(interfaces, &iface, nullptr, 0, &needed, nullptr);
                    if (!needed) continue;
                    QByteArray detailStorage(int(needed), char(0));
                    auto *detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(detailStorage.data());
                    detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
                    SP_DEVINFO_DATA linked{};
                    linked.cbSize = sizeof(linked);
                    if (!SetupDiGetDeviceInterfaceDetailW(interfaces, &iface, detail, needed, nullptr, &linked))
                        continue;
                    wchar_t linkedId[1024]{};
                    if (!SetupDiGetDeviceInstanceIdW(interfaces, &linked, linkedId, 1024, nullptr))
                        continue;
                    if (QString::fromWCharArray(linkedId).compare(deviceInstance, Qt::CaseInsensitive) != 0)
                        continue;
                    interfaceFound = true;
                    HANDLE handle = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
                    if (handle == INVALID_HANDLE_VALUE) {
                        dfuLog->appendPlainText(tr("WinUSB interface found; opening failed (Windows error %1).")
                            .arg(GetLastError()));
                        break;
                    }
                    WINUSB_INTERFACE_HANDLE usb = nullptr;
                    if (!WinUsb_Initialize(handle, &usb)) {
                        dfuLog->appendPlainText(tr("WinUSB initialization failed (Windows error %1).")
                            .arg(GetLastError()));
                    } else {
                        USB_DEVICE_DESCRIPTOR descriptor{};
                        ULONG transferred = 0;
                        if (WinUsb_GetDescriptor(usb, USB_DEVICE_DESCRIPTOR_TYPE, 0, 0,
                                reinterpret_cast<PUCHAR>(&descriptor), sizeof(descriptor), &transferred)
                                && transferred == sizeof(descriptor)) {
                            dfuLog->appendPlainText(tr("WinUSB open OK: VID %1, PID %2, USB %3.%4")
                                .arg(descriptor.idVendor, 4, 16, QLatin1Char('0'))
                                .arg(descriptor.idProduct, 4, 16, QLatin1Char('0'))
                                .arg((descriptor.bcdUSB >> 8) & 0xff)
                                .arg((descriptor.bcdUSB >> 4) & 0xf));
                        } else {
                            dfuLog->appendPlainText(tr("WinUSB opened; device descriptor read failed (Windows error %1).")
                                .arg(GetLastError()));
                        }
                        // Inspect DFU interface alternate settings and memory layout
                        // strings. These standard descriptor requests are read-only.
                        USB_INTERFACE_DESCRIPTOR ifaceDescriptor{};
                        if (WinUsb_QueryInterfaceSettings(usb, 0, &ifaceDescriptor)) {
                            dfuLog->appendPlainText(tr("USB interface: class=%1, subclass=%2, protocol=%3, alternate=%4")
                                .arg(unsigned(ifaceDescriptor.bInterfaceClass))
                                .arg(unsigned(ifaceDescriptor.bInterfaceSubClass))
                                .arg(unsigned(ifaceDescriptor.bInterfaceProtocol))
                                .arg(unsigned(ifaceDescriptor.bAlternateSetting)));
                            if (ifaceDescriptor.bInterfaceClass != 0xFE ||
                                    ifaceDescriptor.bInterfaceSubClass != 0x01) {
                                dfuLog->appendPlainText(tr("Warning: selected USB interface is not DFU class/subclass."));
                            }
                            if (ifaceDescriptor.iInterface != 0) {
                                UCHAR stringBytes[256]{};
                                ULONG stringLength = 0;
                                if (WinUsb_GetDescriptor(usb, USB_STRING_DESCRIPTOR_TYPE,
                                        ifaceDescriptor.iInterface, 0x0409, stringBytes,
                                        sizeof(stringBytes), &stringLength)
                                        && stringLength >= 2 && stringBytes[1] == USB_STRING_DESCRIPTOR_TYPE) {
                                    const int chars = qMin(int(stringBytes[0]), int(stringLength)) / 2 - 1;
                                    QString layout;
                                    for (int j = 0; j < chars; ++j) {
                                        const unsigned code = unsigned(stringBytes[2 + 2*j])
                                            | (unsigned(stringBytes[3 + 2*j]) << 8);
                                        layout.append(QChar(ushort(code)));
                                    }
                                    dfuLog->appendPlainText(tr("DFU interface memory descriptor: %1").arg(layout));
                                } else {
                                    dfuLog->appendPlainText(tr("DFU interface string descriptor unavailable (Windows error %1).")
                                        .arg(GetLastError()));
                                }
                            }
                        } else {
                            dfuLog->appendPlainText(tr("USB interface descriptor unavailable (Windows error %1).")
                                .arg(GetLastError()));
                        }
                        // Parse the DFU functional descriptor from the configuration
                        // descriptor before allowing any future upload implementation.
                        // This is a standard USB descriptor read, not a memory read.
                        UCHAR configBytes[1024]{};
                        ULONG configLength = 0;
                        if (WinUsb_GetDescriptor(usb, USB_CONFIGURATION_DESCRIPTOR_TYPE, 0, 0,
                                configBytes, sizeof(configBytes), &configLength)
                                && configLength >= 9) {
                            bool foundDfu = false;
                            for (ULONG offset = 0; offset + 2 <= configLength;) {
                                const UCHAR len = configBytes[offset];
                                if (len < 2 || offset + len > configLength) break;
                                if (configBytes[offset + 1] == 0x21 && len >= 9) {
                                    const unsigned attributes = configBytes[offset + 2];
                                    const unsigned transferSize = unsigned(configBytes[offset + 5])
                                        | (unsigned(configBytes[offset + 6]) << 8);
                                    const unsigned version = unsigned(configBytes[offset + 7])
                                        | (unsigned(configBytes[offset + 8]) << 8);
                                    dfuLog->appendPlainText(tr("DFU capabilities: upload=%1, download=%2, transfer=%3 bytes, DFU version=0x%4")
                                        .arg((attributes & 0x02) ? tr("yes") : tr("no"))
                                        .arg((attributes & 0x01) ? tr("yes") : tr("no"))
                                        .arg(transferSize)
                                        .arg(version, 4, 16, QLatin1Char('0')));
                                    foundDfu = true;
                                    break;
                                }
                                offset += len;
                            }
                            if (!foundDfu)
                                dfuLog->appendPlainText(tr("DFU functional descriptor not found; memory reading not enabled."));
                        } else {
                            dfuLog->appendPlainText(tr("USB configuration descriptor unavailable; memory reading not enabled."));
                        }
                        // DFU_GETSTATE (0x05) and DFU_GETSTATUS (0x03) are
                        // read-only USB class requests. Never issue DNLOAD here.
                        UCHAR state = 0xff;
                        ULONG received = 0;
                        WINUSB_SETUP_PACKET stateRequest{};
                        stateRequest.RequestType = 0xA1;
                        stateRequest.Request = 0x05;
                        stateRequest.Value = 0;
                        stateRequest.Index = 0;
                        stateRequest.Length = 1;
                        if (WinUsb_ControlTransfer(usb, stateRequest, &state, 1, &received, nullptr)
                                && received == 1) {
                            dfuLog->appendPlainText(tr("DFU GETSTATE OK: state=%1").arg(unsigned(state)));
                        } else {
                            dfuLog->appendPlainText(tr("DFU GETSTATE failed (Windows error %1).")
                                .arg(GetLastError()));
                        }
                        UCHAR status[6]{};
                        received = 0;
                        WINUSB_SETUP_PACKET statusRequest{};
                        statusRequest.RequestType = 0xA1;
                        statusRequest.Request = 0x03;
                        statusRequest.Value = 0;
                        statusRequest.Index = 0;
                        statusRequest.Length = 6;
                        if (WinUsb_ControlTransfer(usb, statusRequest, status, 6, &received, nullptr)
                                && received == 6) {
                            const unsigned pollMs = unsigned(status[1]) | (unsigned(status[2]) << 8)
                                | (unsigned(status[3]) << 16);
                            dfuLog->appendPlainText(tr("DFU GETSTATUS OK: status=%1, poll=%2 ms, state=%3, iString=%4")
                                .arg(unsigned(status[0])).arg(pollMs)
                                .arg(unsigned(status[4])).arg(unsigned(status[5])));
                        } else {
                            dfuLog->appendPlainText(tr("DFU GETSTATUS failed (Windows error %1).")
                                .arg(GetLastError()));
                        }
                        // DFU_CLRSTATUS is only valid in dfuERROR (state 10).
                        // Never send it on a healthy or unidentified interface.
                        if (received == 6 && status[4] == 10) {
                            WINUSB_SETUP_PACKET clearRequest{};
                            clearRequest.RequestType = 0x21;
                            clearRequest.Request = 0x04;
                            clearRequest.Value = 0;
                            clearRequest.Index = 0;
                            clearRequest.Length = 0;
                            ULONG cleared = 0;
                            UCHAR unused = 0;
                            if (WinUsb_ControlTransfer(usb, clearRequest, &unused, 0, &cleared, nullptr)) {
                                dfuLog->appendPlainText(tr("DFU CLRSTATUS sent (no flash erase/write)."));
                                UCHAR after[6]{};
                                ULONG afterLength = 0;
                                if (WinUsb_ControlTransfer(usb, statusRequest, after, 6, &afterLength, nullptr)
                                        && afterLength == 6) {
                                    dfuLog->appendPlainText(tr("DFU after CLRSTATUS: status=%1, state=%2")
                                        .arg(unsigned(after[0])).arg(unsigned(after[4])));
                                } else {
                                    dfuLog->appendPlainText(tr("DFU status after CLRSTATUS failed (Windows error %1).")
                                        .arg(GetLastError()));
                                }
                            } else {
                                dfuLog->appendPlainText(tr("DFU CLRSTATUS failed (Windows error %1).")
                                    .arg(GetLastError()));
                            }
                        }
                        // STM32 DfuSe: set address pointer with DNLOAD block 0.
                        // This command changes the bootloader read cursor only;
                        // it does not erase or program flash. Read exactly 256 bytes.
                        UCHAR ready[6]{};
                        ULONG readyLength = 0;
                        if (WinUsb_ControlTransfer(usb, statusRequest, ready, 6, &readyLength, nullptr)
                                && readyLength == 6 && ready[0] == 0 && ready[4] == 2) {
                            WINUSB_SETUP_PACKET addressRequest{};
                            addressRequest.RequestType = 0x21;
                            addressRequest.Request = 0x01; // DFU_DNLOAD
                            addressRequest.Value = 0;
                            addressRequest.Index = 0;
                            addressRequest.Length = 5;
                            UCHAR addressCommand[5] = {0x21, 0x00, 0x00, 0x00, 0x08};
                            ULONG sent = 0;
                            if (WinUsb_ControlTransfer(usb, addressRequest, addressCommand, 5, &sent, nullptr)
                                    && sent == 5) {
                                UCHAR pointerStatus[6]{};
                                ULONG pointerLength = 0;
                                if (WinUsb_ControlTransfer(usb, statusRequest, pointerStatus, 6,
                                        &pointerLength, nullptr) && pointerLength == 6) {
                                    const unsigned poll = unsigned(pointerStatus[1])
                                        | (unsigned(pointerStatus[2]) << 8)
                                        | (unsigned(pointerStatus[3]) << 16);
                                    if (poll > 0 && poll <= 1000) Sleep(poll);
                                    UCHAR finalStatus[6]{};
                                    ULONG finalLength = 0;
                                    if (WinUsb_ControlTransfer(usb, statusRequest, finalStatus, 6,
                                            &finalLength, nullptr) && finalLength == 6
                                            && finalStatus[0] == 0 && finalStatus[4] == 5) {
                                        // DNLOAD leaves the device in dfuDNLOAD-IDLE (5).
                                        // DFU_UPLOAD is only legal after DFU_ABORT returns it
                                        // to dfuIDLE (2). The address pointer is retained.
                                        WINUSB_SETUP_PACKET toIdle{};
                                        toIdle.RequestType = 0x21;
                                        toIdle.Request = 0x06; // DFU_ABORT
                                        toIdle.Index = 0;
                                        toIdle.Length = 0;
                                        UCHAR unusedByte = 0;
                                        ULONG abortLength = 0;
                                        const bool aborted = WinUsb_ControlTransfer(usb, toIdle,
                                            &unusedByte, 0, &abortLength, nullptr) != FALSE;
                                        UCHAR idleStatus[6]{};
                                        ULONG idleLength = 0;
                                        const bool idle = aborted &&
                                            WinUsb_ControlTransfer(usb, statusRequest, idleStatus, 6,
                                                &idleLength, nullptr) && idleLength == 6
                                            && idleStatus[0] == 0 && idleStatus[4] == 2;
                                        dfuLog->appendPlainText(tr("DFU before UPLOAD: ABORT=%1, status=%2, state=%3")
                                            .arg(aborted ? tr("OK") : tr("FAILED"))
                                            .arg(unsigned(idleStatus[0])).arg(unsigned(idleStatus[4])));
                                        if (idle) {
                                        // Read only the selected firmware length, never beyond
                                        // the 1 MiB internal Flash descriptor. No erase/write.
                                        QFile reference(imagePath->text());
                                        const bool referenceOpen = !imagePath->text().isEmpty()
                                            && reference.open(QIODevice::ReadOnly);
                                        // Backup the entire erase footprint, not just the
                                        // firmware length: otherwise the last sector's
                                        // trailing bytes would be lost on erase.
                                        const qint64 firmwareSize = referenceOpen ? reference.size() : 256;
                                        const qint64 sectorEnds[] = {16384,32768,49152,65536,
                                            131072,262144,393216,524288,655360,786432,917504,1048576};
                                        qint64 targetSize = firmwareSize;
                                        if (referenceOpen && firmwareSize >= 8 && firmwareSize <= 1048576) {
                                            for (const qint64 end : sectorEnds) {
                                                if (firmwareSize <= end) { targetSize = end; break; }
                                            }
                                        }
                                        if (targetSize < 8 || targetSize > 1024 * 1024) {
                                            dfuLog->appendPlainText(tr("Flash read skipped: invalid selected firmware size."));
                                        } else {
                                            QByteArray flash;
                                            flash.reserve(int(targetSize));
                                            bool complete = true;
                                            const int transferSize = 2048;
                                            for (int offset = 0; offset < targetSize; offset += transferSize) {
                                                const int length = int(qMin(qint64(transferSize), targetSize - offset));
                                                WINUSB_SETUP_PACKET upload{};
                                                upload.RequestType = 0xA1;
                                                upload.Request = 0x02; // DFU_UPLOAD
                                                upload.Value = USHORT(2 + offset / transferSize);
                                                upload.Index = 0;
                                                upload.Length = USHORT(length);
                                                UCHAR chunk[2048]{};
                                                ULONG readLength = 0;
                                                const BOOL ok = WinUsb_ControlTransfer(usb, upload, chunk,
                                                    ULONG(length), &readLength, nullptr);
                                                if (!ok || readLength != ULONG(length)) {
                                                    const DWORD error = ok ? 0 : GetLastError();
                                                    dfuLog->appendPlainText(tr("Flash read FAILED at offset %1: received %2/%3 bytes, Windows error %4")
                                                        .arg(offset).arg(readLength).arg(length).arg(error));
                                                    complete = false;
                                                    break;
                                                }
                                                flash.append(reinterpret_cast<const char *>(chunk), length);
                                            }
                                            if (complete && flash.size() == targetSize) {
                                                dfuLog->appendPlainText(tr("Flash read OK: %1 bytes at 0x08000000; SHA256=%2")
                                                    .arg(flash.size())
                                                    .arg(QString::fromLatin1(QCryptographicHash::hash(
                                                        flash, QCryptographicHash::Sha256).toHex())));
                                                dfuLog->appendPlainText(tr("First 32 bytes: %1")
                                                    .arg(QString::fromLatin1(flash.left(32).toHex(' '))));
                                                if (referenceOpen) {
                                                    const QByteArray expected = reference.readAll();
                                                    if (expected.size() == firmwareSize
                                                            && flash.left(expected.size()) == expected) {
                                                        dfuLog->appendPlainText(tr("Firmware comparison PASS: all %1 firmware bytes match.")
                                                            .arg(expected.size()));
                                                    } else {
                                                        int mismatch = 0;
                                                        while (mismatch < expected.size() && mismatch < flash.size()
                                                            && expected[mismatch] == flash[mismatch]) ++mismatch;
                                                        dfuLog->appendPlainText(tr("Firmware comparison DIFFERENT at offset 0x%1.")
                                                            .arg(mismatch, 8, 16, QLatin1Char('0')));
                                                    }
                                                    dfuLog->appendPlainText(tr("Erase-footprint backup: %1 bytes read, firmware length %2 bytes.")
                                                        .arg(flash.size()).arg(firmwareSize));
                                                }
                                                const QString backupPath = QFileDialog::getSaveFileName(firmwarePage,
                                                    tr("Save ECU Flash backup"), QStringLiteral("ecu-flash-sector-backup.bin"),
                                                    tr("Binary firmware (*.bin)"));
                                                if (!backupPath.isEmpty()) {
                                                    if (QFileInfo(backupPath).absoluteFilePath()
                                                            == QFileInfo(imagePath->text()).absoluteFilePath()) {
                                                        dfuLog->appendPlainText(tr("Backup refused: destination is the selected firmware file."));
                                                    } else {
                                                        QSaveFile backup(backupPath);
                                                        if (backup.open(QIODevice::WriteOnly)
                                                                && backup.write(flash) == flash.size()
                                                                && backup.commit())
                                                            dfuLog->appendPlainText(tr("ECU Flash backup saved: %1").arg(backupPath));
                                                        else
                                                            dfuLog->appendPlainText(tr("ECU Flash backup save FAILED."));
                                                    }
                                                }
                                            }
                                        }
                                        } else {
                                            dfuLog->appendPlainText(tr("Flash read skipped: DFU did not return to idle after address selection."));
                                        }
                                    } else {
                                        dfuLog->appendPlainText(tr("Address pointer not ready: status=%1, state=%2")
                                            .arg(unsigned(finalStatus[0])).arg(unsigned(finalStatus[4])));
                                    }
                                }
                            } else {
                                dfuLog->appendPlainText(tr("DFU set-address failed (Windows error %1)")
                                    .arg(GetLastError()));
                            }
                            // Return the DFU state machine to idle after an upload.
                            WINUSB_SETUP_PACKET abortRequest{};
                            abortRequest.RequestType = 0x21;
                            abortRequest.Request = 0x06; // DFU_ABORT
                            abortRequest.Index = 0;
                            abortRequest.Length = 0;
                            UCHAR dummy = 0;
                            ULONG ignored = 0;
                            WinUsb_ControlTransfer(usb, abortRequest, &dummy, 0, &ignored, nullptr);
                        } else {
                            dfuLog->appendPlainText(tr("Flash read skipped: DFU is not in idle state."));
                        }
                        WinUsb_Free(usb);
                    }
                    CloseHandle(handle);
                    break;
                }
                SetupDiDestroyDeviceInfoList(interfaces);
            }
            if (!interfaceFound)
                dfuLog->appendPlainText(tr("No matching WinUSB interface path found; USB handle not opened."));
            dfuLog->appendPlainText(tr("DFU diagnostics may read Flash. No Flash erase or programming operation was attempted."));
        }
        SetupDiDestroyDeviceInfoList(devices);
        dfuStatus->setText(dfuPort->count() > 0
            ? tr("STM32 USB DFU bootloader detected by Windows. Direct flashing is not yet implemented; flashing remains disabled.")
            : tr("No STM32 DFU bootloader (VID 0483 / PID DF11) found. Check bootloader mode and USB driver."));
#else
        dfuStatus->setText(tr("Native DFU detection is available on Windows only."));
#endif
    });
    // Native Windows discovery is read-only. Never enable the legacy CLI
    // flash handler for a Windows device instance ID: it is not a DFU port.
    connect(flashDfu, &QPushButton::clicked, firmwarePage, [=]() {
        QMessageBox::information(firmwarePage, tr("DFU flashing unavailable"),
            tr("STM32 BOOTLOADER detection is working, but direct DFU communication, "
               "target verification, erase, write and read-back are not yet implemented. "
               "No firmware was written."));
    });
    // Firmware is accessed exclusively from the top menu.
    // Keep its widget as a hidden tab page to preserve the existing DFU controls.
    tabs->addTab(firmwarePage, tr("Firmware"));
    const int firmwareTabIndex = tabs->indexOf(firmwarePage);
    tabs->tabBar()->setTabVisible(firmwareTabIndex, false);
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
                auto *saveTps = new QPushButton(tr("Save offline TPS calibration..."), page);
                form->addRow(saveTps);
                connect(saveTps, &QPushButton::clicked, page, [minVoltage, maxVoltage, page]() {
                    const double closed = minVoltage->value();
                    const double open = maxVoltage->value();
                    if (open - closed < 0.2) {
                        QMessageBox::warning(page, QObject::tr("Invalid TPS calibration"),
                            QObject::tr("Open throttle voltage must exceed closed throttle voltage by at least 0.2 V."));
                        return;
                    }
                    const QString path = QFileDialog::getSaveFileName(page, QObject::tr("Save offline TPS calibration"),
                        QStringLiteral("tps-calibration.json"), QObject::tr("JSON files (*.json)"));
                    if (path.isEmpty()) return;
                    QJsonObject root;
                    root.insert(QStringLiteral("format"), QStringLiteral("tunuino-tps-calibration-v1"));
                    root.insert(QStringLiteral("closed_voltage_v"), closed);
                    root.insert(QStringLiteral("open_voltage_v"), open);
                    root.insert(QStringLiteral("source"), QStringLiteral("manual-reference-not-measured"));
                    QSaveFile file(path);
                    if (!file.open(QIODevice::WriteOnly)) {
                        QMessageBox::warning(page, QObject::tr("Save failed"), file.errorString());
                        return;
                    }
                    const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Indented);
                    if (file.write(data) != data.size() || !file.commit())
                        QMessageBox::warning(page, QObject::tr("Save failed"), file.errorString());
                });
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
                auto *saveStrategy = new QPushButton(tr("Save offline strategy settings..."), page);
                form->addRow(saveStrategy);
                connect(saveStrategy, &QPushButton::clicked, page, [=]() {
                    // Export is a draft only: it never enables an ECU output.
                    if (mode->currentIndex() == 0) {
                        QMessageBox::warning(page, QObject::tr("Strategy not configured"),
                            QObject::tr("Select a strategy mode before saving the offline draft."));
                        return;
                    }
                    if (modeName == QStringLiteral("Launch Control") &&
                        (rpm->value() <= 0 || threshold->value() <= 0 || hysteresis->value() <= 0)) {
                        QMessageBox::warning(page, QObject::tr("Launch limits incomplete"),
                            QObject::tr("Set RPM, minimum TPS and maximum activation duration."));
                        return;
                    }
                    if (modeName != QStringLiteral("Launch Control") && output->currentIndex() == 0) {
                        QMessageBox::warning(page, QObject::tr("Output not selected"),
                            QObject::tr("Select an output for this offline strategy draft."));
                        return;
                    }
                    const QString path = QFileDialog::getSaveFileName(page, QObject::tr("Save offline strategy draft"),
                        QStringLiteral("engine-strategy.json"), QObject::tr("JSON files (*.json)"));
                    if (path.isEmpty()) return;
                    QJsonObject root;
                    root.insert(QStringLiteral("format"), QStringLiteral("tunuino-strategy-draft-v1"));
                    root.insert(QStringLiteral("strategy"), modeName);
                    root.insert(QStringLiteral("mode"), mode->currentText());
                    root.insert(QStringLiteral("rpm_reference"), rpm->value());
                    root.insert(QStringLiteral("threshold_reference"), threshold->value());
                    root.insert(QStringLiteral("secondary_reference"), hysteresis->value());
                    root.insert(QStringLiteral("output_reference"), output->currentText());
                    root.insert(QStringLiteral("ecu_activation_enabled"), false);
                    root.insert(QStringLiteral("offline_only"), true);
                    QSaveFile file(path);
                    if (!file.open(QIODevice::WriteOnly)) {
                        QMessageBox::warning(page, QObject::tr("Save failed"), file.errorString());
                        return;
                    }
                    const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Indented);
                    if (file.write(data) != data.size() || !file.commit())
                        QMessageBox::warning(page, QObject::tr("Save failed"), file.errorString());
                });
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
                    auto *wheel = new QComboBox(page);
                    wheel->addItems({tr("Select wheel pattern"), "60-2", "36-1", "36-2", "24-1", "12-1", "12-0", tr("Custom")});
                    auto *teeth = new QSpinBox(page);
                    teeth->setRange(1, 360);
                    teeth->setValue(60);
                    auto *missing = new QSpinBox(page);
                    missing->setRange(0, 12);
                    missing->setValue(2);
                    auto *triggerAngle = new QDoubleSpinBox(page);
                    triggerAngle->setRange(-360.0, 360.0);
                    triggerAngle->setDecimals(1);
                    triggerAngle->setSuffix(QStringLiteral(" deg"));
                    auto *polarity = new QComboBox(page);
                    polarity->addItems({tr("Not selected"), tr("Rising edge"), tr("Falling edge")});
                    form->addRow(tr("Trigger wheel"), wheel);
                    form->addRow(tr("Nominal teeth"), teeth);
                    form->addRow(tr("Missing teeth"), missing);
                    form->addRow(tr("Trigger angle (unverified)"), triggerAngle);
                    form->addRow(tr("Trigger edge"), polarity);
                    const auto updateWheel = [=](int index) {
                        const bool custom = index == wheel->count() - 1;
                        teeth->setEnabled(custom);
                        missing->setEnabled(custom);
                        if (!custom) {
                            const QStringList parts = wheel->currentText().split(QLatin1Char('-'));
                            if (parts.size() == 2) {
                                teeth->setValue(parts.at(0).toInt());
                                missing->setValue(parts.at(1).toInt());
                            }
                        }
                    };
                    connect(wheel, QOverload<int>::of(&QComboBox::currentIndexChanged), page, updateWheel);
                    updateWheel(wheel->currentIndex());
                    auto *saveWheel = new QPushButton(tr("Save offline trigger setup..."), page);
                    form->addRow(saveWheel);
                    connect(saveWheel, &QPushButton::clicked, page, [=]() {
                        if (wheel->currentIndex() == 0 || polarity->currentIndex() == 0 ||
                            missing->value() >= teeth->value()) {
                            QMessageBox::warning(page, QObject::tr("Incomplete trigger setup"),
                                QObject::tr("Select a wheel and trigger edge; missing teeth must be fewer than nominal teeth."));
                            return;
                        }
                        const QString path = QFileDialog::getSaveFileName(page, QObject::tr("Save offline trigger setup"),
                            QStringLiteral("trigger-setup.json"), QObject::tr("JSON files (*.json)"));
                        if (path.isEmpty()) return;
                        QJsonObject root;
                        root.insert(QStringLiteral("format"), QStringLiteral("tunuino-trigger-v1"));
                        root.insert(QStringLiteral("wheel"), wheel->currentText());
                        root.insert(QStringLiteral("nominal_teeth"), teeth->value());
                        root.insert(QStringLiteral("missing_teeth"), missing->value());
                        root.insert(QStringLiteral("trigger_angle_deg_unverified"), triggerAngle->value());
                        root.insert(QStringLiteral("trigger_edge"), polarity->currentText());
                        root.insert(QStringLiteral("offline_only"), true);
                        QSaveFile file(path);
                        if (!file.open(QIODevice::WriteOnly)) {
                            QMessageBox::warning(page, QObject::tr("Save failed"), file.errorString());
                            return;
                        }
                        const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Indented);
                        if (file.write(data) != data.size() || !file.commit())
                            QMessageBox::warning(page, QObject::tr("Save failed"), file.errorString());
                    });
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
                auto *note = new QLabel(tr("Enter verified injector manufacturer dead-time values in milliseconds. Zero means not configured. Offline only; nothing is sent to the ECU."), page);
                note->setWordWrap(true);
                form->addRow(note);
                auto *table = new QTableWidget(5, 2, page);
                table->setHorizontalHeaderLabels({tr("Battery voltage (V)"), tr("Dead Time (ms)")});
                const QStringList volts = {"8", "10", "12", "14", "16"};
                for (int i = 0; i < volts.size(); ++i) {
                    auto *voltage = new QTableWidgetItem(volts[i]);
                    voltage->setFlags(voltage->flags() & ~Qt::ItemIsEditable);
                    table->setItem(i, 0, voltage);
                    auto *deadTime = new QDoubleSpinBox(table);
                    deadTime->setRange(0.0, 10.0);
                    deadTime->setDecimals(3);
                    deadTime->setSingleStep(0.025);
                    deadTime->setSuffix(QStringLiteral(" ms"));
                    deadTime->setSpecialValueText(tr("Not configured"));
                    deadTime->setValue(0.0);
                    table->setCellWidget(i, 1, deadTime);
                }
                table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
                table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
                table->setMinimumHeight(220);
                form->addRow(table);
                auto *save = new QPushButton(tr("Save offline dead-time table..."), page);
                form->addRow(save);
                connect(save, &QPushButton::clicked, page, [table, page]() {
                    QJsonArray values;
                    for (int i = 0; i < table->rowCount(); ++i) {
                        const auto *spin = qobject_cast<QDoubleSpinBox *>(table->cellWidget(i, 1));
                        if (!spin || spin->value() <= 0.0) {
                            QMessageBox::warning(page, QObject::tr("Incomplete dead-time table"),
                                QObject::tr("Enter a verified nonzero dead time for every battery-voltage row before saving."));
                            return;
                        }
                        QJsonObject entry;
                        entry.insert(QStringLiteral("voltage_v"), table->item(i, 0)->text().toInt());
                        entry.insert(QStringLiteral("dead_time_ms"), spin->value());
                        values.append(entry);
                    }
                    const QString path = QFileDialog::getSaveFileName(page, QObject::tr("Save offline dead-time table"),
                        QStringLiteral("injector-dead-time.json"), QObject::tr("JSON files (*.json)"));
                    if (path.isEmpty()) return;
                    QSaveFile file(path);
                    if (!file.open(QIODevice::WriteOnly)) {
                        QMessageBox::warning(page, QObject::tr("Save failed"), file.errorString());
                        return;
                    }
                    QJsonObject root;
                    root.insert(QStringLiteral("format"), QStringLiteral("tunuino-injector-dead-time-v1"));
                    root.insert(QStringLiteral("units"), QStringLiteral("ms"));
                    root.insert(QStringLiteral("values"), values);
                    const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Indented);
                    if (file.write(data) != data.size() || !file.commit())
                        QMessageBox::warning(page, QObject::tr("Save failed"), file.errorString());
                });
                auto *apply = new QPushButton(tr("Write to ECU (protocol not validated)"), page);
                apply->setEnabled(false);
                form->addRow(apply);
            }
            pages->addWidget(page);
            item->setData(0, Qt::UserRole, pages->count() - 1);
        }
        parentItem->setExpanded(true);
    }
    // Route the main motorsport navigation directly to the corresponding real
    // configuration panel instead of opening an unrelated default panel.
    const QMap<QString, QString> mainNavigationTargets = {
        {tr("TPS Calibration"), tr("TPS")},
        {tr("MAP Calibration"), tr("MAP")},
        {tr("Temperature sensors"), tr("CLT")},
        {tr("Lambda / O2"), tr("Lambda")},
        {tr("Injector Dead Time"), tr("Injector Dead Time")},
        {tr("Trigger setup"), tr("Trigger")},
        {tr("Idle control"), tr("Idle Control")},
        {tr("Boost control"), tr("Boost Control")},
        {tr("Electronic throttle"), tr("ETB / Drive by Wire")},
        {tr("Output configuration"), tr("Outputs")},
        {tr("Communication settings"), tr("Inputs")}
    };
    connect(navigationTree, &QTreeWidget::itemClicked, calibrationPage,
            [navigation, mainNavigationTargets](QTreeWidgetItem *selected, int) {
        if (!selected || !selected->parent()) return;
        const QString target = mainNavigationTargets.value(selected->text(0));
        if (target.isEmpty()) return;
        for (int i = 0; i < navigation->topLevelItemCount(); ++i) {
            auto *category = navigation->topLevelItem(i);
            for (int j = 0; j < category->childCount(); ++j) {
                auto *child = category->child(j);
                if (child->text(0) == target) {
                    navigation->setCurrentItem(child);
                    return;
                }
            }
        }
    });
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
    auto liveReadings = QSharedPointer<QVector<QLabel *>>::create();
    for (const char *parameter : {"RPM", "TPS", "MAP", "Lambda", "CLT", "IAT", "Battery"}) {
        auto *value = new QLabel(tr("No telemetry"), statusPanel);
        value->setStyleSheet(QStringLiteral("color: #a9aaad;"));
        readings->addRow(QString::fromLatin1(parameter), value);
        liveReadings->append(value);
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
    struct MapTransfer {
        int page = 0;
        int offset = 0;
        QByteArray bytes;
    };
    auto transfer = QSharedPointer<MapTransfer>::create();
    auto sendChunk = [session, transfer]() -> bool {
        if (!transfer->page || transfer->offset >= 576) return false;
        const int count = qMin(64, 576 - transfer->offset);
        QByteArray command(7, char(0));
        command[0] = 'r';
        command[1] = char(0);
        command[2] = char(transfer->page);
        command[3] = char((transfer->offset >> 8) & 0xff);
        command[4] = char(transfer->offset & 0xff);
        command[5] = char((count >> 8) & 0xff);
        command[6] = char(count & 0xff);
        return session->exchange(command, 3000);
    };
    auto abortTransfer = [=](const QString &message) {
        const int page = transfer->page;
        transfer->page = 0;
        transfer->offset = 0;
        transfer->bytes.clear();
        fuelRead->setEnabled(*identityVerified && session->isOpen());
        ignitionRead->setEnabled(*identityVerified && session->isOpen());
        if (page == 2) fuelState->setText(message);
        statusBar()->showMessage(message, 10000);
        liveTimer->start();
    };
    auto startTransfer = [=](int page) {
        if (!*identityVerified || !session->isOpen() || transfer->page) return;
        liveTimer->stop();
        transfer->page = page;
        transfer->offset = 0;
        transfer->bytes.clear();
        fuelRead->setEnabled(false);
        ignitionRead->setEnabled(false);
        if (page == 2) fuelState->setText(tr("READING ECU MAP..."));
        if (!sendChunk()) abortTransfer(tr("Unable to request map page"));
    };
    connect(fuelRead, &QPushButton::clicked, this, [=]() { startTransfer(2); });
    connect(ignitionRead, &QPushButton::clicked, this, [=]() { startTransfer(3); });
    auto *fileMenu = menuBar()->addMenu(tr("File"));
    auto *openProject = fileMenu->addAction(tr("Open offline map..."));
    openProject->setShortcut(QKeySequence::Open);
    connect(openProject, &QAction::triggered, openExample, &QPushButton::click);
    auto *saveProject = fileMenu->addAction(tr("Save offline map as..."));
    saveProject->setShortcut(QKeySequence::Save);
    connect(saveProject, &QAction::triggered, saveExample, &QPushButton::click);
    fileMenu->addSeparator();
    auto *fullScreen = fileMenu->addAction(tr("Full screen"));
    fullScreen->setShortcut(QKeySequence(QStringLiteral("Ctrl+F")));
    fullScreen->setCheckable(true);
    connect(fullScreen, &QAction::toggled, this, [this](bool enabled) {
        if (enabled) showFullScreen(); else showNormal();
    });
    fileMenu->addSeparator();
    auto *exitAction = fileMenu->addAction(tr("Exit"), this, &QWidget::close);
    exitAction->setShortcut(QKeySequence(QStringLiteral("Alt+X")));
    auto *editMenu = menuBar()->addMenu(tr("Edit"));
    auto *editInfo = editMenu->addAction(tr("Edit offline base map"));
    connect(editInfo, &QAction::triggered, this, [tabs]() { tabs->setCurrentIndex(3); });
    auto *viewMenu = menuBar()->addMenu(tr("View"));
    viewMenu->addAction(navigationDock->toggleViewAction());
    viewMenu->addAction(inspectorDock->toggleViewAction());
    auto *toolsMenu = menuBar()->addMenu(tr("Tools"));
    auto *validationAction = toolsMenu->addAction(tr("Check ECU configuration (read-only)"));
    validationAction->setEnabled(false);
    auto validationPending = QSharedPointer<bool>::create(false);
    connect(validationAction, &QAction::triggered, this, [=]() {
        if (!*identityVerified || !session->isOpen() || transfer->page || *validationPending) return;
        liveTimer->stop();
        *validationPending = true;
        if (!session->exchange(QByteArray("TUNUINO:V"), 3000)) {
            *validationPending = false;
            liveTimer->start();
        }
    });
    auto *calibrationAction = toolsMenu->addAction(tr("Calibration and setup"));
    connect(calibrationAction, &QAction::triggered, this, [tabs]() { tabs->setCurrentIndex(6); });
    auto *helpMenu = menuBar()->addMenu(tr("Help"));
    auto *aboutAction = helpMenu->addAction(tr("About Tunuino Console"));
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, tr("About Tunuino Console"),
            tr("Tunuino Console — Qt 6\\nOffline map editing and ECU diagnostics.\\nFirmware writing and unverified ECU configuration operations are disabled."));
    });
    auto *ecuMenu = menuBar()->addMenu(tr("ECU"));
    auto *connectMenu = ecuMenu->addMenu(tr("Connect to ECU"));
    auto *disconnectAction = ecuMenu->addAction(tr("Disconnect"));
    disconnectAction->setEnabled(false);
    auto *settingsMenu = menuBar()->addMenu(tr("Settings"));
    auto *refreshAction = settingsMenu->addAction(tr("Refresh serial ports"));
    auto *firmwareMenu = menuBar()->addMenu(tr("Firmware"));
    auto *openFirmware = firmwareMenu->addAction(tr("Firmware / USB DFU"));
    connect(openFirmware, &QAction::triggered, this, [tabs, firmwarePage]() {
        tabs->setCurrentWidget(firmwarePage);
    });
    auto *dfuInfo = firmwareMenu->addAction(tr("USB DFU status / instructions"));
    connect(dfuInfo, &QAction::triggered, this, [this]() {
        QMessageBox::information(this, tr("Firmware / DFU"),
            tr("Enter USB DFU bootloader mode, select a verified firmware image and detect the device. Flashing remains disabled pending hardware validation."));
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
                transfer->page = 0;
                transfer->bytes.clear();
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
        transfer->page = 0;
        transfer->bytes.clear();
        *identityVerified = false;
        session->close();
        connectionStatus_->setText(tr("ECU disconnected"));
    });
    connect(session, &tunuino::SerialSession::connectionChanged, this, [=](bool connected) {
        disconnectAction->setEnabled(connected);
        if (!connected) { liveTimer->stop(); transfer->page = 0; *identityVerified = false; *validationPending = false; validationAction->setEnabled(false); fuelRead->setEnabled(false); ignitionRead->setEnabled(false); connectionStatus_->setText(tr("ECU disconnected")); }
    });
    connect(session, &tunuino::SerialSession::exchangeFailed, this, [=](const QString &error) {
        liveTimer->stop();
        transfer->page = 0;
        *identityVerified = false;
        *validationPending = false;
        validationAction->setEnabled(false);
        fuelRead->setEnabled(false);
        ignitionRead->setEnabled(false);
        connectionStatus_->setText(tr("ECU communication failed: %1").arg(error));
        statusBar()->showMessage(error, 10000);
    });
    connect(session, &tunuino::SerialSession::responseReceived, this, [=](const QByteArray &data) {
        if (*validationPending) {
            *validationPending = false;
            liveTimer->start();
            if (data.size() != 16 || quint8(data[0]) != 0 || quint8(data[1]) != 1) {
                QMessageBox::warning(this, tr("ECU configuration"),
                    tr("Invalid validation response. No ECU settings changed."));
                return;
            }
            const auto read32 = [&data](int i) -> quint32 {
                return (quint32(quint8(data[i])) << 24) | (quint32(quint8(data[i+1])) << 16)
                    | (quint32(quint8(data[i+2])) << 8) | quint32(quint8(data[i+3]));
            };
            QMessageBox::information(this, tr("ECU configuration"),
                tr("Staged changes: %1\\nPersisted: %2\\nValidation errors: 0x%3\\nGeneration: %4\\nHeader CRC32: 0x%5\\n\\nRead-only inspection.")
                    .arg(quint8(data[2]) ? tr("Yes") : tr("No"))
                    .arg(quint8(data[3]) ? tr("Yes") : tr("No"))
                    .arg(QString::number(read32(4), 16))
                    .arg(read32(8))
                    .arg(QString::number(read32(12), 16)));
            return;
        }
        if (transfer->page) {
            const int count = qMin(64, 576 - transfer->offset);
            if (data.size() != count + 1 || quint8(data[0]) != 0) {
                abortTransfer(tr("Invalid map response; table not updated"));
                return;
            }
            transfer->bytes.append(data.constData() + 1, count);
            transfer->offset += count;
            if (transfer->offset < 576) {
                if (!sendChunk()) abortTransfer(tr("Map read interrupted"));
                return;
            }
            if (transfer->bytes.size() != 576) {
                abortTransfer(tr("Incomplete map read"));
                return;
            }
            const auto u16 = [transfer](int offset) -> quint16 {
                return (quint16(quint8(transfer->bytes[offset])) << 8)
                    | quint16(quint8(transfer->bytes[offset + 1]));
            };
            bool valid = true;
            for (int i = 0; i < 16; ++i) {
                if (u16(2 * i) > 20000 || u16(32 + 2 * i) > 50000 ||
                    (i && (u16(2 * i) <= u16(2 * (i - 1)) ||
                           u16(32 + 2 * i) <= u16(32 + 2 * (i - 1))))) valid = false;
            }
            const int page = transfer->page;
            for (int i = 0; i < 256; ++i) {
                const qint16 value = qint16(u16(64 + 2 * i));
                if ((page == 2 && (value < 0 || value > 30000)) ||
                    (page == 3 && (value < -9000 || value > 9000))) valid = false;
            }
            if (!valid) {
                abortTransfer(tr("Map axes or cells failed validation; table unchanged"));
                return;
            }
            auto *table = page == 2 ? maps : ignitionMap;
            QStringList rpmLabels, loadLabels;
            for (int i = 0; i < 16; ++i) {
                rpmLabels.append(QString::number(u16(2 * i)));
                loadLabels.append(QString::number(u16(32 + 2 * i) / 100.0, 'f', 1));
            }
            table->setHorizontalHeaderLabels(rpmLabels);
            table->setVerticalHeaderLabels(loadLabels);
            for (int row = 0; row < 16; ++row)
                for (int col = 0; col < 16; ++col)
                    table->item(row, col)->setText(QString::number(qint16(u16(64 + 2 * (row * 16 + col))) / 100.0, 'f', 2));
            abortTransfer(tr("Map page %1 read from ECU (read-only)").arg(page));
            return;
        }
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
        validationAction->setEnabled(true);
        fuelRead->setEnabled(true);
        ignitionRead->setEnabled(true);
        liveTimer->start();
        statusBar()->showMessage(tr("ECU identity verified. Read-only runtime polling active; configuration writes disabled."));
    });
    refreshPorts();
    statusBar()->showMessage(tr("TUNUINO  |  ECU connection ready  |  Map read/write unavailable until firmware integration"));
}
