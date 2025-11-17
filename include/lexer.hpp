#pragma once
#include <string>
#include "token.hpp"

class Lexer {
public:
    explicit Lexer(const std::string& src);
    Token nextToken();

private:
    std::string source;
    size_t pos = 0;

    char peek() const;
    char get();
    void skipWhitespace();
};