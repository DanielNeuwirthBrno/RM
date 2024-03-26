/*******************************************************************************
 Copyright 2020-22 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include <QMessageBox>
#include <QObject>
#include <QProgressBar>
#include <QString>
#include "player/player_position.h"
#include "squadwidget.h"
#include "shared/messages.h"
#include "ui/custom/ui_inputdialog.h"

SquadWidget::SquadWidget(QWidget * parent, const QDate & currentDate, Team * const team, const ConditionWeights & conditionSettings):
    QWidget(parent), ui(new Ui_SquadWidget), _myTeam(team), _benchSelection(false), _conditionSettings(conditionSettings),
    _players(team->squad()), _currentPlayer(nullptr) {

    this->setObjectName(on::widgets["squad"]);

    ui->setupUi(this, currentDate, _players, team->type());

    if (_myTeam->areAllPlayersSelected())
        this->displayPlayerAttributesAverages();

    for (auto row: *ui->fields) {

        QCheckBox * const selectedForNextMatchCheckbox =
            this->findFieldInRow<QCheckBox *>(row, on::squadwidget.selectedForNextMatch);
        connect(selectedForNextMatchCheckbox, &QCheckBox::stateChanged, this, &SquadWidget::selectIntoSquad);

        ClickableLabel * const lastNameLabel =
            this->findFieldInRow<ClickableLabel *>(row, on::widgets_shared.lastName);
        connect(lastNameLabel, &ClickableLabel::leftClicked, this, &SquadWidget::displayPlayerAttributes);

        ClickableLabel * const currentPositionLabel =
            this->findFieldInRow<ClickableLabel *>(row, on::squadwidget.currentPosition);
        connect(currentPositionLabel, &ClickableLabel::textChanged, this, &SquadWidget::selectIntoSquad);
        connect(currentPositionLabel, &ClickableLabel::leftClicked, this, &SquadWidget::changeCurrentPositionStandard);
    }

    const int8_t from = static_cast<int8_t>(player::PreferredForAction::CAPTAIN);
    const int8_t max = static_cast<int8_t>(player::PreferredForAction::MAX_VALUE);
    for (int8_t curr = from; curr < max; ++curr) {

        if (!player::preferenceColumnNames.contains(static_cast<player::PreferredForAction>(curr)))
            continue;

        const auto preference = static_cast<player::PreferredForAction>(curr);
        const QString preferenceName = player::preferenceColumnNames[preference].trimmed().replace(" ", "_");
        const QString checkBoxObjectName = preferenceName + on::squadwidget.preferenceState;
        QCheckBox * const currentPreferenceStateCheckBox = parent->findChild<QCheckBox *>(checkBoxObjectName);

        connect(currentPreferenceStateCheckBox, &QCheckBox::clicked, this, &SquadWidget::changePlayerPreference);
    }

    connect(ui->switchToBenchButton, &QPushButton::toggled, this,
            [this](bool checked) -> void { _benchSelection = checked; return; } );
    connect(ui->resetSelectionButton, &QPushButton::clicked, this, &SquadWidget::resetSelectionProperties);
    connect(ui->clearSelectionButton, &QPushButton::clicked, this, &SquadWidget::setSelectionProperties);
    connect(ui->automaticSelectionButton, &QPushButton::clicked, this, &SquadWidget::automaticSelection);
}

Player * SquadWidget::findPlayerByCode(const uint32_t code) {

    const auto it = std::find_if(this->_players.begin(), this->_players.end(),
                                 [&code](Player * const & it) { return it->code() == code; });
    return ((it != this->_players.end()) ? (*it) : nullptr);
}

void SquadWidget::displayPlayerAttributesAverages() const {

    ui->currentPlayerNameLabel->setText(ui->attrAvgDesc);

    QMap<player::Attributes, uint16_t> attributeSums;
    for (uint8_t i = 0; i < static_cast<uint8_t>(player::Attributes::TOTAL_NUMBER); ++i) {

        uint8_t noOfPlayers = 0;
        const player::Attributes currentAttribute = static_cast<player::Attributes>(i);
        attributeSums.insert(currentAttribute, 0);

        for (auto player: _players)
            if (player->isBasePlayer()) {

                if (player->attribute(currentAttribute) > 0)
                    attributeSums[currentAttribute] += player->attribute(currentAttribute);
                else break;
                ++noOfPlayers;
            }

        QLabel * const attributeValueLabel = ui->currentPlayerAttributeLabels.at(i).attributeValueLabel;
        QProgressBar * const attributeValueProgressBar = ui->currentPlayerAttributeLabels.at(i).attributeValueProgressBar;

        if (noOfPlayers == numberOfPlayers.PlayersOnPitch) {

            const double attributeAverageValue = attributeSums[currentAttribute] / (noOfPlayers * 1.0);
            const QString attributeValueText = QString::number(attributeAverageValue, 'f', 2);
            attributeValueLabel->setText(attributeValueText);
            if (PlayerAttributes::isSkill(currentAttribute))
                attributeValueProgressBar->setValue(attributeAverageValue);
        }
        else
            attributeValueLabel->setText(PlayerAttributes::unknownValue);

        attributeValueProgressBar->setVisible(noOfPlayers == numberOfPlayers.PlayersOnPitch &&
                                              PlayerAttributes::isSkill(currentAttribute));
    }
    ui->preferenceHeaderLabel->setVisible(false);
    ui->currentPlayerPreferenceWidget->setVisible(false);

    return;
}

void SquadWidget::updateElementsAfterPositionChanged(ClickableLabel * const currentPositionLabel,
    Player * const currentPlayer, PlayerPosition_index_item * const position) {

    // reset preference for scrum
    if (currentPlayer->position()->currentPositionNo() == 9 && position->positionNo() != 9)
        currentPlayer->setAsPreferredFor(player::PreferredForAction::SCRUM, false);

    // save value (new position)
    currentPlayer->assignCurrentPosition(position);

    // refresh attributes/preference panel
    if (currentPlayer->fullName() == ui->currentPlayerNameLabel->text())
        this->findWidgetBySourceWidget<ClickableLabel *, ClickableLabel *>(currentPositionLabel, on::widgets_shared.lastName)->leftClicked();

    return;
}

// [slot]
void SquadWidget::selectIntoSquad(const int state) {

    // signal emitted by QCheckBox (emitted by Qt)
    QCheckBox * senderCheckBox = qobject_cast<QCheckBox *>(sender());

    bool withoutAssignedPosition = false;
    if (senderCheckBox != nullptr) {

        // players without assigned position can't be selected
        ClickableLabel * const currentPosition = this->findWidgetBySourceWidget<ClickableLabel *, QCheckBox *>
            (senderCheckBox, on::squadwidget.currentPosition);

        if (currentPosition->text() == PlayerPosition::notAssigned) {

            withoutAssignedPosition = true;

            senderCheckBox->setCheckState(Qt::Unchecked);
            // if selectIntoSquad() has been called as part of ResetSelectionProperties procedure
            // player's position might have been returned back to notAssigned state => in such a
            // case it's not possible to return immediately; shirtNo has to be cleared first which
            // is done down below: see comment "taken out of squad"
            // [return;]
        }
    }

    // signal emitted by ClickableLabel (emitted by "hand" after player position change)
    // note: player's position is sure to be assigned if this signal has been called
    if (senderCheckBox == nullptr) {

        ClickableLabel * const senderClickableLabel = qobject_cast<ClickableLabel *>(sender());
        senderCheckBox = this->findWidgetBySourceWidget<QCheckBox *, ClickableLabel *>
            (senderClickableLabel, on::squadwidget.selectedForNextMatch);
    }

    // we must iterate over all rows because we don't know which row's checkbox has been used
    for (auto row: *ui->fields) {

        QCheckBox * const selectedForNextMatchCheckbox =
            this->findFieldInRow<QCheckBox *>(row, on::squadwidget.selectedForNextMatch);

        // if widget (checkbox) which sent currently processed signal is in this row
        if (senderCheckBox == selectedForNextMatchCheckbox) {

            const uint32_t code = this->findFieldInRow<HiddenLabel *>(row, on::widgets_shared.playerCodeHidden)->text().toUInt();

            if (state == Qt::Checked && !withoutAssignedPosition) { // selected into squad

                if (_benchSelection) { // replacement players

                    const uint8_t noOfSubstitutes = std::count_if(_myTeam->squad().cbegin(), _myTeam->squad().cend(),
                                                    [](Player * const player) { return player->isBenchPlayer(); });

                    Player * player = this->findPlayerByCode(code);
                    const uint8_t shirtNumberForSub = (player->isBenchPlayer())
                          ? player->shirtNo() : noOfSubstitutes + numberOfPlayers.PlayersOnPitch + 1;

                    if (shirtNumberForSub > numberOfPlayers.PlayersInSquad) {

                        senderCheckBox->setCheckState(Qt::Unchecked);
                        return; // bench is full
                    }

                    // display label
                    QLabel * const shirtNoLabel = this->findFieldInRow<QLabel *>(row, on::squadwidget.shirtNo);
                    shirtNoLabel->setText(QString::number(shirtNumberForSub));
                    shirtNoLabel->setStyleSheet(ui->shirtNoStyleSheet(!_benchSelection));
                    shirtNoLabel->setVisible(true);
                    // save value
                    player->assignShirtNo(shirtNoLabel->text().toUInt());
                }

                if (!_benchSelection) { // base players

                    const QString currentPosition = this->findFieldInRow<QLabel *>(row, on::squadwidget.currentPosition)->text();

                    for (auto row_other_player: *ui->fields) {

                        // if other player is already selected for this position, kick him out :-)
                        QCheckBox * const otherPlayerSelectedCheckbox =
                            this->findFieldInRow<QCheckBox *>(row_other_player, on::squadwidget.selectedForNextMatch);

                        if (this->findFieldInRow<QLabel *>(row_other_player, on::squadwidget.currentPosition)->text() == currentPosition &&
                            row_other_player != row && otherPlayerSelectedCheckbox->checkState() == Qt::Checked) {

                            otherPlayerSelectedCheckbox->setCheckState(Qt::Unchecked);
                            break; // there should by only one such player
                        }
                    }

                    uint8_t playerPositionCode = 0;
                    if ((playerPositionCode = playerPosition_index.findPlayerPositionCodeByName(currentPosition)) != 0) {

                        // display label
                        QLabel * const shirtNoLabel = this->findFieldInRow<QLabel *>(row, on::squadwidget.shirtNo);
                        shirtNoLabel->setText(QString::number(playerPositionCode));
                        shirtNoLabel->setStyleSheet(ui->shirtNoStyleSheet(!_benchSelection));
                        shirtNoLabel->setVisible(true);
                        // save value
                        this->findPlayerByCode(code)->assignShirtNo(shirtNoLabel->text().toUInt());
                    }
                }
            }

            if (state == Qt::Unchecked)  { // taken out of squad

                // clear label
                QLabel * const shirtNoLabel = this->findFieldInRow<QLabel *>(row, "shirtNoLabel");
                shirtNoLabel->clear();
                shirtNoLabel->setVisible(false);
                // save value
                Player * const player = this->findPlayerByCode(code);
                player->assignShirtNo(0);
                // disable checkbox if player is unavailable
                senderCheckBox->setEnabled(player->isAvailable());
            }
        }
    }
    return;
}

// [slot]
void SquadWidget::displayPlayerAttributes() {

    ClickableLabel * senderLabel = qobject_cast<ClickableLabel *>(sender());

    // we must iterate over all rows because we don't know which row's label has been used
    for (auto row: *ui->fields) {

        ClickableLabel * const lastNameLabel =
            this->findFieldInRow<ClickableLabel *>(row, on::widgets_shared.lastName);

        // if widget (label) which sent currently processed signal is in this row
        if (senderLabel == lastNameLabel) {

            const uint32_t code = this->findFieldInRow<HiddenLabel *>(row, on::widgets_shared.playerCodeHidden)->text().toUInt();
            _currentPlayer = this->findPlayerByCode(code);
            ui->currentPlayerNameLabel->setText(_currentPlayer->fullName());

            for (uint8_t i = 0; i < static_cast<uint8_t>(player::Attributes::TOTAL_NUMBER); ++i) {

                const auto attribute = static_cast<player::Attributes>(i);
                const uint8_t attributeValue = _currentPlayer->attribute(attribute);
                // change value (text)
                QLabel * const attributeValueLabel = ui->currentPlayerAttributeLabels.at(i).attributeValueLabel;
                const QString attributeValueText =
                    (attributeValue == 0) ? PlayerAttributes::unknownValue : QString::number(attributeValue);
                attributeValueLabel->setText(attributeValueText);
                // change value (progress bar)
                QProgressBar * const attributeValueProgressBar =
                    ui->currentPlayerAttributeLabels.at(i).attributeValueProgressBar;
                attributeValueProgressBar->setValue(attributeValue);
                attributeValueProgressBar->setVisible(PlayerAttributes::isSkill(attribute));
            }

            int8_t curr = static_cast<int8_t>(player::PreferredForAction::CAPTAIN);
            const int8_t max = static_cast<int8_t>(player::PreferredForAction::MAX_VALUE);
            for (uint8_t i = 0; curr < max; ++curr) {

                if (!player::preferenceColumnNames.contains(static_cast<player::PreferredForAction>(curr)))
                    continue;

                const auto preference = static_cast<player::PreferredForAction>(curr);
                const bool preferenceState = _currentPlayer->isPreferredFor(preference);
                QCheckBox * const preferenceCheckBox = ui->currentPlayerPreferenceLabels.at(i).preferenceStateCheckBox;
                preferenceCheckBox->setChecked(preferenceState);
                // only scrum-half can throw ball into a scrum
                preferenceCheckBox->setDisabled(preference == player::PreferredForAction::SCRUM &&
                                               _currentPlayer->position()->currentPositionNo() != 9);
                ++i;
            }
            ui->preferenceHeaderLabel->setVisible(true);
            ui->currentPlayerPreferenceWidget->setVisible(true);
        }
    }
    return;
}

// [slot]
void SquadWidget::changePlayerPreference(const bool checked) {

    QCheckBox * const senderCheckBox = qobject_cast<QCheckBox *>(sender());

    // we need to find out which preference (state) is being toggled
    const QString objectName = senderCheckBox->objectName().replace(on::squadwidget.preferenceState, QString()).replace("_", " ");
    const player::PreferredForAction currentPreference = player::preferenceColumnNames.key(objectName);

    switch (currentPreference) {

        case player::PreferredForAction::CAPTAIN: {

            // only one of the players can be a captain (remove current captain)
            for (auto player: this->_players) {

                if (player->isPreferredFor(currentPreference)) {

                    player->setAsPreferredFor(currentPreference, false);
                    ui->captainAgeLabel->setStyleSheet(styleSheet());
                    if (player->isPreferredFor(player::PreferredForAction::PENALTY))
                        ui->captainAgeLabel->setStyleSheet(ss::shared.style(ss::squadwidget.PenaltyExecutorStyle));
                    ui->captainAgeLabel->repaint();
                    ui->captainAgeLabel = nullptr;
                    break;
                }
            }

            // set new captain
            if (checked) {

                _currentPlayer->setAsPreferredFor(currentPreference, true);
                QLabel * currentPlayerAgeLabel = this->findWidgetByPlayer<QLabel *>(_currentPlayer, on::squadwidget.age);
                currentPlayerAgeLabel->setStyleSheet(ss::shared.style(ss::squadwidget.CaptainStyle));
                if (_currentPlayer->isPreferredFor(player::PreferredForAction::PENALTY))
                    currentPlayerAgeLabel->setStyleSheet(ss::shared.style(ss::squadwidget.CaptainAndPenaltyExecutorStyle));
                currentPlayerAgeLabel->repaint();
                ui->captainAgeLabel = currentPlayerAgeLabel;
            }
            break;
        }
        case player::PreferredForAction::SCRUM: // fall through
        case player::PreferredForAction::KICK_OFF: // fall through
        case player::PreferredForAction::LINEOUT: // fall through
        case player::PreferredForAction::PENALTY: // fall through
        case player::PreferredForAction::CONVERSION: {

            // only limited number of players can be set as preferenced for given activity
            uint8_t noOfPlayers = 0;
            for (auto player: this->_players) {

                if (player->isPreferredFor(currentPreference))
                    ++noOfPlayers;
                if ((noOfPlayers == player::maxNumberOfPlayersForPreference && checked)
                    // if maximum number of players for given activity has already been set and we're trying to add another one
                    || (player == _currentPlayer && !checked)) { // or if we're just trying to deselect currently assigned player

                    // remove (one of) currently assigned player(s)
                    player->setAsPreferredFor(currentPreference, false);
                    if (ui->penaltyExecutorsAgeLabel.contains(player->code())) {

                        ui->penaltyExecutorsAgeLabel[player->code()]->setStyleSheet(styleSheet());
                        if (player->isCaptain())
                            ui->penaltyExecutorsAgeLabel[player->code()]->setStyleSheet(ss::shared.style(ss::squadwidget.CaptainStyle));
                        ui->penaltyExecutorsAgeLabel[player->code()]->repaint();
                        ui->penaltyExecutorsAgeLabel.remove(player->code());
                    }
                    break;
                }
            }

            // set new player for given activity
            if (checked) {

                _currentPlayer->setAsPreferredFor(currentPreference, true);
                QLabel * currentPlayerAgeLabel = this->findWidgetByPlayer<QLabel *>(_currentPlayer, on::squadwidget.age);
                if (currentPreference == player::PreferredForAction::PENALTY) {

                    currentPlayerAgeLabel->setStyleSheet(ss::shared.style(ss::squadwidget.PenaltyExecutorStyle));
                    if (_currentPlayer->isCaptain())
                        currentPlayerAgeLabel->setStyleSheet(ss::shared.style(ss::squadwidget.CaptainAndPenaltyExecutorStyle));
                    currentPlayerAgeLabel->repaint();
                }
                ui->penaltyExecutorsAgeLabel.insert(_currentPlayer->code(), currentPlayerAgeLabel);
            }
            break;
        }
        default: return;
    }

    return;
}

// [slot]
bool SquadWidget::changeCurrentPositionStandard() {

    ClickableLabel * senderLabel = qobject_cast<ClickableLabel *>(sender());

    // we must iterate over all rows because we don't know which row's label has been used
    for (auto row: *ui->fields) {

        ClickableLabel * const currentPositionLabel =
            this->findFieldInRow<ClickableLabel *>(row, on::squadwidget.currentPosition);

        // if widget (label) which sent currently processed signal is in this row
        if (senderLabel == currentPositionLabel) {

            const uint32_t code = this->findFieldInRow<HiddenLabel *>(row, on::widgets_shared.playerCodeHidden)->text().toUInt();
            Player * const currentPlayer = this->findPlayerByCode(code);
            const PlayerPosition_index_item::PositionType positionType = currentPlayer->position()->positionType();

            QStringList positionNames;
            QMap<QString, PlayerPosition_index_item *> mapPositions;

            // positions (from list) which position types match player's position type
            const QVector<PlayerPosition_index_item *> items =
                playerPosition_index.findPlayerPositionsByType(positionType);

            for (auto position: items) {

                positionNames << position->positionName();
                mapPositions.insert(position->positionName(), position);
            }
            if (positionNames.isEmpty())
                return false;

            const QString changeToExtended = QStringLiteral("<other options>");
            positionNames << changeToExtended;
            bool ok = false;
            const QString selectedCurrentPosition =
                InputDialog::getItem(this, QStringLiteral("Select position (standard)"),
                                     QStringLiteral("Select current position (recommended options):"), positionNames, &ok, 400);
            if (!ok || selectedCurrentPosition.isEmpty())
                return false;

            if (selectedCurrentPosition == changeToExtended)
                return (changeCurrentPositionExtended(senderLabel));

            currentPositionLabel->setText(selectedCurrentPosition);
            this->updateElementsAfterPositionChanged(currentPositionLabel, currentPlayer, mapPositions[selectedCurrentPosition]);

            // select into/deselect from squad (emit signal)
            const int state = this->findFieldInRow<QCheckBox *>(row, on::squadwidget.selectedForNextMatch)->checkState();
            emit senderLabel->textChanged(state);
        }
    }
    return true;
}

// [slot]
bool SquadWidget::changeCurrentPositionExtended(ClickableLabel * senderLabel) {

    if (senderLabel == nullptr)
        qobject_cast<ClickableLabel *>(sender());

    // we must iterate over all rows because we don't know which row's label has been used
    for (auto row: *ui->fields) {

        ClickableLabel * const currentPositionLabel =
            this->findFieldInRow<ClickableLabel *>(row, on::squadwidget.currentPosition);

        // if widget (label) which sent currently processed signal is in this row
        if (senderLabel == currentPositionLabel) {

            const uint32_t code = this->findFieldInRow<HiddenLabel *>(row, on::widgets_shared.playerCodeHidden)->text().toUInt();
            Player * const currentPlayer = this->findPlayerByCode(code);
            const PlayerPosition_index_item::PositionType positionType = currentPlayer->position()->positionType();

            const PlayerPosition_index_item::PositionBaseType positionBaseType =
                playerPosition_index.findPositionBaseTypeByType(positionType);

            QStringList positionNames;
            QMap<QString, PlayerPosition_index_item *> mapPositions;

            // positions (from list) which position types match player's position type
            const QVector<PlayerPosition_index_item *> items =
                playerPosition_index.findPlayerPositionsByType(positionBaseType);

            for (auto position: items) {

                positionNames << position->positionName();
                mapPositions.insert(position->positionName(), position);
            }
            if (positionNames.isEmpty())
                return false;

            bool ok = false;
            const QString selectedCurrentPosition =
                InputDialog::getItem(this, QStringLiteral("Select position (extended)"),
                                     QStringLiteral("Select current position (all available options):"), positionNames, &ok, 400);
            if (!ok || selectedCurrentPosition.isEmpty())
                return false;

            currentPositionLabel->setText(selectedCurrentPosition);
            this->updateElementsAfterPositionChanged(currentPositionLabel, currentPlayer, mapPositions[selectedCurrentPosition]);

            // select into/deselect from squad (emit signal)
            const int state = this->findFieldInRow<QCheckBox *>(row, on::squadwidget.selectedForNextMatch)->checkState();
            emit senderLabel->textChanged(state);
        }
    }
    return true;
}

// [slot]
void SquadWidget::setSelectionProperties(const bool assigned /* = false */) {

    uint16_t row = 1;

    // update UI elements
    for (auto player: this->_players) {

        // display preferences (for all players)
        int8_t curr = static_cast<int8_t>(player::PreferredForAction::CAPTAIN);
        const int8_t max = static_cast<int8_t>(player::PreferredForAction::MAX_VALUE);
        QWidget * const parent = dynamic_cast<QWidget *>(this->parent());
        Player * const currentPlayer = _currentPlayer;

        for (uint8_t i = 0; curr < max; ++curr) {

            const auto preference = static_cast<player::PreferredForAction>(curr);
            if (!player::preferenceColumnNames.contains(preference) || preference == player::PreferredForAction::CAPTAIN)
                continue;

            const bool preferenceState = player->isPreferredFor(preference);
            const QString preferenceName = player::preferenceColumnNames[preference].trimmed().replace(" ", "_");
            const QString checkBoxObjectName = preferenceName + on::squadwidget.preferenceState;

            _currentPlayer = player;
            // we need to simulate CheckBox click to set off all associated actions
            // i.e. display colour tags, fill in list of executors (ui.penaltyExecutorsAgeLabel), etc.
            QCheckBox * const currentPreferenceStateCheckBox = parent->findChild<QCheckBox *>(checkBoxObjectName);
            currentPreferenceStateCheckBox->setChecked(!preferenceState);
            currentPreferenceStateCheckBox->click();

            ++i;
        }
        _currentPlayer = currentPlayer;

        if (player->isBasePlayer() || player->isBenchPlayer()) {

            // display position (for currently selected players)
            if (assigned) {

                const QString currentPositionObjectName = on::squadwidget.currentPosition + on::sep + QString::number(row);
                QLabel * const currentPositionLabel = this->findChild<QLabel *>(currentPositionObjectName);
                currentPositionLabel->setText(player->position()->currentPosition());
            }

            const QString selectedForNextMatchObjectName = on::squadwidget.selectedForNextMatch + on::sep + QString::number(row);
            QCheckBox * selectedForNextMatchCheckBox = this->findChild<QCheckBox *>(selectedForNextMatchObjectName);
            selectedForNextMatchCheckBox->setChecked(assigned);
        }
        ++row;
    }

    return;
}

