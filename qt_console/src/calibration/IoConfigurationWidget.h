#pragma once
#include "../ui/WriteButtonState.h"
#include "ProteusPinoutWidget.h"
#include <QAbstractItemView>
#include <QShowEvent>
#include <QFormLayout>
#include <QRegularExpression>
#include <QSplitter>
#include <QComboBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <functional>

namespace tunuino {
class IoConfigurationWidget final : public QWidget {
public:
    std::function<void(const QString &)> openConfiguration;
    std::function<bool()> configurationAllowed;
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
        QLabel *roleLabel = nullptr;
        QString connectionHelp;
    };

    explicit IoConfigurationWidget(QWidget *parent = nullptr) : QWidget(parent) {
        setObjectName("ioConfiguration");
        auto *layout = new QVBoxLayout(this);
        auto *hint = new QLabel(tr("Select a connector pin or open a function to configure its connection."), this);
        hint->setWordWrap(true); layout->addWidget(hint);
        auto *splitter = new QSplitter(Qt::Horizontal, this);
        splitter->setObjectName("ioPinoutSplitter");
        splitter->setChildrenCollapsible(false);
        table_ = new QTableWidget(0, 3, this);
        table_->setObjectName("ioAssignments");
        table_->setHorizontalHeaderLabels({tr("Sensor / actuator"), tr("ECU connection"),
                                           tr("Configuration")});
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
            if(row>=0 && row<bindings_.size()) { selectedConnections_=displayContacts(bindings_[row]); pinout_->highlight(selectedConnections_); }
        });
        pinout_->contactSelected = [this](const QString &contact) {
            for(int row=0;row<bindings_.size();++row) if(bindings_[row].visible() && displayContacts(bindings_[row]).contains(contact)) {
                table_->selectRow(row); table_->scrollToItem(table_->item(row,0));
                if (!configurationAllowed || configurationAllowed()) bindings_[row].configure();
                return;
            }
        };
        auto *timer = new QTimer(this);
        timer->setInterval(200);
        connect(timer, &QTimer::timeout, this, [this] { if (isVisible()) refresh(); else for (auto &binding : bindings_)
                if (binding.choice->parentWidget()->isVisible()) refreshEditor(binding); });
        timer->start();
    }

    void add(QWidget *owner, Binding binding) {
        const int row = bindings_.size();
        table_->insertRow(row);
        table_->setItem(row, 0, new QTableWidgetItem(binding.name));
        auto *form = owner->layout()->findChild<QFormLayout*>("ecuConnections");
        if (!form) form=owner->layout()->findChild<QFormLayout*>("outputSettings");
        if (!form) {
            form = new QFormLayout;
            form->setObjectName("ecuConnections");
            form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
            static_cast<QVBoxLayout*>(owner->layout())->insertLayout(owner->findChild<QToolBar*>() ? 1 : 0, form);
        }
        binding.roleLabel = new QLabel(binding.name, owner);
        binding.choice = new QComboBox(owner);
        binding.choice->setObjectName("connection:" + binding.name);
        binding.choice->addItems(connectionLabels(binding));
        binding.choice->setToolTip(binding.connectionHelp);
        if (form->objectName()=="outputSettings") form->insertRow(0,binding.roleLabel,binding.choice);
        else form->addRow(binding.roleLabel,binding.choice);
        table_->setItem(row, 1, new QTableWidgetItem);
        auto *configure = new QPushButton(tr("Configure…"), table_);
        connect(configure, &QPushButton::clicked, this, [this,row] {
            if (!configurationAllowed || configurationAllowed()) bindings_[row].configure();
        });
        table_->setCellWidget(row, 2, configure);
        connect(binding.choice, &QComboBox::activated, this, [this, row](int selected) {
            assignConnection(row, selected);
        });
        bindings_.append(std::move(binding));
        refresh();
    }

    void refresh() {
        QMap<QString,QStringList> assignments;
        QMap<QString,QString> dedicated;
        for (int i = 0; i < bindings_.size(); ++i) {
            const auto &binding = bindings_[i];
            table_->setRowHidden(i, !binding.visible());
            if (binding.label) table_->item(i,0)->setText(binding.label());
            refreshEditor(binding);
            table_->item(i,1)->setText(binding.choice->currentText());
            const bool pending = binding.sourceWrite->property("ecuWritePending").toBool();
            if(binding.visible()) for(const auto &contact : contacts(binding)) {
                const QString name = binding.label ? binding.label() : binding.name;
                assignments[contact] << (pending ? tr("%1 (pending)").arg(name) : name);
            }
            if(binding.visible() && dedicatedPin(binding) && binding.connections.size()==2 && binding.current()==0)
                for(const auto &contact:displayContacts(binding)) dedicated[contact]=tr("%1 — disabled").arg(binding.label?binding.label():binding.name)+(pending?tr(" (pending)"):QString{});
        }
        pinout_->setDedicatedFunctions(dedicated);
        pinout_->setAssignments(assignments);
        const int selected = table_->currentRow();
        if(selected>=0 && selected<bindings_.size()) {
            const auto updated=displayContacts(bindings_[selected]);
            if(updated!=selectedConnections_) {selectedConnections_=updated; pinout_->highlight(updated);}
        }
    }

