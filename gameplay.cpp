/*******************************************************************************
 Copyright 2021-23 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include <QInputDialog>
#include <QMessageBox>
#include <QStringBuilder>
#include <QThread>
#include <array>
#include "fixtureswidget.h"
#include "matchwidget.h"
#include "match/gameplay.h"
#include "match/match.h"
#include "player/position_types.h"
#include "settings/matchsettings.h"
#include "shared/constants.h"
#include "shared/handle.h"
#include "shared/html.h"
#include "shared/messages.h"
#include "shared/random.h"
#include "shared/texts.h"
#include "ui/custom/ui_inputdialog.h"

const QString GamePlay::_penaltyInfringement = QStringLiteral("/penaltyInfringement");
const QString GamePlay::_penaltySelectedType = QStringLiteral("/penaltySelectedType");
const QString GamePlay::_scrumInfringement = QStringLiteral("/scrumInfringement");
const QString GamePlay::_dangerousTackle = QStringLiteral("/dangerousTackle");
const QString GamePlay::_playerSubstitution = QStringLiteral("/playerSubstitution");

const QMap<GamePlay::PenaltyAction, QString> GamePlay::actionAfterPenaltyInfringement {

    { GamePlay::PenaltyAction::KICK_AT_GOAL, QStringLiteral("kick at goal") },
    { GamePlay::PenaltyAction::KICK_TO_TOUCH, QStringLiteral("kick into touch") },
    { GamePlay::PenaltyAction::SCRUM, QStringLiteral("prefer scrum") },
    { GamePlay::PenaltyAction::TAP_PENALTY, QStringLiteral("tap penalty") }
};

template <typename T>
GamePlay::GamePlay(T * widget, Settings * const settings , DateTime & dateTime, Match * const match, Team * const myTeam):
    _periods(new MatchPeriods()), _settings(settings), _dateTime(dateTime), _automaticSelection(false),
    _hostsFirstKickOff(false), _restartPlay(false), _incrementCarries(true), _distanceFromHalfwayLine(0),
    _noOfPhases(0), _match(match), _myTeam(myTeam), _teamInPossession(nullptr), _playerInPossession(nullptr) {

    this->setObjectName("GamePlayObject");

    // if called in non-interactive mode, both are assigned nullptr
    _mw = dynamic_cast<MatchWidget *>(widget);
    _fw = dynamic_cast<FixturesWidget *>(widget);

    if (this->displayOn(FW))
        _periods->changeDoNotStopAtPeriodTo(_fw->playUntilAtLeastPeriod());

    connect(this, SIGNAL(timeChanged()), Handle::getMainWindowHandle(), SLOT(updateDateAndTimeLabel()));

    html_functions.dummyCallToSuppressCompilerWarning();
}

void GamePlay::startOfMatch() {

    // set players No. 1-15 of both teams as being on pitch
    for (uint8_t i = 0; i < 2; ++i) {

        const MatchType::Location loc = static_cast<MatchType::Location>(i);
        for (auto player: this->_match->team(loc)->squad()) {

            player->includePlayerIntoStartingXV();
            if (player->isOnPitch()) {

                this->_match->addNewStatsRecordForPlayer(loc, player);
                this->updateStatistics(loc, StatsType::NumberOf::GAMES_PLAYED, player);
            }
        }
    }

    if (this->displayOn(MW))
        _mw->updatePackWeight();
    }

void GamePlay::endOfMatch() {

    for (uint8_t i = 0; i < 2; ++i) {

        const MatchType::Location loc = static_cast<MatchType::Location>(i);
        this->_match->team(loc)->cleanPitch();

        if (this->_match->type() == MatchType::Type::REGULAR) {

            // update teams' results (win/draw/loss)
            const bool tryBonusPoint = this->_match->score(loc)->bonusPointTry();
            const bool diffBonusPoint = this->_match->diffBonusPoint(loc);
            const TeamResults::ResultType resultType = this->_match->resultTypeForTeam(loc);

            this->_match->team(loc)->results().updateResults(resultType, tryBonusPoint, diffBonusPoint);

            // update teams' points (tries, conversions, ...)
            const uint16_t pointsAgainst = (loc == MatchType::Location::HOSTS)
                ? this->_match->score(MatchType::Location::VISITORS)->points()
                : this->_match->score(MatchType::Location::HOSTS)->points();
            const uint8_t triesAgainst = (loc == MatchType::Location::HOSTS)
                ? this->_match->score(MatchType::Location::VISITORS)->points(PointEvent::TRY)
                : this->_match->score(MatchType::Location::HOSTS)->points(PointEvent::TRY);

            this->_match->team(loc)->scoredPoints().updateFromMatchScore(this->_match->score(loc), pointsAgainst, triesAgainst);
        }
    }

    this->_match->matchFinished();
    return;
}

bool GamePlay::refreshTime(const uint8_t seconds, const bool changeTimePeriod, const bool immediateRepaint) {

    const MatchType::Location teamInTerritory =
        (_distanceFromHalfwayLine < 0) ? MatchType::Location::VISITORS : MatchType::Location::HOSTS;
    const bool teamInPossesionInOwnHalf = (this->_match->team(teamInTerritory) != _teamInPossession);

    const double teamInPossessionRatio_orig = this->_match->calculatePossessionTimeRatio(this->whoIsInPossession().first, 0);
    const double teamInTerritoryRatio_orig = std::abs(100 * static_cast<uint8_t>(teamInPossesionInOwnHalf)
                                           - this->_match->calculateTerritoryTimeRatio(teamInTerritory, 0));

    this->_match->timePlayed().addTime(seconds);

    const double teamInPossessionRatio = this->_match->calculatePossessionTimeRatio(this->whoIsInPossession().first, seconds);
    const double teamInTerritoryRatio = std::abs(100 * static_cast<uint8_t>(teamInPossesionInOwnHalf)
                                      - this->_match->calculateTerritoryTimeRatio(teamInTerritory, seconds));

    if (this->displayOn(MW)) {

        _mw->ui->timePlayedLabel->setText(this->_match->timePlayed().timePlayed());
        _mw->ui->matchProgressProgressBar->setValue(this->_match->timePlayed().timePlayedInSecondsInPeriod());

        _mw->updateStatisticsUI(this->whoIsInPossession().first, "PossessionLabel",
                                string_functions.formatNumber<double>(teamInPossessionRatio));
        _mw->updateStatisticsUI(this->whoIsInPossession().second, "PossessionLabel",
                                string_functions.formatNumber<double>(100 - teamInPossessionRatio));

        _mw->updateStatisticsUI(this->whoIsInPossession().first, "TerritoryLabel",
                                string_functions.formatNumber<double>(teamInTerritoryRatio));
        _mw->updateStatisticsUI(this->whoIsInPossession().second, "TerritoryLabel",
                                string_functions.formatNumber<double>(100 - teamInTerritoryRatio));

        if (_mw->extendedLog()) {

            if (std::round(teamInPossessionRatio * 100) != std::round(teamInPossessionRatio_orig * 100))
                _mw->logRecord(_mw->dominationStatsForLog(teamInPossessionRatio, 0,
                               this->whoIsInPossession().first == MatchType::Location::VISITORS));
            if (std::round(teamInTerritoryRatio * 100) != std::round(teamInTerritoryRatio_orig * 100))
                _mw->logRecord(_mw->dominationStatsForLog(teamInTerritoryRatio, 1,
                               this->whoIsInPossession().first == MatchType::Location::VISITORS));
        }

        if (immediateRepaint) {

            _mw->ui->timePlayedLabel->repaint();
            _mw->ui->matchProgressProgressBar->repaint();
        }
    }

    if (this->displayOn(FW)) {

        _fw->ui->currentMatchProgress->setValue(this->_match->timePlayed().timePlayedInSecondsInPeriod());
        if (immediateRepaint)
            _fw->ui->currentMatchProgress->repaint();
    }

    // sudden-death time ends with score change
    const bool immediateEndOfMatch = this->_periods->matchEndsWithResultChange(this->_match->currentPeriod()) &&
        this->_match->resultTypeForTeam(MatchType::HOSTS) != this->_periods->resultType(this->_match->currentPeriod());

    if ((this->_match->timePlayedInSeconds() >= this->_periods->timePlayed(this->_match->currentPeriod(), 60) &&
        this->_periods->isPlayingTime(this->_match->currentPeriod())) || immediateEndOfMatch) {

        const bool endOfMatch = immediateEndOfMatch ||
                                (changeTimePeriod && this->_periods->matchEnds(this->_match->currentPeriod(),
                                 this->_match->type(), this->_match->resultTypeForTeam(MatchType::HOSTS)));
        if (endOfMatch) {

            // refresh system time
            this->_dateTime.refreshSystemDateAndTime(this->_match->timePlayed().timePlayedInPeriod(this->_match->currentPeriod()));
            emit timeChanged();

            this->_match->timePlayed().switchTimePeriodTo(MatchPeriod::TimePeriod::FULL_TIME);

            return true;
        }

        // if subsequent action is called from within ongoing action (e.g. a penalty execution after some infringement
        // happening during a scrum) and time of current time period is exceeded during this subsequent action, then
        // time period should not be changed at this moment (it will be changed after original action finishes)
        // => in such a case this function should be called with changeMatchPeriod::TimePeriod = false
        if (changeTimePeriod) {

            // refresh system time
            this->_dateTime.refreshSystemDateAndTime(this->_match->timePlayed().timePlayedInPeriod(this->_match->currentPeriod()));
            emit timeChanged();

            this->_match->timePlayed().switchTimePeriodTo(/*Next*/);
            const MatchPeriod::TimePeriod currentPeriod = this->_match->currentPeriod();

            if (this->displayOn(FW)) {

                QProgressBar * const progress = _fw->ui->currentMatchProgress;
                if (progress->maximum() != this->_periods->maximumValue(currentPeriod)) {

                    // reset progress bar
                    progress->setValue(0);
                    progress->setMaximum(this->_periods->maximumValue(currentPeriod));
                    progress->setStyleSheet(ss::shared.style(this->_periods->style(currentPeriod)));

                    if (immediateRepaint)
                        progress->repaint();
                }
            }

            if (this->displayOn(MW)) {

                QProgressBar * const progress = _mw->ui->matchProgressProgressBar;
                if (progress->maximum() != this->_periods->maximumValue(currentPeriod)) {

                    // reset progress bar
                    progress->setValue(0);
                    progress->setMaximum(this->_periods->maximumValue(currentPeriod));

                    if (immediateRepaint)
                        progress->repaint();
                }

                _mw->ui->updatePeriod(this->_periods->description(currentPeriod));
                _mw->logRecord(this->_periods->description(currentPeriod));
                _mw->timeStoppedMessageBox(this->_periods->messageBoxDefinition(currentPeriod));

                // in case of my team's match (MatchWidget) stop at half-time; otherwise continue to play
                return true;
            }
        }
    }
    return false;
}

void GamePlay::celebrationsTime() {

    const uint16_t celebrationsLength = this->_periods->length(this->_match->currentPeriod(), 60);
    this->_match->timePlayed().setTimeForInterval(celebrationsLength);

    this->_dateTime.refreshSystemDateAndTime(celebrationsLength);
    emit timeChanged();

    return;
}

void GamePlay::resetPhases() {

    _noOfPhases = 0;

    if (this->displayOn(MW)) {

        if (this->_mw->ui->hostsNoOfPhasesLabel->isVisible()) {

            this->_mw->ui->hostsNoOfPhasesLabel->setVisible(false);
            this->_mw->ui->hostsNoOfPhasesLabel->repaint();
        }
        else {

            this->_mw->ui->visitorsNoOfPhasesLabel->setVisible(false);
            this->_mw->ui->hostsNoOfPhasesLabel->repaint();
        }
    }

    return;
}

// opponent = false (default) => increase probability for team-in-possession
// e.g. RUN_OVER_GOAL_LINE_TRY_SCORED increases from 95 to 98 (higher probability of scoring)
// opponent = true => increase probability for team-not-in-possession
// e.g. RUN_TACKLE_COMPLETED increases from 90 to 95 (higher probability of successful tackle)
uint8_t GamePlay::probability(const MatchActionSubtype::MatchActivityType type, const bool opponent) const {

    const uint8_t baseProbability = this->_settings->matchActivities().probability(type);

    int8_t teamRankingDiff = static_cast<int8_t>(this->_match->team(this->whoIsInPossession().second)->ranking())
                           - static_cast<int8_t>(this->_match->team(this->whoIsInPossession().first)->ranking());

    const uint8_t teamRankingMaxDiff = std::min(std::abs(teamRankingDiff), 20);

    teamRankingDiff = teamRankingMaxDiff * (teamRankingDiff/std::abs(teamRankingDiff)) *
                      std::pow(-1, static_cast<uint8_t>(opponent));

    const uint8_t adjustedProbability = std::min(std::max(baseProbability + teamRankingDiff, 1), 99);

    return adjustedProbability;
}

void GamePlay::changeBallPossession(Team * & team) {

    this->resetPhases();

    if (team == this->_match->team(MatchType::Location::HOSTS)) {

        team = this->_match->team(MatchType::Location::VISITORS);
        if (this->displayOn(MW)) {

            _mw->ui->visitorsInPossessionLabel->setEnabled(true);
            _mw->ui->hostsInPossessionLabel->setEnabled(false);
        }
    }
    else {

        team = this->_match->team(MatchType::Location::HOSTS);
        if (this->displayOn(MW)) {

            _mw->ui->hostsInPossessionLabel->setEnabled(true);
            _mw->ui->visitorsInPossessionLabel->setEnabled(false);
        }
    }

    if (this->displayOn(MW))
        _mw->logRecord(team->name() + QStringLiteral(" team is now in possession of the ball."));

    return;
}

uint32_t GamePlay::playerSuitabilityAssessment(const player::PreferredForAction action, Player * const player) const {

    uint32_t assessmentValue = player->condition(player::Conditions::OVERALL);

    switch (action) {

        case player::PreferredForAction::KICK_OFF: // fall through
        case player::PreferredForAction::PENALTY: // fall through
        case player::PreferredForAction::CONVERSION:
            assessmentValue *= player->attribute(player::Attributes::KICKING); break;
        case player::PreferredForAction::LINEOUT: // fall through
        case player::PreferredForAction::SCRUM:
            assessmentValue *= player->attribute(player::Attributes::HANDLING); break;
        default: assessmentValue *= 1;
    }

    return assessmentValue;
}

Player * GamePlay::searchForPlayerWhoTakesOverBall() const {

    // current player's position type (e.g. second row)
    const PlayerPosition_index_item::PositionType currentPlayerPositionType =
        this->_playerInPossession->position()->positionType();

    // every position type has certain probability of taking over the ball
    // note: these probabilities depend on current player's position type
    uint8_t sumOfProbabilities = 0;
    QMap<uint8_t, PlayerPosition_index_item::PositionType> probabilitiesForPositionTypes;
    for (uint8_t i = 0; i <= static_cast<uint8_t>(PlayerPosition_index_item::PositionType::FULLBACK); ++i) {

        probabilitiesForPositionTypes.insert(
            sumOfProbabilities += (7 - std::abs(i - static_cast<uint8_t>(currentPlayerPositionType))),
            static_cast<PlayerPosition_index_item::PositionType>(i));
    }

    uint8_t noOfPlayersWhoCanTakeOverBall = 0;
    Player * playerWhoTakesOverTheBall = nullptr;

    do {

        // which position type takes over the ball
        PlayerPosition_index_item::PositionType futurePlayerPositionType = currentPlayerPositionType;
        const uint8_t probability = RandomValue::generateRandomInt<uint8_t>(1, sumOfProbabilities);

        QMap<uint8_t, PlayerPosition_index_item::PositionType>::key_iterator key_it = probabilitiesForPositionTypes.keyBegin();
        for (; key_it != probabilitiesForPositionTypes.keyEnd(); ++key_it)

            // QMap's keys contain cummulated probabilities (upper bounds) for different position types
            if (probability <= (*key_it)) {

                // if generated probability falls within certain upper bound (key)
                // then this key is used for retrieving associated position type
                // if position type can't be retrieved then currentPlayerPositionType is used (this shouldn't happen)
                futurePlayerPositionType = static_cast<PlayerPosition_index_item::PositionType>
                                           (probabilitiesForPositionTypes.value((*key_it), currentPlayerPositionType));
                break;
            }

        // search for players (with given position type) who are eligible for taking over the ball
        // we search always among players of team-in-possession because in case that possession changes (e.g. when
        // losing the ball to an opponent), function changePlayerInPossession is called only after changeBallPossession
        QVector<Player *> availablePlayers;
        noOfPlayersWhoCanTakeOverBall = _match->team(this->whoIsInPossession().first)->
            availablePlayers(futurePlayerPositionType, _playerInPossession, availablePlayers);

        // which (particular) player takes over the ball
        if (noOfPlayersWhoCanTakeOverBall == 1) // there's only one eligible player
            playerWhoTakesOverTheBall = availablePlayers.at(0);

        if (noOfPlayersWhoCanTakeOverBall > 1) { // there are more eligible players

            const uint8_t playerToPick = RandomValue::generateRandomInt<uint8_t>(1, noOfPlayersWhoCanTakeOverBall);
            playerWhoTakesOverTheBall = availablePlayers.at(playerToPick-1);
        }
    }
    while (noOfPlayersWhoCanTakeOverBall == 0); // if none player is found => repeat process

    return playerWhoTakesOverTheBall;
}

