/*******************************************************************************
 Copyright 2021 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include <algorithm>
#include "player/player_attributes.h"
#include "shared/random.h"

const QString PlayerAttributes::unknownValue = QStringLiteral("N/A");
const AttributeLimits PlayerAttributes::attributeLimits = AttributeLimits();
const QString PlayerAttributes::attributeDescription = QStringLiteral("/attributeDescription");

PlayerAttributes::PlayerAttributes(const int16_t caps, const uint8_t age,
                                   const PlayerPosition_index_item::PositionType positionType,
                                   const uint8_t teamRanking, const QMap<player::Attributes, uint8_t> & abilities):
    _height(abilities[player::Attributes::HEIGHT]), _weight(abilities[player::Attributes::WEIGHT]),
    _agility(abilities[player::Attributes::AGILITY]), _dexterity(abilities[player::Attributes::DEXTERITY]),
    _endurance(abilities[player::Attributes::ENDURANCE]), _handling(abilities[player::Attributes::HANDLING]),
    _kicking(abilities[player::Attributes::KICKING]), _speed(abilities[player::Attributes::SPEED]),
    _strength(abilities[player::Attributes::STRENGTH]), _tackling(abilities[player::Attributes::TACKLING]) {

    // agility = ability to avoid opponent's tackle attempt
    if (this->_agility == 0)
        this->calculateAgility(caps, age, teamRanking);

    // decreases probability of injury (both caused and sustained)
    if (this->_dexterity == 0)
        this->calculateDexterity(age, positionType);

    // affects level of exhaustion in relation to time played
    if (this->_endurance == 0)
        this->calculateEndurance(age, teamRanking);

    // affects probability of knock-ons / handling errors
    if (this->_handling == 0)
        this->calculateHandling(age, caps, teamRanking, positionType);

    // affects success rate of kicking (both penalties and during play)
    if (this->_kicking == 0)
        this->calculateKicking(age, teamRanking, positionType);

    // affects distance covered in given time period
    if (this->_speed == 0)
        this->calculateSpeed(age, positionType);

    // contributes to overall propability of winning a scrum
    if (this->_strength == 0)
        this->calculateStrength(age, positionType);

    // affects probability of successful tackle and decreases probability of injury
    if (this->_tackling == 0)
        this->calculateTackling(age, caps, teamRanking, positionType);
}

// > height < agility; > weight << agility; > caps > agility; < age >> agility; > ranking > agility
void PlayerAttributes::calculateAgility(const uint16_t caps, const uint8_t age, const uint8_t teamRanking) {

    uint8_t maxValue = attributeLimits.maxValue
                     - static_cast<uint8_t>(this->_height > attributeLimits.heightUpperLimit)
                     - static_cast<uint8_t>(this->_weight > attributeLimits.weightMiddleLimit)
                     - static_cast<uint8_t>(this->_weight > attributeLimits.weightUpperLimit);
    maxValue += std::max(0, (2 - static_cast<int8_t>(teamRanking / 8)));
    if (maxValue > attributeLimits.maxValue)
        maxValue = attributeLimits.maxValue;

    uint8_t minValue = attributeLimits.minValue
                     + static_cast<uint8_t>(age < attributeLimits.ageUpperLimit && age != 0)
                     + static_cast<uint8_t>(age < attributeLimits.ageMiddleLimit && age != 0)
                     + static_cast<uint8_t>(caps >= attributeLimits.capsMiddleLimit);
    minValue += std::max(0, (2 - static_cast<int8_t>(teamRanking / 8)));
    if (minValue > maxValue)
        minValue = maxValue;

    this->_agility = RandomValue::generateRandomInt<uint8_t>(minValue, maxValue);

    return;
}

// > height < dexterity; < weight < dexterity; position (first row) < dexterity; < age >> dexterity
void PlayerAttributes::calculateDexterity(const uint8_t age, const PlayerPosition_index_item::PositionType position) {

    const uint8_t maxValue = attributeLimits.maxValue
                           - static_cast<uint8_t>(this->_height > attributeLimits.heightUpperLimit)
                           - static_cast<uint8_t>(this->_weight < attributeLimits.weightLowerLimit)
                           - static_cast<uint8_t>(position == PlayerPosition_index_item::FIRST_ROW);

    uint8_t minValue = attributeLimits.minValue
                     + static_cast<uint8_t>(age < attributeLimits.ageUpperLimit && age != 0)
                     + static_cast<uint8_t>(age < attributeLimits.ageMiddleLimit && age != 0);
    if (minValue > maxValue)
        minValue = maxValue;

    this->_dexterity = RandomValue::generateRandomInt<uint8_t>(minValue, maxValue);

    return;
}

// > weight << endurance; < age >> endurance; > ranking > endurance
void PlayerAttributes::calculateEndurance(const uint8_t age, const uint8_t teamRanking) {

    uint8_t maxValue = attributeLimits.maxValue
                     - static_cast<uint8_t>(this->_weight > attributeLimits.weightMiddleLimit)
                     - static_cast<uint8_t>(this->_weight > attributeLimits.weightUpperLimit);
    maxValue += std::max(0, (2 - static_cast<int8_t>(teamRanking / 8)));
    if (maxValue > attributeLimits.maxValue)
        maxValue = attributeLimits.maxValue;

    uint8_t minValue = attributeLimits.minValue
                     + static_cast<uint8_t>(age < attributeLimits.ageUpperLimit && age != 0)
                     + static_cast<uint8_t>(age < attributeLimits.ageMiddleLimit && age != 0);
    minValue += std::max(0, (2 - static_cast<int8_t>(teamRanking / 8)));
    if (minValue > maxValue)
        minValue = maxValue;

    this->_endurance = RandomValue::generateRandomInt<uint8_t>(minValue, maxValue);

    return;
}

// > caps > handling; position (half-back) > handling; ranking > handling
void PlayerAttributes::calculateHandling(const uint8_t age, const uint16_t caps, const uint8_t teamRanking,
                                         const PlayerPosition_index_item::PositionType position) {

    const uint8_t maxValue = attributeLimits.maxValue
                           - static_cast<uint8_t>(age < attributeLimits.ageLowerLimit || age == 0);

    uint8_t minValue = attributeLimits.minValue
                     + static_cast<uint8_t>(caps >= attributeLimits.capsMiddleLimit)
                     + static_cast<uint8_t>(position == PlayerPosition_index_item::HALF_BACK);
    minValue += std::max(0, (2 - static_cast<int8_t>(teamRanking / 8)));
    if (minValue > maxValue)
        minValue = maxValue;

    this->_handling = RandomValue::generateRandomInt<uint8_t>(minValue, maxValue);

    return;
}

// > weight < kicking; position (back) > kicking, (fullback) > kicking; < age < kicking; > ranking > kicking
void PlayerAttributes::calculateKicking(const uint8_t age, const uint8_t teamRanking,
                                        const PlayerPosition_index_item::PositionType position) {

    uint8_t maxValue = attributeLimits.maxValue
                     - static_cast<uint8_t>(this->_weight > attributeLimits.weightUpperLimit)
                     - static_cast<uint8_t>(age < attributeLimits.ageLowerLimit || age == 0);
    maxValue += std::max(0, (2 - static_cast<int8_t>(teamRanking / 8)));
    if (maxValue > attributeLimits.maxValue)
        maxValue = attributeLimits.maxValue;

    typedef PlayerPosition_index_item::PositionType pos;
    const QVector<PlayerPosition_index_item::PositionType> kickingPositions
        { pos::FULLBACK, pos::WING, pos::CENTRE, pos::HALF_BACK };

    uint8_t minValue = attributeLimits.minValue
                     + static_cast<uint8_t>(position == PlayerPosition_index_item::FULLBACK)
                     + static_cast<uint8_t>(std::any_of(kickingPositions.begin(), kickingPositions.end(),
                       [&position](const PlayerPosition_index_item::PositionType it) { return it == position; }));
    minValue += std::max(0, (2 - static_cast<int8_t>(teamRanking / 8)));
    if (minValue > maxValue)
        minValue = maxValue;

    this->_kicking = RandomValue::generateRandomInt<uint8_t>(minValue, maxValue);

    return;
}

// > height < speed; > weight << speed; position position (back) > speed, (wing) > speed; > age << speed
void PlayerAttributes::calculateSpeed(const uint8_t age, const PlayerPosition_index_item::PositionType position) {

    const uint8_t maxValue = attributeLimits.maxValue
                           - static_cast<uint8_t>(this->_height > attributeLimits.heightUpperLimit)
                           - static_cast<uint8_t>(this->_weight > attributeLimits.weightMiddleLimit)
                           - static_cast<uint8_t>(this->_weight > attributeLimits.weightUpperLimit);

    typedef PlayerPosition_index_item::PositionType pos;
    const QVector<PlayerPosition_index_item::PositionType> kickingPositions
        { pos::FULLBACK, pos::WING, pos::CENTRE, pos::HALF_BACK };

    uint8_t minValue = attributeLimits.minValue
                     + static_cast<uint8_t>(age < attributeLimits.ageUpperLimit && age != 0)
                     + static_cast<uint8_t>(position == PlayerPosition_index_item::WING)
                     + static_cast<uint8_t>(std::any_of(kickingPositions.begin(), kickingPositions.end(),
                       [&position](const PlayerPosition_index_item::PositionType it) { return it == position; }));
    if (minValue > maxValue)
        minValue = maxValue;

    this->_speed = RandomValue::generateRandomInt<uint8_t>(minValue, maxValue);

    return;
}

// < weight < strength, > weight > strength ; position (forward) > strength; < age < strength, > age < strength
void PlayerAttributes::calculateStrength(const uint8_t age, const PlayerPosition_index_item::PositionType position) {

    const uint8_t maxValue = attributeLimits.maxValue
                     - static_cast<uint8_t>(this->_weight < attributeLimits.weightLowerLimit)
                     - static_cast<uint8_t>(age < attributeLimits.ageLowerLimit || age == 0)
                     - static_cast<uint8_t>(age > attributeLimits.ageUpperLimit);

    typedef PlayerPosition_index_item::PositionType pos;
    const QVector<PlayerPosition_index_item::PositionType> kickingPositions
        { pos::THIRD_ROW, pos::SECOND_ROW, pos::FIRST_ROW };

    uint8_t minValue = attributeLimits.minValue
                     + static_cast<uint8_t>(this->_weight > attributeLimits.weightUpperLimit)
                     + static_cast<uint8_t>(position == PlayerPosition_index_item::FIRST_ROW)
                     + static_cast<uint8_t>(std::any_of(kickingPositions.begin(), kickingPositions.end(),
                       [&position](const PlayerPosition_index_item::PositionType it) { return it == position; }));
    if (minValue > maxValue)
        minValue = maxValue;

    this->_strength = RandomValue::generateRandomInt<uint8_t>(minValue, maxValue);

    return;
}

// > height < tackling; > weight > tackling; > caps > tackling ; position; < age < tackling; > ranking > tackling
void PlayerAttributes::calculateTackling(const uint8_t age, const uint16_t caps, const uint8_t teamRanking,
                                         const PlayerPosition_index_item::PositionType position) {

    uint8_t maxValue = attributeLimits.maxValue
                     - static_cast<uint8_t>(this->_height > attributeLimits.heightUpperLimit)
                     - static_cast<uint8_t>(age < attributeLimits.ageLowerLimit || age == 0)
                     - static_cast<uint8_t>(position == PlayerPosition_index_item::WING)
                     - static_cast<uint8_t>(position == PlayerPosition_index_item::FULLBACK);
    maxValue += std::max(0, (2 - static_cast<int8_t>(teamRanking / 8)));
    if (maxValue > attributeLimits.maxValue)
        maxValue = attributeLimits.maxValue;

    uint8_t minValue = attributeLimits.minValue
                     + static_cast<uint8_t>(this->_weight > attributeLimits.weightLowerLimit)
                     + static_cast<uint8_t>(caps >= attributeLimits.capsMiddleLimit)
                     + static_cast<uint8_t>(position == PlayerPosition_index_item::SECOND_ROW)
                     + static_cast<uint8_t>(position == PlayerPosition_index_item::THIRD_ROW) * 2;
    minValue += std::max(0, (2 - static_cast<int8_t>(teamRanking / 8)));
    if (minValue > maxValue)
        minValue = maxValue;

    this->_tackling = RandomValue::generateRandomInt<uint8_t>(minValue, maxValue);

    return;
}

uint8_t PlayerAttributes::getValue(const player::Attributes attribute) const {

    uint8_t value = 0;

    switch (attribute) {

        case player::Attributes::HEIGHT: value = _height; break;
        case player::Attributes::WEIGHT: value = _weight; break;
        case player::Attributes::AGILITY: value = _agility; break;
        case player::Attributes::DEXTERITY: value = _dexterity; break;
        case player::Attributes::ENDURANCE: value = _endurance; break;
        case player::Attributes::HANDLING: value = _handling; break;
        case player::Attributes::KICKING: value = _kicking; break;
        case player::Attributes::SPEED: value = _speed; break;
        case player::Attributes::STRENGTH: value = _strength; break;
        case player::Attributes::TACKLING: value = _tackling; break;
        default: value = 0;
    }

    return value;
}
