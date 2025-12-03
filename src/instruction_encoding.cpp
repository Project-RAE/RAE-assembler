#include "instruction_encoding.hpp"
#include <stdexcept>
#include <cstdlib>
#include <iostream>

InstructionEncoder::InstructionEncoder() {
    windowsOpcodes = {
        {"MOV", 0x01},
        {"ADD", 0x02},
        {"SUB", 0x03},
        {"FOO", 0x10} // Windows-specific 
    };

    linuxOpcodes = {
        {"MOV", 0x01},
        {"ADD", 0x02},
        {"SUB", 0x03},
        {"FOO", 0x20} // Linux version
    };

    macosOpcodes = {
        {"MOV", 0x01},
        {"ADD", 0x02},
        {"SUB", 0x03},
        {"FOO", 0x30} // macOS version
    };

    std::string os = detectOS();
    selectOpcodeMap(os);
}

std::string InstructionEncoder::detectOS() {
    #if defined(_WIN32) || defined(_WIN64)
        return "Windows";
    #elif defined(__linux__)
        return "Linux";
    #elif defined(__APPLE__)
        return "macOS";
    #else
        return "Unknown";
    #endif
}

void InstructionEncoder::selectOpcodeMap(const std::string& os) {
    if (os == "Windows") currentMap = &windowsOpcodes;
    else if (os == "Linux") currentMap = &linuxOpcodes;
    else if (os == "macOS") currentMap = &macosOpcodes;
    else throw std::runtime_error("Unsupported OS: " + os);
}

std::vector<uint8_t> InstructionEncoder::encode(const Instruction& instr) {
    std::vector<uint8_t> binary;

    //lookup code
    auto it = currentMap->find(instr.opcode);
    if (it == currentMap->end()) {
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