/*******************************************************************************
 Copyright 2020-23 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include <QDebug>
#include <QMessageBox>
#include <QSqlRecord>
#include <QSqlRelationalTableModel>
#include <QStringList>
#include <algorithm>
#include "db/query.h"
#include "db/table.h"
#include "match/playoff_rules.h"
#include "player/player_attributes.h"
#include "player/position_types.h"
#include "session.h"
#include "shared/error.h"
#include "shared/file.h"
#include "shared/handle.h"
#include "shared/texts.h"
#include "ui/custom/ui_inputdialog.h"
#include "ui/shared/objectnames.h"

PlayerPosition_index playerPosition_index;

Session::Session(QWidget * const parent): _mainWindowHandle(parent), _clipboard(QGuiApplication::clipboard()),
                 _settings(new Settings()), _dateTime(DateTime()), _db(new Database()), _competition(Competition()) {

    RandomValue::seedRandomGenerator();
}

Session::~Session() {

    for (auto fixture: _fixtures)
        delete fixture;
    for (auto team: _teams)
        delete team;
    for (auto referee: _referees)
        delete referee;

    delete _db;
    delete _settings;
}

bool Session::runUserQuery(const QString & query) const {

    if (this->_db->dbConnected())
        return (this->_db->executeCustomQuery(query));

    // query on system database
    Database * systemDb = new Database();
    const bool dbConnected = systemDb->connectSystemDb();
    if (dbConnected)
        systemDb->executeCustomQuery(query);
    delete systemDb;

    return dbConnected;
}

bool Session::copySystemDbFile(const QString & fileName) const {

    Database * systemDb = new Database();

    try {

        if (!systemDb->connectSystemDb())
            throw DatabaseOperationFailedException();

        dBFile systemDbFile(DbSettings.SystemDb+DbSettings.FileExtension);
        if (!systemDbFile.copy(fileName))
            throw FileOperationFailedException();
    }
    catch (DatabaseOperationFailedException & e) {

        qDebug() << e.description();
        delete systemDb;
        return false;
    }
    catch (FileOperationFailedException & e) {

        qDebug() << e.description();
        delete systemDb;
        return false;
    }

    delete systemDb;
    return true;
}

bool Session::backupSystemDbFile() const {

    // remove old backup of system db (if exists)
    const QString fileNameBackup = DbSettings.SystemDbBackup+DbSettings.FileExtension;
    dBFile systemDbBackupFile(fileNameBackup);

    // backup db can't be connected to => calling remove (instead of removeFile) is sufficient
    if (systemDbBackupFile.exists())
        if (!systemDbBackupFile.remove())
            return false;

    // backup current system db (= copy file)
    return (this->copySystemDbFile(fileNameBackup));
}

bool Session::dropCurrentSystemDb() const {

    // drop database (= remove file)
    const QString fileName = DbSettings.SystemDb+DbSettings.FileExtension;
    dBFile systemDbFile(fileName);

    return (systemDbFile.removeFile(this->_db));
}

Database::transactionResult Session::runMigrationScripts() const {

    Database::transactionResult transactionResult = Database::transactionResult::NO_CONNECTION;

    // create database (= connect to new file)
    Database * systemDb = new Database();
    const bool dbConnected = systemDb->connectSystemDb();
    qDebug() << systemDb->setDbName() << systemDb->setConnName() << systemDb->noOfRowsSelectedReported();

    // run generating scripts
    QStringList queries;
    if (dbConnected) {

        const uint8_t no_of_queries =
            systemDb->loadQueryFromResource(QStringLiteral(":/sql/sql/restore_system_db.sql"), queries);

        if (no_of_queries > 0) {

            bool querySuccess = systemDb->executeCustomQuery(Database::SQL_BEGIN_TRAN);
            for (const auto & query: queries) {

                querySuccess = querySuccess && systemDb->executeCustomQuery(query);
                if (!querySuccess)
                    break;
            }

            if (querySuccess) {

                transactionResult = Database::transactionResult::COMMIT;
                querySuccess = systemDb->executeCustomQuery(Database::SQL_COMMIT);
            }

            if (!querySuccess) {

                // we hope that rollback (on db level) will be successful
                transactionResult = Database::transactionResult::ROLLBACK;
                systemDb->executeCustomQuery(Database::SQL_ROLLBACK);
            }
        }
    }

    delete systemDb;
    return transactionResult;
}

bool Session::restoreFromSystemDbFileBackup() const {

    // delete newly created systemDb file (after rollback it's empty)
    const QString fileName = DbSettings.SystemDb+DbSettings.FileExtension;
    dBFile systemDbFile(fileName);
    if (systemDbFile.exists())
        if (!systemDbFile.remove())
           return false;

    // restore from backup (= rename file)
    const QString fileNameBackup = DbSettings.SystemDbBackup+DbSettings.FileExtension;
    dBFile systemDbBackupFile(fileNameBackup);
    if (!systemDbBackupFile.rename(fileName))
        return false;

    return true;
}

Session::SystemDbRestore Session::restoreSystemDb() const {

    if (!backupSystemDbFile())
        return SystemDbRestore::RESTORE_FAILED;
    if (!dropCurrentSystemDb())
        return SystemDbRestore::RESTORE_FAILED;

    switch (runMigrationScripts()) {

        case Database::COMMIT:
            return SystemDbRestore::RESTORE_OK;
        case Database::ROLLBACK:
            if (restoreFromSystemDbFileBackup())
                return SystemDbRestore::RESTORE_FAILED_ROLLBACK_OK;
            // otherwise fallthrough
        case Database::NO_CONNECTION:
        default: ;
    }

    return SystemDbRestore::RESTORE_FAILED;
}

bool Session::selectCompetitionType(uint8_t & competitionType) {

    QSqlTableModel * const table = new QSqlTableModel(nullptr, this->_db->db());

    try {

        const QString tableName = QStringLiteral("CompetitionType");
        const QPair<uint16_t, Qt::SortOrder> sort = qMakePair(1, Qt::AscendingOrder);

        const bool querySuccess = this->_db->executeQueryForModel(table, tableName, QString(), sort);
        if (!querySuccess)
            throw SelectFromDatabaseFailedException();
        if (table->rowCount() == 0)
            throw SelectFromDatabaseReturnedNullException();

        QStringList competitionTypes;
        QMap<QString, uint8_t> mapCompetitionTypes;

        for (int i = 0; i < table->rowCount(); ++i) {

            const QSqlRecord row = table->record(i);
            competitionTypes << row.value("name").toString();
            mapCompetitionTypes.insert(row.value("name").toString(), row.value("code").toUInt());
        }
        if (competitionTypes.isEmpty())
            throw SelectFromDatabaseReturnedNullException();

        bool ok = false;
        const QString selectedCompetitionType =
            InputDialog::getItem(_mainWindowHandle, QStringLiteral("Competition type"),
                                 QStringLiteral("Select competition type:"), competitionTypes, &ok, 400);
        if (!ok || selectedCompetitionType.isEmpty())
            throw NoSuppliedValueException();
        competitionType = mapCompetitionTypes[selectedCompetitionType];
    }
    catch (SelectFromDatabaseFailedException & e) {

        qDebug() << e.description();
        QMessageBox::critical(_mainWindowHandle, e.description(), QueryErrorText::executionFailed(table->query().lastError()));

        delete table;
        return false;
    }
    catch (SelectFromDatabaseReturnedNullException & e) {

        qDebug() << e.description();
        const QString query = QueryErrorText::nullReturned(table->query().lastQuery());

        this->_clipboard->setText(query);
        QMessageBox::critical(_mainWindowHandle, e.description(), query);

        delete table;
        return false;
    }
    catch (NoSuppliedValueException & e) {

        qDebug() << e.description();

        delete table;
        return false;
    }

    delete table;
    return true;
}

bool Session::selectCompetition(const uint8_t competitionType) {

    QSqlRelationalTableModel * const table = new QSqlRelationalTableModel(nullptr, this->_db->db());

    try {

        const QString tableName = QStringLiteral("Competition");
        const QPair<uint16_t, QSqlRelation> relation =
            qMakePair<uint16_t, QSqlRelation>(2, QSqlRelation("Country", "code", "name"));
        const QString filter = QStringLiteral("valid = 1 AND Competition.type = ") + QString::number(competitionType);
        const QPair<uint16_t, Qt::SortOrder> sort = qMakePair(0, Qt::AscendingOrder);

        const bool querySuccess = this->_db->executeQueryForModelWithRelation(table, relation, tableName, filter, sort);
        if (!querySuccess)
            throw SelectFromDatabaseFailedException();
        if (table->rowCount() == 0)
            throw SelectFromDatabaseReturnedNullException();

        QStringList competitionsList;
        QMap<QString, QSqlRecord> mapCompetitions;

        for (int i = 0; i < table->rowCount(); ++i) {

            const QSqlRecord row = table->record(i);
            const QString competitionDescription = (!row.value("Country_name_2").toString().isEmpty()) ?
                row.value("name").toString() + " (" + row.value("Country_name_2").toString() + ")" :
                row.value("name").toString();  // country is null (in case of multiple host countries)

            competitionsList << competitionDescription;
            mapCompetitions.insert(competitionDescription, row);
        }
        if (competitionsList.isEmpty())
            throw SelectFromDatabaseReturnedNullException();

        bool ok = false;
        const QString selectedCompetition = InputDialog::getItem(_mainWindowHandle,
            QStringLiteral("Competition"), QStringLiteral("Select competition:"), competitionsList, &ok, 400);
        if (!ok || selectedCompetition.isEmpty())
            throw NoSuppliedValueException();

        const QSqlRecord row = mapCompetitions[selectedCompetition];
        this->_competition = Competition(row.value("code").toUInt(), row.value("name").toString(),
                             row.value("Country_name_2").toString(), static_cast<CompetitionType::Type>
                             (row.value("type").toUInt()), row.value("level").toUInt(), row.value("pools").toUInt(),
                             row.value("playoffs").toBool(), row.value("fromDate").toDate(), row.value("toDate").toDate());
    }
    catch (SelectFromDatabaseFailedException & e) {

        qDebug() << e.description();
        QMessageBox::critical(_mainWindowHandle, e.description(), QueryErrorText::executionFailed(table->query().lastError()));

        delete table;
        return false;
    }
    catch (SelectFromDatabaseReturnedNullException & e) {

        qDebug() << e.description();
        const QString query = QueryErrorText::nullReturned(table->query().lastQuery());

        this->_clipboard->setText(query);
        QMessageBox::critical(_mainWindowHandle, e.description(), query);

        delete table;
        return false;
    }
    catch (NoSuppliedValueException & e) {

        qDebug() << e.description();

        delete table;
        return false;
    }

    delete table;
    return true;
}

bool Session::selectTeam(uint16_t & myTeamCode, QMap<QString, QPair<uint8_t, QString>> & teamCodes,
                         const QueryBindings & bindings, const uint8_t competitionType) {

    QueryResults * results = new QueryResults();

    try {

        const QString queryString = this->_db->loadQueryFromResource(QStringLiteral(":/sql/sql/select_team.sql"));

        const bool querySuccess = this->_db->executeCustomQuery(queryString, results, bindings);
        if (!querySuccess)
            throw SelectFromDatabaseFailedException();
        if (results->isEmpty())
            throw SelectFromDatabaseReturnedNullException();

        QStringList teams;
        QMap<QString, uint16_t> mapTeams;

        for (auto row: results->rows()) {

            const uint16_t code = row.at(results->columnNo("code")).toUInt();
            const QString name = row.at(results->columnNo("name")).toString();
            const QString city = row.at(results->columnNo("city")).toString();
            const QString country = row.at(results->columnNo("country_code")).toString();
            const uint8_t ranking = row.at(results->columnNo("ranking")).toUInt();
            const QString group = row.at(results->columnNo("pool")).toString();

            QString teamDescription = QString();
            switch (competitionType) {

                case 0: /* domestic league */ teamDescription = name + " (" + city + ")"; break;
                case 1: /* european cup */ teamDescription = name + " (" + city + ", " + country + ")"; break;
                case 2: /* international */
                default: teamDescription = name + " (" + QString::number(ranking) + ")";
            }

            teams << teamDescription;
            mapTeams.insert(teamDescription, code);

            // we make use of currently joined TeamInCompetition table to save teams' ratings
            // this avoids another unnecessary join when calling loadTeams()
            teamCodes.insert(QString::number(code), qMakePair<uint8_t, QString>(ranking, group));
        }
        if (teams.isEmpty() || teamCodes.isEmpty())
            throw SelectFromDatabaseReturnedNullException();

        bool ok = false;
        const QString selectedTeam = InputDialog::getItem(_mainWindowHandle, QStringLiteral("Team"),
                                                          QStringLiteral("Select your team:"), teams, &ok, 400);
        if (!ok || selectedTeam.isEmpty())
            throw NoSuppliedValueException();
        myTeamCode = mapTeams[selectedTeam];
    }
    catch (SelectFromDatabaseFailedException & e) {

        qDebug() << e.description();
        QMessageBox::critical(_mainWindowHandle, e.description(), results->errorText());

        delete results;
        return false;
    }
    catch (SelectFromDatabaseReturnedNullException & e) {

        qDebug() << e.description();
        const QString query = QueryErrorText::nullReturned(results->queryText(), bindings.bindings_list());

        this->_clipboard->setText(query);
        QMessageBox::critical(_mainWindowHandle, e.description(), query);

        delete results;
        return false;
    }
    catch (NoSuppliedValueException & e) {

        qDebug() << e.description();

        delete results;
        return false;
    }

    delete results;
    return true;
}

