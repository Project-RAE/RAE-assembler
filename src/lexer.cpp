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

    for (auto &r : regs) if (up == r) return { TokenType::REGISTER, up };

    // if followed by ':' it's a label - but lexer doesn't know next char; parser will handle label detection,
    // but we can return IDENTIFIER and parser will expect COLON token after it.
    return { TokenType::IDENTIFIER, up };
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
    return { TokenType::NUMBER, s };
}

Token Lexer::nextToken() {
    if (pos >= src.size()) return { TokenType::END_OF_FILE, "" };
    skipSpaces();
    char c = peek();
    if (c=='\0') return { TokenType::END_OF_FILE, "" };
    if (c==',') { get(); return { TokenType::COMMA, "," }; }
    if (c=='+') { get(); return { TokenType::PLUS, "+" }; }
    if (c=='-') { get(); return { TokenType::MINUS, "-" }; }
    if (c=='*') { get(); return { TokenType::MUL, "*" }; }
    if (c=='[') { get(); return { TokenType::LBRACKET, "[" }; }
    if (c==']') { get(); return { TokenType::RBRACKET, "]" }; }
    if (c==':') { get(); return { TokenType::COLON, ":" }; }
    if (c=='\n') { get(); return { TokenType::NEWLINE, "\n" }; }
    if (std::isalpha((unsigned char)c) || c=='_') {
        // could be identifier or register
        Token t = identifierOrRegister();
        // check if next non-space char is ':' => label
        size_t save = pos;
        skipSpaces();
        if (peek() == ':') {
            pos = save; // restore
            return Token{TokenType::IDENTIFIER, t.text};
        }
        pos = save;
        return t;
    }
    if (std::isdigit((unsigned char)c)) {
        return numberToken();
    }
    // unknown -> consume
    std::string u; u += get();
    return { TokenType::UNKNOWN, u };
}
