/*******************************************************************************
 Copyright 2020-22 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include "settings/config.h"

void Config::saveConfiguration(const uint16_t teamCode, Team * const team, const QString & manager) {

    _teamCode = teamCode;
    _team = team;
    _manager = manager;

    return;
}
