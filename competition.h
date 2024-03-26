/*******************************************************************************
 Copyright 2020-22 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#ifndef COMPETITION_H
#define COMPETITION_H

#include <QDate>
#include <QString>
#include <cstdint>
#include "shared/shared_types.h"
#include "shared/texts.h"

class Competition {

    public:
        explicit Competition(): _period(MatchType::Type::UNDEFINED) {}
        explicit Competition(const uint8_t code, const QString & name, const QString & country,
                             const CompetitionType::Type type, const uint8_t level, const uint8_t numberOfGroups,
                             const bool hasPlayoffs, const QDate & fromDate, const QDate & toDate):
            _period(MatchType::Type::REGULAR), _code(code), _name(name), _country(country), _type(type), _level(level),
            _numberOfGroups(numberOfGroups), _hasPlayoffs(hasPlayoffs), _fromDate(fromDate), _toDate(toDate) {}
        ~Competition() {}

        QString competitionSeasonDescription(const MatchType::Type type = MatchType::Type::UNDEFINED) const {
            const MatchType::Type period = (type == MatchType::Type::UNDEFINED) ? this->_period : type;
            return PartsOfSeason.value(qMakePair<season::ct, season::mt>(this->_type, period), QString());
        }
        QString competitionDescription() const {
            return this->_name + string_functions.wrapInBrackets(this->competitionSeasonDescription());
        }

        bool switchPlayoffsFlag() { return (_hasPlayoffs = !(_hasPlayoffs)); }

        inline uint16_t code() const { return _code; }
        inline QString name() const { return _name; }
        inline CompetitionType::Type type() const { return _type; }

        inline MatchType::Type period() const { return _period; }
        inline MatchType::Type * periodToSwitch() { return &(_period); }

        inline bool hasPlayoffs() const { return _hasPlayoffs; }
        inline QDate fromDate() const { return _fromDate; }

    private:
        MatchType::Type _period;

        uint16_t _code;
        QString _name;
        QString _country;
        CompetitionType::Type _type;
        uint8_t _level;
        uint8_t _numberOfGroups;
        bool _hasPlayoffs;
        QDate _fromDate;
        QDate _toDate;
};

#endif // COMPETITION_H
