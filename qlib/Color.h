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

#ifndef COLOR_H
#define COLOR_H

#include <iostream>

/// Console ouput colors enumeration. Zero means default color.
enum EColor {
    C_NO_COLOR = 0,
    C_BLUE,
    C_GREEN,
    C_CYAN,
    C_RED,
    C_PURPLE,
    C_BROWN,
    C_LIGHT_GRAY,
    C_DARK_GRAY,
    C_LIGHT_BLUE,
    C_LIGHT_GREEN,
    C_LIGHT_CYAN,
    C_LIGHT_RED,
    C_LIGHT_PURPLE,
    C_YELLOW,
    C_WHITE
};

/**
 * @attention Global variable is used inside this class.
 * @brief Console output color manipulator
 */
class Color {
    public:
        /**
         * @param color Desired color of console output. If omitted, default
         * color is assumed.
         */
        Color(EColor color = C_NO_COLOR);
        Color(const Color &);
        ~Color();
        /**
         * @attention Global variable is used inside this class.
         * @brief Enable/disable color ouput @b glaobally.
         */
        static void enable(bool);
        /**
         * @return Returns true if color output is @b gloablly enabled.
         */
        static bool isEnabled();
    private:
        Color& operator= (const Color &);
        static bool useColors;
        struct Private;
        Private *d;
        friend std::ostream& operator<< (std::ostream &, const Color &);
};
/// This behaves as standard stream manipulators.
std::ostream& operator<< (std::ostream &, const Color &);

#endif /* COLOR_H */
