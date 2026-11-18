#pragma once
#include "../ui/WriteButtonState.h"
#include "ProteusPinoutWidget.h"
#include <QAbstractItemView>
#include <QRegularExpression>
#include <QSplitter>
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
        auto *hint = new QLabel(tr("Choose an ECU connection in the list below. Configure opens the function settings; Write to ECU saves the changes."), this);
        hint->setWordWrap(true); layout->addWidget(hint);
        auto *splitter = new QSplitter(Qt::Horizontal, this);
        splitter->setObjectName("ioPinoutSplitter");
        splitter->setChildrenCollapsible(false);
        table_ = new QTableWidget(0, 4, this);
        table_->setObjectName("ioAssignments");
        table_->setHorizontalHeaderLabels({tr("Sensor / actuator"), tr("ECU connection"),
                                           tr("Configuration"), tr("ECU")});
        table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        table_->verticalHeader()->hide();
        table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        table_->setSelectionMode(QAbstractItemView::SingleSelection);
        splitter->addWidget(table_);
        pinout_ = new ProteusPinoutWidget(splitter);
        splitter->addWidget(pinout_); splitter->setSizes({570,430});
        splitter->setStretchFactor(0, 3); splitter->setStretchFactor(1, 2);
        table_->setMinimumWidth(0); pinout_->setMinimumWidth(0);
        layout->addWidget(splitter, 1);
        auto *state = new QLabel(tr("Connections shown are the current editor assignments. Pending changes require Write to ECU."), this);
        state->setWordWrap(true); layout->addWidget(state);
        connect(table_, &QTableWidget::currentCellChanged, this, [this](int row,int,int,int) {
            if(row>=0 && row<bindings_.size()) { selectedConnections_=contacts(bindings_[row]); pinout_->highlight(selectedConnections_); }
        });
        pinout_->contactSelected = [this](const QString &contact) {
            for(int row=0;row<bindings_.size();++row) if(bindings_[row].visible() && contacts(bindings_[row]).contains(contact)) {
                table_->selectRow(row); table_->scrollToItem(table_->item(row,0)); return;
            }
        };
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
        binding.choice->setToolTip(tr("Select the physical ECU pin for this function. Changes remain local until Write to ECU."));
        binding.choice->setStyleSheet("QComboBox { border: 1px solid #696f76; padding: 3px 20px 3px 5px; }");
        table_->setCellWidget(row, 1, binding.choice);
        auto *configure = new QPushButton(tr("Configure..."), table_);
        connect(configure, &QPushButton::clicked, this, binding.configure);
        table_->setCellWidget(row, 2, configure);
        binding.write = new QPushButton(tr("Write to ECU"), table_);
        table_->setCellWidget(row, 3, binding.write);
        connect(binding.write, &QPushButton::clicked, binding.sourceWrite, &QPushButton::click);
        connect(binding.choice, &QComboBox::activated, this, [this, row](int selected) {
            auto &binding = bindings_[row];
            table_->setCurrentCell(row,0);
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
        QMap<QString,QStringList> assignments;
        for (int i = 0; i < bindings_.size(); ++i) {
            const auto &binding = bindings_[i];
            table_->setRowHidden(i, !binding.visible());
            if (binding.label) table_->item(i,0)->setText(binding.label());
            QSignalBlocker blocker(binding.choice);
            if(!binding.choice->view()->isVisible()) binding.choice->setCurrentIndex(binding.current());
            // Offline editing is independent of whether an ECU write is available.
            binding.choice->setEnabled(binding.connections.size()>1);
            binding.write->setEnabled(binding.sourceWrite->isEnabled());
            const bool pending = binding.sourceWrite->property("ecuWritePending").toBool();
            showPendingWrite(binding.write, pending);
            if(binding.visible()) for(const auto &contact : contacts(binding)) {
                const QString name = binding.label ? binding.label() : binding.name;
                assignments[contact] << (pending ? tr("%1 (pending)").arg(name) : name);
            }
        }
        pinout_->setAssignments(assignments);
        const int selected = table_->currentRow();
        if(selected>=0 && selected<bindings_.size()) {
            const auto updated=contacts(bindings_[selected]);
            if(updated!=selectedConnections_) {selectedConnections_=updated; pinout_->highlight(updated);}
        }
    }

private:
    static QStringList contacts(const Binding &binding) {
        const int current=binding.current();
        if(current<0 || current>=binding.connections.size() || (current==0 && binding.connections.size()>1)) return {};
        const auto text=binding.connections[current];
        const auto match=QRegularExpression("(J10[123])\\s*/\\s*(.+)").match(text);
        if(!match.hasMatch()) return {};
        const QString tail=match.captured(2).section(QChar(0x2014),0,0);
        auto numbers=QRegularExpression("\\b[0-9]{1,2}\\b").globalMatch(tail);
        QStringList result;
        const int max=match.captured(1)=="J101"?23:35;
        while(numbers.hasNext()) { const int pin=numbers.next().captured().toInt();
            if(pin>=1&&pin<=max) result<<match.captured(1)+"/"+QString::number(pin);
        }
        result.removeDuplicates(); return result;
    }
    QTableWidget *table_;
    ProteusPinoutWidget *pinout_;
    QVector<Binding> bindings_;
    QStringList selectedConnections_;
};
}
