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
    // OS-specific opcode maps
    std::unordered_map<std::string, uint8_t> windowsOpcodes;
    std::unordered_map<std::string, uint8_t> linuxOpcodes;
    std::unordered_map<std::string, uint8_t> macosOpcodes;
    
    // Active map
    std::unordered_map<std::string, uint8_t>* currentMap;

    std::string detectOS();
    void selectOpcodeMap(const std::string& os);
};