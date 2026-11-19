#pragma once
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QSignalBlocker>
#include <functional>
namespace tunuino {
// Project target choice. An unselected project has no ECU editing or transport access.
class ProjectEcuSelection final : public QWidget {
public:
    std::function<bool()> mayChange;
    std::function<void()> changed;
    explicit ProjectEcuSelection(QWidget *parent=nullptr):QWidget(parent) {
        setObjectName("projectEcuSelection");auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,4);
        auto *title=new QLabel(tr("1. Select ECU"),this);title->setStyleSheet("font-weight:bold");layout->addWidget(title);
        choice_=new QComboBox(this);choice_->setObjectName("projectEcuChoice");
        choice_->addItem(tr("Select ECU…"),"");choice_->addItem("Proteus","Proteus");choice_->addItem("UAefi","UAefi");layout->addWidget(choice_);
        auto *apply=new QPushButton(tr("Use selected ECU"),this);apply->setObjectName("projectEcuApply");layout->addWidget(apply);
        state_=new QLabel(tr("Choose an ECU before configuring the project."),this);state_->setObjectName("projectEcuState");state_->setWordWrap(true);layout->addWidget(state_);
        connect(apply,&QPushButton::clicked,this,[this]{QString error;if(!select(choice_->currentData().toString(),error))state_->setText(error);});
    }
    QString selected() const {return board_;}
    bool proteus() const {return board_=="Proteus";}
    bool select(const QString &board,QString &error) {
        error.clear();
        if(board!="Proteus"&&board!="UAefi"){error=tr("Select Proteus or UAefi first.");return false;}
        if(board==board_)return true;
        if(mayChange&&!mayChange()){choice_->setCurrentIndex(choice_->findData(board_));error=tr("Finish the active ECU operation before changing ECU. The current selection is unchanged.");return false;}
        board_=board;choice_->setCurrentIndex(choice_->findData(board));setProperty("selectedEcu",board);
        state_->setText(proteus()?tr("Proteus selected. UAefi locked."):tr("UAefi selected. Proteus locked. Pinout only; native firmware is not available yet."));
        if(changed)changed();return true;
    }
private:
    QString board_;QComboBox *choice_;QLabel *state_;
};
}
