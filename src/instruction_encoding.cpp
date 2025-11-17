#include "instruction_encoding.hpp"
#include <stdexcept>
#include <cstdlib>
#include <iostream>

InstructionEncoder::InstructionEncoder() {
    //Map mnemonics to opcode numbers
    opcodeMap = {
        {"MOV", 0x01},
        {"ADD", 0x02},
        {"SUB", 0x03},
    };
}

std::vector<uint8_t> InstructionEncoder::encode(const Instruction& instr) {
    std::vector<uint8_t> binary;

    //lookup code
    auto it = opcodeMap.find(instr.opcode);
    if (it == opcodeMap.end()) {
        throw std::runtime_error("Unknown instruction: " + instr.opcode);
    }

    binary.push_back(it->second); //opcode byte

    //encode operands as 1 byte each (simple)
    for (const auto& op : instr.operands) {
        int value = std::stoi(op); //convert string to int
        if (value < 0 || value > 255) {
            throw std::runtime_error("Operand out of range: " + op);
        }
        binary.push_back(static_cast<uint8_t>(value));
    }

    return binary;
}   