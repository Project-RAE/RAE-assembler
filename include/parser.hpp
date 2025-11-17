#pragma once
#include "lexer.hpp"
#include "instruction_encoding.hpp"
#include <vector>

class Parser {
public:
    explicit Parser(Lexer& lexer);

    // Parse all tokens into a list of instructions
    std::vector<Instruction> parse();

private:
    Lexer& lexer;

    Token currentToken();

    void advance();
    Instruction parseInstruction();
};