// [slot]
void SquadWidget::resetSelectionProperties(const bool keepPositions) {

    // if called from reset signal (ResetSelection) => keepPositions = false; otherwise true
    if (!keepPositions) {

        uint16_t row = 1;

        for (auto player: this->_players) {

           const QString currentPositionObjectName = on::squadwidget.currentPosition + on::sep + QString::number(row);
           QLabel * const currentPositionLabel = this->findChild<QLabel *>(currentPositionObjectName);

           // find code of player's original position
           const uint8_t code = playerPosition_index.findPlayerPositionCodeByName(player->position()->originalPosition());
           if (code > 0) {

               // if original position is specific (e.g. blindside flanker)
               PlayerPosition_index_item * const position = playerPosition_index.findPlayerPositionByCode(code);
               player->position()->assignNewPlayerPosition(position);
           }
           else
               // if original position is non-specific (e.g. flanker)
               player->position()->assignNewPlayerPosition(nullptr);

           currentPositionLabel->setText(player->position()->currentPosition());

           ++row;
        }
    }

    this->setSelectionProperties(false);

    return;
}

// [slot]
void SquadWidget::automaticSelection() {

    this->resetSelectionProperties(true);

    const bool selectionComplete = this->_myTeam->selectPlayersForNextMatch(_conditionSettings);
    if (!selectionComplete) {

        QMessageBox::information(this, QStringLiteral("Squad selection"), message.display(this->objectName(), "automaticSelection"));
        return;
    }

    // setSelectionProperties() triggers &ClickableLabel::textChanged signal -> &SquadWidget::selectIntoSquad slot
    // since we have selected base players right now, _benchSelection must be false so correct players are marked
    const bool currentBenchSelection = _benchSelection;
    _benchSelection = false;

    this->setSelectionProperties(true);

    if (currentBenchSelection) {

        this->_myTeam->selectSubstitutes(_conditionSettings);

        _benchSelection = currentBenchSelection;
        this->setSelectionProperties(true);
    }

    this->displayPlayerAttributesAverages();

    return;
}
