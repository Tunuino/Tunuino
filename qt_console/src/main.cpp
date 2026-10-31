#include <QApplication>
#include <QIcon>
#include <QStyleFactory>
#include "MainWindow.h"
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Tunuino Console");
    QApplication::setWindowIcon(QIcon(":/branding/tunuino.png"));
    QApplication::setOrganizationName("Tunuino");
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setStyleSheet(R"QSS(
        /* Graphite motorsport workspace: neutral panels, amber active accents. */
        QMainWindow, QWidget { background: #242629; color: #e1e1e1; font-family: "Segoe UI"; font-size: 9pt; }
        QMenuBar { background: #191b1d; color: #e4e4e4; padding: 3px; border-bottom: 1px solid #424448; }
        QMenuBar::item:selected, QMenu::item:selected { background: #b97822; color: #ffffff; }
        QMenu { background: #292b2e; border: 1px solid #515358; padding: 4px; }
        QTabWidget::pane { border: 1px solid #45474a; background: #292b2e; }
        QTabBar::tab { background: #34363a; border: 1px solid #4a4c50; border-bottom: 0; padding: 8px 15px; margin-right: 1px; color: #d2d2d2; }
        QTabBar::tab:selected { background: #4a4c50; color: #ffffff; border-top: 2px solid #e69a32; }
        QDockWidget { color: #e5e5e5; font-weight: 600; }
        QDockWidget::title { background: #303236; padding: 7px; border-bottom: 1px solid #515357; }
        QTreeWidget, QListWidget, QTableWidget { background: #292b2e; alternate-background-color: #303236; color: #e5e5e5; border: 1px solid #4a4c50; gridline-color: #4c4e52; selection-background-color: #8b5a20; selection-color: white; }
        QTreeWidget::item, QListWidget::item { padding: 6px; }
        QTreeWidget::item:selected, QListWidget::item:selected { background: #a66d25; color: white; }
        QHeaderView::section { background: #3b3d40; color: #e7e7e7; padding: 5px; border: 1px solid #525458; }
        QLineEdit, QDoubleSpinBox, QSpinBox, QComboBox { background: #34363a; color: #f0f0f0; border: 1px solid #65676a; border-radius: 2px; padding: 5px; }
        QLineEdit:focus, QDoubleSpinBox:focus, QComboBox:focus { border: 1px solid #e69a32; }
        QComboBox QAbstractItemView { background: #34363a; color: white; selection-background-color: #a66d25; }
        QPushButton { background: #45474b; color: #f0f0f0; border: 1px solid #696b6e; border-radius: 2px; padding: 7px 12px; }
        QPushButton:hover { background: #626469; border-color: #d28b2e; }
        QPushButton:disabled { background: #303236; color: #8c8d90; border-color: #45474a; }
        QGroupBox { border: 1px solid #525458; border-radius: 2px; margin-top: 13px; padding: 10px; font-weight: 600; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }
        QStatusBar { background: #191b1d; color: #d9d9d9; border-top: 1px solid #494b4e; }
        QSplitter::handle { background: #515357; }
        QScrollBar { background: #292b2e; }
        QScrollBar::handle { background: #606267; min-height: 18px; min-width: 18px; }
    )QSS");
    MainWindow window;
    window.show();
    return app.exec();
}
