#pragma once
#include <string>

enum class TokenType {
    IDENTIFIER,
    NUMBER,
    REGISTER,
    LBRACKET, // [
    RBRACKET, // ]
    PLUS,
    MINUS,
    MUL,
    COMMA,
    COLON,
    NEWLINE,
    END_OF_FILE,
    LABEL, // identifier followed by :
    UNKNOWN
};

struct Token {
    TokenType type;
    std::string text;
};
