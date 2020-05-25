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

#include "output_edit_dialog.hpp"
#include "../util/constants.hpp"
#include "tuna_gui.hpp"
#include "ui_output_edit_dialog.h"
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#ifdef _WIN32
#include <QTextStream>
#endif

output_edit_dialog::output_edit_dialog(edit_mode m, QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::output_edit_dialog)
    , m_mode(m)
{
    ui->setupUi(this);
    ui->txt_format->setText(T_SONG_FORMAT_DEFAULT);
    m_tuna = dynamic_cast<tuna_gui*>(parent);

    if (m == edit_mode::modify) {
        QString format, path;
        bool log_mode = false;
        m_tuna->get_selected_output(format, path, log_mode);
        ui->txt_format->setText(format);
        ui->txt_path->setText(path);
        ui->cb_logmode->setChecked(log_mode);
    }
}

output_edit_dialog::~output_edit_dialog()
{
    delete ui;
}

static inline bool is_valid_file(const QString& file)
{
    bool result = false;
#ifdef _WIN32
    QFile f(file); /* On NTFS file checks don't work unless the file exists */
    result = f.open(QIODevice::WriteOnly | QIODevice::Text);
    f.close();
#else
    QFile test(file);

    result = test.open(QFile::OpenModeFlag::ReadWrite);
    if (result)
        test.close();
#endif
    return result;
}

void output_edit_dialog::on_buttonBox_accepted()
{
    bool empty = ui->txt_format->text().isEmpty();
    bool valid = is_valid_file(ui->txt_path->text());

    if (empty || !valid) {
        QMessageBox::warning(this, T_OUTPUT_ERROR_TITLE, T_OUTPUT_ERROR);
        return; /* Nothing to do */
    }

    if (m_mode == edit_mode::create) {
        m_tuna->add_output(ui->txt_format->text(), ui->txt_path->text(), ui->cb_logmode->isChecked());
    } else {
        m_tuna->edit_output(ui->txt_format->text(), ui->txt_path->text(), ui->cb_logmode->isChecked());
    }
}

void output_edit_dialog::on_pushButton_clicked()
{
    QString path = QFileDialog::getSaveFileName(this, tr(T_SELECT_SONG_FILE), QDir::home().path(),
        tr(FILTER("Text file", "*.txt")));
    ui->txt_path->setText(path);
}
