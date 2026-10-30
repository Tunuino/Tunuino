#include <QApplication>
#include <QStyleFactory>
#include "MainWindow.h"
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Tunuino Console");
    QApplication::setOrganizationName("Tunuino");
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setStyleSheet(R"QSS(
        QMainWindow, QWidget { background: #121a24; color: #e3ebf5; font-family: "Segoe UI"; font-size: 10pt; }
        QMenuBar { background: #101720; padding: 5px; border-bottom: 1px solid #304052; }
        QMenuBar::item:selected, QMenu::item:selected { background: #1b6fa8; }
        QMenu { background: #1b2633; border: 1px solid #405368; padding: 5px; }
        QTabWidget::pane { border: 1px solid #34495d; background: #172331; }
        QTabBar::tab { background: #202f3e; padding: 11px 17px; margin-right: 2px; color: #b9ccda; }
        QTabBar::tab:selected { background: #137eb8; color: white; border-bottom: 2px solid #50c6fa; }
        QDockWidget { color: #e3ebf5; font-weight: 600; }
        QDockWidget::title { background: #1e2d3b; padding: 9px; }
        QTreeWidget, QListWidget, QTableWidget { background: #172432; alternate-background-color: #1d3040; border: 1px solid #34485a; gridline-color: #35485a; }
        QTreeWidget::item, QListWidget::item { padding: 7px; }
        QTreeWidget::item:selected, QListWidget::item:selected { background: #147bb3; color: white; }
        QHeaderView::section { background: #263b4e; color: #e0eaf2; padding: 7px; border: 1px solid #34495b; }
        QLineEdit, QDoubleSpinBox, QComboBox { background: #203142; color: white; border: 1px solid #4b6479; border-radius: 4px; padding: 6px; }
        QLineEdit:focus, QDoubleSpinBox:focus, QComboBox:focus { border: 1px solid #40b9f1; }
        QComboBox QAbstractItemView { background: #203142; color: white; selection-background-color: #147bb3; }
        QPushButton { background: #176d9f; color: white; border: 1px solid #3698ce; border-radius: 5px; padding: 8px 14px; }
        QPushButton:hover { background: #228ac4; }
        QPushButton:disabled { background: #283747; color: #8195a7; border-color: #35495a; }
        QGroupBox { border: 1px solid #3d5569; border-radius: 5px; margin-top: 13px; padding: 12px; font-weight: 600; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }
        QStatusBar { background: #0d1720; color: #9ac8e4; border-top: 1px solid #31465a; }
        QSplitter::handle { background: #35485a; }
    )QSS");
    MainWindow window;
    window.show();
    return app.exec();
}
