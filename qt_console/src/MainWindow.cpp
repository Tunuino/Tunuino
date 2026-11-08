#include "MainWindow.h"
#include "ui/CalibrationAppearance.h"
#include "project/MapProject.h"
#include "NativeDfuFlash.h"
#include "logging/GraphLogWidget.h"
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
#include <QToolButton>
#include <QResizeEvent>
#include <QFontMetrics>
#include <QStyle>
#include <functional>
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
#include <QElapsedTimer>
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
#include "ecu/AutoConnection.h"
#include "ecu/EcuPowerState.h"
#include "ecu/MapWriteTransaction.h"
#include "protocol/MapPageCodec.h"
#include "calibration/SensorCalibrationWidget.h"
#include "calibration/SensorNavigation.h"
#include "calibration/SensorInputsWidget.h"
#include "calibration/VssMonitor.h"
#include "calibration/SensorCurveWizard.h"
#include "parameters/ParameterWidget.h"
#include "parameters/TriggerWizard.h"
#include "parameters/TriggerPresetDialog.h"
#include "display/TuneDisplayWidget.h"
#include "tools/ToolsSuite.h"
#include "tools/ScopeWidget.h"
#include "tools/OutputTestWidget.h"
#include "tools/AutotuneWidget.h"
#include "tools/ServiceDataWidget.h"
#include <QDateTime>
#include <QUuid>
#include <QCloseEvent>

