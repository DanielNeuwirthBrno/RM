/*******************************************************************************
 Copyright 2021-22 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#ifndef SQUADSWINDOW_H
#define SQUADSWINDOW_H

#include <QCheckBox>
#include <QPair>
#include <QSlider>
#include <QWidget>
#include "match/match.h"
#include "settings/config.h"
#include "ui/windows/ui_squadswindow.h"

class SquadsWindow: public QDialog {

    Q_OBJECT

    public:
        explicit SquadsWindow(Match * const match, const QPair<MatchType::Location, QString> & manager,
                              SubstitutionRules & rules, QWidget * parent = nullptr): QDialog(parent), ui(new Ui_SquadsWindow) {

            bool & substitutions = rules.automaticSubstitutions();
            uint8_t & interval = rules.replacementInterval();
            bool & transfer = rules.transferPreferences();

            ui->setupUi(match, manager, substitutions, transfer, interval, this);

            connect(ui->replacementIntervalSlider, &QSlider::valueChanged, this,
                    [& interval](const int value) -> void { interval = value; return; } );
            connect(ui->replacementIntervalSlider, &QSlider::valueChanged, this, [this](const int value)
                    -> void { ui->replacementIntervalLineEdit->setText(QString::number(value)); return; } );
            connect(ui->automaticSubstitutionsCheckBox, &QCheckBox::toggled, this,
                    [& substitutions](const bool value) -> void { substitutions = value; return; } );
            connect(ui->transferPreferencesCheckBox, &QCheckBox::toggled, this,
                    [& transfer](const bool value) -> void { transfer = value; return; } );
            connect(ui->closeButton, &QPushButton::clicked, this, [this]() -> void { this->close(); } );
        }
        ~SquadsWindow() { delete ui; }

    private:
        Ui_SquadsWindow * ui;
};

#endif // SQUADSWINDOW_H
