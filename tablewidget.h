/*******************************************************************************
 Copyright 2021 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#ifndef TABLEWIDGET_H
#define TABLEWIDGET_H

#include <QStringList>
#include <QWidget>
#include "team.h"
#include "ui/widgets/ui_tablewidget.h"

class TableWidget: public QWidget {

    Q_OBJECT

    public:
        Q_DISABLE_COPY(TableWidget)

        explicit TableWidget(QWidget *, Team * const, const QVector<Team *> &);
        ~TableWidget() { delete ui; }

    private:
        struct {

            bool operator()(Team * const team1, Team * const team2) const {

                /* tiebraker criteria: 1] number of team points
                 *                     2] overall point difference
                 *                     3] overall tries difference
                 *                     4] overall number of points
                 *                     5] overall number of tries
                 *                     6] ranking
                 */
                return (
                    /* 1] */ team1->results().pointsTotal() > team2->results().pointsTotal() ||
                    /* 2] */ (team1->results().pointsTotal() == team2->results().pointsTotal() &&
                              team1->scoredPoints().pointDifference() > team2->scoredPoints().pointDifference()) ||
                    /* 3] */ (team1->results().pointsTotal() == team2->results().pointsTotal() &&
                              team1->scoredPoints().pointDifference() == team2->scoredPoints().pointDifference() &&
                              team1->scoredPoints().tryDifference() > team2->scoredPoints().tryDifference()) ||
                    /* 4] */ (team1->results().pointsTotal() == team2->results().pointsTotal() &&
                              team1->scoredPoints().pointDifference() == team2->scoredPoints().pointDifference() &&
                              team1->scoredPoints().tryDifference() == team2->scoredPoints().tryDifference() &&
                              team1->scoredPoints().points() > team2->scoredPoints().points()) ||
                    /* 5] */ (team1->results().pointsTotal() == team2->results().pointsTotal() &&
                              team1->scoredPoints().pointDifference() == team2->scoredPoints().pointDifference() &&
                              team1->scoredPoints().tryDifference() == team2->scoredPoints().tryDifference() &&
                              team1->scoredPoints().points() == team2->scoredPoints().points() &&
                              team1->scoredPoints().tries() > team2->scoredPoints().tries()) ||
                    /* 6] */ (team1->results().pointsTotal() == team2->results().pointsTotal() &&
                              team1->scoredPoints().pointDifference() == team2->scoredPoints().pointDifference() &&
                              team1->scoredPoints().tryDifference() == team2->scoredPoints().tryDifference() &&
                              team1->scoredPoints().points() == team2->scoredPoints().points() &&
                              team1->scoredPoints().tries() == team2->scoredPoints().tries() &&
                              team1->ranking() < team2->ranking())
                );
            }

        } sortTable;

        Ui_TableWidget * ui;

        QStringList _groups;
        uint8_t _currentGroup;

        Team * const _myTeam;
        QVector<Team *> _teamsSorted;

    private slots:
        void previousGroup();
        void nextGroup();
};

#endif // TABLEWIDGET_H
