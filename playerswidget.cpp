/*******************************************************************************
 Copyright 2021 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include <QRegularExpression>
#include "playerswidget.h"
#include "player/player_attributes.h"
#include "player/player_utils.h"
#include "shared/handle.h"
#include "shared/html.h"

PlayersWidget::PlayersWidget(QWidget * parent, const QDate & currentDate, const QVector<Team *> & teams):
    PlayerView(parent), ui(new Ui_PlayersWidget), _currentFilter(FilteredColumnsPlW::NO_FILTER),
    _currentDisplay(DisplayedColumnsPlW::BASE), _recordsValidToDate(currentDate), _teams(teams) {

    this->setObjectName(on::widgets["players"]);

    ui->setupUi(this, _currentFilter, _currentFilterValue, _currentDisplay, _recordsValidToDate, _teams);

    connect(ui->displayBasicColumnsButton, &QPushButton::toggled, this,
            [this](bool checked) -> void { if (checked) this->_currentDisplay = DisplayedColumnsPlW::BASE; return; } );
    connect(ui->displayAttributesButton, &QPushButton::toggled, this, &PlayersWidget::showAttributes);
    connect(ui->displayConditionButton, &QPushButton::toggled, this, &PlayersWidget::showCondition);
    connect(ui->quickFilterPropertyNamesComboBox, &QComboBox::currentTextChanged, this, &PlayersWidget::fillFilterPropertyValues);
    connect(ui->quickFilterPropertyValuesComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated),
            this, &PlayersWidget::applySelectedFilter);

    this->connectClickableLables();
}

void PlayersWidget::connectClickableLables() {

    QMetaObject::Connection connection;

    for (auto row: *ui->fields) {

        ClickableLabel * const lastNameLabel = this->findFieldInRow<ClickableLabel *>(row, on::widgets_shared.lastName);
        connection = connect(lastNameLabel, &ClickableLabel::leftClicked, this, &PlayersWidget::displayDetailsPanel);

        if (static_cast<bool>(connection))
            this->connections.append(connection);
    }
    return;
}

void PlayersWidget::disconnectClickableLables() {

    for (auto connection: this->connections)
        disconnect(connection);
    connections.clear();

    return;
}

Player * PlayersWidget::findPlayerByCode(const uint32_t code) {

    for (auto team: _teams) {

        const auto it = std::find_if(team->squad().begin(), team->squad().end(),
                                     [&code](Player * const & it) { return it->code() == code; });

        if (it != team->squad().end())
            return (*it);
    }
    return nullptr;
}

void PlayersWidget::addPropertyValue(const FilteredColumnsPlW filter, QStringList & valuesForFilter, Player * const player) {

    switch (filter) {

        case FilteredColumnsPlW::AGE:
            if (player->age(_recordsValidToDate) == 0)
                return;
            if (!valuesForFilter.contains(QString::number(player->age(_recordsValidToDate))))
                  valuesForFilter.append(QString::number(player->age(_recordsValidToDate)));
            break;
        case FilteredColumnsPlW::COUNTRY:
            if (!valuesForFilter.contains(player->country()))
                valuesForFilter.append(player->country());
            break;
        case FilteredColumnsPlW::CLUB:
            if (!valuesForFilter.contains(player->club()))
                valuesForFilter.append(player->club());
            break;
        case FilteredColumnsPlW::HEALTH:
            if (!valuesForFilter.contains(player::healthStatusColumnNames[player->condition()->currentState()]))
                valuesForFilter.append(player::healthStatusColumnNames[player->condition()->currentState()]);
            break;
        case FilteredColumnsPlW::POSITION:
            if (!valuesForFilter.contains(player->position()->currentPosition()))
                valuesForFilter.append(player->position()->currentPosition());
            break;
        default: ;
    }

    return;
}

// [slot]
void PlayersWidget::showAttributes(const bool checked) {

    // display headers
    const QRegularExpression attributesLabelsRegex = QRegularExpression(on::playerswidget.headerAttributeColumn);
    const QList<QLabel *> attributesHeaderLabels =
        this->ui->getScrollAreaWidget()->findChildren<QLabel *>(attributesLabelsRegex, Qt::FindDirectChildrenOnly);
    for (auto label: attributesHeaderLabels)
        label->setVisible(checked);

    // display values
    for (auto widgetSet: ui->columnWidgets)
        if (widgetSet.group() == static_cast<uint8_t>(DisplayedColumnsPlW::ATTRIBUTES))
            widgetSet.widget()->setVisible(checked);

    if (checked)
        this->_currentDisplay = DisplayedColumnsPlW::ATTRIBUTES;

    return;
}

// [slot]
void PlayersWidget::showCondition(const bool checked) {

    // display headers
    const QRegularExpression conditionLabelsRegex = QRegularExpression(on::playerswidget.headerConditionColumn);
    QList<QLabel *> conditionHeaderLabels =
        this->ui->getScrollAreaWidget()->findChildren<QLabel *>(conditionLabelsRegex, Qt::FindDirectChildrenOnly);
    for (auto label: conditionHeaderLabels)
         label->setVisible(checked);

    // display values
    for (auto widgetSet: ui->columnWidgets)
        if (widgetSet.group() == static_cast<uint8_t>(DisplayedColumnsPlW::CONDITION))
            widgetSet.widget()->setVisible(checked);

    if (checked)
        this->_currentDisplay = DisplayedColumnsPlW::CONDITION;

    return;
}

// [slot]
void PlayersWidget::fillFilterPropertyValues(const QString & currentItem) {

    this->_currentFilter = ui->filteredColumns[currentItem];

    ui->quickFilterPropertyValuesComboBox->clear();
    if (ui->filteredColumns[currentItem] == FilteredColumnsPlW::NO_FILTER) {

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
void PlayersWidget::applySelectedFilter() {

    this->_currentFilterValue = ui->quickFilterPropertyValuesComboBox->currentText();

    // remove old (current) column widgets
    this->disconnectClickableLables();
    for (auto columnWidget: ui->columnWidgets)
        columnWidget.clearColumnWidget(ui->gridLayout);
    ui->columnWidgets.clear();

    QWidget * parent = Handle::getWindowHandle("players");
    ui->setupGrid(parent, static_cast<uint8_t>(_currentFilter), _currentFilterValue, _teams);

    if (_currentDisplay == DisplayedColumnsPlW::ATTRIBUTES)
        this->showAttributes(true);
    if (_currentDisplay == DisplayedColumnsPlW::CONDITION)
        this->showCondition(true);

    this->connectClickableLables();
    return;
}

// [slot]
void PlayersWidget::displayDetailsPanel() {

    ClickableLabel * senderLabel = qobject_cast<ClickableLabel *>(sender());

    // we must iterate over all rows because we don't know which row's label has been used
    for (auto row: *ui->fields) {

        ClickableLabel * const lastNameLabel = this->findFieldInRow<ClickableLabel *>(row, on::widgets_shared.lastName);

        // if widget (label) which sent currently processed signal is in this row
        if (senderLabel == lastNameLabel) {

            const uint32_t code = this->findFieldInRow<HiddenLabel *>(row, on::widgets_shared.playerCodeHidden)->text().toUInt();

            Player * const currentPlayer = this->findPlayerByCode(code);
            QString playerHealthIssuesText = html_functions.startTag(html_tags.boldText) % currentPlayer->fullName() %
                                             html_functions.endTag(html_tags.boldText);
            QVector<QStringList> tableRows;
            uint16_t totalNumberOfDays = 0;

            for (auto health_issue: currentPlayer->condition()->completeHealthStatusHistory(totalNumberOfDays)) {

                const QString duration = (health_issue->duration() > 0)
                    ? QString::number(health_issue->duration()) + QStringLiteral(" day(s)") : QString();

                QStringList tableRow = QStringList {
                    health_issue->statusValidFrom().toString(Qt::DefaultLocaleShortDate) % QStringLiteral("-") %
                    health_issue->statusValidTo().toString(Qt::DefaultLocaleShortDate),
                    player::healthStatusColumnNames[health_issue->healthStatus()], duration
                };

                tableRows.push_back(tableRow);
            }

            if (totalNumberOfDays > 0 || !currentPlayer->isHealthy()) {

                const QString availability = (!currentPlayer->isHealthy()) ?
                    html_functions.startTag(html_tags.italicText) % QStringLiteral("unavailable") %
                    html_functions.endTag(html_tags.italicText) : QString();
                const QString totalNumberOfDaysText = (totalNumberOfDays > 0) ?
                    QString::number(totalNumberOfDays) + QStringLiteral(" day(s)") : QString();

                const QStringList tableRowTotals = QStringList { availability, QStringLiteral("total:"), totalNumberOfDaysText };
                tableRows.push_back(tableRowTotals);
            }

            html_functions.buildTable(playerHealthIssuesText, tableRows);
            ui->playerDetailsTextEdit->setHtml(playerHealthIssuesText);
            ui->resizePlayerDetails(2 + tableRows.size());
        }
    }
    return;
}
