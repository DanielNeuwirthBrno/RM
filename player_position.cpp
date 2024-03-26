/*******************************************************************************
 Copyright 2020-21 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include "player/player_position.h"

const QString PlayerPosition::notAssigned = QStringLiteral("<not assigned>");

uint8_t PlayerPosition::currentPositionNo() const {

    if (_playerPosition != nullptr)
        return _playerPosition->positionNo();
    else
        return 0;
}

QString PlayerPosition::currentPosition() const {

    if (_playerPosition != nullptr)
        return _playerPosition->positionName();
    else
        return notAssigned;
}
