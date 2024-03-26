/*******************************************************************************
 Copyright 2022-23 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

// function definitions for (store-to-db-related) functions from session.h header (organizational split)

#include <QDebug>
#include <QMessageBox>
#include <QProgressDialog>
#include <QStringList>
#include "db/builder.h"
#include "player/player_utils.h"
#include "session.h"
#include "shared/error.h"
#include "shared/handle.h"
#include "shared/messages.h"

bool Session::saveFixtures(QueryBuilder * const queryBuilder) {

    const QString saveProgressDescription = _mainWindowHandle->objectName() + QStringLiteral("/saveProgressDescription");

    QString queryString, table;

    try {

        uint16_t matchNo = 0;
        std::array<uint16_t, 2> matchScore;

        uint16_t noOfMatchesStored = 0;
        const uint16_t noOfMatchesToBeStored = std::count_if(this->fixtures()->cbegin(), this->fixtures()->cend(),
                                             [](Match * const match) { return (match->played() && !match->storedInDb()); });

        const QString title = message.displayWithReplace(saveProgressDescription, "saveMatches",
                                                         { QString::number(noOfMatchesToBeStored) });
        QProgressDialog * const saveProgress = new QProgressDialog(title, QString(), 0, noOfMatchesToBeStored, _mainWindowHandle);
        saveProgress->setWindowModality(Qt::WindowModal);
        saveProgress->setValue(noOfMatchesStored);

        for (auto match: this->_fixtures) {

            if (!match->played())
                break;

            // check if match score has been stored already; if yes then skip this match and continue with another one
            if (match->storedInDb())
                { ++matchNo; continue; }

            for (uint8_t i = 0; i < 2; ++i) {

                const MatchType::Location loc = static_cast<MatchType::Location>(i);
                const MatchScore & ms = *(match->score(loc));

                const QStringList valuesList = {

                    QString::number(matchNo*2+i),
                    QString::number(matchScore[i] = ms.points()),
                    QString::number(ms.points(PointEvent::TRY)),
                    QString::number(ms.points(PointEvent::CONVERSION)),
                    QString::number(ms.points(PointEvent::PENALTY)),
                    QString::number(ms.points(PointEvent::DROPGOAL)),
                    QString::number(ms.shootOutGoals()),
                    QString::number(ms.stats<uint16_t>(StatsType::NumberOf::METRES_RUN)),
                    QString::number(ms.stats<uint16_t>(StatsType::NumberOf::METRES_KICKED)),
                    QString::number(ms.tackles(MatchScore::Tackles::COMPLETED)),
                    QString::number(ms.tackles(MatchScore::Tackles::MISSED)),
                    QString::number(ms.stats<uint16_t>(StatsType::NumberOf::CARRIES)),
                    QString::number(ms.passes(MatchScore::Passes::COMPLETED)),
                    QString::number(ms.passes(MatchScore::Passes::MISSED)),
                    QString::number(ms.lineouts(MatchScore::Lineouts::WON)),
                    QString::number(ms.lineouts(MatchScore::Lineouts::LOST)),
                    QString::number(ms.stats<uint8_t>(StatsType::NumberOf::PENALTIES_CAUSED)),
                    QString::number(ms.stats<uint8_t>(StatsType::NumberOf::HANDLING_ERRORS)),
                    QString::number(ms.stats<uint8_t>(StatsType::NumberOf::OFFLOADS)),
                    QString::number(ms.scrums(MatchScore::Scrums::WON)),
                    QString::number(ms.scrums(MatchScore::Scrums::LOST)),
                    QString::number(ms.possession()),
                    QString::number(ms.territory()),
                    QString::number(ms.stats<uint8_t>(StatsType::NumberOf::YELLOW_CARDS)),
                    QString::number(ms.stats<uint8_t>(StatsType::NumberOf::RED_CARDS))
                };

                if (!queryBuilder->buildInsertQuery(queryString, table = "tFixtureScore", &valuesList))
                    throw BuildInsertQueryFailedException();

                if (!this->_db->executeCustomQuery(queryString))
                    throw UpdateDatabaseFailedException();

                match->matchSaved();
            }

            // update fixtures table (score_hosts, score_visitors, played)
            queryString = QString();
            const QStringList valuesList = { QString::number(matchScore[0]), QString::number(matchScore[1]), QString::number(1) };

            QueryCondition condition = QueryCondition("code", "=", match->code());
            if (!queryBuilder->buildUpdateQuery(queryString, table = "tFixture", &valuesList, &condition))
                throw BuildUpdateQueryFailedException();

            if (!this->_db->executeCustomQuery(queryString))
                throw UpdateDatabaseFailedException();

            ++matchNo;
            saveProgress->setValue(++noOfMatchesStored);
        }

        delete saveProgress;
    }
    catch (BuildInsertQueryFailedException & e) {

        qDebug() << e.description();
        QMessageBox::critical(_mainWindowHandle, table, e.description());
        return false;
    }
    catch (BuildUpdateQueryFailedException & e) {

        qDebug() << e.description();
        QMessageBox::critical(_mainWindowHandle, table, e.description());
        return false;
    }
    catch (UpdateDatabaseFailedException & e) {

        qDebug() << e.description();
        QMessageBox::critical(_mainWindowHandle, e.description(), queryString);
        return false;
    }

    return true;
}

bool Session::savePlayers(QueryBuilder * const queryBuilder) {

    const QString saveProgressDescription = _mainWindowHandle->objectName() + QStringLiteral("/saveProgressDescription");

    QString queryString, table;

    try {

        uint16_t noOfPlayersStored = 0;

        auto noOfPlayersInTeam = [](int sum, Team * const team) { return (sum + team->squad().size()); };
        const uint16_t noOfPlayersToBeStored = std::accumulate(this->teams().cbegin(), this->teams().cend(), 0, noOfPlayersInTeam);

        const QString title = message.displayWithReplace(saveProgressDescription, "savePlayers",
                                                         { QString::number(noOfPlayersToBeStored) });
        QProgressDialog * const saveProgress = new QProgressDialog(title, QString(), 0, noOfPlayersToBeStored, _mainWindowHandle);
        saveProgress->setWindowModality(Qt::WindowModal);
        saveProgress->setValue(noOfPlayersStored);

        for (auto team: this->_teams) {
            for (auto player: team->squad()) {

                // points
                if (player->points()->points() != 0) {

                    const QStringList valuesList = {

                        QString::number(player->code()),
                        QString::number(player->points()->getPointsValue(StatsType::NumberOf::TRIES)),
                        QString::number(player->points()->getPointsValue(StatsType::NumberOf::CONVERSIONS)),
                        QString::number(player->points()->getPointsValue(StatsType::NumberOf::PENALTIES)),
                        QString::number(player->points()->getPointsValue(StatsType::NumberOf::DROPGOALS))
                    };

                    if (!queryBuilder->buildInsertQuery(queryString, table = "tPlayerPoints", &valuesList, true))
                        throw BuildInsertQueryFailedException();



                    if (!this->_db->executeCustomQuery(queryString))
                        throw UpdateDatabaseFailedException();
                }

                // stats
                if (!player->stats()->noMatchesPlayed()) {

                    const QStringList valuesList = {

                        QString::number(player->code()),
                        QString::number(player->stats()->getStatsValue(StatsType::NumberOf::GAMES_PLAYED)),
                        QString::number(player->stats()->getStatsValue(StatsType::NumberOf::GAMES_PLAYED_SUB)),
                        QString::number(player->stats()->getStatsValue(StatsType::NumberOf::MINS_PLAYED)),
                        QString::number(player->stats()->getStatsValue(StatsType::NumberOf::YELLOW_CARDS)),
                        QString::number(player->stats()->getStatsValue(StatsType::NumberOf::RED_CARDS)),
                        QString::number(player->stats()->getStatsValue(StatsType::NumberOf::TACKLES_MADE)),
                        QString::number(player->stats()->getStatsValue(StatsType::NumberOf::TACKLES_COMPLETED)),
                        QString::number(player->stats()->getStatsValue(StatsType::NumberOf::HIGH_TACKLES)),
                        QString::number(player->stats()->getStatsValue(StatsType::NumberOf::DANGEROUS_TACKLES)),
                        QString::number(player->stats()->getStatsValue(StatsType::NumberOf::TACKLES_RECEIVED)),
                        QString::number(player->stats()->getStatsValue(StatsType::NumberOf::PASSES_MADE)),
                        QString::number(player->stats()->getStatsValue(StatsType::NumberOf::PASSES_COMPLETED)),
                        QString::number(player->stats()->getStatsValue(StatsType::NumberOf::CARRIES)),
                        QString::number(player->stats()->getStatsValue(StatsType::NumberOf::OFFLOADS)),
                        QString::number(player->stats()->getStatsValue(StatsType::NumberOf::HANDLING_ERRORS)),
                        QString::number(player->stats()->getStatsValue(StatsType::NumberOf::PENALTIES_CAUSED)),
                        QString::number(player->stats()->metresRun()),
                        QString::number(player->stats()->metresKicked())
                    };

                    if (!queryBuilder->buildInsertQuery(queryString, table = "tPlayerStats", &valuesList, true))
                        throw BuildInsertQueryFailedException();

                    if (!this->_db->executeCustomQuery(queryString))
                        throw UpdateDatabaseFailedException();
                }

                // attributes
                if (true) {

                    for (uint8_t i = 0; i < static_cast<uint8_t>(player::Attributes::TOTAL_NUMBER); ++i) {

                        if (!PlayerAttributes::isSkill(static_cast<player::Attributes>(i)))
                            continue;

                        const QStringList valuesList = {
                            QString::number(i), // attribute code
                            QString::number(player->code()),
                            QString::number(player->attribute(static_cast<player::Attributes>(i)))
                        };

                        if (!queryBuilder->buildInsertQuery(queryString, table = "tPlayerAttributes", &valuesList, true))
                            throw BuildInsertQueryFailedException();

                        if (!this->_db->executeCustomQuery(queryString))
                            throw UpdateDatabaseFailedException();
                    }
                }

                saveProgress->setValue(++noOfPlayersStored);
            }
        }

        delete saveProgress;
    }
    catch (BuildInsertQueryFailedException & e) {

        qDebug() << e.description();
        QMessageBox::critical(_mainWindowHandle, table, e.description());
        return false;
    }
    catch (UpdateDatabaseFailedException & e) {

        qDebug() << e.description();
        QMessageBox::critical(_mainWindowHandle, e.description(), queryString);
        return false;
    }

    return true;
}

bool Session::saveGame() {

    QueryBuilder * queryBuilder = new QueryBuilder();

    bool dataStoredToDb = true;
    QMessageBox::StandardButton nextAction = QMessageBox::Ignore;

    if (nextAction == QMessageBox::Ignore && !(dataStoredToDb &= this->saveFixtures(queryBuilder)))
        nextAction = QMessageBox::warning(_mainWindowHandle, QStringLiteral("Fixtures"),
                                          QStringLiteral("SaveFixtures scenario failed."),
                                          QMessageBox::Abort | QMessageBox::Ignore, QMessageBox::Abort);

    if (nextAction == QMessageBox::Ignore && !(dataStoredToDb &= this->savePlayers(queryBuilder)))
        nextAction = QMessageBox::warning(_mainWindowHandle, QStringLiteral("Players"),
                                          QStringLiteral("SavePlayers scenario failed."),
                                          QMessageBox::Abort | QMessageBox::Ignore, QMessageBox::Abort);

    delete queryBuilder;
    return dataStoredToDb;
}
