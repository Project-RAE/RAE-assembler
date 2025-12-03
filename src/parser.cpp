#include "parser.hpp"
#include <stdexcept>
#include <sstream>
#include <cctype>

Parser::Parser(Lexer& lex) : lexer(lex) { next(); }

void Parser::next() { cur = lexer.nextToken(); }

int64_t Parser::parseNumberText(const std::string& s) {
    if (s.size() > 1 && s[0]=='0' && (s[1]=='x' || s[1]=='X')) {
        return std::stoll(s, nullptr, 16);
    }
    return std::stoll(s, nullptr, 10);
}

ParsedOperand Parser::parseOperand() {
    ParsedOperand op;
    if (cur.type == TokenType::REGISTER) {
        op.kind = ParsedOperand::REG;
        op.text = cur.text;
        next();
        return op;
    }
    if (cur.type == TokenType::NUMBER) {
        op.kind = ParsedOperand::IMM;
        op.text = cur.text;
        next();
        return op;
    }
    if (cur.type == TokenType::IDENTIFIER) {
        // could be label reference as operand (like JMP label)
        op.kind = ParsedOperand::LABEL;
        op.text = cur.text;
        next();
        return op;
    }
    if (cur.type == TokenType::LBRACKET) {
        // memory operand: [ base + index*scale + disp ]
        next(); // consume [
        // initialize
        op.kind = ParsedOperand::MEM;
        op.scale = 1;
        op.disp = 0;
        bool expectPlus = false;
        while (cur.type != TokenType::RBRACKET && cur.type != TokenType::END_OF_FILE) {
            if (cur.type == TokenType::REGISTER) {
                if (!op.base) op.base = cur.text;
                else if (!op.index) op.index = cur.text;
                else throw std::runtime_error("Too many registers in memory operand");
                next();
            } else if (cur.type == TokenType::MUL) {
                // scale after index: * <number>
                next();
                if (cur.type != TokenType::NUMBER) throw std::runtime_error("Expected number after *");
                op.scale = std::stoi(cur.text);
                next();
            } else if (cur.type == TokenType::PLUS) {
                next();
            } else if (cur.type == TokenType::MINUS) {
                // negative displacement
                next();
                if (cur.type != TokenType::NUMBER) throw std::runtime_error("Expected number after -");
                op.disp -= parseNumberText(cur.text);
                next();
            } else if (cur.type == TokenType::NUMBER) {
                op.disp += parseNumberText(cur.text);
                next();
            } else {
                throw std::runtime_error("Unexpected token in memory operand: " + cur.text);
            }
        }
        if (cur.type != TokenType::RBRACKET) throw std::runtime_error("Unclosed memory bracket");
        next(); // consume ]
        return op;
    }
    throw std::runtime_error("Unexpected token in operand: " + cur.text);
}

ParsedInstruction Parser::parseLine() {
    ParsedInstruction instr;
    instr.sourceLine = 0;
    // skip empty/newline tokens
    while (cur.type == TokenType::NEWLINE) next();
    if (cur.type == TokenType::END_OF_FILE) return instr;

    // label?
    if (cur.type == TokenType::IDENTIFIER) {
        Token look = lexer.nextToken(); // peek next token (lexer has no pushback; we used nextToken)
        // we need a simple lookahead: since our Lexer doesn't support unget, we will instead rely on colon token following
        // But simpler: if next token AFTER identifier is COLON then treat as label.
        // For correctness, let's implement a manual approach: we use current cur and then ask next to see colon.
        // Implementation simplification: assume parser was constructed with lexer at start; here we check next by saving state not supported
    }

    // Simpler approach: we will treat identifier followed by COLON as label if the next token is COLON.
    // Because Lexer can't peek, we simulate by consuming identifier, then check next token.
    if (cur.type == TokenType::IDENTIFIER) {
        std::string id = cur.text;
        next();
        if (cur.type == TokenType::COLON) {
            instr.label = id;
            next(); // consume colon
            // eat rest of line (possible newline)
            while (cur.type == TokenType::NEWLINE) next();
            return instr;
        } else {
            // it was a mnemonic
            instr.mnemonic = id;
        }
    } else if (cur.type == TokenType::IDENTIFIER) {
        instr.mnemonic = cur.text;
        next();
    } else if (cur.type == TokenType::REGISTER || cur.type == TokenType::NUMBER) {
        // invalid as start
        throw std::runtime_error("Expected mnemonic or label, got: " + cur.text);
    } else {
        // if it's something else (like newline) return empty
        if (cur.type == TokenType::END_OF_FILE) return instr;
        throw std::runtime_error("Unexpected token at line start: " + cur.text);
    }

    // parse operands if any
    while (cur.type != TokenType::NEWLINE && cur.type != TokenType::END_OF_FILE) {
        if (cur.type == TokenType::COMMA) { next(); continue; }
        ParsedOperand op = parseOperand();
        instr.operands.push_back(op);
        if (cur.type == TokenType::COMMA) next();
    }

    if (cur.type == TokenType::NEWLINE) next();
    return instr;
}

std::vector<ParsedInstruction> Parser::parseAll() {
    std::vector<ParsedInstruction> out;
    while (cur.type != TokenType::END_OF_FILE) {
        if (cur.type == TokenType::NEWLINE) { next(); continue; }
        ParsedInstruction pi = parseLine();
        // skip empty results
        if (!pi.mnemonic.empty() || pi.label) out.push_back(pi);
    }
    return out;
}
