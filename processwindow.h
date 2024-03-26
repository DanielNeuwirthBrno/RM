/*******************************************************************************
 Copyright 2021-23 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#ifndef PROCESSWINDOW_H
#define PROCESSWINDOW_H

#include <QDialog>
#include <QVector>
#include <QWidget>
#include "match/match.h"
#include "player/player.h"
#include "settings/config.h"
#include "shared/datetime.h"
#include "team.h"
#include "ui/windows/ui_processwindow.h"

class ProcessWindow: public QDialog {

    Q_OBJECT

    public:
        explicit ProcessWindow(DateTime &, Match * const, const QVector<Team *> &, Team * const,
                               const std::array<const MessageDisplayRule, 2> &, const bool, QWidget * = nullptr);
        explicit ProcessWindow(DateTime &, Match * const, const QVector<Team *> &);
        ~ProcessWindow();

        void timeShiftExternal() { this->timeShift(); return; }

    private:
        void healthValueUpdate(Player * const);
        void fatigueValueUpdate(Player * const);

        void healthConditionUpdate(Player * const, const bool);
        void unavailabilityUpdate_health(Player * const, const bool);

        void suspensionUpdate(Player * const, const bool);
        void unavailabilityUpdate_suspension(Player * const, const bool);

        void timeShift(const QDate &);

        Ui_ProcessWindow * ui;

        DateTime & _currentDateTime;
        MessageDisplayRule _showHealthReportMessages;
        MessageDisplayRule _showDisciplinaryMessages;

        Team * const _myTeam;
        Match * const _nextMatch;
        QVector<Team *> _teams;

    private slots:
        void timeShift();

    signals:
        void timeChanged();
};

#endif // PROCESSWINDOW_H