bool Session::loadTeams(Team * & myTeam, const uint16_t myTeamCode, const QMap<QString, QPair<uint8_t, QString>> & teams) {

    QSqlRelationalTableModel * const table = new QSqlRelationalTableModel(nullptr, this->_db->db());

    try {

        const uint16_t relationColumnNo = 2;
        const QString alias = queryRelation.relationTableAlias(relationColumnNo);

        const QString tableName = QStringLiteral("Team");
        const QString retrievedColumns = QStringLiteral("name as country_name,") % alias %
            QStringLiteral(".code as country_code,") % alias % QStringLiteral(".nickname");

        const QPair<uint16_t, QSqlRelation> relation =
            qMakePair<uint16_t, QSqlRelation>(relationColumnNo, QSqlRelation("Country", "code", retrievedColumns));
        const QString selectOnlyTeams = teams.keys().join(',');
        const QString filter = QStringLiteral("Team.code IN (") + selectOnlyTeams + QStringLiteral(")");

        const bool querySuccess = this->_db->executeQueryForModelWithRelation(table, relation, tableName, filter);
        if (!querySuccess)
            throw SelectFromDatabaseFailedException();
        if (table->rowCount() == 0)
            throw SelectFromDatabaseReturnedNullException();

        for (int i = 0; i < table->rowCount(); ++i) {

            const QSqlRecord row = table->record(i);

            const QString name = row.value("name").toString();
            const QString city = row.value("city").toString();

            const Team::TeamType type = static_cast<Team::TeamType>(row.value("type").toUInt());
            const QString abbr = (type == Team::TeamType::NATIONAL) ? row.value("country_code").toString() :
                                 string_functions.abbreviate(name, 11, ' ', city, 3, { "Rugby", "Union", "Sportive" });

            Team * team = new Team(row.value("code").toUInt(), name, abbr, row.value("nickname").toString(),
                                   row.value("country_name").toString(), city, row.value("venue").toString(), type,
                                   row.value("manager").toString(), teams[row.value("code").toString()].first,
                                   teams[row.value("code").toString()].second, row.value("colour").toString());

            this->_teams.push_back(team);
            if (team->code() == myTeamCode)
                myTeam = team;
        }
        if (_teams.size() == 0)
            throw SelectFromDatabaseReturnedNullException();
    }
    catch (SelectFromDatabaseFailedException & e) {

        qDebug() << e.description();
        QMessageBox::critical(_mainWindowHandle, e.description(), QueryErrorText::executionFailed(table->query().lastError()));

        delete table;
        return false;
    }
    catch (SelectFromDatabaseReturnedNullException & e) {

        qDebug() << e.description();
        const QString query = QueryErrorText::nullReturned(table->query().lastQuery());

        this->_clipboard->setText(query);
        QMessageBox::critical(_mainWindowHandle, e.description(), query);

        delete table;
        return false;
    }

    delete table;
    return true;
}

