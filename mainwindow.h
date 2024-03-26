/*******************************************************************************
 Copyright 2020-22 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDialog>
#include <QString>
#include <QWidget>
#include "session.h"
#include "ui/ui_mainwindow.h"

class MainWindow: public QDialog {

    Q_OBJECT

    public:
        explicit MainWindow(QWidget * = nullptr);
        ~MainWindow();

        Ui_MainWindow * ui;

    private:
        void enableButtons(const bool) const;

        void removeCurrentWidget();

        QString _widgetInDrawingArea;
        Session * _currentSession;

    public slots:
        void updateDateAndTimeLabel();

    private slots:    
        void userQueryDialog();
        void restoreSystemQueryDialog();

        int progress(const bool = false);
        int about();

        void newgame();
        void loadgame();
        void savegame();

        void fixtures();
        void players();
        void squad();
        void statistics();
        void teams();
        void table();
        void nextMatch();
};

#endif // MAINWINDOW_H
