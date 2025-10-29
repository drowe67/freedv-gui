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

#include "FlexKeyValueParser.h"

std::map<std::string, std::string> FlexKeyValueParser::GetCommandParameters(std::stringstream& ss, char delimiter)
{
    std::string word = "";
    std::map<std::string, std::string> ret;

    while (std::getline(ss, word, delimiter)) 
    {
        std::stringstream wordStream(word);
        std::string parameter = "";
        std::string value = "";
    
        std::getline(wordStream, parameter, '=');
        std::getline(wordStream, value);
        ret[parameter] = value;
    }

    return ret;
}
