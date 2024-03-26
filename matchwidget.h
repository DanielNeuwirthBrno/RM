/*******************************************************************************
 Copyright 2020-22 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#ifndef MATCHWIDGET_H
#define MATCHWIDGET_H

#include <QLabel>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <cmath>
#include "match/gameplay.h"
#include "settings/config.h"
#include "shared/datetime.h"
#include "team.h"
#include "ui/widgets/ui_matchwidget.h"

class Match;

class MatchWidget: public QWidget {

   Q_OBJECT

    public:
        Q_DISABLE_COPY(MatchWidget)

        enum ResumePlay { NO_ACTION = 0, SUBSTITUTION = 1, SETTINGS = 2 };

        explicit MatchWidget(QWidget *, Match *, Match *, MatchType::Type *, Team * const, Settings * const, DateTime &);
        ~MatchWidget();

        void timeStoppedMessageBox(const QString &, const QStringList & = QStringList());

        void updateStatisticsUI(const MatchType::Location, const QString &, const QString &, const bool = true) const;

        QString currentScore() const;
        void displayPoints(const QMap<PointEvent, QStringList> &, const MatchType::Location) const;

        QString playerForSubstitution(Player * const) const;

        inline bool extendedLog() const { return (_settings->logging() == LogLevel::EXTENDED); }
        inline QString pointsInfoForLog(const uint8_t noOfPoints) const
            { return QStringLiteral(" (") + QString::number(noOfPoints) + QStringLiteral(" points)"); }
        QString dominationStatsForLog(const double, const uint8_t, const bool = false) const;
        void logRecord(const QString &) const;

        void updatePackWeight() const;
        void updatePlayer(const QString &, const MatchType::Location) const;

        inline void nextAction(const ResumePlay nextAction) { _resumePlay = nextAction; return; }
        inline ResumePlay resumePlay() const { return _resumePlay; }

        Ui_MatchWidget * ui;

    private:
        QString labelName(const QString &, const QString &) const;
        QLabel * findWidgetByObjectName(const QString &, const MatchType::Location) const;

        Settings * _settings;
        DateTime & _dateTime;

        Match * _match;
        Match * _nextMatch;
        MatchType::Type * _competitionPeriod;
        Team * _myTeam;

        ResumePlay _resumePlay;
        GamePlay * _play;

    private slots:
        void playMatchInDiagnosticMode();
        void playMatch();
};

#endif // MATCHWIDGET_H