bool Session::loadReferees(const QueryBindings & bindings) {

    QueryResults * results = new QueryResults();

    try {

        const QString queryString = this->_db->loadQueryFromResource(QStringLiteral(":/sql/sql/load_referees.sql"));

        const bool querySuccess = this->_db->executeCustomQuery(queryString, results, bindings);
        if (!querySuccess)
            throw SelectFromDatabaseFailedException();
        if (results->isEmpty())
            throw SelectFromDatabaseReturnedNullException();

        for (auto row: results->rows()) {

            const uint16_t code = row.at(results->columnNo("code")).toUInt();
            const QString firstname = row.at(results->columnNo("firstname")).toString();
            const QString lastname = row.at(results->columnNo("lastname")).toString();
            const QString country = row.at(results->columnNo("country_code")).toString();
            const bool eligibleForDraw = row.at(results->columnNo("eligible")).toBool();

            Referee * const referee = new Referee(code, firstname, lastname, country, eligibleForDraw);
            this->_referees.push_back(referee);
        }
        if (this->_referees.isEmpty())
            throw SelectFromDatabaseReturnedNullException();
    }
    catch (SelectFromDatabaseFailedException & e) {

        qDebug() << e.description();
        QMessageBox::critical(_mainWindowHandle, e.description(), results->errorText());

        delete results;
        return false;
    }
    catch (SelectFromDatabaseReturnedNullException & e) {

        qDebug() << e.description();
        const QString query = QueryErrorText::nullReturned(results->queryText(), bindings.bindings_list());

        this->_clipboard->setText(query);
        const QMessageBox::StandardButton nextAction = QMessageBox::warning(_mainWindowHandle, e.description(),
            query, QMessageBox::Abort | QMessageBox::Ignore, QMessageBox::Abort);

        delete results;
        return (nextAction == QMessageBox::Ignore); // non-fatal
    }

    delete results;
    return true;
}

