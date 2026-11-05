#include "logging/GraphLogWidget.h"
#include <QApplication>
#include <QPushButton>
#include <QTableWidget>
#include <QLabel>
#include <cstdlib>
#include <iostream>
#define CHECK(condition) do { if (!(condition)) { std::cerr << "Failed: " << #condition << " at " << __LINE__ << std::endl; std::exit(1); } } while (false)

int main(int argc, char **argv) {
    QApplication app(argc,argv);
    tunuino::GraphLogWidget widget;
    auto button=[&](const QString &text) -> QPushButton* {
        for (auto *b:widget.findChildren<QPushButton*>()) if(b->text()==text) return b;
        std::exit(2);
    };
    auto *table=widget.findChild<QTableWidget*>();
    CHECK(!button("Start logging")->isEnabled());
    widget.setConnected(true);
    button("Start logging")->click();
    std::array<double,tunuino::LogChannelCount> values;
    values.fill(std::numeric_limits<double>::quiet_NaN());
    values[0]=0; values[2]=101.3;
    widget.appendRuntime(values);
    CHECK(table->item(0,1)->text()=="0");
    CHECK(table->item(1,1)->text()=="Unavailable");
    button("Pause display")->click();
    values[0]=1500;
    widget.appendRuntime(values);
    CHECK(table->item(0,1)->text()=="0");
    button("Resume display")->click();
    CHECK(table->item(0,1)->text()=="1500");
    widget.setConnected(false);
    CHECK(!button("Start logging")->isEnabled());
    values[0]=4000;
    widget.appendRuntime(values);
    CHECK(table->item(0,1)->text()=="1500");
    widget.resize(1000,700); widget.show(); app.processEvents();
    CHECK(!widget.grab().isNull());
    std::cout << "Graph Log capture, pause/resume, missing channels, disconnect and rendering: passed\n";
}
