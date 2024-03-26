/*******************************************************************************
 Copyright 2020-23 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include <QApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include "aboutwindow.h"
#include "fixtureswidget.h"
#include "mainwindow.h"
#include "matchwidget.h"
#include "nextmatchwindow.h"
#include "playerswidget.h"
#include "processwindow.h"
#include "shared/messages.h"
#include "squadwidget.h"
#include "statswidget.h"
#include "tablewidget.h"

MainWindow::MainWindow(QWidget * parent):
    QDialog(parent), ui(new Ui_MainWindow), _widgetInDrawingArea(QString()), _currentSession(new Session(this)) {

    ui->setupUi(this);

    connect(ui->dbQueryShortCut, &QShortcut::activated, this, &MainWindow::userQueryDialog);
    connect(ui->restoreSystemDbShortCut, &QShortcut::activated, this, &MainWindow::restoreSystemQueryDialog);

    connect(ui->dateAndTimeIconLabel, &TimeShiftLabel::leftClicked, this, &MainWindow::progress);
    connect(ui->aboutLabel, &ClickableLabel::leftClicked, this, &MainWindow::about);
    connect(ui->quitLabel, &ClickableLabel::leftClicked, this, &QApplication::quit);

    connect(ui->newGameButton, &QPushButton::clicked, this, &MainWindow::newgame);
    connect(ui->loadGameButton, &QPushButton::clicked, this, &MainWindow::loadgame);
    connect(ui->saveGameButton, &QPushButton::clicked, this, &MainWindow::savegame);

    connect(ui->calendarButton, &QPushButton::clicked, this, &MainWindow::fixtures);
    connect(ui->playersButton, &QPushButton::clicked, this, &MainWindow::players);
    connect(ui->statsButton, &QPushButton::clicked, this, &MainWindow::statistics);
    connect(ui->squadButton, &QPushButton::clicked, this, &MainWindow::squad);
    connect(ui->teamsButton, &QPushButton::clicked, this, &MainWindow::teams);
    connect(ui->tablesButton, &QPushButton::clicked, this, &MainWindow::table);
    connect(ui->nextMatchButton, &QPushButton::clicked, this, &MainWindow::nextMatch);
}

MainWindow::~MainWindow() {

    delete _currentSession;
    delete ui;
}

void MainWindow::enableButtons(const bool enabled) const {

    ui->newGameButton->setEnabled(enabled);
    ui->loadGameButton->setEnabled(enabled);
    ui->saveGameButton->setEnabled(enabled);
    ui->optionsButton->setEnabled(enabled);
    ui->aboutLabel->setEnabled(enabled);
    ui->quitLabel->setEnabled(enabled);
    ui->newsButton->setEnabled(enabled);
    ui->calendarButton->setEnabled(enabled);
    ui->squadButton->setEnabled(enabled);
    ui->trainingButton->setEnabled(enabled);
    ui->tacticsButton->setEnabled(enabled);
    ui->financeButton->setEnabled(enabled);
    ui->teamsButton->setEnabled(enabled);
    ui->tablesButton->setEnabled(enabled);
    ui->playersButton->setEnabled(enabled);
    ui->statsButton->setEnabled(enabled);
    ui->rulesButton->setEnabled(enabled);

    return;
}

void MainWindow::removeCurrentWidget() {

    // setting "intermediary" (empty) widget ensures that previous widget's destructor is called before new widget's constructor
    QWidget * widget = new QWidget();
    this->ui->drawingAreaScrollArea->setWidget(widget);

    for (auto it: this->findChildren<QPushButton *>(QString(), Qt::FindDirectChildrenOnly))
        it->setStyleSheet(styleSheet());

    return;
}

// [slot]
void MainWindow::updateDateAndTimeLabel() {

    const QString date = this->_currentSession->datetime().systemDate().toString(Qt::DefaultLocaleLongDate);
    const QString time = this->_currentSession->datetime().systemTime().toString(Qt::DefaultLocaleShortDate);
    const QString datetime = date + QStringLiteral(", ") + time;

    this->ui->dateAndTimeTextLabel->setText(datetime);
    this->ui->dateAndTimeTextLabel->setToolTip(this->_currentSession->competition().competitionDescription());

    return;
}

// [slot]
int MainWindow::progress(const bool withoutUi) {

    TimeShiftLabel * const senderLabel = qobject_cast<TimeShiftLabel *>(sender());
    const bool runByUser = (senderLabel == ui->dateAndTimeIconLabel);

    if (runByUser)
        this->removeCurrentWidget();

    Match * const nextMatch = this->_currentSession->nextMatchAllTeams();
    int result = 0;

    if (nextMatch != nullptr && this->_currentSession->datetime().systemDate() < nextMatch->date()) {

        ProcessWindow * processWindow = nullptr;

        if (withoutUi) {

            // used when called from FixturesWidget in PlayAllMatches mode
            processWindow = new ProcessWindow(this->_currentSession->datetime(), nextMatch, this->_currentSession->teams());
            processWindow->timeShiftExternal();
            result = 1;
        }
        else {

            // information about events processed on the background are displayed (in dialog)
            processWindow = new ProcessWindow(this->_currentSession->datetime(), nextMatch, this->_currentSession->teams(),
                this->_currentSession->config().team(), this->_currentSession->settings()->messages(), runByUser, this);
            result = processWindow->exec();
        }
        delete processWindow;
    }

    return result;
}

// [slot]
int MainWindow::about() {

    AboutWindow * const aboutWindow = new AboutWindow(this);
    const int result = aboutWindow->exec();
    delete aboutWindow;

    return result;
}

// [slot]
void MainWindow::userQueryDialog() {

    const QString queryString =
        QInputDialog::getText(this, QStringLiteral("SQLite"), QStringLiteral("SQL query to execute:"));

    if (!queryString.isEmpty())
        this->_currentSession->runUserQuery(queryString);

    return;
}

// [slot]
void MainWindow::restoreSystemQueryDialog() {

    const QMessageBox::StandardButton restoreSystemDb =
        QMessageBox::question(this, QStringLiteral("System DB restore"),
            QStringLiteral("Restore system database? This process is irreversible!"));

    if (restoreSystemDb == QMessageBox::Yes) {

        Session::SystemDbRestore restoreOK = this->_currentSession->restoreSystemDb();

        if (restoreOK == Session::SystemDbRestore::RESTORE_FAILED)
            QMessageBox::warning(this, QStringLiteral("System DB restore"),
                QStringLiteral("Restore of system database failed."));
        if (restoreOK == Session::SystemDbRestore::RESTORE_FAILED_ROLLBACK_OK)
            QMessageBox::information(this, QStringLiteral("System DB restore"),
                QStringLiteral("Restore of system database failed. Rollback successful."));
    }

    return;
}

// [slot]
void MainWindow::newgame() {

    this->removeCurrentWidget();
    ui->newGameButton->setStyleSheet(cc::shared.colour(cc::pressedButtonColour));

    const QMessageBox::StandardButton newGame =
        QMessageBox::question(this, QStringLiteral("New game"), QStringLiteral("Setup new game?"));

    if (newGame == QMessageBox::Yes) {

        if (this->_currentSession->newGame())
            this->updateDateAndTimeLabel();
    }

    return;
}

// [slot]
void MainWindow::loadgame() {

    // this->removeCurrentWidget();
    ui->loadGameButton->setStyleSheet(cc::shared.colour(cc::pressedButtonColour));

    /* bool success = */ this->_currentSession->loadGame();

    return;
}