Match * Session::buildMatchFromRetrievedRecord(const QSqlRecord & record) {

    Team * const hosts = (record.value("hosts_team_code").isNull()) ? nullptr :
                         this->findTeamByCode(record.value("hosts_team_code").toUInt());
    Team * const visitors = (record.value("visitors_team_code").isNull()) ? nullptr :
                            this->findTeamByCode(record.value("visitors_team_code").toUInt());

    // value of referee in db can be null (not assigned)
    bool refereeCodeIsNotNull = false;
    const uint16_t code = record.value("referee_code").toUInt(&refereeCodeIsNotNull);
    Referee * const referee = (refereeCodeIsNotNull) ? this->findRefereeByCode(code) : nullptr;

    // score_hosts and score_visitors ("pointers" to FixtureScore table) can be null (no score = not played yet)
    std::array<bool, 2> scoreIsNotNull = { false };
    record.value("score_hosts").toUInt(&scoreIsNotNull[0]), record.value("score_visitors").toUInt(&scoreIsNotNull[1]);
    const bool storedInDb = scoreIsNotNull.at(0) && scoreIsNotNull.at(1);

    // mark played as true only if score_hosts and score_visitors exist
    const bool played = record.value("played").toBool() && storedInDb;

    Match * const match = new Match(record.value("code").toUInt(), record.value("datetime").toDateTime(), hosts,
                                    visitors, static_cast<MatchType::Type>(record.value("type").toUInt()), referee,
                                    record.value("venue").toString(), played, storedInDb);
    return match;
}

