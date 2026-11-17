#pragma once
#include "../ui/WriteButtonState.h"
#include <QComboBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <functional>

namespace tunuino {
class IoConfigurationWidget final : public QWidget {
public:
    std::function<void(const QString &)> openConfiguration;
    struct Binding {
        QString name;
        QString resource;
        QStringList connections;
        std::function<int()> current;
        std::function<bool(int, QString &)> assign;
        std::function<bool()> visible;
        std::function<void()> configure;
        QPushButton *sourceWrite = nullptr;
        std::function<QString()> label;
        QComboBox *choice = nullptr;
        QPushButton *write = nullptr;
    };

    explicit IoConfigurationWidget(QWidget *parent = nullptr) : QWidget(parent) {
        setObjectName("ioConfiguration");
        auto *layout = new QVBoxLayout(this);
        table_ = new QTableWidget(0, 4, this);
        table_->setObjectName("ioAssignments");
        table_->setHorizontalHeaderLabels({tr("Sensor / actuator"), tr("Proteus connection"),
                                           tr("Configuration"), tr("ECU")});
        table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        table_->verticalHeader()->hide();
        table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        layout->addWidget(table_);
        auto *timer = new QTimer(this);
        timer->setInterval(200);
        connect(timer, &QTimer::timeout, this, [this] { if (isVisible()) refresh(); });
        timer->start();
    }

    void add(Binding binding) {
        const int row = bindings_.size();
        table_->insertRow(row);
        table_->setItem(row, 0, new QTableWidgetItem(binding.name));
        binding.choice = new QComboBox(table_);
        binding.choice->addItems(binding.connections);
        table_->setCellWidget(row, 1, binding.choice);
        auto *configure = new QPushButton(tr("Configure..."), table_);
        connect(configure, &QPushButton::clicked, this, binding.configure);
        table_->setCellWidget(row, 2, configure);
        binding.write = new QPushButton(tr("Write to ECU"), table_);
        table_->setCellWidget(row, 3, binding.write);
        connect(binding.write, &QPushButton::clicked, binding.sourceWrite, &QPushButton::click);
        connect(binding.choice, &QComboBox::activated, this, [this, row](int selected) {
            auto &binding = bindings_[row];
            for (int i = 0; i < bindings_.size(); ++i) {
                const auto &other = bindings_[i];
                if (i != row && selected && other.resource == binding.resource &&
                    other.visible() && other.current() == selected) {
                    QMessageBox::warning(this, tr("Connection occupied"),
                        tr("%1 already uses this connection.").arg(other.name));
                    refresh();
                    return;
                }
            }
            QString error;
            if (!binding.assign(selected, error))
                QMessageBox::warning(this, tr("Connection assignment"), error);
            refresh();
        });
        bindings_.append(std::move(binding));
        refresh();
    }

    void refresh() {
        for (int i = 0; i < bindings_.size(); ++i) {
            const auto &binding = bindings_[i];
            table_->setRowHidden(i, !binding.visible());
            if (binding.label) table_->item(i,0)->setText(binding.label());
            QSignalBlocker blocker(binding.choice);
            binding.choice->setCurrentIndex(binding.current());
            binding.choice->setEnabled(binding.sourceWrite->isEnabled());
            binding.write->setEnabled(binding.sourceWrite->isEnabled());
            showPendingWrite(binding.write, binding.sourceWrite->property("ecuWritePending").toBool());
        }
    }

private:
    QTableWidget *table_;
    QVector<Binding> bindings_;
};
}
