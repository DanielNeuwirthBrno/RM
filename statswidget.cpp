/*******************************************************************************
 Copyright 2021-22 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include <QStringBuilder>
#include "shared/handle.h"
#include "shared/html.h"
#include "statswidget.h"

StatsWidget::StatsWidget(QWidget * parent, const QVector<Team *> & teams, QVector<Match *> * const fixtures):
    PlayerView(parent), ui(new Ui_StatsWidget), _currentFilter(FilteredColumnsStW::NO_FILTER),
    _currentDisplay(DisplayedColumnsStW::BASE), _teams(teams), _fixtures(fixtures) {

    this->setObjectName(on::widgets["statistics"]);

    ui->setupUi(this, _currentFilter, _currentFilterValue, _currentDisplay, _teams);

    connect(ui->displayBasicColumnsButton, &QPushButton::toggled, this,
            [this](bool checked) -> void { if (checked) this->_currentDisplay = DisplayedColumnsStW::BASE; return; } );
    connect(ui->displayPointsButton, &QPushButton::toggled, this, &StatsWidget::showPoints);
    connect(ui->displayGameplayButton, &QPushButton::toggled, this, &StatsWidget::showPasses);
    connect(ui->displayTacklesButton, &QPushButton::toggled, this, &StatsWidget::showTackles);
    connect(ui->displayDisciplineButton, &QPushButton::toggled, this, &StatsWidget::showDiscipline);

    connect(ui->quickFilterPropertyNamesComboBox, &QComboBox::currentTextChanged, this, &StatsWidget::fillFilterPropertyValues);
    connect(ui->quickFilterPropertyValuesComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated),
            this, &StatsWidget::applySelectedFilter);
    connect(ui->displayOnlyPlayersWhoPlayed, &QPushButton::clicked, this, &StatsWidget::applySelectedFilter);

    this->connectClickableLables();
}

void StatsWidget::connectClickableLables() {

    QMetaObject::Connection connection;

    for (auto row: *ui->fields) {

        ClickableLabel * const lastNameLabel = this->findFieldInRow<ClickableLabel *>(row, on::widgets_shared.lastName);
        connection = connect(lastNameLabel, &ClickableLabel::leftClicked, this, &StatsWidget::displayDetailsPanel);

        if (static_cast<bool>(connection))
            this->connections.append(connection);
    }
    return;
}

void StatsWidget::disconnectClickableLables() {

    for (auto connection: this->connections)
        disconnect(connection);
    connections.clear();

    return;
}

Player * StatsWidget::findPlayerByCode(const uint32_t code) {

    for (auto team: _teams) {

        const auto it = std::find_if(team->squad().begin(), team->squad().end(),
                                     [&code](Player * const & it) { return it->code() == code; });

        if (it != team->squad().end())
            return (*it);
    }
    return nullptr;
}

void StatsWidget::addPropertyValue(const FilteredColumnsStW filter, QStringList & valuesForFilter, Player * const player) {

    switch (filter) {

        case FilteredColumnsStW::COUNTRY:
            if (!valuesForFilter.contains(player->country()))
                valuesForFilter.append(player->country());
            break;
        case FilteredColumnsStW::CLUB:
            if (!valuesForFilter.contains(player->club()))
                valuesForFilter.append(player->club());
            break;
        default: ;
    }

    return;
}

void StatsWidget::showSelectedStatistics(const QRegularExpression & labelsRegex,
                                         const DisplayedColumnsStW displayedColumns,  const bool checked) {
    // display headers
    const QList<QLabel *> headerLabels =
        this->ui->getScrollAreaWidget()->findChildren<QLabel *>(labelsRegex, Qt::FindDirectChildrenOnly);
    for (auto label: headerLabels)
        label->setVisible(checked);

    // display values
    for (auto widgetSet: ui->columnWidgets)
        if (widgetSet.group() == static_cast<uint8_t>(displayedColumns))
            widgetSet.widget()->setVisible(checked);

    if (checked)
        this->_currentDisplay = displayedColumns;

    return;
}

// [slot]
void StatsWidget::showPoints(const bool checked) {

    const QRegularExpression labelsRegex = QRegularExpression(on::statswidget.headerPointsColumn);
    this->showSelectedStatistics(labelsRegex, DisplayedColumnsStW::POINTS, checked);

    return;
}

// [slot]
void StatsWidget::showPasses(const bool checked) {

    const QRegularExpression labelsRegex = QRegularExpression(on::statswidget.headerPassesColumn);
    this->showSelectedStatistics(labelsRegex, DisplayedColumnsStW::PASSES, checked);

    return;
}

// [slot]
void StatsWidget::showTackles(const bool checked) {

    const QRegularExpression labelsRegex = QRegularExpression(on::statswidget.headerTacklesColumn);
    this->showSelectedStatistics(labelsRegex, DisplayedColumnsStW::TACKLES, checked);

    return;
}

// [slot]
void StatsWidget::showDiscipline(const bool checked) {

    const QRegularExpression labelsRegex = QRegularExpression(on::statswidget.headerDisciplineColumn);
    this->showSelectedStatistics(labelsRegex, DisplayedColumnsStW::DISCIPLINE, checked);

    return;
}

// [slot]
void StatsWidget::fillFilterPropertyValues(const QString & currentItem) {

    this->_currentFilter = ui->filteredColumns[currentItem];

    ui->quickFilterPropertyValuesComboBox->clear();
    if (ui->filteredColumns[currentItem] == FilteredColumnsStW::NO_FILTER) {

        ui->quickFilterPropertyValuesComboBox->setDisabled(true);
        this->applySelectedFilter();
        return;
    }

    QStringList possibleFilterPropertyValues;
    for (auto team: _teams)
        for (auto player: team->squad())
            addPropertyValue(ui->filteredColumns[currentItem], possibleFilterPropertyValues, player);

    possibleFilterPropertyValues.sort();
    ui->quickFilterPropertyValuesComboBox->addItems(possibleFilterPropertyValues);
    ui->quickFilterPropertyValuesComboBox->setDisabled(possibleFilterPropertyValues.isEmpty());

    return;
}

// [slot]
void StatsWidget::applySelectedFilter() {

    this->_currentFilterValue = ui->quickFilterPropertyValuesComboBox->currentText();

    // remove old (current) column widgets
    this->disconnectClickableLables();
    for (auto columnWidget: ui->columnWidgets)
        columnWidget.clearColumnWidget(ui->gridLayout);
    ui->columnWidgets.clear();

    QWidget * parent = Handle::getWindowHandle("players");
    ui->setupGrid(parent, static_cast<uint8_t>(_currentFilter), _currentFilterValue, _teams);

    if (_currentDisplay == DisplayedColumnsStW::POINTS)
        this->showPoints(true);
    if (_currentDisplay == DisplayedColumnsStW::PASSES)
        this->showPasses(true);
    if (_currentDisplay == DisplayedColumnsStW::TACKLES)
        this->showTackles(true);
    if (_currentDisplay == DisplayedColumnsStW::DISCIPLINE)
        this->showDiscipline(true);

    this->connectClickableLables();
    return;
}

// [slot]
void StatsWidget::displayDetailsPanel() {

    ClickableLabel * senderLabel = qobject_cast<ClickableLabel *>(sender());

    // we must iterate over all rows because we don't know which row's label has been used
    for (auto row: *ui->fields) {

        ClickableLabel * const lastNameLabel = this->findFieldInRow<ClickableLabel *>(row, on::widgets_shared.lastName);

        // if widget (label) which sent currently processed signal is in this row
        if (senderLabel == lastNameLabel) {

            const uint32_t code = this->findFieldInRow<HiddenLabel *>(row, on::widgets_shared.playerCodeHidden)->text().toUInt();

            Player * const currentPlayer = this->findPlayerByCode(code);
            QString statsPerGameText = html_functions.startTag(html_tags.boldText) % currentPlayer->fullName() %
                                       html_functions.endTag(html_tags.boldText);

            const auto it = std::find_if(_teams.begin(), _teams.end(), [&currentPlayer](Team const * it) {
                return (it->type() == Team::TeamType::CLUB) ? it->name() == currentPlayer->club()
                                                            : it->name() == currentPlayer->country();
            });

            const QStringList tableHeader = QStringList { QString(), QString(), QString(), "T", "C", "P", "D", "pts", "min" };
            const QList<Align> alignment = QList<Align> { NONE, NONE, NONE, ALIGN_CENTER, ALIGN_CENTER,
                                                          ALIGN_CENTER, ALIGN_CENTER, ALIGN_CENTER, ALIGN_CENTER };
            QVector<QStringList> tableRows;

            for (auto match: *this->_fixtures) {

                if (!match->played())
                    break; // don't display non-played (future) matches

                uint8_t team = 0;
                if (match->team(static_cast<MatchType::Location>(team)) == *it || match->team(static_cast<MatchType::Location>(++team)) == *it) {

                    PlayerStats * const stats = match->playerStats(static_cast<MatchType::Location>(team), currentPlayer);
                    if (stats == nullptr || stats->getStatsValue(StatsType::NumberOf::GAMES_PLAYED) == 0)
                        continue; // == player didn't play in this match => nothing to display

                    PlayerPoints * const points = match->playerPoints_ReadOnly(static_cast<MatchType::Location>(team), currentPlayer);

                    QStringList tableRow = QStringList {
                        match->date().toString(Qt::DefaultLocaleShortDate), PitchLocation[team],
                        match->team(static_cast<MatchType::Location>(std::abs(team-1)))->name().left(20)
                    };

                    const QString null_values = QStringLiteral("-,-,-,-,-");
                    QStringList tableRowPoints = null_values.split(',');

                    if (points != nullptr) {

                        tableRowPoints = QStringList {
                            QString::number(points->getPointsValue(StatsType::NumberOf::TRIES)),
                            QString::number(points->getPointsValue(StatsType::NumberOf::CONVERSIONS)),
                            QString::number(points->getPointsValue(StatsType::NumberOf::PENALTIES)),
                            QString::number(points->getPointsValue(StatsType::NumberOf::DROPGOALS)),
                            QString::number(points->points())
                        };
                    }

                    tableRow.append(tableRowPoints);
                    tableRow.append(QString::number(stats->getStatsValue(StatsType::NumberOf::MINS_PLAYED)));

                    tableRows.push_back(tableRow);
                }
            }

            html_functions.buildTable(statsPerGameText, tableRows, tableHeader, alignment);
            ui->playerDetailsTextEdit->setHtml(statsPerGameText);
            ui->resizePlayerDetails(3 + tableRows.size());
        }
    }
    return;
}
