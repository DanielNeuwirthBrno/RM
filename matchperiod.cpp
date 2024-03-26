/*******************************************************************************
 Copyright 2022 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include "match/matchperiod.h"
#include "shared/constants.h"
#include "ui/shared/stylesheets.h"

MatchPeriod::MatchPeriod():
    _period(MatchPeriod::TimePeriod::UNDETERMINED), _periodType(MatchPeriod::TimePeriodType::UNDEFINED),
    _length(0), _timePlayed(0), _messageBoxDefinition(QString()), _matchContinuesIf(
    QPair<MatchType::Type, TeamResults::ResultType>(MatchType::Type::UNDEFINED, TeamResults::ResultType::DRAW)),
    _progressConfig(QPair<uint8_t, QVector<QString>>(0, ss::fixtureswidget.RegularTimeProgressBarStyle)),
    _description(QString()) {}

MatchPeriod::MatchPeriod(const MatchPeriod::TimePeriod period, const MatchPeriod::TimePeriodType periodType,
                         const uint8_t length, const uint8_t timePlayed, const QString & mbDef,
                         const QPair<MatchType::Type, TeamResults::ResultType> & matchContinuesIf,
                         const QPair<uint8_t, QVector<QString>> progressConfig, const QString & description):
    _period(period), _periodType(periodType), _length(length), _timePlayed(timePlayed), _messageBoxDefinition(mbDef),
    _matchContinuesIf(matchContinuesIf), _progressConfig(progressConfig), _description(description) {}

bool MatchPeriod::isPlayingTime() const {

    return (_periodType == TimePeriodType::REGULAR_TIME_PERIOD ||
            _periodType == TimePeriodType::EXTRA_TIME_PERIOD ||
            _periodType == TimePeriodType::SUDDEN_DEATH_TIME_PERIOD);
}

MatchPeriods::MatchPeriods(): _doNotStopAt_diag(MatchPeriod::TimePeriod::UNDETERMINED) {

    const uint8_t noLength = 0;
    const QPair<MatchType::Type, TeamResults::ResultType> notApplicable =
        qMakePair<MatchType::Type, TeamResults::ResultType>(MatchType::Type::UNDEFINED, TeamResults::ResultType::DRAW);
    const QPair<MatchType::Type, TeamResults::ResultType> matchContinuesIf =
        qMakePair<MatchType::Type, TeamResults::ResultType>(MatchType::Type::PLAYOFFS, TeamResults::ResultType::DRAW);

    _periods.insert(MatchPeriod::TimePeriod::WARM_UP,
        MatchPeriod { MatchPeriod::TimePeriod::WARM_UP,
                      MatchPeriod::TimePeriodType::OTHER_PERIOD,
                      noLength, noLength, QString(), notApplicable,
                      qMakePair<uint8_t, QVector<QString>>(0, ss::fixtureswidget.RegularTimeProgressBarStyle),
                      MatchPeriods::description()
                    } );

    _periods.insert(MatchPeriod::TimePeriod::DRAW,
        MatchPeriod { MatchPeriod::TimePeriod::DRAW,
                      MatchPeriod::TimePeriodType::INTERVAL_PERIOD,
                      noLength, noLength,
                      QStringLiteral("beforeStartOfMatch"), notApplicable,
                      qMakePair<uint8_t, QVector<QString>>(matchTime.RegularTime, ss::fixtureswidget.RegularTimeProgressBarStyle),
                      QStringLiteral("draw in progress")
                    } );

    _periods.insert(MatchPeriod::TimePeriod::FIRST_HALF_TIME,
        MatchPeriod { MatchPeriod::TimePeriod::FIRST_HALF_TIME,
                      MatchPeriod::TimePeriodType::REGULAR_TIME_PERIOD,
                      matchTime.HalfTime, matchTime.HalfTime,
                      QString(), notApplicable,
                      qMakePair<uint8_t, QVector<QString>>(matchTime.RegularTime, ss::fixtureswidget.RegularTimeProgressBarStyle),
                      QStringLiteral("first half-time in progress")
                    } );

    _periods.insert(MatchPeriod::TimePeriod::HALF_TIME_INTERVAL,
        MatchPeriod { MatchPeriod::TimePeriod::HALF_TIME_INTERVAL,
                      MatchPeriod::TimePeriodType::INTERVAL_PERIOD,
                      matchTime.HalfTimeInterval, matchTime.HalfTime,
                      QStringLiteral("endOfFirstHalf"), notApplicable,
                      qMakePair<uint8_t, QVector<QString>>(matchTime.RegularTime, ss::fixtureswidget.RegularTimeProgressBarStyle),
                      QStringLiteral("half-time interval")
                    } );

    _periods.insert(MatchPeriod::TimePeriod::SECOND_HALF_TIME,
        MatchPeriod { MatchPeriod::TimePeriod::SECOND_HALF_TIME,
                      MatchPeriod::TimePeriodType::REGULAR_TIME_PERIOD,
                      matchTime.HalfTime, matchTime.RegularTime,
                      QString(), matchContinuesIf,
                      qMakePair<uint8_t, QVector<QString>>(matchTime.RegularTime, ss::fixtureswidget.RegularTimeProgressBarStyle),
                      QStringLiteral("second half-time in progress")
                    } );

    _periods.insert(MatchPeriod::TimePeriod::BEFORE_EXTRA_TIME_INTERVAL,
        MatchPeriod { MatchPeriod::TimePeriod::BEFORE_EXTRA_TIME_INTERVAL,
                      MatchPeriod::TimePeriodType::INTERVAL_PERIOD,
                      matchTime.ExtraTimeInterval, matchTime.RegularTime,
                      QStringLiteral("endOfSecondHalf"), notApplicable,
                      qMakePair<uint8_t, QVector<QString>>(matchTime.ExtraTime, ss::fixtureswidget.ExtraTimeProgressBarStyle),
                      QStringLiteral("before extra-time")
                    } );

    _periods.insert(MatchPeriod::TimePeriod::FIRST_EXTRA_TIME,
        MatchPeriod { MatchPeriod::TimePeriod::FIRST_EXTRA_TIME,
                      MatchPeriod::TimePeriodType::EXTRA_TIME_PERIOD,
                      matchTime.ExtraTimePeriod, matchTime.RegularTime+matchTime.ExtraTimePeriod,
                      QString(), notApplicable,
                      qMakePair<uint8_t, QVector<QString>>(matchTime.ExtraTime, ss::fixtureswidget.ExtraTimeProgressBarStyle),
                      QStringLiteral("first extra-time period in progress")
                    } );

    _periods.insert(MatchPeriod::TimePeriod::EXTRA_TIME_INTERVAL,
        MatchPeriod { MatchPeriod::TimePeriod::EXTRA_TIME_INTERVAL,
                      MatchPeriod::TimePeriodType::INTERVAL_PERIOD,
                      matchTime.ExtraTimeInterval, matchTime.RegularTime+matchTime.ExtraTimePeriod,
                      QStringLiteral("endOfFirstExtraTime"), notApplicable,
                      qMakePair<uint8_t, QVector<QString>>(matchTime.ExtraTime, ss::fixtureswidget.ExtraTimeProgressBarStyle),
                      QStringLiteral("extra-time interval")
                    } );

    _periods.insert(MatchPeriod::TimePeriod::SECOND_EXTRA_TIME,
        MatchPeriod { MatchPeriod::TimePeriod::SECOND_EXTRA_TIME,
                      MatchPeriod::TimePeriodType::EXTRA_TIME_PERIOD,
                      matchTime.ExtraTimePeriod, matchTime.RegularTime+matchTime.ExtraTime,
                      QString(), matchContinuesIf,
                      qMakePair<uint8_t, QVector<QString>>(matchTime.ExtraTime, ss::fixtureswidget.ExtraTimeProgressBarStyle),
                      QStringLiteral("second extra-time period in progress")
                    } );

    _periods.insert(MatchPeriod::TimePeriod::BEFORE_SUDDEN_DEATH_TIME_INTERVAL,
         MatchPeriod { MatchPeriod::TimePeriod::BEFORE_SUDDEN_DEATH_TIME_INTERVAL,
                       MatchPeriod::TimePeriodType::INTERVAL_PERIOD,
                       matchTime.ExtraTimeInterval,matchTime.RegularTime+matchTime.ExtraTime,
                       QStringLiteral("endOfSecondExtraTime"), notApplicable,
                       qMakePair<uint8_t, QVector<QString>>(matchTime.SuddenDeathTime, ss::fixtureswidget.SuddenDeathProgressBarStyle),
                       QStringLiteral("before sudden-death interval")
                    } );

    _periods.insert(MatchPeriod::TimePeriod::SUDDEN_DEATH_TIME,
         MatchPeriod { MatchPeriod::TimePeriod::SUDDEN_DEATH_TIME,
                       MatchPeriod::TimePeriodType::SUDDEN_DEATH_TIME_PERIOD,
                       matchTime.SuddenDeathTime, matchTime.RegularTime+matchTime.ExtraTime+matchTime.SuddenDeathTime,
                       QString(), matchContinuesIf,
                       qMakePair<uint8_t, QVector<QString>>(matchTime.SuddenDeathTime, ss::fixtureswidget.SuddenDeathProgressBarStyle),
                       QStringLiteral("sudden-death period in progress")
                    } );

    _periods.insert(MatchPeriod::TimePeriod::BEFORE_KICKING_INTERVAL,
         MatchPeriod { MatchPeriod::TimePeriod::BEFORE_KICKING_INTERVAL,
                       MatchPeriod::TimePeriodType::INTERVAL_PERIOD,
                       noLength, matchTime.RegularTime+matchTime.ExtraTime+matchTime.SuddenDeathTime,
                       QStringLiteral("endOfSuddenDeathTime"), notApplicable,
                       qMakePair<uint8_t, QVector<QString>>(1, ss::fixtureswidget.SuddenDeathProgressBarStyle),
                       QStringLiteral("kicking competition in preparation")
                    } );

    _periods.insert(MatchPeriod::TimePeriod::KICKING_COMPETITION,
         MatchPeriod { MatchPeriod::TimePeriod::KICKING_COMPETITION,
                       MatchPeriod::TimePeriodType::OTHER_PERIOD,
                       static_cast<uint8_t>(5), matchTime.RegularTime+matchTime.ExtraTime+matchTime.SuddenDeathTime,
                       QString(), notApplicable,
                       qMakePair<uint8_t, QVector<QString>>(0, ss::fixtureswidget.SuddenDeathProgressBarStyle),
                       QStringLiteral("kicking competition in progress")
                    } );

    _periods.insert(MatchPeriod::TimePeriod::FULL_TIME,
         MatchPeriod { MatchPeriod::TimePeriod::FULL_TIME,
                       MatchPeriod::TimePeriodType::OTHER_PERIOD,
                       static_cast<uint8_t>(3), UINT8_MAX,
                       QStringLiteral("endOfMatch"), notApplicable,
                       qMakePair<uint8_t, QVector<QString>>(0, ss::fixtureswidget.RegularTimeProgressBarStyle),
                       QStringLiteral("end of match (full-time)")
                    } );
}

bool MatchPeriods::matchEnds(const MatchPeriod::TimePeriod period, const MatchType::Type type, const TeamResults::ResultType result) const {

    return (matchType(period) != MatchType::Type::UNDEFINED && this->_doNotStopAt_diag <= period &&
            (matchType(period) != type || resultType(period) != result));
};
