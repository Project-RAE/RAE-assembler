#pragma once
#include "token.hpp"
#include "lexer.hpp"
#include "encoder.hpp" // for Instruction struct
#include <vector>
#include <string>
#include <optional>
#include <unordered_map>

struct ParsedOperand {
    enum Kind { REG, IMM, MEM, LABEL } kind;
    std::string text; // register name, immediate text, or label name
    // For memory operands we store components
    std::optional<std::string> base;   // base register
    std::optional<std::string> index;  // index register
    int scale = 1;
    int64_t disp = 0;
};

struct Instruction {
    std::string mnemonic;
    std::vector<ParsedOperand> operands;
    std::optional<std::string> label; // if this line defines a label
    size_t sourceLine = 0;
};

class Parser {
public:
    explicit Parser(Lexer& lex);
    std::vector<Instruction> parseAll();

private:
    Lexer& lexer;
    Token cur;
    void next();
    Instruction parseLine();
    ParsedOperand parseOperand();
    int64_t parseNumberText(const std::string& s);
};
