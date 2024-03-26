/*******************************************************************************
 Copyright 2021-22 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include "match/matchscore.h"

const QString MatchScore::unknownValue = QStringLiteral("N/A");
const QString MatchScore::vsSeparator = QStringLiteral(" vs. ");

template<typename T>
T MatchScore::stats(const StatsType::NumberOf type) const {

    switch (type) {

        case StatsType::NumberOf::METRES_RUN: return _distanceByRunning;
        case StatsType::NumberOf::METRES_KICKED: return _distanceByKicking;
        case StatsType::NumberOf::CARRIES: return _carries;
        case StatsType::NumberOf::PENALTIES_CAUSED: return _penaltyInfringements;
        case StatsType::NumberOf::HANDLING_ERRORS: return _handlingErrors;
        case StatsType::NumberOf::OFFLOADS: return _offloads;
        case StatsType::NumberOf::YELLOW_CARDS: return _yellowCards;
        case StatsType::NumberOf::RED_CARDS: return _redCards;
        default: /* other enum values can be ignored */;
    }
    return static_cast<uint8_t>(0);
}

uint8_t MatchScore::points(const PointEvent event) const {

    switch (event) {

        case TRY: return this->_tries;
        case CONVERSION: return this->_conversions;
        case PENALTY: return this->_penalties;
        case DROPGOAL: return this->_dropGoals;
    }
    return 0;
}

uint16_t MatchScore::tackles(const MatchScore::Tackles tackle) const {

    switch (tackle) {

        case Tackles::COMPLETED: return _tacklesCompleted;
        case Tackles::MISSED: return _tacklesMissed;
        case Tackles::ATTEMPTED:
        default: return (_tacklesCompleted + _tacklesMissed);
    }
    return 0;
}

QString MatchScore::tacklesSuccessRate() const {

    if (tackles(MatchScore::Tackles::ATTEMPTED) == 0)
        return MatchScore::unknownValue;

    const QString rate = QString::number(_tacklesCompleted * 100 / static_cast<double>(tackles()), 'f', 2);

    return rate;
}

void MatchScore::tackleAttempted(const MatchScore::Tackles type) {

    if (type == MatchScore::Tackles::COMPLETED)
        ++(_tacklesCompleted);
    if (type == MatchScore::Tackles::MISSED)
        ++(_tacklesMissed);

    return;
}

uint16_t MatchScore::passes(const MatchScore::Passes pass) const {

    switch (pass) {

        case Passes::COMPLETED: return _passesCompleted;
        case Passes::MISSED: return _passesMissed;
        case Passes::ATTEMPTED:
        default: return (_passesCompleted + _passesMissed);
    }
    return 0;
}

QString MatchScore::passesSuccessRate() const {

    if (passes(MatchScore::Passes::ATTEMPTED) == 0)
        return MatchScore::unknownValue;

    const QString rate = QString::number(_passesCompleted * 100 / static_cast<double>(passes()), 'f', 2);

    return rate;
}

void MatchScore::passAttempted(const MatchScore::Passes type) {

    if (type == MatchScore::Passes::COMPLETED)
        ++(_passesCompleted);
    if (type == MatchScore::Passes::MISSED)
        ++(_passesMissed);

    return;
}

uint8_t MatchScore::lineouts(const MatchScore::Lineouts lineout) const {

    switch (lineout) {

        case Lineouts::WON: return _lineoutsWon;
        case Lineouts::LOST: return _lineoutsLost;
        case Lineouts::THROWN:
        default: return (_lineoutsWon + _lineoutsLost);
    }
    return 0;
}

QString MatchScore::lineoutsSuccessRate() const {

    if (lineouts(MatchScore::Lineouts::THROWN) == 0)
        return MatchScore::unknownValue;

    const QString rate = QString::number(_lineoutsWon * 100 / static_cast<double>(lineouts()), 'f', 2);

    return rate;
}

void MatchScore::lineoutThrown(const MatchScore::Lineouts type) {

    if (type == MatchScore::Lineouts::WON)
        ++(_lineoutsWon);
    if (type == MatchScore::Lineouts::LOST)
        ++(_lineoutsLost);

    return;
}

uint8_t MatchScore::scrums(const MatchScore::Scrums scrum) const {

    switch (scrum) {

        case Scrums::WON: return _scrumsWon;
        case Scrums::LOST: return  _scrumsLost;
        case Scrums::THROWN_INTO:
        default: return (_scrumsWon + _scrumsLost);
    }
    return 0;
}

void MatchScore::scrumThrown(const MatchScore::Scrums type) {

    if (type == MatchScore::Scrums::WON)
        ++(_scrumsWon);
    if (type == MatchScore::Scrums::LOST)
        ++(_scrumsLost);
    if (type == MatchScore::Scrums::UNDETERMINED)
        ; // nothing to be done - this is intentional

    return;
}

// explicit instantiation
template uint8_t MatchScore::stats(const StatsType::NumberOf type) const;
template uint16_t MatchScore::stats(const StatsType::NumberOf type) const;
