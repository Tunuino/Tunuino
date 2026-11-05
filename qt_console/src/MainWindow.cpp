#include "MainWindow.h"
#include "NativeDfuFlash.h"
#include <QPainter>
#include <QClipboard>
#include <QApplication>
#include <QSignalBlocker>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPolygonF>
#include <QFrame>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QPointer>
#include <QSettings>
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
#include <QInputDialog>
#include <QRegularExpression>
#include <QSplitter>
#include <QAbstractItemView>
#include <QComboBox>
#include <QCheckBox>
#include <QSerialPortInfo>
#include <QAction>
#include <QActionGroup>
#include <QSettings>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QVersionNumber>
#include <QDesktopServices>
#include <QUrl>
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
#include <limits>
#include <algorithm>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <setupapi.h>
#include <winusb.h>
#include <usbiodef.h>
#endif
#include "ecu/SerialSession.h"

// Live engine operating-point crosshair; visual overlay only, never edits cells.
class LiveMapTable final : public QTableWidget {
public:
    explicit LiveMapTable(QWidget *parent) : QTableWidget(16, 16, parent) {}
    void setOperatingPoint(double rpm, double kpa, const QStringList &rpmBins) {
        if (!std::isfinite(rpm) || !std::isfinite(kpa) || rpmBins.size() != 16) {
            clearOperatingPoint();
            return;
        }
        auto nearest = [](double value, const QVector<double> &bins) {
            int best = -1;
            double distance = std::numeric_limits<double>::infinity();
            for (int i = 0; i < bins.size(); ++i) {
                if (!std::isfinite(bins[i])) continue;
                const double d = std::abs(value - bins[i]);
                if (d < distance) { distance = d; best = i; }
            }
            return best;
        };
        QVector<double> rpmAxis, loadAxis;
        for (int col = 0; col < 16; ++col) {
            bool ok = false;
            const double v = rpmBins.at(col).toDouble(&ok);
            rpmAxis.append(ok ? v : std::numeric_limits<double>::quiet_NaN());
        }
        for (int row = 0; row < 16; ++row) {
            const auto *header = verticalHeaderItem(row);
            bool ok = false;
            const double v = header ? header->text().toDouble(&ok) : 0.0;
            loadAxis.append(ok ? v : std::numeric_limits<double>::quiet_NaN());
        }
        column_ = nearest(rpm, rpmAxis);
        row_ = nearest(kpa, loadAxis);
        viewport()->update();
    }
    void clearOperatingPoint() {
        if (row_ == -1 && column_ == -1) return;
        row_ = column_ = -1;
        viewport()->update();
    }
protected:
    void paintEvent(QPaintEvent *event) override {
        QTableWidget::paintEvent(event);
        if (row_ < 0 || column_ < 0) return;
        const QRect cell = visualRect(model()->index(row_, column_));
        if (cell.isEmpty()) return;
        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing, false);
        QPen dark(QColor(18, 18, 18, 220), 4);
        painter.setPen(dark);
        painter.drawLine(cell.center().x(), 0, cell.center().x(), viewport()->height());
        painter.drawLine(0, cell.center().y(), viewport()->width(), cell.center().y());
        painter.setPen(QPen(QColor(255, 255, 255, 245), 2));
        painter.drawLine(cell.center().x(), 0, cell.center().x(), viewport()->height());
        painter.drawLine(0, cell.center().y(), viewport()->width(), cell.center().y());
        painter.setPen(QPen(QColor(255, 120, 25), 3));
        painter.drawRect(cell.adjusted(2, 2, -2, -2));
    }
private:
    int row_ = -1;
    int column_ = -1;
};

// Keep floating MDI windows entirely within the central workspace viewport.
class WorkspaceBoundsGuard final : public QObject {
public:
    explicit WorkspaceBoundsGuard(QMdiArea *area) : QObject(area), area_(area) {
        area_->viewport()->installEventFilter(this);
    }
    void watch(QMdiSubWindow *window) {
        window->installEventFilter(this);
        windows_.append(window);
    }
protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::Resize && watched == area_->viewport()) {
            QTimer::singleShot(0, this, [this]() { for (const auto &w : windows_) clamp(w.data()); });
        } else if (event->type() == QEvent::Move || event->type() == QEvent::Resize) {
            if (auto *w = qobject_cast<QMdiSubWindow *>(watched)) clamp(w);
        }
        return QObject::eventFilter(watched, event);
    }
private:
    void clamp(QMdiSubWindow *w) {
        if (!w || !w->isVisible() || w->isMaximized() || adjusting_) return;
        const QRect bounds = area_->viewport()->rect();
        if (bounds.isEmpty()) return;
        adjusting_ = true;
        if (w->width() > bounds.width() || w->height() > bounds.height())
            w->resize(qMin(w->width(), bounds.width()), qMin(w->height(), bounds.height()));
        const QPoint p = w->pos();
        const QPoint bounded(qBound(0, p.x(), qMax(0, bounds.width() - w->width())),
                             qBound(0, p.y(), qMax(0, bounds.height() - w->height())));
        if (p != bounded) w->move(bounded);
        adjusting_ = false;
    }
    QMdiArea *area_;
    QList<QPointer<QMdiSubWindow>> windows_;
    bool adjusting_ = false;
};

// Read-only perspective surface. The plotted samples are taken directly from
// the visible ECU map table; empty cells never generate synthetic map values.

class MapSurfacePreview final : public QFrame {
public:
    explicit MapSurfacePreview(QTableWidget *source, QWidget *parent = nullptr)
        : QFrame(parent), source_(source) {
        setMinimumSize(310, 340);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setFrameShape(QFrame::StyledPanel);
        setCursor(Qt::OpenHandCursor);
        setToolTip(tr("Drag left/right/up/down to rotate; use mouse wheel to zoom; double-click to reset view."));
        connect(source_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *) { update(); });
    }
protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            dragging_ = true;
            lastPos_ = event->position();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
        QFrame::mousePressEvent(event);
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        if (dragging_) {
            const QPointF delta = event->position() - lastPos_;
            yaw_ += delta.x() * 0.012;
            pitch_ = qBound(0.12, pitch_ + delta.y() * 0.008, 1.48);
            lastPos_ = event->position();
            update();
            event->accept();
            return;
        }
        QFrame::mouseMoveEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            dragging_ = false;
            setCursor(Qt::OpenHandCursor);
            event->accept();
            return;
        }
        QFrame::mouseReleaseEvent(event);
    }
    void mouseDoubleClickEvent(QMouseEvent *event) override {
        yaw_ = 0.72;
        pitch_ = 0.65;
        zoom_ = 1.0;
        update();
        event->accept();
    }
    void wheelEvent(QWheelEvent *event) override {
        zoom_ = qBound(0.4, zoom_ * (event->angleDelta().y() > 0 ? 1.12 : 1.0 / 1.12), 3.0);
        update();
        event->accept();
    }
    void paintEvent(QPaintEvent *event) override {
        QFrame::paintEvent(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect().adjusted(1, 1, -1, -1), QColor(32, 35, 40));
        double z[16][16] = {};
        double minValue = 1e100, maxValue = -1e100;
        for (int row = 0; row < 16; ++row) {
            for (int col = 0; col < 16; ++col) {
                const auto *item = source_->item(row, col);
                bool ok = false;
                const double value = item ? item->text().toDouble(&ok) : 0.0;
                if (!ok || !std::isfinite(value)) {
                    painter.setPen(QColor(185, 188, 192));
                    painter.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap,
                                     tr("No valid map values available"));
                    return;
                }
                z[row][col] = value;
                minValue = (std::min)(minValue, value);
                maxValue = (std::max)(maxValue, value);
            }
        }
        const double span = (std::max)(0.001, maxValue - minValue);
        const double cy = std::cos(yaw_), sy = std::sin(yaw_);
        const double cp = std::cos(pitch_), sp = std::sin(pitch_);
        struct Vertex { QPointF p; double depth; };
        auto project = [&](int row, int col) -> Vertex {
            const double x = (col - 7.5) / 7.5;
            const double y = (row - 7.5) / 7.5;
            const double h = ((z[row][col] - minValue) / span - 0.5) * 0.9;
            const double rx = x * cy - y * sy;
            const double ry = x * sy + y * cy;
            return {QPointF(rx, ry * cp - h * sp), ry * sp + h * cp};
        };
        Vertex verts[16][16];
        double left = 1e100, right = -1e100, top = 1e100, bottom = -1e100;
        for (int row = 0; row < 16; ++row)
            for (int col = 0; col < 16; ++col) {
                verts[row][col] = project(row, col);
                left = (std::min)(left, verts[row][col].p.x());
                right = (std::max)(right, verts[row][col].p.x());
                top = (std::min)(top, verts[row][col].p.y());
                bottom = (std::max)(bottom, verts[row][col].p.y());
            }
        const QRectF view = QRectF(rect()).adjusted(16, 38, -16, -16);
        const double fit = (std::min)(view.width() / (std::max)(0.01, right-left),
                                      view.height() / (std::max)(0.01, bottom-top));
        const double scale = fit * 0.82 * zoom_;
        const QPointF center = view.center();
        const QPointF boundsCenter((left+right)*0.5, (top+bottom)*0.5);
        auto screen = [&](const Vertex &v) { return center + (v.p - boundsCenter) * scale; };
        struct Face { int row; int col; double depth; };
        QVector<Face> faces;
        faces.reserve(225);
        for (int row = 0; row < 15; ++row)
            for (int col = 0; col < 15; ++col)
                faces.append({row, col, (verts[row][col].depth + verts[row+1][col].depth
                    + verts[row][col+1].depth + verts[row+1][col+1].depth) * 0.25});
        std::sort(faces.begin(), faces.end(), [](const Face &a, const Face &b) {
            return a.depth < b.depth;
        });
        painter.save();
        painter.setClipRect(view);
        for (const Face &face : faces) {
            const int row = face.row, col = face.col;
            const double avg = (z[row][col] + z[row+1][col] +
                                z[row][col+1] + z[row+1][col+1]) / 4.0;
            const double level = (avg - minValue) / span;
            const QColor color = QColor::fromHsvF(0.33 - level * 0.32, 0.76, 0.86);
            painter.setBrush(color);
            painter.setPen(QPen(color.darker(155), 0.6));
            painter.drawPolygon(QPolygonF{screen(verts[row][col]), screen(verts[row][col+1]),
                                           screen(verts[row+1][col+1]), screen(verts[row+1][col])});
        }
        painter.restore();
        painter.setPen(QColor(220, 223, 225));
        painter.drawText(QRect(8, 6, width()-16, 24), Qt::AlignLeft,
                         tr("3D map | drag to rotate | wheel to zoom | double-click to reset"));
    }
