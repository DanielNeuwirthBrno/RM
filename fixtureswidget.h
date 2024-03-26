/*******************************************************************************
Copyright 2020-22 Daniel Neuwirth
This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#ifndef FIXTURESWIDGET_H
#define FIXTURESWIDGET_H

#include <QString>
#include <QVector>
#include <QWidget>
#include "competition.h"
#include "match/match.h"
#include "settings/config.h"
#include "shared/datetime.h"
#include "ui/widgets/ui_fixtureswidget.h"

class FixturesWidget: public QWidget {

    Q_OBJECT

    public:
        Q_DISABLE_COPY(FixturesWidget)

        explicit FixturesWidget(DateTime &, Match *, MatchType::Type *, Team * const, Settings * const,
                                QVector<Match *> * const, const QVector<Referee *> &);
        explicit FixturesWidget(QWidget *, const Competition &, const QVector<Team *>, /* <- for ui only */
                                DateTime &, Match *, MatchType::Type *, Team * const, Settings * const,
                                QVector<Match *> * const, const QVector<Referee *> &);
        ~FixturesWidget();

        template<typename T>
        T findWidgetByCode(const uint16_t code, const QString & objectName) const {

            QString sourceObjectName = QString();
            QWidget * rowWidget = nullptr;

            for (auto it: ui->scrollAreaWidget->findChildren<HiddenLabel *>(sourceObjectName, Qt::FindDirectChildrenOnly)) {

                if (it->text() == QString::number(code)) {

                    sourceObjectName = it->objectName();
                    rowWidget = ui->rowWidgets.value(it, nullptr);
                    break;
                }
            }
            if (rowWidget == nullptr)
                return nullptr;

            const QString destObjectName = objectName + sourceObjectName.mid(sourceObjectName.indexOf(on::sep));
            T widget = rowWidget->findChild<T>(destObjectName, Qt::FindDirectChildrenOnly);

            return widget;
        };

        inline MatchPeriod::TimePeriod playUntilAtLeastPeriod() const { return _playUntilAtLeastPeriod; }
        void updateScore(const MatchType::Location);

        Ui_FixturesWidget * ui;

    private:
        void updateTime_Rewind();
        void updateTeamNames(QVector<Match *>::iterator);

        bool hasPartOfSeasonFinished(const MatchType::Type = MatchType::Type::REGULAR) const;

        Team * _myTeam;
        DateTime & _dateTime;

        QVector<Match *> * _fixtures;
        QVector<Team *> _teams;
        const QVector<Referee *> & _referees;

        Match * _nextMatch;
        Settings * _settings;

        bool _allMatchesMode;
        MatchType::Type _matchTypeModeForDisplay;

        const Competition * const _competition;
        MatchType::Type * _seasonMatchType;

        MatchPeriod::TimePeriod _playUntilAtLeastPeriod; // used for testing extra-time (and beyond) periods' progress

    signals:
        void timeShift(const bool = false);
        void timeChanged();

    public slots:
        bool playNextMatch(const bool = false);

    private slots:
        void switchFixtureTypeMode();
        void playNextMatches();
        void setPlayUntilAtLeastPeriod();
        void displayPeriodDurations();
};

#endif // FIXTURESWIDGET_H
