/*******************************************************************************
 Copyright 2020-21 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#ifndef SQUADWIDGET_H
#define SQUADWIDGET_H

#include <QDate>
#include <QVector>
#include <QWidget>
#include "team.h"
#include "ui/widgets/ui_squadwidget.h"

class SquadWidget: public QWidget {

    Q_OBJECT

    public:
        Q_DISABLE_COPY(SquadWidget)

        explicit SquadWidget(QWidget *, const QDate &, Team * const, const ConditionWeights &);
        ~SquadWidget() { delete ui; }

        Player * findPlayerByCode(const uint32_t);

        template <typename T>
        T findFieldInRow(QVector<QWidget *> row, const QString & objectName) const {

            for (auto field: row) {

                const QString fieldObjectName = field->objectName();
                if (fieldObjectName.left(fieldObjectName.indexOf(on::sep)) == objectName)
                    return (dynamic_cast<T>(field));
            }
            return nullptr;
        }

        template <typename T>
        T findWidgetByPlayer(Player * const player, const QString & objectName) const {

            for (auto field: *ui->fields) {

                // this construct assumes that playerCodeHiddenLabel is always the last widget in row
                HiddenLabel * const codeLabel = dynamic_cast<HiddenLabel *>(field.last());

                if (codeLabel->text().toUInt() == player->code())
                    return (this->findFieldInRow<T>(field, objectName));
            }
            return nullptr;
        }

        template <typename T, typename S>
        T findWidgetBySourceWidget(S const source, const QString & objectName) {

            const QString sourceObjectName = source->objectName();
            const QString destObjectName = objectName + sourceObjectName.mid(sourceObjectName.indexOf(on::sep));

            return (ui->scrollAreaWidget->findChild<T>(destObjectName, Qt::FindDirectChildrenOnly));
        }

    private:
        void displayPlayerAttributesAverages() const;
        void updateElementsAfterPositionChanged(ClickableLabel * const, Player * const, PlayerPosition_index_item * const);

        Ui_SquadWidget * ui;

        Team * _myTeam;
        bool _benchSelection;
        ConditionWeights _conditionSettings;
        QVector<Player *> _players;
        Player * _currentPlayer;

    private slots:
        void selectIntoSquad(const int);
        void displayPlayerAttributes();
        void changePlayerPreference(const bool);
        bool changeCurrentPositionStandard();
        bool changeCurrentPositionExtended(ClickableLabel * = nullptr);

        void setSelectionProperties(const bool = false);
        void resetSelectionProperties(const bool);
        void automaticSelection();
};

#endif // SQUADWIDGET_H
