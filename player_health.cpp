/*******************************************************************************
 Copyright 2021 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include "player/player_health.h"

const QVector<TimeOfAbsence> PlayerHealth::_timeOfAbsenceCategories = {

    { player::HealthStatus::NAUSEA, 1, 3, 200 },
    { player::HealthStatus::SICK, 3, 10, 300 },
    { player::HealthStatus::INJURY, 5, 30, 280 },
    { player::HealthStatus::SERIOUS_INJURY, 30, 90, 200 },
    { player::HealthStatus::MENTAL, 1, 90, 18 },
    { player::HealthStatus::COMA, 45, 90, 2 }
};

PlayerHealth::PlayerHealth(const QDate & from, const QDate & to, const player::HealthStatus currentState, const bool live):
    _statusValidFrom(from), _statusValidTo(to), _healthStatus(currentState), _liveRecord(live) {}

uint16_t PlayerHealth::absenceSumOfProbabilities(const player::HealthStatus sumUpToStatus) {

    uint16_t sum = 0;

    for (auto absence: _timeOfAbsenceCategories) {

        sum += absence.probability();
        if (absence.causeOfAbsence() == sumUpToStatus)
            return sum;
    }
    return sum;
}

QPair<uint8_t, uint8_t> PlayerHealth::absenceTimePeriod(const player::HealthStatus status) {

    for (auto it: _timeOfAbsenceCategories)
        if (it.causeOfAbsence() == status)
            return qMakePair<uint8_t, uint8_t>(it.lowerBound(), it.upperBound());

    return (qMakePair<uint8_t, uint8_t>(0, 0));
}
