/*******************************************************************************
 Copyright 2020-23 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include <QInputDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include "fixtureswidget.h"
#include "match/gameplay.h"
#include "match/playoffs.h"
#include "shared/handle.h"
#include "shared/messages.h"

// called from NextMatch mode <=> without ui (when we want to play matches of other teams in the background)
FixturesWidget::FixturesWidget(DateTime & datetime, Match * nextMatch, MatchType::Type * seasonMatchType,
                               Team * const team, Settings * const settings, QVector<Match *> * const fixtures,
                               const QVector<Referee *> & referees):
    ui(nullptr), _myTeam(team), _dateTime(datetime), _fixtures(fixtures), _referees(referees), _nextMatch(nextMatch),
    _settings(settings), _allMatchesMode(false), _matchTypeModeForDisplay(MatchType::Type::UNDEFINED), _competition(nullptr),
    _seasonMatchType(seasonMatchType), _playUntilAtLeastPeriod(MatchPeriod::TimePeriod::UNDETERMINED) {

    this->setObjectName(on::widgets["fixtures_no_ui"]);

    connect(this, SIGNAL(timeShift(const bool)), Handle::getMainWindowHandle(),
                  SLOT(progress(const bool)), Qt::ConnectionType::DirectConnection);
    connect(this, SIGNAL(timeChanged()), Handle::getMainWindowHandle(), SLOT(updateDateAndTimeLabel()));
}

// called from Fixtures <=> with ui (when we want to play matches of other/my team(s) in the foreground)
FixturesWidget::FixturesWidget(QWidget * parent, const Competition & competition, const QVector<Team *> teams,
                               DateTime & datetime, Match * nextMatch, MatchType::Type * seasonMatchType,
                               Team * const team, Settings * const settings, QVector<Match *> * const fixtures,
                               const QVector<Referee *> & referees):
    QWidget(parent), ui(new Ui_FixturesWidget), _myTeam(team), _dateTime(datetime), _fixtures(fixtures), _teams(teams),
    _referees(referees), _nextMatch(nextMatch), _settings(settings), _allMatchesMode(false),
    _matchTypeModeForDisplay(competition.period()), _competition(&competition), _seasonMatchType(seasonMatchType),
    _playUntilAtLeastPeriod(MatchPeriod::TimePeriod::UNDETERMINED) {

    this->setObjectName(on::widgets["fixtures"]);

    ui->setupUi(this, _myTeam, _fixtures, competition);

    connect(ui->switchFixtureTypeButton, &QPushButton::clicked, this, &FixturesWidget::switchFixtureTypeMode);
    connect(ui->playNextMatchButton, &QPushButton::clicked, this, &FixturesWidget::playNextMatch);
    connect(ui->playAllMatchesButton, &QPushButton::clicked, this, &FixturesWidget::playNextMatches);
    connect(ui->playUntilAtLeastShortCut, &QShortcut::activated, this, &FixturesWidget::setPlayUntilAtLeastPeriod);

    connect(this, SIGNAL(timeShift(const bool)), Handle::getMainWindowHandle(),
                  SLOT(progress(const bool)), Qt::ConnectionType::DirectConnection);
    connect(this, SIGNAL(timeChanged()), Handle::getMainWindowHandle(), SLOT(updateDateAndTimeLabel()));

    const QRegularExpression scoreSeparatorRegex = QRegularExpression(on::fixtureswidget.scoreSeparator);
    const QList<ClickableLabel *> scoreSeparatorLables =
        ui->scrollAreaWidget->findChildren<ClickableLabel *>(scoreSeparatorRegex);

    for (auto label: scoreSeparatorLables)
        connect(label, &ClickableLabel::leftClicked, this, &FixturesWidget::displayPeriodDurations);

    // this->dumpObjectInfo();
}

FixturesWidget::~FixturesWidget() {

    if (ui != nullptr)  { // == not in non-interactive mode

        this->updateTime_Rewind();
        delete ui;
    }
}

void FixturesWidget::updateScore(const MatchType::Location team) {

    const QString score = on::fixtureswidget.teamScore[static_cast<uint8_t>(team)];
    QLabel * scoreLabel = this->findWidgetByCode<QLabel *>(this->_nextMatch->code(), score);
    scoreLabel->setText(QString::number(this->_nextMatch->score(team)->points()));
    scoreLabel->repaint();

    return;
}

// if next match's start time is less (earlier) than current time, time is rewound back
void FixturesWidget::updateTime_Rewind() {

    if (this->_nextMatch != nullptr &&
        this->_nextMatch->date() == this->_dateTime.systemDate() &&
        this->_nextMatch->time() < this->_dateTime.systemTime())
        this->_dateTime.refreshSystemDateAndTime(this->_nextMatch->date(), this->_nextMatch->time());

    return;
}

void FixturesWidget::updateTeamNames(QVector<Match *>::iterator fromMatch) {

    for (auto it = fromMatch; it < _fixtures->end(); ++it) {

        Match * match = (*it);
        for (uint8_t i = 0; i < 2; ++i) {

            const MatchType::Location loc = static_cast<MatchType::Location>(i);
            QLabel * flagLabel = this->findWidgetByCode<QLabel *>(match->code(), on::fixtureswidget.teamFlag[loc]);
            QLabel * nameLabel = this->findWidgetByCode<QLabel *>(match->code(), on::fixtureswidget.teamName[loc]);
            ui->setNameAndFlag(this->_myTeam, match, this->_fixtures, loc, flagLabel, nameLabel);
        }
    }
    return;
}

bool FixturesWidget::hasPartOfSeasonFinished(const MatchType::Type period) const {

    return (this->_nextMatch != nullptr && *(this->_seasonMatchType) == period &&
            *(this->_seasonMatchType) != this->_nextMatch->type());
}

// [slot]
// can be called either from signal (QPushButton::clicked) or from this->playNextMatches() function
// or from MainWindow (when playing other teams' matches in the background) <=> in nonInteractiveMode
// return value: true if match was played and next match was found; false otherwise
bool FixturesWidget::playNextMatch(const bool nonInteractiveMode) {

    // if no matches remaining
    // note: skipped for nonInteractive (nonInt) mode; if last match involves my team it wouldn't be played in nonInt
    // mode ever; if last match doesn't involve my team it wouldn't be played in nonInt mode in case there's no match
    // involving my team to be played afterwards (in the same part of season)
    if (!nonInteractiveMode && (_nextMatch == nullptr || _nextMatch->type() != this->_matchTypeModeForDisplay)) {

         QMessageBox::information(this, QStringLiteral("Next match"),
             message.displayWithReplace(this->objectName(), QStringLiteral("noMatchesRemaining"),
             { this->_competition->competitionSeasonDescription(this->_matchTypeModeForDisplay) }));
         return false;
    }

    // update players' feature values which can change over time
    this->updateTime_Rewind();
    emit timeShift(_allMatchesMode);

    // if next match involves my team
    if (_nextMatch->isTeamInPlay(this->_myTeam)) {

        if (nonInteractiveMode)
            return false;

        const QMessageBox::StandardButton proceedToGame =
            QMessageBox::question(this, QStringLiteral("Next match"), message.display(this->objectName(), "myTeamInNextMatch"));

        if (proceedToGame == QMessageBox::No)
            return false;
    }

    // if referee is not assigned draw someone from pool of referees
    if (_nextMatch->refereeNotAssigned()) {

        QVector<Referee *> excludedReferees;
        for (auto fixture: *_fixtures)
            if (fixture->date() == _nextMatch->date() && !fixture->refereeNotAssigned())
                excludedReferees.push_back(fixture->referee());
        _nextMatch->assignReferee(_nextMatch->drawReferee(_referees, excludedReferees));

        if (!nonInteractiveMode && !_nextMatch->refereeNotAssigned()) {

            QLabel * const refereeLabel = this->findWidgetByCode<QLabel *>(this->_nextMatch->code(), on::fixtureswidget.referee);
            refereeLabel->setText(_nextMatch->referee()->referee());
            refereeLabel->repaint();
        }
    }

    // select squads of both teams
    // if my team is involved than force automatic selection only if current selection isn't complete
    Team * const hosts = this->_nextMatch->team(MatchType::Location::HOSTS);
    if ((hosts == this->_myTeam && !hosts->areAllPlayersSelected()) || hosts != this->_myTeam) {

        const bool selectionComplete = hosts->selectPlayersForNextMatch(this->_settings->playerConditions());
        if (!selectionComplete) {

            QMessageBox::information(this, hosts->name() + QStringLiteral(" (hosts)"),
                                     message.display(this->objectName(), "hostsNotComplete"));
            return false;
        }
        hosts->selectSubstitutes(this->_settings->playerConditions());
    }
    Team * const visitors = _nextMatch->team(MatchType::Location::VISITORS);
    if ((visitors == this->_myTeam && !visitors->areAllPlayersSelected()) || visitors != this->_myTeam) {

        const bool selectionComplete = visitors->selectPlayersForNextMatch(this->_settings->playerConditions());
        if (!selectionComplete) {

            QMessageBox::information(this, visitors->name() + QStringLiteral(" (visitors)"),
                                     message.display(this->objectName(), "visitorsNotComplete"));
            return false;
        }
        visitors->selectSubstitutes(this->_settings->playerConditions());
    }

    // update system date/time
    this->_dateTime.refreshSystemDateAndTime(this->_nextMatch->date(), this->_nextMatch->time());
    emit timeChanged();

    // play match
    FixturesWidget * thisWidget = (nonInteractiveMode) ? nullptr : this;
    GamePlay * play = new GamePlay(thisWidget, _settings, _dateTime, this->_nextMatch);
    if (!nonInteractiveMode)
        ui->currentMatchProgress = this->findWidgetByCode<QProgressBar *>(this->_nextMatch->code(), on::fixtureswidget.matchProgress);
    play->playMatch();

    if (!nonInteractiveMode && this->_nextMatch->type() == MatchType::Type::PLAYOFFS && this->_nextMatch->winner() != nullptr) {

        const bool hostsWon = (this->_nextMatch->winner() == this->_nextMatch->team(MatchType::Location::HOSTS));
        const QString objectNamePrefix = on::fixtureswidget.teamName[static_cast<uint8_t>(!hostsWon)];
        QLabel * const teamNameLabel = this->findWidgetByCode<QLabel *>(this->_nextMatch->code(), objectNamePrefix);
        teamNameLabel->setStyleSheet(teamNameLabel->styleSheet() + cc::shared.colour(ss::fixtureswidget.winningTeamColour, cc::colourArea::FONT));

        if (!this->_nextMatch->shootOutResult().isNull()) {

            ClickableLabel * const scoreSeparator =
                this->findWidgetByCode<ClickableLabel *>(this->_nextMatch->code(), on::fixtureswidget.scoreSeparator);
            if (scoreSeparator != nullptr)
                scoreSeparator->setToolTip(this->_nextMatch->shootOutResult());
        }
    }
    delete play;

    // find (new) next match
    bool next = false;
    for (const auto & match: *this->_fixtures) {

        if (next)
            { this->_nextMatch = match; break; }
        if (match == this->_nextMatch) {
            { next = true; this->_nextMatch = nullptr; }
        }
    }

    // draw/update play-offs
    const bool playOffsInProgress = *(this->_seasonMatchType) == MatchType::Type::PLAYOFFS;
    if (playOffsInProgress || (!nonInteractiveMode && this->_competition->hasPlayoffs())) {

        Playoffs * const playoffs = new Playoffs(_fixtures);
        QVector<Match *>::iterator fromMatch = nullptr;

        // next play-offs' rounds
        if (playOffsInProgress)
            playoffs->assignTeamsForPlayoffsMatches(&fromMatch);

        // first play-offs' round
        if (this->hasPartOfSeasonFinished()) {

            if (playoffs->drawPlayoffs(this->_teams, &fromMatch)) {

                QMessageBox::information(this, QStringLiteral("Play-offs"),
                    message.displayWithReplace(this->objectName(), QStringLiteral("teamsToPlayoffsMatchesAssigned"),
                    { this->_competition->name(), this->_competition->competitionSeasonDescription(this->_competition->period()) }));
            }

            *(this->_seasonMatchType) = _nextMatch->type();
            emit timeChanged();
        }

        if (fromMatch != nullptr && fromMatch != _fixtures->end())
            this->updateTeamNames(fromMatch);

        delete playoffs;
    }

    return next;
}

// [slot]
void FixturesWidget::switchFixtureTypeMode() {

    this->_matchTypeModeForDisplay = (this->_matchTypeModeForDisplay == MatchType::Type::REGULAR)
                                   ? MatchType::Type::PLAYOFFS : MatchType::Type::REGULAR;
    ui->switchFixtureTypeButton->setText(_competition->competitionSeasonDescription(this->_matchTypeModeForDisplay));
    ui->playNextMatchButton->setEnabled(this->_matchTypeModeForDisplay == *(this->_seasonMatchType));
    ui->playAllMatchesButton->setEnabled(this->_matchTypeModeForDisplay == *(this->_seasonMatchType));

    typedef Match * const cpMatch;
    for (QMap<HiddenLabel *, QWidget *>::iterator it = ui->rowWidgets.begin(); it != ui->rowWidgets.end(); ++it) {

        const uint16_t code = it.key()->text().toUInt();
        cpMatch * const match = std::find_if(_fixtures->cbegin(), _fixtures->cend(),
                                             [& code](Match * const m) -> bool { return m->code() == code; });

        const bool visibility = ((*match)->type() == this->_matchTypeModeForDisplay);
        (*it)->setVisible(visibility);
    }
    return;
}

// [slot]
void FixturesWidget::playNextMatches() {

    _allMatchesMode = true;
    while (this->playNextMatch());
    _allMatchesMode = false;

    return;
}

// [slot] (for testing purposes only)
void FixturesWidget::setPlayUntilAtLeastPeriod() {

    const QStringList periods = MatchTime::_periodDescriptions.values();
    bool selectedInDialog;

    const QString selectedPeriod = QInputDialog::getItem(nullptr, QStringLiteral("Select period"),
        QStringLiteral("Next match(es) won't stop before selected period."), periods, 0, false, &selectedInDialog);

    if (selectedInDialog)
        this->_playUntilAtLeastPeriod = MatchTime::_periodDescriptions.key(selectedPeriod, MatchPeriod::TimePeriod::UNDETERMINED);

    return;
}

// [slot]
void FixturesWidget::displayPeriodDurations() {

    ClickableLabel * const senderLabel = qobject_cast<ClickableLabel *>(sender());
    const QString sourceObjectName = senderLabel->objectName();
    const QString destinationObjectName = on::fixtureswidget.matchCodeHidden +
                                          sourceObjectName.mid(sourceObjectName.indexOf(on::sep));

    HiddenLabel * const matchCodeHiddenLabel =
        ui->scrollAreaWidget->findChild<HiddenLabel *>(destinationObjectName, Qt::FindDirectChildrenOnly);
    const uint32_t matchCode = matchCodeHiddenLabel->text().toUInt();

    for (auto match: *_fixtures)
        if (match->code() == matchCode) {

            const QString description = (match->played()) ? match->timePlayed().listOfAllPeriods()
                                                          : QStringLiteral("This match hasn't been played yet.");
            QMessageBox::information(this, QStringLiteral("Period durations"), description);
            break;
        }

    return;
}
