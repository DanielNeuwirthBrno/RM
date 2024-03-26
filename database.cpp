/*******************************************************************************
 Copyright 2020-23 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QIODevice>
#include <QSqlError>
#include <QSqlRecord>
#include "db/database.h"

const QString Database::SQL_BEGIN_TRAN = QStringLiteral("BEGIN TRANSACTION;");
const QString Database::SQL_COMMIT = QStringLiteral("COMMIT;");
const QString Database::SQL_ROLLBACK = QStringLiteral("ROLLBACK;");

Database::Database(): _dbConnection(new QSqlDatabase()) {}

Database::~Database() {

   if (this->dbConnected())
       this->disconnectDatabase();
    const QString connectionName = this->_dbConnection->connectionName();
    delete _dbConnection;

    if (!connectionName.isEmpty())
        QSqlDatabase::removeDatabase(connectionName);
}

void Database::addDatabase(const QString & name) {

    this->_dbName = name + DbSettings.FileExtension;
    *(_dbConnection) = QSqlDatabase::addDatabase(DbSettings.DbDriver, DbSettings.ConnPrefix + this->_dbName);

    return;
}

bool Database::openDatabase() const {

    if (this->_dbConnection->isOpen())
        this->_dbConnection->close();
    this->_dbConnection->setDatabaseName(_dbName);

    return (this->_dbConnection->open());
}

QString Database::loadQueryFromResource(const QString & resourcePath) const {

    QFile resource(resourcePath);
    resource.open(QIODevice::ReadOnly | QIODevice::Text);
    const QByteArray queryFromResource = resource.readAll();
    QString queryString(queryFromResource);

    return queryString;
}

uint8_t Database::loadQueryFromResource(const QString & resourcePath, QStringList & queries) const {

    QFile resource(resourcePath);
    const bool resourceLoaded = resource.open(QIODevice::ReadOnly | QIODevice::Text);
    if (!resourceLoaded)
        return 0;

    char rawTextInputBuffer[2048];
    QString queryText;

    while (resource.readLine(rawTextInputBuffer, sizeof(rawTextInputBuffer)) > 0) {

        queryText = QString(rawTextInputBuffer).trimmed();
        if (queryText.left(2) != "--") // exclude single-line comments
            queries.append(queryText);
      }

    return (queries.size());
}

bool Database::executeCustomQuery(const QString & queryString, QueryResults * const results,
                                  const QueryBindings & bindings) const {

    QSqlQuery * query = new QSqlQuery(*_dbConnection);

    bool querySuccess = query->prepare(queryString);
    if (querySuccess) {

        // set bindings
        if (bindings.size() > 0) {

            for (auto binding: bindings.bindings()) {

                query->bindValue(binding.first, binding.second);
                qDebug() << binding.first << binding.second;
            }
        }
        // execute query
        querySuccess = query->exec();
    }

    qDebug() << "query:" << query->lastQuery();

    if (querySuccess) {

        if (query->isSelect()) {

            if (results != nullptr) {

                qDebug() << queryExecutionMessage.SelectQueryOK << queryExecutionMessage.ResultsProcessed;
                if (this->noOfRowsSelectedReported())
                    qDebug() << query->size() << "record(s) affected";

                results->setQueryText(query->lastQuery());
                this->processQueryWithResults(query, results);
            }
            else {

                qDebug() << queryExecutionMessage.SelectQueryOK << queryExecutionMessage.ResultsNotProcessed;
                this->processQueryWithoutResults(query);
            }
        }
        else {

            qDebug() << queryExecutionMessage.ModifyQueryOK;
            qDebug() << query->numRowsAffected() << "record(s) affected";

            this->processQueryWithoutResults(query);
        }
    }
    else {

        qDebug() << queryExecutionMessage.QueryNotExecuted;

        if (results != nullptr)
            results->setErrorText(QueryErrorText::executionFailed(query->lastError()));
    }

    delete query;
    return querySuccess;
}

bool Database::executeQueryForModel(QSqlTableModel * const table, const QString & tableName,
    const QString & filter, const QPair<uint16_t, Qt::SortOrder> & sort) const {

    table->setTable(tableName);
    if (!filter.isEmpty())
        table->setFilter(filter);
    if (sort != QPair<uint16_t, Qt::SortOrder>())
        table->setSort(sort.first, sort.second);

    const bool querySucess = table->select();

    qDebug() << "query:" << table->query().lastQuery();
    qDebug() << ((querySucess) ? queryExecutionMessage.SelectQueryOK : queryExecutionMessage.SelectQueryNotOK);

    return querySucess;
}

bool Database::executeQueryForModelWithRelation(QSqlRelationalTableModel * const table,
    const QPair<uint16_t, QSqlRelation> & relation, const QString & tableName, const QString & filter,
    const QPair<uint16_t, Qt::SortOrder> & sort) const {

    table->setTable(tableName);
    table->setRelation(relation.first, relation.second);
    table->setJoinMode(QSqlRelationalTableModel::LeftJoin);
    if (!filter.isEmpty())
        table->setFilter(filter);
    if (sort != QPair<uint16_t, Qt::SortOrder>())
        table->setSort(sort.first, sort.second);

    const bool querySucess = table->select();

    qDebug() << table->query().lastQuery();
    qDebug() << ((querySucess) ? queryExecutionMessage.SelectQueryOK : queryExecutionMessage.SelectQueryNotOK);

    return querySucess;
}

bool Database::processQueryWithResults(QSqlQuery * const query, QueryResults * const results) const {

    bool recordRetrieved = query->first();
    const uint16_t noOfColumns = query->record().count();
    // set header (column names)
    for (int column = 0; column < noOfColumns; ++column)
        results->addRecordToHeader(query->record().fieldName(column));

    // table rows
    while (recordRetrieved) {

        // table columns
        QVector<QVariant> row;
        for (int column = 0; column < noOfColumns; ++column)
            if (query->value(column).isValid())
                row.push_back(query->value(column));

        results->addRecordToRows(row);
        row.clear();

        recordRetrieved = query->next();
    }
    return recordRetrieved;
}

bool Database::processQueryWithoutResults(QSqlQuery * const query) const {

    // number of affected rows
    return (static_cast<bool>(query->numRowsAffected()));
}
