/*******************************************************************************
 Copyright 2020-23 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include <QDebug>
#include <algorithm>
#include "match/match.h"

const QString Match::unknownReferee = QStringLiteral("<not assigned>");
const QString Match::unknownVenue = QStringLiteral("neutral ground");

Match::Match(const uint32_t code, const QDateTime & datetime, Team * const hosts, Team * const visitors,
             const MatchType::Type type, Referee * const referee, const QString & venue, const bool played,
             const bool storedInDb, const QPair<MatchType::ToPlayOff, void *> & playoffsRule):
    _code(code), _date(datetime.date()), _time(datetime.time()), _playoffsRule(playoffsRule), _teamHosts(hosts),
    _teamVisitors(visitors), _type(type), _referee(referee), _venue(venue), _timePlayed(MatchTime()),
    _scoreHosts(new MatchScore()), _scoreVisitors(new MatchScore()), _played(played), _storedInDb(storedInDb) {}

Match::~Match() {

    for (auto it: this->_pointsHostsPlayers.values())
        delete it;

    for (auto it: this->_pointsVisitorsPlayers.values())
        delete it;

    for (auto it: this->_statsHostsPlayers.values())
        delete it;

    for (auto it: this->_statsVisitorsPlayers.values())
        delete it;

    switch (this->playoffsType()) {

        case MatchType::ToPlayOff::FROM_REGULAR:
            if (this->playoffsRule() != nullptr) {
                RegularToPlayoffsRule * rule = reinterpret_cast<RegularToPlayoffsRule *>(this->_playoffsRule.second);
                delete rule;
            }
            break;
        case MatchType::ToPlayOff::FROM_PLAYOFFS:
            if (this->playoffsRule() != nullptr) {
               PlayoffsToPlayoffsRule * rule = reinterpret_cast<PlayoffsToPlayoffsRule *>(this->_playoffsRule.second);
               delete rule;
            }
            break;
        case MatchType::ToPlayOff::UNDEFINED:
            if (this->playoffsRule() != nullptr)
                qDebug() << "Unspecified PlayoffsRule pointer type at " << this->_playoffsRule.second << " (memory not released).";
        default: ;
    };
}

uint8_t Match::points(const MatchType::Location loc) const {

    uint8_t points = 0;
    MatchScore * scoreRequested = this->_scoreHosts;
    MatchScore * scoreOpponent = this->_scoreVisitors;

    if (loc == MatchType::Location::VISITORS)
        std::swap(scoreRequested, scoreOpponent);

    if (scoreRequested->points() > scoreOpponent->points())
            points += matchPoints.Win;
        else if (scoreRequested->points() == scoreOpponent->points())
            points += matchPoints.Draw;
        else if (scoreOpponent->points() - scoreRequested->points() <= matchPoints.NoOfPointsForDiffPoint)
            points += matchPoints.SevenPointDifference;

        if (scoreRequested->bonusPointTry())
            points += matchPoints.FourTries;

        return points;
}

// positive input = team in loc is in lead, negative input = team not in loc is in lead
bool Match::pointDifferenceInRange(const MatchType::Location loc, const int8_t minDifference, const int8_t maxDifference,
                                   const bool minClosedInterval, const bool maxClosedInterval) const {

    const int8_t negateDifference = (loc == MatchType::Location::VISITORS) ? -1 : 1;
    const int8_t pointDifference = (static_cast<int16_t>(this->score(MatchType::Location::HOSTS)->points())
                                 - static_cast<int16_t>(this->score(MatchType::Location::VISITORS)->points())) * negateDifference;

    const bool minDifferenceOK = (minClosedInterval) ? pointDifference >= minDifference : pointDifference > minDifference;
    const bool maxDifferenceOK = (maxClosedInterval) ? pointDifference <= maxDifference : pointDifference < maxDifference;

    return (minDifferenceOK && maxDifferenceOK);
}

bool Match::setTeam(const MatchType::Location loc, Team * const team) {

    bool teamSet = false;

    if ((teamSet = loc == MatchType::Location::HOSTS))
        _teamHosts = team;
    if ((teamSet = loc == MatchType::Location::VISITORS))
        _teamVisitors = team;

    team->toPlayoffs(true);

    return teamSet;
}

Team * Match::winner(const TeamResults::ResultType result) const {

    if (this->resultTypeForTeam(MatchType::Location::HOSTS) == result)
        return _teamHosts;
    if (this->resultTypeForTeam(MatchType::Location::VISITORS) == result)
        return _teamVisitors;

    return nullptr;
}

Referee * Match::drawReferee(const QVector<Referee *> & referees, const QVector<Referee *> & excluded) const {

    // referees can be either directly assigned for a given match (in db in Fixture table) or drawed from a pool
    // of referees who are appointed for currently played competition (in db in RefereeInCompetition table); because
    // both of these approaches can be mixed, we need to load all referees and then iterate only over their subset;
    // this subset contains referees who are eligible for judging a match in given competition
    QVector<Referee *> eligibleReferees;
    std::copy_if(referees.cbegin(), referees.cend(), std::back_inserter(eligibleReferees),
                 [&excluded](Referee * referee) { return referee->inDrawPool() && !excluded.contains(referee); });

    // if referee is not directly assigned and no referees are appointed for given competition
    if (eligibleReferees.empty())
        return nullptr;

    const uint16_t pos = RandomValue::generateRandomInt<uint8_t>(0, eligibleReferees.size()-1);

    return eligibleReferees.at(pos);
}

QString Match::venue() const {

    if (!_venue.isEmpty())
        return _venue;
    if (this->team(MatchType::Location::HOSTS) != nullptr && !this->team(MatchType::Location::HOSTS)->venue().isEmpty())
        return this->team(MatchType::Location::HOSTS)->venue();

    return unknownVenue;
}

// returns true if no player of given team has been sin-binned so far (in this match)
bool Match::noSuspensions(const MatchType::Location loc) const {

    const uint8_t noOfSuspensions = std::count_if(this->sinBin().cbegin(), this->sinBin().cend(),
                                    [&loc](const SinBin & sinBin) { return (sinBin.team() == loc); });

    return !static_cast<bool>(noOfSuspensions);
}

// returns true if no player of given team has been replaced (substituted) so far (in this match)
bool Match::noReplacements(const MatchType::Location loc) const {

    const uint8_t noOfReplacements = std::count_if(this->replacements().cbegin(), this->replacements().cend(),
                                    [&loc](const Substitution & substitution) { return (substitution.team() == loc); });

    return !static_cast<bool>(noOfReplacements);
}

double Match::calculatePossessionTimeRatio(const MatchType::Location loc, const uint16_t lastStretchLength) {

    const uint16_t secondsInPossession = this->score(loc)->possession(lastStretchLength);
    double ratio = secondsInPossession * 100.0 / this->timePlayed().timePlayedInSecondsRaw();

    return ratio;
}

double Match::calculateTerritoryTimeRatio(const MatchType::Location loc, const uint16_t lastStretchLength) {

    const uint16_t secondsInTerritory = this->score(loc)->territory(lastStretchLength);
    double ratio = secondsInTerritory * 100.0 / this->timePlayed().timePlayedInSecondsRaw();

    return ratio;
}

void Match::addSuspension(Player * const player, const uint8_t number, const MatchType::Location team,
                          const MatchActionSubtype::MatchActivityType type) {

    const uint8_t suspensionAtMinute = this->timePlayed().minutesPlayed() + 1;
    const SinBin sinBinProperties(player, number, team, type, suspensionAtMinute);

    this->_sinBin.push_back(sinBinProperties);

    return;
}

void Match::deductSuspensionMinutesRemaining(QVector<QPair<Player *, uint8_t>> & returningPlayers,
                                             uint8_t & update, const uint8_t minutes) {

    for (QVector<SinBin>::iterator it = _sinBin.begin(); it != _sinBin.end(); ++it) {

        // it is not necessary to introduce special logic for red cards because in such a case minutesRemaining = 0
        const uint8_t minutesToDeduct = std::min(it->minutesRemaining(), minutes);

        if (it->deductMinutesRemaining(minutesToDeduct)) {

            returningPlayers.push_back(it->player());
            update |= static_cast<uint8_t>(it->team())+1;
        }
    }
    return;
}

bool Match::addSubstitution(Player * const playerOut, Player * const PlayerIn, const MatchType::Location team) {

    const uint8_t substitutionAtMinute = this->timePlayed().minutesPlayed() + 1;
    const bool firstSubstitutionAtThisMinute =
        (_replacements.isEmpty() || _replacements.constLast().minute() < substitutionAtMinute);

    const Substitution substitutionProperties(playerOut, PlayerIn, team, substitutionAtMinute);
    this->_replacements.push_back(substitutionProperties);

    return firstSubstitutionAtThisMinute;
}

// called for every player who makes points in a match (=> not for every player who plays!)
void Match::addNewPointsRecordForPlayer(const MatchType::Location loc, Player * const player) {

    if (loc == MatchType::Location::HOSTS)
        this->_pointsHostsPlayers.insert(player, new PlayerPoints());

    if (loc == MatchType::Location::VISITORS)
        this->_pointsVisitorsPlayers.insert(player, new PlayerPoints());

    return;
}

// use during a match
PlayerPoints * Match::playerPoints(const MatchType::Location loc, Player * const player) {

    if (loc == MatchType::Location::HOSTS) {

        if (!_pointsHostsPlayers.contains(player))
            this->addNewPointsRecordForPlayer(loc, player);
        return _pointsHostsPlayers[player];
    }

    if (loc == MatchType::Location::VISITORS) {

        if (!_pointsVisitorsPlayers.contains(player))
            this->addNewPointsRecordForPlayer(loc, player);
        return _pointsVisitorsPlayers[player];
    }

    return nullptr;
}

// use in any other situation
PlayerPoints * Match::playerPoints_ReadOnly(const MatchType::Location loc, Player * const player) const {

    if (loc == MatchType::Location::HOSTS)
        return _pointsHostsPlayers.value(player, nullptr);

    if (loc == MatchType::Location::VISITORS)
        return _pointsVisitorsPlayers.value(player, nullptr);

    return nullptr;
}

// called for every player who plays in a match
void Match::addNewStatsRecordForPlayer(const MatchType::Location loc, Player * const player) {

    if (loc == MatchType::Location::HOSTS)
        this->_statsHostsPlayers.insert(player, new PlayerStats());

    if (loc == MatchType::Location::VISITORS)
        this->_statsVisitorsPlayers.insert(player, new PlayerStats());

    return;
}

PlayerStats * Match::playerStats(const MatchType::Location loc, Player * const player) {

    if (loc == MatchType::Location::HOSTS)
        return _statsHostsPlayers.value(player, nullptr);

    if (loc == MatchType::Location::VISITORS)
        return _statsVisitorsPlayers.value(player, nullptr);

    return nullptr;
}

TeamResults::ResultType Match::resultTypeForTeam(const MatchType::Location loc) const {

    TeamResults::ResultType result = TeamResults::ResultType::DRAW;

    if (this->_scoreHosts->points() > this->_scoreVisitors->points())
        result = TeamResults::ResultType::WIN;
    if (this->_scoreHosts->points() < this->_scoreVisitors->points())
        result = TeamResults::ResultType::LOSS;

    if (this->type() == MatchType::Type::PLAYOFFS && result == TeamResults::ResultType::DRAW) {

        const int8_t shootOutGoalsDifference = this->_scoreHosts->shootOutGoals() - this->_scoreVisitors->shootOutGoals();
        if (shootOutGoalsDifference > 0)
            result = TeamResults::ResultType::WIN;
        if (shootOutGoalsDifference < 0)
            result = TeamResults::ResultType::LOSS;
    }

    if (loc == MatchType::Location::VISITORS && result != TeamResults::ResultType::DRAW)
        result = static_cast<TeamResults::ResultType>(!static_cast<bool>(result));

    return result;
}

QString Match::shootOutResult() const {

    const QPair<uint8_t, uint8_t> shootOutGoals = qMakePair<uint8_t, uint8_t>
        (this->score(MatchType::Location::HOSTS)->shootOutGoals(),
         this->score(MatchType::Location::VISITORS)->shootOutGoals());

    if (shootOutGoals.first + shootOutGoals.second == 0 || shootOutGoals.first == shootOutGoals.second)
        return QString();

    return QString::number(shootOutGoals.first) + QStringLiteral(" : ") + QString::number(shootOutGoals.second);
}

bool Match::diffBonusPoint(const MatchType::Location loc) const {

    const int16_t pointDifference = this->score(MatchType::Location::HOSTS)->points() - this->score(MatchType::Location::VISITORS)->points();

    if (pointDifference == 0 || std::abs(pointDifference) > matchPoints.NoOfPointsForDiffPoint)
        return false;

    return ((loc == MatchType::Location::HOSTS) ? (pointDifference < 0) : (pointDifference > 0));
}

double Match::playersOnPitchRatio(const MatchType::Location loc) const {

    const double ratio = static_cast<double>(this->team(MatchType::Location::HOSTS)->numberOfPlayersOnPitch()) /
                         static_cast<double>(this->team(MatchType::Location::VISITORS)->numberOfPlayersOnPitch());

    return ((loc == MatchType::Location::HOSTS) ? ratio : (1/ratio));
}
