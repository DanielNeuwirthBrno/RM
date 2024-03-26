/*******************************************************************************
 Copyright 2021-22 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include "player/player_points.h"

PlayerPoints::PlayerPoints(const uint16_t tries, const uint16_t conversions, const uint16_t penalties, const uint16_t dropGoals):
    _tries(tries), _conversions(conversions), _penalties(penalties), _dropGoals(dropGoals) {}

uint16_t PlayerPoints::getPointsValue(const StatsType::NumberOf entity) const {

    uint16_t integerValue;
    double doubleValue;

    const bool valueRetrieved = this->getPointsValue(entity, integerValue, doubleValue);

    return ((valueRetrieved) ? integerValue : 0);
}

bool PlayerPoints::getPointsValue(const StatsType::NumberOf entity, uint16_t & integerValue, double & doubleValue,
                                  const uint16_t gamesPlayed) const {
    integerValue = 0;
    doubleValue = std::numeric_limits<double>::infinity();

    switch (entity) {

        case StatsType::NumberOf::TRIES: integerValue = _tries; break;
        case StatsType::NumberOf::TRIES_PER_MATCH: doubleValue = this->averagePerMatch(_tries, gamesPlayed); break;
        case StatsType::NumberOf::CONVERSIONS: integerValue = _conversions; break;
        case StatsType::NumberOf::PENALTIES: integerValue = _penalties; break;
        case StatsType::NumberOf::DROPGOALS: integerValue = _dropGoals; break;
        case StatsType::NumberOf::POINTS: integerValue = this->points(); break;
        case StatsType::NumberOf::POINTS_PER_MATCH: doubleValue = this->averagePerMatch(this->points(), gamesPlayed); break;
        default: return false;
    }

    return (std::isinf(doubleValue) || !std::isnan(doubleValue));
}

uint16_t PlayerPoints::points() const {

    return (this->_tries * pointValue.Try + this->_conversions * pointValue.Conversion +
            this->_penalties * pointValue.Penalty + this->_dropGoals * pointValue.DropGoal);
}

double PlayerPoints::averagePerMatch(const uint16_t points, const uint16_t gamesPlayed) const {

    const double avgValuePerMatch = (gamesPlayed > 0) ? points/static_cast<double>(gamesPlayed)
                                                      : std::numeric_limits<double>::quiet_NaN();
    return avgValuePerMatch;
}