bool Session::loadFixtures(const uint16_t competitionCode) {

    QSqlTableModel * const table = new QSqlTableModel(nullptr, this->_db->db());

    try {

        const uint8_t regularSeasonMatchType = static_cast<uint8_t>(MatchType::Type::REGULAR);

        const QString tableName = QStringLiteral("Fixture");
        const QString filter = QStringLiteral(" type = ") % QString::number(regularSeasonMatchType) %
                               QStringLiteral(" AND competition_code = ") % QString::number(competitionCode);
        const QPair<uint16_t, Qt::SortOrder> sort = qMakePair(1, Qt::AscendingOrder);

        const bool querySuccess = this->_db->executeQueryForModel(table, tableName, filter, sort);
        if (!querySuccess)
            throw SelectFromDatabaseFailedException();
        if (table->rowCount() == 0)
            throw SelectFromDatabaseReturnedNullException();

        for (int i = 0; i < table->rowCount(); ++i) {

            Match * const match = this->buildMatchFromRetrievedRecord(table->record(i));
            this->_fixtures.push_back(match);
        }
        if (this->_fixtures.isEmpty())
            throw SelectFromDatabaseReturnedNullException();
    }
    catch (SelectFromDatabaseFailedException & e) {

        qDebug() << e.description();
        QMessageBox::critical(_mainWindowHandle, e.description(), QueryErrorText::executionFailed(table->query().lastError()));

        delete table;
        return false;
    }
    catch (SelectFromDatabaseReturnedNullException & e) {

        qDebug() << e.description();
        const QString query = QueryErrorText::nullReturned(table->query().lastQuery());

        this->_clipboard->setText(query);
        const QMessageBox::StandardButton nextAction = QMessageBox::warning(_mainWindowHandle, e.description(),
            query, QMessageBox::Abort | QMessageBox::Ignore, QMessageBox::Abort);

        delete table;
        return (nextAction == QMessageBox::Ignore); // non-fatal
    }

    delete table;
    return true;
}

bool Session::loadFixturesPlayoffs(const QString & source_suffix, const QueryBindings & bindings,
    const QStringList & columNames, QVector<QPair<Match * const, QVector<QVariant>>> & columnValues) {

    QueryResults * results = new QueryResults();

    try {

        const QString queryString = this->_db->loadQueryFromResource(
            QStringLiteral(":/sql/sql/load_playoff_fixtures_") % source_suffix % QStringLiteral(".sql"));

        const bool querySuccess = this->_db->executeCustomQuery(queryString, results, bindings);
        if (!querySuccess)
            throw SelectFromDatabaseFailedException();
        if (results->isEmpty())
            throw SelectFromDatabaseReturnedNullException();

        for (auto row: results->rows()) {

            // set values for [Regular|Playoffs]ToPlayoffsRule object
            QVector<QVariant> columnValuesRaw;
            for (auto column: columNames)
                columnValuesRaw.push_back(row.at(results->columnNo(column)));

            Match * const match = this->buildMatchFromRetrievedRecord(results->convertRowToRecord(row));
            this->_fixtures.push_back(match);

            columnValues.push_back(qMakePair<Match * const, QVector<QVariant>>(match, columnValuesRaw));
        }
    }
    catch (SelectFromDatabaseFailedException & e) {

        qDebug() << e.description();
        QMessageBox::critical(_mainWindowHandle, e.description(), results->errorText());

        delete results;
        return false;
    }
    catch (SelectFromDatabaseReturnedNullException & e) {

        qDebug() << e.description();

        delete results;
        return false;
    }

    delete results;
    return true;
}

bool Session::loadFixturesPlayoffs_phase1(const QueryBindings & bindings) {

    const QStringList columnNames = { "hosts_group_name", "hosts_ranking", "visitors_group_code", "visitors_ranking" };
    QVector<QPair<Match * const, QVector<QVariant>>> playoffProperties;

    const bool dataLoaded = this->loadFixturesPlayoffs(QStringLiteral("phase1"), bindings, columnNames, playoffProperties);

    if (dataLoaded) {

        for (auto match: playoffProperties) {

            RegularToPlayoffsRule * const playoffsRule = new RegularToPlayoffsRule();

            playoffsRule->setData(match.second[0].toString(), match.second[1].toUInt(),
                                  match.second[2].toString(), match.second[3].toUInt());

            const QPair<MatchType::ToPlayOff, void *> playoffsRuleWithType =
                qMakePair<MatchType::ToPlayOff, void *>(MatchType::ToPlayOff::FROM_REGULAR, playoffsRule);

            match.first->setPlayoffsRule(playoffsRuleWithType);
        }
    }
    return dataLoaded;
}