Player * GamePlay::searchForOpponentsPlayer() {

    // team switch is only temporary (to allow us to search for player in opponent's team
    // we can't use changeBallPossession() function for this because
    // that would perform also other (at this moment undesirable) actions
    Team * const teamInPossession = _teamInPossession;
    _teamInPossession = (_teamInPossession == this->_match->team(MatchType::Location::HOSTS))
        ? this->_match->team(MatchType::Location::VISITORS) : this->_match->team(MatchType::Location::HOSTS);

    Player * const opponentPlayer = this->searchForPlayerWhoTakesOverBall();
    _teamInPossession = teamInPossession;

    return opponentPlayer;
}

void GamePlay::displayDiagnosticDataExtended(QStringList & diagnosticDataItems) const {

    diagnosticDataItems.append(html_tags.lineBreak % html_functions.buildBoldText(QStringLiteral("minutes played (match): ")) %
                               QString::number(this->_match->playerStats(this->whoIsInPossession().first,
                                               _playerInPossession)->getStatsValue(StatsType::NumberOf::MINS_PLAYED)));
    diagnosticDataItems.append(html_functions.buildBoldText(QStringLiteral("minutes played (total): ")) %
                               QString::number(this->_playerInPossession->stats()->getStatsValue(StatsType::NumberOf::MINS_PLAYED)));

    uint16_t metres = 0;
    for (auto player: _teamInPossession->squad())
        metres += player->stats()->metresRun();

    diagnosticDataItems.append(html_tags.lineBreak % QStringLiteral("<b>Running:</b> players: ") %
                               QString::number(metres) % QStringLiteral(" m, team: ") +
                               QString::number(_match->score(this->whoIsInPossession().first)
                                               ->stats<uint16_t>(StatsType::NumberOf::METRES_RUN)) %
                               QStringLiteral(" m"));

    metres = 0;
    for (auto player: _teamInPossession->squad())
        metres += player->stats()->metresKicked();

    diagnosticDataItems.append(QStringLiteral("<b>Kicking:</b> players: ") %
                               QString::number(metres) % QStringLiteral(" m, team: ") %
                               QString::number(_match->score(this->whoIsInPossession().first)
                                               ->stats<uint16_t>(StatsType::NumberOf::METRES_KICKED)) %
                               QStringLiteral(" m"));

    diagnosticDataItems.append(html_functions.buildBoldText(QStringLiteral("Players on pitch:")) %
                               string_functions.wrapInBrackets(PitchLocation[MatchType::Location::HOSTS]) %
                               QString::number(this->_match->team(MatchType::Location::HOSTS)->numberOfPlayersOnPitch()) %
                               string_functions.wrapInBrackets(PitchLocation[MatchType::Location::VISITORS]) %
                               QString::number(this->_match->team(MatchType::Location::VISITORS)->numberOfPlayersOnPitch()));
    return;
}

void GamePlay::displayDiagnosticData(const player::PreferredForAction lastAction) const {

    QStringList diagnosticDataItems;

    diagnosticDataItems.append(html_functions.buildBoldText(QStringLiteral("_dateTime: ")) %
                               _match->timePlayed().timePlayed() % html_functions.nonBreakingSpace.repeated(40));
    diagnosticDataItems.append(html_functions.buildBoldText(QStringLiteral("_period: ")) +
                               this->_periods->description(this->_match->currentPeriod()));
    diagnosticDataItems.append(html_functions.buildBoldText(QStringLiteral("_restartPlay: ")) %
                               string_functions.boolValue(_restartPlay) % html_tags.lineBreak);
    diagnosticDataItems.append(html_functions.buildBoldText(QStringLiteral("_distanceFromHalfwayLine: ")) %
                               QString::number(_distanceFromHalfwayLine) % QStringLiteral(" m"));
    diagnosticDataItems.append(html_functions.buildBoldText(QStringLiteral("_incrementCarries: ")) %
                               string_functions.boolValue(_incrementCarries) % html_tags.lineBreak);
    diagnosticDataItems.append(html_functions.buildBoldText(QStringLiteral("_noOfPhases: ")) %
                               QString::number(_noOfPhases) % html_tags.lineBreak);
    diagnosticDataItems.append(html_functions.buildBoldText(QStringLiteral("_teamInPossession: ")) +
                               _teamInPossession->name());
    diagnosticDataItems.append(html_functions.buildBoldText(QStringLiteral("_playerInPossession: ")) +
                               _playerInPossession->fullName());
    diagnosticDataItems.append(html_functions.buildBoldText(QStringLiteral("_playerInPosPosition: ")) +
                               _playerInPossession->position()->currentPosition());

    const QString lastActionDescription = player::preferenceColumnNames.value(lastAction, QString());
    if (!lastActionDescription.isNull())
        diagnosticDataItems.append(html_functions.buildBoldText(QStringLiteral("_action: ")) + lastActionDescription);

    this->displayDiagnosticDataExtended(diagnosticDataItems);

    const QString diagnosticData = diagnosticDataItems.join(html_tags.lineBreak);
    const QMessageBox::StandardButton result =
        QMessageBox::warning(nullptr, QStringLiteral("Diagnostic data"), diagnosticData, QMessageBox::Ok | QMessageBox::Cancel);

    if (result == QMessageBox::Cancel)
        this->_settings->toggleDiagnosticMode(false);

    return;
}

void GamePlay::refreshPointsList(const MatchType::Location loc) const {

    const QMap<Player *, PlayerPoints *> & players = this->_match->allPlayersPoints(loc);

    using iter = QMap<Player *, PlayerPoints *>::const_iterator;
    const QPair<iter, iter> range = qMakePair<iter,iter>(players.cbegin(), players.cend());

    QMap<PointEvent, QStringList> pointsAndPlayers;
    for (auto it = range.first; it != range.second; ++it) {

        if (it.value()->getPointsValue(StatsType::NumberOf::TRIES) > 0) {

            const QString number = (!(it.value()->getPointsValue(StatsType::NumberOf::TRIES) > 1)) ? QString() :
                string_functions.wrapInBrackets(QString::number(it.value()->getPointsValue(StatsType::NumberOf::TRIES)));
            pointsAndPlayers[PointEvent::TRY].append(it.key()->abridgedFullName() + number);
        }
        if (it.value()->getPointsValue(StatsType::NumberOf::CONVERSIONS) > 0) {

            const QString number = (!(it.value()->getPointsValue(StatsType::NumberOf::CONVERSIONS) > 1)) ? QString() :
                string_functions.wrapInBrackets(QString::number(it.value()->getPointsValue(StatsType::NumberOf::CONVERSIONS)));
            pointsAndPlayers[PointEvent::CONVERSION].append(it.key()->abridgedFullName() + number);
        }
        if (it.value()->getPointsValue(StatsType::NumberOf::PENALTIES) > 0) {

            const QString number = (!(it.value()->getPointsValue(StatsType::NumberOf::PENALTIES) > 1)) ? QString() :
                string_functions.wrapInBrackets(QString::number(it.value()->getPointsValue(StatsType::NumberOf::PENALTIES)));
            pointsAndPlayers[PointEvent::PENALTY].append(it.key()->abridgedFullName() + number);
        }
        if (it.value()->getPointsValue(StatsType::NumberOf::DROPGOALS) > 0) {

            const QString number = (!(it.value()->getPointsValue(StatsType::NumberOf::DROPGOALS) > 1)) ? QString() :
                string_functions.wrapInBrackets(QString::number(it.value()->getPointsValue(StatsType::NumberOf::DROPGOALS)));
            pointsAndPlayers[PointEvent::DROPGOAL].append(it.key()->abridgedFullName() + number);
        }
    }

    _mw->displayPoints(pointsAndPlayers, loc);

    return;
}

void GamePlay::updateStatistics(const MatchType::Location loc, const StatsType::NumberOf stats, Player * player) {

    // totals (stored "under" player)
    player->stats()->incrementStatsValue(stats);
    // current game (stored "under" match/player_score)
    this->_match->playerStats(loc, player)->incrementStatsValue(stats);

    // note: Ʃ(games-stats) = total-stats
    return;
}

void GamePlay::changePlayerInPossession() {

    _incrementCarries = true;

    // no position for a specialist = passing to team-mate or losing ball to opponent
    _playerInPossession = this->searchForPlayerWhoTakesOverBall();

    if (this->displayOn(MW)) {

        _mw->updatePlayer(_playerInPossession->fullName(), this->whoIsInPossession().first);
        if (this->_settings->diagnosticMode())
            this->displayDiagnosticData();
    }

    return;
}

void GamePlay::changePlayerInPossessionToSpecialist(const player::PreferredForAction action) {

    _incrementCarries = true;

    if (action == player::PreferredForAction::NO_ACTION)
        { _playerInPossession = this->searchForPlayerWhoTakesOverBall(); return; }

    const MatchType::Location teamInPossession = this->whoIsInPossession().first;

    QMap<uint32_t, Player *> playersPreferredForAction;
    Player * randomPlayerIfNoPreferredPlayer;
    uint8_t randomPlayerNo = RandomValue::generateRandomInt(static_cast<uint8_t>(1),
                             this->_match->team(teamInPossession)->numberOfPlayersOnPitch());

    for (auto player: this->_match->team(teamInPossession)->squad()) {

        if (!player->isOnPitch() || !player->isHealthy())
            continue;

        if (--randomPlayerNo == 0)
            randomPlayerIfNoPreferredPlayer = player;

        if (player->isPreferredFor(action))
            playersPreferredForAction.insert(this->playerSuitabilityAssessment(action, player), player);
    }

    bool selectedInDialog = false;
    if (this->displayOn(MW) && playersPreferredForAction.size() > 1 && !_automaticSelection &&
        (action == player::PreferredForAction::PENALTY || action == player::PreferredForAction::CONVERSION)) {

        QStringList playersForSelection;
        for (auto player: playersPreferredForAction) {

            const QString playerDescription =
                player->fullName() % string_functions.wrapInBrackets(QString::number(player->shirtNo())) %
                QStringLiteral("- ") % player->position()->currentPosition();
            playersForSelection.append(playerDescription);
        }

        const QString selectedPlayer = QInputDialog::getItem(nullptr, QStringLiteral("Select player"),
            message.displayWithReplace(this->objectName(), QStringLiteral("selectPlayerForAction"),
            { player::preferenceColumnNames[action] }), playersForSelection, 0, false, &selectedInDialog);
        _automaticSelection = !selectedInDialog;

        for (auto player: playersPreferredForAction)
            if (selectedPlayer.contains(player->fullName()) && selectedPlayer.contains(QString::number(player->shirtNo())))
                _playerInPossession = player;
    }

    if (!selectedInDialog) {

        // select either randomly picked player or "best" player from all players with given preference
        _playerInPossession = (playersPreferredForAction.isEmpty()) ? randomPlayerIfNoPreferredPlayer :
                               playersPreferredForAction[playersPreferredForAction.lastKey()];
    }

    if (this->displayOn(MW)) {

        _mw->updatePlayer(_playerInPossession->fullName(), teamInPossession);
        if (this->_settings->diagnosticMode())
            this->displayDiagnosticData(action);
    }

    return;
}

// result: first team (type) is in possession of the ball, second team (type) is not in possession of the ball
// results either in <HOSTS, VISITORS> or in <VISITORS, HOSTS>
QPair<MatchType::Location, MatchType::Location> GamePlay::whoIsInPossession() const {

    const QPair<MatchType::Location, MatchType::Location> inPossession =
        qMakePair<MatchType::Location, MatchType::Location>(MatchType::Location::HOSTS, MatchType::Location::VISITORS);

    return ((_match->team(MatchType::Location::HOSTS) == _teamInPossession) ?
            inPossession : qMakePair(inPossession.second, inPossession.first));
}

int8_t GamePlay::distanceToGoalLine() const {

    const int8_t distanceForHosts =
        static_cast<int8_t>(groundDimensions.fromGoalLineToHalfwayLine) - this->_distanceFromHalfwayLine;

    if (this->_teamInPossession == this->_match->team(MatchType::Location::HOSTS))
        return distanceForHosts;
    else
        return (static_cast<int8_t>(groundDimensions.fromGoalLineToGoalLine) - distanceForHosts);
}

// return value: returns pointer to Team which is in posession of ball
// ie. either won the draw and chose ball or lost the draw and opponent chose side
Team * GamePlay::draw() {

    this->_match->timePlayed().switchTimePeriodTo();
    if (this->displayOn(MW)) {

        _mw->ui->updatePeriod(this->_periods->description(this->_match->currentPeriod()));
        _mw->logRecord(QStringLiteral("Draw in progress."));

        // show information message
        _mw->timeStoppedMessageBox("beforeStartOfMatch", { this->_match->referee()->referee() } );
    }

    this->_hostsFirstKickOff = RandomValue::generateRandomBool(50);
    const MatchType::Location team = (this->_hostsFirstKickOff) ? MatchType::Location::HOSTS : MatchType::Location::VISITORS;
    Team * teamInPossession = this->_match->team(team);
    if (this->displayOn(MW))
        _mw->logRecord(QStringLiteral("Draw won by: ") + teamInPossession->name());

    if (teamInPossession == this->_myTeam) {

        if (this->displayOn(MW)) {

            QMessageBox::StandardButton sideOrBall = QMessageBox::question(nullptr, QStringLiteral("Draw won."),
                QStringLiteral("Would you prefer to choose side (Yes) over ball (No)?"),
                QMessageBox::StandardButtons(QMessageBox::Yes|QMessageBox::No));

            if (sideOrBall == QMessageBox::Yes) {

                this->changeBallPossession(teamInPossession);
                this->_hostsFirstKickOff = !this->_hostsFirstKickOff;
                // vybrat stranu
            }
        }
    }
    else {

        const bool sideOrBall = RandomValue::generateRandomBool(70);
        if (sideOrBall) {

            this->changeBallPossession(teamInPossession);
            this->_hostsFirstKickOff = !this->_hostsFirstKickOff;
            if (this->displayOn(MW))
                QMessageBox::information(nullptr, QStringLiteral("Draw lost."), QStringLiteral("Your opponent chooses side."));
        }
        else {

            if (this->displayOn(MW))
                QMessageBox::information(nullptr, QStringLiteral("Draw lost."), QStringLiteral("Your opponent chooses ball."));
            // vybrat stranu
        }
    }

    // tag team in possession of the ball (with icon)
    if (this->displayOn(MW)) {

        _mw->ui->hostsInPossessionLabel->setEnabled(this->_hostsFirstKickOff);
        _mw->ui->visitorsInPossessionLabel->setEnabled(!this->_hostsFirstKickOff);
    }

    return teamInPossession;
}

