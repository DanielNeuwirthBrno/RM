/*******************************************************************************
 Copyright 2020-22 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include "matchwidget.h"
#include "match/match.h"
#include "shared/constants.h"
#include "shared/html.h"
#include "shared/messages.h"
#include "shared/texts.h"
#include "ui/custom/ui_messagebox.h"

MatchWidget::MatchWidget(QWidget * parent, Match * match, Match * nextMatch, MatchType::Type * competitionPeriod,
                         Team * const myTeam, Settings * const settings, DateTime & systemDateAndTime):
    QWidget(parent), ui(new Ui_MatchWidget), _settings(settings), _dateTime(systemDateAndTime),
    _match(match), _nextMatch(nextMatch), _competitionPeriod(competitionPeriod), _myTeam(myTeam),
    _resumePlay(ResumePlay::NO_ACTION), _play(nullptr) {

    this->setObjectName(on::widgets["match"]);

    ui->setupUi(this, match);

    connect(ui->timeIconLabel, &DiagnosticLabel::leftClicked, this, &MatchWidget::playMatch);
    connect(ui->timeIconLabel, &DiagnosticLabel::rightClicked, this, &MatchWidget::playMatchInDiagnosticMode);
    connect(ui->timePlayedLabel, &DiagnosticLabel::leftClicked, this, &MatchWidget::playMatch);
    connect(ui->timePlayedLabel, &DiagnosticLabel::rightClicked, this, &MatchWidget::playMatchInDiagnosticMode);

    html_functions.dummyCallToSuppressCompilerWarning();
}

MatchWidget::~MatchWidget() {

    // if next match's start time is less (earlier) than current time, time is rewound back
    if (_nextMatch != nullptr && // current match was not the last
        _nextMatch->date() == _dateTime.systemDate() && _nextMatch->time() < _dateTime.systemTime())
        this->_dateTime.refreshSystemDateAndTime(this->_nextMatch->date(), this->_nextMatch->time());

    delete ui;

    if (_play != nullptr)
        delete _play;
}

QString MatchWidget::labelName(const QString & prefix, const QString & name) const {

    return (prefix + on::matchwidget.statsLabels[on::matchwidget.position(name)]);
}

QLabel * MatchWidget::findWidgetByObjectName(const QString & objectName, const MatchType::Location team) const {

    const uint8_t column = (team == MatchType::Location::HOSTS) ? 0 : 2;
    for (const auto & row: ui->fields) {

        if (row[column]->objectName() == objectName)
            return row[column];
    }
    return nullptr;
}

void MatchWidget::timeStoppedMessageBox(const QString & key, const QStringList & insertedTexts) {

    this->_resumePlay = ResumePlay::NO_ACTION;
    QString * clickedButtonObjectName = new QString();
    const QString timePlayed = _match->timePlayed().timePlayed();

    TimeStoppedMessageBox messageBox(this->objectName(), key, clickedButtonObjectName, timePlayed, insertedTexts);
    messageBox.exec();

    if (*clickedButtonObjectName == on::timestoppedmessagebox.substitutions)
        _resumePlay = ResumePlay::SUBSTITUTION;
    if (*clickedButtonObjectName == on::timestoppedmessagebox.settings)
        _resumePlay = ResumePlay::SETTINGS;

    delete clickedButtonObjectName;
    return;
}

void MatchWidget::updateStatisticsUI(const MatchType::Location team, const QString & statsLabel,
                                     const QString & newValue, const bool immediateRepaint) const {

    const QString prefix = (team == MatchType::Location::HOSTS) ? on::shared.hostsPrefix : on::shared.visitorsPrefix;

    QLabel * const updateLabel = this->findWidgetByObjectName(this->labelName(prefix, statsLabel), team);
    updateLabel->setText(newValue);

    if (immediateRepaint)
        updateLabel->repaint();

    return;
}

QString MatchWidget::currentScore() const {

    const QString newLogRowScoreUpdate =
        QStringLiteral("Score: ") + this->_match->team(MatchType::Location::HOSTS)->name() +
        QStringLiteral(" ") + QString::number(this->_match->score(MatchType::Location::HOSTS)->points()) +
        QStringLiteral(", ") + this->_match->team(MatchType::Location::VISITORS)->name() +
        QStringLiteral(" ") + QString::number(this->_match->score(MatchType::Location::VISITORS)->points());

    return newLogRowScoreUpdate;
}

void MatchWidget::displayPoints(const QMap<PointEvent, QStringList> & players, const MatchType::Location loc) const {

    QTextEdit * const window = (loc == MatchType::Location::HOSTS) ? ui->hostsPointsTextEdit : ui->visitorsPointsTextEdit;
    window->clear();

    // points
    for (auto it: players.toStdMap()) {

        window->append(html_functions.startTag(html_tags.boldText) % pointEventDesc[it.first] %
                       html_functions.endTag(html_tags.boldText));
        for (auto scorers: it.second)
            window->append(scorers);
        window->append(QString());
    }

    // suspensions
    if (!this->_match->noSuspensions(loc)) {

        window->append(html_functions.startTag(html_tags.boldText) % QStringLiteral("Discipline") %
                       html_functions.endTag(html_tags.boldText));

        for (auto it: this->_match->sinBin()) {

            if (it.team() != loc)
                continue;

            const QString description = (it.outOfPlay())
                ? html_functions.startTag(html_tags.italicText) % it.suspensionInfo() %
                  html_functions.endTag(html_tags.italicText)
                : it.suspensionInfo();
            window->append(description);
        }
        window->append(QString());
    }

    // substitutions
    if (!this->_match->noReplacements(loc)) {

        window->append(html_functions.startTag(html_tags.boldText) % QStringLiteral("Replacements") %
                       html_functions.endTag(html_tags.boldText));

        for (auto it: this->_match->replacements()) {

            if (it.team() != loc)
                continue;
            window->append(it.substitutionInfo());
        }
    }

    window->repaint();

    return;
}

QString MatchWidget::playerForSubstitution(Player * const player) const {

    const QString number = QString::number(player->shirtNo());
    const QString shirtNo = QString(QChar(32)).repeated(2-number.length()) + number;

    const QString description = shirtNo % QChar(32) % player->fullName() %
        string_functions.wrapInBrackets(player->position()->currentPosition()) %
        string_functions.wrapInBrackets(QString::number(player->condition(player::Conditions::FATIGUE)), "[]", false);

    return description;
}

QString MatchWidget::dominationStatsForLog(const double ratio, const uint8_t statType, const bool switchSides) const {

    const QString statistic[] = { QStringLiteral("possesion"), QStringLiteral("territory") };
    const auto ratios = (!switchSides) ? qMakePair<double, double>(ratio, 100-ratio) : qMakePair<double, double>(100-ratio, ratio);

    return statistic[statType] % QStringLiteral(": ") % string_functions.formatNumber<double>(ratios.first) %
           QStringLiteral(" : ") % string_functions.formatNumber<double>(ratios.second) % QStringLiteral(" %");
}

void MatchWidget::logRecord(const QString & text) const {

    if (this->_settings->logging() == LogLevel::NONE)
        return;

    const QString newLogRow = this->_match->timePlayed().timePlayed() + QStringLiteral(" ") + text;
    ui->logWindowTextEdit->append(newLogRow);
    ui->logWindowTextEdit->repaint();

    return;
}

// when to update: match start; player replacement; injury; suspension(in/out)
void MatchWidget::updatePackWeight() const {

    // both packs' weights are always updated (for simplicity)
    const QString packWeightTitle = QStringLiteral("Pack: ");

    { // hosts
        bool isWeightAdjusted = false;

        const uint16_t packWeight = _match->team(MatchType::Location::HOSTS)->packWeight(&isWeightAdjusted);
        const QString circa = (isWeightAdjusted) ? QStringLiteral("~") : QString();

        const QString packWeightText = (packWeight > 0)
                                     ? packWeightTitle % circa % QString::number(packWeight) % QStringLiteral(" kg")
                                     : packWeightTitle + MatchScore::unknownValue;
        this->ui->hostsPackWeightLabel->setText(packWeightText);
    }

    { // visitors
        bool isWeightAdjusted = false;

        const uint16_t packWeight = _match->team(MatchType::Location::VISITORS)->packWeight(&isWeightAdjusted);
        const QString circa = (isWeightAdjusted) ? QStringLiteral("~") : QString();

        const QString packWeightText = (packWeight > 0)
                                     ? packWeightTitle % circa % QString::number(packWeight) % QStringLiteral(" kg")
                                     : packWeightTitle + MatchScore::unknownValue;
        this->ui->visitorsPackWeightLabel->setText(packWeightText);
    }

    return;
}

void MatchWidget::updatePlayer(const QString & playerInPossession, const MatchType::Location loc) const {

    if (loc == MatchType::Location::HOSTS) {

        this->ui->hostsPlayerInPossessionLabel->setText(playerInPossession);
        this->ui->visitorsPlayerInPossessionLabel->clear();
    };

    if (loc == MatchType::Location::VISITORS) {

        this->ui->visitorsPlayerInPossessionLabel->setText(playerInPossession);
        this->ui->hostsPlayerInPossessionLabel->clear();
    }

    this->ui->hostsPlayerInPossessionLabel->repaint();
    this->ui->visitorsPlayerInPossessionLabel->repaint();

    return;
}

// [slot]
void MatchWidget::playMatchInDiagnosticMode() {

    this->_settings->toggleDiagnosticMode(true);
    this->playMatch();
    return;
}

// [slot]
void MatchWidget::playMatch() {

    if (this->_play == nullptr) {
        this->_play = new GamePlay(this, _settings, _dateTime, _match, _myTeam);
    }
    this->_play->playMatch();

    return;
}
