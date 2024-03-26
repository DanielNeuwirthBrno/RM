/*******************************************************************************
 Copyright 2021-22 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include "match/matchtime.h"
#include "shared/constants.h"
#include "shared/html.h"
#include "shared/texts.h"

const QMap<MatchPeriod::TimePeriod, QString> MatchTime::_periodDescriptions {

    { MatchPeriod::TimePeriod::WARM_UP, QStringLiteral("before match (warm-up)") },
    { MatchPeriod::TimePeriod::DRAW, QStringLiteral("coin toss (draw)") },
    { MatchPeriod::TimePeriod::FIRST_HALF_TIME, QStringLiteral("first half-time") },
    { MatchPeriod::TimePeriod::HALF_TIME_INTERVAL,  QStringLiteral("half-time interval") },
    { MatchPeriod::TimePeriod::SECOND_HALF_TIME, QStringLiteral("second half-time") },
    { MatchPeriod::TimePeriod::BEFORE_EXTRA_TIME_INTERVAL, QStringLiteral("before extra-time") },
    { MatchPeriod::TimePeriod::FIRST_EXTRA_TIME, QStringLiteral("first extra-time") },
    { MatchPeriod::TimePeriod::EXTRA_TIME_INTERVAL, QStringLiteral("extra-time interval") },
    { MatchPeriod::TimePeriod::SECOND_EXTRA_TIME, QStringLiteral("second extra-time") },
    { MatchPeriod::TimePeriod::BEFORE_SUDDEN_DEATH_TIME_INTERVAL, QStringLiteral("before sudden-death") },
    { MatchPeriod::TimePeriod::SUDDEN_DEATH_TIME, QStringLiteral("sudden-death-time") },
    { MatchPeriod::TimePeriod::BEFORE_KICKING_INTERVAL, QStringLiteral("before kicking comp.") },
    { MatchPeriod::TimePeriod::KICKING_COMPETITION, QStringLiteral("kicking competition") },
    { MatchPeriod::TimePeriod::FULL_TIME, QStringLiteral("end of match (full-time)") }
};

void MatchTime::switchTimePeriodTo(const MatchPeriod::TimePeriod nextPeriod) {

    _currentTimePeriod = (nextPeriod != MatchPeriod::TimePeriod::UNDETERMINED) ? nextPeriod
                       : static_cast<MatchPeriod::TimePeriod>(static_cast<int8_t>(_currentTimePeriod)+1);

    // add new period to list of played periods
    _timePeriodLengths.insert(_currentTimePeriod, 0);

    return;
}

MatchPeriod::TimePeriod MatchTime::lastPeriodPlayed() const {

    if (_timePeriodLengths.isEmpty())
        return MatchPeriod::TimePeriod::UNDETERMINED;
    if (_timePeriodLengths.size() == 1)
        return _timePeriodLengths.lastKey();

    // return key of the last-but-one record
    return *(--(--_timePeriodLengths.keyEnd()));
}

// needed for calculation of possession time ratio and territory time ratio
uint16_t MatchTime::timePlayedInSecondsRaw() const {

    uint16_t timeInSeconds = 0;
    for (auto period: _timePeriodLengths.values())
        timeInSeconds += period;

    return timeInSeconds;
}

uint16_t MatchTime::timePlayedInSecondsInPeriod() const {

    uint16_t timePlayedInSeconds = this->timePlayedInSeconds();

    switch (_currentTimePeriod) {

        case MatchPeriod::TimePeriod::UNDETERMINED: return 0;
        case MatchPeriod::TimePeriod::BEFORE_EXTRA_TIME_INTERVAL:
        case MatchPeriod::TimePeriod::FIRST_EXTRA_TIME:
        case MatchPeriod::TimePeriod::EXTRA_TIME_INTERVAL:
        case MatchPeriod::TimePeriod::SECOND_EXTRA_TIME: return (timePlayedInSeconds - matchTime.RegularTime*60);
        case MatchPeriod::TimePeriod::BEFORE_SUDDEN_DEATH_TIME_INTERVAL:
        case MatchPeriod::TimePeriod::SUDDEN_DEATH_TIME: return (timePlayedInSeconds - (matchTime.RegularTime+matchTime.ExtraTime)*60);
        default: ;
    }

    return timePlayedInSeconds;
}

QString MatchTime::listOfAllPeriods() const {

    QStringList periodsDescriptionItems;

    for (auto period: this->_timePeriodLengths.keys()) {

        periodsDescriptionItems.append(html_functions.buildBoldText(
            string_functions.wrapInBrackets(this->timePlayed(this->_timePeriodLengths.value(period, 0)), "[]")) +
            this->_periodDescriptions.value(period, QStringLiteral("unknown")));
    }
    const QString periodsDescription = periodsDescriptionItems.join(html_tags.lineBreak);

    return periodsDescription;
}

QString MatchTime::timePlayed(const uint8_t minutes, const uint8_t seconds) const {

    QString timePlayed = QStringLiteral(" 0m:0s");

    const QString minutesPlayed = QString::number(minutes);
    const QString secondsPlayed = QString::number(seconds);

    timePlayed.replace(3-minutesPlayed.length(), minutesPlayed.length(), minutesPlayed);
    timePlayed.replace(6-secondsPlayed.length(), secondsPlayed.length(), secondsPlayed);

    return timePlayed;
}

void MatchTime::addTime(const uint16_t seconds) {

    this->addMinutes(seconds / 60);
    this->addSeconds(seconds % 60);

    // update time played in current time period (as yet)
    if (_timePeriodLengths.contains(_currentTimePeriod))
        _timePeriodLengths[_currentTimePeriod] += seconds;
    else _timePeriodLengths.insert(_currentTimePeriod, seconds);

    return;
}

void MatchTime::setTimeForInterval(const uint16_t seconds) {

    if (_timePeriodLengths.size() > 1) {

        // if time was added to "interval"/"full-time" period move this time chunk to previous "playing" period
        const MatchPeriod::TimePeriod previousPeriod = (_currentTimePeriod == MatchPeriod::TimePeriod::FULL_TIME) ?
            this->lastPeriodPlayed() : static_cast<MatchPeriod::TimePeriod>(static_cast<int8_t>(_currentTimePeriod)-1);
        _timePeriodLengths[previousPeriod] += _timePeriodLengths[_currentTimePeriod];
    }

    _timePeriodLengths[_currentTimePeriod] = seconds;

    return;
}

void MatchTime::addMinutes(const uint8_t minutes) {

    _minutesPlayed += minutes;
    _lastIncrement += minutes;

    return;
}

void MatchTime::addSeconds(const uint8_t seconds) {

    _secondsPlayed += seconds;
    _lastIncrement += _secondsPlayed / 60;

    // if _secondsPlayed >= 60
    _minutesPlayed += _secondsPlayed / 60;
    _secondsPlayed = _secondsPlayed % 60;

    return;
}
