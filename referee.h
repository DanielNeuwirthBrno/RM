/*******************************************************************************
 Copyright 2020-22 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#ifndef REFEREE_H
#define REFEREE_H

#include <QString>
#include "shared/texts.h"

class Referee {

    public:
        Referee() = delete;
        Referee(const uint16_t code, const QString & firstName, const QString & lastName,
                const QString & country, const bool eligibleForDraw = false):
            _code(code), _firstName(firstName), _lastName(lastName), _country(country), _inDrawPool(eligibleForDraw) {}
        ~Referee() {}

        inline uint16_t code() const { return _code; }
        inline QString firstName() const { return _firstName; }
        inline QString lastName() const { return _lastName; }
        inline QString country() const { return _country; }
        inline bool inDrawPool() const { return _inDrawPool; }

        QString referee() const {

            const QString country = (!this->_country.isEmpty())
                ? QChar(32) + string_functions.wrapInBrackets(this->_country, QString(), false) : QString();
            return (this->_firstName % QChar(32) % this->_lastName % country);
        }

    private:
        uint16_t _code;
        QString _firstName;
        QString _lastName;
        QString _country;
        bool _inDrawPool;
};

#endif // REFEREE_H
