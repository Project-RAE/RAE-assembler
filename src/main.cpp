#include "instruction_encoding.hpp"
#include <iostream>

int main() {
    InstructionEncoder encoder;

    Instruction instr;
    instr.opcode = "MOV";
    instr.operands = {"1", "2"};

    auto bytes = encoder.encode(instr);

    std::cout << "Encoded bytes: ";
    for (auto b : bytes) std::cout << std::hex << (int)b << " ";
    std::cout << "\n";

    return 0;
}