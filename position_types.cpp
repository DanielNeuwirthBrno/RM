/*******************************************************************************
 Copyright 2020-21 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include "player/position_types.h"

void PlayerPosition_index::addPlayerPosition(const PlayerPosition_index_item::PositionBaseType baseType,
    const PlayerPosition_index_item::PositionType type, const QString & typeName, const uint8_t no, const QString & name) {

    PlayerPosition_index_item * newItem = new PlayerPosition_index_item(baseType, type, typeName, no, name);
    this->_playerPosition_index.push_back(newItem);

    return;
}

PlayerPosition_index_item::PositionBaseType PlayerPosition_index::findPositionBaseTypeByType(
    const PlayerPosition_index_item::PositionType type) const {

    for (auto position: _playerPosition_index)
        if (position->positionType() == type)
            return (position->positionType(PlayerPosition_index_item::PositionBaseType::UNKNOWN));

    return (PlayerPosition_index_item::PositionBaseType::UNKNOWN);
}

uint8_t PlayerPosition_index::findPlayerPositionCodeByName(const QString & name) const {

    const auto it = std::find_if(_playerPosition_index.begin(), _playerPosition_index.end(),
                                 [&name](PlayerPosition_index_item * const & it) { return it->positionName() == name; });
    return ((it != _playerPosition_index.end()) ? (*it)->positionNo() : 0);
}

PlayerPosition_index_item * PlayerPosition_index::findPlayerPositionByCode(const uint8_t code) const {

    const auto it = std::find_if(_playerPosition_index.begin(), _playerPosition_index.end(),
                                 [&code](PlayerPosition_index_item * const & it) { return it->positionNo() == code; });
    return ((it != _playerPosition_index.end()) ? (*it) : nullptr);
}

template <typename T>
QVector<PlayerPosition_index_item *> PlayerPosition_index::findPlayerPositionsByType(const T type) const {

    QVector<PlayerPosition_index_item *> playerPositions;

    for (auto position: _playerPosition_index) {

        // value of -1 (BaseType::UNKNOWN or Type::NOT_DEFINED) returns all items
        if (position->positionType(type) == type || static_cast<int8_t>(type) == -1)
            playerPositions.push_back(position);
    }

    return playerPositions;
}

// explicit instantiations of findPlayerPositionsByType function
template QVector<PlayerPosition_index_item *>
PlayerPosition_index::findPlayerPositionsByType<PlayerPosition_index_item::PositionType>
(const PlayerPosition_index_item::PositionType) const;

template QVector<PlayerPosition_index_item *>
PlayerPosition_index::findPlayerPositionsByType<PlayerPosition_index_item::PositionBaseType>
(const PlayerPosition_index_item::PositionBaseType) const;
