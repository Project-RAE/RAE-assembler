#include "lexer.hpp"
#include <cctype>
#include <algorithm>

Lexer::Lexer(const std::string& s) : src(s), pos(0) {}

char Lexer::peek() const { return pos < src.size() ? src[pos] : '\0'; }
char Lexer::get() { return pos < src.size() ? src[pos++] : '\0'; }

void Lexer::skipSpaces() {
    while (std::isspace((unsigned char)peek())) {
        // keep newline as token, so break on '\n'
        if (peek() == '\n') break;
        get();
    }
}

static std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::toupper(c); });
    return s;
}

Token Lexer::identifierOrRegister() {
    std::string s;
    while (std::isalnum((unsigned char)peek()) || peek() == '_' ) s += get();
    std::string up = toUpper(s);

    // registers - basic list (lower/upper accepted)
    static const std::vector<std::string> regs = {
        "RAX","RCX","RDX","RBX","RSP","RBP","RSI","RDI",
        "R8","R9","R10","R11","R12","R13","R14","R15",
        "EAX","ECX","EDX","EBX","ESP","EBP","ESI","EDI",
        "AX","BX","CX","DX","SI","DI","SP","BP"
    };

    for (auto &r : regs) if (up == r) return { Token::Type::IDENT, up };
    return { Token::Type::IDENT, up };
}

Token Lexer::numberToken() {
    std::string s;
    // support hex 0x..., decimal, negative is handled in parser with MINUS token
    if (peek()=='0' && (pos+1 < src.size()) && (src[pos+1]=='x' || src[pos+1]=='X')) {
        s += get(); // 0
        s += get(); // x
        while (std::isxdigit((unsigned char)peek())) s += get();
    } else {
        while (std::isdigit((unsigned char)peek())) s += get();
    }
    return { Token::Type::NUMBER, s };
}

Token Lexer::nextToken() {
    if (pos >= src.size()) return { Token::Type::END, "" };
    skipSpaces();
    char c = peek();
    if (c=='\0') return { Token::Type::END, "" };
    if (c==',') { get(); return { Token::Type::COMMA, "," }; }
    if (c=='+') { get(); return { Token::Type::PLUS, "+" }; }
    if (c=='-') { get(); return { Token::Type::MINUS, "-" }; }
    if (c=='*') { get(); return { Token::Type::MUL, "*" }; }
    if (c=='[') { get(); return { Token::Type::LBRACKET, "[" }; }
    if (c==']') { get(); return { Token::Type::RBRACKET, "]" }; }
    if (c==':') { get(); return { Token::Type::COLON, ":" }; }
    if (c=='\n') { get(); return { Token::Type::EOL, "\n" }; }
    
    if (std::isalpha((unsigned char)c) || c=='_') {
        Token t = identifierOrRegister();
        size_t save = pos;
        skipSpaces();
        if (peek() == ':') {
            pos = save;
            return Token{Token::Type::IDENT, t.text};
        }
        pos = save;
        return t;
    }
    if (std::isdigit((unsigned char)c)) {
        return numberToken();
    }
    std::string u; u += get();
    return { Token::Type::UNKNOWN, u };
}