void GamePlay::updateBallPositionProgressBars() const {

    if (_distanceFromHalfwayLine >= 0) {

        _mw->ui->ballPositionVisitorsProgressBar->setValue(0);
        const uint8_t value = std::min<uint8_t>(static_cast<uint8_t>(_distanceFromHalfwayLine),
                                                groundDimensions.fromGoalLineToHalfwayLine);
        _mw->ui->ballPositionHostsProgressBar->setValue(value);
        _mw->ui->ballPositionHostsProgressBar->repaint();
    }

    if (_distanceFromHalfwayLine <= 0) {

        _mw->ui->ballPositionHostsProgressBar->setValue(0);
        const uint8_t value = std::min<uint8_t>(static_cast<uint8_t>(std::abs(_distanceFromHalfwayLine)),
                                                groundDimensions.fromGoalLineToHalfwayLine);
        _mw->ui->ballPositionVisitorsProgressBar->setValue(value);
        _mw->ui->ballPositionVisitorsProgressBar->repaint();
    }

    return;
}

void GamePlay::changeBallPositionOnPitch(const uint8_t metresMade) {

    if (this->whoIsInPossession().first == MatchType::Location::HOSTS)
        _distanceFromHalfwayLine += metresMade;

    if (this->whoIsInPossession().first == MatchType::Location::VISITORS)
        _distanceFromHalfwayLine -= metresMade;

    // update ball position progress bars
    if (this->displayOn(MW))
        this->updateBallPositionProgressBars();

    return;
}

// move ball to specific position on pitch (back towards half-way line) <=> without having any impact on statistics
// e.g. penalties, line-outs and scrums can't be executed from within less than 5m distance from the goal-line
uint8_t GamePlay::moveBallToSpecificPositionOnPitch(const uint8_t moveToThisDistanceFromGoalLine) {

    if (moveToThisDistanceFromGoalLine > groundDimensions.fromGoalLineToHalfwayLine)
        return (static_cast<uint8_t>(this->distanceToGoalLine()));

    // result might be negative (e.g. if try was scored than _distanceFromHalfwayLine > 50)
    if (static_cast<int8_t>(groundDimensions.fromGoalLineToHalfwayLine)
        - std::abs(_distanceFromHalfwayLine) < moveToThisDistanceFromGoalLine) {

        _distanceFromHalfwayLine = _distanceFromHalfwayLine/std::abs(_distanceFromHalfwayLine) *
            (groundDimensions.fromGoalLineToHalfwayLine - moveToThisDistanceFromGoalLine);

        // update ball position progress bars
        if (this->displayOn(MW))
            this->updateBallPositionProgressBars();
    }

    return (static_cast<uint8_t>(this->distanceToGoalLine()));
}

double GamePlay::kickAtGoalProbability(const uint8_t distanceFromMiddle, const uint8_t metresFromGoalLine) const {

    // calculace effective angle for the penalty (the wider the angle the greater the probability of success)
    const double distanceToFirstGoalPost = std::abs(groundDimensions.widthBetweenGoalPosts/2.0 - distanceFromMiddle);
    const double distanceToSecondGoalPost = distanceFromMiddle + groundDimensions.widthBetweenGoalPosts/2.0;

    const double angleToFirstGoalPost = std::atan(distanceToFirstGoalPost/metresFromGoalLine);
    const double angleToSecondGoalPost = std::atan(distanceToSecondGoalPost/metresFromGoalLine);

    const double effectiveAngle = (distanceFromMiddle < groundDimensions.widthBetweenGoalPosts/2.0)
        /* if inside goal posts => add angles */ ? angleToSecondGoalPost + angleToFirstGoalPost
        /* if outside goal posts => subtract angles */ : angleToSecondGoalPost - angleToFirstGoalPost;

    // probability based on angle
    const double max_to_min_ratio = groundDimensionsInferred.maxAngle / groundDimensionsInferred.minAngle;
    const double effective_to_min_ratio = effectiveAngle / groundDimensionsInferred.minAngle;
    const double effective_in_max_min_range_ratio = effective_to_min_ratio / max_to_min_ratio;
    const double angleProbabilityCoef = (1 - effective_in_max_min_range_ratio)/2;
    const double angleProbability = effective_in_max_min_range_ratio + angleProbabilityCoef;

    // probability based on distance
    const double kickDistance = this->kickDistance(distanceFromMiddle, metresFromGoalLine);
    const double distanceProbability = (kickDistance <= groundDimensionsInferred.maxDistance * 0.8) ? 1 : 0.8;

    // probability based on player's attributes (kicking)
    const double playerSkillProbability =
        0.8 + (_playerInPossession->attribute(player::Attributes::KICKING) / static_cast<double>(50));

    return (angleProbability * distanceProbability * playerSkillProbability);
}

void GamePlay::penaltyScored() const {

    // update points and penalties count
    const MatchType::Location team = this->whoIsInPossession().first;

    const uint8_t currentValuePenalties = this->_match->score(team)->penaltiesScored();
    _playerInPossession->points()->penaltyScored();
    this->_match->playerPoints(team, _playerInPossession)->penaltyScored();

    if (this->displayOn(MW)) {

        _mw->updateStatisticsUI(team, QStringLiteral("PointsLabel"), QString::number(this->_match->score(team)->points()));
        _mw->updateStatisticsUI(team, QStringLiteral("PenaltiesLabel"), QString::number(currentValuePenalties));

        this->refreshPointsList(team);

        // display current score in log
        _mw->logRecord(_mw->currentScore());

        // show information message
        _mw->timeStoppedMessageBox("penaltyScored",
            { player::preferenceColumnNames[player::PreferredForAction::PENALTY], _playerInPossession->fullName() });
    }
    if (this->displayOn(FW))
        _fw->updateScore(team);

    this->changeInMorale(_playerInPossession, true);

    return;
}

QString GamePlay::selectActionAfterPenaltyInfringement(const QStringList & optionsForPenalty, const uint8_t distanceFromMiddle) const {

    const MatchType::Location team = this->whoIsInPossession().first;
    const bool pointsInSpecifiedRange =
        this->_match->pointDifferenceInRange(team, -pointValue.Penalty, -(pointValue.Try+pointValue.Conversion), false);

    // select scrum if: a] distance <= 10m, b] score difference is in (3,7] or no of tries is 3
    if (optionsForPenalty.contains(GamePlay::actionAfterPenaltyInfringement[GamePlay::PenaltyAction::SCRUM]) &&
        this->distanceToGoalLine() <= (groundDimensions.fromGoalLineTo5metreLine * 2) &&
        (pointsInSpecifiedRange || this->_match->score(team)->points(PointEvent::TRY) == matchPoints.NoOfPointsForDiffPoint-1))
        return GamePlay::actionAfterPenaltyInfringement[GamePlay::PenaltyAction::SCRUM];

    // select kick at goal if: a] distance is <= 22m, b] side distance <= 12
    if (optionsForPenalty.contains(GamePlay::actionAfterPenaltyInfringement[GamePlay::PenaltyAction::KICK_AT_GOAL]) &&
        this->distanceToGoalLine() <= groundDimensions.fromGoalLineTo22metreLine &&
        distanceFromMiddle <= (groundDimensions.fromTouchToHalfwayPoint / 3 + 1))
        return GamePlay::actionAfterPenaltyInfringement[GamePlay::PenaltyAction::KICK_AT_GOAL];

    // select kick into touch if: a] distance > 22m, b] score difference is in (3,7] x no of tries is 3
    if (optionsForPenalty.contains(GamePlay::actionAfterPenaltyInfringement[GamePlay::PenaltyAction::KICK_TO_TOUCH]) &&
        this->distanceToGoalLine() > groundDimensions.fromGoalLineTo22metreLine &&
        (pointsInSpecifiedRange || this->_match->score(team)->points(PointEvent::TRY) == matchPoints.NoOfPointsForDiffPoint-1))
        return GamePlay::actionAfterPenaltyInfringement[GamePlay::PenaltyAction::KICK_TO_TOUCH];

    const uint8_t randomSelection = RandomValue::generateRandomInt<uint8_t>(0, optionsForPenalty.size()-1);

    return optionsForPenalty.at(randomSelection);
}

timePassed GamePlay::penalty() {

    this->resetPhases();

    // update penalty infringements' statistics
    const MatchType::Location team = this->whoIsInPossession().second;
    const uint8_t currentValueInfringements = this->_match->score(team)->penaltyInfringements();
    if (this->displayOn(MW))
        _mw->updateStatisticsUI(team, QStringLiteral("PenaltyInfringementsLabel"), QString::number(currentValueInfringements));

    // find out where the infringement has occurred (distance from the middle of the goal-line)
    const uint8_t distanceFromMiddle = RandomValue::generateRandomInt<uint8_t>(0, groundDimensions.fromTouchToHalfwayPoint);

    const uint8_t distanceFromGoalLine = this->distanceToGoalLine();
    // distance from goal-line from which the penalty kick (or other selected type of restart)
    // is going to be executed (this can't be closer than 5m from the goal-line)
    const uint8_t metresFromGoalLine = this->moveBallToSpecificPositionOnPitch(groundDimensions.fromGoalLineTo5metreLine);
    const double kickDistance = this->kickDistance(distanceFromMiddle, metresFromGoalLine);

    // options how to resume play after penalty infringement
    QStringList optionsForPenalty = actionAfterPenaltyInfringement.values();
    QString selectedAction = GamePlay::actionAfterPenaltyInfringement[GamePlay::PenaltyAction::KICK_TO_TOUCH];

    // penalty kick (at goal) can't be executed from over halfway-line
    if (this->distanceToGoalLine() >= std::min(this->_settings->kickMaxDistance(), groundDimensions.fromGoalLineToHalfwayLine))
        optionsForPenalty.removeOne(GamePlay::actionAfterPenaltyInfringement[GamePlay::PenaltyAction::KICK_AT_GOAL]);
    // tap penalty is possible only on some occassions (and not within 5m of the goal-line)
    if (!RandomValue::generateRandomBool(this->_settings->matchActivities().probability
        (MatchActionSubtype::TAP_PENALTY_POSSIBLE)) || distanceFromGoalLine < 5)
        optionsForPenalty.removeOne(GamePlay::actionAfterPenaltyInfringement[GamePlay::PenaltyAction::TAP_PENALTY]);

    if (this->displayOn(MW) && _myTeam == _teamInPossession) {

        const QString side = (RandomValue::generateRandomBool(50)) ? QStringLiteral("left") : QStringLiteral("right");

        const QString restartMovedTo5m = (distanceFromGoalLine < 5 || distanceFromGoalLine > 95)
                                       ? QStringLiteral(" Restart is moved to 5m line.") : QString();
        const QString dialogText = message.displayWithReplace(this->objectName(), "penaltyAttempt",
            { QString::number(distanceFromGoalLine), QString::number(distanceFromMiddle), side, restartMovedTo5m });

        selectedAction = QInputDialog::getItem(nullptr, QStringLiteral("Select action after penalty infringement"),
                                               dialogText, optionsForPenalty);
    }
    else
        // automatic selection of (type of) resuming play from possible options (after penalty infringement)
        selectedAction = this->selectActionAfterPenaltyInfringement(optionsForPenalty, distanceFromMiddle);

    const GamePlay::PenaltyAction actionAfterInfringement = actionAfterPenaltyInfringement.key(selectedAction);

    switch (actionAfterInfringement) {

        case GamePlay::PenaltyAction::KICK_AT_GOAL: {

            this->changePlayerInPossessionToSpecialist(player::PreferredForAction::PENALTY);

            if (this->displayOn(MW)) {

                _mw->logRecord(message.displayWithReplace(this->penaltySelectedType(),
                               QStringLiteral("penaltyKickAtGoal"), { _teamInPossession->name() }));
                _mw->updatePlayer(_playerInPossession->fullName(), this->whoIsInPossession().first);
            }

            // calculate probability of success
            const double probability = this->kickAtGoalProbability(distanceFromMiddle, metresFromGoalLine) *
                                       this->_settings->matchActivities().probability(MatchActionSubtype::PENALTY_SCORED);

            const bool penaltyScored = RandomValue::generateRandomBool(static_cast<uint8_t>(probability));
            QString penaltyScoredText =
                QStringLiteral("Penalty kick (from ") % QString::number(kickDistance, 'f', 2) % QStringLiteral(" m) by ") %
                _playerInPossession->fullName() % QStringLiteral(": kick at goal was not successful");

            if (!penaltyScored) {

                if (this->displayOn(MW)) {

                    _mw->logRecord(penaltyScoredText);
                }

                // if the ball is caught by the opposition before it leaves the field of play, play continues (not implemented);
                // otherwise play is restarted with a drop-out from the offenders' 22m line (currently always)
                this->moveBallToSpecificPositionOnPitch(groundDimensions.fromGoalLineTo22metreLine);
            }
            else {

                if (this->displayOn(MW)) {

                    penaltyScoredText = penaltyScoredText.replace("not ", QString()) + _mw->pointsInfoForLog(pointValue.Penalty);
                    _mw->logRecord(penaltyScoredText);
                }
                this->penaltyScored();
                this->_restartPlay = true; // restart kick follows
            }

            // team which was the penalty kicked against takes possession of the ball
            this->changeBallPossession(_teamInPossession);
            this->changePlayerInPossessionToSpecialist(player::PreferredForAction::KICK_OFF);

            if (this->displayOn(MW))
                _mw->updatePlayer(_playerInPossession->fullName(), this->whoIsInPossession().first);

            break;
        }
        case GamePlay::PenaltyAction::KICK_TO_TOUCH: {

            this->changePlayerInPossessionToSpecialist(player::PreferredForAction::KICK_OFF);

            if (this->displayOn(MW)) {

                _mw->logRecord(message.displayWithReplace(this->penaltySelectedType(),
                               QStringLiteral("penaltyKickIntoTouch"), { _teamInPossession->name() }));
                _mw->updatePlayer(_playerInPossession->fullName(), this->whoIsInPossession().first);
            }

            // kick distance
            const uint8_t maxDistance = std::min<uint8_t>(_settings->kickMaxDistance(), distanceToGoalLine());
            uint8_t metresMade = RandomValue::generateRandomInt<uint8_t>(1, maxDistance);

            // has kick really ended in touch (as was intended) or not?
            const uint8_t probability = this->_settings->matchActivities().probability(MatchActionSubtype::PENALTY_KICK_INTO_TOUCH);
            const bool kickIntoTouch = RandomValue::generateRandomBool(probability);

            // if line-out would be thrown within 5m distance off goal line, it is formed on the 5-metre line
            if (kickIntoTouch && (this->distanceToGoalLine() - metresMade < groundDimensions.fromGoalLineTo5metreLine))
                metresMade = this->distanceToGoalLine() - groundDimensions.fromGoalLineTo5metreLine;

            // change ball position
            this->changeBallPositionOnPitch(metresMade);

            // if the ball makes it to the touch line-out follows
            if (kickIntoTouch) {

                this->refreshTime(timeForGameAction.PENALTY, false);
                const timePassed timePassed = this->lineOutIsThrowed();
                return timePassed;
            }
            else {

                // if the kick (into touch) is unsuccessful and the ball is caught by the opposition before it leaves
                // the field of play, play continues; if the ball goes into touch-in-goal then play is restarted with
                // a drop-out from the offenders' 22m line (currently not implemented)

                if (this->displayOn(MW))
                    _mw->logRecord(message.display(this->penaltySelectedType(), QStringLiteral("kickIntoTouchMissed")));

                this->changeBallPossession(_teamInPossession);
                this->changePlayerInPossession();

                return (timeForGameAction.PENALTY/2);
            }

            break;
        }
        case GamePlay::PenaltyAction::SCRUM: {

            if (this->displayOn(MW))
                _mw->logRecord(message.displayWithReplace(this->penaltySelectedType(),
                               QStringLiteral("scrumInsteadOfPenalty"), { _teamInPossession->name() }));

            const timePassed timePassed = this->scrum();
            return timePassed;
        }
        case GamePlay::PenaltyAction::TAP_PENALTY: {

            this->changePlayerInPossession();
            return (timeForGameAction.PENALTY/4);
        }
        default: ;
    }

    return timeForGameAction.PENALTY;
}