bool Session::loadFixturesPlayoffs_phase2(const QueryBindings & bindings) {

    const QStringList columnNames = { "hosts_fixture_code", "hosts_is_winner", "visitors_fixture_code", "visitors_is_winner" };
    QVector<QPair<Match * const, QVector<QVariant>>> playoffProperties;

    const bool dataLoaded = this->loadFixturesPlayoffs(QStringLiteral("phase2"), bindings, columnNames, playoffProperties);

    if (dataLoaded) {

        for (auto match: playoffProperties) {

            PlayoffsToPlayoffsRule * const playoffsRule = new PlayoffsToPlayoffsRule();

            playoffsRule->setData(match.second[0].toUInt(), match.second[1].toBool(),
                                  match.second[2].toUInt(), match.second[3].toBool());

            const QPair<MatchType::ToPlayOff, void *> playoffsRuleWithType =
                qMakePair<MatchType::ToPlayOff, void *>(MatchType::ToPlayOff::FROM_PLAYOFFS, playoffsRule);

            match.first->setPlayoffsRule(playoffsRuleWithType);
        }
    }
    return dataLoaded;
}

bool Session::loadPlayerPositionsList() const {

    QSqlRelationalTableModel * const table = new QSqlRelationalTableModel(nullptr, this->_db->db());

    try {

        const uint16_t relationColumnNo = 3;
        const QString alias = queryRelation.relationTableAlias(relationColumnNo);

        const QString tableName = QStringLiteral("PlayerPosition");
        const QString retrievedColumns = QStringLiteral("name as typename,") % alias % QStringLiteral(".code as typecode");

        const QPair<uint16_t, QSqlRelation> relation = qMakePair<uint16_t, QSqlRelation>
            (relationColumnNo, QSqlRelation("PlayerPositionType", "code", retrievedColumns));
        const QString filter = tableName + QStringLiteral(".code BETWEEN 1 AND 15");

        const bool querySuccess = this->_db->executeQueryForModelWithRelation(table, relation, tableName, filter);
        if (!querySuccess)
            throw SelectFromDatabaseFailedException();
        if (table->rowCount() == 0)
            throw SelectFromDatabaseReturnedNullException();

        for (int i = 0; i < table->rowCount(); ++i) {

            const QSqlRecord row = table->record(i);
            playerPosition_index.addPlayerPosition(
                static_cast<PlayerPosition_index_item::PositionBaseType>(row.value("basetype").toUInt()),
                static_cast<PlayerPosition_index_item::PositionType>(row.value("typecode").toUInt()),
                row.value("typename").toString(), row.value("code").toUInt(),
                row.value("name").toString());
        }
        if (playerPosition_index.isEmpty())
            throw SelectFromDatabaseReturnedNullException();
    }
    catch (SelectFromDatabaseFailedException & e) {

        qDebug() << e.description();
        QMessageBox::critical(_mainWindowHandle, e.description(), QueryErrorText::executionFailed(table->query().lastError()));

        delete table;
        return false;
    }
    catch (SelectFromDatabaseReturnedNullException & e) {

        qDebug() << e.description();
        const QString query = QueryErrorText::nullReturned(table->query().lastQuery());

        this->_clipboard->setText(query);
        QMessageBox::critical(_mainWindowHandle, e.description(), query);

        delete table;
        return false;
    }

    delete table;
    return true;
}

bool Session::loadPlayers(Team * team, const QueryBindings & bindings) {

    QueryResults * results = new QueryResults();

    try {

        const QString queryString = (team->type() == Team::TeamType::CLUB)
            ? this->_db->loadQueryFromResource(QStringLiteral(":/sql/sql/load_club_players.sql"))
            : this->_db->loadQueryFromResource(QStringLiteral(":/sql/sql/load_national_team_players.sql"));

        const bool querySuccess = this->_db->executeCustomQuery(queryString, results, bindings);
        if (!querySuccess)
            throw SelectFromDatabaseFailedException();
        if (results->isEmpty())
            throw SelectFromDatabaseReturnedNullException();

        for (auto row: results->rows()) {

            PlayerPosition_index_item * const currentPosition =
                playerPosition_index.findPlayerPositionByCode(row.at(results->columnNo("position_code")).toUInt());

            const PlayerPosition position(row.at(results->columnNo("position_name")).toString(),
                static_cast<PlayerPosition_index_item::PositionType>(row.at(results->columnNo("type")).toUInt()), currentPosition);

            const QString club = (team->type() == Team::TeamType::CLUB)
                               ? team->name() : row.at(results->columnNo("club_name")).toString();

            Player * player = new Player(position, row.at(results->columnNo("code")).toUInt(),
                row.at(results->columnNo("firstname")).toString(), row.at(results->columnNo("lastname")).toString(),
                row.at(results->columnNo("country_name")).toString(), club, row.at(results->columnNo("caps")).toUInt(),
                row.at(results->columnNo("birthdate")).toDate(), row.at(results->columnNo("captain")).toInt()*(-2)+1,
                row.at(results->columnNo("shirtno")).toUInt());
            team->addPlayer(player);
        }
    }
    catch (SelectFromDatabaseFailedException & e) {

        qDebug() << e.description();
        QMessageBox::critical(_mainWindowHandle, e.description(), results->errorText());

        delete results;
        return false;
    }
    catch (SelectFromDatabaseReturnedNullException & e) {

        qDebug() << e.description();
        const QString query = QueryErrorText::nullReturned(results->queryText(), bindings.bindings_list());

        this->_clipboard->setText(query);
        const QMessageBox::StandardButton nextAction = QMessageBox::warning(_mainWindowHandle, e.description(),
            query, QMessageBox::Abort | QMessageBox::Ignore, QMessageBox::Abort);

        delete results;
        return (nextAction == QMessageBox::Ignore); // non-fatal
    }

    delete results;
    return true;
}

