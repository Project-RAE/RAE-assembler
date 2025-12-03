#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>
#include "parser.hpp" // ParsedInstruction, ParsedOperand

// Final encoded output chunk per instruction
struct Encoded {
    std::vector<uint8_t> bytes;
};

class InstructionEncoder {
public:
    InstructionEncoder();
    // encode one parsed instruction (needs label address resolution externally for rel32)
    Encoded encodeInstruction(const ParsedInstruction& instr, const std::unordered_map<std::string,uint64_t>& labelAddrs, uint64_t currentAddress);

    // helper to write little-endian
    static void writeLE(std::vector<uint8_t>& out, uint64_t value, size_t bytes);
private:
    // register maps
    std::unordered_map<std::string, uint8_t> reg64;

    // helpers to form REX, ModRM, SIB, etc.
    uint8_t rex(bool w, bool r, bool x, bool b);
    uint8_t modrm(uint8_t mod, uint8_t reg, uint8_t rm);
    uint8_t sib(uint8_t scale, uint8_t index, uint8_t base);

    // encoding helpers
    void encodeMOV(const ParsedInstruction& instr, Encoded& out, const std::unordered_map<std::string,uint64_t>& labels, uint64_t addr);
    void encodeADD(const ParsedInstruction& instr, Encoded& out);
    void encodeSUB(const ParsedInstruction& instr, Encoded& out);
    void encodeJMP(const ParsedInstruction& instr, Encoded& out, const std::unordered_map<std::string,uint64_t>& labels, uint64_t addr);
    void encodeCMP(const ParsedInstruction& instr, Encoded& out);
    void encodeJE(const ParsedInstruction& instr, Encoded& out, const std::unordered_map<std::string,uint64_t>& labels, uint64_t addr);
    void encodeCALL(const ParsedInstruction& instr, Encoded& out, const std::unordered_map<std::string,uint64_t>& labels, uint64_t addr);
    void encodeRET(const ParsedInstruction& instr, Encoded& out);
};