#include "lexer.hpp"
#include <cctype>

Lexer::Lexer(const std::string& src) : source(src) {}

char Lexer::peek() const {
    return pos < source.size() ? source[pos] : '\0';
}

char Lexer::get() {
    return pos < source.size() ? source[pos++] : '\0';
}

void Lexer::skipWhitespace() {
    while (isspace(peek()) && peek() != '\n') get();
}

Token Lexer::nextToken() {
    skipWhitespace();
    char c = peek();

    if (c == '\0') return {TokenType::END_OF_FILE, ""};
    if (c == ',') {
        get();
        return {TokenType::COMMA, ","};
    } 
    if (c == '\n') {
        get();
        return {TokenType::NEWLINE, "\n"};
    }

    if (isalpha(c)) {
        std::string val;
        while (isalnum(peek())) val += get();
        return {TokenType::IDENTIFIER, val};
    }

    if (isdigit(c)) {
        std::string val;
        while (isdigit(peek())) val += get();
        return {TokenType::NUMBER, val};
    }

    // Skip unknown characters
    get();
    return nextToken();
}