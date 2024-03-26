/*******************************************************************************
 Copyright 2020-23 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include <algorithm>
#include "settings/matchsettings.h"
#include "team.h"

void TeamPoints::updateFromMatchScore(MatchScore * const score, const uint16_t pointsAgainst, const uint8_t triesAgainst) {

    this->_tries += score->points(PointEvent::TRY);
    this->_conversions += score->points(PointEvent::CONVERSION);
    this->_penalties += score->points(PointEvent::PENALTY);
    this->_dropgoals += score->points(PointEvent::DROPGOAL);

    this->_pointsAgainst += pointsAgainst;
    this->_triesAgainst += triesAgainst;

    return;
}

void TeamResults::updateResults(const TeamResults::ResultType result, const bool TryBonus, const bool DiffBonus) {

    switch (result) {

        case ResultType::WIN: {

            if (TryBonus) _winsWithBonusPoint += 1;
            else _winsWithoutBonusPoint += 1;
            break;
        }
        case ResultType::DRAW: {

            if (TryBonus) _drawsWithBonusPoint += 1;
            else _drawsWithoutBonusPoint += 1;
            break;
        }
        case ResultType::LOSS: {

            if (TryBonus && DiffBonus) _lossesWithBothPonusPoints += 1;
            else if (TryBonus && !DiffBonus) _lossesWithTriesBonusPoint += 1;
            else if (!TryBonus && DiffBonus) _lossesWithDiffBonusPoint += 1;
            else _lossesWithoutBonusPoints += 1;
            break;
        }
    }
}

uint16_t TeamResults::pointsTotal() const {

    const uint16_t pointsWins = _winsWithBonusPoint * (matchPoints.Win + matchPoints.FourTries)
            + _winsWithoutBonusPoint * matchPoints.Win;
    const uint16_t pointsDraws = _drawsWithBonusPoint * (matchPoints.Draw + matchPoints.FourTries)
            + _drawsWithoutBonusPoint * matchPoints.Draw;
    const uint16_t pointsLosses = _lossesWithBothPonusPoints * (matchPoints.FourTries + matchPoints.SevenPointDifference)
            + _lossesWithTriesBonusPoint * matchPoints.FourTries
            + _lossesWithDiffBonusPoint * matchPoints.SevenPointDifference;

    return (pointsWins + pointsDraws + pointsLosses);
}

Team::Team(const uint16_t code, const QString & name, const QString & abbr, const QString & nick,
           const QString & country, const QString & city, const QString & venue, const TeamType type,
           const QString & manager, const uint8_t ranking, const QString & group, const QString & colour):
    _scoredPoints(TeamPoints()), _results(TeamResults()), _inPlayoffs(false), _code(code), _name(name), _abbr(abbr),
    _nick(nick), _country(country), _city(city), _venue(venue), _type(type), _manager(manager), _ranking(ranking),
    _group(group), _colour(colour) {}

uint8_t Team::availablePlayers(const PlayerPosition_index_item::PositionType positionType,
                               Player * const playerInPossession, QVector<Player *> & availablePlayers) {

    for (auto player: this->squad()) {

        if (player->position()->positionType() == positionType && player->isOnPitch() && player != playerInPossession)
            availablePlayers.push_back(player);
    }
    return availablePlayers.size();
}

bool Team::areAllPlayersSelected() const {

    uint16_t teamSquadCheckSum = 0;
    for (auto player: _squad)
        if (player->isBasePlayer() && player->isAvailable())
            teamSquadCheckSum += player->shirtNo();

    auto fullSquadCheckSum = []() -> uint16_t
        { uint16_t x = 0; for (auto i = 1; i <= numberOfPlayers.PlayersOnPitch; ++i) x+=i; return x; };

    return (teamSquadCheckSum == fullSquadCheckSum());
}

bool Team::selectPlayersForNextMatch(const ConditionWeights & conditionSettings) {

    // clear previous selection
    for (auto player: _squad) {

        player->assignShirtNo(0);
        player->resetAllPreferences();
    }

    // go through all (specific) positions [1-15]
    for (auto position: playerPosition_index.findPlayerPositionsByType(PlayerPosition_index_item::PositionBaseType::UNKNOWN)) {

        QVector<Player *> playersForSelection;

        // "first round" selection - all players with corresponding position
        for (auto player: _squad) {

            if (!player->isAvailable())
                continue;

            // player's position either exactly matches or is non-specific and its type matches
            if (position->positionName() == player->position()->originalPosition() ||
                (position->positionType() == player->position()->positionType())) {

                // select for "second round" selection (only players not selected for another position yet)
                if (player->shirtNo() == 0)
                    playersForSelection.push_back(player);
            }
        }

        // if no players have been selected in previous step try to search among players matched by base type
        if (playersForSelection.isEmpty()) {

            for (auto player: _squad) {

                if (!player->isAvailable())
                    continue;

                // player's position is either specific or non-specific and only its base type matches
                if (playerPosition_index.findPositionBaseTypeByType(position->positionType()) ==
                    playerPosition_index.findPositionBaseTypeByType(player->position()->positionType())) {

                    // select for "second round" selection (only players not selected for another position yet)
                    if (player->shirtNo() == 0)
                        playersForSelection.push_back(player);
                }
            }
        }

        uint16_t bestPlayerShape = 0;
        player::MatchTypes bestPlayerMatchType;
        Player * bestPlayer = nullptr;

        // "second round" selection - player who is currently in best "shape"
        for (auto player: playersForSelection) {

            ConditionThresholds::ConditionValue severity;
            uint16_t currentPlayerShape = player->condition(player::Conditions::OVERALL, severity);
            player::MatchTypes currentPlayerMatchType = player::MatchTypes::EXACT_POSITION;

            // players with specific position (which is currently being assigned to) are more preferable
            if (position->positionName() == player->position()->originalPosition() &&
                severity != ConditionThresholds::ConditionValue::CRITICAL)
                currentPlayerShape *= conditionSettings.matchTypeWeight(player::MatchTypes::EXACT_POSITION);
            else if (position->positionName() == player->position()->originalPosition())
                currentPlayerShape *= conditionSettings.matchTypeWeight(player::MatchTypes::EXACT_POSITION);
            // players with non-specific position (but with same position type) are less preferable
            else if (player->position()->isPositionGeneric())
                currentPlayerShape *= conditionSettings.matchTypeWeight(currentPlayerMatchType = player::MatchTypes::GENERIC_POSITION);
            // players with specific (but different) position than position which is currently
            // being assigned to (but with same position type) are even less preferable
            else if (position->positionType() == player->position()->positionType())
                currentPlayerShape *= conditionSettings.matchTypeWeight(currentPlayerMatchType = player::MatchTypes::DIFFERENT_POSITION);
            // players with different position type (but with same position base type)
            else
                currentPlayerShape *= conditionSettings.matchTypeWeight(currentPlayerMatchType = player::MatchTypes::UNRELATED_POSITION);

            if (currentPlayerShape >= bestPlayerShape) {

                bestPlayerShape = currentPlayerShape;
                bestPlayerMatchType = currentPlayerMatchType;
                bestPlayer = player;
            }
        }

        // no player selected for current position
        if (bestPlayer == nullptr)
            return false;

        // decrease condition(s) if player is "less-suitable" (only when selected for this position for the first time)
        if (bestPlayerMatchType == player::MatchTypes::DIFFERENT_POSITION && position != bestPlayer->position()->playerPosition()) {

            const bool probability = RandomValue::generateRandomBool(25);
            bestPlayer->condition()->decreaseCondition(player::Conditions::FORM, static_cast<uint8_t>(probability));
        }
        if (bestPlayerMatchType == player::MatchTypes::UNRELATED_POSITION && position != bestPlayer->position()->playerPosition()) {

            bool probability = RandomValue::generateRandomBool(50);
            bestPlayer->condition()->decreaseCondition(player::Conditions::FORM, static_cast<uint8_t>(probability));
            probability = RandomValue::generateRandomBool(25);
            bestPlayer->condition()->decreaseCondition(player::Conditions::MORALE, static_cast<uint8_t>(probability));
        }

        // best player for current position selected
        bestPlayer->assignShirtNo(position->positionNo());
        bestPlayer->assignCurrentPosition(position);

        // assign players for specified gameplay actions
        switch (bestPlayer->shirtNo()) {

            case 2 /* hooker */: bestPlayer->setAsPreferredFor(player::PreferredForAction::LINEOUT); break;
            case 9 /* scrum-half */: bestPlayer->setAsPreferredFor(player::PreferredForAction::SCRUM); break;
            case 10 /* fly-half */: bestPlayer->setAsPreferredFor(player::PreferredForAction::PENALTY);
                                    bestPlayer->setAsPreferredFor(player::PreferredForAction::CONVERSION); break;
            case 15 /* fullback */: bestPlayer->setAsPreferredFor(player::PreferredForAction::KICK_OFF); break;
        }
    }

    return true;
}