// [slot]
void MainWindow::savegame() {

    // this->removeCurrentWidget();
    ui->saveGameButton->setStyleSheet(cc::shared.colour(cc::pressedButtonColour));

    if (this->_currentSession->saveGame())
        QMessageBox::information(this, QStringLiteral("Save game"), message.display(this->objectName(), "saveGameOK"));
    else
        QMessageBox::critical(this, QStringLiteral("Save game"), message.display(this->objectName(), "saveGameNotOK"));

    ui->saveGameButton->setStyleSheet(styleSheet());

    return;
}

// [slot]
void MainWindow::fixtures() {

    // if game is not loaded
    if (this->_currentSession->config().team() == nullptr) {

        QMessageBox::information(this, QStringLiteral("Fixtures (calendar)"), message.display(this->objectName(), "fixturesNotAvailable"));
        return;
    }

    this->removeCurrentWidget();
    ui->calendarButton->setStyleSheet(cc::shared.colour(cc::pressedButtonColour));

    // next match
    Match * nextMatch = this->_currentSession->nextMatchAllTeams();

    FixturesWidget * fixturesWidget = new FixturesWidget(this->ui->drawingArea, this->_currentSession->competition(),
        this->_currentSession->teams(), this->_currentSession->datetime(), nextMatch,
        this->_currentSession->competition().periodToSwitch(), this->_currentSession->config().team(),
        this->_currentSession->settings(), this->_currentSession->fixtures(), this->_currentSession->referees());

    this->_widgetInDrawingArea = fixturesWidget->objectName();
    this->ui->drawingAreaScrollArea->setWidget(fixturesWidget);

    return;
}

