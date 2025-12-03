#pragma once
#include "token.hpp"
#include <string>
#include <vector>

class Lexer {
public:
    explicit Lexer(const std::string& src);
    Token nextToken();

private:
    std::string src;
    size_t pos = 0;

    char peek() const;
    char get();
    void skipSpaces();
    Token identifierOrRegister();
    Token numberToken();
};
