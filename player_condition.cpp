/*******************************************************************************
 Copyright 2021 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include <algorithm>
#include "player/player_condition.h"

PlayerCondition::PlayerCondition(): changeCondition(nullptr), _fatigue(maxValue), _fitness(maxValue),
                                    _health(maxValue), _morale(maxValue), _form(maxValue) {
    this->fullCondition();
    this->generateCurrentForm();
}

PlayerCondition::PlayerCondition(const std::array<uint8_t, 4> & conditions, const uint8_t form):
    _fatigue(conditions[0]), _fitness(conditions[1]), _health(conditions[2]), _morale(conditions[3]), _form(form) {

    this->fullCondition();

    const uint8_t formMaxValue = (this->currentState() == player::HealthStatus::HEALTHY) ? maxValue : maxValue/2;
    if (this->_form == 0)
        generateCurrentForm(formMaxValue);
}

void PlayerCondition::generateCurrentForm(const uint8_t upperLimit) {

    const uint8_t form = RandomValue::generateRandomInt<uint8_t>(1, upperLimit);
    if (form != maxValue)
        this->decreaseCondition(player::Conditions::FORM, maxValue-form);

    return;
}

void PlayerCondition::fullCondition() {

    this->_decreaseInCondition.clear();
    for (uint8_t i = 0; i < static_cast<uint8_t>(player::Conditions::OVERALL); ++i)
        this->_decreaseInCondition.insert(static_cast<player::Conditions>(i), 0);

    return;
}

player::HealthStatus PlayerCondition::currentState(const QDate & stateValidTo) const {

    // if list of health incidents is empty => player is healthy
    if (this->_healthStatus_list.isEmpty())
        return player::HealthStatus::HEALTHY;

    // if no date has been supplied => return current state (if found)
    if (stateValidTo.isNull()) {

        // search for PlayerHealth with "live" status
        PlayerHealth * const playerHealth = this->liveHealthStatus();

        return (playerHealth == nullptr) ? /* no "live" record */ player::HealthStatus::HEALTHY : playerHealth->healthStatus();
    }

    // if a date has been supplied => return state related to associated time period (if found)
    for (auto it: _healthStatus_list) {

        if (it->statusValidFrom() <= stateValidTo && (it->statusValidTo() >= stateValidTo || it->statusValidTo().isNull()))
            return it->healthStatus();
    }

    /* state related to associated time period not found */
    return player::HealthStatus::HEALTHY;
}

QDate PlayerCondition::dateOfRecovery(bool & recoveryDateNotKnown, const QDate & stateValidTo) const {

    // if list of health incidents is empty => player is healthy
    if (this->_healthStatus_list.isEmpty())
        return QDate();

    // if no date has been supplied => return date of recovery for current state (if found)
    if (stateValidTo.isNull()) {

        // search for PlayerHealth with "live" status
        PlayerHealth * const playerHealth = this->liveHealthStatus();

        return (playerHealth == nullptr) ? /* no "live" record */ QDate()
                                         : (recoveryDateNotKnown = true, playerHealth->statusValidTo());
    }

    // if a date has been supplied => return date of recovery for state related to associated time period (if found)
    for (auto it: _healthStatus_list) {

        if (it->statusValidFrom() <= stateValidTo && (it->statusValidTo() >= stateValidTo || it->statusValidTo().isNull()))
            return recoveryDateNotKnown = true, it->statusValidTo();
    }

    /* state related to associated time period not found */
    return QDate();
}

PlayerHealth * PlayerCondition::liveHealthStatus() {

    auto status = std::find_if(this->_healthStatus_list.begin(), this->_healthStatus_list.end(),
                               [](PlayerHealth * const & it) { return it->isLive(); });

    return (status != this->_healthStatus_list.end()) ? *(status) : nullptr;
}

PlayerHealth * PlayerCondition::liveHealthStatus() const {

    const auto status = std::find_if(this->_healthStatus_list.cbegin(), this->_healthStatus_list.cend(),
                                     [](PlayerHealth * const & it) { return it->isLive(); });

    return (status != this->_healthStatus_list.end()) ? *(status) : nullptr;
}

