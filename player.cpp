/*******************************************************************************
 Copyright 2020-21 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include "player/player.h"

const QString Player::preferenceDescription = QStringLiteral("/preferenceDescription");

Player::Player(const PlayerPosition & position, const uint32_t code, const QString & firstName, const QString & lastName,
               const QString & country, const QString & club, const uint16_t caps, const QDate & birthDate,
               const int16_t preferredFor, const uint8_t shirtNo, const uint8_t noOnPitch, const QDate suspendedUntil):
    _code(code), _firstName(firstName), _lastName(lastName), _country(country), _club(club), _caps(caps),
    _birthDate(birthDate), _preferredFor(preferredFor), _shirtNo(shirtNo), _noOnPitch(noOnPitch),
    _lastMatchSentOff(false), _suspended(!suspendedUntil.isNull()), _suspendedUntil(suspendedUntil) {

    _position = new PlayerPosition(position);
    _attributes = new PlayerAttributes();
    _condition = new PlayerCondition();
    _stats = new PlayerStats();
    _points = new PlayerPoints();
}

Player::~Player() {

    delete _points;
    delete _stats;
    delete _condition;
    delete _attributes;
    delete _position;
}

uint8_t Player::age(const QDate & currentDate) const {

    if (!_birthDate.isValid() || _birthDate >= currentDate)
        return 0; // age can't be determined

    uint8_t age = currentDate.year() - _birthDate.year();
    if (currentDate.month() < _birthDate.month() ||
        (currentDate.month() == _birthDate.month() && currentDate.day() < _birthDate.day()))
        --age;

    return age;
}

uint16_t Player::condition(const player::Conditions specificCondition, ConditionThresholds::ConditionValue & severity) const {

    const uint16_t value = this->_condition->getValue(specificCondition);

    severity = static_cast<ConditionThresholds::ConditionValue>(0 +
                   static_cast<uint8_t>(value <= conditionThresholds.lowThreshold(specificCondition)) +
                   static_cast<uint8_t>(value <= conditionThresholds.criticalThreshold(specificCondition))
               );

    return value;
}

QString Player::availability(const player::Conditions specificCondition, const QDate & currentDate) const {

    QString text = QString();

    if (specificCondition == player::Conditions::AVAILABILITY)
        text = player::healthStatusColumnNames[this->condition()->currentState(currentDate)];

    if (specificCondition == player::Conditions::RETURN_DATE) {

        bool unknownRecoveryDate = false;
        const QDate recoveryDate = this->condition()->dateOfRecovery(unknownRecoveryDate, currentDate);

        const int16_t noOfDaysBeforeReturn = (recoveryDate.isValid()) ? currentDate.daysTo(recoveryDate) : -1;

        if (noOfDaysBeforeReturn == -1 && unknownRecoveryDate)
            text = QStringLiteral("unknown");
        else if (noOfDaysBeforeReturn == 0)
            text = QStringLiteral("this day");
        else if (noOfDaysBeforeReturn > 0)
            text = QString::number(noOfDaysBeforeReturn) + " more day(s)";
    }

    return text;
}

bool Player::isCaptain() const {

    const player::PreferredForAction isCaptain =
        static_cast<player::PreferredForAction>(this->_preferredFor/std::abs(this->_preferredFor));

    return (isCaptain == player::PreferredForAction::CAPTAIN);
}

void Player::resetAllPreferences() {

    this->_preferredFor = (this->isCaptain()) ? static_cast<int8_t>(player::PreferredForAction::CAPTAIN)
                                              : static_cast<int8_t>(player::PreferredForAction::NO_ACTION);
    return;
}

// transfers preferences from one player (this) to another player (toPlayer)
void Player::transferPreferences(Player * const toPlayer, const player::PreferredForAction lowerBound,
                                 const player::PreferredForAction upperBound, const bool removeFromFromPlayer) {

    const int8_t from = static_cast<int8_t>(lowerBound);
    const int8_t to = static_cast<int8_t>(upperBound);

    for (int8_t curr = from; curr <= to; ++curr) {

        if (!player::preferenceColumnNames.contains(static_cast<player::PreferredForAction>(curr)))
            continue;

        const auto preference = static_cast<player::PreferredForAction>(curr);

        if (this->isPreferredFor(preference)) {

            toPlayer->setAsPreferredFor(preference);

            // warning: if removeFromFromPlayer == false preferences are not removed from the "original" player; this
            // might lead to a situation where more than 3 players have certain preference set (which is not allowed
            // but there is no check for that)
            if (removeFromFromPlayer)
                this->setAsPreferredFor(preference, false);
        }
    }
    return;
}

void Player::setAsPreferredFor(const player::PreferredForAction action, const bool addPreference) {

    if (action == player::PreferredForAction::NO_ACTION)
        this->resetAllPreferences();

    // no change necessary
    if ((this->isPreferredFor(action) && addPreference) || (!this->isPreferredFor(action) && !addPreference))
        return;

     this->_preferredFor = (addPreference) ? this->_preferredFor * static_cast<int8_t>(action)
                                           : this->_preferredFor / static_cast<int8_t>(action);
    return;
}

bool Player::isPreferredFor(const player::PreferredForAction preference) const {

    if (preference == player::PreferredForAction::CAPTAIN)
        return this->isCaptain();

    const int8_t currentPreferenceNumberValue = static_cast<int8_t>(preference);

    return (this->_preferredFor % currentPreferenceNumberValue == 0);
}