void GamePlay::conversionScored() const {

    // update points and conversions count
    const MatchType::Location team = this->whoIsInPossession().first;

    const uint8_t currentValueConversions = this->_match->score(team)->conversionScored();
    _playerInPossession->points()->conversionScored();
    this->_match->playerPoints(team, _playerInPossession)->conversionScored();

    if (this->displayOn(MW)) {

        _mw->updateStatisticsUI(team, QStringLiteral("PointsLabel"), QString::number(this->_match->score(team)->points()));
        _mw->updateStatisticsUI(team, QStringLiteral("ConversionsLabel"), QString::number(currentValueConversions));

        this->refreshPointsList(team);

        // display current score in log
        const QString conversionScoredText = QStringLiteral("Try converted by ") % _playerInPossession->fullName() %
                                             '.' % _mw->pointsInfoForLog(pointValue.Conversion);
        _mw->logRecord(conversionScoredText);
        _mw->logRecord(_mw->currentScore());

        // show information message
        _mw->timeStoppedMessageBox("conversionScored",
            { player::preferenceColumnNames[player::PreferredForAction::CONVERSION], _playerInPossession->fullName() });
    }
    if (this->displayOn(FW))
        _fw->updateScore(team);

    this->changeInMorale(_playerInPossession, true);

    return;
}

timePassed GamePlay::conversionAttempt() {

    // find out where the try has been scored (distance from the middle of the goal-line)
    const uint8_t distanceFromMiddle = RandomValue::generateRandomInt<uint8_t>(0, groundDimensions.fromTouchToHalfwayPoint);

    uint8_t metresFromGoalLine = groundDimensions.fromGoalLineTo5metreLine;
    bool executeConversion = true; // team can decide not to execute the conversion kick

    if (this->displayOn(MW) && _myTeam == _teamInPossession) {

        const QString side = (RandomValue::generateRandomBool(50)) ? QStringLiteral("left") : QStringLiteral("right");

        // distance from goal-line from which the conversion kick is going to be executed
        const QString dialogText = message.displayWithReplace(this->objectName(), "conversionAttempt",
                                                              { QString::number(distanceFromMiddle), side });

        metresFromGoalLine = QInputDialog::getInt(nullptr, QStringLiteral("Conversion"), dialogText,
                                                  metresFromGoalLine, groundDimensions.fromGoalLineTo5metreLine,
                                                  groundDimensions.fromGoalLineToHalfwayLine, 1, &executeConversion);
    }
    else {

        // distance from goal-line from which the conversion kick is going to be executed
        if (static_cast<float>(distanceFromMiddle) <= std::round(groundDimensions.widthBetweenGoalPosts/2.0f))
            metresFromGoalLine = groundDimensions.fromGoalLineTo5metreLine;
        else if (static_cast<float>(distanceFromMiddle) <= std::round(groundDimensions.fromTouchToHalfwayPoint/2.0f))
            metresFromGoalLine = RandomValue::generateRandomInt(
                static_cast<uint8_t>(groundDimensions.fromGoalLineTo5metreLine * 2), groundDimensions.fromGoalLineTo22metreLine);
        else metresFromGoalLine = RandomValue::generateRandomInt(
                groundDimensions.fromGoalLineTo22metreLine, groundDimensions.fromGoalLineTo10metreLine);

        executeConversion = RandomValue::generateRandomBool(
            this->_settings->matchActivities().probability(MatchActionSubtype::MatchActivityType::CONVERSION_KICKED));
    }

    if (executeConversion) {

        this->changePlayerInPossessionToSpecialist(player::PreferredForAction::CONVERSION);

        if (this->displayOn(MW))
            _mw->updatePlayer(_playerInPossession->fullName(), this->whoIsInPossession().first);
    }
    else  { // conversion is not executed (as desired by team which scored)

        this->changeBallPossession(_teamInPossession);
        this->changePlayerInPossessionToSpecialist(player::PreferredForAction::KICK_OFF);
        this->_restartPlay = true;

        if (this->displayOn(MW))
            _mw->updatePlayer(_playerInPossession->fullName(), this->whoIsInPossession().first);

        this->changeInMorale(_teamInPossession, false);

        return timePassed(0);
    }

    // calculate probability
    const double probability = this->kickAtGoalProbability(distanceFromMiddle, metresFromGoalLine) *
                               this->_settings->matchActivities().probability(MatchActionSubtype::CONVERSION_SUCCESSFUL);

    const bool conversionConverted = RandomValue::generateRandomBool(static_cast<uint8_t>(probability));

    if (!conversionConverted) {

        if (this->displayOn(MW)) {

            const QString conversionScoredText = QStringLiteral("Try not converted.");
            _mw->logRecord(conversionScoredText);
        }
    }
    else
        this->conversionScored();

    // team which was scored against takes possession of the ball (restart kick follows)
    this->changeBallPossession(_teamInPossession);
    this->changePlayerInPossessionToSpecialist(player::PreferredForAction::KICK_OFF);
    this->_restartPlay = true;

    this->changeInMorale(_teamInPossession, false);

    if (this->displayOn(MW))
        _mw->updatePlayer(_playerInPossession->fullName(), this->whoIsInPossession().first);

    return timeForGameAction.CONVERSION;
}

timePassed GamePlay::tryScored() {

    const MatchType::Location team = this->whoIsInPossession().first;

    // update points and tries count
    const uint8_t currentValueTries = this->_match->score(team)->tryScored();
    _playerInPossession->points()->tryScored();
    this->_match->playerPoints(team, _playerInPossession)->tryScored();

    if (this->displayOn(MW)) {

        _mw->updateStatisticsUI(team, QStringLiteral("PointsLabel"), QString::number(this->_match->score(team)->points()));
        _mw->updateStatisticsUI(team, QStringLiteral("TriesLabel"), QString::number(currentValueTries));

        // bonus point try
        const bool bonusPoint = (currentValueTries == matchPoints.NoOfTriesForBonusPoint);
        if (bonusPoint) {

            const QString prefix = (team == MatchType::Location::HOSTS) ? on::shared.hostsPrefix : on::shared.visitorsPrefix;
            const QString labelName = prefix + QStringLiteral("BonusPointLabel");
            QLabel * const bonusPointLabel = _mw->findChild<QLabel *>(labelName, Qt::FindDirectChildrenOnly);

            bonusPointLabel->setStyleSheet(ss::shared.style(ss::matchwidget.BonusPointStyleEnabled));
        }

        this->refreshPointsList(team);

        // display current score in log
        const QString bonusPointTry = (bonusPoint) ? QStringLiteral(" [bonus point try]") : QString();
        const QString tryScoredText = QStringLiteral("Try scored by: ") % _playerInPossession->fullName() % ", " %
                                      _teamInPossession->name() % _mw->pointsInfoForLog(pointValue.Try) % bonusPointTry;
        _mw->logRecord(tryScoredText);
        _mw->logRecord(_mw->currentScore());

        // show information message
        _mw->timeStoppedMessageBox("tryScored", { _playerInPossession->fullName(), _teamInPossession->name(),
                                                  QString::number(this->_match->score(team)->points(PointEvent::TRY)) });
    }
    if (this->displayOn(FW))
        _fw->updateScore(team);

    this->changeInMorale(_teamInPossession, true);

    // team which was scored against takes possession of the ball (but only after conversion kick is carried out)
    // => don't switch ball possession yet; it will/should be switched after conversion

    return timeForGameAction.TRY_SCORED;
}

timePassed GamePlay::dropGoalScored() {

    const MatchType::Location team = this->whoIsInPossession().first;

    // update points and drop goals count
    const uint8_t currentValueDropGoals = this->_match->score(team)->dropScored();
    _playerInPossession->points()->dropGoalScored();
    this->_match->playerPoints(team, _playerInPossession)->dropGoalScored();

    if (this->displayOn(MW)) {

        _mw->updateStatisticsUI(team, QStringLiteral("PointsLabel"), QString::number(this->_match->score(team)->points()));
        _mw->updateStatisticsUI(team, QStringLiteral("DropGoalsLabel"), QString::number(currentValueDropGoals));

        this->refreshPointsList(team);

        // display current score in log
        const QString dropGoalScoredText = QStringLiteral("Drop goal scored by: ") % _playerInPossession->fullName() %
                                           ", " % _teamInPossession->name() % _mw->pointsInfoForLog(pointValue.DropGoal);
        _mw->logRecord(dropGoalScoredText);
        _mw->logRecord(_mw->currentScore());

        // show information message
        _mw->timeStoppedMessageBox("dropGoalScored", { _playerInPossession->fullName(), _teamInPossession->name() });
    }
    if (this->displayOn(FW))
        _fw->updateScore(team);

    this->changeInMorale(_playerInPossession, true);

    // team which was scored against takes possession of the ball (restart kick follows)
    this->changeBallPossession(_teamInPossession);
    this->changePlayerInPossessionToSpecialist(player::PreferredForAction::KICK_OFF);
    this->_restartPlay = true;

    if (this->displayOn(MW))
        _mw->updatePlayer(_playerInPossession->fullName(), this->whoIsInPossession().first);

    return timeForGameAction.DROP_GOAL;
}

void GamePlay::ballCarried() {

    const MatchType::Location team = this->whoIsInPossession().first;
    const QString newValueCarries = QString::number(this->_match->score(team)->carries());

    this->updateStatistics(team, StatsType::NumberOf::CARRIES, _playerInPossession);

    if (this->displayOn(MW))
        _mw->updateStatisticsUI(team, QStringLiteral("CarriesLabel"), newValueCarries);

    // toggle back to true after player in possession changes
    this->_incrementCarries = false;

    return;
}

void GamePlay::ballPassed(const MatchScore::Passes pass) {

    MatchType::Location team = this->whoIsInPossession().first;
    this->_match->score(team)->passAttempted(pass);

    if (this->displayOn(MW)) {

        // update passes' (either completed or missed) stats (team in possession)
        const QString passesLabelName = (pass == MatchScore::Passes::COMPLETED) ?
            QStringLiteral("PassesCompletedLabel") : QStringLiteral("PassesMissedLabel");
        const QString newValuePasses = QString::number(this->_match->score(team)->passes(pass));
        _mw->updateStatisticsUI(team, passesLabelName, newValuePasses);

        // update passes' (made in total) stats
        const QString newValuePassesMade = QString::number(this->_match->score(team)->passes(MatchScore::Passes::ATTEMPTED));
        _mw->updateStatisticsUI(team, QStringLiteral("PassesMadeLabel"), newValuePassesMade);

        // update passes' success rate stats
        _mw->updateStatisticsUI(team, QStringLiteral("PassesSuccessRateLabel"), this->_match->score(team)->passesSuccessRate());
    }

    return;
}

timePassed GamePlay::scrum() {

    this->resetPhases();

    // scrum can't be executed from within less than 5m distance of the goal-line
    const uint8_t metresFromGoalLine = this->moveBallToSpecificPositionOnPitch(groundDimensions.fromGoalLineTo5metreLine);

    const MatchType::Location team = this->whoIsInPossession().first;
    const MatchType::Location opponent = this->whoIsInPossession().second;

    this->changePlayerInPossessionToSpecialist(player::PreferredForAction::SCRUM);

    // display in log
    const QString scrumInOwn22 = (metresFromGoalLine <= groundDimensions.fromGoalLineTo22metreLine)
        ? QStringLiteral(" (") % QString::number(metresFromGoalLine) % QStringLiteral(" m)") : QString();
    const QString scrumAwardedToText = QStringLiteral("Scrum awarded to: ") % this->_teamInPossession->name() % scrumInOwn22;
    if (this->displayOn(MW))
        _mw->logRecord(scrumAwardedToText);

    // is ball thrown straight into the scrum?
    const bool thrownInStraight = RandomValue::generateRandomBool(
        this->_settings->matchActivities().probability(MatchActionSubtype::SCRUM_BALL_THROWN_STRAIGHT));

    // if not thrown straight, free kick is awarded to the opponent
    if (!thrownInStraight) {

        this->changeBallPossession(_teamInPossession);
        this->changePlayerInPossessionToSpecialist(player::PreferredForAction::KICK_OFF);

        if (this->displayOn(MW)) {

            _mw->logRecord(QStringLiteral("Ball was not thrown straight into the scrum."));
            _mw->updatePlayer(_playerInPossession->fullName(), this->whoIsInPossession().first);
        }

        return (timeForGameAction.SCRUM / 2);
    }

    // what is the result of the scrum?
    MatchScore::Scrums scrumResultForTeamInPossession = MatchScore::Scrums::UNDETERMINED;
    bool infringementByTeamInPossession = false;

    int16_t teamPackWeight = this->_match->team(team)->packWeight();
    int16_t opponentPackWeight = this->_match->team(opponent)->packWeight();
    if (teamPackWeight == 0)
        teamPackWeight = static_cast<uint16_t>(std::round(opponentPackWeight * coefficients.UNKNOWN_WEIGHT));
    if (opponentPackWeight == 0)
        opponentPackWeight = static_cast<uint16_t>(std::round(teamPackWeight * coefficients.UNKNOWN_WEIGHT));

    const int16_t packWeightsDiff = teamPackWeight - opponentPackWeight;
    const int8_t compensationCoeff = (packWeightsDiff == std::abs(packWeightsDiff))
        ? std::min(packWeightsDiff/10, 5) : std::max(packWeightsDiff/10, -5);
    const uint16_t scrum = RandomValue::generateRandomInt<uint16_t>(6-compensationCoeff, 120);

    const MatchActionSubtype::MatchActivityType scrumResult =
        _settings->matchActivities().action(scrum, MatchActionType::MatchActivityBaseType::SCRUM);
    QString infringementDescription = MatchScore::unknownValue;

    switch (scrumResult) {

        case MatchActionSubtype::SCRUM_WON: scrumResultForTeamInPossession = MatchScore::Scrums::WON; break;
        case MatchActionSubtype::SCRUM_LOST: scrumResultForTeamInPossession = MatchScore::Scrums::LOST; break;
        case MatchActionSubtype::SCRUM_INTENTIONALLY_COLLAPSED: // fall through
        case MatchActionSubtype::SCRUM_NOT_BINDING_PROPERLY: // fall through
        case MatchActionSubtype::SCRUM_NOT_PUSHING_STRAIGHT: {

            infringementDescription.clear(); // null value means an infringements has occurred
            infringementByTeamInPossession = RandomValue::generateRandomBool(25);
            scrumResultForTeamInPossession = (infringementByTeamInPossession)
                                           ? MatchScore::Scrums::LOST : MatchScore::Scrums::WON;
            break;
        }
        case MatchActionSubtype::SCRUM_COLLAPSED: // fall through (new scrum follows)
        default: break;
    }

    // update team-in-possession's statistics
    this->_match->score(team)->scrumThrown(scrumResultForTeamInPossession);

    // team that threw in the ball has won
    if (scrumResultForTeamInPossession == MatchScore::Scrums::WON) {

        // update opponent's statistics
        this->_match->score(opponent)->scrumThrown(MatchScore::Scrums::LOST);

        if (this->displayOn(MW)) {

            const QString newValueScrumsWon = QString::number(this->_match->score(team)->scrums(scrumResultForTeamInPossession));
            _mw->updateStatisticsUI(team, QStringLiteral("ScrumsWonLabel"), newValueScrumsWon);

            const QString newValueScrumsLost = QString::number(this->_match->score(opponent)->scrums(MatchScore::Scrums::LOST));
            _mw->updateStatisticsUI(opponent, QStringLiteral("ScrumsLostLabel"), newValueScrumsLost);

            // display scrum result in log
            const QString scrumResultText = QStringLiteral("Scrum won by: ") + this->_match->team(team)->name();
            _mw->logRecord(scrumResultText);
        }
    }

    // team that didn't throw in the ball has won
    if (scrumResultForTeamInPossession == MatchScore::Scrums::LOST) {

        // update opponent's statistics
        this->_match->score(opponent)->scrumThrown(MatchScore::Scrums::WON);

        if (this->displayOn(MW)) {

            const QString newValueScrumsWon = QString::number(this->_match->score(opponent)->scrums(MatchScore::Scrums::WON));
            _mw->updateStatisticsUI(opponent, QStringLiteral("ScrumsWonLabel"), newValueScrumsWon);

            const QString newValueScrumsLost = QString::number(this->_match->score(team)->scrums(scrumResultForTeamInPossession));
            _mw->updateStatisticsUI(team, QStringLiteral("ScrumsLostLabel"), newValueScrumsLost);

            // display scrum result in log (but not in case of an infringement)
            if (infringementDescription == MatchScore::unknownValue) {

                const QString scrumResultText = QStringLiteral("Scrum won by: ") + this->_match->team(opponent)->name();
                _mw->logRecord(scrumResultText);
            }
        }
    }

    // subsequent action if an infringement has occurred
    switch (scrumResult) {

        case MatchActionSubtype::SCRUM_COLLAPSED: {

            if (this->displayOn(MW))
                _mw->logRecord(message.display(this->scrumInfringement(), QStringLiteral("scrumCollapsed")));

            this->refreshTime(timeForGameAction.SCRUM/2, false);
            const timePassed timePassed = this->scrum();
            return timePassed;
        }
        case MatchActionSubtype::SCRUM_INTENTIONALLY_COLLAPSED:
            infringementDescription = QStringLiteral("scrumIntentionallyCollapsed"); // fall through
        case MatchActionSubtype::SCRUM_NOT_BINDING_PROPERLY:
            if (infringementDescription.isNull())
                infringementDescription = QStringLiteral("notBindedProperly"); // fall through
        case MatchActionSubtype::SCRUM_NOT_PUSHING_STRAIGHT: {

            if (infringementDescription.isNull())
                infringementDescription = QStringLiteral("notPushingStraight");

            const QStringList teams = (!infringementByTeamInPossession)
                ? QStringList { this->_match->team(opponent)->name(), this->_match->team(team)->name() }
                : QStringList { this->_match->team(team)->name(), this->_match->team(opponent)->name() };

            if (this->displayOn(MW))
               _mw->logRecord(message.displayWithReplace(this->scrumInfringement(), infringementDescription, teams));

            if (infringementByTeamInPossession)
                this->changeBallPossession(this->_teamInPossession);

            // add time spent in scrum and process penalty
            this->refreshTime(timeForGameAction.SCRUM, false);
            const timePassed timePassed = this->penalty();
            return (timePassed);
        }
        default: ;
    }

    if (scrumResultForTeamInPossession == MatchScore::Scrums::LOST)
        this->changeBallPossession(_teamInPossession);
    // scrum-half picks up the ball after one of the teams wins possession
    this->changePlayerInPossessionToSpecialist(player::PreferredForAction::SCRUM);

    return timeForGameAction.SCRUM;
}

