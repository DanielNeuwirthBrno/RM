
QT += core gui sql
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

DEFINES += QT_DEPRECATED_WARNINGS

HEADERS += aboutwindow.h \
           competition.h \
           db/builder.h \
           db/database.h \
           db/query.h \
           db/table.h \
           fixtureswidget.h \
           mainwindow.h \
           match/gameplay.h \
           match/match.h \
           match/matchperiod.h \
           match/matchscore.h \
           match/matchtime.h \
           match/playoff_rules.h \
           match/playoffs.h \
           match/sinbin.h \
           match/substitution.h \
           matchwidget.h \
           nextmatchwindow.h \
           player/player.h \
           player/player_attributes.h \
           player/player_condition.h \
           player/player_health.h \
           player/player_points.h \
           player/player_position.h \
           player/player_stats.h \
           player/player_utils.h \
           player/position_types.h \
           playerswidget.h \
           playerview.h \
           processwindow.h \
           referee.h \
           session.h \
           settings/config.h \
           settings/matchsettings.h \
           settings/playersettings.h \
           shared/constants.h \
           shared/datetime.h \
           shared/error.h \
           shared/file.h \
           shared/handle.h \
           shared/html.h \
           shared/messages.h \
           shared/random.h \
           shared/score.h \
           shared/shared_types.h \
           shared/sort.h \
           shared/texts.h \
           squadswindow.h \
           squadwidget.h \
           statswidget.h \
           tablewidget.h \
           team.h \
           ui/custom/ui_custombutton.h \
           ui/custom/ui_customlabel.h \
           ui/custom/ui_inputdialog.h \
           ui/custom/ui_messagebox.h \
           ui/shared/colours.h \
           ui/shared/objectnames.h \
           ui/shared/stylesheets.h \
           ui/shared/widgetsets.h \
           ui/ui_mainwindow.h \
           ui/ui_playerview.h \
           ui/widgets/ui_fixtureswidget.h \
           ui/widgets/ui_matchwidget.h \
           ui/widgets/ui_playerswidget.h \
           ui/widgets/ui_squadwidget.h \
           ui/widgets/ui_statswidget.h \
           ui/widgets/ui_tablewidget.h \
           ui/windows/ui_aboutwindow.h \
           ui/windows/ui_nextmatchwindow.h \
           ui/windows/ui_processwindow.h \
           ui/windows/ui_squadswindow.h

SOURCES += aboutwindow.cpp \
           config.cpp \
           database.cpp \
           fixtureswidget.cpp \
           gameplay.cpp \
           main.cpp \
           mainwindow.cpp \
           match.cpp \
           matchperiod.cpp \
           matchscore.cpp \
           matchtime.cpp \
           matchwidget.cpp \
           player.cpp \
           player_attributes.cpp \
           player_condition.cpp \
           player_health.cpp \
           player_points.cpp \
           player_position.cpp \
           player_stats.cpp \
           playerswidget.cpp \
           playoff_rules.cpp \
           playoffs.cpp \
           position_types.cpp \
           processwindow.cpp \
           session.cpp \
           session_save.cpp \
           squadwidget.cpp \
           statswidget.cpp \
           tablewidget.cpp \
           team.cpp

RESOURCES += resource.qrc

DISTFILES += notes.txt

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
