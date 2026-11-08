#include <QApplication>
#include <QIcon>
#include <QStyleFactory>
#include "MainWindow.h"
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Tunuino Console");
    QApplication::setApplicationVersion(QStringLiteral(TUNUINO_CONSOLE_VERSION));
    QApplication::setWindowIcon(QIcon(":/branding/tunuino.png"));
    QApplication::setOrganizationName("Tunuino");
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setStyleSheet(R"QSS(
        /* Graphite motorsport workspace: neutral panels and blue selection. */
        QMainWindow, QWidget { background: #242629; color: #e1e1e1; font-family: "Segoe UI"; font-size: 9pt; }
        QMenuBar { background: #666764; color: #efefea; font-size: 10pt; padding: 1px; border-bottom: 1px solid #a0a19a; }
        QMenuBar::item { padding: 4px 12px; }
        QMenuBar::item:selected, QMenu::item:selected { background: #0078d7; color: #ffffff; }
        QMenu { background: #414343; color: #efefea; font-size: 10pt; border: 1px solid #b1b5ad; padding: 3px; }
        QMenu::item { padding: 5px 24px 5px 32px; }
        QMenu::separator { height: 1px; background: #92968f; margin: 4px 5px 4px 31px; }
        QTabWidget::pane { border: 1px solid #45474a; background: #292b2e; }
        QTabBar::tab { background: #34363a; border: 1px solid #4a4c50; border-bottom: 0; padding: 8px 15px; margin-right: 1px; color: #d2d2d2; }
        QTabBar::tab:selected { background: #4a4c50; color: #ffffff; border-top: 2px solid #168bd2; }
        QDockWidget { color: #e5e5e5; font-weight: 600; }
        QDockWidget::title { background: #303236; padding: 7px; border-bottom: 1px solid #515357; }
        QTreeWidget, QListWidget, QTableWidget { background: #292b2e; alternate-background-color: #303236; color: #e5e5e5; border: 1px solid #4a4c50; gridline-color: #4c4e52; selection-background-color: #0a70b1; selection-color: white; }
        QTreeWidget::item, QListWidget::item { padding: 6px; }
        QTreeWidget::item:selected, QListWidget::item:selected { background: #0078d7; color: white; }
        QHeaderView::section { background: #3b3d40; color: #e7e7e7; padding: 5px; border: 1px solid #525458; }
        QLineEdit, QDoubleSpinBox, QSpinBox, QComboBox { background: #34363a; color: #f0f0f0; border: 1px solid #65676a; border-radius: 2px; padding: 5px; }
        QLineEdit:focus, QDoubleSpinBox:focus, QComboBox:focus { border: 1px solid #168bd2; }
        QComboBox QAbstractItemView { background: #34363a; color: white; selection-background-color: #0078d7; }
        QPushButton { background: #45474b; color: #f0f0f0; border: 1px solid #696b6e; border-radius: 2px; padding: 7px 12px; }
        QPushButton:hover { background: #626469; border-color: #2294cf; }
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
