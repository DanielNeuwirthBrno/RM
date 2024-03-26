/*******************************************************************************
 Copyright 2021 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#ifndef PLAYERSWIDGET_H
#define PLAYERSWIDGET_H

#include <QDate>
#include <QStringList>
#include "playerview.h"
#include "team.h"
#include "ui/widgets/ui_playerswidget.h"

class PlayersWidget: public PlayerView {

    Q_OBJECT

    public:
        Q_DISABLE_COPY(PlayersWidget)

        explicit PlayersWidget(QWidget *, const QDate &, const QVector<Team *> &);
        ~PlayersWidget() { delete ui; }

        Player * findPlayerByCode(const uint32_t) override;

    private:
        void connectClickableLables() override;
        void disconnectClickableLables() override;

        void addPropertyValue(const FilteredColumnsPlW, QStringList &, Player * const);

        Ui_PlayersWidget * ui;

        FilteredColumnsPlW _currentFilter;

        DisplayedColumnsPlW _currentDisplay;
        QDate _recordsValidToDate;

        QVector<Team *> _teams;

    private slots:
        void showAttributes(const bool);
        void showCondition(const bool);
        void fillFilterPropertyValues(const QString &);
        void applySelectedFilter();

        void displayDetailsPanel() override;
};

#endif // PLAYERSWIDGET_H
