#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

struct Instruction {
    std::string opcode;
    std::vector<std::string> operands;
};

class InstructionEncoder {
public:
    InstructionEncoder();
    std::vector<uint8_t> encode(const Instruction& instr);

private:
    std::unordered_map<std::string, uint8_t> opcodeMap;
};