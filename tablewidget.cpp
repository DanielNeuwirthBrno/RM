/*******************************************************************************
 Copyright 2021 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include <algorithm>
#include "shared/html.h"
#include "tablewidget.h"

TableWidget::TableWidget(QWidget * parent, Team * const myTeam, const QVector<Team *> & teams):
    QWidget(parent), ui(new Ui_TableWidget), _currentGroup(0), _myTeam(myTeam), _teamsSorted(teams) {

    this->setObjectName(on::widgets["table"]);

    for (auto team: teams)
        if (!team->group().isNull() && !_groups.contains(team->group()))
            _groups.push_back(team->group());
    _groups.sort();

    std::sort(_teamsSorted.begin(), _teamsSorted.end(), sortTable);

    ui->setupUi(this, myTeam, _teamsSorted, _groups);

    connect(ui->previousGroupButton, &QPushButton::clicked, this, &TableWidget::previousGroup);
    connect(ui->nextGroupButton, &QPushButton::clicked, this, &TableWidget::nextGroup);

    html_functions.dummyCallToSuppressCompilerWarning();
}

// [slot]
void TableWidget::previousGroup() {

    if (_currentGroup == 0)
        _currentGroup = _groups.size();
     --(_currentGroup);

    ui->setupGrid(this, _myTeam, _teamsSorted, _groups, _currentGroup);
}

// [slot]
void TableWidget::nextGroup() {

    ++(_currentGroup);
    if (_currentGroup == _groups.size())
        _currentGroup = 0;

    ui->setupGrid(this, _myTeam, _teamsSorted, _groups, _currentGroup);

    return;
}