bool Team::selectSubstitutes(const ConditionWeights & conditionSettings) {

    QVector<QPair<Player *, uint16_t>> playersForSelection;

    for (auto player: _squad) {

        if (player->isAvailable() && player->shirtNo() == 0 &&
            player->position()->currentPosition() != PlayerPosition::notAssigned) {

            uint16_t currentPlayerShape = player->condition(player::Conditions::OVERALL);

            // player is on his original position (which is not unassigned)
            if (player->position()->currentPosition() == player->position()->originalPosition())
                currentPlayerShape *= conditionSettings.matchTypeWeight(player::MatchTypes::EXACT_POSITION);
            // player is on different position (original position is unassigned)
            else if (player->position()->currentPosition() != player->position()->originalPosition() &&
                     player->position()->originalPosition() == PlayerPosition::notAssigned)
                currentPlayerShape *= conditionSettings.matchTypeWeight(player::MatchTypes::GENERIC_POSITION);
            // player is on different position (original position is not unassigned)
            else if (player->position()->currentPosition() != player->position()->originalPosition() &&
                     player->position()->originalPosition() != PlayerPosition::notAssigned)
                currentPlayerShape *= conditionSettings.matchTypeWeight(player::MatchTypes::DIFFERENT_POSITION);
            else
                currentPlayerShape *= conditionSettings.matchTypeWeight(player::MatchTypes::UNRELATED_POSITION);

            playersForSelection.push_back(qMakePair<Player *, uint16_t>(player, currentPlayerShape));
        }
    }

    // sort players by score (current shape)
    std::sort(playersForSelection.begin(), playersForSelection.end(),
              [](QPair<Player *, uint16_t> previous, QPair<Player *, uint16_t> next)
        { return next.second >= previous.second; });
    // use (max.) first PlayersOnBench (8) only
    playersForSelection.resize(std::min(static_cast<uint8_t>(playersForSelection.size()), numberOfPlayers.PlayersOnBench));
    // sort players by position
    std::sort(playersForSelection.begin(), playersForSelection.end(),
              [](QPair<Player *, uint16_t> previous, QPair<Player *, uint16_t> next)
        { return next.first->position()->currentPositionNo() >= previous.first->position()->currentPositionNo(); });

    uint8_t shirtNo = numberOfPlayers.PlayersOnPitch;
    for (auto player: playersForSelection)
        player.first->assignShirtNo(++shirtNo);

    return true;
}