// Live engine operating-point crosshair; visual overlay only, never edits cells.
class LiveMapTable final : public QTableWidget {
public:
    explicit LiveMapTable(QWidget *parent) : QTableWidget(16, 16, parent) {}
    void setOperatingPoint(double rpm, double kpa, const QStringList &rpmBins) {
        if (property("followCursorEnabled").isValid() && !property("followCursorEnabled").toBool()) { clearOperatingPoint(); return; }
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
    void resizeEvent(QResizeEvent *event) override {
        QTableWidget::resizeEvent(event);
        // Adapt typography to actual cell width, not the MDI window size.
        // A fixed 9pt bold font was eliding values as "..." in compact maps.
        const int cellWidth = (std::max)(1, viewport()->width() / (std::max)(1, columnCount()));
        const int cellHeight = (std::max)(1, viewport()->height() / (std::max)(1, rowCount()));
        QFont valueFont = font();
        valueFont.setBold(true);
        int points = 11;
        for (; points > 4; --points) {
            valueFont.setPointSize(points);
            const QFontMetrics fm(valueFont);
            if (fm.horizontalAdvance(QStringLiteral("9999")) + 3 <= cellWidth &&
                fm.height() + 2 <= cellHeight) break;
        }
        valueFont.setPointSize((std::max)(4, points));
        if (font().pointSize() != valueFont.pointSize()) setFont(valueFont);
        QFont headerFont = verticalHeader()->font();
        headerFont.setPointSize((std::max)(5, (std::min)(9, valueFont.pointSize())));
        verticalHeader()->setFont(headerFont);
    }
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
        if (!w || !w->isVisible() || w->isMaximized() || w->isMinimized() || adjusting_) return;
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
    setWindowTitle("Tunuino Console");
    resize(1400, 850);
    // First MDI migration stage: retain the existing tab controller and ECU
    // connections, but host it in a movable, resizable, closable workspace window.
    auto *workspace = new QMdiArea(this);
    workspace->setObjectName(QStringLiteral("TunuinoWorkspace"));
    workspace->setViewMode(QMdiArea::SubWindowView);
    workspace->setBackground(QBrush(QColor(43, 46, 51)));
    setCentralWidget(workspace);
    auto *boundsGuard = new WorkspaceBoundsGuard(workspace);
    auto toolBusy=QSharedPointer<bool>::create(false);
    auto toolReply=QSharedPointer<std::function<void(const QByteArray&)>>::create();
    auto toolRuntime=QSharedPointer<bool>::create(false);
    auto toolLease=QSharedPointer<quint64>::create(0);
    // Hidden page registry only; no legacy "Tables and configuration" MDI window.
    auto *tabs = new QTabWidget(this);
    tabs->hide();
    auto settingsWindows = QSharedPointer<QVector<QPointer<QMdiSubWindow>>>::create(6);
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
            auto *window = workspace->addSubWindow(page);tunuino::CalibrationAppearance::frame(window);
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
    auto tuneWindow=QSharedPointer<QPointer<QMdiSubWindow>>::create();
    auto parameterNavigation=QSharedPointer<std::function<void(QString)>>::create();
    auto toolNavigation=QSharedPointer<std::function<void(int)>>::create();
    const auto openTuneDisplay=[=]{
        if(!*tuneWindow)return;auto *window=tuneWindow->data();
        if(window->isMinimized())window->showNormal();else window->show();
        window->widget()->show();workspace->setActiveSubWindow(window);window->raise();
    };
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
    navigationTree->setObjectName("MainNavigation");navigationTree->setStyleSheet("QTreeWidget{font-family:'Segoe UI';font-size:10pt;}QTreeWidget::item{padding:1px 0;min-height:21px;}QTreeWidget::item:selected{background:#168bd2;color:white;}");navigationTree->setIconSize(QSize(18,18));
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
    auto *sensorsCategory=addCategory(tr("Sensors and inputs"),{});
    const auto parameterItem=[=](QTreeWidgetItem *parent,const QString &name,const QString &key){auto *item=new QTreeWidgetItem(parent,{name});item->setIcon(0,tunuino::CalibrationAppearance::icon(key.endsWith("wizard")?tunuino::CalibrationAppearance::Wizard:tunuino::CalibrationAppearance::Parameters));item->setData(0,Qt::UserRole,100);item->setData(0,Qt::UserRole+1,key);return item;};
    tunuino::populateSensorNavigation(sensorsCategory);
    addCategory(tr("Engine start"), {{tr("Cranking"),-1},{tr("Afterstart"),-1}});
    auto *fuelingCategory=addCategory(tr("Fueling"),{{tr("Autotune"),12}});
    parameterItem(fuelingCategory,tr("General"),"fuel.general");parameterItem(fuelingCategory,tr("Fuel cut"),"fuel.cut");
    const auto unavailable=[=](QTreeWidgetItem *parent,const QString &title,const QString &reason){auto *item=new QTreeWidgetItem(parent,{title});item->setFlags(item->flags()&~Qt::ItemIsEnabled);item->setToolTip(0,reason);return item;};
    parameterItem(fuelingCategory,tr("Lambda guard"),"fuel.guard");
    parameterItem(fuelingCategory,tr("Acceleration"),"fuel.acceleration");
    auto *fuelCorrections=new QTreeWidgetItem(fuelingCategory,{tr("Corrections")});parameterItem(fuelCorrections,tr("Warmup"),"fuel.corrections");
    parameterItem(fuelingCategory,tr("Cylinders trim"),"fuel.trim");parameterItem(fuelingCategory,tr("Short term trim"),"fuel.shortterm");
    auto *fuelTables=new QTreeWidgetItem(fuelingCategory,{tr("Fuel tables")});
    auto *ve=new QTreeWidgetItem(fuelTables,{tr("Fuel Table")});ve->setData(0,Qt::UserRole,0);
    auto *fuelGraphItem=new QTreeWidgetItem(fuelTables,{tr("Fuel 3D Graph")});fuelGraphItem->setData(0,Qt::UserRole,7);
    parameterItem(fuelTables,tr("Target lambda"),"fuel.target");auto *afrItem=new QTreeWidgetItem(fuelTables,{tr("AFR Target (offline)")});afrItem->setData(0,Qt::UserRole,2);
    parameterItem(fuelingCategory,tr("Flex fuel"),"fuel.flex");
    auto *injectors=new QTreeWidgetItem(fuelingCategory,{tr("Injectors")});parameterItem(injectors,tr("Injector outputs"),"injectors.outputs");parameterItem(injectors,tr("Injector dead time"),"injectors.deadtime");parameterItem(injectors,tr("Voltage compensation"),"voltage.compensation");
    unavailable(fuelingCategory,tr("Secondary injectors"),tr("Not implemented: staged secondary injection."));
    auto *ignitionCategory=addCategory(tr("Ignition"),{});
    parameterItem(ignitionCategory,tr("Trigger wizard"),"ignition.wizard");
    parameterItem(ignitionCategory,tr("Firing order"),"ignition.firing");
    auto *coils=new QTreeWidgetItem(ignitionCategory,{tr("Coils")});parameterItem(coils,tr("Ignition outputs"),"coils.outputs");parameterItem(coils,tr("Coil dwell time"),"coils.dwell");parameterItem(coils,tr("Voltage compensation"),"voltage.compensation");
    auto *sparkCorrections=new QTreeWidgetItem(ignitionCategory,{tr("Corrections")});parameterItem(sparkCorrections,tr("IAT"),"ignition.iat");parameterItem(sparkCorrections,tr("Cylinders trim"),"ignition.trim");
    auto *ignTables=new QTreeWidgetItem(ignitionCategory,{tr("Ign. tables")});auto *ignTable=new QTreeWidgetItem(ignTables,{tr("Ignition Table")});ignTable->setData(0,Qt::UserRole,1);auto *ignGraphItem=new QTreeWidgetItem(ignTables,{tr("Ignition 3D Graph")});ignGraphItem->setData(0,Qt::UserRole,8);
    parameterItem(ignitionCategory,tr("Rev limiters"),"ignition.limit");auto *triggers=new QTreeWidgetItem(ignitionCategory,{tr("Triggers")});parameterItem(triggers,tr("Primary trigger"),"ignition.primary");parameterItem(triggers,tr("Phase"),"ignition.phase");
    for(auto *group:{coils,sparkCorrections,ignTables,triggers,injectors,fuelCorrections,fuelTables})group->setIcon(0,tunuino::CalibrationAppearance::icon(tunuino::CalibrationAppearance::Group));
    addCategory(tr("Overrun"), {{tr("Fuel cut"),-1}});
    addCategory(tr("Knock sensors"), {{tr("Knock settings"),-1}});
    addCategory(tr("Idle"), {{tr("Idle control"),-1}});
    addCategory(tr("Functions"), {{tr("General functions"),-1}});
    addCategory(tr("Outputs"), {{tr("Output configuration"),-1}});
    addCategory(tr("Boost"), {{tr("Boost control"),-1}});
    addCategory(tr("DBW"), {{tr("Electronic throttle"),-1}});
    addCategory(tr("Traction control"), {{tr("Traction settings"),-1}});
    addCategory(tr("VVT"), {{tr("Cam timing"),-1}});
    addCategory(tr("Sport"), {{tr("Sport mode"),-1}});
    addCategory(tr("Nitrous"), {{tr("Nitrous control"),-1}});
    addCategory(tr("Tables switching"), {{tr("Table selection"),-1}});
    addCategory(tr("Engine protection"), {{tr("Protection settings"),-1}});
    addCategory(tr("Timers"), {{tr("Timers setup"),-1}});
    addCategory(tr("DSG Gearbox"), {{tr("Gearbox settings"),-1}});
    addCategory(tr("CAN, Serial"), {{tr("Communication settings"),-1}});
    addCategory(tr("Log"), {{tr("Scope"),11},{tr("Graph Log"),4},{tr("Tune Display"),10},{tr("Autotune"),12}});
    addCategory(tr("Gauges"), {{tr("Runtime gauges"),-1}});
    auto engineStatusDockRef = QSharedPointer<QPointer<QDockWidget>>::create();
    connect(navigationTree, &QTreeWidget::itemClicked, tabs, [tabs, workspace, openSettingsPage, fuelGraphWindow, ignitionGraphWindow, engineStatusDockRef, mapWindows, openTuneDisplay, toolNavigation, parameterNavigation](QTreeWidgetItem *item, int) {
        if(!item->data(0,Qt::UserRole).isValid()){item->setExpanded(!item->isExpanded());return;}
        const int index = item->data(0, Qt::UserRole).toInt();
        if(item->data(0,Qt::UserRole+1).isValid()){if(*parameterNavigation)(*parameterNavigation)(item->data(0,Qt::UserRole+1).toString());}
        else if(item->parent()&&(index==11||index==12)){if(*toolNavigation)(*toolNavigation)(index);}
        else if(item->parent()&&index==10){openTuneDisplay();}
        else if (item->parent() && index == 9) {
            if (*engineStatusDockRef) { (*engineStatusDockRef)->show(); (*engineStatusDockRef)->raise(); }
        } else if (item->parent() && (index == 7 || index == 8)) {
            auto window = (index == 7) ? *fuelGraphWindow : *ignitionGraphWindow;
            if (window) { window->showNormal(); if (window->widget()) window->widget()->show(); window->update(); workspace->setActiveSubWindow(window); }
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
        std::function<bool(QTreeWidgetItem*,bool)> filter=[&](QTreeWidgetItem *item,bool inherited){const bool own=inherited||term.isEmpty()||item->text(0).contains(term,Qt::CaseInsensitive);bool match=own;for(int i=0;i<item->childCount();++i)match=filter(item->child(i),own)||match;item->setHidden(!match);if(!term.isEmpty()&&match)item->setExpanded(true);return match;};
        for(int i=0;i<navigationTree->topLevelItemCount();++i){auto *category=navigationTree->topLevelItem(i);const bool match=filter(category,false);category->setHidden(!match||!QSettings().value("tools/strategies/"+category->text(0),true).toBool());}
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
    auto *inspectorSafety = new QLabel(tr("Calibration writes require a fresh ECU read, stopped engine and disarmed outputs."), inspectorBody);
    inspectorSafety->setWordWrap(true);
    inspectorSafety->setStyleSheet(QStringLiteral("color: #b8b8b8;"));
    inspectorLayout->addWidget(inspectorSafety);
    inspectorLayout->addStretch(1);
    inspectorDock->setWidget(inspectorBody);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock);
    inspectorDock->hide();
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

    // Calibration workspace; explicit map writes are provided by the ECU menu.
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
    fuelSave->setToolTip(tr("Write the active Fuel Table through ECU > Write to ECU (F2)."));
    // Automatic ECU synchronization replaces the per-page read control.
    // Writing is owned by the verified map transaction in the ECU menu.
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
        valueFont.setPointSize(9);
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
        auto *openAction = new QAction(tr("Import table"), this);
        auto *saveAction = new QAction(tr("Export table"), this);
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
            const QString filename = QFileDialog::getSaveFileName(this, tr("Export table"), mapId + QStringLiteral(".json"), tr("Tunuino table (*.json)"));
            if (filename.isEmpty()) return;
            QSaveFile file(filename);
            if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(doc).toJson(QJsonDocument::Indented)) < 0 || !file.commit())
                QMessageBox::warning(this, tr("Save failed"), tr("Could not save map."));
        });
        connect(openAction, &QAction::triggered, this, [this, table, mapId, rpmAxis, mapAxis, maximum]() {
            const QString filename = QFileDialog::getOpenFileName(this, tr("Import table"), QString(), tr("Tunuino table (*.json)"));
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
    // EMU Black V3: nine documented 3D-table toolbar actions.
    // Graph is a separate MDI window; split commands tile the table and graph.
    auto installEcumasterToolbar = [this, workspace](QVBoxLayout *layout, QWidget *page,
        QTableWidget *table, const QPair<QAction *, QAction *> &files,
        QMdiSubWindow *graph, const QString &name) {
        auto *bar = new QHBoxLayout();
        bar->setSpacing(2);
        auto add = [bar, page](QStyle::StandardPixmap glyph, const QString &tip, const std::function<void()> &fn) {
            auto *button = new QToolButton(page);
            button->setIcon(page->style()->standardIcon(glyph));
            button->setToolTip(tip);
            button->setAccessibleName(tip);
            button->setToolButtonStyle(Qt::ToolButtonIconOnly);
            button->setIconSize(QSize(20, 20));
            button->setFixedSize(30, 30);
            button->setAutoRaise(true);
            QObject::connect(button, &QToolButton::clicked, page, [fn]() { fn(); });
            bar->addWidget(button);
            return button;
        };
        add(QStyle::SP_DirOpenIcon, tr("Open 3D table (offline)"), [files]() { files.first->trigger(); });
        add(QStyle::SP_DialogSaveButton, tr("Save 3D table (offline)"), [files]() { files.second->trigger(); });
        add(QStyle::SP_FileDialogDetailedView, tr("Only 3D table"), [graph, page]() { graph->hide(); if (auto *w = qobject_cast<QMdiSubWindow *>(page->parentWidget())) w->showNormal(); });
        add(QStyle::SP_ComputerIcon, tr("Only 3D graph"), [graph, workspace, page]() {
            if (auto *w = qobject_cast<QMdiSubWindow *>(page->parentWidget())) w->hide();
            graph->showNormal(); if (graph->widget()) graph->widget()->show();
            workspace->setActiveSubWindow(graph);
        });
        auto split = [graph, workspace, page](bool vertical) {
            auto *tableWindow = qobject_cast<QMdiSubWindow *>(page->parentWidget());
            if (!tableWindow) return;
            tableWindow->showNormal();
            graph->showNormal();
            if (graph->widget()) graph->widget()->show();
            const QSize area = workspace->viewport()->size();
            const int availableWidth = (std::max)(1, area.width() - 8);
            const int availableHeight = (std::max)(1, area.height() - 8);
            if (vertical) {
                const int tableHeight = (std::max)(1, availableHeight * 3 / 5);
                tableWindow->setGeometry(0, 0, availableWidth, tableHeight);
                graph->setGeometry(0, tableHeight, availableWidth,
                                   (std::max)(1, availableHeight - tableHeight));
            } else {
                // A 16-column calibration grid needs more width than the 3D preview.
                // Equal halves cause clipped cells and RPM breakpoints.
                const int tableWidth = (std::max)(1, availableWidth * 3 / 5);
                tableWindow->setGeometry(0, 0, tableWidth, availableHeight);
                graph->setGeometry(tableWidth, 0,
                                   (std::max)(1, availableWidth - tableWidth), availableHeight);
            }
            tableWindow->raise();
        };
        add(QStyle::SP_TitleBarNormalButton, tr("Split vertically: table above graph"), [split]() { split(true); });
        add(QStyle::SP_TitleBarMaxButton, tr("Split horizontally: table beside graph"), [split]() { split(false); });
        auto *follow = add(QStyle::SP_ArrowRight, tr("Follow cursor (F)"), []() {});
        follow->setCheckable(true);
        follow->setChecked(true);
        table->setProperty("followCursorEnabled", true);
        QObject::connect(follow, &QToolButton::toggled, page, [table](bool enabled) {
            table->setProperty("followCursorEnabled", enabled);
            if (!enabled) if (auto *live = dynamic_cast<LiveMapTable *>(table)) live->clearOperatingPoint();
        });
        // Live operating-point tracking is managed by LiveMapTable.
        auto *modify = add(QStyle::SP_ArrowUp, tr("Automodify above (D)"), []() {});
        modify->setCheckable(true);
        QObject::connect(table, &QTableWidget::itemChanged, page, [table, modify](QTableWidgetItem *item) {
            if (!modify->isChecked() || !item) return;
            bool ok = false;
            const double value = item->text().toDouble(&ok);
            if (!ok || !std::isfinite(value)) return;
            QSignalBlocker block(table);
            for (int row = item->row() - 1; row >= 0; --row) {
                auto *above = table->item(row, item->column());
                if (!above) continue;
                bool valid = false;
                const double old = above->text().toDouble(&valid);
                if (valid && old < value) above->setText(item->text());
            }
        });
        add(QStyle::SP_DialogHelpButton, tr("Help: 3D table"), [this, name]() {
            QMessageBox::information(this, tr("3D table help"),
                tr("%1\nOpen/Save: offline map files. Table/Graph: switch views. "
                   "Split V/H: arrange table and graph. Follow: ECU operating-point cursor. "
                   "Auto above: raise lower values above the edited cell. "
                   "Right-click cells for interpolation and editing.").arg(name));
        });
        bar->addStretch();
        layout->insertLayout(0, bar);
    };
    auto *maps = new LiveMapTable(fuelPage);
    maps->setAlternatingRowColors(true);
    maps->setSelectionMode(QAbstractItemView::ExtendedSelection);
    maps->setSelectionBehavior(QAbstractItemView::SelectItems);
    maps->setCornerButtonEnabled(false);
    maps->setToolTip(tr("Tunuino offline base map. Validate before use on a real engine."));
    maps->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    maps->horizontalHeader()->setDefaultSectionSize(38);
    maps->horizontalHeader()->setMinimumSectionSize(18);
    maps->verticalHeader()->setMinimumSectionSize(16);
    maps->horizontalHeader()->setMinimumSectionSize(18);
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
    maps->setMinimumSize(220, 180);
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
    fuelRpmAxis->setFixedWidth(24);
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
    fuelRpmValues->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    // Match the 16 RPM breakpoints to the exact 16 map column widths.
    // Independent Stretch headers drift apart as the MDI window shrinks.
    for (int col = 0; col < 16; ++col)
        fuelRpmValues->setColumnWidth(col, maps->columnWidth(col));
    connect(maps->horizontalHeader(), &QHeaderView::sectionResized,
            fuelRpmValues, [fuelRpmValues](int column, int, int width) {
                fuelRpmValues->setColumnWidth(column, width);
            });
    connect(maps->verticalHeader(), &QHeaderView::sectionResized,
            maps, [maps](int, int, int) {
                const int rowHeight = maps->rowHeight(0);
                QFont f = maps->verticalHeader()->font();
                f.setPointSize((std::max)(5, (std::min)(9, rowHeight / 3)));
                maps->verticalHeader()->setFont(f);
            });
    QFont fuelRpmValuesFont = fuelRpmValues->font();
    fuelRpmValuesFont.setPointSize(6);
    fuelRpmValues->setFont(fuelRpmValuesFont);
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
    auto *fuelGraphSubWindow = workspace->addSubWindow(fuelSurface);tunuino::CalibrationAppearance::frame(fuelGraphSubWindow);
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
    // Reopening a closed MDI subwindow must also re-show its contained graph widget.
    // Qt can leave the child hidden after a QMdiSubWindow close/show cycle.
    installEcumasterToolbar(fuelLayout, fuelPage, maps, fuelFileActions, fuelGraphSubWindow, tr("Fuel Table"));
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
    // Writing is owned by the verified map transaction in the ECU menu.
    ignitionActionsLayout->addStretch();
    ignitionActions->hide();
    auto *ignitionMap = new LiveMapTable(ignitionPage);
    ignitionMap->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ignitionMap->horizontalHeader()->setDefaultSectionSize(38);
    ignitionMap->horizontalHeader()->setMinimumSectionSize(18);
    ignitionMap->verticalHeader()->setMinimumSectionSize(16);
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
    ignitionMap->setMinimumSize(220, 180);
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
    ignitionRpmAxis->setFixedWidth(24);
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
    ignitionRpmValues->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    // Match the 16 RPM breakpoints to the exact 16 map column widths.
    // Independent Stretch headers drift apart as the MDI window shrinks.
    for (int col = 0; col < 16; ++col)
        ignitionRpmValues->setColumnWidth(col, ignitionMap->columnWidth(col));
    connect(ignitionMap->horizontalHeader(), &QHeaderView::sectionResized,
            ignitionRpmValues, [ignitionRpmValues](int column, int, int width) {
                ignitionRpmValues->setColumnWidth(column, width);
            });
    connect(ignitionMap->verticalHeader(), &QHeaderView::sectionResized,
            ignitionMap, [ignitionMap](int, int, int) {
                const int rowHeight = ignitionMap->rowHeight(0);
                QFont f = ignitionMap->verticalHeader()->font();
                f.setPointSize((std::max)(5, (std::min)(9, rowHeight / 3)));
                ignitionMap->verticalHeader()->setFont(f);
            });
    QFont ignitionRpmValuesFont = ignitionRpmValues->font();
    ignitionRpmValuesFont.setPointSize(6);
    ignitionRpmValues->setFont(ignitionRpmValuesFont);
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
    auto *ignitionGraphSubWindow = workspace->addSubWindow(ignitionSurface);tunuino::CalibrationAppearance::frame(ignitionGraphSubWindow);
    ignitionGraphSubWindow->setWindowTitle(tr("Ignition | 3D Graph"));
    ignitionGraphSubWindow->setWindowFlags(Qt::SubWindow | Qt::WindowTitleHint |
                                      Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint |
                                      Qt::WindowCloseButtonHint);
    ignitionGraphSubWindow->setAttribute(Qt::WA_DeleteOnClose, false);
    ignitionGraphSubWindow->resize(450, 420);
    *ignitionGraphWindow = ignitionGraphSubWindow;
    boundsGuard->watch(ignitionGraphSubWindow);
    ignitionGraphSubWindow->move(75, 85);
    installEcumasterToolbar(ignitionLayout, ignitionPage, ignitionMap, ignitionFileActions, ignitionGraphSubWindow, tr("Ignition Table"));
    ignitionLayout->addLayout(ignitionWorkspace, 1);
    tabs->addTab(ignitionPage, tr("Ignition Table"));
    // AFR Target is an editable OFFLINE template only. Firmware page storage is not
    // implemented for this table, so it cannot be written to the ECU.
    auto *afrPage = new QWidget(tabs);
    auto *afrLayout = new QVBoxLayout(afrPage);
    // Title is displayed by the MDI window.
    auto *afrMap = new LiveMapTable(afrPage);
    afrMap->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    afrMap->horizontalHeader()->setDefaultSectionSize(38);
    afrMap->horizontalHeader()->setMinimumSectionSize(18);
    afrMap->verticalHeader()->setMinimumSectionSize(16);
    afrMap->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    afrMap->verticalHeader()->setDefaultSectionSize(28);
    afrMap->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    afrMap->setAlternatingRowColors(true);
    afrMap->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    afrMap->setMinimumSize(220, 180);

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
    afrMapAxisLabel->setFixedWidth(24);
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
    afrRpmValues->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    // Match the 16 RPM breakpoints to the exact 16 map column widths.
    // Independent Stretch headers drift apart as the MDI window shrinks.
    for (int col = 0; col < 16; ++col)
        afrRpmValues->setColumnWidth(col, afrMap->columnWidth(col));
    connect(afrMap->horizontalHeader(), &QHeaderView::sectionResized,
            afrRpmValues, [afrRpmValues](int column, int, int width) {
                afrRpmValues->setColumnWidth(column, width);
            });
    connect(afrMap->verticalHeader(), &QHeaderView::sectionResized,
            afrMap, [afrMap](int, int, int) {
                const int rowHeight = afrMap->rowHeight(0);
                QFont f = afrMap->verticalHeader()->font();
                f.setPointSize((std::max)(5, (std::min)(9, rowHeight / 3)));
                afrMap->verticalHeader()->setFont(f);
            });
    QFont afrRpmValuesFont = afrRpmValues->font();
    afrRpmValuesFont.setPointSize(6);
    afrRpmValues->setFont(afrRpmValuesFont);
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
    auto *afrSurface = new MapSurfacePreview(afrMap, afrPage);
    auto *afrGraphWindow = workspace->addSubWindow(afrSurface);tunuino::CalibrationAppearance::frame(afrGraphWindow);
    afrGraphWindow->setWindowTitle(tr("AFR Target | 3D Graph"));
    afrGraphWindow->setWindowFlags(Qt::SubWindow | Qt::WindowTitleHint |
        Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
    afrGraphWindow->setAttribute(Qt::WA_DeleteOnClose, false);
    afrGraphWindow->resize(450, 420);
    boundsGuard->watch(afrGraphWindow);
    afrGraphWindow->hide();
    installEcumasterToolbar(afrLayout, afrPage, afrMap, afrFileActions, afrGraphWindow, tr("AFR Target"));
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
        auto *window = workspace->addSubWindow(page);tunuino::CalibrationAppearance::frame(window);
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
        window->hide();
    }
    // Start with an empty workspace. Open maps and graphs only on user request.
    if (*fuelGraphWindow) (*fuelGraphWindow)->hide();
    if (*ignitionGraphWindow) (*ignitionGraphWindow)->hide();
    workspace->setActiveSubWindow(nullptr);
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

    auto *graphLog = new tunuino::GraphLogWidget(tabs);
    tabs->addTab(graphLog, tr("Graph Log"));
    auto *tools=new tunuino::ToolsSuite(this,navigationTree);
    auto *serviceTool=new tunuino::ServiceDataWidget;
    auto *scopeTool=new tunuino::ScopeWidget;
    auto *triggerWizard=new tunuino::TriggerWizard;
    auto *outputTestTool=new tunuino::OutputTestWidget;
    auto *autotuneTool=new tunuino::AutotuneWidget;
    const auto toolWindow=[=](QWidget *widget,const QString &title){auto *window=workspace->addSubWindow(widget);tunuino::CalibrationAppearance::frame(window);window->setWindowTitle(title);window->setAttribute(Qt::WA_DeleteOnClose,false);window->resize(900,640);boundsGuard->watch(window);window->hide();return window;};
    auto *serviceWindow=toolWindow(serviceTool,tr("ECU information and Logbook"));
    auto *scopeWindow=toolWindow(scopeTool,tr("Scope"));auto *outputTestWindow=toolWindow(outputTestTool,tr("Test outputs"));auto *autotuneWindow=toolWindow(autotuneTool,tr("Autotune"));
    const auto showTool=[=](QMdiSubWindow *window){window->showNormal();window->widget()->show();workspace->setActiveSubWindow(window);window->raise();};
    *toolNavigation=[=](int index){showTool(index==11?scopeWindow:autotuneWindow);};
    tools->openServiceData=[=](int page){showTool(serviceWindow);serviceTool->showPage(page);};
    tools->openScope=[=]{showTool(scopeWindow);};tools->openOutputTest=[=]{showTool(outputTestWindow);};tools->openAutotune=[=]{showTool(autotuneWindow);};
    auto *tuneDisplay=new tunuino::TuneDisplayWidget;
    auto *tuneSubWindow=workspace->addSubWindow(tuneDisplay);tunuino::CalibrationAppearance::frame(tuneSubWindow);
    tuneSubWindow->setWindowTitle(tr("Tune Display"));
    tuneSubWindow->setWindowFlags(Qt::SubWindow|Qt::WindowTitleHint|Qt::WindowSystemMenuHint|Qt::WindowMinMaxButtonsHint|Qt::WindowCloseButtonHint);
    tuneSubWindow->setAttribute(Qt::WA_DeleteOnClose,false);tuneSubWindow->resize(780,560);
    *tuneWindow=tuneSubWindow;boundsGuard->watch(tuneSubWindow);tuneSubWindow->hide();

    auto *firmwarePage = new QWidget(tabs);
    auto *firmwareLayout = new QVBoxLayout(firmwarePage);
    auto *firmwareHeading = new QLabel(tr("FIRMWARE  |  Device management"), firmwarePage);
    firmwareHeading->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 11pt; color: #e69a32;"));
    firmwareLayout->addWidget(firmwareHeading);
    auto *firmwareInfo = new QLabel(tr("USB DFU device management. Windows bootloader detection is available without external tools. Native DFU flashing is experimental. Firmware is read back and verified after programming."), firmwarePage);
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
    auto *dfuStatus = new QLabel(tr("Select the firmware .bin and detect USB DFU, then use Flash and verify."), firmwarePage);
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
        const QString imageHash = QString::fromLatin1(QCryptographicHash::hash(image,
            QCryptographicHash::Sha256).toHex());
        const QString warning = tr("This operation erases ECU Flash sectors and replaces the firmware.\n\n"
            "Firmware: %1\nBytes: %2\nSHA256: %3\n\n"
            "Confirm that the firmware is intended for this ECU and that the engine and actuators are disconnected. "
            "The ECU may require DFU recovery if interrupted.")
            .arg(QFileInfo(imagePath->text()).fileName()).arg(image.size()).arg(imageHash);
        if (QMessageBox::warning(firmwarePage, tr("Confirm destructive ECU Flash operation"),
            warning, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
            return;
        flashDfu->setEnabled(false);
        detectDfu->setEnabled(false);
        dfuLog->appendPlainText(tr("Starting native DFU flash; do not disconnect power or USB."));
        const bool success = TunuinoDfu::flash(image,
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
        dfuLog->appendPlainText(tr("Preparation is read-only. Flash and verify programs the image and verifies it by reading it back."));
        dfuLog->appendPlainText(tr("Firmware SHA256: %1")
            .arg(QString::fromLatin1(QCryptographicHash::hash(bytes,
                QCryptographicHash::Sha256).toHex())));
        dfuLog->appendPlainText(tr("Preparation only: no erase/write attempted. This plan does not prove board compatibility."));
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
        if (property("ecuTransportOpen").toBool() || !firmwarePage->isVisible()) return;
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
                                        const qint64 firmwareSize = referenceOpen ? reference.size() : 256;
                                        const qint64 targetSize = firmwareSize;
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
            ? tr("STM32 USB DFU bootloader detected by Windows. Select the firmware and use Flash and verify.")
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
    // Each calibration has one persistent MDI window. No second navigation tree.
    tabs->tabBar()->hide();
    auto sensorPanels=QSharedPointer<QVector<tunuino::SensorCalibrationWidget*>>::create();
    auto parameterPanels=QSharedPointer<QVector<tunuino::ParameterWidget*>>::create();
    auto parameterWindows=QSharedPointer<QMap<QString,QPointer<QMdiSubWindow>>>::create();
    QPushButton *tpsReadButton=nullptr;QLabel *tpsReadStatus=nullptr;
    for(auto kind:{tunuino::SensorKind::Tps,tunuino::SensorKind::Map,tunuino::SensorKind::Clt,tunuino::SensorKind::Iat,tunuino::SensorKind::Lambda,tunuino::SensorKind::Battery,tunuino::SensorKind::FuelPressure,tunuino::SensorKind::OilPressure}){
        auto *panel=new tunuino::SensorCalibrationWidget(kind);sensorPanels->append(panel);const auto name=tunuino::SensorCalibrationCodec::name(kind);
        auto *window=toolWindow(panel,tr("Sensors and inputs - %1").arg(name));window->resize(panel->sizeHint()+QSize(12,30));(*parameterWindows)["sensor:"+name]=window;
    }
    auto inputPanels=QSharedPointer<QVector<tunuino::SensorInputsWidget*>>::create();
    const QStringList inputKeys{"inputs.analog","inputs.crank","inputs.cam1","inputs.temperature","inputs.pressure"};
    for(int mode=0;mode<5;++mode){auto *panel=new tunuino::SensorInputsWidget(tunuino::SensorInputsWidget::Mode(mode));inputPanels->append(panel);panel->openScope=[=]{showTool(scopeWindow);};auto *window=toolWindow(panel,panel->title());window->resize(panel->sizeHint()+QSize(12,30));(*parameterWindows)[mode==0?"inputs.monitor":mode==2?"inputs.cam1.monitor":mode==4?"inputs.pressure.monitor":inputKeys[mode]]=window;}
    auto *cam2Monitor=new tunuino::VssMonitor(nullptr,tunuino::VssMonitor::Source::Cam2);auto *cam2Window=toolWindow(cam2Monitor,tr("CAM 2 - Live pulses"));(*parameterWindows)["inputs.cam2.monitor"]=cam2Window;cam2Window->resize(cam2Monitor->sizeHint()+QSize(12,30));
    auto *vssMonitor=new tunuino::VssMonitor;auto *vssWindow=toolWindow(vssMonitor,tr("Vehicle speed - Live readings"));(*parameterWindows)["speed.monitor"]=vssWindow;vssWindow->resize(vssMonitor->sizeHint()+QSize(12,30));
    auto sensorWizards=QSharedPointer<QMap<QString,QPointer<tunuino::SensorCurveWizard>>>::create();
    (*sensorWizards)["sensorwizard:IAT"]=new tunuino::SensorCurveWizard(tunuino::SensorKind::Iat,false,this);
    (*sensorWizards)["sensorwizard:CLT"]=new tunuino::SensorCurveWizard(tunuino::SensorKind::Clt,false,this);
    (*sensorWizards)["sensorwizard:Temperature"]=new tunuino::SensorCurveWizard(tunuino::SensorKind::Iat,true,this);
    (*sensorWizards)["sensorwizard:MAP"]=new tunuino::SensorCurveWizard(tunuino::SensorKind::Map,false,this);
    (*sensorWizards)["sensorwizard:Pressure"]=new tunuino::SensorCurveWizard(tunuino::SensorKind::Map,false,this,true);
    for(const auto &block:tunuino::ParameterCatalog::blocks()){auto *panel=new tunuino::ParameterWidget(block);parameterPanels->append(panel);auto *window=toolWindow(panel,block.title);window->resize(panel->sizeHint()+QSize(12,30));(*parameterWindows)[block.key]=window;}
    auto *manualTriggerWindow=toolWindow(triggerWizard,tr("Triggers - Manual setup"));
    manualTriggerWindow->resize(triggerWizard->sizeHint()+QSize(12,30));
    auto *triggerPresets=new tunuino::TriggerPresetDialog(this);
    triggerPresets->openManual=[=]{showTool(manualTriggerWindow);};
    triggerWizard->openScope=[=]{showTool(scopeWindow);};triggerWizard->openFiringOrder=[=]{if(*parameterNavigation)(*parameterNavigation)("ignition.firing");};
    scopeTool->captured=[=](const tunuino::ToolSnapshot &source,const QByteArray &header,const QByteArray &samples){triggerWizard->observeScope(source,header,samples);};
    *parameterNavigation=[=](const QString &key){if((*sensorWizards).contains(key)){(*sensorWizards)[key]->open();return;}if(key=="ignition.wizard"){triggerPresets->open();return;}auto *window=(*parameterWindows).value(key).data();if(!window)return;if(!window->property("parameterOpened").toBool()){if(auto *parameters=dynamic_cast<tunuino::ParameterWidget*>(window->widget()))window->resize(parameters->sizeHint()+QSize(12,30));window->setProperty("parameterOpened",true);}window->showNormal();window->widget()->show();workspace->setActiveSubWindow(window);window->raise();};
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
    dock->hide();
    *engineStatusDockRef = dock;
    auto *session = new tunuino::SerialSession(this);
    auto identityVerified = QSharedPointer<bool>::create(false);
    auto ecuOnline = QSharedPointer<bool>::create(false);
    auto ecuClean = QSharedPointer<bool>::create(false);
    auto powerState = QSharedPointer<tunuino::EcuPowerState>::create();
    auto ecuIdentity = QSharedPointer<QByteArray>::create();
    auto *writer = new tunuino::MapWriteTransaction(this);
    auto readBusy = QSharedPointer<bool>::create(false);
    auto validationPending = QSharedPointer<bool>::create(false);
    auto writePending = QSharedPointer<bool>::create(false);
    auto ecuGeneration = QSharedPointer<quint32>::create(0);
    struct MapBase { QByteArray bytes, identity; quint32 generation=0; };
    auto mapBases = QSharedPointer<std::array<MapBase,2>>::create();
    struct SensorBase { QByteArray header,aux,identity;quint32 generation=0; };
    auto sensorBase=QSharedPointer<SensorBase>::create();
    auto *mapProject=new tunuino::MapProject(this);
    auto projectLoaded=QSharedPointer<bool>::create(false);
    auto projectPages=QSharedPointer<QJsonObject>::create();
    auto activeSensorWrite=QSharedPointer<int>::create(-1);
    auto activeParameterFinished=QSharedPointer<std::function<void(bool,const QString&)>>::create();
    auto writeActionRef = QSharedPointer<QPointer<QAction>>::create();
    const auto updateWriteAvailability = [=]() {
        const bool ready = *ecuOnline && *ecuClean && *identityVerified && session->isOpen() && !writer->busy() && !*writePending && !*readBusy && !*validationPending && !*toolBusy && !*toolReply;
        const auto &fuelBase=(*mapBases)[0]; const auto &ignitionBase=(*mapBases)[1];
        fuelSave->setEnabled(ready && !fuelBase.bytes.isEmpty() && fuelBase.identity==*ecuIdentity && fuelBase.generation!=0 && fuelBase.generation==*ecuGeneration);
        ignitionSave->setEnabled(ready && !ignitionBase.bytes.isEmpty() && ignitionBase.identity==*ecuIdentity && ignitionBase.generation!=0 && ignitionBase.generation==*ecuGeneration);
        const bool sensorReady=ready&&sensorBase->header.size()==256&&sensorBase->aux.size()==256
            &&sensorBase->identity==*ecuIdentity&&sensorBase->generation==*ecuGeneration;
        for(auto *panel:*sensorPanels)panel->setAvailable(sensorReady);for(auto *panel:*inputPanels)panel->setAvailable(sensorReady);
        for(auto *panel:*parameterPanels)panel->setAvailable(sensorReady);triggerWizard->setAvailable(sensorReady);
        if (*writeActionRef) {
            const auto *window=workspace->activeSubWindow(); auto *page=window?window->widget():nullptr;
            const auto *sensorPage=dynamic_cast<const tunuino::SensorCalibrationWidget*>(page);
            (*writeActionRef)->setEnabled(((page==fuelPage || page==fuelSurface) && fuelSave->isEnabled())
                || ((page==ignitionPage || page==ignitionSurface) && ignitionSave->isEnabled())
                || (sensorReady&&((sensorPage&&sensorPage->writeEnabled())||dynamic_cast<const tunuino::ParameterWidget*>(page)||(page==triggerWizard&&triggerWizard->reviewReady()))));
        }
    };
    auto connectionClock = QSharedPointer<QElapsedTimer>::create();
    connectionClock->start();
    auto *connectionBadge = new QLabel(tr("OFFLINE"), this);
    connectionBadge->setObjectName(QStringLiteral("EcuConnectionBadge"));
    statusBar()->addPermanentWidget(connectionBadge);
    const auto setConnectionIndicator = [=](bool online) {
        *ecuOnline = online;
        connectionBadge->setText(online ? tr("ONLINE") : tr("OFFLINE"));
        connectionBadge->setStyleSheet(online
            ? QStringLiteral("color: #76d58a; font-weight: bold; padding: 4px 12px;")
            : QStringLiteral("color: #aaaaaa; font-weight: bold; padding: 4px 12px;"));
        if (!online) {
            for(auto *panel:*sensorPanels)panel->setAvailable(false);for(auto *panel:*inputPanels)panel->setAvailable(false);
            for(auto *panel:*parameterPanels)panel->setAvailable(false);triggerWizard->setAvailable(false);
            graphLog->setConnected(false);
            tuneDisplay->setConnected(false);
            autotuneTool->disconnected();
            for (auto *value : *liveReadings) value->setText(tr("Unavailable"));
            maps->clearOperatingPoint();
            ignitionMap->clearOperatingPoint();
            afrMap->clearOperatingPoint();
        }
    };
    setConnectionIndicator(false);
    // The firmware runtime command is read-only: O + offset(U16 BE) + count(U16 BE).
    auto *liveTimer = new QTimer(this);
    liveTimer->setInterval(500);
    auto requestLive = [session, identityVerified, writer, writePending]() {
        if (!*identityVerified || !session->isOpen() || writer->busy() || *writePending) return;
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
        const int total = (transfer->page == 1 || transfer->page == 4) ? 256 : 576;
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
        *readBusy = false;
        *autoSync = false;
        transfer->page = 0;
        transfer->offset = 0;
        transfer->bytes.clear();
        fuelRead->setEnabled(*ecuOnline && *identityVerified && session->isOpen());
        ignitionRead->setEnabled(*ecuOnline && *identityVerified && session->isOpen());
        if (tpsReadButton) tpsReadButton->setEnabled(*ecuOnline && *identityVerified && session->isOpen());
        // Transfer feedback is displayed in the status bar.
        statusBar()->showMessage(message, 10000);
        liveTimer->start();
        updateWriteAvailability();
    };
    auto startTransfer = [=](int page) {
        if (!*ecuOnline || !*identityVerified || !session->isOpen() || transfer->page || writer->busy() || *writePending) return;
        liveTimer->stop();
        transfer->page = page;
        *readBusy = true;
        updateWriteAvailability();
        transfer->offset = 0;
        transfer->bytes.clear();
        fuelSave->setEnabled(false); ignitionSave->setEnabled(false);
        if (*writeActionRef) (*writeActionRef)->setEnabled(false);
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
    auto *openProject = fileMenu->addAction(tr("Open Project..."));
    openProject->setShortcut(QKeySequence::Open);
    auto *saveProject = fileMenu->addAction(tr("Save Project as..."));
    saveProject->setShortcut(QKeySequence::Save);
    openProject->setObjectName("fileOpenMap");saveProject->setObjectName("fileSaveMap");
    mapProject->canUse=[=]{return !writer->busy()&&!*writePending&&!*readBusy&&!*validationPending&&!*toolBusy;};
    mapProject->capture=[=](QString &error){
        auto pages=*projectPages;if(pages.isEmpty())pages=tunuino::MapProject::defaultPages();
        const auto live=tools->snapshot?tools->snapshot():tunuino::ToolSnapshot{};
        if(!*projectLoaded&&live.valid()){const QStringList names{"header","fuel","ignition","aux"};for(int i=0;i<4;++i)pages[names[i]]=QString::fromLatin1(live.pages[i].toHex());}
        QJsonObject tables,parameters,sensors;
        const QList<QTableWidget*> grids{maps,ignitionMap,afrMap},rpms{fuelRpmValues,ignitionRpmValues,afrRpmValues};const QStringList names{"fuel","ignition","afr"};
        for(int i=0;i<3;++i){const auto state=tunuino::MapProject::tableState(grids[i],rpms[i]);if(!tunuino::MapProject::tableState(grids[i],rpms[i],state,i==1?-90:0,i==0?300:i==1?90:25,false,error))return QJsonObject{};tables[names[i]]=state;}
        for(auto *panel:*parameterPanels)parameters[panel->block().key]=panel->editorState();
        for(auto *panel:*sensorPanels)sensors[tunuino::SensorCalibrationCodec::name(panel->kind())]=panel->editorState();
        return QJsonObject{{"target",QJsonObject{{"family",1},{"mcu",1},{"hardwareRevision",7},{"schema",2}}},{"pages",pages},{"tables",tables},{"parameters",parameters},{"sensors",sensors},{"trigger",triggerWizard->editorState()},{"service",QJsonObject{{"comment",serviceTool->projectComment()}}}};
    };
    mapProject->apply=[=](const QJsonObject &document,QString &error){
        const auto tables=document["tables"].toObject();auto parameters=document["parameters"].toObject();auto sensors=document["sensors"].toObject();
        // Older v2 projects have six sensor editors. Add only the two new
        // pressure editors from their stored base; never discard legacy fields.
        const auto storedHeader=QByteArray::fromHex(document["pages"].toObject()["header"].toString().toLatin1());
        if(!tunuino::Cam2Codec::format(storedHeader)){error=tr("Invalid CAM 2 extension.");return false;}
        if(!tunuino::VssCodec::format(storedHeader)){error=tr("Invalid VSS extension.");return false;}
        if(!tunuino::SensorCalibrationCodec::camInputFormat(storedHeader)){error=tr("Invalid CAM input filter extension.");return false;}
        if(!tunuino::SensorCalibrationCodec::primaryInputFormat(storedHeader)){error=tr("Invalid primary input filter extension.");return false;}
        if(!tunuino::SensorCalibrationCodec::inputFilterFormat(storedHeader)){error=tr("Invalid analog input filter extension.");return false;}
        if(!tunuino::SensorCalibrationCodec::pressureFormat(storedHeader)){error=tr("Invalid project pressure extension.");return false;}
        for(auto kind:{tunuino::SensorKind::FuelPressure,tunuino::SensorKind::OilPressure}){const auto name=tunuino::SensorCalibrationCodec::name(kind);if(!sensors.contains(name)){const auto c=tunuino::SensorCalibrationCodec::decode(kind,storedHeader,QByteArray(256,0));sensors[name]=QJsonObject{{"input",c.channel},{"volts",QJsonArray{tunuino::SensorCalibrationCodec::voltage(c.raw[0],storedHeader),tunuino::SensorCalibrationCodec::voltage(c.raw[1],storedHeader)}},{"values",QJsonArray{c.values[0],c.values[1]}}};}}
        if(!parameters.contains("inputs.pressure")){QJsonObject values;for(const auto &block:tunuino::ParameterCatalog::blocks())if(block.key=="inputs.pressure")for(const auto &field:block.fields)values[field.key]=tunuino::ParameterCatalog::value(storedHeader,field);parameters["inputs.pressure"]=QJsonObject{{"values",values}};}
        if(!parameters.contains("inputs.analog")){QJsonObject values;for(const auto &block:tunuino::ParameterCatalog::blocks())if(block.key=="inputs.analog")for(const auto &field:block.fields)values[field.key]=tunuino::ParameterCatalog::value(storedHeader,field);parameters["inputs.analog"]=QJsonObject{{"values",values}};}
        if(!parameters.contains("inputs.primary")){QJsonObject values;for(const auto &block:tunuino::ParameterCatalog::blocks())if(block.key=="inputs.primary")for(const auto &field:block.fields)values[field.key]=tunuino::ParameterCatalog::value(storedHeader,field);parameters["inputs.primary"]=QJsonObject{{"values",values}};}
        if(!parameters.contains("inputs.cam1")){QJsonObject values;for(const auto &block:tunuino::ParameterCatalog::blocks())if(block.key=="inputs.cam1")for(const auto &field:block.fields)values[field.key]=tunuino::ParameterCatalog::value(storedHeader,field);parameters["inputs.cam1"]=QJsonObject{{"values",values}};}
        if(!parameters.contains("inputs.cam2")){const auto header=tunuino::Cam2Codec::defaults(storedHeader);QJsonObject values;for(const auto &block:tunuino::ParameterCatalog::blocks())if(block.key=="inputs.cam2")for(const auto &field:block.fields)values[field.key]=tunuino::ParameterCatalog::value(header,field);parameters["inputs.cam2"]=QJsonObject{{"values",values}};}
        for(const auto &key:{QString("inputs.vss"),QString("speed.vehicle")})if(!parameters.contains(key)){const auto header=tunuino::VssCodec::defaults(storedHeader);QJsonObject values;for(const auto &block:tunuino::ParameterCatalog::blocks())if(block.key==key)for(const auto &field:block.fields)values[field.key]=tunuino::ParameterCatalog::value(header,field);parameters[key]=QJsonObject{{"values",values}};}
        if(parameters.size()!=parameterPanels->size()||sensors.size()!=sensorPanels->size()){error=tr("This map does not contain all supported parameters and sensors.");return false;}
        const QList<QTableWidget*> grids{maps,ignitionMap,afrMap},rpms{fuelRpmValues,ignitionRpmValues,afrRpmValues};const QStringList names{"fuel","ignition","afr"};
        for(int i=0;i<3;++i)if(!tunuino::MapProject::tableState(grids[i],rpms[i],tables[names[i]].toObject(),i==1?-90:0,i==0?300:i==1?90:25,false,error))return false;
        for(auto *panel:*parameterPanels)if(!panel->restoreEditor(parameters[panel->block().key].toObject(),false,error))return false;
        for(auto *panel:*sensorPanels)if(!panel->restoreEditor(sensors[tunuino::SensorCalibrationCodec::name(panel->kind())].toObject(),false,error))return false;
        if(!triggerWizard->restoreEditor(document["trigger"].toObject(),false,error))return false;
        const QJsonValue comment=document["service"].toObject()["comment"];if(!comment.isString()||comment.toString().size()>128){error=tr("Invalid project comment.");return false;}
        // All content is validated before changing any editor. File data never
        // replaces the trusted live ECU snapshot used by the write transaction.
        *projectPages=document["pages"].toObject();*projectLoaded=true;
        const auto live=tools->snapshot?tools->snapshot():tunuino::ToolSnapshot{};
        const auto header=live.valid()?live.pages[0]:QByteArray::fromHex((*projectPages)["header"].toString().toLatin1());
        const auto aux=live.valid()?live.pages[3]:QByteArray::fromHex((*projectPages)["aux"].toString().toLatin1());
        QByteArray identity=live.valid()?live.identity:QByteArray(32,0);if(!live.valid())identity[19]=char(255);const quint32 generation=live.valid()?live.generation:1;
        for(auto *panel:*parameterPanels){panel->keepProjectEdits(false);panel->load(header,aux,identity,generation);panel->restoreEditor(parameters[panel->block().key].toObject(),true,error);panel->keepProjectEdits(true);if(!live.valid())panel->setAvailable(false);}
        for(auto *panel:*sensorPanels){panel->keepProjectEdits(false);panel->load(header,aux,identity,generation);panel->restoreEditor(sensors[tunuino::SensorCalibrationCodec::name(panel->kind())].toObject(),true,error);panel->keepProjectEdits(true);if(!live.valid())panel->setAvailable(false);}
        triggerWizard->keepProjectEdits(false);if(live.valid())triggerWizard->load(header,aux,identity,generation);else triggerWizard->loadOffline(header,aux);triggerWizard->restoreEditor(document["trigger"].toObject(),true,error);triggerWizard->keepProjectEdits(true);if(!live.valid())triggerWizard->setAvailable(false);
        for(int i=0;i<3;++i)tunuino::MapProject::tableState(grids[i],rpms[i],tables[names[i]].toObject(),i==1?-90:0,i==0?300:i==1?90:25,true,error);
        serviceTool->setProjectComment(comment.toString());fuelSurface->update();ignitionSurface->update();afrSurface->update();
        statusBar()->showMessage(tr("Project opened locally. No ECU write was performed."),10000);return true;
    };
    for(const auto &wizard:*sensorWizards)wizard->prepare=[=](tunuino::SensorKind kind,const QJsonObject &curve,QString &error){
        if(!mapProject->canUse()){error=tr("Wait for the current ECU operation to finish.");return false;}
        for(auto *panel:*sensorPanels)if(panel->kind()==kind){auto state=panel->editorState();state["volts"]=curve["volts"];state["values"]=curve["values"];if(!panel->restoreEditor(state,false,error))return false;panel->restoreEditor(state,true,error);panel->keepProjectEdits(true);statusBar()->showMessage(tr("Sensor curve prepared locally. Input preserved; no ECU write."),10000);return true;}
        error=tr("This sensor has no native calibration editor.");return false;
    };
    triggerPresets->applyRequested=[=](int index,int groups,QString &error){
        if(!mapProject->canUse()){error=tr("Wait for the current ECU operation to finish.");return false;}
        const auto current=mapProject->capture(error);if(current.isEmpty())return false;
        QJsonObject desired;if(!tunuino::TriggerPresets::apply(current,index,groups,desired,error))return false;
        const auto parameters=desired["parameters"].toObject();
        const auto selected=[=](const QString &key){return (key=="ignition.primary"&&(groups&tunuino::TriggerPresets::Primary))||(key=="ignition.firing"&&(groups&tunuino::TriggerPresets::Firing));};
        // Validate every selected editor before mutating any. Other drafts and
        // their live-base/conflict state are deliberately left untouched.
        for(auto *panel:*parameterPanels)if(selected(panel->block().key)&&!panel->restoreEditor(parameters[panel->block().key].toObject(),false,error))return false;
        if((groups&tunuino::TriggerPresets::Primary)&&!triggerWizard->restoreEditor(desired["trigger"].toObject(),false,error))return false;
        for(auto *panel:*parameterPanels)if(selected(panel->block().key)){panel->restoreEditor(parameters[panel->block().key].toObject(),true,error);panel->keepProjectEdits(true);}
        if(groups&tunuino::TriggerPresets::Primary){triggerWizard->restoreEditor(desired["trigger"].toObject(),true,error);triggerWizard->keepProjectEdits(true);}
        statusBar()->showMessage(tr("Trigger preset applied to local editors. Review settings before Write to ECU."),10000);return true;
    };
    serviceTool->saveMap=[mapProject]{mapProject->saveAs();};
    connect(openProject,&QAction::triggered,mapProject,&tunuino::MapProject::openDialog);
    connect(saveProject,&QAction::triggered,mapProject,&tunuino::MapProject::saveAs);
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
    auto *inputMonitorAction=viewMenu->addAction(tr("Analog input monitor"));connect(inputMonitorAction,&QAction::triggered,this,[=]{(*parameterNavigation)("inputs.monitor");});
    auto *primaryMonitorAction=viewMenu->addAction(tr("Primary trigger monitor"));connect(primaryMonitorAction,&QAction::triggered,this,[=]{(*parameterNavigation)("inputs.crank");});
    auto *camMonitorAction=viewMenu->addAction(tr("CAM 1 monitor"));connect(camMonitorAction,&QAction::triggered,this,[=]{(*parameterNavigation)("inputs.cam1.monitor");});
    auto *pressureMonitorAction=viewMenu->addAction(tr("Pressure monitor"));connect(pressureMonitorAction,&QAction::triggered,this,[=]{(*parameterNavigation)("inputs.pressure.monitor");});
    auto *cam2MonitorAction=viewMenu->addAction(tr("CAM 2 monitor"));connect(cam2MonitorAction,&QAction::triggered,this,[=]{(*parameterNavigation)("inputs.cam2.monitor");});
    auto *vssMonitorAction=viewMenu->addAction(tr("Vehicle speed monitor"));connect(vssMonitorAction,&QAction::triggered,this,[=]{(*parameterNavigation)("speed.monitor");});
    viewMenu->addAction(navigationDock->toggleViewAction());
    viewMenu->addAction(inspectorDock->toggleViewAction());
    viewMenu->addAction(dock->toggleViewAction());
    auto *toolsMenu = menuBar()->addMenu(tr("Tools"));
    auto *tuneDisplayAction=toolsMenu->addAction(tr("Tune Display"),this,openTuneDisplay);
    tuneDisplayAction->setShortcut(QKeySequence(QStringLiteral("F7")));
    auto *graphLogAction = toolsMenu->addAction(tr("Graph Log"));
    graphLogAction->setShortcut(QKeySequence(QStringLiteral("F6")));
    connect(graphLogAction, &QAction::triggered, this, [openSettingsPage]() { openSettingsPage(4); });
    tools->populate(toolsMenu,graphLogAction,tuneDisplayAction);
    tools->snapshot=[=]{
        tunuino::ToolSnapshot result;
        if(!*ecuOnline||!*ecuClean||!*identityVerified||!session->isOpen()||*readBusy||*validationPending||writer->busy()||*writePending)return result;
        if(sensorBase->identity!=*ecuIdentity||sensorBase->generation!=*ecuGeneration)return result;
        for(const auto &base:*mapBases)if(base.identity!=*ecuIdentity||base.generation!=*ecuGeneration)return result;
        result.identity=*ecuIdentity;result.generation=*ecuGeneration;result.pages={sensorBase->header,(*mapBases)[0].bytes,(*mapBases)[1].bytes,sensorBase->aux};return result;
    };
    const auto changeToolBusy=[=](bool busy){*toolBusy=busy;setProperty("ecuToolBusy",busy);updateWriteAvailability();};
    serviceTool->busyChanged=changeToolBusy;
    scopeTool->busyChanged=changeToolBusy;outputTestTool->busyChanged=changeToolBusy;autotuneTool->busyChanged=changeToolBusy;
    scopeTool->ready=[=]{const auto base=tools->snapshot();return base.valid()&&(quint8(base.identity[19])&16)&&(!*toolBusy||scopeTool->busy());};
    serviceTool->snapshot=tools->snapshot;serviceTool->canStart=[=]{return !*toolBusy&&!*toolReply;};
    scopeTool->snapshot=tools->snapshot;
    outputTestTool->snapshot=tools->snapshot;autotuneTool->snapshot=tools->snapshot;
    outputTestTool->canStart=[=]{return !*toolBusy&&!*toolReply;};autotuneTool->canStart=outputTestTool->canStart;
    const auto toolExchange=[=](const QByteArray &request,std::function<void(const QByteArray&)> reply){
        if(!*ecuOnline||!*identityVerified||!session->isOpen()||session->isBusy()||*toolReply||writer->busy()||*writePending||*readBusy||*validationPending)return false;
        *toolReply=std::move(reply);*toolRuntime=!request.isEmpty()&&request[0]=='O';const auto lease=++*toolLease;
        const int timeout=request.startsWith("TUNUINO:N")&&request.size()>9&&quint8(request[9])>=2?15000:1500;
        if(!session->exchange(request,timeout)){*toolReply={};return false;}
        QTimer::singleShot(timeout+300,this,[=]{if(*toolReply&&*toolLease==lease){auto callback=std::move(*toolReply);*toolReply={};callback({});}});return true;
    };
    cam2Monitor->exchange=toolExchange;cam2Monitor->snapshot=tools->snapshot;cam2Monitor->ready=[=]{return !*toolBusy&&!*toolReply;};
    vssMonitor->exchange=toolExchange;vssMonitor->snapshot=tools->snapshot;vssMonitor->ready=[=]{return !*toolBusy&&!*toolReply;};
    serviceTool->exchange=toolExchange;
    scopeTool->exchange=toolExchange;outputTestTool->exchange=toolExchange;
    autotuneTool->applyToEditor=[=](const tunuino::ToolSnapshot &base,const QByteArray &desired){
        const auto current=tools->snapshot();if(current.identity!=base.identity||current.generation!=base.generation||current.pages!=base.pages||!tunuino::MapPageCodec::valid(2,desired))return false;
        tunuino::MapPageValues values;bool valid=true;
        for(int i=0;i<16;++i){bool ok=false;values.rpm[i]=fuelRpmValues->item(0,i)->text().toDouble(&ok);valid&=ok;values.load[i]=maps->verticalHeaderItem(15-i)->text().toDouble(&ok);valid&=ok;}
        for(int r=0;r<16;++r)for(int c=0;c<16;++c){bool ok=false;values.cells[r*16+c]=maps->item(15-r,c)->text().toDouble(&ok);valid&=ok;}
        QString error;if(!valid||tunuino::MapPageCodec::encode(2,values,error)!=base.pages[1])return false;
        for(int r=0;r<16;++r)for(int c=0;c<16;++c)maps->item(15-r,c)->setText(QString::number(tunuino::SensorCalibrationCodec::u16(desired,64+2*(r*16+c))/100.0,'f',2));return true;
    };
    auto *validationAction = new QAction(tr("Check ECU configuration (read-only)"),this);
    validationAction->setEnabled(false);
    connect(validationAction, &QAction::triggered, this, [=]() {
        if (!*ecuOnline || !*identityVerified || !session->isOpen() || transfer->page || *validationPending || writer->busy() || *writePending || *toolBusy || *toolReply) return;
        liveTimer->stop();
        *validationPending = true;
        updateWriteAvailability();
        if (!session->exchange(QByteArray("TUNUINO:V"), 3000)) {
            *validationPending = false;
            liveTimer->start(); updateWriteAvailability();
        }
    });
    writer->send = [session](const QByteArray &request,int timeout) { return session->exchange(request,timeout); };
    writer->savePrevious = [](const QByteArray &identity,quint32 generation,const tunuino::MapWriteTransaction::Pages &pages) {
        const QString directory=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+QStringLiteral("/ecu-history");
        if (!QDir().mkpath(directory)) return false;
        QJsonArray data; QByteArray all;
        for (const auto &page:pages) { data.append(QString::fromLatin1(page.toHex())); all+=page; }
        const QJsonObject object{{QStringLiteral("format"),QStringLiteral("tunuino-ecu-snapshot-v1")},
            {QStringLiteral("identity"),QString::fromLatin1(identity.toHex())},
            {QStringLiteral("generation"),double(generation)},{QStringLiteral("pages"),data},
            {QStringLiteral("sha256"),QString::fromLatin1(QCryptographicHash::hash(all,QCryptographicHash::Sha256).toHex())}};
        const QString name=QString::fromLatin1(identity.mid(20,12).toHex())+QStringLiteral("-")
            +QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"))+QStringLiteral("-")
            +QUuid::createUuid().toString(QUuid::WithoutBraces)+QStringLiteral(".json");
        QSaveFile file(directory+QStringLiteral("/")+name);
        const QByteArray bytes=QJsonDocument(object).toJson(QJsonDocument::Compact);
        return file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size() && file.commit();
    };
    const auto freezeMapEditing = [=](bool frozen) {
        openProject->setEnabled(!frozen);saveProject->setEnabled(!frozen);
        fuelPage->setEnabled(!frozen); ignitionPage->setEnabled(!frozen);
        for(auto *panel:*sensorPanels)panel->setEnabled(!frozen);for(auto *panel:*parameterPanels)panel->setEnabled(!frozen);triggerWizard->setEnabled(!frozen);
        for (auto *action : {fuelFileActions.first,fuelFileActions.second,ignitionFileActions.first,ignitionFileActions.second})
            action->setEnabled(!frozen);
    };
    writer->completed = [=](bool success,bool uncertain,const QString &message) {
        *writePending=false; setProperty("mapWriteBusy",false); freezeMapEditing(false);
        (*mapBases)[0]={}; (*mapBases)[1]={}; *sensorBase={};
        if(*activeParameterFinished){auto done=std::move(*activeParameterFinished);*activeParameterFinished={};done(success,message);}
        if(success&&*activeSensorWrite>=0)for(auto *panel:*sensorPanels)if(int(panel->kind())==*activeSensorWrite)panel->saved();
        statusBar()->showMessage(message,30000);
        if (success) {
            *ecuGeneration=writer->generation(); *ecuClean=true;
            validationAction->setEnabled(*ecuOnline);
            if (*ecuOnline && *identityVerified && session->isOpen()) {
                *autoSync=true;
                QTimer::singleShot(0,this,[=] { startTransfer(1); });
            }
        } else {
            // A failed or uncertain operation is never repeated. Reconnect only
            // performs the normal read path; the next write needs an explicit action.
            if (!writer->writeRequested() && writer->identityMatched() && session->isOpen() && *identityVerified) {
                *autoSync=true;liveTimer->start();requestLive();
                validationAction->setEnabled(*ecuOnline);
            } else session->close();
            QTimer::singleShot(0,this,[this,uncertain,message] {
                QMessageBox::warning(this,uncertain?tr("ECU save requires verification"):tr("ECU map write stopped"),message);
            });
        }
        updateWriteAvailability();
    };
    struct PendingMapWrite { int page=0,sensor=-1; QByteArray desired,base,identity,mask; quint32 generation=0; };
    auto pendingWrite = QSharedPointer<PendingMapWrite>::create();
    auto *writeLaunch = new QTimer(this); writeLaunch->setInterval(25);
    auto writeWait = QSharedPointer<QElapsedTimer>::create();
    connect(writeLaunch,&QTimer::timeout,this,[=] {
        if (!*writePending) { writeLaunch->stop();if(!writer->busy()&&*activeParameterFinished){auto done=std::move(*activeParameterFinished);*activeParameterFinished={};done(false,tr("Write request cancelled before starting."));}return; }
        if (!*ecuOnline || !*identityVerified || !session->isOpen() || writeWait->elapsed()>3000) {
            writeLaunch->stop(); *writePending=false; setProperty("mapWriteBusy",false); freezeMapEditing(false);
            if (*identityVerified && session->isOpen())liveTimer->start();
            validationAction->setEnabled(*ecuOnline);
            fuelRead->setEnabled(*ecuOnline);ignitionRead->setEnabled(*ecuOnline);if(tpsReadButton)tpsReadButton->setEnabled(*ecuOnline);
            updateWriteAvailability();
            if(*activeParameterFinished){auto done=std::move(*activeParameterFinished);*activeParameterFinished={};done(false,tr("ECU unavailable or communication busy. No write started."));}
            statusBar()->showMessage(tr("Map write cancelled: ECU unavailable or communication busy."),10000);return;
        }
        if (session->isBusy()) return;
        writeLaunch->stop(); *writePending=false;
        const bool started=pendingWrite->sensor==-2?writer->startParameters(pendingWrite->page,pendingWrite->desired,pendingWrite->base,pendingWrite->identity,pendingWrite->generation,pendingWrite->mask):pendingWrite->sensor>=0?writer->startSensor(tunuino::SensorKind(pendingWrite->sensor),pendingWrite->desired,pendingWrite->base,pendingWrite->identity,pendingWrite->generation):
            writer->start(pendingWrite->page,pendingWrite->desired,pendingWrite->base,pendingWrite->identity,pendingWrite->generation);
        if (!started) {
            setProperty("mapWriteBusy",false); freezeMapEditing(false); liveTimer->start();
            validationAction->setEnabled(*ecuOnline);
            fuelRead->setEnabled(*ecuOnline);ignitionRead->setEnabled(*ecuOnline);if(tpsReadButton)tpsReadButton->setEnabled(*ecuOnline);
            updateWriteAvailability();
            if(*activeParameterFinished){auto done=std::move(*activeParameterFinished);*activeParameterFinished={};done(false,tr("Write could not start. Re-read the ECU configuration."));}
            QMessageBox::warning(this,tr("ECU map write"),tr("Map write could not start. Re-read the ECU and check map values."));
        }
    });
    const auto queueMapWrite = [=](int page) {
        if (!*ecuOnline || !*identityVerified || !session->isOpen() || transfer->page || *validationPending || writer->busy() || *writePending || *toolBusy || *toolReply) return;
        auto *table=page==2?maps:ignitionMap; auto *rpm=page==2?fuelRpmValues:ignitionRpmValues;
        const auto base=(*mapBases)[size_t(page-2)];
        if (base.bytes.isEmpty() || base.identity!=*ecuIdentity) return;
        tunuino::MapPageValues values;
        bool valid=true;
        for (int i=0;i<16;++i) {
            bool ok=false; values.rpm[i]=rpm->item(0,i)->text().toDouble(&ok); valid &= ok;
            const auto *load=table->verticalHeaderItem(15-i); ok=false;
            values.load[i]=load?load->text().toDouble(&ok):0; valid &= ok;
        }
        for (int row=0;row<16;++row) for(int col=0;col<16;++col) {
            const auto *cell=table->item(15-row,col);bool ok=false;
            values.cells[size_t(row*16+col)]=cell?cell->text().toDouble(&ok):0;valid &= ok;
        }
        QString error;const QByteArray desired=tunuino::MapPageCodec::encode(page,values,error);
        if (!valid || desired.isEmpty()) { QMessageBox::warning(this,tr("Invalid ECU map"),error.isEmpty()?tr("All map values must be numeric."):error);return; }
        const QString name=page==2?tr("Fuel Table"):tr("Ignition Table");
        if (QMessageBox::question(this,tr("Write to ECU"),tr("Save %1 to the connected ECU? The engine must be stopped and outputs disarmed. Other settings will be preserved.").arg(name),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes) return;
        if (!*ecuOnline || !*identityVerified || !session->isOpen() || transfer->page
            || *ecuIdentity!=base.identity || *ecuGeneration!=base.generation) {
            statusBar()->showMessage(tr("ECU changed while confirming. Wait for a fresh read before writing."),15000);return;
        }
        pendingWrite->sensor=-1;*activeSensorWrite=-1;*activeParameterFinished={};
        pendingWrite->page=page;pendingWrite->desired=desired;pendingWrite->base=base.bytes;
        pendingWrite->identity=base.identity;pendingWrite->generation=base.generation;
        *writePending=true;setProperty("mapWriteBusy",true);writeWait->start();liveTimer->stop();validationAction->setEnabled(false);
        fuelRead->setEnabled(false);ignitionRead->setEnabled(false);if(tpsReadButton)tpsReadButton->setEnabled(false);
        freezeMapEditing(true);updateWriteAvailability();writeLaunch->start();
        statusBar()->showMessage(tr("Checking ECU and preparing %1 write...").arg(name));
    };
    for(auto *panel:*sensorPanels)panel->writeRequested=[=](tunuino::SensorKind kind,const QByteArray &desired){
        if(!*ecuOnline||!*ecuClean||!*identityVerified||!session->isOpen()||transfer->page||*validationPending||writer->busy()||*writePending||*toolBusy||*toolReply)return;
        const auto base=*sensorBase;
        if(base.header.size()!=256||base.aux.size()!=256||base.identity!=*ecuIdentity||base.generation!=*ecuGeneration)return;
        const QString name=tunuino::SensorCalibrationCodec::name(kind);
        if(QMessageBox::question(this,tr("Write sensor to ECU"),tr("Save %1 calibration to the ECU? Engine must be stopped and outputs disarmed. Other calibrations and maps will be preserved.").arg(name),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
        if(!*ecuOnline||!*identityVerified||!session->isOpen()||transfer->page||*ecuIdentity!=base.identity||*ecuGeneration!=base.generation)return;
        pendingWrite->sensor=int(kind);*activeSensorWrite=int(kind);*activeParameterFinished={};pendingWrite->page=tunuino::SensorCalibrationCodec::page(kind);
        pendingWrite->desired=desired;pendingWrite->base=tunuino::SensorCalibrationCodec::auxiliary(kind)?base.aux:base.header;
        pendingWrite->identity=base.identity;pendingWrite->generation=base.generation;
        *writePending=true;setProperty("mapWriteBusy",true);writeWait->start();liveTimer->stop();validationAction->setEnabled(false);
        fuelRead->setEnabled(false);ignitionRead->setEnabled(false);if(tpsReadButton)tpsReadButton->setEnabled(false);
        freezeMapEditing(true);updateWriteAvailability();writeLaunch->start();
        statusBar()->showMessage(tr("Checking ECU and preparing %1 calibration...").arg(name));
    };
    const auto queueParameters=[=](const QString &title,int page,const QByteArray &desired,const QByteArray &mask,std::function<void(bool,const QString&)> done)->bool{
        if(!*ecuOnline||!*ecuClean||!*identityVerified||!session->isOpen()||transfer->page||*validationPending||writer->busy()||*writePending||*toolBusy||*toolReply)return false;
        const auto base=*sensorBase;if(base.identity!=*ecuIdentity||base.generation!=*ecuGeneration)return false;
        if(QMessageBox::question(this,tr("Write parameters to ECU"),tr("Save %1? Engine must be stopped and outputs disarmed. Only this block is written; other window edits are separate.").arg(title),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return false;
        if(!*ecuOnline||!*identityVerified||!session->isOpen()||transfer->page||*ecuIdentity!=base.identity||*ecuGeneration!=base.generation)return false;
        pendingWrite->sensor=-2;*activeSensorWrite=-1;*activeParameterFinished=std::move(done);pendingWrite->page=page;pendingWrite->desired=desired;pendingWrite->mask=mask;pendingWrite->base=page==1?base.header:base.aux;pendingWrite->identity=base.identity;pendingWrite->generation=base.generation;
        *writePending=true;setProperty("mapWriteBusy",true);writeWait->start();liveTimer->stop();validationAction->setEnabled(false);freezeMapEditing(true);updateWriteAvailability();writeLaunch->start();return true;
    };
    for(auto *panel:*parameterPanels)panel->writeRequested=[=](int page,const QByteArray &desired,const QByteArray &mask){queueParameters(panel->block().title,page,desired,mask,[panel](bool success,const QString&){if(success)panel->saved();});};
    triggerWizard->writeRequested=[=](int page,const QByteArray &desired,const QByteArray &mask){return queueParameters(tr("Ignition - Trigger wizard"),page,desired,mask,[triggerWizard](bool success,const QString &message){triggerWizard->writeFinished(success,message);});};
    connect(fuelSave,&QPushButton::clicked,this,[=] { queueMapWrite(2); });
    connect(ignitionSave,&QPushButton::clicked,this,[=] { queueMapWrite(3); });
    connect(workspace,&QMdiArea::subWindowActivated,this,[=](QMdiSubWindow*) { updateWriteAvailability(); });
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
                    if (property("mapWriteBusy").toBool()||property("ecuToolBusy").toBool()) {
                        QMessageBox::information(this,tr("Console update"),tr("Finish or stop the active ECU tool/write before installing an update."));return;
                    }
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
            tr("Tunuino Console — Qt 6\\nMap editing, ECU monitoring and verified map/sensor calibration writes."));
    });
    auto *ecuMenu = menuBar()->addMenu(tr("ECU"));
    ecuMenu->addAction(validationAction);
    auto *writeActiveMap=ecuMenu->addAction(tr("Write to ECU"));
    *writeActionRef=writeActiveMap;writeActiveMap->setShortcut(QKeySequence(QStringLiteral("F2")));
    writeActiveMap->setEnabled(false);
    connect(writeActiveMap,&QAction::triggered,this,[=] {
        const auto *window=workspace->activeSubWindow();auto *page=window?window->widget():nullptr;
        if(page==fuelPage || page==fuelSurface)fuelSave->click();
        else if(page==ignitionPage || page==ignitionSurface)ignitionSave->click();
        else if(auto *sensor=dynamic_cast<tunuino::SensorCalibrationWidget*>(page))sensor->save();
        else if(auto *parameters=dynamic_cast<tunuino::ParameterWidget*>(page))parameters->save();
        else if(page==triggerWizard)triggerWizard->save();
    });

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
    auto *openFirmware = ecuMenu->addAction(tr("Firmware / USB DFU"));
    connect(openFirmware, &QAction::triggered, this, [=]() {
        if (writer->busy() || *writePending) return;
        openSettingsPage(5);
        session->close();
    });
    auto *dfuInfo = ecuMenu->addAction(tr("USB DFU status / instructions"));
    connect(dfuInfo, &QAction::triggered, this, [this]() {
        QMessageBox::information(this, tr("Firmware / DFU"),
            tr("Enter USB DFU bootloader mode, select a verified firmware image and detect the device. Flash and verify programs the selected image and verifies it by reading it back."));
    });
    tunuino::AutoConnection::Hooks autoConnectionHooks;
    autoConnectionHooks.ports = [] {
        QVector<tunuino::AutoPort> result;
        for (const auto &port : QSerialPortInfo::availablePorts())
            result.append({port.portName(), port.vendorIdentifier(), port.productIdentifier()});
        return result;
    };
    autoConnectionHooks.isOpen = [session] { return session->isOpen(); };
    autoConnectionHooks.suspended = [firmwarePage] { return firmwarePage->isVisible(); };
    autoConnectionHooks.close = [session] { session->close(); };
    autoConnectionHooks.begin = [=](const QString &name) {
        liveTimer->stop();
        transfer->page = 0;
        transfer->offset = 0;
        transfer->bytes.clear();
        *identityVerified = false;
        *autoSync = false;
        powerState->reset();
        ecuIdentity->clear();
        connectionStatus_->setText(tr("Offline | Detecting ECU"));
        if (!session->open(name, 115200)) return false;
        if (!session->exchange(QByteArray("TUNUINO?"), 1500)) {
            session->close();
            return false;
        }
        return true;
    };
    auto *autoConnection = new tunuino::AutoConnection(autoConnectionHooks, this);
    connect(session, &tunuino::SerialSession::connectionChanged, this, [=](bool connected) {
        setProperty("ecuTransportOpen", connected);
        if (!connected) {
            *writePending=false; *readBusy=false; setProperty("mapWriteBusy",false); freezeMapEditing(false);
            (*mapBases)[0]={}; (*mapBases)[1]={}; *sensorBase={}; *ecuGeneration=0; *ecuClean=false;
            writer->transportFailed(tr("ECU disconnected during map write."));
            setConnectionIndicator(false);
            powerState->reset();
            ecuIdentity->clear();
        }
        if (!connected) { *autoSync = false; liveTimer->stop(); transfer->page = 0; *identityVerified = false; *validationPending = false; validationAction->setEnabled(false); fuelRead->setEnabled(false); ignitionRead->setEnabled(false); if (tpsReadButton) tpsReadButton->setEnabled(false); connectionStatus_->setText(tr("Offline | Waiting for ECU")); updateWriteAvailability(); }
    });
    connect(session, &tunuino::SerialSession::exchangeFailed, this, [=](const QString &error) {
        writer->transportFailed(error);
        setConnectionIndicator(false);
        updateWriteAvailability();
        liveTimer->stop();
        transfer->page = 0;
        *autoSync = false;
        *identityVerified = false;
        *validationPending = false;
        validationAction->setEnabled(false);
        fuelRead->setEnabled(false);
        ignitionRead->setEnabled(false);
        if (tpsReadButton) tpsReadButton->setEnabled(false);
        connectionStatus_->setText(tr("Offline | Waiting for ECU"));
        statusBar()->showMessage(error, 10000);
    });
    connect(session, &tunuino::SerialSession::responseReceived, this, [=](const QByteArray &data) {
        if(*toolReply){auto callback=std::move(*toolReply);*toolReply={};const bool runtime=*toolRuntime;callback(data);if(!runtime)return;}
        if (writer->busy()) { writer->receive(data); return; }
        if (*validationPending) {
            *validationPending = false;
            liveTimer->start(); updateWriteAvailability();
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
            const int total = (transfer->page == 1 || transfer->page == 4) ? 256 : 576;
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
                const bool schemaValid=u16(0)==2;
                if(schemaValid)sensorBase->header=transfer->bytes;
                if(schemaValid&&*autoSync){transfer->page=0;transfer->offset=0;transfer->bytes.clear();QTimer::singleShot(0,this,[=]{startTransfer(2);});}
                else abortTransfer(schemaValid?tr("Configuration page read"):tr("Unsupported ECU calibration schema"));
                return;
            }
            if(transfer->page==4){
                sensorBase->aux=transfer->bytes;sensorBase->identity=*ecuIdentity;sensorBase->generation=*ecuGeneration;
                if(!*projectLoaded&&sensorBase->header.size()==256){*projectPages=QJsonObject{{"header",QString::fromLatin1(sensorBase->header.toHex())},{"fuel",QString::fromLatin1((*mapBases)[0].bytes.toHex())},{"ignition",QString::fromLatin1((*mapBases)[1].bytes.toHex())},{"aux",QString::fromLatin1(sensorBase->aux.toHex())}};}
                if(sensorBase->header.size()==256){for(auto *panel:*inputPanels)panel->load(sensorBase->header,sensorBase->aux,*ecuIdentity,*ecuGeneration);for(auto *panel:*sensorPanels)panel->load(sensorBase->header,sensorBase->aux,*ecuIdentity,*ecuGeneration);for(auto *panel:*parameterPanels)panel->load(sensorBase->header,sensorBase->aux,*ecuIdentity,*ecuGeneration);triggerWizard->load(sensorBase->header,sensorBase->aux,*ecuIdentity,*ecuGeneration);}
                abortTransfer(tr("ECU synchronized: Fuel, Ignition and sensor calibrations."));return;
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
            (*mapBases)[size_t(page-2)]={transfer->bytes,*ecuIdentity,*ecuGeneration};
            auto *table = page == 2 ? maps : ignitionMap;
            QStringList rpmLabels, loadLabels;
            for (int i = 0; i < 16; ++i) {
                rpmLabels.append(QString::number(u16(2 * i)));
                loadLabels.append(QString::number(u16(32 + 2 * i) / 100.0, 'f', 1));
            }
            // Rows are MAP load (kPa); columns are engine speed (RPM).
            // Firmware storage is load-major: cells[load][rpm].
            auto *rpmValues = (table == maps) ? fuelRpmValues : ignitionRpmValues;
            if(!*projectLoaded){
            for (int i = 0; i < 16; ++i)
                rpmValues->item(0, i)->setText(rpmLabels.at(i));
            std::reverse(loadLabels.begin(), loadLabels.end());
            table->setVerticalHeaderLabels(loadLabels);
            for (int row = 0; row < 16; ++row)
                for (int col = 0; col < 16; ++col)
                    table->item(15 - row, col)->setText(QString::number(qint16(u16(64 + 2 * (row * 16 + col))) / 100.0, 'f', 2));
            }
            // Force the independent 3D view to repaint after the complete ECU map arrives.
            (page == 2 ? fuelSurface : ignitionSurface)->update();
            if (*autoSync && (page == 2 || page == 3)) {
                transfer->page = 0;
                transfer->offset = 0;
                transfer->bytes.clear();
                QTimer::singleShot(0, this, [=]() { startTransfer(page+1); });
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
                session->close();
                statusBar()->showMessage(tr("Offline | Invalid ECU runtime response; reconnecting automatically."));
                return;
            }
            if (data.mid(1, 31) != ecuIdentity->mid(1, 31)) {
                session->close();
                return;
            }
            const bool previouslyClean=*ecuClean;
            *ecuClean=(quint8(data[40])&2)==0;
            const quint32 previousGeneration=*ecuGeneration;
            *ecuGeneration=(quint32(quint8(data[57]))<<24)|(quint32(quint8(data[58]))<<16)
                |(quint32(quint8(data[59]))<<8)|quint32(quint8(data[60]));
            const bool wasOnline = *ecuOnline;
            const bool powered = powerState->update(data, connectionClock->elapsed());
            if (!powered) {
                setConnectionIndicator(false);
                validationAction->setEnabled(false);
                fuelRead->setEnabled(false);
                ignitionRead->setEnabled(false);
                if (tpsReadButton) tpsReadButton->setEnabled(false);
                updateWriteAvailability();
                connectionStatus_->setText(tr("Offline | Waiting for ECU power"));
                statusBar()->showMessage(tr("OFFLINE | USB detected; waiting for external ECU power"));
                return;
            }
            if (!wasOnline) {
                setConnectionIndicator(true);
                connectionStatus_->setText(tr("Online | ECU connected"));
                validationAction->setEnabled(true);
                if (*ecuClean) { *autoSync = true; startTransfer(1); }
            }
            if (*ecuClean && !previouslyClean) connectionStatus_->setText(tr("Online | ECU connected"));
            if (!*ecuClean) {
                (*mapBases)[0]={}; (*mapBases)[1]={};*sensorBase={};
                connectionStatus_->setText(tr("Online | Temporary ECU changes; reset ECU before writing"));
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
            // Auxiliary readings use stored calibrations and the actual selected ADC inputs.
            // Never manufacture these readings from unrelated raw ADC channels.
            double calibratedTps=0;
            const bool tpsValid=sensorBase->generation==*ecuGeneration&&sensorBase->identity==*ecuIdentity
                &&tunuino::SensorCalibrationCodec::tpsPercent(sensorBase->aux,data,calibratedTps);
            setReading(1,tpsValid,QString::number(calibratedTps,'f',2)+QStringLiteral(" %"));
            double calibratedIat=0,calibratedBattery=0;
            const bool sameSensorBase=sensorBase->generation==*ecuGeneration&&sensorBase->identity==*ecuIdentity;
            const bool iatValid=sameSensorBase&&tunuino::SensorCalibrationCodec::auxiliaryReading(tunuino::SensorKind::Iat,sensorBase->aux,data,calibratedIat);
            const bool batteryValid=sameSensorBase&&tunuino::SensorCalibrationCodec::auxiliaryReading(tunuino::SensorKind::Battery,sensorBase->aux,data,calibratedBattery);
            setReading(5,iatValid,QString::number(calibratedIat,'f',2)+QStringLiteral(" C"));
            setReading(6,batteryValid,QString::number(calibratedBattery,'f',2)+QStringLiteral(" V"));
            std::array<double, tunuino::LogChannelCount> logValues;
            logValues.fill(std::numeric_limits<double>::quiet_NaN());
            if ((mask & 1) || stoppedEngine) logValues[0] = rpmMilli / 1000.0;
            if(tpsValid)logValues[1]=calibratedTps;
            if(iatValid)logValues[5]=calibratedIat;
            if(batteryValid)logValues[6]=calibratedBattery;
            if (mask & 2) logValues[2] = u32(1 + 44) / 1000.0;
            if (mask & 4) logValues[3] = u32(1 + 48) / 1000000.0;
            if (mask & 8) logValues[4] = qint32(u32(1 + 52)) / 1000.0;
            graphLog->setConnected(true);
            graphLog->appendRuntime(logValues);
            tuneDisplay->setConnected(true);
            const double tuneTarget=sameSensorBase?tunuino::ecuTargetLambda(sensorBase->aux,data):std::numeric_limits<double>::quiet_NaN();
            tuneDisplay->setReadings(logValues,tuneTarget);
            autotuneTool->sample(logValues,tuneTarget);
            for(auto *panel:*sensorPanels)panel->updateRaw(data);for(auto *panel:*inputPanels)panel->updateRaw(data);
            if (*ecuClean && previousGeneration && (previousGeneration!=*ecuGeneration || !previouslyClean || *autoSync) && !transfer->page && !*writePending) {
                (*mapBases)[0]={}; (*mapBases)[1]={}; *sensorBase={}; *autoSync=true;
                QTimer::singleShot(0,this,[=] { startTransfer(1); });
            }
            if (!transfer->page) updateWriteAvailability();
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
        if (!tunuino::EcuPowerState::supportedIdentity(data)) {
            session->close();
            connectionStatus_->setText(tr("Offline | Waiting for a supported Tunuino ECU"));
            return;
        }
        *ecuIdentity = data;
        *identityVerified = true;
        powerState->reset();
        connectionStatus_->setText(tr("Offline | Waiting for ECU power"));
        liveTimer->start();
        requestLive();
    });
    autoConnection->start();
    statusBar()->showMessage(tr("OFFLINE | Waiting for ECU — automatic connection"));
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (property("mapWriteBusy").toBool()||property("ecuToolBusy").toBool()) {
        statusBar()->showMessage(tr("Finish or stop the active ECU tool/write before closing the Console."));
        event->ignore();return;
    }
    QMainWindow::closeEvent(event);
}
