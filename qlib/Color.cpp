/*
 * Copyright (C) 2009 Kamil Dudka <kdudka@redhat.com>
 *
 * This file is part of cgt (Call Graph Tools).
 *
 * cgt is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * cgt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with cgt.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Color.h"

// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Color implementation
bool Color::useColors = false;
struct Color::Private {
    EColor            color;
};
Color::Color(EColor color):
    d(new Private)
{
    d->color = color;
}
Color::Color(const Color &cObj):
    d(new Private)
{
    d->color = cObj.d->color;
}
Color::~Color() {
    delete d;
}
void Color::enable(bool b) {
    Color::useColors = b;
}
bool Color::isEnabled() {
    return Color::useColors;
}
std::ostream& operator<< (std::ostream &stream, const Color &cObj) {
    if (Color::useColors) {
        static const char ESC = '\033';
        stream << ESC;
        switch (cObj.d->color) {
            case C_NO_COLOR:     stream << "[0m";    break;
            case C_BLUE:         stream << "[0;34m"; break;
            case C_GREEN:        stream << "[0;32m"; break;
            case C_CYAN:         stream << "[0;36m"; break;
            case C_RED:          stream << "[0;31m"; break;
            case C_PURPLE:       stream << "[0;35m"; break;
            case C_BROWN:        stream << "[0;33m"; break;
            case C_LIGHT_GRAY:   stream << "[0;37m"; break;
            case C_DARK_GRAY:    stream << "[1;30m"; break;
            case C_LIGHT_BLUE:   stream << "[1;34m"; break;
            case C_LIGHT_GREEN:  stream << "[1;32m"; break;
            case C_LIGHT_CYAN:   stream << "[1;36m"; break;
            case C_LIGHT_RED:    stream << "[1;31m"; break;
            case C_LIGHT_PURPLE: stream << "[1;35m"; break;
            case C_YELLOW:       stream << "[1;33m"; break;
            case C_WHITE:        stream << "[1;37m"; break;
        }
    }
    return stream;
}