bool Session::loadPlayerAttributes(Player * player, const QueryBindings & bindings) {

    QueryResults * results = new QueryResults();

    try {

        const QString queryString = this->_db->loadQueryFromResource(QStringLiteral(":/sql/sql/load_player_attributes.sql"));

        const bool querySuccess = this->_db->executeCustomQuery(queryString, results, bindings);
        if (!querySuccess)
            throw SelectFromDatabaseFailedException();
        if (results->isEmpty())
            throw SelectFromDatabaseReturnedNullException();

        const QString attributeName = QStringLiteral("attribute_name");
        const QString attributeValue = QStringLiteral("attribute_value");
        QMap<player::Attributes, uint8_t> playerAttributes;
        for (uint8_t i = 0; i < static_cast<uint8_t>(player::Attributes::TOTAL_NUMBER); ++i) {

            const auto attribute = static_cast<player::Attributes>(i);
            playerAttributes.insert(attribute, results->fieldValue(attributeName, attributeValue,
                                    player::attributeColumnNames[attribute]).toUInt());
        }

        PlayerAttributes * const attributes = new PlayerAttributes(player->caps(),
            player->age(this->datetime().systemDate()), player->position()->positionType(),
            this->teamRanking(player->country()), playerAttributes);
        player->changeAttributes(attributes);
    }
    catch (SelectFromDatabaseFailedException & e) {

        qDebug() << e.description();
        QMessageBox::critical(_mainWindowHandle, e.description(), results->errorText());

        delete results;
        return false;
    }
    catch (SelectFromDatabaseReturnedNullException & e) {

        qDebug() << e.description();
        const QString query = QueryErrorText::nullReturned(results->queryText(), bindings.bindings_list());

        this->_clipboard->setText(query);
        const QMessageBox::StandardButton nextAction = QMessageBox::warning(_mainWindowHandle, e.description(),
            query, QMessageBox::Abort | QMessageBox::Ignore, QMessageBox::Abort);

        delete results;
        return (nextAction == QMessageBox::Ignore); // non-fatal
    }

    delete results;
    return true;
}

bool Session::setGameName(QString & gameName, QString & fileName) const {

    try {
        // game name (database file name)
        bool ok = false;
        gameName = QInputDialog::getText(_mainWindowHandle, QStringLiteral("Database name"),
                   QStringLiteral("Insert name for your game database:"), QLineEdit::Normal, QString(), &ok);
        if (!ok || gameName.isEmpty())
            throw NoSuppliedValueException();

        fileName = gameName.trimmed().remove(" ") + DbSettings.FileExtension;
        dBFile gameDbFile(fileName);

        if (gameDbFile.exists()) {

            const QMessageBox::StandardButton overwriteGameFile =
                QMessageBox::question(_mainWindowHandle, QStringLiteral("File already exists"),
                QStringLiteral("Game database with this name already exists. Overwrite?"));
            if (overwriteGameFile == QMessageBox::No)
                throw NoSuppliedValueException();
            if (!gameDbFile.removeFile(this->_db))
                throw FileOperationFailedException();
        }
    }
    catch (FileOperationFailedException & e) {

        qDebug() << e.description();
        QMessageBox::critical(_mainWindowHandle, e.description(), QStringLiteral("File cannot be overwritten."));
        return false;
    }
    catch (NoSuppliedValueException & e) {

        qDebug() << e.description();
        return false;
    }
    return true;
}

bool Session::setManagerName(QString & managerName) const {

    try {
        // manager name
        bool ok = false;
        managerName = QInputDialog::getText(_mainWindowHandle, QStringLiteral("Manager"),
            QStringLiteral("Your name (name of manager):"), QLineEdit::Normal, QString(), &ok);
        if (!ok || managerName.isEmpty())
            throw NoSuppliedValueException();
    }
    catch (NoSuppliedValueException & e) {

        qDebug() << e.description();
        return false;
    }
    return true;
}

