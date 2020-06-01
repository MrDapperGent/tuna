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

#include "utility.hpp"
#include "../query/music_source.hpp"
#include "config.hpp"
#include "constants.hpp"
#include "format.hpp"
#include <QGuiApplication>
#include <QScreen>

#if DISABLE_TUNA_VLC
bool load_libvlc_module()
{
    return false;
}
bool load_vlc_funcs()
{
    return false;
}
bool load_libvlc()
{
    return false;
}
void unload_libvlc() {}
#else
#include "vlc_internal.h"
#endif
#include <QFile>
#include <QMessageBox>
#include <QTextStream>
#include <ctime>
#include <curl/curl.h>
#include <obs-module.h>
#include <stdio.h>
#include <util/platform.h>

namespace util {

bool vlc_loaded = false;

void load_vlc()
{
#ifndef DISABLE_TUNA_VLC
    auto ver = obs_get_version();
    bool result = true;

    if (obs_get_version() != LIBOBS_API_VER) {
        int major = ver >> 24 & 0xFF;
        int minor = ver >> 16 & 0xFF;
        int patch = ver & 0xFF;
        bwarn("libobs version %d.%d.%d is "
              "invalid. Tuna expects %d.%d.%d for"
              " VLC sources to work",
            major, minor, patch, LIBOBS_API_MAJOR_VER, LIBOBS_API_MINOR_VER, LIBOBS_API_PATCH_VER);

        result = CGET_BOOL(CFG_FORCE_VLC_DECISION);

        /* If this is the first startup with the new version
         * ask user */
        if (!CGET_BOOL(CFG_ERROR_MESSAGE_SHOWN)) {
            result = QMessageBox::question(nullptr, T_ERROR_TITLE, T_VLC_VERSION_ISSUE) == QMessageBox::StandardButton::Yes;
        }

        if (result)
            bwarn("User force enabled VLC support");
        CSET_BOOL(CFG_ERROR_MESSAGE_SHOWN, true);
        CSET_BOOL(CFG_FORCE_VLC_DECISION, result);
    } else {
        /* reset warning config */
        CSET_BOOL(CFG_ERROR_MESSAGE_SHOWN, false);
        CSET_BOOL(CFG_FORCE_VLC_DECISION, false);
    }

    if (result) {
        if (load_libvlc_module() && load_vlc_funcs() && load_libvlc()) {
            binfo("Loaded libVLC. VLC source support enabled");
            vlc_loaded = true;
        } else {
            bwarn("Couldn't load libVLC,"
                  " VLC source support disabled");
        }
    }
#endif
}

void unload_vlc()
{
    unload_libvlc();
    vlc_loaded = false;
}

size_t write_data(void* ptr, size_t size, size_t nmemb, FILE* stream)
{
    size_t written;
    written = fwrite(ptr, size, nmemb, stream);
    return written;
}

bool curl_download(const char* url, const char* path)
{
    CURL* curl = curl_easy_init();
    FILE* fp = nullptr;
#ifdef _WIN32
    wchar_t* wstr = NULL;
    os_utf8_to_wcs_ptr(path, strlen(path), &wstr);
    fp = _wfopen(wstr, L"wb");
    bfree(wstr);
#else
    fp = fopen(path, "wb");
#endif

    bool result = false;
    if (fp && curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
#ifdef DEBUG
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
#endif
        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            berr("Couldn't fetch file from %s to %s", url, path);
        } else {
            result = true;
            bdebug("Fetched %s to %s", url, path);
        }
    }

    if (fp)
        fclose(fp);

    if (curl)
        curl_easy_cleanup(curl);
    return result;
}

void download_lyrics(const song& song)
{
    static QString last_lyrics = "";

    if (song.data() & CAP_LYRICS && last_lyrics != song.lyrics()) {
        last_lyrics = song.lyrics();
        if (!curl_download(qt_to_utf8(song.lyrics()), config::lyrics_path)) {
            berr("Couldn't dowload lyrics from '%s' to '%s'", qt_to_utf8(song.lyrics()), config::lyrics_path);
        }
    }
}

