#include <QApplication>
#include "MainWindow.h"
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Tunuino Console");
    QApplication::setOrganizationName("Tunuino");
    MainWindow window;
    window.show();
    return app.exec();
}
