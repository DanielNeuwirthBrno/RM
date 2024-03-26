/*******************************************************************************
 Copyright 2020-23 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

/* Application:     Rugby Union Manager
 * Version:         0.154
 * Worktime:        614 h
 *
 * Author:          Daniel Neuwirth
 * E-mail:          d.neuwirth.cz@gmail.com
 * Created:         2020-12-04
 * Modified:        2023-04-09
 *
 * IDE/framework:   Qt 5.14.0
 * Compiler:        MinGW 7.3.0 32-bit
 * Language:        C++11
 */

#include <QApplication>
#include <QLocale>
#include "mainwindow.h"

int main(int argc, char * argv[]) {

    Q_INIT_RESOURCE(resource);

    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedKingdom));

    QApplication app(argc, argv);

    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
