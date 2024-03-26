/*******************************************************************************
 Copyright 2022 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include <QMap>
#include "match/playoffs.h"
#include "match/playoff_rules.h"
#include "shared/sort.h"

QVector<Match *>::iterator Playoffs::from_match(const MatchType::ToPlayOff playOffType) const {

    return (std::find_if(_fixtures->begin(), _fixtures->end(), [playOffType](Match * const match) -> bool
    { return !match->played() && match->type() == MatchType::Type::PLAYOFFS && match->playoffsType() == playOffType; }));
}

bool Playoffs::drawPlayoffs(const QVector<Team *> & teams, QVector<Match *>::iterator * fromMatch) const {

    uint8_t teamsAssignedToPlayoffMatches = 0;
    QMap<QString, QVector<Team *>> teamsInGroups;

    auto beginFromMatch = this->from_match(MatchType::ToPlayOff::FROM_REGULAR);
    if (fromMatch != nullptr)
        *(fromMatch) = beginFromMatch;

    for (auto it = beginFromMatch; it < _fixtures->end(); ++it) {

        Match * match = (*it);
        if (match->type() != MatchType::Type::PLAYOFFS)
            continue;

        if (match->playoffsType() == MatchType::ToPlayOff::FROM_REGULAR) {

            RegularToPlayoffsRule * rule = reinterpret_cast<RegularToPlayoffsRule *>(match->playoffsRule());

            for (uint8_t i = 0; i < 2; ++i) {

                const MatchType::Location loc = static_cast<MatchType::Location>(i);
                const QString & group = rule->ranking(loc).first;

                if (match->team(loc) == nullptr) {

                    QVector<Team *> & teamsInGroup = teamsInGroups[group]; // if current group is missing => create it
                    const uint8_t & ranking = rule->ranking(loc).second - 1;

                    if (teamsInGroup.isEmpty()) {

                        for (auto team: teams)
                            if (team->group() == group)
                                teamsInGroup.push_back(team);
                        std::sort(teamsInGroup.begin(), teamsInGroup.end(), sortTable);
                    }
                    teamsAssignedToPlayoffMatches += static_cast<uint8_t>(match->setTeam(loc, teamsInGroup[ranking]));
                }
            }
        }
    }
    return static_cast<bool>(teamsAssignedToPlayoffMatches);
}

void Playoffs::assignTeamsForPlayoffsMatches(QVector<Match *>::iterator * fromMatch) const {

    auto beginFromMatch = this->from_match(MatchType::ToPlayOff::FROM_PLAYOFFS);
    if (fromMatch != nullptr)
        *(fromMatch) = beginFromMatch;

    if (beginFromMatch != _fixtures->end()) {

        for (auto match = beginFromMatch; match < _fixtures->end(); ++match) {

            const PlayoffsToPlayoffsRule * const rule = reinterpret_cast<PlayoffsToPlayoffsRule *>((*match)->playoffsRule());

            for (uint8_t i = 0; i < 2; ++i) {

                const MatchType::Location loc = static_cast<MatchType::Location>(i);

                Match * const * sourceMatch = std::find_if(_fixtures->cbegin(), _fixtures->cend(),
                    [&rule, loc](Match * const it) -> bool { return it->code() == rule->teamFromMatch(loc).first; });
                TeamResults::ResultType result = static_cast<TeamResults::ResultType>(rule->teamFromMatch(loc).second);

                if ((*sourceMatch)->winner(result) != nullptr)
                    (*match)->setTeam(loc, (*sourceMatch)->winner(result));
            }
        }
    }
    return;
}
