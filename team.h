/*******************************************************************************
 Copyright 2020-23 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#ifndef TEAM_H
#define TEAM_H

#include <QString>
#include <QVector>
#include <cstdint>
#include "match/matchscore.h"
#include "player/player.h"

class TeamPoints {

    public:
        TeamPoints() {}
        ~TeamPoints() {}

        inline uint16_t tries() const { return _tries; }
        inline uint16_t points() const {

            return (this->_tries * pointValue.Try + this->_conversions * pointValue.Conversion +
                    this->_penalties * pointValue.Penalty + this->_dropgoals * pointValue.DropGoal);
        }

        inline uint16_t pointsConceded() const { return _pointsAgainst; }
        inline uint16_t triesConceded() const { return  _triesAgainst; }

        inline int16_t pointDifference() const
            { return (static_cast<int16_t>(this->points()) - static_cast<int16_t>(this->_pointsAgainst)); }
        inline int16_t tryDifference() const
            { return (static_cast<int16_t>(this->tries()) - static_cast<int16_t>(this->_triesAgainst)); }

        void updateFromMatchScore(MatchScore * const, const uint16_t, const uint8_t);

    private:
        uint16_t _pointsAgainst = 0;
        uint16_t _triesAgainst = 0;

        uint16_t _tries = 0;
        uint16_t _conversions = 0;
        uint16_t _penalties = 0;
        uint16_t _dropgoals = 0;
};

class TeamResults {

    public:
        // do not change the assigned values; program logic relies on these specific values in a number of places when casting
        enum class ResultType: uint8_t { LOSS = 0, WIN = 1, DRAW = 2 };

        TeamResults() {}
        ~TeamResults() {}

        void updateResults(const ResultType, const bool, const bool = false);

        inline uint8_t wins() const { return (_winsWithBonusPoint + _winsWithoutBonusPoint); }
        inline uint8_t draws() const { return (_drawsWithBonusPoint + _drawsWithoutBonusPoint); }
        inline uint8_t losses() const { return (_lossesWithBothPonusPoints + _lossesWithTriesBonusPoint +
                                                _lossesWithDiffBonusPoint + _lossesWithoutBonusPoints); }

        inline uint8_t tryBonusPoint() const { return (_winsWithBonusPoint + _drawsWithBonusPoint +
                                                      _lossesWithBothPonusPoints + _lossesWithTriesBonusPoint); }
        inline uint8_t diffBonusPoint() const { return (_lossesWithBothPonusPoints + _lossesWithDiffBonusPoint); }

        inline uint8_t matchesPlayed() const { return (this->wins() + this->draws() + this->losses()); };

        uint16_t pointsTotal() const;

    private:
        uint8_t _winsWithBonusPoint = 0;
        uint8_t _winsWithoutBonusPoint = 0;
        uint8_t _drawsWithBonusPoint = 0;
        uint8_t _drawsWithoutBonusPoint = 0;
        uint8_t _lossesWithBothPonusPoints = 0;
        uint8_t _lossesWithTriesBonusPoint = 0;
        uint8_t _lossesWithDiffBonusPoint = 0;
        uint8_t _lossesWithoutBonusPoints = 0;
};

class Team {

    public:
        enum TeamType { UNKNOWN = -1, CLUB = 0, NATIONAL = 1 };

        Team() = delete;
        Team(const uint16_t, const QString &, const QString &, const QString &, const QString &, const QString &,
             const QString &, const TeamType, const QString &, const uint8_t, const QString &, const QString &);
        ~Team() {}

        inline TeamPoints & scoredPoints() { return _scoredPoints; }
        inline TeamResults & results() { return _results; }
        inline bool inPlayoffs() const { return _inPlayoffs; }

        inline QVector<Player *> squad() const { return _squad; }
        inline QVector<Player *> & squad() { return _squad; }
        uint8_t availablePlayers(const PlayerPosition_index_item::PositionType, Player * const, QVector<Player *> &);

        QString teamName(Player * const player) const
            { return (this->type() == Team::TeamType::CLUB) ? player->club() : player->country(); }

        inline uint16_t code() const { return _code; }
        inline QString name() const { return _name; }
        inline QString abbr() const { return _abbr; }
        inline QString country() const { return _country; }
        inline QString venue() const { return _venue; }
        inline TeamType type() const { return _type; }
        inline QString manager() const { return _manager; }
        inline uint8_t ranking() const { return _ranking; }
        inline QString group() const { return _group; }
        inline QString colour() const { return _colour; }

        inline void toPlayoffs(const bool progressed) { _inPlayoffs = progressed; return; };
        inline void addPlayer(Player * const player) { _squad.push_back(player); return; }

        bool areAllPlayersSelected() const;
        bool selectPlayersForNextMatch(const ConditionWeights &);
        bool selectSubstitutes(const ConditionWeights &);

        void cleanPitch();

        uint8_t numberOfPlayersOnPitch() const;
        uint16_t packWeight(bool * = nullptr) const;

    private:
        TeamPoints _scoredPoints;
        TeamResults _results;
        bool _inPlayoffs;

        QVector<Player *> _squad;

        uint16_t _code;
        QString _name;
        QString _abbr;
        QString _nick;
        QString _country;
        QString _city;

        QString _venue;
        TeamType _type;
        QString _manager;

        uint8_t _ranking;
        QString _group;
        QString _colour;
};

#endif // TEAM_H
