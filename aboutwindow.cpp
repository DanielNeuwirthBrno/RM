/*******************************************************************************
 Copyright 2021 Daniel Neuwirth
 This program is distributed under the terms of the GNU General Public License.
*******************************************************************************/

#include "aboutwindow.h"

AboutWindow::AboutWindow(QWidget * parent):
    QDialog(parent), ui(new Ui_AboutWindow) {

    ui->setupUi(this, version, email);

    connect(ui->pushButton_OK, &QPushButton::clicked, this, &AboutWindow::close);
}

AboutWindow::~AboutWindow() {

    delete ui;
}
