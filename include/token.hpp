#pragma once
#include <string>

struct Token {
    enum Type { IDENT, NUMBER, COLON, COMMA, LBRACKET, RBRACKET, PLUS, MINUS, MUL, EOL, END, UNKNOWN } type;
    std::string text;
    size_t line = 0;
};