void download_cover(const song& song, bool reset)
{
    static QString last_cover = "";

    if (reset) {
        last_cover = "";
        return;
    }

    if (!config::download_cover || song.cover() == last_cover)
        return;

    auto found_cover = false;
    auto path = utf8_to_qt(config::cover_path);
    auto tmp = path + ".tmp";

    if (song.cover() != "n/a")
        found_cover = curl_download(qt_to_utf8(song.cover()), qt_to_utf8(tmp));

    /* Replace cover only after download is done */
    QFile current(path);
    current.remove();

    if (found_cover) {
        if (QFile::rename(tmp, utf8_to_qt(config::cover_path)))
            last_cover = song.cover();
        else
            berr("Couldn't rename temporary cover file");
    } else if (last_cover != "n/a") {
        last_cover = "n/a";
        set_placeholder(true);
    }
}

void reset_cover()
{
    auto path = utf8_to_qt(config::cover_path);
    QFile current(path);
    current.remove();
    if (!QFile::copy(utf8_to_qt(config::cover_placeholder), path))
        berr("Couldn't move placeholder cover");
}

void write_song(config::output& o, const QString& str)
{
    if (o.last_output == str)
        return;
    o.last_output = str;

    QFile out(o.path);
    bool success = false;
    if (o.log_mode)
        success = out.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append);
    else
        success = out.open(QIODevice::WriteOnly | QIODevice::Text);

    if (success) {
        QTextStream stream(&out);
        stream.setCodec("UTF-8");
        stream << str;
        if (o.log_mode)
            stream << "\n";
        stream.flush();
        out.close();
    } else {
        berr("Couldn't open song output file %s", qt_to_utf8(o.path));
    }
}

void handle_outputs(const song& s)
{
    static QString tmp_text = "";

    for (auto& o : config::outputs) {
        tmp_text.clear();
        tmp_text = o.format;
        format::execute(tmp_text);

        if (tmp_text.isEmpty() || !s.playing()) {
            tmp_text = config::placeholder;
            /* OBS seems to cut leading and trailing spaces
             * when loading the config file so this workaround
             * allows users to still use them */
            tmp_text.replace("%s", " ");
        }
        if (!s.playing() && o.log_mode)
            continue; /* No song playing text doesn't make sense in the log */
        write_song(o, tmp_text);
    }
}

int64_t epoch()
{
    return time(nullptr);
}

bool window_pos_valid(QRect rect)
{
    for (QScreen* screen : QGuiApplication::screens()) {
        if (screen->availableGeometry().intersects(rect))
            return true;
    }
    return false;
}

void set_placeholder(bool on)
{
    static int8_t last_state = -1;

    if (on == last_state)
        return;
    last_state = on;

    if (on) {
        auto path = utf8_to_qt(config::cover_path);
        QFile current(path);
        QString cover_off(path + ".off");
        QFile cover_file(path + "");

        if (cover_file.exists() && !cover_file.remove())
            berr("Couldn't remove '%s'", qt_to_utf8(cover_off));

        if (current.exists() && !current.rename(cover_off))
            berr("Couldn't move '%s' to '%s' file", config::cover_path, qt_to_utf8(cover_off));
        if (!QFile::copy(utf8_to_qt(config::cover_placeholder), path))
            berr("Couldn't copy '%s' to '%s'", config::cover_placeholder, qt_to_utf8(path));
    } else {
        auto path = utf8_to_qt(config::cover_path);
        QFile current(path);
        QString cover_off(path + ".off");
        QFile cover_file(cover_off);

        if (current.exists() && !current.remove())
            berr("Couldn't remove '%s'", qt_to_utf8(path));
        if (cover_file.exists() && !QFile::rename(cover_off, path))
            berr("Couldn't move '%s' to '%s'", qt_to_utf8(cover_off), config::cover_path);
    }
}

} // namespace util
