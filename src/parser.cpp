#include "parser.hpp"
#include <stdexcept>
#include <sstream>
#include <cctype>
#include <cstdint>
#include <unordered_set>

Parser::Parser(Lexer& lex) : lexer(lex), cur(lexer.nextToken()) {}

void Parser::next() {
    cur = lexer.nextToken();
}

int64_t Parser::parseNumberText(const std::string& s) {
    if (s.size() > 1 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        return std::stoll(s, nullptr, 16);
    }
    return std::stoll(s, nullptr, 10);
}

ParsedOperand Parser::parseOperand() {
    ParsedOperand op;
    op.kind = ParsedOperand::IMM;
    op.text = "";

    // Handle memory operands: [base + index*scale + disp]
    if (cur.type == Token::LBRACKET) {
        op.kind = ParsedOperand::MEM;
        next();

        // Parse base register
        if (cur.type == Token::IDENT) {
            op.base = cur.text;
            next();
        }

        // Parse index, scale, displacement
        while (cur.type != Token::RBRACKET) {
            if (cur.type == Token::PLUS) {
                next();
                if (cur.type == Token::IDENT) {
                    op.index = cur.text;
                    next();
                    // Check for *scale
                    if (cur.type == Token::MUL) {
                        next();
                        if (cur.type == Token::NUMBER) {
                            op.scale = (int)parseNumberText(cur.text);
                            next();
                        }
                    }
                } else if (cur.type == Token::NUMBER) {
                    op.disp += parseNumberText(cur.text);
                    next();
                }
            } else if (cur.type == Token::MINUS) {
                next();
                if (cur.type == Token::NUMBER) {
                    op.disp -= parseNumberText(cur.text);
                    next();
                }
            } else {
                next();
            }
        }
        if (cur.type == Token::RBRACKET) next();
        return op;
    }

    // Handle registers and labels
    if (cur.type == Token::IDENT) {
        op.text = cur.text;
        
        // Check if it's a known register
        static const std::unordered_set<std::string> registers = {
            "RAX","RCX","RDX","RBX","RSP","RBP","RSI","RDI",
            "R8","R9","R10","R11","R12","R13","R14","R15",
            "EAX","ECX","EDX","EBX","ESP","EBP","ESI","EDI",
            "AX","BX","CX","DX","SI","DI","SP","BP"
        };
        
        if (registers.find(op.text) != registers.end()) {
            op.kind = ParsedOperand::REG;
        } else {
            op.kind = ParsedOperand::LABEL;
        }
        
        next();
        return op;
    }

    // Handle immediates (numbers)
    if (cur.type == Token::NUMBER) {
        op.text = cur.text;
        op.kind = ParsedOperand::IMM;
        next();
        return op;
    }

    // Handle minus sign for negative immediates
    if (cur.type == Token::MINUS) {
        next();
        if (cur.type == Token::NUMBER) {
            op.text = "-" + cur.text;
            op.kind = ParsedOperand::IMM;
            next();
            return op;
        }
    }

    throw std::runtime_error("Invalid operand");
}

ParsedInstruction Parser::parseLine() {
    ParsedInstruction instr;
    instr.sourceLine = cur.line;

    // Check for label (identifier followed by colon)
    if (cur.type == Token::IDENT) {
        std::string potentialLabel = cur.text;
        next();
        if (cur.type == Token::COLON) {
            // This is a label
            instr.label = potentialLabel;
            next();
            // After label, check if there's an instruction on the same line
            if (cur.type == Token::IDENT) {
                instr.mnemonic = cur.text;
                next();
                // Parse operands
                while (cur.type != Token::EOL && cur.type != Token::END) {
                    instr.operands.push_back(parseOperand());
                    if (cur.type == Token::COMMA) {
                        next();
                    }
                }
            }
            // If no instruction after label, mnemonic stays empty
            return instr;
        } else {
            // Not a label, potentialLabel is the mnemonic
            instr.mnemonic = potentialLabel;
            // Parse operands
            while (cur.type != Token::EOL && cur.type != Token::END) {
                instr.operands.push_back(parseOperand());
                if (cur.type == Token::COMMA) {
                    next();
                }
            }
            return instr;
        }
    }

    // If we get here with no identifier, skip to next line
    return instr;
}

std::vector<ParsedInstruction> Parser::parseAll() {
    std::vector<ParsedInstruction> result;
    
    while (cur.type != Token::END) {
        if (cur.type == Token::EOL) {
            next();
            continue;
        }
        
        ParsedInstruction instr = parseLine();
        result.push_back(instr);
        
        // Skip to next line
        if (cur.type == Token::EOL) {
            next();
        }
    }
    
    return result;
}