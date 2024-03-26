/*******************************************************************************
 Copyright 2021 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#ifndef PLAYERVIEW_H
#define PLAYERVIEW_H

#include <QList>
#include <QMetaObject>
#include <QString>
#include <QVector>
#include <QWidget>
#include "ui/shared/objectnames.h"
#include "ui/ui_playerview.h"

class PlayerView: public QWidget {

    Q_OBJECT

    public:
        explicit PlayerView(QWidget * parent): QWidget(parent), _currentFilterValue(QString()) {}
        ~PlayerView() {}

    protected:
        template <typename T>
        T findFieldInRow(QVector<QWidget *> row, const QString & objectName) const {

            for (auto field: row) {

                const QString fieldObjectName = field->objectName();
                if (fieldObjectName.left(fieldObjectName.indexOf(on::sep)) == objectName)
                    return (dynamic_cast<T>(field));
            }
            return nullptr;
        }

        QList<QMetaObject::Connection> connections;
        QString _currentFilterValue;

    private:
        virtual Player * findPlayerByCode(const uint32_t) = 0;

        virtual void connectClickableLables() = 0;
        virtual void disconnectClickableLables() = 0;

    private slots:
        virtual void displayDetailsPanel() = 0;
};

#endif // PLAYERVIEW_H
