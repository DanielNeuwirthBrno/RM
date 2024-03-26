/*******************************************************************************
 Copyright 2022 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include "match/playoff_rules.h"
#include "shared/shared_types.h"

QString RegularToPlayoffsRule::hosts() const
    { return (_hostsGroup + " (" + string_functions.rankingWithSuffix<uint8_t>(_hostsRanking) + ")"); }

QString RegularToPlayoffsRule::visitors() const
{ return (_visitorsGroup + " (" + string_functions.rankingWithSuffix<uint8_t>(_visitorsRanking) + ")"); }

QPair<QString, uint8_t> RegularToPlayoffsRule::ranking(const MatchType::Location loc) const {

    if (loc == MatchType::Location::HOSTS)
        return qMakePair<QString, uint8_t>(_hostsGroup, _hostsRanking);
    if (loc == MatchType::Location::VISITORS)
        return qMakePair<QString, uint8_t>(_visitorsGroup, _visitorsRanking);

    return QPair<QString, uint8_t>();
}

void RegularToPlayoffsRule::setData(const QString & hostsGroup, const uint8_t hostsRanking,
                                    const QString & visitorsGroup, const uint8_t visitorsRanking) {
    _hostsGroup = hostsGroup;
    _hostsRanking = hostsRanking;
    _visitorsGroup = visitorsGroup;
    _visitorsRanking = visitorsRanking;

    return;
};

QPair<uint32_t, bool> PlayoffsToPlayoffsRule::teamFromMatch(const MatchType::Location loc) const {

    if (loc == MatchType::Location::HOSTS)
        return qMakePair<uint32_t, bool>(_hostsFromMatchNumber, _hostsAreWinnersOfPreviousMatch);
    if (loc == MatchType::Location::VISITORS)
        return qMakePair<uint32_t, bool>(_visitorsFromMatchNumber, _visitorsAreWinnersOfPreviousMatch);

    return QPair<uint32_t, bool>();
}

void PlayoffsToPlayoffsRule::setData(const uint32_t hostsFromMatch, const bool hostsWon,
                                     const uint32_t visitorsFromMatch, const bool visitorsWon) {
    _hostsFromMatchNumber = hostsFromMatch;
    _hostsAreWinnersOfPreviousMatch = hostsWon;
    _visitorsFromMatchNumber = visitorsFromMatch;
    _visitorsAreWinnersOfPreviousMatch = visitorsWon;

    return;
};