private:
    QTableWidget *source_;
    bool dragging_ = false;
    QPointF lastPos_;
    double yaw_ = 0.72;
    double pitch_ = 0.65;
    double zoom_ = 1.0;
};

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("Tunuino Console — Qt 6");
    resize(1400, 850);
    // First MDI migration stage: retain the existing tab controller and ECU
    // connections, but host it in a movable, resizable, closable workspace window.
    auto *workspace = new QMdiArea(this);
    workspace->setObjectName(QStringLiteral("TunuinoWorkspace"));
    workspace->setViewMode(QMdiArea::SubWindowView);
    workspace->setBackground(QBrush(QColor(43, 46, 51)));
    setCentralWidget(workspace);
    auto *boundsGuard = new WorkspaceBoundsGuard(workspace);
    // Hidden page registry only; no legacy "Tables and configuration" MDI window.
    auto *tabs = new QTabWidget(this);
    tabs->hide();
    auto settingsWindows = QSharedPointer<QVector<QPointer<QMdiSubWindow>>>::create(7);
    const auto openSettingsPage = [this, workspace, tabs, settingsWindows, boundsGuard](int index) {
        if (index < 3 || index >= tabs->count()) return;
        auto &existing = (*settingsWindows)[index];
        if (!existing) {
            QWidget *page = tabs->widget(index);
            if (!page) return;
            const QString title = tabs->tabText(index);
            tabs->removeTab(index);
            page->setParent(nullptr);
            tabs->insertTab(index, new QWidget(tabs), title);
            auto *window = workspace->addSubWindow(page);
            window->setWindowTitle(title);
            window->setWindowFlags(Qt::SubWindow | Qt::WindowTitleHint |
                                   Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint |
                                   Qt::WindowCloseButtonHint);
            window->setAttribute(Qt::WA_DeleteOnClose, false);
            window->resize(1000, 740);
            existing = window;
            boundsGuard->watch(window);
        }
        existing->showNormal();
        if (existing->widget()) existing->widget()->show();
        existing->show();
        workspace->setActiveSubWindow(existing);
        existing->raise();
    };
    // Graph windows are independent from their map tables and can be closed
    // and reopened from the navigation tree without discarding ECU data.
    auto fuelGraphWindow = QSharedPointer<QPointer<QMdiSubWindow>>::create();
    auto ignitionGraphWindow = QSharedPointer<QPointer<QMdiSubWindow>>::create();
    auto mapWindows = QSharedPointer<QVector<QPointer<QMdiSubWindow>>>::create(3);
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
                // Do not present unfinished ECU functions as clickable controls.
                // They remain visible in the intended navigation structure.
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
                item->setForeground(0, QColor(QStringLiteral("#858585")));
                item->setToolTip(0, tr("Not available: ECU firmware and configuration protocol not implemented yet"));
            }
        }
        return category;
    };
    addCategory(tr("Sensors and inputs"), {{tr("TPS Calibration"),6},{tr("MAP Calibration"),6},{tr("Temperature sensors"),6},{tr("Lambda / O2"),6}});
    addCategory(tr("Engine start"), {{tr("Cranking"),-1},{tr("Afterstart"),-1}});
    addCategory(tr("Fueling"), {{tr("Fuel Table"),0},{tr("Fuel 3D Graph"),7},{tr("AFR Target"),2},{tr("Injector Dead Time"),6}});
    addCategory(tr("Ignition"), {{tr("Ignition Table"),1},{tr("Ignition 3D Graph"),8},{tr("Trigger setup"),6}});
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
    addCategory(tr("Calibration & Setup"), {{tr("Sensor and engine calibration"),6}});
    auto engineStatusDockRef = QSharedPointer<QPointer<QDockWidget>>::create();
    connect(navigationTree, &QTreeWidget::itemClicked, tabs, [tabs, workspace, openSettingsPage, fuelGraphWindow, ignitionGraphWindow, engineStatusDockRef, mapWindows](QTreeWidgetItem *item, int) {
        const int index = item->data(0, Qt::UserRole).toInt();
        if (item->parent() && index == 9) {
            if (*engineStatusDockRef) { (*engineStatusDockRef)->show(); (*engineStatusDockRef)->raise(); }
        } else if (item->parent() && (index == 7 || index == 8)) {
            auto window = (index == 7) ? *fuelGraphWindow : *ignitionGraphWindow;
            if (window) { window->showNormal(); workspace->setActiveSubWindow(window); }
        } else if (item->parent() && index >= 0 && index <= 2 && (*mapWindows)[index]) {
            auto window = (*mapWindows)[index];
            window->showNormal();
            if (window->widget()) window->widget()->show();
            window->show();
            workspace->setActiveSubWindow(window);
            window->raise();
        } else if (item->parent() && index >= 0 && index < tabs->count()) {
            openSettingsPage(index);
        }
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
        tr("Fuel / VE table. ECU values must be read and verified before use."),
        tr("Ignition advance table. Verify trigger configuration and timing before running an engine."),
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
    auto *fuelActions = new QWidget(fuelPage);
    auto *fuelActionsLayout = new QHBoxLayout(fuelActions);
    fuelActionsLayout->setContentsMargins(0, 0, 0, 0);
    auto *fuelRead = new QPushButton(tr("Read from ECU"), fuelActions);
    auto *fuelSave = new QPushButton(tr("Write to ECU"), fuelActions);
    fuelRead->setEnabled(false);
    fuelSave->setEnabled(false);
    fuelRead->setToolTip(QString());
    fuelSave->setToolTip(tr("Requires validated ECU map-write protocol and safety checks."));
    // Automatic ECU synchronization replaces the per-page read control.
    // Write is unavailable until ECU persistence is verified.
    fuelActionsLayout->addStretch();
    // Editing is exposed by the table context menu, not a button row.
    fuelActions->hide();
    const QStringList defaultRpmAxis = {QStringLiteral("200"), QStringLiteral("400"), QStringLiteral("600"), QStringLiteral("800"), QStringLiteral("1000"), QStringLiteral("1500"), QStringLiteral("2000"), QStringLiteral("2500"), QStringLiteral("3000"), QStringLiteral("3500"), QStringLiteral("4000"), QStringLiteral("4500"), QStringLiteral("5000"), QStringLiteral("5500"), QStringLiteral("6000"), QStringLiteral("7000")};
    const QStringList defaultMapAxis = {QStringLiteral("105"), QStringLiteral("100"), QStringLiteral("90"), QStringLiteral("80"), QStringLiteral("75"), QStringLiteral("70"), QStringLiteral("65"), QStringLiteral("60"), QStringLiteral("55"), QStringLiteral("50"), QStringLiteral("45"), QStringLiteral("40"), QStringLiteral("35"), QStringLiteral("30"), QStringLiteral("25"), QStringLiteral("20")};
    // Tunuino default offline tables: 16x16 cells in ascending MAP order.
    // Compact byte-packed values; AFR uses tenths. Display rows are reversed
    // so MAP increases from bottom to top. No ECU write is performed here.
    const QByteArray fuelDefaults = QByteArray::fromBase64("TExQNyUlIywqKiopKSkpKUxGUC4iJiMqMzIxMC8uLi5NTVUzKC4qNzQ0NDQ0NDQ0TE5RQTMzLz41NjY3Nzg4OFFZWj81OTU8NTY3ODk6OjpVWls4LzItNjs7Ozs7Ozs7V1pcQTU0NDtFREJBQD8+PlRZXUQ7NjU9RUVFRD5ERERWWWRFQTw7Qz09PDw8Ozs7SEtZR0JBQEM9PT4+Pj8/P01NWUZIRkRFP0JER0lMTU1OVFpITE1NUVZVVVRUU1NTTlZaTFhaWVZdXV1dXV1dXU5WXVFfYF9oYmNjZGVmZmZOVl5TY2ZrgI2JhYB8eHZ2T1RmYmttdIabjIeCeXh4eA==");
    const QByteArray ignitionDefaults = QByteArray::fromBase64("Dw0PGBkcHyEiIyQlJiYnKA8NDxgZHB8hIiMkJSYmJygPDQ8YGRwfISIjJCUmJicoDw0PGBkcHyEiIiQlJSUmJw8NDxcYHB8gIiIkJSUlJicPDRAXGBwfICEiIyQkJCUmDw0RFxgbHh8gISIjJCQlJg8NEBcYGx4fICEiIyQkJSYODRAXGBodHyAhIiMjIyQkDg0QERIUHR4fICEiIiIjIw4NEBESFB0eHyAgISEhIiIODREREhQaGx8fICAgICEhDQ0RERIUGhseHh8fHx8gIA0OERESFBobHh4eHh4eHx8NDxEREhMZGh0dHR0dHR0dDQ8RERITGxwcHBwQHBwcHA==");
    // Three trailing AFR samples were absent in the imported data; temporarily
    // repeat the last supplied sample to complete the 16x16 display grid.
    const QByteArray afrDefaults = QByteArray::fromBase64("k5OTlJeXlpOTk5OTk5OTk5OTk5SXl5aTk5OTk5OTk5OTk5OUl5eWk5OTk5OTk5OTlJaWlZOSkpKRkZCQkJOTk5SVlpSSkI+OjYyMjIyTk5OTlJWTko2Mi4mIh4eHj4+Pk5OTko6KioqIh4aGho6OjpOTk5GMiIiIhoWDg4ONjY6RkY+MiIOCgoKBgYGBjY2Ojo2MiYaCgoGBgYGBgY2NjouIiIaDgYGBgYCAgICMjIyHhoaEgoGBgYCAgICAiYmJgIGBgYCAgICAgICAgIaGhoCBgIB/f39/f39/f3+Dg4OAgH9+fn5+fn5+fn5+gICAgIB/fX19fX19fX19fX19fQ==");
    const QStringList fuelRpmBins = QStringLiteral("500,700,900,1500,2100,2900,3800,4700,5500,5700,5900,6100,6300,6700,7000,7200").split(',');
    const QStringList fuelMapBins = QStringLiteral("16,26,30,36,40,46,50,56,60,66,70,76,86,90,96,100").split(',');
    const QStringList ignitionRpmBins = QStringLiteral("500,1000,1500,2000,2500,3000,3500,4000,4500,5000,5500,6000,6300,6700,7000,7200").split(',');
    const QStringList ignitionMapBins = QStringLiteral("10,16,20,26,30,36,40,46,50,56,66,74,80,88,96,100").split(',');
    const QStringList afrRpmBins = QStringLiteral("500,700,900,1500,2100,2900,3800,4700,5500,5700,5900,6100,6300,6500,6500,6500").split(',');
    const QStringList afrMapBins = fuelMapBins;
    auto fillDefaultTable = [](QTableWidget *table, const QByteArray &packed, double divisor, const QStringList &mapAxis) {
        if (packed.size() != 256 || mapAxis.size() != 16) return;
        QStringList verticalLabels;
        for (int row = 0; row < 16; ++row) {
            const int sourceRow = 15 - row;
            verticalLabels.append(mapAxis.at(sourceRow));
            for (int col = 0; col < 16; ++col) {
                const int raw = static_cast<unsigned char>(packed.at(sourceRow * 16 + col));
                auto *item = new QTableWidgetItem(QString::number(raw / divisor, 'f', divisor == 1.0 ? 0 : 1));
                item->setTextAlignment(Qt::AlignCenter);
                table->setItem(row, col, item);
            }
        }
        table->setVerticalHeaderLabels(verticalLabels);
    };
    // Heatmap for the 2D calibration grids. The color range follows the
    // actual numeric cells and is refreshed after edits or opening a map.
    auto installMapHeatmap = [this](QTableWidget *table) {
        // Improve numeric readability without changing window geometry or cell sizes.
        QFont valueFont = table->font();
        valueFont.setPointSize(11);
        valueFont.setWeight(QFont::Bold);
        table->setFont(valueFont);
        const auto recolor = [table]() {
            double minimum = 1e100, maximum = -1e100;
            for (int row = 0; row < table->rowCount(); ++row)
                for (int col = 0; col < table->columnCount(); ++col) {
                    const auto *item = table->item(row, col);
                    bool ok = false;
                    const double value = item ? item->text().toDouble(&ok) : 0.0;
                    if (!ok || !std::isfinite(value)) continue;
                    minimum = (std::min)(minimum, value);
                    maximum = (std::max)(maximum, value);
                }
            const double range = (std::max)(0.001, maximum - minimum);
            QSignalBlocker blocker(table);
            for (int row = 0; row < table->rowCount(); ++row)
                for (int col = 0; col < table->columnCount(); ++col) {
                    auto *item = table->item(row, col);
                    if (!item) continue;
                    bool ok = false;
                    const double value = item->text().toDouble(&ok);
                    if (!ok || !std::isfinite(value)) {
                        item->setBackground(QColor(92, 45, 49));
                        item->setForeground(QColor(255, 255, 255));
                        continue;
                    }
                    const double level = qBound(0.0, (value - minimum) / range, 1.0);
                    // Blue/green for lower values, yellow/orange/red for higher.
                    const QColor color = QColor::fromHsvF(0.66 * (1.0 - level), 0.72, 0.86);
                    item->setBackground(color);
                    item->setForeground(color.lightnessF() < 0.48 ? QColor(255, 255, 255)
                                                                      : QColor(20, 24, 28));
                }
        };
        recolor();
        connect(table, &QTableWidget::itemChanged, this, [recolor](QTableWidgetItem *) { recolor(); });
    };
    // Edit the selected rectangular region of the active offline map.
    // Read all source values before applying changes to avoid order-dependent smoothing.
    auto editMapRegion = [this](QTableWidget *table, int mode, bool afr) {
        if (!table) return;
        const auto ranges = table->selectedRanges();
        if (ranges.size() != 1) {
            QMessageBox::information(this, tr("Select map cells"),
                tr("Select one rectangular region of cells first."));
            return;
        }
        const QTableWidgetSelectionRange area = ranges.first();
        const int top = area.topRow(), bottom = area.bottomRow();
        const int left = area.leftColumn(), right = area.rightColumn();
        if (((mode == 0 || mode == 3) && (bottom == top || right == left)) ||
            (mode == 1 && right == left) || (mode == 2 && bottom == top)) {
            QMessageBox::information(this, tr("Interpolation"),
                tr("Select at least two rows and two columns to interpolate."));
            return;
        }
        double values[16][16] = {};
        for (int row = 0; row < 16; ++row)
            for (int col = 0; col < 16; ++col) {
                const auto *cell = table->item(row, col);
                bool ok = false;
                const double v = cell ? cell->text().toDouble(&ok) : 0.0;
                if (!ok || !std::isfinite(v)) {
                    QMessageBox::warning(this, tr("Invalid map"), tr("Correct invalid map values before editing."));
                    return;
                }
                values[row][col] = v;
            }
        double output[16][16] = {};
        for (int row = top; row <= bottom; ++row)
            for (int col = left; col <= right; ++col) {
                double value;
                if (mode == 4) {
                    // One pass, 3x3 weighted filter, constrained to the selection.
                    double total = 0.0, weightSum = 0.0;
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx) {
                            const int rr = row + dy, cc = col + dx;
                            if (rr < top || rr > bottom || cc < left || cc > right) continue;
                            const double weight = (dy == 0 ? 2.0 : 1.0) * (dx == 0 ? 2.0 : 1.0);
                            total += values[rr][cc] * weight;
                            weightSum += weight;
                        }
                    value = total / weightSum;
                } else if (mode == 1) {
                    const double x = double(col - left) / double(right - left);
                    value = (1.0-x)*values[row][left] + x*values[row][right];
                } else if (mode == 2) {
                    const double y = double(row - top) / double(bottom - top);
                    value = (1.0-y)*values[top][col] + y*values[bottom][col];
                } else {
                    const double x = double(col - left) / double(right - left);
                    const double y = double(row - top) / double(bottom - top);
                    value = (1.0-y)*((1.0-x)*values[top][left] + x*values[top][right])
                          + y*((1.0-x)*values[bottom][left] + x*values[bottom][right]);
                }
                output[row][col] = value;
            }
        for (int row = top; row <= bottom; ++row)
            for (int col = left; col <= right; ++col)
                table->item(row, col)->setText(QString::number(output[row][col], 'f', afr ? 1 : 0));
    };
    // Per-table undo/redo history. Snapshot changes once per event-loop turn,
    // so bulk interpolation/smoothing is one undoable operation.
    auto installTableEditControls = [this, editMapRegion](QHBoxLayout *bar, QTableWidget *table, bool afr) {
        struct History {
            QVector<QStringList> undo;
            QVector<QStringList> redo;
            QStringList current;
            bool pending = false;
            bool restoring = false;
        };
        auto history = QSharedPointer<History>::create();
        const auto snapshot = [table]() {
            QStringList result;
            result.reserve(256);
            for (int row = 0; row < 16; ++row)
                for (int col = 0; col < 16; ++col)
                    result.append(table->item(row, col) ? table->item(row, col)->text() : QString());
            return result;
        };
        history->current = snapshot();
        const auto restore = [table, history](const QStringList &values) {
            if (values.size() != 256) return;
            history->restoring = true;
            for (int row = 0; row < 16; ++row)
                for (int col = 0; col < 16; ++col)
                    if (auto *cell = table->item(row, col))
                        cell->setText(values.at(row * 16 + col));
            history->restoring = false;
        };
        const auto flush = [history, snapshot]() {
            history->pending = false;
            const QStringList next = snapshot();
            if (next == history->current) return;
            history->undo.append(history->current);
            if (history->undo.size() > 50) history->undo.removeFirst();
            history->redo.clear();
            history->current = next;
        };
        connect(table, &QTableWidget::itemChanged, this, [this, history, flush](QTableWidgetItem *) {
            if (history->restoring || history->pending) return;
            history->pending = true;
            QTimer::singleShot(0, this, [history, flush]() {
                if (history->pending) flush();
            });
        });
        auto *interpolate = new QAction(tr("Interpolate"), table);
        auto *smooth = new QAction(tr("Smooth"), table);
        auto *undo = new QAction(tr("Undo"), table);
        auto *redo = new QAction(tr("Redo"), table);
        // QAction objects have no visual widget: editing is available only through the context menu.
        Q_UNUSED(bar);
        connect(interpolate, &QAction::triggered, this, [table, afr, editMapRegion, flush]() {
            flush();
            editMapRegion(table, 3, afr);
            flush();
        });
        connect(smooth, &QAction::triggered, this, [table, afr, editMapRegion, flush]() {
            flush();
            editMapRegion(table, 4, afr);
            flush();
        });
        connect(undo, &QAction::triggered, this, [history, flush, snapshot, restore]() {
            flush();
            if (history->undo.isEmpty()) return;
            history->redo.append(history->current);
            const QStringList previous = history->undo.takeLast();
            restore(previous);
            history->current = snapshot();
        });
        connect(redo, &QAction::triggered, this, [history, flush, snapshot, restore]() {
            flush();
            if (history->redo.isEmpty()) return;
            history->undo.append(history->current);
            const QStringList next = history->redo.takeLast();
            restore(next);
            history->current = snapshot();
        });
        // Context commands documented in EMU Black V3 Software Guide.
        // The existing interpolation and smoothing handlers retain their undo history.
        table->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(table, &QWidget::customContextMenuRequested, table,
                [table, afr, editMapRegion, flush, interpolate, smooth, undo, redo](const QPoint &point) {
            QMenu menu(table);
            auto *horizontalAction = menu.addAction(QObject::tr("Interpolate horizontally\tCtrl+H"));
            auto *verticalAction = menu.addAction(QObject::tr("Interpolate vertically\tCtrl+L"));
            auto *diagonalAction = menu.addAction(QObject::tr("Interpolate diagonally\tCtrl+D"));
            auto *smoothAction = menu.addAction(QObject::tr("Equalize selection\tE"));
            menu.addSeparator();
            auto *copyAction = menu.addAction(QObject::tr("Copy cells"));
            auto *pasteAction = menu.addAction(QObject::tr("Paste cells"));
            menu.addSeparator();
            auto *undoAction = menu.addAction(QObject::tr("Undo"));
            auto *redoAction = menu.addAction(QObject::tr("Redo"));
            QAction *chosen = menu.exec(table->viewport()->mapToGlobal(point));
            if (chosen == horizontalAction || chosen == verticalAction || chosen == diagonalAction) {
                flush();
                editMapRegion(table, chosen == horizontalAction ? 1 : chosen == verticalAction ? 2 : 3, afr);
                flush();
            }
            else if (chosen == smoothAction) smooth->trigger();
            else if (chosen == undoAction) undo->trigger();
            else if (chosen == redoAction) redo->trigger();
            else if (chosen == copyAction) {
                QStringList rows;
                const auto range = table->selectedRanges();
                if (range.isEmpty()) return;
                const auto area = range.first();
                for (int row = area.topRow(); row <= area.bottomRow(); ++row) {
                    QStringList cells;
                    for (int col = area.leftColumn(); col <= area.rightColumn(); ++col) {
                        const auto *item = table->item(row, col);
                        cells << (item ? item->text() : QString());
                    }
                    rows << cells.join(QLatin1Char('\t'));
                }
                QApplication::clipboard()->setText(rows.join(QLatin1Char('\n')));
            } else if (chosen == pasteAction) {
                const auto selected = table->selectedRanges();
                if (selected.isEmpty()) return;
                const QString clipboard = QApplication::clipboard()->text();
                const QStringList rows = clipboard.split(QLatin1Char('\n'));
                const int top = selected.first().topRow();
                const int left = selected.first().leftColumn();
                for (int r = 0; r < rows.size() && top + r < table->rowCount(); ++r) {
                    const QStringList cells = rows.at(r).split(QLatin1Char('\t'));
                    for (int c = 0; c < cells.size() && left + c < table->columnCount(); ++c) {
                        bool ok = false;
                        const double value = cells.at(c).trimmed().toDouble(&ok);
                        if (ok && std::isfinite(value) && table->item(top + r, left + c))
                            table->item(top + r, left + c)->setText(QString::number(value));
                    }
                }
            }
        });
    };
    // Offline map persistence; never sends data to the ECU.
    auto installMapFileActions = [this](QTableWidget *table, const QString &mapId,
                                        const QStringList &rpmAxis, const QStringList &mapAxis,
                                        double maximum) {
        auto *openAction = new QAction(tr("Open map"), this);
        auto *saveAction = new QAction(tr("Save map"), this);
        connect(saveAction, &QAction::triggered, this, [this, table, mapId, rpmAxis, mapAxis, maximum]() {
            QJsonArray rows;
            for (int row = 0; row < 16; ++row) {
                QJsonArray values;
                for (int col = 0; col < 16; ++col) {
                    bool ok = false;
                    const auto *item = table->item(row, col);
                    const double value = item ? item->text().toDouble(&ok) : 0.0;
                    if (!ok || !std::isfinite(value) || value < 0.0 || value > maximum) {
                        QMessageBox::warning(this, tr("Invalid map"), tr("Invalid value at row %1, column %2.").arg(row+1).arg(col+1));
                        return;
                    }
                    values.append(value);
                }
                rows.append(values);
            }
            QJsonArray rpms, loads;
            for (const QString &value : rpmAxis) rpms.append(value.toDouble());
            for (int row = 15; row >= 0; --row) loads.append(mapAxis.at(row).toDouble());
            QJsonObject doc;
            doc.insert(QStringLiteral("format"), QStringLiteral("tunuino-map-v1"));
            doc.insert(QStringLiteral("map"), mapId);
            doc.insert(QStringLiteral("rpm"), rpms);
            doc.insert(QStringLiteral("mapKpa"), loads);
            doc.insert(QStringLiteral("values"), rows);
            const QString filename = QFileDialog::getSaveFileName(this, tr("Save map"), mapId + QStringLiteral(".json"), tr("Tunuino maps (*.json)"));
            if (filename.isEmpty()) return;
            QSaveFile file(filename);
            if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(doc).toJson(QJsonDocument::Indented)) < 0 || !file.commit())
                QMessageBox::warning(this, tr("Save failed"), tr("Could not save map."));
        });
        connect(openAction, &QAction::triggered, this, [this, table, mapId, rpmAxis, mapAxis, maximum]() {
            const QString filename = QFileDialog::getOpenFileName(this, tr("Open map"), QString(), tr("Tunuino maps (*.json)"));
            if (filename.isEmpty()) return;
            QFile file(filename);
            if (!file.open(QIODevice::ReadOnly)) {
                QMessageBox::warning(this, tr("Open failed"), tr("Could not read map."));
                return;
            }
            QJsonParseError error;
            const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
            const QJsonObject obj = doc.object();
            const QJsonArray rows = obj.value(QStringLiteral("values")).toArray();
            const QJsonArray rpms = obj.value(QStringLiteral("rpm")).toArray();
            const QJsonArray loads = obj.value(QStringLiteral("mapKpa")).toArray();
            bool valid = error.error == QJsonParseError::NoError && doc.isObject()
                && obj.value(QStringLiteral("format")).toString() == QStringLiteral("tunuino-map-v1")
                && obj.value(QStringLiteral("map")).toString() == mapId
                && rows.size() == 16 && rpms.size() == 16 && loads.size() == 16;
            for (int i = 0; valid && i < 16; ++i) {
                if (!rpms.at(i).isDouble() || !loads.at(i).isDouble()
                    || std::abs(rpms.at(i).toDouble() - rpmAxis.at(i).toDouble()) > 0.01
                    || std::abs(loads.at(i).toDouble() - mapAxis.at(15-i).toDouble()) > 0.01
                    || !rows.at(i).isArray() || rows.at(i).toArray().size() != 16) { valid = false; break; }
                const QJsonArray row = rows.at(i).toArray();
                for (int j = 0; j < 16; ++j) {
                    const QJsonValue v = row.at(j);
                    if (!v.isDouble() || !std::isfinite(v.toDouble()) || v.toDouble() < 0.0 || v.toDouble() > maximum) {
                        valid = false; break;
                    }
                }
            }
            if (!valid) {
                QMessageBox::warning(this, tr("Invalid map"), tr("Map type, axes, dimensions or cell values are incompatible."));
                return;
            }
            for (int i = 0; i < 16; ++i)
                for (int j = 0; j < 16; ++j)
                    table->item(i, j)->setText(QString::number(rows.at(i).toArray().at(j).toDouble(), 'f', mapId == QStringLiteral("afr") ? 1 : 0));
        });
        return qMakePair(openAction, saveAction);
    };
    auto *maps = new LiveMapTable(fuelPage);
    maps->setAlternatingRowColors(true);
    maps->setSelectionMode(QAbstractItemView::ExtendedSelection);
    maps->setSelectionBehavior(QAbstractItemView::SelectItems);
    maps->setCornerButtonEnabled(false);
    maps->setToolTip(tr("Tunuino offline base map. Validate before use on a real engine."));
    maps->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    maps->horizontalHeader()->setDefaultSectionSize(62);
    maps->horizontalHeader()->setMinimumSectionSize(48);
    maps->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    maps->verticalHeader()->setDefaultSectionSize(28);
    maps->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    maps->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    maps->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    fillDefaultTable(maps, fuelDefaults, 1.0, fuelMapBins);
    installMapHeatmap(maps);
    fuelActionsLayout->insertStretch(2, 1);
    installTableEditControls(fuelActionsLayout, maps, false);
    const auto fuelFileActions = installMapFileActions(maps, QStringLiteral("fuel"), fuelRpmBins, fuelMapBins, 255.0);
    maps->setCornerButtonEnabled(false);
    maps->horizontalHeader()->setToolTip(tr("RPM (rev/min)"));
    maps->verticalHeader()->setToolTip(tr("MAP (kPa)"));
    maps->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    maps->setMinimumSize(480, 420);
    auto *fuelGridPanel = new QWidget(fuelPage);
    auto *fuelGridLayout = new QVBoxLayout(fuelGridPanel);
    fuelGridLayout->setContentsMargins(0, 0, 0, 0);
    fuelGridLayout->setSpacing(3);
    auto *fuelAxisRow = new QHBoxLayout();
    fuelAxisRow->setSpacing(5);
    // MAP axis label is positioned to the left of the grid.
    fuelAxisRow->addWidget(maps, 1);
    auto *fuelRpmAxis = new QLabel(tr("M\nA\nP\n\nk\nP\na"), fuelGridPanel);
    fuelRpmAxis->setAlignment(Qt::AlignCenter);
    fuelRpmAxis->setFixedWidth(34);
    fuelRpmAxis->setWordWrap(false);
    fuelRpmAxis->setStyleSheet(QStringLiteral("font-weight: 600; color: #e69a32;"));
    fuelAxisRow->insertWidget(0, fuelRpmAxis, 0, Qt::AlignVCenter);
    fuelGridLayout->addLayout(fuelAxisRow, 1);
    maps->horizontalHeader()->hide();
    auto *fuelRpmValues = new QTableWidget(1, 16, fuelGridPanel);
    fuelRpmValues->horizontalHeader()->hide();
    fuelRpmValues->verticalHeader()->hide();
    fuelRpmValues->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    fuelRpmValues->setFocusPolicy(Qt::NoFocus);
    fuelRpmValues->setSelectionMode(QAbstractItemView::NoSelection);
    fuelRpmValues->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    fuelRpmValues->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    fuelRpmValues->horizontalHeader()->setDefaultSectionSize(maps->horizontalHeader()->defaultSectionSize());
    fuelRpmValues->verticalHeader()->setDefaultSectionSize(28);
    fuelRpmValues->setMinimumHeight(32);
    fuelRpmValues->setMaximumHeight(32);
    fuelRpmValues->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    fuelRpmValues->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    for (int i = 0; i < 16; ++i) {
        auto *cell = new QTableWidgetItem(fuelRpmBins.at(i));
        cell->setTextAlignment(Qt::AlignCenter);
        fuelRpmValues->setItem(0, i, cell);
    }
    auto *fuelBottomAxis = new QHBoxLayout();
    fuelBottomAxis->setContentsMargins(fuelRpmAxis->sizeHint().width() + 5 + maps->verticalHeader()->width(), 0, 0, 0);
    fuelBottomAxis->addWidget(fuelRpmValues, 1);
    fuelGridLayout->addLayout(fuelBottomAxis);

    auto *fuelMapAxis = new QLabel(tr("RPM (rev/min)"), fuelGridPanel);
    fuelMapAxis->setAlignment(Qt::AlignHCenter);
    fuelMapAxis->setStyleSheet(QStringLiteral("font-weight: 600; color: #e69a32;"));
    fuelGridLayout->addWidget(fuelMapAxis);
    // Edit MAP breakpoints by double-clicking the vertical header.
    connect(maps->verticalHeader(), &QHeaderView::sectionDoubleClicked, fuelPage,
            [maps](int row) {
        bool ok = false;
        const double current = maps->verticalHeaderItem(row)->text().toDouble();
        const double value = QInputDialog::getDouble(maps, QObject::tr("MAP breakpoint"),
            QObject::tr("MAP (kPa)"), current, 0.0, 500.0, 1, &ok);
        if (ok) maps->verticalHeaderItem(row)->setText(QString::number(value, 'f', 1));
    });
    auto *fuelWorkspace = new QHBoxLayout();
    fuelWorkspace->addWidget(fuelGridPanel, 1);
    auto *fuelSurface = new MapSurfacePreview(maps, fuelPage);
    // The 3D graph has its own MDI window; the table remains independently visible.
    auto *fuelGraphSubWindow = workspace->addSubWindow(fuelSurface);
    fuelGraphSubWindow->setWindowTitle(tr("Fuel | 3D Graph"));
    fuelGraphSubWindow->setWindowFlags(Qt::SubWindow | Qt::WindowTitleHint |
                                      Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint |
                                      Qt::WindowCloseButtonHint);
    fuelGraphSubWindow->setAttribute(Qt::WA_DeleteOnClose, false);
    fuelGraphSubWindow->resize(450, 420);
    *fuelGraphWindow = fuelGraphSubWindow;
    // Graph windows open on demand from the navigation tree.
    boundsGuard->watch(fuelGraphSubWindow);
    fuelGraphSubWindow->move(35, 45);
    // Compact view toolbar: editing commands remain on the table context menu.
    auto *fuelViewBar = new QHBoxLayout();
    auto *fuelTableView = new QPushButton(tr("2D Table"), fuelPage);
    auto *fuelGraphView = new QPushButton(tr("3D Graph"), fuelPage);
    auto *fuelSplitView = new QPushButton(tr("2D + 3D"), fuelPage);
    fuelTableView->setToolTip(tr("Show the 2D fuel table"));
    fuelGraphView->setToolTip(tr("Open the 3D fuel graph window"));
    fuelSplitView->setToolTip(tr("Show both 2D fuel table and 3D graph"));
    fuelViewBar->addWidget(fuelTableView);
    fuelViewBar->addWidget(fuelGraphView);
    fuelViewBar->addWidget(fuelSplitView);
    fuelViewBar->addStretch();
    fuelLayout->insertLayout(0, fuelViewBar);
    connect(fuelTableView, &QPushButton::clicked, fuelPage, [fuelGraphSubWindow]() {
        fuelGraphSubWindow->hide();
    });
    connect(fuelGraphView, &QPushButton::clicked, fuelPage, [fuelGraphSubWindow]() {
        fuelGraphSubWindow->show();
        fuelGraphSubWindow->raise();
        fuelGraphSubWindow->setFocus();
    });
    connect(fuelSplitView, &QPushButton::clicked, fuelPage, [fuelGraphSubWindow]() {
        fuelGraphSubWindow->show();
        fuelGraphSubWindow->raise();
    });
    fuelLayout->addLayout(fuelWorkspace);
    
    tabs->addTab(fuelPage, tr("Fuel Table"));
    auto *ignitionPage = new QWidget(tabs);
    auto *ignitionLayout = new QVBoxLayout(ignitionPage);
    // Title is displayed by the MDI window.
    auto *ignitionActions = new QWidget(ignitionPage);
    auto *ignitionActionsLayout = new QHBoxLayout(ignitionActions);
    ignitionActionsLayout->setContentsMargins(0, 0, 0, 0);
    auto *ignitionRead = new QPushButton(tr("Read from ECU"), ignitionActions);
    auto *ignitionSave = new QPushButton(tr("Write to ECU"), ignitionActions);
    ignitionRead->setEnabled(false);
    ignitionSave->setEnabled(false);
    // Automatic ECU synchronization replaces the per-page read control.
    // Write is unavailable until ECU persistence is verified.
    ignitionActionsLayout->addStretch();
    ignitionActions->hide();
    auto *ignitionMap = new LiveMapTable(ignitionPage);
    ignitionMap->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ignitionMap->horizontalHeader()->setDefaultSectionSize(62);
    ignitionMap->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ignitionMap->verticalHeader()->setDefaultSectionSize(28);
    ignitionMap->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    ignitionMap->setAlternatingRowColors(true);
    fillDefaultTable(ignitionMap, ignitionDefaults, 1.0, ignitionMapBins);
    installMapHeatmap(ignitionMap);
    ignitionActionsLayout->insertStretch(2, 1);
    installTableEditControls(ignitionActionsLayout, ignitionMap, false);
    const auto ignitionFileActions = installMapFileActions(ignitionMap, QStringLiteral("ignition"), ignitionRpmBins, ignitionMapBins, 90.0);
    ignitionMap->setCornerButtonEnabled(false);
    ignitionMap->horizontalHeader()->setToolTip(tr("RPM (rev/min)"));
    ignitionMap->verticalHeader()->setToolTip(tr("MAP (kPa)"));
    // Keep the ignition grid resizable inside its workspace window.
    // A fixed 16-column pixel width previously prevented the user from resizing it.
    ignitionMap->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ignitionMap->setMinimumSize(480, 420);
    auto *ignitionGridPanel = new QWidget(ignitionPage);
    auto *ignitionGridLayout = new QVBoxLayout(ignitionGridPanel);
    ignitionGridLayout->setContentsMargins(0, 0, 0, 0);
    ignitionGridLayout->setSpacing(3);
    auto *ignitionAxisRow = new QHBoxLayout();
    ignitionAxisRow->setSpacing(5);
    // MAP axis label is positioned to the left of the grid.
    ignitionAxisRow->addWidget(ignitionMap, 1);
    auto *ignitionRpmAxis = new QLabel(tr("M\nA\nP\n\nk\nP\na"), ignitionGridPanel);
    ignitionRpmAxis->setAlignment(Qt::AlignCenter);
    ignitionRpmAxis->setFixedWidth(34);
    ignitionRpmAxis->setWordWrap(false);
    ignitionRpmAxis->setStyleSheet(QStringLiteral("font-weight: 600; color: #e69a32;"));
    ignitionAxisRow->insertWidget(0, ignitionRpmAxis, 0, Qt::AlignVCenter);
    ignitionGridLayout->addLayout(ignitionAxisRow, 1);
    ignitionMap->horizontalHeader()->hide();
    auto *ignitionRpmValues = new QTableWidget(1, 16, ignitionGridPanel);
    ignitionRpmValues->horizontalHeader()->hide();
    ignitionRpmValues->verticalHeader()->hide();
    ignitionRpmValues->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    ignitionRpmValues->setFocusPolicy(Qt::NoFocus);
    ignitionRpmValues->setSelectionMode(QAbstractItemView::NoSelection);
    ignitionRpmValues->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ignitionRpmValues->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ignitionRpmValues->horizontalHeader()->setDefaultSectionSize(ignitionMap->horizontalHeader()->defaultSectionSize());
    ignitionRpmValues->verticalHeader()->setDefaultSectionSize(28);
    ignitionRpmValues->setMinimumHeight(32);
    ignitionRpmValues->setMaximumHeight(32);
    ignitionRpmValues->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ignitionRpmValues->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    for (int i = 0; i < 16; ++i) {
        auto *cell = new QTableWidgetItem(ignitionRpmBins.at(i));
        cell->setTextAlignment(Qt::AlignCenter);
        ignitionRpmValues->setItem(0, i, cell);
    }
    auto *ignitionBottomAxis = new QHBoxLayout();
    ignitionBottomAxis->setContentsMargins(ignitionRpmAxis->sizeHint().width() + 5 + ignitionMap->verticalHeader()->width(), 0, 0, 0);
    ignitionBottomAxis->addWidget(ignitionRpmValues, 1);
    ignitionGridLayout->addLayout(ignitionBottomAxis);

    auto *ignitionMapAxis = new QLabel(tr("RPM (rev/min)"), ignitionGridPanel);
    ignitionMapAxis->setAlignment(Qt::AlignHCenter);
    ignitionMapAxis->setStyleSheet(QStringLiteral("font-weight: 600; color: #e69a32;"));
    ignitionGridLayout->addWidget(ignitionMapAxis);
    // Edit MAP breakpoints by double-clicking the vertical header.
    connect(ignitionMap->verticalHeader(), &QHeaderView::sectionDoubleClicked, ignitionPage,
            [ignitionMap](int row) {
        bool ok = false;
        const double current = ignitionMap->verticalHeaderItem(row)->text().toDouble();
        const double value = QInputDialog::getDouble(ignitionMap, QObject::tr("MAP breakpoint"),
            QObject::tr("MAP (kPa)"), current, 0.0, 500.0, 1, &ok);
        if (ok) ignitionMap->verticalHeaderItem(row)->setText(QString::number(value, 'f', 1));
    });
    auto *ignitionWorkspace = new QHBoxLayout();
    ignitionWorkspace->addWidget(ignitionGridPanel, 1);
    auto *ignitionSurface = new MapSurfacePreview(ignitionMap, ignitionPage);
    // The 3D graph has its own MDI window; the table remains independently visible.
    auto *ignitionGraphSubWindow = workspace->addSubWindow(ignitionSurface);
    ignitionGraphSubWindow->setWindowTitle(tr("Ignition | 3D Graph"));
    ignitionGraphSubWindow->setWindowFlags(Qt::SubWindow | Qt::WindowTitleHint |
                                      Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint |
                                      Qt::WindowCloseButtonHint);
    ignitionGraphSubWindow->setAttribute(Qt::WA_DeleteOnClose, false);
    ignitionGraphSubWindow->resize(450, 420);
    *ignitionGraphWindow = ignitionGraphSubWindow;
    boundsGuard->watch(ignitionGraphSubWindow);
    ignitionGraphSubWindow->move(75, 85);
    ignitionLayout->addLayout(ignitionWorkspace, 1);
    tabs->addTab(ignitionPage, tr("Ignition Table"));
    // AFR Target is an editable OFFLINE template only. Firmware page storage is not
    // implemented for this table, so it cannot be written to the ECU.
    auto *afrPage = new QWidget(tabs);
    auto *afrLayout = new QVBoxLayout(afrPage);
    // Title is displayed by the MDI window.
    auto *afrMap = new LiveMapTable(afrPage);
    afrMap->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    afrMap->horizontalHeader()->setDefaultSectionSize(62);
    afrMap->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    afrMap->verticalHeader()->setDefaultSectionSize(28);
    afrMap->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    afrMap->setAlternatingRowColors(true);
    afrMap->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    afrMap->setMinimumSize(480, 420);

    QStringList rpmHeaders, loadHeaders;
    for (int i = 0; i < 16; ++i) {
        rpmHeaders.append(afrRpmBins.at(i));
        loadHeaders.append(afrMapBins.at(15 - i));
    }
    afrMap->setVerticalHeaderLabels(loadHeaders);
    afrMap->horizontalHeader()->hide();
    fillDefaultTable(afrMap, afrDefaults, 10.0, afrMapBins);
    installMapHeatmap(afrMap);
    auto *afrEditBar = new QHBoxLayout();
    afrEditBar->addStretch();
    installTableEditControls(afrEditBar, afrMap, true);
    // Context menu owns the editing actions; no right-side editing buttons.
    const auto afrFileActions = installMapFileActions(afrMap, QStringLiteral("afr"), afrRpmBins, afrMapBins, 25.0);
    auto *afrGridPanel = new QWidget(afrPage);
    auto *afrGridLayout = new QVBoxLayout(afrGridPanel);
    afrGridLayout->setContentsMargins(0, 0, 0, 0);
    afrGridLayout->setSpacing(3);
    auto *afrAxisRow = new QHBoxLayout();
    afrAxisRow->setSpacing(5);
    auto *afrMapAxisLabel = new QLabel(tr("M\nA\nP\n\nk\nP\na"), afrGridPanel);
    afrMapAxisLabel->setAlignment(Qt::AlignCenter);
    afrMapAxisLabel->setFixedWidth(34);
    afrMapAxisLabel->setStyleSheet(QStringLiteral("font-weight: 600; color: #e69a32;"));
    afrAxisRow->addWidget(afrMapAxisLabel, 0, Qt::AlignVCenter);
    afrAxisRow->addWidget(afrMap, 1);
    afrGridLayout->addLayout(afrAxisRow, 1);
    auto *afrRpmValues = new QTableWidget(1, 16, afrGridPanel);
    afrRpmValues->horizontalHeader()->hide();
    afrRpmValues->verticalHeader()->hide();
    afrRpmValues->setEditTriggers(QAbstractItemView::NoEditTriggers);
    afrRpmValues->setFocusPolicy(Qt::NoFocus);
    afrRpmValues->setSelectionMode(QAbstractItemView::NoSelection);
    afrRpmValues->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    afrRpmValues->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    afrRpmValues->setMinimumHeight(32);
    afrRpmValues->setMaximumHeight(32);
    afrRpmValues->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    afrRpmValues->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    for (int i = 0; i < 16; ++i) {
        auto *cell = new QTableWidgetItem(afrRpmBins.at(i));
        cell->setTextAlignment(Qt::AlignCenter);
        afrRpmValues->setItem(0, i, cell);
    }
    auto *afrBottomAxis = new QHBoxLayout();
    afrBottomAxis->setContentsMargins(afrMapAxisLabel->width() + 5 + afrMap->verticalHeader()->width(), 0, 0, 0);
    afrBottomAxis->addWidget(afrRpmValues, 1);
    afrGridLayout->addLayout(afrBottomAxis);
    auto *afrRpmLabel = new QLabel(tr("RPM (rev/min)"), afrGridPanel);
    afrRpmLabel->setAlignment(Qt::AlignHCenter);
    afrRpmLabel->setStyleSheet(QStringLiteral("font-weight: 600; color: #e69a32;"));
    afrGridLayout->addWidget(afrRpmLabel);
    afrLayout->addWidget(afrGridPanel, 1);
    auto *afrFoot = new QLabel(tr("RPM (columns) / MAP kPa (rows). AFR assumes gasoline stoichiometric 14.7. No ECU AFR table protocol exists yet."), afrPage);
    afrFoot->setWordWrap(true);
    afrLayout->addWidget(afrFoot);
    tabs->addTab(afrPage, tr("AFR Target"));

    // Each map is an independent MDI window. Keep its tab index reserved so
    // existing navigation indices for the other configuration pages stay stable.
    const QVector<QWidget *> mapPages{fuelPage, ignitionPage, afrPage};
    const QStringList mapTitles{tr("Fuel Table"), tr("Ignition Table"), tr("AFR Target")};
    for (int index = 2; index >= 0; --index) {
        // QTabWidget hides the removed page. Detach it before handing
        // ownership to QMdiSubWindow, then explicitly restore visibility.
        tabs->removeTab(index);
        QWidget *page = mapPages.at(index);
        page->setParent(nullptr);
        auto *placeholder = new QWidget(tabs);
        auto *placeholderLayout = new QVBoxLayout(placeholder);
        auto *openMap = new QPushButton(tr("Show %1").arg(mapTitles.at(index)), placeholder);
        placeholderLayout->addWidget(openMap, 0, Qt::AlignCenter);
        tabs->insertTab(index, placeholder, mapTitles.at(index));
        auto *window = workspace->addSubWindow(page);
        window->setWindowTitle(mapTitles.at(index));
        window->setWindowFlags(Qt::SubWindow | Qt::WindowTitleHint |
                               Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint |
                               Qt::WindowCloseButtonHint);
        window->setAttribute(Qt::WA_DeleteOnClose, false);
        window->resize(1000, 740);
        window->move(35 + index * 35, 35 + index * 35);
        (*mapWindows)[index] = window;
        boundsGuard->watch(window);
        connect(openMap, &QPushButton::clicked, this, [workspace, window]() {
            window->showNormal();
            window->show();
            workspace->setActiveSubWindow(window);
        });
        if (index == 0) {
            page->show();
            window->show();
        } else {
            window->hide();
        }
    }
    // Startup: show Fuel Table only. All other maps and graphs remain available
    // through navigation, without creating the old configuration window.
    if (*fuelGraphWindow) (*fuelGraphWindow)->hide();
    if (*ignitionGraphWindow) (*ignitionGraphWindow)->hide();
    workspace->setActiveSubWindow((*mapWindows)[0]);
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
    auto *firmwareInfo = new QLabel(tr("USB DFU device management. Windows bootloader detection is available without external tools. Native DFU flashing is experimental. Always verify the firmware and preserve a full ECU backup before programming."), firmwarePage);
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
    auto *flashDfu = new QPushButton(tr("Flash and verify"), firmwarePage);
    flashDfu->setEnabled(true);
    auto *inspectBin = new QPushButton(tr("Validate firmware .bin"), firmwarePage);
    dfuButtons->addWidget(detectDfu);
    dfuButtons->addWidget(inspectBin);
    auto *prepareFlash = new QPushButton(tr("Prepare flash (no write)"), firmwarePage);
    dfuButtons->addWidget(prepareFlash);
    dfuButtons->addWidget(flashDfu);
    dfuButtons->addStretch();
    firmwareLayout->addLayout(dfuButtons);
    auto *dfuStatus = new QLabel(tr("Select the firmware .bin and detect USB DFU. Flash and verify requires a verified sector backup."), firmwarePage);
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
    connect(flashDfu, &QPushButton::clicked, firmwarePage, [=]() {
#ifdef Q_OS_WIN
        QFile firmware(imagePath->text());
        if (!firmware.open(QIODevice::ReadOnly)) {
            dfuLog->appendPlainText(tr("FLASH BLOCKED: select a firmware .bin."));
            return;
        }
        const QByteArray image = firmware.readAll();
        if (image.size() < 8 || image.size() > 1048576) {
            dfuLog->appendPlainText(tr("FLASH BLOCKED: invalid firmware size."));
            return;
        }
        const QString backupPath = QFileDialog::getOpenFileName(firmwarePage,
            tr("Select verified ECU sector backup"), QString(),
            tr("Binary backup (*.bin)"));
        if (backupPath.isEmpty()) return;
        if (QFileInfo(backupPath).absoluteFilePath() == QFileInfo(imagePath->text()).absoluteFilePath()) {
            dfuLog->appendPlainText(tr("FLASH BLOCKED: firmware and backup must be different files."));
            return;
        }
        QFile backupFile(backupPath);
        if (!backupFile.open(QIODevice::ReadOnly)) return;
        const QByteArray backup = backupFile.readAll();
        const QString imageHash = QString::fromLatin1(QCryptographicHash::hash(image,
            QCryptographicHash::Sha256).toHex());
        const QString backupHash = QString::fromLatin1(QCryptographicHash::hash(backup,
            QCryptographicHash::Sha256).toHex());
        const QString warning = tr("DANGER: This operation ERASES ECU Flash sectors and replaces the firmware.\n\n"
            "Firmware: %1\nBytes: %2\nSHA256: %3\n\n"
            "Backup: %4\nBytes: %5\nSHA256: %6\n\n"
            "Confirm that the firmware is intended for this ECU, that the engine and actuators are disconnected, "
            "and that the backup is stored safely. The ECU may require DFU recovery if interrupted.")
            .arg(QFileInfo(imagePath->text()).fileName()).arg(image.size()).arg(imageHash)
            .arg(QFileInfo(backupPath).fileName()).arg(backup.size()).arg(backupHash);
        if (QMessageBox::warning(firmwarePage, tr("Confirm destructive ECU Flash operation"),
            warning, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
            return;
        flashDfu->setEnabled(false);
        detectDfu->setEnabled(false);
        dfuLog->appendPlainText(tr("Starting native DFU flash; do not disconnect power or USB."));
        const bool success = TunuinoDfu::flash(image, backup,
            [dfuLog](const QString &message) { dfuLog->appendPlainText(message); });
        dfuStatus->setText(success ? tr("Flash and verify PASS.")
            : tr("Flash blocked or failed. See DFU log; do not assume ECU is programmed."));
        flashDfu->setEnabled(true);
        detectDfu->setEnabled(true);
#else
        dfuLog->appendPlainText(tr("Native USB DFU flashing is Windows-only."));
#endif
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
        dfuLog->appendPlainText(tr("Preparation is read-only. Backup selection and verification occur during Flash and verify."));
        dfuLog->appendPlainText(tr("Firmware SHA256: %1")
            .arg(QString::fromLatin1(QCryptographicHash::hash(bytes,
                QCryptographicHash::Sha256).toHex())));
        dfuLog->appendPlainText(tr("Preparation only: no erase/write attempted. This plan does not prove board compatibility or backup completeness."));
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
            ? tr("BIN vector preflight passed. This does NOT prove ECU compatibility.")
            : tr("BIN vector preflight FAILED. Do not flash this image."));
    });
    connect(detectDfu, &QPushButton::clicked, firmwarePage, [=]() {
        dfuPort->clear();
        flashDfu->setEnabled(true);
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
            ? tr("STM32 USB DFU bootloader detected by Windows. Flash and verify requires a verified backup.")
            : tr("No STM32 DFU bootloader (VID 0483 / PID DF11) found. Check bootloader mode and USB driver."));
#else
        dfuStatus->setText(tr("Native DFU detection is available on Windows only."));
#endif
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
    QPushButton *tpsReadButton = nullptr;
    QLabel *tpsReadStatus = nullptr;
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
                auto *readsaveTps = new QPushButton(tr("Read from ECU"), page);
                tpsReadButton = readsaveTps;
                tpsReadStatus = info;
                readsaveTps->setEnabled(false);
                readsaveTps->setToolTip(tr("Requires validated ECU configuration read protocol."));
    // Automatic ECU synchronization replaces the per-page read control.
                auto *saveTps = new QPushButton(tr("Write to ECU"), page);
                saveTps->setEnabled(false);
                saveTps->setToolTip(tr("Not available: ECU configuration write and persistent storage protocol must be implemented and validated. This control does not save to the PC."));
                form->addRow(saveTps);
                // ECU write action is deliberately unavailable until the persistent
                // configuration protocol has been implemented and validated.
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
                auto *readsaveStrategy = new QPushButton(tr("Read from ECU"), page);
                readsaveStrategy->setEnabled(false);
                readsaveStrategy->setToolTip(tr("Requires validated ECU configuration read protocol."));
                form->addRow(readsaveStrategy);
                auto *saveStrategy = new QPushButton(tr("Write to ECU"), page);
                saveStrategy->setEnabled(false);
                saveStrategy->setToolTip(tr("Not available: ECU configuration write and persistent storage protocol must be implemented and validated. This control does not save to the PC."));
                form->addRow(saveStrategy);
                // ECU write action is deliberately unavailable until the persistent
                // configuration protocol has been implemented and validated.
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
                    auto *readsaveWheel = new QPushButton(tr("Read from ECU"), page);
                readsaveWheel->setEnabled(false);
                readsaveWheel->setToolTip(tr("Requires validated ECU configuration read protocol."));
                form->addRow(readsaveWheel);
                auto *saveWheel = new QPushButton(tr("Write to ECU"), page);
                saveWheel->setEnabled(false);
                saveWheel->setToolTip(tr("Not available: ECU configuration write and persistent storage protocol must be implemented and validated. This control does not save to the PC."));
                    form->addRow(saveWheel);
                    // ECU write action is deliberately unavailable until the persistent
                    // configuration protocol has been implemented and validated.
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
                auto *readsave = new QPushButton(tr("Read from ECU"), page);
                readsave->setEnabled(false);
                readsave->setToolTip(tr("Requires validated ECU configuration read protocol."));
                form->addRow(readsave);
                auto *save = new QPushButton(tr("Write to ECU"), page);
                save->setEnabled(false);
                save->setToolTip(tr("Not available: ECU configuration write and persistent storage protocol must be implemented and validated. This control does not save to the PC."));
                form->addRow(save);
                // ECU write action is deliberately unavailable until the persistent
                // configuration protocol has been implemented and validated.
                auto *read = new QPushButton(tr("Read from ECU"), page);
                read->setEnabled(false);
                read->setToolTip(tr("Requires validated ECU configuration read protocol."));
                form->addRow(read);
                auto *apply = new QPushButton(tr("Write to ECU"), page);
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
    // Navigation and the main menu select pages; do not duplicate them as inner tabs.
    tabs->tabBar()->hide();
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
    // Read only validated, real telemetry values from the Engine Status panel.
    // Invalid or disconnected telemetry removes the crosshair rather than simulating it.
    auto *operatingPointTimer = new QTimer(this);
    operatingPointTimer->setInterval(150);
    connect(operatingPointTimer, &QTimer::timeout, this,
            [maps, ignitionMap, afrMap, liveReadings, fuelRpmBins, ignitionRpmBins, afrRpmBins]() {
        bool rpmOk = false, mapOk = false;
        const double rpm = liveReadings->at(0)->text().section(QLatin1Char(' '), 0, 0).toDouble(&rpmOk);
        const QString mapText = liveReadings->at(2)->text();
        const double kpa = mapText.section(QLatin1Char(' '), 0, 0).toDouble(&mapOk);
        if (!rpmOk || !mapOk) {
            maps->clearOperatingPoint();
            ignitionMap->clearOperatingPoint();
            afrMap->clearOperatingPoint();
            return;
        }
        maps->setOperatingPoint(rpm, kpa, fuelRpmBins);
        ignitionMap->setOperatingPoint(rpm, kpa, ignitionRpmBins);
        afrMap->setOperatingPoint(rpm, kpa, afrRpmBins);
    });
    operatingPointTimer->start();

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
    *engineStatusDockRef = dock;
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
    auto autoSync = QSharedPointer<bool>::create(false);
    auto sendChunk = [session, transfer]() -> bool {
        const int total = transfer->page == 1 ? 256 : 576;
        if (!transfer->page || transfer->offset >= total) return false;
        const int count = qMin(64, total - transfer->offset);
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
        *autoSync = false;
        transfer->page = 0;
        transfer->offset = 0;
        transfer->bytes.clear();
        fuelRead->setEnabled(*identityVerified && session->isOpen());
        ignitionRead->setEnabled(*identityVerified && session->isOpen());
        if (tpsReadButton) tpsReadButton->setEnabled(*identityVerified && session->isOpen());
        // Transfer feedback is displayed in the status bar.
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
        if (tpsReadButton) tpsReadButton->setEnabled(false);
        // Transfer progress is displayed in the status bar.
        statusBar()->showMessage(tr("Reading ECU page %1...").arg(page));
        if (!sendChunk()) abortTransfer(tr("Unable to request map page"));
    };
    if (tpsReadButton) connect(tpsReadButton, &QPushButton::clicked, this, [=]() { startTransfer(1); });
    connect(fuelRead, &QPushButton::clicked, this, [=]() { startTransfer(2); });
    connect(ignitionRead, &QPushButton::clicked, this, [=]() { startTransfer(3); });
    auto *fileMenu = menuBar()->addMenu(tr("File"));
    auto *openProject = fileMenu->addAction(tr("Open Map..."));
    openProject->setShortcut(QKeySequence::Open);
    auto *saveProject = fileMenu->addAction(tr("Save Map..."));
    saveProject->setShortcut(QKeySequence::Save);
    // File actions operate on the active map, never on the example preview.
    const auto activeMapActions = [workspace, tabs, mapWindows, fuelPage, ignitionPage, afrPage, fuelFileActions, ignitionFileActions, afrFileActions]() {
        const auto *active = workspace->activeSubWindow();
        const QWidget *page = active ? active->widget() : tabs->currentWidget();
        if (page == ignitionPage) return ignitionFileActions;
        if (page == afrPage) return afrFileActions;
        return fuelFileActions;
    };
    connect(openProject, &QAction::triggered, this, [activeMapActions]() { activeMapActions().first->trigger(); });
    connect(saveProject, &QAction::triggered, this, [activeMapActions]() { activeMapActions().second->trigger(); });
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
    auto *baseMapsMenu = menuBar()->addMenu(tr("Base Maps"));
    auto *openBaseMaps = baseMapsMenu->addAction(tr("Open Base Maps"));
    connect(openBaseMaps, &QAction::triggered, this, [openSettingsPage]() { openSettingsPage(3); });
    auto *viewMenu = menuBar()->addMenu(tr("View"));
    viewMenu->addAction(navigationDock->toggleViewAction());
    viewMenu->addAction(inspectorDock->toggleViewAction());
    viewMenu->addAction(dock->toggleViewAction());
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
    connect(calibrationAction, &QAction::triggered, this, [openSettingsPage]() { openSettingsPage(6); });
    auto *helpMenu = menuBar()->addMenu(tr("Help"));
    auto *checkUpdates = helpMenu->addAction(tr("Check for updates..."));
    connect(checkUpdates, &QAction::triggered, this, [this, checkUpdates]() {
        const QString channel = QSettings(QStringLiteral("Tunuino"), QStringLiteral("TunuinoConsole"))
            .value(QStringLiteral("updates/channel"), QStringLiteral("beta")).toString() == QLatin1String("stable")
            ? QStringLiteral("stable") : QStringLiteral("beta");
        // Public HTTPS manifest: no GitHub login or token is used by the Console.
        const QUrl manifestUrl(QStringLiteral("https://tunuino.github.io/Tunuino/updates/") + channel + QStringLiteral(".json"));
        auto *network = new QNetworkAccessManager(this);
        checkUpdates->setEnabled(false);
        QNetworkRequest manifestRequest(manifestUrl);
        manifestRequest.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
        manifestRequest.setRawHeader("Cache-Control", "no-cache, no-store, max-age=0");
        manifestRequest.setRawHeader("Pragma", "no-cache");
        auto *reply = network->get(manifestRequest);
        connect(reply, &QNetworkReply::finished, this, [this, reply, network, checkUpdates, channel]() {
            checkUpdates->setEnabled(true);
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
                QMessageBox::warning(this, tr("Console updates"),
                    tr("The public %1 update feed is not available yet. No sign-in is required.").arg(channel));
                network->deleteLater();
                return;
            }
            const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
            const QJsonObject item = document.object();
            const QString version = item.value(QStringLiteral("version")).toString();
            const QUrl packageUrl(item.value(QStringLiteral("url")).toString());
            const QByteArray expectedHash = item.value(QStringLiteral("sha256")).toString().toLatin1().toLower();
            const bool valid = !version.isEmpty() && QVersionNumber::fromString(version).segmentCount() > 0
                && packageUrl.isValid() && packageUrl.scheme() == QLatin1String("https")
                && expectedHash.size() == 64 &&
                std::all_of(expectedHash.cbegin(), expectedHash.cend(), [](char c) {
                    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
                });
            if (!valid) {
                QMessageBox::warning(this, tr("Console updates"), tr("Invalid update manifest. Installation cancelled."));
                network->deleteLater();
                return;
            }
            const QVersionNumber installed = QVersionNumber::fromString(QCoreApplication::applicationVersion());
            if (installed.segmentCount() && QVersionNumber::compare(QVersionNumber::fromString(version), installed) <= 0) {
                QMessageBox::information(this, tr("Console updates"),
                    tr("No newer %1 update is published.\nInstalled: %2\nPublished: %3\n\nThe Windows Setup artifact may be newer than the public update feed.")
                        .arg(channel, QCoreApplication::applicationVersion(), version));
                network->deleteLater();
                return;
            }
            if (QMessageBox::question(this, tr("Console update available"),
                    tr("Download and install Console %1 (%2)? Firmware will not be changed.").arg(version, channel))
                != QMessageBox::Yes) {
                network->deleteLater();
                return;
            }
            auto *download = network->get(QNetworkRequest(packageUrl));
            connect(download, &QNetworkReply::finished, this, [this, download, network, expectedHash, version]() {
                download->deleteLater();
                network->deleteLater();
                const QByteArray bytes = download->readAll();
                if (download->error() != QNetworkReply::NoError || bytes.isEmpty()
                    || QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex() != expectedHash) {
                    QMessageBox::warning(this, tr("Console updates"), tr("Download failed or SHA-256 verification failed. Nothing was installed."));
                    return;
                }
                const QString folder = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                    + QStringLiteral("/updates");
                if (!QDir().mkpath(folder)) {
                    QMessageBox::warning(this, tr("Console updates"), tr("Cannot create the update directory."));
                    return;
                }
                const QString installer = folder + QStringLiteral("/Tunuino-Console-") + version + QStringLiteral(".exe");
                QSaveFile output(installer);
                if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit()) {
                    QMessageBox::warning(this, tr("Console updates"), tr("Cannot save the verified installer."));
                    return;
                }
                if (QMessageBox::question(this, tr("Install Console update"),
                        tr("The verified installer is ready. Close the Console and launch installation now?"))
                    == QMessageBox::Yes) {
                    if (QProcess::startDetached(installer, QStringList()))
                        QCoreApplication::quit();
                    else
                        QMessageBox::warning(this, tr("Console updates"), tr("Could not start the installer."));
                }
            });
        });
    });
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
    auto *updatesMenu = settingsMenu->addMenu(tr("Update channel"));
    auto *channelActions = new QActionGroup(this);
    channelActions->setExclusive(true);
    auto *betaChannel = updatesMenu->addAction(tr("Beta — test builds"));
    auto *stableChannel = updatesMenu->addAction(tr("Stable — validated releases"));
    for (auto *action : {betaChannel, stableChannel}) {
        action->setCheckable(true);
        channelActions->addAction(action);
    }
    QSettings updateSettings(QStringLiteral("Tunuino"), QStringLiteral("TunuinoConsole"));
    const QString initialChannel = updateSettings.value(QStringLiteral("updates/channel"),
        QStringLiteral("beta")).toString();
    (initialChannel == QLatin1String("stable") ? stableChannel : betaChannel)->setChecked(true);
    connect(betaChannel, &QAction::triggered, this, []() {
        QSettings(QStringLiteral("Tunuino"), QStringLiteral("TunuinoConsole"))
            .setValue(QStringLiteral("updates/channel"), QStringLiteral("beta"));
    });
    connect(stableChannel, &QAction::triggered, this, []() {
        QSettings(QStringLiteral("Tunuino"), QStringLiteral("TunuinoConsole"))
            .setValue(QStringLiteral("updates/channel"), QStringLiteral("stable"));
    });
    auto *refreshAction = settingsMenu->addAction(tr("Refresh serial ports"));
    auto *openFirmware = ecuMenu->addAction(tr("Firmware / USB DFU"));
    connect(openFirmware, &QAction::triggered, this, [tabs, firmwarePage]() {
        tabs->setCurrentWidget(firmwarePage);
    });
    auto *dfuInfo = ecuMenu->addAction(tr("USB DFU status / instructions"));
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
                *autoSync = false;
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
        *autoSync = false;
        session->close();
        connectionStatus_->setText(tr("ECU disconnected"));
    });
    connect(session, &tunuino::SerialSession::connectionChanged, this, [=](bool connected) {
        disconnectAction->setEnabled(connected);
        if (!connected) { *autoSync = false; liveTimer->stop(); transfer->page = 0; *identityVerified = false; *validationPending = false; validationAction->setEnabled(false); fuelRead->setEnabled(false); ignitionRead->setEnabled(false); if (tpsReadButton) tpsReadButton->setEnabled(false); connectionStatus_->setText(tr("ECU disconnected")); }
    });
    connect(session, &tunuino::SerialSession::exchangeFailed, this, [=](const QString &error) {
        liveTimer->stop();
        transfer->page = 0;
        *autoSync = false;
        *identityVerified = false;
        *validationPending = false;
        validationAction->setEnabled(false);
        fuelRead->setEnabled(false);
        ignitionRead->setEnabled(false);
        if (tpsReadButton) tpsReadButton->setEnabled(false);
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
            const int total = transfer->page == 1 ? 256 : 576;
            const int count = qMin(64, total - transfer->offset);
            if (data.size() != count + 1 || quint8(data[0]) != 0) {
                abortTransfer(tr("Invalid map response; table not updated"));
                return;
            }
            transfer->bytes.append(data.constData() + 1, count);
            transfer->offset += count;
            if (transfer->offset < total) {
                if (!sendChunk()) abortTransfer(tr("Map read interrupted"));
                return;
            }
            if (transfer->bytes.size() != total) {
                abortTransfer(tr("Incomplete map read"));
                return;
            }
            const auto u16 = [transfer](int offset) -> quint16 {
                return (quint16(quint8(transfer->bytes[offset])) << 8)
                    | quint16(quint8(transfer->bytes[offset + 1]));
            };
            if (transfer->page == 1) {
                const bool schemaValid = u16(0) != 0;
                if (tpsReadStatus) {
                    tpsReadStatus->setText(schemaValid
                        ? tr("ECU configuration page read successfully (schema %1). TPS closed/open calibration is not yet represented in firmware; reference fields remain local and unchanged.").arg(u16(0))
                        : tr("ECU configuration schema invalid. TPS references unchanged."));
                }
                if (schemaValid && *autoSync) {
                    transfer->page = 0;
                    transfer->offset = 0;
                    transfer->bytes.clear();
                    QTimer::singleShot(0, this, [=]() { startTransfer(2); });
                } else {
                    abortTransfer(schemaValid ? tr("ECU configuration read (read-only)") : tr("Invalid ECU configuration schema"));
                }
                return;
            }
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
            // Rows are MAP load (kPa); columns are engine speed (RPM).
            // Firmware storage is load-major: cells[load][rpm].
            auto *rpmValues = (table == maps) ? fuelRpmValues : ignitionRpmValues;
            for (int i = 0; i < 16; ++i)
                rpmValues->item(0, i)->setText(rpmLabels.at(i));
            std::reverse(loadLabels.begin(), loadLabels.end());
            table->setVerticalHeaderLabels(loadLabels);
            for (int row = 0; row < 16; ++row)
                for (int col = 0; col < 16; ++col)
                    table->item(15 - row, col)->setText(QString::number(qint16(u16(64 + 2 * (row * 16 + col))) / 100.0, 'f', 2));
            // Force the independent 3D view to repaint after the complete ECU map arrives.
            (page == 2 ? fuelSurface : ignitionSurface)->update();
            if (*autoSync && page == 2) {
                transfer->page = 0;
                transfer->offset = 0;
                transfer->bytes.clear();
                QTimer::singleShot(0, this, [=]() { startTransfer(3); });
            } else {
                const bool completed = *autoSync && page == 3;
                abortTransfer(completed
                    ? tr("ECU synchronized: configuration, Fuel and Ignition maps. Other parameters are not yet supported.")
                    : tr("Map page %1 read from ECU (read-only)").arg(page));
            }
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
            // A stopped engine has zero RPM even when the trigger decoder has no sync.
            // Only show zero if the ECU explicitly reports zero RPM and no trigger errors;
            // never turn a nonzero, unvalidated reading into a valid RPM value.
            const quint32 rpmMilli = u32(1 + 40);
            const quint32 triggerErrors = u32(1 + 96);
            const bool stoppedEngine = !(mask & 1) && rpmMilli == 0 && triggerErrors == 0;
            setReading(0, (mask & 1) || stoppedEngine,
                QString::number(rpmMilli / 1000.0, 'f', 0) + QStringLiteral(" rpm"));
            setReading(2, mask & 2, QString::number(u32(1 + 44) / 1000.0, 'f', 1) + QStringLiteral(" kPa"));
            setReading(3, mask & 4, QString::number(u32(1 + 48) / 1000000.0, 'f', 3));
            setReading(4, mask & 8, QString::number(qint32(u32(1 + 52)) / 1000.0, 'f', 1) + QStringLiteral(" C"));
            // The current runtime protocol has no calibrated TPS, IAT or battery fields.
            // Never manufacture these readings from unrelated raw ADC channels.
            setReading(1, false, QString());
            setReading(5, false, QString());
            setReading(6, false, QString());
            const quint8 flags = quint8(data[1 + 39]);
            const quint32 uptimeSeconds = u32(1 + 32);
            statusBar()->showMessage(tr("ECU live: uptime %1 s | ADC %2 | trigger %3 | valid channels 0x%4 | trigger errors %5")
                .arg(uptimeSeconds)
                .arg((flags & 1) ? tr("OK") : tr("unavailable"))
                .arg((flags & 8) ? tr("synchronised") : tr("not synchronised"))
                .arg(QString::number(mask, 16))
                .arg(triggerErrors));
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
        *autoSync = true;
        validationAction->setEnabled(true);
        fuelRead->setEnabled(true);
        ignitionRead->setEnabled(true);
        if (tpsReadButton) tpsReadButton->setEnabled(true);
        startTransfer(1);
        statusBar()->showMessage(tr("ECU identity verified. Fuel and Ignition map reading available; ECU writes disabled."));
    });
    refreshPorts();
    statusBar()->showMessage(tr("TUNUINO  |  ECU connection ready  |  Connect ECU to read maps; map writing disabled"));
}
