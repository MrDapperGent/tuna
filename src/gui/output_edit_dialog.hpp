/*************************************************************************
 * This file is part of tuna
 * github.con/univrsal/tuna
 * Copyright 2020 univrsal <universailp@web.de>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *************************************************************************/

#pragma once

#include <QDialog>

namespace Ui {
class output_edit_dialog;
}

class tuna_gui;

enum class edit_mode { create,
    modify };

class output_edit_dialog : public QDialog {
    Q_OBJECT

public:
    output_edit_dialog(edit_mode m, QWidget* parent = nullptr);
    ~output_edit_dialog();

private slots:
    void on_buttonBox_accepted();

    void on_pushButton_clicked();

private:
    Ui::output_edit_dialog* ui;
    edit_mode m_mode;
    tuna_gui* m_tuna;
};
