#pragma once
#include <string>

enum class TokenType {
    IDENTIFIER,
    NUMBER,
    COMMA,
    NEWLINE,
    END_OF_FILE,
};

struct Token {
    TokenType type;
    std::string value;
};