// [slot]
void MainWindow::players() {

    // if game is not loaded
    if (this->_currentSession->config().team() == nullptr) {

        QMessageBox::information(this, QStringLiteral("Players"), message.display(this->objectName(), "playersNotAvailable"));
        return;
    }

    this->removeCurrentWidget();
    ui->playersButton->setStyleSheet(cc::shared.colour(cc::pressedButtonColour));

    PlayersWidget * playersWidget = new PlayersWidget(
        this->ui->drawingArea, this->_currentSession->datetime().systemDate(), this->_currentSession->teams());

    this->_widgetInDrawingArea = playersWidget->objectName();
    this->ui->drawingAreaScrollArea->setWidget(playersWidget);

    return;
}

// [slot]
void MainWindow::squad() {

    // if game is not loaded
    if (this->_currentSession->config().team() == nullptr) {

        QMessageBox::information(this, QStringLiteral("Squad"), message.display(this->objectName(), "squadNotAvailable"));
        return;
    }

    this->removeCurrentWidget();
    ui->squadButton->setStyleSheet(cc::shared.colour(cc::pressedButtonColour));

    SquadWidget * squadWidget = new SquadWidget(this->ui->drawingArea, this->_currentSession->datetime().systemDate(),
        this->_currentSession->config().team(), this->_currentSession->settings()->playerConditions());

    this->_widgetInDrawingArea = squadWidget->objectName();
    this->ui->drawingAreaScrollArea->setWidget(squadWidget);

    return;
}

// [slot]
void MainWindow::statistics() {

    // if game is not loaded
    if (this->_currentSession->config().team() == nullptr) {

        QMessageBox::information(this, QStringLiteral("Statistics"), message.display(this->objectName(), "statsNotAvailable"));
        return;
    }

    this->removeCurrentWidget();
    ui->statsButton->setStyleSheet(cc::shared.colour(cc::pressedButtonColour));

    StatsWidget * statsWidget = new StatsWidget(this->ui->drawingArea, this->_currentSession->teams(), _currentSession->fixtures());

    this->_widgetInDrawingArea = statsWidget->objectName();
    this->ui->drawingAreaScrollArea->setWidget(statsWidget);

    return;
}

// [slot]
void MainWindow::teams() {

    // if game is not loaded
    if (this->_currentSession->config().team() == nullptr) {

        QMessageBox::information(this, QStringLiteral("Teams"), message.display(this->objectName(), "teamsNotAvailable"));
        return;
    }

    this->removeCurrentWidget();
    ui->teamsButton->setStyleSheet(cc::shared.colour(cc::pressedButtonColour));

    return;
}

// [slot]
void MainWindow::table() {

    // if game is not loaded
    if (this->_currentSession->config().team() == nullptr) {

        QMessageBox::information(this, QStringLiteral("Tables"), message.display(this->objectName(), "tableNotAvailable"));
        return;
    }

    this->removeCurrentWidget();
    ui->tablesButton->setStyleSheet(cc::shared.colour(cc::pressedButtonColour));

    TableWidget * tableWidget =
        new TableWidget(this->ui->drawingArea, this->_currentSession->config().team(), this->_currentSession->teams());

    this->_widgetInDrawingArea = tableWidget->objectName();
    this->ui->drawingAreaScrollArea->setWidget(tableWidget);

    return;
}

