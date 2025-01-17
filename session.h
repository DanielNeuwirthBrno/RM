/*******************************************************************************
 Copyright 2020-23 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#ifndef SESSION_H
#define SESSION_H

#include <QClipboard>
#include <QDate>
#include <QDateTime>
#include <QMap>
#include <QPair>
#include <QString>
#include <QVector>
#include <QWidget>
#include "competition.h"
#include "db/builder.h"
#include "db/database.h"
#include "match/playoffs.h"
#include "settings/config.h"
#include "shared/datetime.h"
#include "shared/random.h"
#include "match/match.h"
#include "referee.h"
#include "team.h"

class Session {

    public:
        enum SystemDbRestore { RESTORE_FAILED = -1, RESTORE_FAILED_ROLLBACK_OK, RESTORE_OK };

        Session(QWidget * const = nullptr);
        ~Session();

        inline Config config() const { return _config; }
        inline Settings * settings() const { return _settings; }
        inline DateTime & datetime() { return _dateTime; }

        inline const QVector<Referee *> & referees() const { return (_referees); }
        inline const QVector<Team *> & teams() const { return (_teams); };
        inline QVector<Match *> * fixtures() { return &(_fixtures); }

        bool runUserQuery(const QString &) const;
        SystemDbRestore restoreSystemDb() const;

        bool setGameName(QString &, QString &) const;
        bool setManagerName(QString &) const;

        bool newGame();
        bool loadGame();
        bool saveGame();

        Match * nextMatchMyTeam() const;
        Match * nextMatchAllTeams() const;

        void assignTeamsToPlayoffsMatches(const bool);

        inline const Competition & competition() const { return _competition; }
        inline Competition & competition() { return _competition; }

    private:
        void sweepOldDataAndUnusedMemory();

        inline Referee * findRefereeByCode(uint16_t code) const
            { for (auto referee: _referees) if (referee->code() == code) return referee; return nullptr; }

        inline Team * findTeamByCode(uint16_t code) const
            { for (const auto & team: _teams) if (team->code() == code) return team; return nullptr; }
        inline uint8_t teamRanking(const QString & country) const
            { for (const auto & team: _teams) if (team->country() == country) return team->ranking(); return 0; }

        bool copySystemDbFile(const QString &) const;

        bool backupSystemDbFile() const;
        bool dropCurrentSystemDb() const;
        Database::transactionResult runMigrationScripts() const;
        bool restoreFromSystemDbFileBackup() const;

        bool selectCompetitionType(uint8_t &);
        bool selectCompetition(const uint8_t);
        bool selectTeam(uint16_t &, QMap<QString, QPair<uint8_t, QString>> &, const QueryBindings &, const uint8_t);
        bool loadTeams(Team * &, const uint16_t, const QMap<QString, QPair<uint8_t, QString>> &);
        bool loadReferees(const QueryBindings &);

        Match * buildMatchFromRetrievedRecord(const QSqlRecord &);
        bool loadFixtures(const uint16_t);
        bool loadFixturesPlayoffs(const QString &, const QueryBindings &, const QStringList &,
                                  QVector<QPair<Match * const, QVector<QVariant>>> &);
        bool loadFixturesPlayoffs_phase1(const QueryBindings &);
        bool loadFixturesPlayoffs_phase2(const QueryBindings &);

        bool loadPlayerPositionsList() const;
        bool loadPlayers(Team *, const QueryBindings &);
        bool loadPlayerAttributes(Player *, const QueryBindings &);

        bool saveFixtures(QueryBuilder * const);
        bool savePlayers(QueryBuilder * const);

        QWidget * const _mainWindowHandle;
        QClipboard * const _clipboard;

        Config _config;
        Settings * _settings;
        DateTime _dateTime;

        Database * _db;
        QVector<Referee *> _referees;
        QVector<Team *> _teams;
        QVector<Match *> _fixtures;
        Competition _competition;
};

#endif // SESSION_H
