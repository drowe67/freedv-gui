/* 
 * This file is part of the ezDV project (https://github.com/tmiw/ezDV).
 * Copyright (c) 2023 Mooneer Salem
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FLEX_KEY_VALUE_PARSER_H
#define FLEX_KEY_VALUE_PARSER_H

#include <map>
#include <sstream>

class FlexKeyValueParser
{
public:
    static std::map<std::string, std::string> GetCommandParameters(std::stringstream& ss, char delimiter = ' ');
};

#endif // FLEX_KEY_VALUE_PARSER_H
