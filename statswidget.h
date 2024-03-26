/*******************************************************************************
 Copyright 2021 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#ifndef STATSWIDGET_H
#define STATSWIDGET_H

#include <QRegularExpression>
#include <QStringList>
#include "match/match.h"
#include "playerview.h"
#include "team.h"
#include "ui/widgets/ui_statswidget.h"

class StatsWidget: public PlayerView {

    Q_OBJECT

    public:
        Q_DISABLE_COPY(StatsWidget)

        explicit StatsWidget(QWidget *, const QVector<Team *> &, QVector<Match *> * const);
        ~StatsWidget() { delete ui; }

        Player * findPlayerByCode(const uint32_t) override;

    private:
        void connectClickableLables() override;
        void disconnectClickableLables() override;

        void addPropertyValue(const FilteredColumnsStW, QStringList &, Player * const);
        void showSelectedStatistics(const QRegularExpression &, const DisplayedColumnsStW,  const bool);

        Ui_StatsWidget * ui;

        FilteredColumnsStW _currentFilter;
        DisplayedColumnsStW _currentDisplay;

        QVector<Team *> _teams;
        QVector<Match *> * const _fixtures;

    private slots:
        void showPoints(const bool);
        void showPasses(const bool);
        void showTackles(const bool);
        void showDiscipline(const bool);
        void fillFilterPropertyValues(const QString &);
        void applySelectedFilter();

        void displayDetailsPanel() override;
};

#endif // STATSWIDGET_H