// [slot]
void MainWindow::nextMatch() {

    // if match is currently under way
    if (this->ui->nextMatchButton->contains("endMatch")) {

        // question dialog - end match (if not finished yet) - TO DO

        this->removeCurrentWidget();
        this->ui->nextMatchButton->setTextEx("nextMatch");
        this->enableButtons(true);

        if (this->_currentSession->competition().hasPlayoffs()) {

            if (this->_currentSession->competition().period() == MatchType::Type::PLAYOFFS)
                this->_currentSession->assignTeamsToPlayoffsMatches(false);
            else {

                Match * const nextMatch = this->_currentSession->nextMatchAllTeams();
                if (nextMatch != nullptr && this->_currentSession->competition().period() != nextMatch->type()) {

                    this->_currentSession->assignTeamsToPlayoffsMatches(true); // == draw Play-offs
                    *(this->_currentSession->competition().periodToSwitch()) = nextMatch->type();
                }
            }
        }

        this->updateDateAndTimeLabel();
        return;
    }

    // if game is not loaded
    if (this->_currentSession->config().team() == nullptr) {

        QMessageBox::information(this, QStringLiteral("Next match"), message.display(this->objectName(), "noNextMatch"));
        return;
    }

    this->removeCurrentWidget();
    ui->nextMatchButton->setStyleSheet(cc::shared.colour(cc::pressedButtonColour));

    Match * nextMatch = this->_currentSession->nextMatchMyTeam();

    // if no matches remaining
    if (nextMatch == nullptr) {

        QMessageBox::information(this, QStringLiteral("Next match"), message.displayWithReplace(this->objectName(),
            "noMatchesRemaining", { this->_currentSession->competition().competitionSeasonDescription() }));
        return;
    }

    // if referee is not assigned draw someone from pool of referees
    if (nextMatch->refereeNotAssigned()) {

        QVector<Referee *> excludedReferees;
        for (auto fixture: *(this->_currentSession->fixtures()))
            if (fixture->date() == nextMatch->date() && !fixture->refereeNotAssigned())
                excludedReferees.push_back(fixture->referee());
        nextMatch->assignReferee(nextMatch->drawReferee(this->_currentSession->referees(), excludedReferees));
    }

    // select squad of opponent's team
    Team * const opponent = (nextMatch->isTeamInPlay(MatchType::Location::HOSTS, this->_currentSession->config().team())) ?
        nextMatch->team(MatchType::Location::VISITORS) : nextMatch->team(MatchType::Location::HOSTS);
    const bool selectionComplete =
        opponent->selectPlayersForNextMatch(this->_currentSession->settings()->playerConditions());

    if (!selectionComplete)
        QMessageBox::information(this, QStringLiteral("Squad selection failed"), message.display(this->objectName(), "noPlayWithoutOpponent"));

    opponent->selectSubstitutes(this->_currentSession->settings()->playerConditions());

    // base match info (dialog)
    SubstitutionRules & substitutionRules = this->_currentSession->settings()->substitutionRules();

    NextMatchWindow * const nextMatchWindow = new NextMatchWindow(nextMatch, this->_currentSession->competition(),
                                                  this->_currentSession->config(), substitutionRules, this);
    const int proceedToMatch = nextMatchWindow->exec();
    delete nextMatchWindow;

    if (proceedToMatch == QDialog::Rejected)
        return;

    if (proceedToMatch == QDialog::Accepted) {

        // if there are other teams' matches to be played before my team's match
        if (nextMatch != this->_currentSession->nextMatchAllTeams()) {

            const QMessageBox::StandardButton generateResults =
                QMessageBox::question(this, QStringLiteral("Generate results and play match"),
                                      message.display(this->objectName(), "otherMatchBeforeMyMatch"));

            if (generateResults == QMessageBox::No)
                return;

            // generate results
            nextMatch = this->_currentSession->nextMatchAllTeams();

            FixturesWidget fixturesWidgetTemp(this->_currentSession->datetime(), nextMatch,
                this->_currentSession->competition().periodToSwitch(), this->_currentSession->config().team(),
                this->_currentSession->settings(), this->_currentSession->fixtures(), this->_currentSession->referees());

            // matches are played one after another until:
            // - there are no matches remaining (in the whole competition) [1] OR
            // - next match involves "my" team (= this match can't be played in non-interactive mode) [2] OR
            // - squad selection for one and/or the other team playing in next match fails [3]
            while (fixturesWidgetTemp.playNextMatch(true));

            if ((nextMatch = this->_currentSession->nextMatchMyTeam()) != this->_currentSession->nextMatchAllTeams())
                // == if [1] or [3]
                return;
        }

        // update players' feature values which can change over time
        this->progress();

        // is squad of opponent's team properly selected?
        const bool opponentTeamSquadComplete = opponent->areAllPlayersSelected();

        if (!opponentTeamSquadComplete) {

            QMessageBox::information(this, QStringLiteral("Squad selection"), message.display(this->objectName(), "opponentNotComplete"));
            return;
        }

        // is squad of my team properly selected?
        const bool myTeamSquadComplete = this->_currentSession->config().team()->areAllPlayersSelected();

        if (!myTeamSquadComplete) {

            QMessageBox::information(this, QStringLiteral("Squad selection"), message.display(this->objectName(), "myTeamNotComplete"));
            return;
        }
    }

    // disable buttons which are not used during match
    this->enableButtons(false);
    this->ui->nextMatchButton->setTextEx("endMatch");

    // update system date
    this->_currentSession->datetime().refreshSystemDateAndTime(nextMatch->date(), nextMatch->time());
    this->updateDateAndTimeLabel();

    // find (new) next match (after this one is played)
    bool next = false;
    Match * matchAfterNextMatch = nullptr;

    for (const auto & match: *(_currentSession->fixtures())) {

        if (next)
            { matchAfterNextMatch = match; break; }
        next = (match == nextMatch);
    }

    MatchWidget * matchWidget = new MatchWidget(this->ui->drawingArea, nextMatch, matchAfterNextMatch,
        this->_currentSession->competition().periodToSwitch(), this->_currentSession->config().team(),
        this->_currentSession->settings(), this->_currentSession->datetime());

    this->_widgetInDrawingArea = matchWidget->objectName();
    this->ui->drawingAreaScrollArea->setWidget(matchWidget);

    return;
}