void Team::cleanPitch() {

    for (auto player: _squad)
        player->withdrawPlayer();

    return;
}

uint8_t Team::numberOfPlayersOnPitch() const {

    return std::count_if(this->squad().cbegin(), this->squad().cend(), [](Player * const player) { return player->isOnPitch(); });
}

uint16_t Team::packWeight(bool * adjusted) const {

    if (adjusted != nullptr)
        *(adjusted) = false;

    uint16_t packWeight = 0;
    uint8_t playerWeight = 0;

    uint8_t noOfPlayersWithWeight = 0;
    uint8_t numberOfPackPlayersOnPitch = 0; // == numberOfPlayers.NoOfForwards if no one is suspended/injured

    for (auto player: this->_squad) {

        if (!player->isPackPlayer())
            continue;

        ++numberOfPackPlayersOnPitch;

        if ((playerWeight = player->attribute(player::Attributes::WEIGHT)) > 0) {

            ++noOfPlayersWithWeight;
            packWeight += playerWeight;
        }
    }

    if (/*noOfPlayersWithWeight == numberOfPlayers.NoOfForwards ||*/ noOfPlayersWithWeight == numberOfPackPlayersOnPitch)
        return packWeight;

    // if we know weight for less than 3 players in the pack, scrap the value
    if (noOfPlayersWithWeight < numberOfPlayers.NoOfForwards/2 - 1)
        return 0;

    // otherwise adjust the value according to the number of players with known weight
    const double adjustedPackWeight = (static_cast<double>(packWeight) / noOfPlayersWithWeight) *
                                      numberOfPackPlayersOnPitch * coefficients.INCOMPLETE_WEIGHT;
    if (adjusted != nullptr)
        *(adjusted) = true;

    return (static_cast<uint16_t>(std::round(adjustedPackWeight)));
}
