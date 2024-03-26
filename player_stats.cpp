/*******************************************************************************
 Copyright 2021-22 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include <limits>
#include "player/player_stats.h"

uint16_t PlayerStats::getStatsValue(const StatsType::NumberOf entity) const {

    uint16_t integerValue;
    double doubleValue;

    const bool valueRetrieved = this->getStatsValue(entity, integerValue, doubleValue);

    return ((valueRetrieved) ? integerValue : 0);
}

bool PlayerStats::getStatsValue(const StatsType::NumberOf entity, uint16_t & integerValue, double & doubleValue) const {

    integerValue = 0;
    doubleValue = std::numeric_limits<double>::infinity();

    switch (entity) {

        case StatsType::NumberOf::GAMES_PLAYED: integerValue = _gamesPlayed; break;
        case StatsType::NumberOf::GAMES_PLAYED_SUB: integerValue = _gamesPlayedAsSubstitute; break;
        case StatsType::NumberOf::MINS_PLAYED: integerValue = _minutesPlayed; break;

        case StatsType::NumberOf::TACKLES_MADE: integerValue = _tacklesMadeAll; break;
        case StatsType::NumberOf::TACKLES_COMPLETED: integerValue = _tacklesMadeSuccess; break;
        case StatsType::NumberOf::TACKLES_MISSED: integerValue = _tacklesMadeAll - _tacklesMadeSuccess; break;
        case StatsType::NumberOf::TACKLES_RECEIVED: integerValue = _tacklesReceivedAll; break;
        case StatsType::NumberOf::TACKLES_SUCCESS_RATE: doubleValue = this->tacklesSuccessRate(); break;

        case StatsType::NumberOf::TACKLES_MADE_PER_MATCH:
            doubleValue = this->averagePerMatch(StatsType::NumberOf::TACKLES_MADE); break;
        case StatsType::NumberOf::TACKLES_MISSED_PER_MATCH:
            doubleValue = this->averagePerMatch(StatsType::NumberOf::TACKLES_MISSED); break;
        case StatsType::NumberOf::TACKLES_RECEIVED_PER_MATCH:
            doubleValue = this->averagePerMatch(StatsType::NumberOf::TACKLES_RECEIVED); break;

        case StatsType::NumberOf::METRES_RUN: integerValue = _metresRun; break;
        case StatsType::NumberOf::METRES_KICKED: integerValue = _metresKicked; break;
        case StatsType::NumberOf::CARRIES: integerValue = _carries; break;

        case StatsType::NumberOf::METRES_RUN_PER_MATCH:
            doubleValue = this->averagePerMatch(StatsType::NumberOf::METRES_RUN); break;
        case StatsType::NumberOf::METRES_KICKED_PER_MATCH:
            doubleValue = this->averagePerMatch(StatsType::NumberOf::METRES_KICKED); break;

        case StatsType::NumberOf::PASSES_MADE: integerValue = _passesMadeAll; break;
        case StatsType::NumberOf::PASSES_COMPLETED: integerValue = _passesMadeSuccess; break;
        case StatsType::NumberOf::OFFLOADS: integerValue = _offloads; break;
        case StatsType::NumberOf::PASSES_SUCCESS_RATE: doubleValue = this->passesSuccessRate(); break;
        case StatsType::NumberOf::HANDLING_ERRORS: integerValue = _handlingErrors; break;

        case StatsType::NumberOf::YELLOW_CARDS: integerValue = _yellowCards; break;
        case StatsType::NumberOf::RED_CARDS: integerValue = _redCards; break;
        case StatsType::NumberOf::PENALTIES_CAUSED: integerValue = _penaltyInfringements; break;
        case StatsType::NumberOf::HIGH_TACKLES: integerValue = _tacklesMadeHigh; break;
        case StatsType::NumberOf::DANGEROUS_TACKLES: integerValue = _tacklesMadeDangerous; break;

        default: return false;
    }

    return (std::isinf(doubleValue) || !std::isnan(doubleValue));
}

// only following statistics (all stored as integer) can be directly assigned to
// other statistics (stored mostly as double) are calculated (based on those directly assigned to)
void PlayerStats::setStatsValue(const StatsType::NumberOf entity, const uint16_t value) {

    switch (entity) {

        case StatsType::NumberOf::GAMES_PLAYED: _gamesPlayed = value; break;
        case StatsType::NumberOf::GAMES_PLAYED_SUB: _gamesPlayedAsSubstitute = value; break;
        case StatsType::NumberOf::MINS_PLAYED: _minutesPlayed = value; break;

        case StatsType::NumberOf::TACKLES_MADE: _tacklesMadeAll = value; break;
        case StatsType::NumberOf::TACKLES_COMPLETED: _tacklesMadeSuccess = value; break;
        case StatsType::NumberOf::TACKLES_RECEIVED: _tacklesReceivedAll = value; break;

        case StatsType::NumberOf::METRES_RUN: _metresRun = value; break;
        case StatsType::NumberOf::METRES_KICKED: _metresKicked = value; break;
        case StatsType::NumberOf::CARRIES: _carries = value; break;

        case StatsType::NumberOf::PASSES_MADE: _passesMadeAll = value; break;
        case StatsType::NumberOf::PASSES_COMPLETED: _passesMadeSuccess = value; break;
        case StatsType::NumberOf::OFFLOADS: _offloads = value; break;
        case StatsType::NumberOf::HANDLING_ERRORS: _handlingErrors = value; break;

        case StatsType::NumberOf::YELLOW_CARDS: _yellowCards = value; break;
        case StatsType::NumberOf::RED_CARDS: _redCards = value; break;
        case StatsType::NumberOf::PENALTIES_CAUSED: _penaltyInfringements = value; break;
        case StatsType::NumberOf::HIGH_TACKLES: _tacklesMadeHigh = value; break;
        case StatsType::NumberOf::DANGEROUS_TACKLES: _tacklesMadeDangerous = value; break;

        default: break; // suppress warnings about unused values of entity
    }
    return;
}

double PlayerStats::averagePerMatch(const StatsType::NumberOf entity) const {

    const uint16_t statsValue = this->getStatsValue(entity);
    const double avgValuePerMatch = (_gamesPlayed > 0) ? statsValue/static_cast<double>(_gamesPlayed)
                                                       : std::numeric_limits<double>::quiet_NaN();
    return avgValuePerMatch;
}

double PlayerStats::tacklesSuccessRate() const {

    return (_tacklesMadeAll == 0) ? std::numeric_limits<double>::quiet_NaN()
                                  : (_tacklesMadeSuccess * 100.0 / _tacklesMadeAll);
}

double PlayerStats::passesSuccessRate() const {

    return (_passesMadeAll == 0) ? std::numeric_limits<double>::quiet_NaN()
                                 : (_passesMadeSuccess * 100.0 / _passesMadeAll);
}
