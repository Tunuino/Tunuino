#pragma once
#include "../ui/WriteButtonState.h"
#include "ProteusPinoutWidget.h"
#include <QAbstractItemView>
#include <QDialog>
#include <QDialogButtonBox>
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
        QPushButton *write = nullptr;
        QString connectionHelp;
    };

    explicit IoConfigurationWidget(QWidget *parent = nullptr) : QWidget(parent) {
        setObjectName("ioConfiguration");
        auto *layout = new QVBoxLayout(this);
        auto *hint = new QLabel(tr("Choose an ECU connection in the list below. Configure edits the ECU pin connection here; Write to ECU saves the changes."), this);
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
            if(row>=0 && row<bindings_.size()) { selectedConnections_=displayContacts(bindings_[row]); pinout_->highlight(selectedConnections_); }
        });
        pinout_->contactSelected = [this](const QString &contact) {
            for(int row=0;row<bindings_.size();++row) if(bindings_[row].visible() && displayContacts(bindings_[row]).contains(contact)) {
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
        binding.choice->addItems(connectionLabels(binding));
        binding.choice->setToolTip(binding.connectionHelp.isEmpty()?tr("Select the physical ECU pin for this function. Changes remain local until Write to ECU."):binding.connectionHelp);
        binding.choice->setStyleSheet("QComboBox { border: 1px solid #696f76; padding: 3px 20px 3px 5px; }");
        table_->setCellWidget(row, 1, binding.choice);
        auto *configure = new QPushButton(tr("Configure..."), table_);
        configure->setToolTip(tr("Configure this function's physical ECU connection."));
        connect(configure, &QPushButton::clicked, this, [this,row] { configurePin(row); });
        table_->setCellWidget(row, 2, configure);
        binding.write = new QPushButton(tr("Write to ECU"), table_);
        table_->setCellWidget(row, 3, binding.write);
        connect(binding.write, &QPushButton::clicked, binding.sourceWrite, &QPushButton::click);
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
            QSignalBlocker blocker(binding.choice);
            if(!binding.choice->view()->isVisible()) binding.choice->setCurrentIndex(binding.current());
            // Offline editing is independent of whether an ECU write is available.
            binding.choice->setEnabled((!configurationAllowed||configurationAllowed())&&binding.connections.size()>1);
            binding.write->setEnabled(binding.sourceWrite->isEnabled());
            const bool pending = binding.sourceWrite->property("ecuWritePending").toBool();
            showPendingWrite(binding.write, pending);
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
    QString describeConnection(const Binding &binding,int selected) const {
        QStringList pins;
        if(selected>=0 && selected<binding.connections.size()) pins=parseContacts(binding.connections[selected]);
        const bool fixed=dedicatedPin(binding);
        if(pins.isEmpty() && fixed) pins=parseContacts(binding.connections.last());
        QStringList details;
        if(!binding.connectionHelp.isEmpty()) details<<binding.connectionHelp;
        if(fixed) details << (binding.connections.size()>1?tr("Dedicated hardware connection. Enable or disable this function; its physical pins do not move."):tr("Fixed hardware connection. These physical pins cannot be reassigned."));
        if(pins.isEmpty()) details << tr("No ECU pin selected. Choose a compatible connection above.");
        for(const auto &contact:pins) for(int connector=0;connector<3;++connector) for(const auto &pin:proteusPins(connector))
            if(contact==proteusContact(connector,pin.number)) details<<QString("%1 — %2: %3").arg(contact,pin.signal,pin.description);
        if(selected==0 && binding.connections.size()>1) details<<tr("Function disabled. Choosing a connection prepares a local change; it does not write to or arm the ECU.");
        return details.join("\n");
    }
    bool assignConnection(int row,int selected) {
        if(configurationAllowed&&!configurationAllowed())return false;
        auto &binding=bindings_[row]; table_->setCurrentCell(row,0);
        for(int i=0;i<bindings_.size();++i) {
            const auto &other=bindings_[i];
            if(i!=row && selected && other.resource==binding.resource && other.visible() && other.current()==selected) {
                QMessageBox::warning(this,tr("Connection occupied"),tr("%1 already uses this connection.").arg(other.label?other.label():other.name));
                refresh(); return false;
            }
        }
        QString error;
        if(selected!=binding.current() && !binding.assign(selected,error)) {
            QMessageBox::warning(this,tr("Connection assignment"),error); refresh(); return false;
        }
        refresh(); return true;
    }
    void configurePin(int row) {
        table_->setCurrentCell(row,0); const auto &binding=bindings_[row];
        pinout_->highlight(displayContacts(binding));
        QDialog dialog(this); dialog.setObjectName("ioPinConfiguration"); dialog.setWindowTitle(tr("ECU pin — %1").arg(binding.label?binding.label():binding.name));
        dialog.resize(620,320); auto *layout=new QVBoxLayout(&dialog);
        auto *heading=new QLabel(binding.label?binding.label():binding.name,&dialog); heading->setStyleSheet("font-weight:bold"); layout->addWidget(heading);
        auto *form=new QFormLayout; auto *choice=new QComboBox(&dialog);choice->setObjectName("ioPinChoice");
        choice->addItems(connectionLabels(binding));choice->setCurrentIndex(binding.current());choice->setEnabled(binding.connections.size()>1);
        form->addRow(tr("ECU connection"),choice);layout->addLayout(form);
        auto *details=new QLabel(&dialog);details->setObjectName("ioPinConnectionDetail");details->setWordWrap(true);details->setTextFormat(Qt::PlainText);layout->addWidget(details);
        auto *state=new QLabel(tr("Apply connection keeps the change in the editor. Write to ECU saves it. Function parameters remain in their own navigation section."),&dialog);
        state->setWordWrap(true);layout->addWidget(state);layout->addStretch();
        auto *buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Close,&dialog);
        auto *apply=buttons->button(QDialogButtonBox::Apply);apply->setText(tr("Apply connection"));apply->setObjectName("ioPinApply");
        auto *write=new QPushButton(tr("Write to ECU"),&dialog);write->setObjectName("ioPinWrite");buttons->addButton(write,QDialogButtonBox::ActionRole);layout->addWidget(buttons);
        bool edited=false;
        const auto update=[&] {
            const auto &source=bindings_[row];
            if(!edited && choice->currentIndex()!=source.current()) {QSignalBlocker blocker(choice);choice->setCurrentIndex(source.current());}
            details->setText(describeConnection(source,choice->currentIndex()));
            apply->setEnabled(choice->currentIndex()!=source.current());write->setEnabled(source.connections.size()>1 && source.sourceWrite->isEnabled());
            showPendingWrite(write,choice->currentIndex()!=source.current() || source.sourceWrite->property("ecuWritePending").toBool());
        };
        connect(choice,&QComboBox::currentIndexChanged,&dialog,[&]{edited=true;update();});
        connect(apply,&QPushButton::clicked,&dialog,[&]{if(assignConnection(row,choice->currentIndex())) {edited=false;state->setText(tr("Connection prepared locally. Use Write to ECU to save it."));}update();});
        connect(write,&QPushButton::clicked,&dialog,[&]{if(assignConnection(row,choice->currentIndex())) {edited=false;if(bindings_[row].sourceWrite->isEnabled()) bindings_[row].sourceWrite->click();}update();});
        connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        QTimer timer; timer.setInterval(200);connect(&timer,&QTimer::timeout,&dialog,[&]{update();});timer.start();update();dialog.exec();
    }
    QTableWidget *table_;
    ProteusPinoutWidget *pinout_;
    QVector<Binding> bindings_;
    QStringList selectedConnections_;
};
}