private:
    static QStringList connectionLabels(const Binding &binding) {
        auto labels=binding.connections;
        if(!labels.isEmpty() && labels.first()==tr("Not assigned")) labels[0]=tr("No pin selected");
        return labels;
    }
    static QStringList contacts(const Binding &binding) {
        const int current=binding.current();
        if(current<0 || current>=binding.connections.size() || (current==0 && binding.connections.size()>1)) return {};
        return parseContacts(binding.connections[current]);
    }
    static QStringList parseContacts(const QString &text) {
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
    static bool dedicatedPin(const Binding &binding) {
        return (binding.connections.size()==1 || binding.connections.size()==2) && !parseContacts(binding.connections.last()).isEmpty();
    }
    static QStringList displayContacts(const Binding &binding) {
        const auto selected=contacts(binding);
        return selected.isEmpty() && dedicatedPin(binding) ? parseContacts(binding.connections.last()) : selected;
    }
    void refreshEditor(const Binding &binding) {
        QSignalBlocker blocker(binding.choice);
        if (!binding.choice->view()->isVisible()) binding.choice->setCurrentIndex(binding.current());
        binding.choice->setEnabled((!configurationAllowed || configurationAllowed()) && binding.connections.size()>1);
        binding.choice->setVisible(binding.visible());
        binding.roleLabel->setVisible(binding.visible());
        if (binding.label) binding.roleLabel->setText(binding.label());
    }
    void showEvent(QShowEvent *event) override {
        QWidget::showEvent(event);
        refresh();
    }
    bool assignConnection(int row,int selected) {
        if(configurationAllowed&&!configurationAllowed())return false;
        auto &binding=bindings_[row];
        if (selected<0 || selected>=binding.connections.size() || !binding.visible() ||
            !binding.sourceWrite->parentWidget()->isEnabled()) return false;
        table_->setCurrentCell(row,0);
        for(int i=0;i<bindings_.size();++i) {
            const auto &other=bindings_[i];
            if(i!=row && selected && other.resource==binding.resource && other.visible() && other.current()==selected) {
                QMessageBox::warning(binding.choice,tr("Connection occupied"),tr("%1 already uses this connection.").arg(other.label?other.label():other.name));
                refresh(); return false;
            }
        }
        QString error;
        if(selected!=binding.current() && !binding.assign(selected,error)) {
            QMessageBox::warning(binding.choice,tr("Connection assignment"),error); refresh(); return false;
        }
        refresh(); return true;
    }
    QTableWidget *table_;
    ProteusPinoutWidget *pinout_;
    QVector<Binding> bindings_;
    QStringList selectedConnections_;
};
}