void Session::sweepOldDataAndUnusedMemory() {

    _dateTime.clear();
    _referees.clear();
    _teams.clear();
    _fixtures.clear();

    for (const auto & widgetName: on::widgets.values()) {

        QWidget * widget = Handle::getWindowHandle(widgetName);
        if (widget != nullptr)
            delete widget;
    }

    return;
}

bool Session::newGame() {

    // set database name
    QString gameName = QString();
    QString fileName = QString();
    if (!this->setGameName(gameName, fileName))
        return false;

    // set manager name
    QString managerName = QString();
    if (!this->setManagerName(managerName))
        return false;

    // copy system db to game db
    if (!this->copySystemDbFile(fileName))
        return false;

    // connect to game db
    dBFile gameDbFile(fileName);
    if (!this->_db->connectGameDb(gameName))
        return false;

    // delete old data and free memory
    this->sweepOldDataAndUnusedMemory();

    // select competition type (league, cup, ...)
    uint8_t competitionType = 0;
    if (!this->selectCompetitionType(competitionType))
        return false;

    // select competition
    if (!this->selectCompetition(competitionType))
        return false;

    // select team
    uint16_t teamCode;
    QMap<QString, QPair<uint8_t, QString>> teamsInSelectedCompetition;
    QueryBindings teamBindings;
    teamBindings.addBinding(QStringLiteral(":competition"), this->_competition.code());
    if (!this->selectTeam(teamCode, teamsInSelectedCompetition, teamBindings, competitionType))
        return false;

    // load venues

    // load teams
    Team * myTeam = nullptr;
    if (!this->loadTeams(myTeam, teamCode, teamsInSelectedCompetition))
        return false;

    // load referees
    QueryBindings refereeBindings;
    refereeBindings.addBinding(QStringLiteral(":competition"), this->_competition.code());
    if (!this->loadReferees(refereeBindings))
        return false;

    // load fixtures
    if (!this->loadFixtures(this->_competition.code()))
        return false;
    if (this->competition().hasPlayoffs()) {

        QueryBindings fixturesBindings;
        fixturesBindings.addBinding(QStringLiteral(":competition_code"), this->competition().code());
        fixturesBindings.addBinding(QStringLiteral(":match_type"), static_cast<uint8_t>(MatchType::Type::PLAYOFFS));

        if (this->loadFixturesPlayoffs_phase1(fixturesBindings)) {

            this->loadFixturesPlayoffs_phase2(fixturesBindings);
        }
        else {

            const QString dialogDescription = this->competition().name() +
                QStringLiteral(" contains play-offs but no play-off games have been found.");
            QMessageBox::warning(_mainWindowHandle, QStringLiteral("No play-off games found"), dialogDescription);

            this->competition().switchPlayoffsFlag();
        }
    }

    // save configuration
    this->_dateTime.refreshSystemDateAndTime(this->_competition.fromDate().addDays(-10), QTime(8,0));
    this->_config.saveConfiguration(teamCode, myTeam, managerName);

    // load global lists
    if (!playerPosition_index.isEmpty())
        playerPosition_index.clear();
    if (!this->loadPlayerPositionsList())
        return false;

    // load players
    for (auto team: this->_teams) {

        QueryBindings playerBindings;
        playerBindings.addBinding(":team_code", team->code());
        playerBindings.addBinding(":competition_code", this->_competition.code());

        if (!this->loadPlayers(team, playerBindings))
            return false;

        // load players' attributes
        for (auto player: team->squad()) {

            QueryBindings playerBindings;
            playerBindings.addBinding(":player_code", player->code());
            this->loadPlayerAttributes(player, playerBindings);
        }
    }

    return true;
}

bool Session::loadGame() {

    return true;
}

// return value: returns next match from fixtures' list where my team plays (and which hasn't been played yet)
Match * Session::nextMatchMyTeam() const {

    for (const auto & match: _fixtures) {

        if (!match->played() && match->isTeamInPlay(this->config().team()) &&
            (match->date() > this->_dateTime.systemDate() ||
            (match->date() == this->_dateTime.systemDate() && match->time() >= this->_dateTime.systemTime())))
            return match;
    }
    return nullptr;
}

// return value: returns next match from fixtures' list which hasn't been played yet
Match * Session::nextMatchAllTeams() const {

    for (const auto & match: _fixtures) {

        if (!match->played() && (match->date() > this->_dateTime.systemDate() ||
            (match->date() == this->_dateTime.systemDate() && match->time() >= this->_dateTime.systemTime())))
            return match;
    }
    return nullptr;
}

void Session::assignTeamsToPlayoffsMatches(const bool drawNewPlayoffs) {

    Playoffs * const playoffs = new Playoffs(&_fixtures);

    if (drawNewPlayoffs)
        playoffs->drawPlayoffs(this->teams());
    else
        playoffs->assignTeamsForPlayoffsMatches();

    delete playoffs;
    return;
}
