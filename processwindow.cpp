/*******************************************************************************
 Copyright 2021-23 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include <QMessageBox>
#include <QPair>
#include <QPushButton>
#include <QStringBuilder>
#include <QtGlobal>
#include <algorithm>
#include "processwindow.h"
#include "shared/handle.h"
#include "shared/messages.h"

// with ui (when we want to display information about background processes)
ProcessWindow::ProcessWindow(DateTime & dateTime, Match * const match, const QVector<Team *> & teams,  Team * const myTeam,
                             const std::array<const MessageDisplayRule, 2> & messages, const bool runByUser, QWidget * parent):
    QDialog(parent), ui(new Ui_ProcessWindow), _currentDateTime(dateTime), _showHealthReportMessages(messages.at(0)),
    _showDisciplinaryMessages(messages.at(1)), _myTeam(myTeam), _nextMatch(match), _teams(teams) {

    const bool progressEnabled = (this->_currentDateTime.systemDate() < this->_nextMatch->date());
    ui->setupUi(this, _myTeam, _teams, _currentDateTime.systemDate(), progressEnabled, runByUser);

    connect(ui->processStepByStepButton, &QPushButton::clicked, this, QOverload<>::of(&ProcessWindow::timeShift));
    connect(ui->processAllButton, &QPushButton::clicked, this, QOverload<>::of(&ProcessWindow::timeShift));
    connect(ui->proceedButton, &QPushButton::clicked, this, [this]() -> void { this->accept(); } );
    connect(ui->cancelButton, &QPushButton::clicked, this, [this]() -> void { this->reject(); } );

    connect(this, SIGNAL(timeChanged()), Handle::getMainWindowHandle(), SLOT(updateDateAndTimeLabel()));
}

// without ui <=> called from FixturesWidget in PlayAllMatches mode
ProcessWindow::ProcessWindow(DateTime & dateTime, Match * const match, const QVector<Team *> & teams):
    ui(nullptr), _currentDateTime(dateTime), _myTeam(nullptr), _nextMatch(match), _teams(teams) {

    connect(this, SIGNAL(timeChanged()), Handle::getMainWindowHandle(), SLOT(updateDateAndTimeLabel()));
}

ProcessWindow::~ProcessWindow() {

    if (ui != nullptr)
        delete ui;
}

// change of condition: health (increase, decrease)
void ProcessWindow::healthValueUpdate(Player * const player) {

    const QPair<const bool, const bool> degreeOfChange = qMakePair<const bool, const bool>
        (RandomValue::generateRandomBool(healthIssuesProbabilities.changeInHealth(player::HEALTH_BIG_CHANGE)),
         RandomValue::generateRandomBool(healthIssuesProbabilities.changeInHealth(player::HEALTH_BETTER)));

    if ((degreeOfChange.second && player->condition(player::Conditions::HEALTH) < PlayerCondition::maxValue) ||
        (!degreeOfChange.second && player->condition(player::Conditions::HEALTH) > PlayerCondition::minValue)) {

        PlayerCondition * const pc = player->condition();
        pc->changeCondition = (degreeOfChange.second)
            ? &PlayerCondition::increaseCondition : &PlayerCondition::decreaseCondition;

        const uint8_t number = 1 + static_cast<uint8_t>(degreeOfChange.first);
        (pc->*(pc->changeCondition))(player::Conditions::HEALTH, number);
    }

    return;
}

// new health issue (injury, sickness, ...) occurring
void ProcessWindow::healthConditionUpdate(Player * const player, const bool myTeam) {

    const QDate currentDate = this->_currentDateTime.systemDate();

    const uint8_t probabilityOfInjury = player->condition(player::Conditions::HEALTH) * 0.45 +
                                        player->condition(player::Conditions::FATIGUE) * 0.15 +
                                        player->condition(player::Conditions::FITNESS) * 0.15 +
                                        player->attribute(player::Attributes::DEXTERITY) * 0.25;

    const bool playerIsOut = RandomValue::generateRandomBool(probabilityOfInjury) &&
                             RandomValue::generateRandomBool(healthIssuesProbabilities.probabilityOfInjury);

    if (playerIsOut) {

        player->condition()->newHealthIssue(currentDate);

        if ((this->_showHealthReportMessages == MessageDisplayRule::ALWAYS) ||
            (this->_showHealthReportMessages == MessageDisplayRule::MY_TEAM_ONLY && myTeam)) {

            const QString reasonOfAbsence = player->availability(player::Conditions::AVAILABILITY, currentDate);
            const QString absentUntil = player->availability(player::Conditions::RETURN_DATE, currentDate);
            const QString name = (this->_showHealthReportMessages == MessageDisplayRule::ALWAYS)
                               ? player->fullName() + string_functions.wrapInBrackets(_myTeam->teamName(player)).chopped(1)
                               : player->fullName();

            const QString dialogText = message.displayWithReplace(this->objectName(), "unavailableBecauseOfHealth",
                                       { name, reasonOfAbsence, absentUntil });

            QMessageBox::warning(this, QStringLiteral("Health report"), dialogText);
        }
    }

    return;
}

void ProcessWindow::unavailabilityUpdate_health(Player * const player, const bool myTeam) {

    const QDate currentDate = this->_currentDateTime.systemDate();

    bool recoveryDateUnknown = false;
    const QDate dateOfRecovery = player->condition()->dateOfRecovery(recoveryDateUnknown);

    // if end date is unknown (= it isn't known yet when given player will be available again)
    if (dateOfRecovery.isNull() && recoveryDateUnknown &&
        player->condition()->liveHealthStatus()->statusValidFrom() != currentDate) {

        if (RandomValue::generateRandomBool(5)) {

            const QDate endDate = player->condition()->addEndDateToHealthIssue(currentDate);

            if ((this->_showHealthReportMessages == MessageDisplayRule::ALWAYS) ||
                (this->_showHealthReportMessages == MessageDisplayRule::MY_TEAM_ONLY && myTeam)) {

                const QString reasonOfAbsence = player->availability(player::Conditions::AVAILABILITY, currentDate);
                const QString name = (this->_showHealthReportMessages == MessageDisplayRule::ALWAYS)
                                   ? player->fullName() + string_functions.wrapInBrackets(_myTeam->teamName(player)).chopped(1)
                                   : player->fullName();

                const QString dialogText = message.displayWithReplace(this->objectName(), "unavailableDateToKnown",
                                           { name, reasonOfAbsence, endDate.toString(Qt::DefaultLocaleShortDate) });
                QMessageBox::information(this, QStringLiteral("Health report (update)"), dialogText);
            }
        }
    }

    // if date of recovery of (until now) "live" record is in the past or date of recovery is empty/
    // invalid, but not due to (so far) unknown date of recovery but because of no associated health
    // record being found (this shouldn't happen though since we search among players with "live"
    // records only so date of recovery is either known and can be compared to current date or is
    // not known yet = will be specified later) then label such record as invalid (= not live anymore)
    if ((!dateOfRecovery.isNull() && dateOfRecovery <= currentDate) || (dateOfRecovery.isNull() && !recoveryDateUnknown)) {

        const uint8_t numberOfDaysOfAbsence =
            player->condition()->liveHealthStatus()->statusValidFrom().daysTo(currentDate);

        player->condition()->invalidateHealthIssue();

        if ((this->_showHealthReportMessages == MessageDisplayRule::ALWAYS) ||
            (this->_showHealthReportMessages == MessageDisplayRule::MY_TEAM_ONLY && myTeam)) {

            const QString name = (this->_showHealthReportMessages == MessageDisplayRule::ALWAYS)
                               ? player->fullName() + string_functions.wrapInBrackets(_myTeam->teamName(player)).chopped(1)
                               : player->fullName();

            const QString dialogText = message.displayWithReplace(this->objectName(), "playerBackInTraining",
                                       { name, QString::number(numberOfDaysOfAbsence) });
            QMessageBox::information(this, QStringLiteral("Health report (update)"), dialogText);
        }
    }

    return;
}

// change of condition: fatigue (increase)
void ProcessWindow::fatigueValueUpdate(Player * const player) {

    if (player->condition(player::Conditions::FATIGUE) < PlayerCondition::maxValue) {

        const uint8_t probabilityOfIncrease = 48 + player->condition(player::Conditions::FITNESS) * 2;
        if (RandomValue::generateRandomBool(probabilityOfIncrease))
            player->condition()->increaseCondition(player::Conditions::FATIGUE);
    }

    return;
}

// player can be suspended for additional (up to) 6 weeks after being sent-off (red-carded) in last match
void ProcessWindow::suspensionUpdate(Player * const player, const bool myTeam) {

    const bool suspended = RandomValue::generateRandomBool(suspensionProbabilities.probabilityOfSuspension);

    if (suspended) {

        const uint8_t numberOfWeeks = RandomValue::generateRandomInt<uint8_t>(1, suspensionProbabilities.maxNumberOfWeeks);
        const QDate suspendedUntil = this->_currentDateTime.systemDate().addDays(numberOfWeeks * 7);

        player->setSuspensionEndDate(suspendedUntil);

        if ((this->_showDisciplinaryMessages == MessageDisplayRule::ALWAYS) ||
            (this->_showDisciplinaryMessages == MessageDisplayRule::MY_TEAM_ONLY && myTeam)) {

            const QString name = (this->_showHealthReportMessages == MessageDisplayRule::ALWAYS)
                               ? player->fullName() + string_functions.wrapInBrackets(_myTeam->teamName(player)).chopped(1)
                               : player->fullName();

            const QString dialogText = message.displayWithReplace(this->objectName(), "unavailableBecauseOfSuspension",
                { name, QString::number(numberOfWeeks), suspendedUntil.toString(Qt::DefaultLocaleShortDate) });
            QMessageBox::warning(this, QStringLiteral("Disciplinary hearing"), dialogText);
        }
    }
    player->sentOff(false);

    return;
}

void ProcessWindow::unavailabilityUpdate_suspension(Player * const player, const bool myTeam) {

    if (this->_currentDateTime.systemDate() > player->suspendedUntil()) {

        player->suspensionEnds();

        if ((this->_showDisciplinaryMessages == MessageDisplayRule::ALWAYS) ||
            (this->_showDisciplinaryMessages == MessageDisplayRule::MY_TEAM_ONLY && myTeam)) {

            const QString name = (this->_showHealthReportMessages == MessageDisplayRule::ALWAYS)
                               ? player->fullName() + string_functions.wrapInBrackets(_myTeam->teamName(player)).chopped(1)
                               : player->fullName();

            const QString dialogText = message.displayWithReplace(this->objectName(), "playerServedHisTerm", { name });
            QMessageBox::information(this, QStringLiteral("Disciplinary hearing (update)"), dialogText);
        }
    }

    return;
}

// update players' feature values which can change over time
void ProcessWindow::timeShift(const QDate & endDate) {

    const bool UiMode = (ui != nullptr);

    QDate currentDate = this->_currentDateTime.systemDate();
    QTime currentTime = this->_currentDateTime.systemTime();

    while (currentDate < endDate) {

        int row = 0;
        if (UiMode)
            ui->resetProgressBars();

        for (auto team: this->_teams) {

            int chunk = 1;
            for (auto player: team->squad()) {

                // only healthy players
                if (player->isHealthy()) {

                    if (RandomValue::generateRandomBool(healthIssuesProbabilities.changeInHealth(player::HEALTH_CHANGE)))
                        this->healthValueUpdate(player);

                    this->healthConditionUpdate(player, team == _myTeam);
                }

                // only non-healthy players
                if (!player->isHealthy())
                    this->unavailabilityUpdate_health(player, team == _myTeam);

                // all players
                this->fatigueValueUpdate(player);

                if (player->lastMatchSentOff())
                    this->suspensionUpdate(player, team == _myTeam);

                if (player->isSuspended())
                    this->unavailabilityUpdate_suspension(player, team == _myTeam);

                if (UiMode)
                    ui->teamProgressElements.at(row).second->setValue((chunk++)*6);
            }
            ++row;
        }

        currentDate = currentDate.addDays(1);
        if (UiMode)
            ui->currentDateLabel->setText(currentDate.toString(Qt::DefaultLocaleShortDate));

        currentTime = (currentDate < this->_nextMatch->date() || currentTime <= this->_nextMatch->time().addSecs(-60*60))
                    ? currentTime : this->_nextMatch->time().addSecs(-60*60);

        this->_currentDateTime.refreshSystemDateAndTime(currentDate, currentTime);
        emit timeChanged();
    }

    if (UiMode && endDate == this->_nextMatch->date()) {

        ui->processStepByStepButton->setEnabled(false);
        ui->processAllButton->setEnabled(false);
        ui->proceedButton->setEnabled(true);
    }

    return;
}

// [slot]
void ProcessWindow::timeShift() {

    QPushButton * const senderButton = qobject_cast<QPushButton *>(sender());

    const bool processAll = (senderButton == nullptr || senderButton->objectName() == on::processwindow.processAllButton);
    const QDate endDate = (processAll) ? this->_nextMatch->date() : this->_currentDateTime.systemDate().addDays(1);

    this->timeShift(endDate);

    return;
}