timePassed GamePlay::lineOutIsThrowed() {

    this->resetPhases();

    // line-out can't be executed from within less than 5m distance of the goal-line
    this->moveBallToSpecificPositionOnPitch(groundDimensions.fromGoalLineTo5metreLine);

    const MatchType::Location team = this->whoIsInPossession().first;

    this->changePlayerInPossessionToSpecialist(player::PreferredForAction::LINEOUT);

    if (this->displayOn(MW))
        _mw->updatePlayer(_playerInPossession->fullName(), this->whoIsInPossession().first);

    // is lineout thrown straight (and goes at least 5 m)?
    const bool straight = RandomValue::generateRandomBool(
        this->_settings->matchActivities().probability(MatchActionSubtype::LINEOUT_STRAIGHT));

    // if lineout is not thrown straight (or touches the ground less than 5m from the touch-line),
    // an option of another lineout (just once) or a scrum is awarded to the team that threw in the ball - TO DO
    if (!straight) {

        this->refreshTime(timeForGameAction.LINEOUT, false);
        const timePassed timePassed = this->scrum();
        return timePassed;
    }

    // who has won the lineout?
    const uint8_t probability = this->probability(MatchActionSubtype::LINEOUT_WON);
    const MatchScore::Lineouts lineoutWon = static_cast<MatchScore::Lineouts>(RandomValue::generateRandomBool(probability));
    this->_match->score(team)->lineoutThrown(lineoutWon);

    // update lineouts' thrown-in-total stats
    const QString newValueLineoutsThrown = QString::number(this->_match->score(team)->lineouts(MatchScore::Lineouts::THROWN));
    // update lineouts' won/lost stats
    const QString newValueLineoutsWonOrLost = QString::number(this->_match->score(team)->lineouts(lineoutWon));

    if (this->displayOn(MW)) {

        _mw->updateStatisticsUI(team, QStringLiteral("LineoutsThrownLabel"), newValueLineoutsThrown);
        _mw->updateStatisticsUI(team, QStringLiteral("LineoutsSuccessRateLabel"), this->_match->score(team)->lineoutsSuccessRate());

        // display lineout result in log
        QString lineoutThrownText = QStringLiteral("Lineout awarded to: ") % this->_teamInPossession->name() % QStringLiteral(" (won)");
        if (lineoutWon == MatchScore::Lineouts::LOST)
            lineoutThrownText.replace("won", "lost");

        _mw->logRecord(lineoutThrownText);
    }

    // team that threw in the ball has won
    if (lineoutWon == MatchScore::Lineouts::WON) {

        if (this->displayOn(MW))
            _mw->updateStatisticsUI(team, QStringLiteral("LineoutsWonLabel"), newValueLineoutsWonOrLost);

        // is maul formed? - TO DO
    }

    // team that didn't throw in the ball has won
    if (lineoutWon == MatchScore::Lineouts::LOST) {

        // team that threw in has lost = team that didn't throw in has stolen the lineout
        if (this->displayOn(MW)) {

            const MatchType::Location opponent = this->whoIsInPossession().second;
            _mw->updateStatisticsUI(opponent, QStringLiteral("LineoutsStolenLabel"), newValueLineoutsWonOrLost);
        }
    }

    if (lineoutWon == MatchScore::Lineouts::LOST)
        this->changeBallPossession(_teamInPossession);
    this->changePlayerInPossession();

    return timeForGameAction.LINEOUT;
}

// returns true if tackling player has been penalized (either sin-binned or sent off)
bool GamePlay::dangerousTackle(Player * const tacklingPlayer, const player::Tackles type_of_tackle) {

    bool penalized = false;
    const MatchType::Location opponent = this->whoIsInPossession().second;

    // is player sin-binned or sent-off (or possibly warned only)?
    const uint8_t probabilityFrom = PlayerCondition::minValue + PlayerCondition::maxValue -
                                    tacklingPlayer->condition(player::Conditions::MORALE);
    const uint8_t punishment = RandomValue::generateRandomInt<uint8_t>(probabilityFrom, 100);
    MatchActionSubtype::MatchActivityType punishmentType =
        _settings->matchActivities().action(punishment, MatchActionType::MatchActivityBaseType::FOUL_PLAY);

    // in case of second yellow card it's a sent-off
    const uint8_t numberOfYellowCardsForPlayer =
        this->_match->playerStats(opponent, tacklingPlayer)->getStatsValue(StatsType::NumberOf::YELLOW_CARDS);
    if (punishmentType == MatchActionSubtype::SIN_BINNED && numberOfYellowCardsForPlayer == 1)
        punishmentType = MatchActionSubtype::SENT_OFF;

    uint8_t numberOfCardsForTeam = 0;
    QString messageBoxKey = QString();
    QString label = QString();

    switch (punishmentType) {

        // yellow card: sin-binned for 10 minutes
        case MatchActionSubtype::SIN_BINNED: {

            // update yellow cards count
            numberOfCardsForTeam = this->_match->score(opponent)->yellowCards();
            tacklingPlayer->stats()->incrementStatsValue(StatsType::NumberOf::YELLOW_CARDS);
            this->_match->playerStats(opponent, tacklingPlayer)->incrementStatsValue(StatsType::NumberOf::YELLOW_CARDS);

            this->_match->addSuspension(tacklingPlayer, tacklingPlayer->noOnPitch(), opponent, punishmentType);
            tacklingPlayer->withdrawPlayer();
            penalized = true;

            if (this->displayOn(MW)) {

                // display in log
                messageBoxKey = QStringLiteral("playerSinBinned");
                const QString playerSinBinnedMessage = message.displayWithReplace(this->objectName(), messageBoxKey,
                    { tacklingPlayer->fullName(), this->_match->team(opponent)->name(), QString::number(::penalty.Minutes) });
                _mw->logRecord(playerSinBinnedMessage);

                label = QStringLiteral("YellowCardsLabel");
            }
            break;
        }
        // red card: sent-off
        case MatchActionSubtype::SENT_OFF: {

            // update red cards count
            numberOfCardsForTeam = this->_match->score(opponent)->redCards();
            tacklingPlayer->stats()->incrementStatsValue(StatsType::NumberOf::RED_CARDS);
            this->_match->playerStats(opponent, tacklingPlayer)->incrementStatsValue(StatsType::NumberOf::RED_CARDS);

            this->_match->addSuspension(tacklingPlayer, tacklingPlayer->noOnPitch(), opponent, punishmentType);
            tacklingPlayer->withdrawPlayer();
            penalized =  true;
            tacklingPlayer->sentOff();

            if (this->displayOn(MW)) {

                // display in log
                messageBoxKey = QStringLiteral("playerSentOff");
                const QString playerSentOffMessage = message.displayWithReplace(this->objectName(), messageBoxKey,
                    { tacklingPlayer->fullName(), this->_match->team(opponent)->name() });
                _mw->logRecord(playerSentOffMessage);

                label = QStringLiteral("RedCardsLabel");
            }
            break;
        }
        case MatchActionSubtype::WARNING: // fall through
        default: break;
    }

    if (this->displayOn(MW) && !label.isNull()) {

        _mw->updateStatisticsUI(opponent, label, QString::number(numberOfCardsForTeam));
        this->refreshPointsList(opponent);

        _mw->updatePackWeight();

        // show information message
        const uint8_t numberOfPlayersOnPitch = this->_match->team(opponent)->numberOfPlayersOnPitch();
        const QString suspensionReasonMessage = ::message.display(this->dangerousTackle(),
                                                player::dangerousPlayReasonDescription[type_of_tackle]);

        _mw->timeStoppedMessageBox(messageBoxKey, { tacklingPlayer->fullName(), this->_match->team(opponent)->name(),
            QString::number(::penalty.Minutes), QString::number(numberOfPlayersOnPitch), this->_match->referee()->referee(),
            suspensionReasonMessage });
    }

    return penalized;
}

bool GamePlay::playerInjured(Player * const player, bool injured) const {

    if (!injured) {

        const uint8_t probabilityOfInjury = 50 + (player->attribute(player::Attributes::AGILITY) * 2 +
                                                  player->attribute(player::Attributes::DEXTERITY) * 5 +
                                                  player->attribute(player::Attributes::TACKLING) * 3) / 2;
        injured = RandomValue::generateRandomBool(100 - probabilityOfInjury);
    }

    if (injured) {

        // add new record to PlayerHealth
        player->condition()->newHealthIssue(this->_dateTime.systemDate(), player::HealthStatus::INJURY,
                                                                          player::HealthStatus::SERIOUS_INJURY);
        if (this->displayOn(MW)) {

            const QString reasonOfAbsence = player->availability(player::Conditions::AVAILABILITY, this->_dateTime.systemDate());
            const QString team = _myTeam->teamName(player);

            // display message box
            const QString dialogTextKey = (_myTeam->squad().contains(player))
                ? QStringLiteral("playerMustBeReplaced") : QStringLiteral("opponentPlayerInjured");
            const QString dialogText = message.displayWithReplace(this->playerSubstitution(),
                dialogTextKey, { player->fullName(), QString::number(player->shirtNo()), reasonOfAbsence });
            const QString dialogTitle = QStringLiteral("Health report (") % team % QStringLiteral(")");

            QMessageBox::critical(nullptr, dialogTitle, dialogText);

            // display in log
            const QString logMessage = player->fullName() % string_functions.wrapInBrackets(team) %
                                       QStringLiteral("has been ") % reasonOfAbsence % QStringLiteral(".");
            _mw->logRecord(logMessage);
        }
    }

    return injured;
}

// return value: tackle is either completed or missed
MatchScore::Tackles GamePlay::playerIsTackled() {

    MatchType::Location team = this->whoIsInPossession().second;

    // is tackle completed (successful) or missed?
    const double probabilityRaw = this->_match->playersOnPitchRatio(team) * // adjusted by players-on-pitch ratio
                                  this->probability(MatchActionSubtype::RUN_TACKLE_COMPLETED, true);
    const uint8_t probability = std::min(static_cast<uint8_t>(std::round(probabilityRaw)), static_cast<uint8_t>(100));

    const MatchScore::Tackles tackleCompleted = static_cast<MatchScore::Tackles>(RandomValue::generateRandomBool(probability));
    this->_match->score(team)->tackleAttempted(tackleCompleted);

    if (this->displayOn(MW)) {

        // update tackles' (either completed or missed) stats (team not in possession)
        const QString tacklesLabelName = (tackleCompleted == MatchScore::Tackles::COMPLETED) ?
            QStringLiteral("TacklesCompletedLabel") : QStringLiteral("TacklesMissedLabel");
        const QString newValueTackles = QString::number(this->_match->score(team)->tackles(tackleCompleted));
        _mw->updateStatisticsUI(team, tacklesLabelName, newValueTackles);

        // update tackles' (made in total) stats
        const QString newValueTacklesMade = QString::number(this->_match->score(team)->tackles(MatchScore::Tackles::ATTEMPTED));
        _mw->updateStatisticsUI(team, QStringLiteral("TacklesMadeLabel"), newValueTacklesMade);

        // update tackles' success rate stats
        _mw->updateStatisticsUI(team, QStringLiteral("TacklesSuccessRateLabel"), this->_match->score(team)->tacklesSuccessRate());
    }

    return tackleCompleted;
}

// return value: returns true if goal line has been crossed, false otherwise
bool GamePlay::playerIsRunning(uint8_t & metresMade) {

    const MatchType::Location team = this->whoIsInPossession().first;

    // update metres-made-by-running stats (subtract metres made after goal line was crossed)
    metresMade = static_cast<uint8_t>(static_cast<int8_t>(metresMade) + std::min<int8_t>(0, this->distanceToGoalLine()));
    _playerInPossession->stats()->addMetresRun(metresMade);
    const uint16_t newValue = this->_match->score(team)->run(metresMade);

    if (this->displayOn(MW))
        _mw->updateStatisticsUI(team, QStringLiteral("MetresMadeByRunningLabel"), QString::number(newValue));

    // if goal line has been crossed (possibly a try)
    if (this->distanceToGoalLine() < 0)
        return true;

    return false;
}

void GamePlay::playerIsKicking(const uint8_t metresMade) {

    const MatchType::Location team = this->whoIsInPossession().first;

    // update metres-made-by-kicking stats
    _playerInPossession->stats()->addMetresKicked(metresMade);
    const uint16_t newValue = this->_match->score(team)->kick(metresMade);

    if (this->displayOn(MW))
        _mw->updateStatisticsUI(team, QStringLiteral("MetresMadeByKickingLabel"), QString::number(newValue));

    return;
}

// true = player injured because of low fatigue
bool GamePlay::changeInFatigue(Player * const player) const {

    const uint8_t probabilityOfDecrease = 72 - player->attribute(player::Attributes::ENDURANCE) * 2;
    const bool fatigueDecrease = RandomValue::generateRandomBool(probabilityOfDecrease);

    if (!fatigueDecrease)
        return false;

    // if fatigue is at minimum level already, don't decrease it's value; declare player injured instead
    if (player->condition(player::Conditions::FATIGUE) == PlayerCondition::minValue) {

        const bool substitution = this->playerInjured(player, true);

        // compulsary replacement (substitution is always true)
        return substitution;
    }

    player->condition()->decreaseCondition(player::Conditions::FATIGUE, static_cast<uint8_t>(fatigueDecrease));

    ConditionThresholds::ConditionValue severity = ConditionThresholds::ConditionValue::NORMAL;
    const uint8_t fatigueNewValue = player->condition(player::Conditions::FATIGUE, severity);

    if (severity == ConditionThresholds::ConditionValue::CRITICAL) {

        // other teams' players are replaced whenever this situation arises
        QMessageBox::StandardButton substitution = QMessageBox::StandardButton::Ok;

        if (this->displayOn(MW) && _myTeam->squad().contains(player)) {

            const QString dialogText = message.displayWithReplace(this->playerSubstitution(), "playerShouldBeReplaced",
                { player->fullName(), QString::number(player->shirtNo()), QString::number(fatigueNewValue) } );
            substitution = QMessageBox::warning(nullptr, QStringLiteral("Player's fatigue very low."), dialogText);
        }

        // voluntary replacement
        return (substitution == QMessageBox::StandardButton::Ok);
    }

    return false;
}

