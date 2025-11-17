#include "parser.hpp"
#include <stdexcept>

Parser::Parser(Lexer& lexer) : lexer(lexer) {}

Token Parser::currentToken() {
    return lexer.nextToken();
}

void Parser::advance() {
    
}

Instruction Parser::parseInstruction() {
    Instruction instr;

    Token tok = lexer.nextToken();
}