/*******************************************************************************
 Copyright 2021-23 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#ifndef ABOUTWINDOW_H
#define ABOUTWINDOW_H

#include <QDialog>
#include <QString>
#include <QWidget>
#include "ui/windows/ui_aboutwindow.h"

class AboutWindow: public QDialog {

    Q_OBJECT

    public:
        explicit AboutWindow(QWidget * = nullptr);
        ~AboutWindow();

    private:
        const QString version = QStringLiteral("0.154");
        const QString email = QStringLiteral("d.neuwirth.cz@gmail.com");

        Ui_AboutWindow * ui;
};

#endif // ABOUTWINDOW_H