void GamePlay::changeInMorale(Team * const team, const bool increase, const uint8_t number) const {

    for (auto player: team->squad()) {

        if (!player->isOnPitch())
            continue;

        this->changeInMorale(player, increase, number);
    }
    return;
}

bool GamePlay::changeInMorale(Player * const player, const bool increase, const uint8_t number) const {

    if (RandomValue::generateRandomBool(25)) {

        PlayerCondition * const pc = player->condition();
        pc->changeCondition = (increase) ? &PlayerCondition::increaseCondition : &PlayerCondition::decreaseCondition;

        (pc->*(pc->changeCondition))(player::Conditions::MORALE, number);

        return true;
    }
    return false;
}

void GamePlay::suspensionsUpdate(const uint8_t minutes) {

    // deduct time of suspension
    QVector<QPair<Player *, uint8_t>> playersBackOnPitch;
    uint8_t listsToRefresh = 0;
    this->_match->deductSuspensionMinutesRemaining(playersBackOnPitch, listsToRefresh, minutes);

    // players returning after suspension
    for (auto player: playersBackOnPitch) {

        // original position is assigned back to player going out of sin-bin
        player.first->introducePlayer(player.second);

        if (this->displayOn(MW)) {

            // display in log
            const QString playerBackFromSinBinMessage =
                player.first->fullName() + QStringLiteral(" is back on pitch (after suspension).");
            _mw->logRecord(playerBackFromSinBinMessage);

            // show information message
            _mw->timeStoppedMessageBox("playerBackFromSinBin", { player.first->fullName() } );
        }
    }

    // update points' lists
    if (this->displayOn(MW)) {

        if ((listsToRefresh & 1) == 1)
            this->refreshPointsList(MatchType::Location::HOSTS);
        if ((listsToRefresh & 2) == 2)
            this->refreshPointsList(MatchType::Location::VISITORS);

        _mw->updatePackWeight();
    }

    return;
}

// returns true if playerPossiblyGoingIn can be used as substitution player
bool GamePlay::isEligibleForSubstitution(Player * const playerGoingOut, Player * const playerPossiblyGoingIn,
                                         const MatchType::Location loc, const bool injuryReplacement) const {

    const PlayerPosition_index_item::PositionBaseType baseType =
        playerPosition_index.findPositionBaseTypeByType(playerGoingOut->position()->positionType());

    return // an already (tactically) replaced player is allowed to return to play when replacing injured player [a)]
        playerPosition_index.findPositionBaseTypeByType(playerPossiblyGoingIn->position()->positionType()) == baseType &&
        playerPossiblyGoingIn->isOnBench() && (this->_match->playerStats(loc, playerPossiblyGoingIn) == nullptr ||
        this->_match->playerStats(loc, playerPossiblyGoingIn)->noMatchesPlayed() || injuryReplacement /* a) */) &&
        playerPossiblyGoingIn->isHealthy(); // a player may have been injured earlier during current match so
        // although he's on the bench (_noOnPitch == 0) such a player is not available for selection
        // (not even for another injured player which would be otherwise possible)
}

void GamePlay::switchPlayers(Player * const playerOut, Player * const playerIn, const MatchType::Location loc) {

    // player going in
    playerIn->introducePlayer(playerOut->noOnPitch());
    playerIn->position()->assignNewPlayerPosition(playerOut->position()->playerPosition());
    this->_match->addNewStatsRecordForPlayer(loc, playerIn);

    // transfer preferences (except captaincy)
    if (this->displayOn(FW) || this->_match->team(loc) != _myTeam || this->_settings->substitutionRules().transferPreferences())
        playerOut->transferPreferences(playerIn, player::PreferredForAction::KICK_OFF, player::PreferredForAction::CONVERSION);

    this->updateStatistics(loc, StatsType::NumberOf::GAMES_PLAYED, playerIn);
    this->updateStatistics(loc, StatsType::NumberOf::GAMES_PLAYED_SUB, playerIn);

    if (playerIn->position()->positionType() != playerOut->position()->positionType())
        playerIn->condition()->decreaseCondition(player::Conditions::FORM,
        static_cast<uint8_t>(RandomValue::generateRandomBool(50)));

    // player going out
    playerOut->withdrawPlayer();

    if (this->displayOn(MW)) {

        this->refreshPointsList(loc);
        _mw->updatePackWeight();

        // display in log
        const QString logMessage = message.displayWithReplace(this->playerSubstitution(),
            "playerReplacedByAnotherPlayer", { this->_match->team(loc)->name(), playerIn->fullName(),
            QString::number(playerIn->shirtNo()), playerOut->fullName(), QString::number(playerOut->shirtNo()) } );
        _mw->logRecord(logMessage);
    }

    return;
}

timePassed GamePlay::regularSubstitutions() {

    timePassed timePassed = 0;

    for (uint8_t i = 0; i < 2; ++i) {

        const MatchType::Location loc = static_cast<MatchType::Location>(i);

        // my team (matchwidget) - called in regular intervals or after request
        if (this->displayOn(MW) && _myTeam == this->_match->team(loc) &&
            ((this->_settings->substitutionRules().replacementInterval() != 0 && // replacement interval from settings
             (this->_match->timePlayed().minutesPlayed() % this->_settings->substitutionRules().replacementInterval()) == 0) ||
             _mw->resumePlay() == MatchWidget::ResumePlay::SUBSTITUTION)) {

            timePassed = this->substitution(_myTeam);
            _mw->nextAction(MatchWidget::ResumePlay::NO_ACTION);
        }

        // opponent (matchwidget) or either team (fixtureswidget) - called in regular intervals
        if (((this->displayOn(MW) && _myTeam != this->_match->team(loc)) || this->displayOn(FW)) &&
            timeForGameAction.REPLACEMENT_INTERVAL != 0 && // default replacement interval
            (this->_match->timePlayed().minutesPlayed() % timeForGameAction.REPLACEMENT_INTERVAL) == 0) {

            timePassed = this->substitution(this->_match->team(loc));
        }
    }

    return timePassed;
}

timePassed GamePlay::substitution(Team * const team, Player * const injuredPlayer) {

    // if injuredPlayer == nullptr => regular substitution; otherwise => forced substitution (e.g. after injury)
    timePassed timePassed = 0;
    bool makeSubstitution = true;

    const MatchType::Location loc = (team == this->_match->team(MatchType::Location::HOSTS)) ? MatchType::Location::HOSTS : MatchType::Location::VISITORS;

    if (this->displayOn(MW) && team == _myTeam && (!this->_settings->substitutionRules().automaticSubstitutions() ||
                                                   _mw->resumePlay() == MatchWidget::ResumePlay::SUBSTITUTION)) {
        QMap<QString, Player *> playersOnPitch;

        for (auto player: team->squad()) {

            if (player->isOnPitch() && player->isHealthy())
                playersOnPitch.insert(_mw->playerForSubstitution(player), player);
        }

        QStringList playersOnPitchList = playersOnPitch.keys();

        do {

            const QString oldPlayer = (injuredPlayer == nullptr) ?
                InputDialog::getItem(nullptr, QStringLiteral("Substitution (phase 1)"),
                QStringLiteral("Select player for substitution (out):"), playersOnPitchList,
                &makeSubstitution, 400, QStringLiteral("transfer.png")) : QString();

            if (makeSubstitution) {

                Player * const playerOut = (injuredPlayer == nullptr) ? playersOnPitch[oldPlayer] : injuredPlayer;

                QMap<QString, Player *> playersOnBench;

                for (auto player: this->_match->team(loc)->squad())
                    if (this->isEligibleForSubstitution(playerOut, player, loc, injuredPlayer != nullptr))
                        playersOnBench.insert(_mw->playerForSubstitution(player), player);

                if (playersOnBench.isEmpty()) {

                    const QString dialogText = message.displayWithReplace(this->playerSubstitution(),
                        "noPlayerForReplacement", { playerOut->position()->playerPosition()->positionTypeName(),
                        playerOut->fullName(), QString::number(playerOut->shirtNo()) });
                    QMessageBox::information(nullptr, QStringLiteral("Substitution"), dialogText);

                    // if there's no suitable player to replace an injured player the injured player must be withdrawn anyway
                    if (injuredPlayer != nullptr) {

                        // player going out
                        playerOut->withdrawPlayer();
                        if (this->displayOn(MW))
                            _mw->updatePackWeight();
                        break;
                    }
                    continue;
                }

                const QStringList playersOnBenchList = playersOnBench.keys();

                const QString newPlayer = InputDialog::getItem(nullptr, QStringLiteral("Substitution (phase 2)"),
                                          QStringLiteral("Select player for substitution (in):"), playersOnBenchList,
                                          &makeSubstitution, 400, QStringLiteral("transfer.png"));
                if (makeSubstitution) {

                    Player * const playerIn = playersOnBench[newPlayer];

                    // add new record to list of substitutions
                    if (this->_match->addSubstitution(playerOut, playerIn, loc) &&
                        !this->_periods->isInterval(this->_match->currentPeriod()))
                        // update time if first sub at given minute (and current game period hasn't finished yet)
                        timePassed = timeForGameAction.REPLACEMENT;

                    this->switchPlayers(playerOut, playerIn, loc);

                    // remove player from list of possible players for substitution
                    playersOnPitchList.removeOne(oldPlayer);
                    playersOnPitch.remove(oldPlayer);
                }
            }
        } while (makeSubstitution == true && injuredPlayer == nullptr);
    }

    /* make substitutions in the background if
     * a] game is played in FW mode (from FixturesWidget)
     * b] game is played in MW mode (from MatchWidget); opponent's team makes substitution
     * c] game is played in MW mode (from MatchWidet); my team makes substitution and
     *    1. automatic substitutions are switched on
     *    2. it's not a substitution on request */
    if ((this->displayOn(MW) && (team != _myTeam || (this->_settings->substitutionRules().automaticSubstitutions() &&
         _mw->resumePlay() != MatchWidget::ResumePlay::SUBSTITUTION))) || this->displayOn(FW)) {

        uint8_t noOfReplacedPlayers = 0;

        for (auto oldPlayer: team->squad()) {

            if (injuredPlayer != nullptr)
                oldPlayer = injuredPlayer;

            // exclude players just introduced (during previous run of the loop) [a)] and
            // exclude players who are less than X minutes on the pitch [b)]
            if (oldPlayer->isOnPitch() && (oldPlayer == injuredPlayer || (oldPlayer->isHealthy() &&
                this->_match->playerStats(loc, oldPlayer)->getStatsValue(StatsType::NumberOf::MINS_PLAYED) >=
                std::max(/* a) */static_cast<uint8_t>(1), /* b) */ timeForGameAction.MIN_PLAY_INTERVAL)))) {

                QVector<Player *> playersForSubstitution;

                for (auto player: team->squad())
                    if (this->isEligibleForSubstitution(oldPlayer, player, loc, injuredPlayer != nullptr))
                        playersForSubstitution.push_back(player);

                auto newPlayerIt = std::max_element(playersForSubstitution.begin(), playersForSubstitution.end(),
                    [](Player * const & first, Player * const & second) -> bool
                    { return first->condition(player::Conditions::OVERALL) < second->condition(player::Conditions::OVERALL); });

                // change players (if a suitable player for substitution has been found); suitable player is
                // in case of regular substitution: player with higher overall condition; in case of injury: any player
                if (newPlayerIt != playersForSubstitution.end() && (injuredPlayer != nullptr ||
                    (*newPlayerIt)->condition(player::Conditions::OVERALL) > oldPlayer->condition(player::Conditions::OVERALL) * 1.05)) {

                    // add new record to list of substitutions
                    if (this->_match->addSubstitution(oldPlayer, *newPlayerIt, loc) &&
                        !this->_periods->isInterval(this->_match->currentPeriod()))
                        // update time if first sub at given minute (and current game period hasn't finished yet)
                        timePassed = timeForGameAction.REPLACEMENT;

                    this->switchPlayers(oldPlayer, *newPlayerIt, loc);
                    ++noOfReplacedPlayers;
                }

                // if there's no suitable player to replace an injured player the injured player must be withdrawn anyway
                if (newPlayerIt == playersForSubstitution.end() && injuredPlayer != nullptr) {

                    // player going out
                    oldPlayer->withdrawPlayer();
                    if (this->displayOn(MW))
                        _mw->updatePackWeight();
                }

            }
            if (injuredPlayer != nullptr || noOfReplacedPlayers == 3)
                break;
        }
    }

    return timePassed;
}

void GamePlay::kickingCompetition() {

    const uint8_t & noOfPlayers = numberOfPlayers.NoOfPlayersForKickingCompetition;
    uint8_t maxNumberOfGoals = noOfPlayers;
    std::array<uint8_t, 2> goalsInShootOut = {0,0};

    for (uint8_t team = 0; team < 2; ++team) {

        const MatchType::Location loc = static_cast<MatchType::Location>(team);
        QVector<uint8_t> bestKicking;

        for (auto player: this->_match->team(loc)->squad())
            if (player->isOnPitch())
                bestKicking.append(player->attribute(player::Attributes::KICKING));
        std::sort(bestKicking.begin(), bestKicking.end());

        const int bestKickingSize =  std::min(static_cast<int>(noOfPlayers), bestKicking.size());
        const uint8_t sumKicking = std::accumulate(bestKicking.crbegin(), bestKicking.crbegin() + bestKickingSize, 0);
        maxNumberOfGoals = static_cast<uint8_t>((sumKicking + 9.9) / 10);
        goalsInShootOut[team] = RandomValue::generateRandomInt<uint8_t>(0, maxNumberOfGoals);
    }

    while (goalsInShootOut.at(0) == goalsInShootOut.at(1))
        goalsInShootOut[1] = RandomValue::generateRandomInt<uint8_t>(0, maxNumberOfGoals);

    this->_match->score(MatchType::Location::HOSTS)->shootOutGoalsScored(goalsInShootOut.at(0));
    this->_match->score(MatchType::Location::VISITORS)->shootOutGoalsScored(goalsInShootOut.at(1));

    this->_match->timePlayed().addTime(this->_periods->length(this->_match->currentPeriod(), 60));

    return;
}

