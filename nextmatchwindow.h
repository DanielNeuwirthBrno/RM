/*******************************************************************************
 Copyright 2020-22 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#ifndef NEXTMATCHWINDOW_H
#define NEXTMATCHWINDOW_H

#include <QPair>
#include <QWidget>
#include "competition.h"
#include "match/match.h"
#include "settings/config.h"
#include "squadswindow.h"
#include "team.h"
#include "ui/windows/ui_nextmatchwindow.h"

class NextMatchWindow: public QDialog {

    Q_OBJECT

    public:
        explicit NextMatchWindow(Match * match, const Competition & competition, const Config & config,
                                 SubstitutionRules & rules, QWidget * parent = nullptr):
            QDialog(parent), ui(new Ui_NextMatchWindow), _match(match), _substitutionRules(rules) {

            const MatchType::Location _myTeamLocation = (_match->isTeamInPlay(MatchType::Location::HOSTS, config.team()))
                                                      ? MatchType::Location::HOSTS : MatchType::Location::VISITORS;
            _manager = qMakePair<MatchType::Location, QString>(_myTeamLocation, config.manager());

            ui->setupUi(match, competition, config.team(), this);

            connect(ui->squadsButton, &QPushButton::clicked, this, &NextMatchWindow::squads);
            connect(ui->proceedButton, &QPushButton::clicked, this, [this]() -> void { this->accept(); } );
            connect(ui->cancelButton, &QPushButton::clicked, this, [this]() -> void { this->reject(); } );
        }
        ~NextMatchWindow() { delete ui; }

    private:
        Ui_NextMatchWindow * ui;

        Match * _match;
        QPair<MatchType::Location, QString> _manager;

        SubstitutionRules & _substitutionRules;

    private slots:
        void squads() {

            QWidget * parent = dynamic_cast<QWidget *>(ui->windowLayout->parent());
            SquadsWindow * squadsWindow = new SquadsWindow(_match, _manager, _substitutionRules, parent);
            squadsWindow->show();
        }
};

#endif // NEXTMATCHWINDOW_H