void PlayerCondition::newHealthIssue(const QDate & currentDate, const player::HealthStatus fromStatus,
                                                                const player::HealthStatus toStatus) {

    const uint16_t from = (fromStatus == PlayerHealth::_timeOfAbsenceCategories.at(0).causeOfAbsence() ||
                           fromStatus == player::HealthStatus::UNKNOWN) ? 1 :
        PlayerHealth::absenceSumOfProbabilities(static_cast<player::HealthStatus>(static_cast<uint8_t>(fromStatus)-1))+1;
    const uint16_t to = PlayerHealth::absenceSumOfProbabilities(toStatus);

    int16_t healthIssueProbability = static_cast<int16_t>(RandomValue::generateRandomInt<uint16_t>(from, to));
    player::HealthStatus healthIssue = player::HealthStatus::UNKNOWN;

    for (auto issue: PlayerHealth::_timeOfAbsenceCategories) {

         if ((healthIssueProbability -= issue.probability()) <= 0)
            { healthIssue = issue.causeOfAbsence(); break; }
    }

    const uint8_t days = RandomValue::generateRandomInt(PlayerHealth::absenceTimePeriod(healthIssue).first,
                                                        PlayerHealth::absenceTimePeriod(healthIssue).second);
    const QDate endDate = (days <= 60) ? currentDate.addDays(days) : QDate();

    PlayerHealth * const newHealthIssue = new PlayerHealth(currentDate, endDate, healthIssue);
    this->_healthStatus_list.push_back(newHealthIssue);

    return;
}

QDate PlayerCondition::addEndDateToHealthIssue(const QDate & currentDate) {

    // search for PlayerHealth with "live" status
    PlayerHealth * const playerHealth = this->liveHealthStatus();

    if (playerHealth == nullptr)
        return QDate();

    const uint8_t numberOfDaysMissedSoFar = playerHealth->statusValidFrom().daysTo(currentDate);
    const uint8_t healthIssueMinDuration = std::max(static_cast<uint8_t>(numberOfDaysMissedSoFar + 1),
                                                    playerHealth->absenceTimePeriod(playerHealth->healthStatus()).first);
    const uint8_t days = RandomValue::generateRandomInt(healthIssueMinDuration,
                         playerHealth->absenceTimePeriod(playerHealth->healthStatus()).second);

    const QDate newEndDate = playerHealth->statusValidFrom().addDays(days);
    playerHealth->setEndDate(newEndDate);

    return newEndDate;
}

void PlayerCondition::invalidateHealthIssue() {

    // search for PlayerHealth with "live" status
    PlayerHealth * const playerHealth = this->liveHealthStatus();

    if (playerHealth != nullptr)
        playerHealth->invalidateRecord();

    return;
}

const QVector<PlayerHealth *> & PlayerCondition::completeHealthStatusHistory(uint16_t & totalNumberOfDays) const {

    for (auto it: _healthStatus_list)
        totalNumberOfDays += it->duration();

    return _healthStatus_list;
}

// original = true: returns original value, original = false (default): returns current value
uint16_t PlayerCondition::getValue(const player::Conditions condition, const bool original) const {

    uint16_t value = 0;

    switch (condition) {

        case player::Conditions::FATIGUE: value = _fatigue; break;
        case player::Conditions::FITNESS: value = _fitness; break;
        case player::Conditions::FORM: value = _form; break;
        case player::Conditions::HEALTH: value = _health; break;
        case player::Conditions::MORALE: value = _morale; break;
        case player::Conditions::OVERALL: value = this->overallCondition(ConditionWeights()); break;
        default: value = 0;
    }

    return (value - ((!original) ? this->_decreaseInCondition.value(condition, 0) : 0));
}

uint16_t PlayerCondition::overallCondition(const ConditionWeights & conditions) const {

    const uint16_t total =
        this->getValue(player::Conditions::FATIGUE) * conditions.conditionWeight(player::Conditions::FATIGUE) +
        this->getValue(player::Conditions::FITNESS) * conditions.conditionWeight(player::Conditions::FITNESS) +
        this->getValue(player::Conditions::FORM) * conditions.conditionWeight(player::Conditions::FORM) +
        this->getValue(player::Conditions::HEALTH) * conditions.conditionWeight(player::Conditions::HEALTH) +
        this->getValue(player::Conditions::MORALE) * conditions.conditionWeight(player::Conditions::MORALE);

    return total;
}

void PlayerCondition::increaseCondition(const player::Conditions condition, const uint8_t value) {

    if (value == 0 || !_decreaseInCondition.contains(condition))
        return;

    _decreaseInCondition[condition] -= std::min<uint8_t>(value, _decreaseInCondition[condition]);

    return;
}

void PlayerCondition::decreaseCondition(const player::Conditions condition, const uint8_t value) {

    if (value == 0 || (_decreaseInCondition.contains(condition) && _decreaseInCondition[condition] == maxValue - minValue))
        return;

    // operator[] updates existing condition/value or constructs (inserts) new condition/value
    _decreaseInCondition[condition] += std::min<uint8_t>(value, maxValue - minValue - _decreaseInCondition[condition]);

    return;
}