void GamePlay::playMatch() {

    this->_restartPlay = false;

    if (this->_match->currentPeriod() == MatchPeriod::TimePeriod::FULL_TIME)
        return;

    // draw is performed at match start
    if (this->_match->currentPeriod() == MatchPeriod::TimePeriod::WARM_UP) {

        this->startOfMatch();

        if (this->displayOn(MW))
            _mw->logRecord(this->_periods->description(this->_match->currentPeriod()));

        if (this->displayOn(FW)) {

            _fw->ui->currentMatchProgress->setVisible(true);

            ClickableLabel * label = _fw->findWidgetByCode<ClickableLabel *>(this->_match->code(), on::fixtureswidget.scoreSeparator);
            label->setText(QStringLiteral(" : "));
            label->repaint();

            for (uint8_t i = 0; i < 2; ++i) {

                QLabel * label = _fw->findWidgetByCode<QLabel *>(this->_match->code(), on::fixtureswidget.teamScore[i]);
                label->setText(QString::number(0));
                label->repaint();
            }
        }

        _teamInPossession = this->draw();
        this->changePlayerInPossessionToSpecialist(player::PreferredForAction::KICK_OFF);
    }

    bool endOfPeriod = false;
    bool isOffload = false;

    while ((_match->timePlayedInSeconds() < this->_periods->timePlayed(MatchPeriod::TimePeriod::BEFORE_EXTRA_TIME_INTERVAL, 60) &&
            _match->currentPeriod() < MatchPeriod::TimePeriod::BEFORE_EXTRA_TIME_INTERVAL) ||
           (_match->timePlayedInSeconds() < this->_periods->timePlayed(MatchPeriod::TimePeriod::BEFORE_SUDDEN_DEATH_TIME_INTERVAL, 60) &&
            _match->currentPeriod() < MatchPeriod::TimePeriod::BEFORE_SUDDEN_DEATH_TIME_INTERVAL) ||
           (_match->timePlayedInSeconds() < this->_periods->timePlayed(MatchPeriod::TimePeriod::BEFORE_KICKING_INTERVAL, 60) &&
            _match->currentPeriod() < MatchPeriod::TimePeriod::BEFORE_KICKING_INTERVAL)) {

        if (this->displayOn(MW))
            QThread::msleep(this->_gameplaySpeedDetailed);
        if (this->displayOn(FW))
            QThread::msleep(this->_gameplaySpeedStandard);

        // after match time is incremented (= refreshTime called): endOfPeriod (bool) value must be
        // assigned to in the following way in case current (to be replaced) value of this variable
        // is already true (after previous call to refreshTime) otherwise it would be overwritten
        // back to false: endOfPeriod |= refreshTime(numberOfSeconds);

        if (this->_periods->isInterval(this->_match->currentPeriod())) {

            // set length of interval period
            const uint16_t periodLength = this->_periods->length(this->_match->currentPeriod(), 60);
            this->_match->timePlayed().setTimeForInterval(periodLength);

            // refresh system time (= move time forward by number of minutes reserved for current interval)
            this->_dateTime.refreshSystemDateAndTime(periodLength, 60);

            // previous period can be longer than "booked" number of minutes (e.g. first half-time could take 40:52)
            // nevertheless subsequent period must always start at the designated time (e.g. 40:00)
            _match->timePlayed().resetTime(this->_periods->timePlayed(this->_match->currentPeriod()));

            this->_match->timePlayed().switchTimePeriodTo(/*Next*/);

            // determine what team (hosts/visitors) starts current period (with a kick-off)
            const MatchType::Location team = (this->_hostsFirstKickOff)
                                           ? MatchType::Location::HOSTS : MatchType::Location::VISITORS;
            _teamInPossession = _match->team(team);
            this->changePlayerInPossessionToSpecialist(player::PreferredForAction::KICK_OFF);
            this->_hostsFirstKickOff = !this->_hostsFirstKickOff;

            if (this->displayOn(MW)) {

                _mw->ui->timePlayedLabel->setText(_match->timePlayed().timePlayed());
                _mw->ui->matchProgressProgressBar->setValue(_match->timePlayedInSeconds());

                _mw->ui->updatePeriod(this->_periods->description(this->_match->currentPeriod()));
                _mw->logRecord(this->_periods->description(this->_match->currentPeriod()));
            }

            this->_restartPlay = true;
            this->resetPhases();
        }

        const uint16_t event = RandomValue::generateRandomInt<uint16_t>(1, 70);
        MatchActionType::MatchActivityBaseType action =
            (this->_restartPlay) ? MatchActionType::KICKING : _settings->matchActivities().action(event);

        // kick-off (at start of each match period) or restart kick (after a score)
        if (this->_restartPlay) {

            // ball is taken back to the half-way line
            this->moveBallToSpecificPositionOnPitch();
            if (this->displayOn(MW))
                _mw->logRecord(QStringLiteral("Kick-off: ") + _teamInPossession->name());
        }

        // player is running (with the ball)
        if (action == MatchActionType::RUNNING) {

            // if no action occurs during running which forces change in PlayerInPossession (tackle, try, etc.)
            // then the entire loop continues and subsequest action (for the same player) follows
            // note: this situation is not detected by DiagnosticMode because this responds only to player changes

            // probability of current player (who carries the ball) being tackled
            const uint8_t probabilityOfTackle = this->probability(MatchActionSubtype::RUN_PLAYER_TACKLED, true);
            const bool opponentTackles = RandomValue::generateRandomBool(probabilityOfTackle);

            uint8_t maxDistance;
            if (opponentTackles) {

                // the closer the goal line, the greater probability that player would make less distance before tackle
                maxDistance = (this->distanceToGoalLine() >= groundDimensions.fromGoalLineTo5metreLine)
                            ? groundDimensions.fromGoalLineTo5metreLine : (this->distanceToGoalLine() + 1);
            }
            else {
                // player with higher speed can cover more distance
                maxDistance = _playerInPossession->attribute(player::Attributes::SPEED) * 2;
            }
            uint8_t metresMade = RandomValue::generateRandomInt<uint8_t>(static_cast<uint8_t>(!opponentTackles), maxDistance);

            // update carries' stats
            if (metresMade > 0 && _incrementCarries)
                this->ballCarried();

            // change ball position ...
            this->changeBallPositionOnPitch(metresMade);

            // ... and check if goal line has been crossed
            const bool goalLineCrossed = this->playerIsRunning(metresMade);

            endOfPeriod |= this->refreshTime(metresMade);

            if (goalLineCrossed) {

                // check if TMO should be consulted
                const uint8_t probabilityOfTMOReview = this->probability(MatchActionSubtype::RUN_OVER_GOAL_LINE_TRY_UNDER_REVIEW);
                bool tryAchieved = RandomValue::generateRandomBool(100-probabilityOfTMOReview);

                // TMO consulted
                if (!tryAchieved) {

                    if (this->displayOn(MW)) {

                        const QString tryUnderReviewText = message.displayWithReplace(this->objectName(), "tryUnderReview",
                                                           { _playerInPossession->fullName(), _teamInPossession->name() });
                        QMessageBox::warning(nullptr, QStringLiteral("TMO review (pending)"), tryUnderReviewText);
                    }

                    // check if try is valid
                    const uint8_t probabilityOfIllegalTry = this->probability(MatchActionSubtype::RUN_OVER_GOAL_LINE_TRY_ILLEGAL);

                    tryAchieved = RandomValue::generateRandomBool(100-probabilityOfIllegalTry);

                    if (!tryAchieved) {

                        if (this->displayOn(MW)) {

                            QString tryNotScoredText = message.displayWithReplace(this->objectName(),
                                                       "tryDeclaredIllegal", { _playerInPossession->fullName() });
                            QMessageBox::information(nullptr, QStringLiteral("TMO review"), tryNotScoredText);
                            _mw->logRecord(QStringLiteral("[TMO review] ") + tryNotScoredText.replace('\n',' '));
                        }
                        endOfPeriod |= this->refreshTime(timeForGameAction.ILLEGAL_TRY);

                        // play resumes with 22-metre drop-out
                        action = MatchActionType::KICKING;
                        this->moveBallToSpecificPositionOnPitch(groundDimensions.fromGoalLineTo22metreLine);
                        this->changeBallPossession(_teamInPossession);
                        this->changePlayerInPossessionToSpecialist(player::PreferredForAction::KICK_OFF);
                    }
                }

                // if try is legal
                if (tryAchieved) {

                    timePassed timePassed = 0; this->tryScored();
                    endOfPeriod |= this->refreshTime(timePassed);

                    // conversion attempt follows (except for sudden-death time)
                    if (!this->_periods->matchEndsWithResultChange(this->_match->timePlayed().lastPeriodPlayed())) {

                        timePassed = 0; this->conversionAttempt();
                        endOfPeriod |= this->refreshTime(timePassed);
                    }
                }
            }
            else {

                if (opponentTackles) {

                    this->updateStatistics(this->whoIsInPossession().first, StatsType::NumberOf::TACKLES_RECEIVED, _playerInPossession);

                    const uint8_t probabilityOfOffload = _settings->matchActivities().probability(MatchActionSubtype::TACKLE_OFFLOAD);
                    isOffload = RandomValue::generateRandomBool(probabilityOfOffload);

                    // player is either able to pass the ball (off-load) or is tackled by the opponent
                    action = (isOffload) ? MatchActionType::PASSING : MatchActionType::TACKLING;
                    endOfPeriod |= this->refreshTime(4);
                }
                else
                    ; // play continues with another action
            }
        }

        // player is being tackled
        if (action == MatchActionType::TACKLING) {

            Player * const tacklingPlayer = this->searchForOpponentsPlayer();
            this->updateStatistics(this->whoIsInPossession().second, StatsType::NumberOf::TACKLES_MADE, tacklingPlayer);

            if (this->playerIsTackled() == MatchScore::Tackles::COMPLETED) {

                this->updateStatistics(this->whoIsInPossession().second, StatsType::NumberOf::TACKLES_COMPLETED, tacklingPlayer);

                // if tackle has been completed then either ruck is formed or ball
                // is lost to opponent or play is stopped due to dangerous tackle
                const uint8_t tackle = RandomValue::generateRandomInt<uint8_t>(1, 100);
                const MatchActionSubtype::MatchActivityType nextAction = _settings->matchActivities().action(tackle, action);

                switch (nextAction) {

                    case MatchActionSubtype::TACKLE_RUCK_IS_FORMED: {

                        action = MatchActionType::RUCK;
                        endOfPeriod |= this->refreshTime(timeForGameAction.TACKLE);
                        break;
                    }
                    case MatchActionSubtype::TACKLE_PLAYER_PUSHED_INTO_OUT: {

                        this->changeBallPossession(_teamInPossession);
                        const timePassed timePassed = this->lineOutIsThrowed();
                        endOfPeriod |= this->refreshTime(timePassed);
                        break;
                    }
                    case MatchActionSubtype::TACKLE_BALL_LOST_TO_OPPONENT: {

                        this->changeBallPossession(_teamInPossession);
                        this->changePlayerInPossession();

                        endOfPeriod |= this->refreshTime(timeForGameAction.TACKLE);
                        break;
                    }
                    // tackling or attempting to tackle an opponent above the line of the shoulders
                    case MatchActionSubtype::TACKLE_HIGH_TACKLE: {

                        this->updateStatistics(this->whoIsInPossession().second, StatsType::NumberOf::HIGH_TACKLES, tacklingPlayer);
                        this->updateStatistics(this->whoIsInPossession().second, StatsType::NumberOf::DANGEROUS_TACKLES, tacklingPlayer);

                        if (this->dangerousTackle(tacklingPlayer, player::Tackles::HIGH_TACKLE)) {

                            endOfPeriod |= refreshTime(timeForGameAction.SUSPENSION);

                            // tackled player may be injured
                            const bool isPlayerInjured = this->playerInjured(_playerInPossession);
                            if (isPlayerInjured) // time is stopped <=> no time increase
                                /* const timePassed timePassed = */ this->substitution(_teamInPossession, _playerInPossession);
                        }

                        // a penalty follows ...
                        const timePassed timePassed = this->penalty();
                        endOfPeriod |= this->refreshTime(timePassed);
                        break;
                    }
                    /* - a player must not tackle an opponent early or late; an opponent who is
                       not in possession of the ball; an opponent whose feet are off the ground
                       - a player must not lift an opponent off the ground and drop that player
                       so that that player's head or torso makes contact with the ground */
                    case MatchActionSubtype::TACKLE_DANGEROUS_PLAY: {

                        this->updateStatistics(this->whoIsInPossession().second, StatsType::NumberOf::DANGEROUS_TACKLES, tacklingPlayer);

                        // check if TMO should be consulted
                        if (this->displayOn(MW)) {

                            const uint8_t probabilityOfTMOReview = this->probability(MatchActionSubtype::TACKLE_UNDER_REVIEW);

                            if (RandomValue::generateRandomBool(probabilityOfTMOReview)) {

                                const QString tackleUnderReviewText =
                                    message.displayWithReplace(this->dangerousTackle(), "tackleUnderReview",
                                    { tacklingPlayer->fullName(), this->_match->team(this->whoIsInPossession().second)->name() });
                                QMessageBox::warning(nullptr, QStringLiteral("TMO review (pending)"), tackleUnderReviewText);
                            }
                        }

                        const uint8_t probability = RandomValue::generateRandomInt(1,4);
                        player::Tackles dangerousTackle = static_cast<player::Tackles>(probability);

                        if (this->dangerousTackle(tacklingPlayer, dangerousTackle)) {

                            endOfPeriod |= refreshTime(timeForGameAction.SUSPENSION);

                            // tackled player may be injured
                            const bool isPlayerInjured = this->playerInjured(_playerInPossession);
                            if (isPlayerInjured) // time is stopped <=> no time increase
                                /* const timePassed timePassed = */ this->substitution(_teamInPossession, _playerInPossession);
                        }

                        // a penalty follows ...
                        const timePassed timePassed = this->penalty();
                        endOfPeriod |= this->refreshTime(timePassed);
                        break;
                    }
                    default: ;
                }
            }
        }

        // ruck is being formed
        if (action == MatchActionType::RUCK) {

            // if ruck is being formed then either another phase of play follows
            // or some kind of infringment occurs (offside, not releasing ball, etc.)
            const uint8_t ruck = RandomValue::generateRandomInt<uint8_t>(1, 50);
            const MatchActionSubtype::MatchActivityType nextAction = _settings->matchActivities().action(ruck, action);

            QString infringement = QString();

            switch (nextAction) {

                case MatchActionSubtype::RUCK_ANOTHER_PHASE: {

                    ++this->_noOfPhases;

                    if (this->displayOn(MW)) {

                        if (this->whoIsInPossession().first == MatchType::Location::HOSTS) {

                            _mw->ui->hostsNoOfPhasesLabel->setText(QStringLiteral("Phase: ") + QString::number(_noOfPhases));
                            if (_noOfPhases == 1) _mw->ui->hostsNoOfPhasesLabel->setVisible(true);
                            _mw->ui->hostsNoOfPhasesLabel->repaint();
                        }
                        else {

                            _mw->ui->visitorsNoOfPhasesLabel->setText(QStringLiteral("Phase: ") + QString::number(_noOfPhases));
                            if (_noOfPhases == 1) _mw->ui->visitorsNoOfPhasesLabel->setVisible(true);
                            _mw->ui->visitorsNoOfPhasesLabel->repaint();
                        }
                    }

                    this->changePlayerInPossession();

                    endOfPeriod |= this->refreshTime(timeForGameAction.RUCK_PHASE);
                    break;
                }
                case MatchActionSubtype::RUCK_NOT_RELEASING_BALL: {

                    this->updateStatistics(this->whoIsInPossession().first, StatsType::NumberOf::PENALTIES_CAUSED, _playerInPossession);

                    if (this->displayOn(MW)) {
                        _mw->logRecord(message.displayWithReplace(this->penaltyInfringement(),
                            QStringLiteral("notReleasingBall"), { _teamInPossession->name() }));
                    }

                    // team-in-possession's infringement
                    this->changeBallPossession(_teamInPossession);
                    const timePassed timePassed = this->penalty();
                    endOfPeriod |= this->refreshTime(timePassed);
                    break;
                }
                case MatchActionSubtype::RUCK_NOT_RELEASING_PLAYER:
                    if (infringement.isNull()) infringement = QStringLiteral("notReleasingPlayer"); // fall through
                case MatchActionSubtype::RUCK_OFFSIDE:
                    if (infringement.isNull()) infringement = QStringLiteral("offSide"); // fall through
                case MatchActionSubtype::RUCK_OFF_FEET:
                    if (infringement.isNull()) infringement = QStringLiteral("offFeet"); // fall through
                case MatchActionSubtype::RUCK_IN_AT_THE_SIDE: {

                    if (infringement.isNull())
                        infringement = QStringLiteral("inAtTheSide");

                    Player * const infringementByPlayer = this->searchForOpponentsPlayer();
                    this->updateStatistics(this->whoIsInPossession().second, StatsType::NumberOf::PENALTIES_CAUSED, infringementByPlayer);

                    if (this->displayOn(MW)) {
                        _mw->logRecord(message.displayWithReplace(this->penaltyInfringement(),
                            infringement, { this->_match->team(this->whoIsInPossession().second)->name() }));
                    }

                    const timePassed timePassed = this->penalty();
                    endOfPeriod |= this->refreshTime(timePassed);
                    break;
                }
                default: ;
            }
        }

        // player is passing (the ball)
        if (action == MatchActionType::PASSING) {

            this->updateStatistics(this->whoIsInPossession().first, StatsType::NumberOf::PASSES_MADE, _playerInPossession);

            // probability of pass being successful is lower if number of players of attacking team,
            // i.e. team in possession of the ball, is less than number of players of defending team
            const double playersRatio = this->_match->playersOnPitchRatio(this->whoIsInPossession().second);
            const uint8_t probabilityFrom = (playersRatio <= 1) ? 1 : static_cast<uint8_t>(std::round((playersRatio - 1) * 100));
            const uint8_t pass = RandomValue::generateRandomInt<uint8_t>(probabilityFrom, 100);

            MatchActionSubtype::MatchActivityType nextAction = _settings->matchActivities().action(pass, action);

            bool isDeliberate = true;
            bool isHandlingError = false;

            const QPair<Player * const, Team * const> passingPlayer =
                qMakePair<Player * const, Team * const>(_playerInPossession, _teamInPossession);

            endOfPeriod |= this->refreshTime(timeForGameAction.PASS);

            switch (nextAction) {

                case MatchActionSubtype::PASS_OK: {

                    this->updateStatistics(this->whoIsInPossession().first, StatsType::NumberOf::PASSES_COMPLETED, _playerInPossession);

                    if (isOffload) {

                        this->updateStatistics(this->whoIsInPossession().first, StatsType::NumberOf::OFFLOADS, _playerInPossession);

                        // update offloads' statistics
                        const MatchType::Location team = this->whoIsInPossession().first;
                        const uint8_t currentValueOffloads = this->_match->score(team)->offloads();

                        if (this->displayOn(MW))
                            _mw->updateStatisticsUI(team, QStringLiteral("OffloadsLabel"), QString::number(currentValueOffloads));
                    }

                    this->ballPassed(MatchScore::Passes::COMPLETED);
                    this->changePlayerInPossession();
                    break;
                }
                case MatchActionSubtype::PASS_MISSED: {

                    this->ballPassed(MatchScore::Passes::MISSED);

                    // missed pass can be picked up either by team which "is"/was in possession or by opponent
                    const bool opponentPicksUpBall = RandomValue::generateRandomBool(50);
                    if (opponentPicksUpBall) {

                        this->changeBallPossession(_teamInPossession);
                        this->changePlayerInPossession();
                        isHandlingError = true;
                    }
                    break;
                }
                case MatchActionSubtype::PASS_KNOCK_ON: isDeliberate = false; // fall through
                case MatchActionSubtype::PASS_FORWARD_PASS: {

                    // is ball passed forward deliberately? if yes a penalty follows, if no a scrum follows
                    isDeliberate &= RandomValue::generateRandomBool(MatchActionSubtype::PASS_DELIBERATE_FORWARD_PASS);

                    if (this->displayOn(MW)) {

                        if (isDeliberate)
                            _mw->logRecord(message.displayWithReplace(this->penaltyInfringement(), "passForward",
                                           { this->_match->team(this->whoIsInPossession().first)->name() }));
                        else
                            _mw->logRecord(message.displayWithReplace(this->objectName(), "scrumAfterKnockOn",
                                           { this->_match->team(this->whoIsInPossession().first)->name() }));
                    }
                    this->changeBallPossession(_teamInPossession);
                    isHandlingError = !isDeliberate;

                    const timePassed timePassed = (isDeliberate) ? this->penalty() : this->scrum();
                    endOfPeriod |= this->refreshTime(timePassed);
                    break;
                }
                case MatchActionSubtype::PASS_PASS_INTERCEPTED: {

                    this->ballPassed(MatchScore::Passes::MISSED);
                    this->changeBallPossession(_teamInPossession);
                    this->changePlayerInPossession();

                    isHandlingError = true;
                    break;
                }
                case MatchActionSubtype::PASS_THROWN_INTO_OUT: {

                    this->ballPassed(MatchScore::Passes::MISSED);
                    this->changeBallPossession(_teamInPossession);
                    isHandlingError = true;

                    const timePassed timePassed = this->lineOutIsThrowed();
                    endOfPeriod |= this->refreshTime(timePassed);
                    break;
                }
                default: ;
            }

            if (isHandlingError) {

                // we don't know at this moment if passing player's team is in possession or not
                // because possession could've changed (penalty, scrum, line-out could be executed in the meantime)
                const MatchType::Location team = (_teamInPossession == passingPlayer.second)
                                    ? this->whoIsInPossession().first : this->whoIsInPossession().second;

                // update handling errors' statistics
                const uint8_t currentValueHandlingErrors = this->_match->score(team)->handlingErrors();

                this->updateStatistics(team, StatsType::NumberOf::HANDLING_ERRORS, passingPlayer.first);

                if (this->displayOn(MW))
                    _mw->updateStatisticsUI(team, QStringLiteral("HandlingErrorsLabel"), QString::number(currentValueHandlingErrors));
            }

            isOffload = false;
        }

        // player is kicking (the ball); incl. kick-off
        if (action == MatchActionType::KICKING) {

            this->resetPhases();

            // if distance to goal line is less than 2 metres (incl.), kick can't go through
            const uint8_t probabilityThreshold =
                _settings->matchActivities().probability(MatchActionSubtype::KICK_KICKED_FORWARD) + 1;
            const uint8_t from = (this->distanceToGoalLine() > 2) ? 1 : probabilityThreshold;
            const uint16_t kick = RandomValue::generateRandomInt<uint16_t>(from, 100);

            MatchActionSubtype::MatchActivityType nextAction = _settings->matchActivities().action(kick, action);

            // drop goal can't be scored from distance over 40m (by default; may be changed in Settings)
            const bool dropGoalPossible = (this->distanceToGoalLine() > _settings->dropGoalMaxDistance()) ? false : true;
            if (_restartPlay || (nextAction == MatchActionSubtype::KICK_DROP_GOAL_ATTEMPT && !dropGoalPossible))
                nextAction = MatchActionSubtype::KICK_KICKED_FORWARD;

            // the closer the goal line, the less powerful kick would be attempted (by kicking player)
            const uint8_t maxDistance = std::min<uint8_t>(_settings->kickMaxDistance(), static_cast<uint8_t>(distanceToGoalLine()));
            uint8_t metresMade = RandomValue::generateRandomInt<uint8_t>(1, maxDistance);

            switch (nextAction) {

                // kick goes through - team in possession moves forward
                case MatchActionSubtype::KICK_KICKED_FORWARD: {

                    this->_restartPlay = false;

                    // change ball position first then update statistics
                    this->changeBallPositionOnPitch(metresMade);
                    playerIsKicking(metresMade);

                    // add metres made to player's stats

                    endOfPeriod |= this->refreshTime(metresMade/4);

                    // who catches the ball
                    const uint8_t probability =
                        _settings->matchActivities().probability(MatchActionSubtype::KICK_CATCHED_BY_OPPONENT);
                    const bool opponentCatchesTheBall = RandomValue::generateRandomBool(probability);
                    if (opponentCatchesTheBall)
                        this->changeBallPossession(_teamInPossession);

                    this->changePlayerInPossession();
                    break;
                }

                // kick goes over touch line; opponent throws line-out
                case MatchActionSubtype::KICK_KICKED_INTO_OUT: {

                    // metres made are counted only to the point where ball crossed touch line
                    const uint8_t metresAfterTouchLineWasCrossed = RandomValue::generateRandomInt<uint8_t>(1, metresMade-1);
                    metresMade -= metresAfterTouchLineWasCrossed;

                    // kicked directly into touch or with bounce?
                    const uint8_t probability =
                        _settings->matchActivities().probability(MatchActionSubtype::KICK_KICKED_DIRECTLY_INTO_OUT);
                    const bool kickedDirectly = RandomValue::generateRandomBool(probability);

                    // kick goes directly over touch line
                    if (kickedDirectly) {

                        // no gain in ground (if kicked from outside of its own 22)
                        if (this->distanceToGoalLine() <= groundDimensions.from22metreLineToOpponentsGoalLine)
                            metresMade = 0;

                        // team in possesion moves forward (if kicked from inside of its own 22)
                        // no action needed <=> metres made already set
                    }
                    // kick bounces off the ground and then goes over touch line
                    else {

                        // team in possesion moves forward
                        // no action needed <=> metres made already set
                    }

                    // if line-out would be thrown within 5m distance off goal-line, it is formed on the 5-metre line
                    // note: move to 5-metre line is actually made inside lineOutIsThrowed() function;
                    // this code serves for adjustment of metres made (kicked) only
                    if (this->distanceToGoalLine() - metresMade < groundDimensions.fromGoalLineTo5metreLine) {

                        // if kick into touch has been performed from within 5m distance off goal-line
                        metresMade = (this->distanceToGoalLine() < groundDimensions.fromGoalLineTo5metreLine)
                                   ? 0 : this->distanceToGoalLine() - groundDimensions.fromGoalLineTo5metreLine;
                    }

                    // change ball position first then update statistics
                    this->changeBallPositionOnPitch(metresMade);
                    this->playerIsKicking(metresMade);

                    endOfPeriod |= this->refreshTime(metresMade/4);

                    // opponent throws line-out
                    this->changeBallPossession(_teamInPossession);
                    const timePassed timePassed = this->lineOutIsThrowed();
                    endOfPeriod |= this->refreshTime(timePassed);

                    break;
                }

                // kick is blocked by opponent's player (no move forward); either team can pick up the ball
                case MatchActionSubtype::KICK_BLOCKED: {

                    endOfPeriod |= this->refreshTime(4);

                    // who picks up the ball
                    const bool opponentPicksUpTheBall = RandomValue::generateRandomBool(50);
                    if (opponentPicksUpTheBall)
                        this->changeBallPossession(_teamInPossession);

                    this->changePlayerInPossession();
                    break;
                }

                // drop goal scored
                case MatchActionSubtype::KICK_DROP_GOAL_ATTEMPT: {

                    // update metres made by kicking <=> whole distance to goal line
                    metresMade = this->distanceToGoalLine();

                    // change ball position first then update statistics
                    this->changeBallPositionOnPitch(metresMade);
                    this->playerIsKicking(metresMade);

                    const timePassed timePassed = this->dropGoalScored();
                    endOfPeriod |= this->refreshTime(metresMade/4 + timePassed);

                    // add metres made to player's stats

                    break;
                }
                default: ;
            }
        }

        if (this->_match->timePlayed().lastIncrement() > 0) {

            const uint8_t minutes = this->_match->timePlayed().lastIncrement();

            // adjust players' characteristics values dependent on time progress
            for (uint8_t i = 0; i < 2; ++i) {

                const MatchType::Location loc = static_cast<MatchType::Location>(i);
                for (auto player: this->_match->team(loc)->squad()) {

                    if (player->isOnPitch() && player->isHealthy()) {

                        // total number of minutes played (in all matches including this one)
                        player->stats()->addMinutesPlayed(minutes);

                        // current number of minutes played in this match
                        const uint8_t minutesPlayedInThisMatchOriginalValue =
                            this->_match->playerStats(loc, player)->getStatsValue(StatsType::NumberOf::MINS_PLAYED);
                        this->_match->playerStats(loc, player)->addMinutesPlayed(minutes);
                        const uint8_t minutesPlayedInThisMatchCurrentValue =
                            this->_match->playerStats(loc, player)->getStatsValue(StatsType::NumberOf::MINS_PLAYED);

                        if (minutesPlayedInThisMatchCurrentValue/10 > minutesPlayedInThisMatchOriginalValue/10) {

                            const bool substitutionFollows = this->changeInFatigue(player);
                            if (substitutionFollows) {

                                const timePassed timePassed = this->substitution(this->_match->team(loc), player);
                                endOfPeriod |= this->refreshTime(timePassed);
                            }
                        }
                    }
                }
            }

            // update sin-bin
            if (!this->_match->sinBin().isEmpty())
                this->suspensionsUpdate(minutes);

            // make substitutions
            if (this->_match->currentPeriod() != MatchPeriod::TimePeriod::FULL_TIME) {

                timePassed timePassed = this->regularSubstitutions();
                endOfPeriod |= this->refreshTime(timePassed);
            }

            // change settings (to be updated)
            // _mw->nextAction(MatchWidget::ResumePlay::NO_ACTION);

            this->_match->timePlayed().resetIncrement();
        }

        if (endOfPeriod) {

            if (this->_match->currentPeriod() == MatchPeriod::TimePeriod::FULL_TIME) {

                // add time spent on celebrations :-)
                this->celebrationsTime();

                if (this->displayOn(MW)) {

                    _mw->ui->updatePeriod(this->_periods->description(this->_match->currentPeriod()));
                    _mw->logRecord(this->_periods->description(this->_match->currentPeriod()));
                    _mw->timeStoppedMessageBox("endOfMatch", { this->_match->team(MatchType::Location::HOSTS)->name(),
                                                               this->_match->team(MatchType::Location::VISITORS)->name()});

                    // display points gained in this match (for regular matches only)
                    if (this->_match->type() == MatchType::Type::REGULAR) {

                        _mw->logRecord(this->_match->team(MatchType::Location::HOSTS)->name() % QStringLiteral(": ") %
                            QString::number(this->_match->points(MatchType::Location::HOSTS)) % QStringLiteral(" point(s)"));
                        _mw->logRecord(this->_match->team(MatchType::Location::VISITORS)->name() % QStringLiteral(": ") %
                            QString::number(this->_match->points(MatchType::Location::VISITORS)) % QStringLiteral(" point(s)"));
                    }
                }

                if (this->displayOn(FW)) {

                    _fw->ui->currentMatchProgress->setVisible(false);
                    _fw->ui->currentMatchProgress->repaint();

                    QLabel * const resultTypeLabel = _fw->findWidgetByCode<QLabel *>
                        (this->_match->code(), on::fixtureswidget.resultType);
                    _fw->ui->displayResultTypeSuffix(this->_match, resultTypeLabel);
                    resultTypeLabel->repaint();

                    if (this->_match->type() == MatchType::Type::REGULAR) {

                        QLabel * const teamsPointsLabel = _fw->findWidgetByCode<QLabel *>
                            (this->_match->code(), on::fixtureswidget.pointsFromGame);
                        _fw->ui->displayTeamsPoints(this->_match, teamsPointsLabel);
                        teamsPointsLabel->repaint();
                    }
                }

                endOfMatch();
            }
            return;
        }
    }

    if (this->_match->currentPeriod() == MatchPeriod::TimePeriod::BEFORE_KICKING_INTERVAL) {

        if (this->displayOn(FW)) {

            _fw->ui->currentMatchProgress->setVisible(false);
            _fw->ui->currentMatchProgress->repaint();
        }

        this->_match->timePlayed().switchTimePeriodTo(/*kicking competition*/);

        if (this->displayOn(MW)) {

            _mw->ui->updatePeriod(this->_periods->description(this->_match->currentPeriod()));
            _mw->logRecord(this->_periods->description(this->_match->currentPeriod()));
        }

        // determine winner of kicking competition
        this->kickingCompetition();

        // refresh system time
        this->_dateTime.refreshSystemDateAndTime(this->_periods->length(this->_match->currentPeriod(), 60));
        emit timeChanged();

        this->_match->timePlayed().switchTimePeriodTo(MatchPeriod::TimePeriod::FULL_TIME);
        this->celebrationsTime();

        if (this->displayOn(FW)) {

            QLabel * const resultTypeLabel = _fw->findWidgetByCode<QLabel *>
                (this->_match->code(), on::fixtureswidget.resultType);
            _fw->ui->displayResultTypeSuffix(this->_match, resultTypeLabel);
            resultTypeLabel->repaint();
        }

        if (this->displayOn(MW)) {

            _mw->ui->updatePeriod(this->_periods->description(this->_match->currentPeriod()));
            _mw->logRecord(this->_periods->description(this->_match->currentPeriod()));
            _mw->timeStoppedMessageBox(this->_periods->messageBoxDefinition(this->_match->currentPeriod()),
                { this->_match->team(MatchType::Location::HOSTS)->name(),
                  this->_match->team(MatchType::Location::VISITORS)->name() });
        }

        endOfMatch();
    }

    return;
}

// explicit instantiation of constructor(s)
template GamePlay::GamePlay(MatchWidget * const, Settings * const, DateTime &, Match * const, Team * const);
template GamePlay::GamePlay(FixturesWidget * const, Settings * const, DateTime &, Match * const, Team * const);
