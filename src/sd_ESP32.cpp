/**
 * Marlin 3D Printer Firmware
 * Copyright (C) 2016 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (C) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "esp3dlibconfig.h"
#if defined(SDSUPPORT) && defined(ESP3D_WIFISUPPORT)

// Marlin 2.1.x uses cardreader module instead of direct SdFat
#include MARLIN_PATH(module/cardreader.h)

#include "sd_ESP32.h"

// Use Marlin's global cardreader instance
// Note: Marlin 2.1.x removed direct SdFat access, using cardreader API instead
#define marlinCard card

ESP_SD::ESP_SD()
{
    _size = 0;
    _pos = 0;
    _readonly = true;
    _currentFilename = "";
}

ESP_SD::~ESP_SD()
{
    if (isopen()) {
        close();
    }
}

bool ESP_SD::isopen()
{
    return marlinCard.isFileOpen();
}

int8_t ESP_SD::card_status(bool forcemount)
{
    if (!marlinCard.isMounted() || forcemount) {
        marlinCard.mount();
    }
    if (!marlinCard.mediaIsInserted()) {
        return 0;    // No sd
    }
    if (marlinCard.isPrinting() || marlinCard.isFileOpen()) {
        return -1;   // busy
    }
    return 1; // ok
}

bool ESP_SD::open(const char * path, bool readonly)
{
    if (path == NULL) {
        return false;
    }
    // Close any previously opened file
    if (isopen()) {
        close();
    }
    _pos = 0;
    _readonly = readonly;
    _currentFilename = path;

    // Use Marlin's card API to open file
    // Note: Marlin 2.1.x cardreader uses internal file handling
    // The actual file operations are handled by Marlin's internal mechanisms
    marlinCard.closeFile();
    marlinCard.openFile(path, readonly, true);
    return marlinCard.isFileOpen();
}

uint32_t ESP_SD::size()
{
    if (marlinCard.isFileOpen()) {
        _size = marlinCard.getFileSize();
    }
    return _size;
}

uint32_t ESP_SD::available()
{
    if (!marlinCard.isFileOpen() || !_readonly) {
        return 0;
    }
    _size = marlinCard.getFileSize();
    if (_size == 0) {
        return 0;
    }
    return _size - _pos;
}

void ESP_SD::close()
{
    if (marlinCard.isFileOpen()) {
        marlinCard.sync();
        _size = marlinCard.getFileSize();
        marlinCard.closeFile();
    }
    _currentFilename = "";
}

int16_t ESP_SD::write(const uint8_t * data, uint16_t len)
{
    if (!_readonly && marlinCard.isFileOpen()) {
        return marlinCard.write(data, len);
    }
    return 0;
}

int16_t ESP_SD::write(const uint8_t byte)
{
    return write(&byte, 1);
}

bool ESP_SD::exists(const char * path)
{
    if (path == NULL) {
        return false;
    }
    // Use Marlin's fileExists method
    return marlinCard.fileExists(path);
}

bool ESP_SD::dir_exists(const char * path)
{
    if (path == NULL) {
        return false;
    }
    // Use Marlin's dirExists method
    return marlinCard.dirExists(path);
}

bool ESP_SD::remove(const char * path)
{
    if (path == NULL) {
        return false;
    }
    return marlinCard.remove(path);
}

bool ESP_SD::rmdir(const char * path)
{
    if (path == NULL) {
        return false;
    }
    if (strcmp(path, "/") == 0) {
        return false;
    }
    return marlinCard.rmdir(path);
}

bool ESP_SD::mkdir(const char * path)
{
    if (path == NULL) {
        return false;
    }
    return marlinCard.mkdir(path);
}

int16_t ESP_SD::read()
{
    if (!_readonly || !marlinCard.isFileOpen()) {
        return 0;
    }
    uint8_t byte;
    if (marlinCard.read(&byte, 1) == 1) {
        _pos++;
        return byte;
    }
    return -1;
}

uint16_t ESP_SD::read(uint8_t * buf, uint16_t nbyte)
{
    if (!_readonly || !marlinCard.isFileOpen()) {
        return 0;
    }
    int16_t count = marlinCard.read(buf, nbyte);
    if (count > 0) {
        _pos += count;
    }
    return count;
}

String ESP_SD::get_path_part(String data, int index)
{
    int found = 0;
    int strIndex[] = {0, -1};
    int maxIndex;
    String no_res;
    String s = data;
    s.trim();
    if (s.length() == 0) {
        return no_res;
    }
    maxIndex = s.length() - 1;
    if ((s[0] == '/') && (s.length() > 1)) {
        String s2 = &s[1];
        s = s2;
    }
    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (s.charAt(i) == '/' || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i + 1 : i;
        }
    }
    return found > index ? s.substring(strIndex[0], strIndex[1]) : no_res;
}

String ESP_SD::makeshortname(String longname, uint8_t index)
{
    String s = longname;
    String part_name;
    String part_ext;
    // Sanity check name is uppercase and no space
    s.replace(" ", "");
    s.toUpperCase();
    int pos = s.lastIndexOf(".");
    // do we have extension ?
    if (pos != -1) {
        part_name = s.substring(0, pos);
        if (part_name.lastIndexOf(".") != -1) {
            part_name.replace(".", "");
            // trick for short name but force ~1 at the end
            part_name += "       ";
        }
        part_ext = s.substring(pos + 1, pos + 4);
    } else {
        part_name = s;
    }
    // check is under 8 char
    if (part_name.length() > 8) {
        // if not cut and use index
        // check file exists is not part of this function
        part_name = part_name.substring(0, 6);
        part_name += "~" + String(index);
    }
    // remove the possible " " for the trick
    part_name.replace(" ", "");
    // create full short name
    if (part_ext.length() > 0) {
        part_name += "." + part_ext;
    }
    return part_name;
}

String ESP_SD::makepath83(String longpath)
{
    String path;
    String tmp;
    int index = 0;
    tmp = get_path_part(longpath, index);
    while (tmp.length() > 0) {
        path += "/";
        path += makeshortname(tmp);
        index++;
        tmp = get_path_part(longpath, index);
    }
    return path;
}

uint64_t ESP_SD::card_total_space()
{
    uint64_t total = 0;
    marlinCard.getSpaceTotal(total);
    return total;
}

uint64_t ESP_SD::card_used_space()
{
    uint64_t used = 0;
    marlinCard.getSpaceUsed(used);
    return used;
}

bool ESP_SD::openDir(String path)
{
    // Marlin's cardreader handles directory iteration internally
    if (path.length() == 0) {
        path = "/";
    }
    // Use Marlin's cd method to change directory
    marlinCard.cd(path.c_str());
    return true;
}

bool ESP_SD::readDir(char name[13], uint32_t * size, bool * isFile)
{
    if ((name == NULL) || (size == NULL) || (isFile == NULL)) {
        return false;
    }
    *size = 0;
    name[0] = 0;
    *isFile = false;

    // Use Marlin's built-in directory listing via lsPrint
    // Note: Marlin's cardreader stores file info internally
    if (marlinCard.lsPrint()) {
        marlinCard.getFilename(name);
        *size = marlinCard.getFileSize();
        *isFile = marlinCard.isFile();
        marlinCard.nextIndex();
        return true;
    }
    return false;
}

#endif // SDSUPPORT && ESP3D_WIFISUPPORT