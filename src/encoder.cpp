#include "encoder.hpp"
#include <stdexcept>
#include <cmath>
#include <iostream>
#include <cstring>

InstructionEncoder::InstructionEncoder() {
    // basic reg64 map
    reg64 = {
        {"RAX",0},{"RCX",1},{"RDX",2},{"RBX",3},{"RSP",4},{"RBP",5},{"RSI",6},{"RDI",7},
        {"R8",8},{"R9",9},{"R10",10},{"R11",11},{"R12",12},{"R13",13},{"R14",14},{"R15",15}
    };
}

uint8_t InstructionEncoder::rex(bool w, bool r, bool x, bool b) {
    return 0x40 | (w?0x08:0) | (r?0x04:0) | (x?0x02:0) | (b?0x01:0);
}
uint8_t InstructionEncoder::modrm(uint8_t mod, uint8_t reg, uint8_t rm) {
    return (uint8_t)(((mod&0x3)<<6) | ((reg&0x7)<<3) | (rm&0x7));
}
uint8_t InstructionEncoder::sib(uint8_t scale, uint8_t index, uint8_t base) {
    return (uint8_t)(((scale&0x3)<<6) | ((index&0x7)<<3) | (base&0x7));
}

void InstructionEncoder::writeLE(std::vector<uint8_t>& out, uint64_t value, size_t bytes) {
    for (size_t i=0;i<bytes;i++) out.push_back((uint8_t)((value >> (i*8)) & 0xFF));
}

// Simplifying assumptions: immediate fits into 32-bit for rel32, mem addressing limited.
// encode MOV with forms: MOV reg, reg | MOV reg, imm | MOV reg, [mem] | MOV [mem], reg
void InstructionEncoder::encodeMOV(const ParsedInstruction& instr, Encoded& out, const std::unordered_map<std::string,uint64_t>& labels, uint64_t addr) {
    // basic checks
    if (instr.operands.size() != 2) throw std::runtime_error("MOV requires 2 operands");

    const ParsedOperand &dst = instr.operands[0];
    const ParsedOperand &src = instr.operands[1];

    // MOV reg, imm -> opcode: 0xB8 + reg (with REX.W if r>=8)
    if (dst.kind==ParsedOperand::REG && src.kind==ParsedOperand::IMM) {
        auto it = reg64.find(dst.text);
        if (it==reg64.end()) throw std::runtime_error("Unknown register: " + dst.text);
        uint8_t reg = it->second;
        bool rexW = true; // 64-bit move immediate -> MOV r64, imm64 uses opcode B8+rd and imm64, requires REX.W on some encodings; simplest: emit REX.W and B8+low3
        bool rexR = (reg & 0x08);
        bool rexB = false;
        if (rexW || rexR || rexB) out.bytes.push_back(rex(rexW, rexR, false, rexB));
        uint8_t opcode = 0xB8 + (reg & 0x7);
        out.bytes.push_back(opcode);
        // immediate: in x86-64 MOV r64, imm64 is 8 byte immediate
        uint64_t imm = 0;
        if (src.text.size()>1 && src.text[0]=='0' && (src.text[1]=='x' || src.text[1]=='X')) imm = std::stoull(src.text, nullptr, 16);
        else imm = std::stoull(src.text, nullptr, 10);
        writeLE(out.bytes, imm, 8);
        return;
    }

    // MOV reg, reg -> opcode 0x89 /r (mov r/m64,r64) or 0x8B /r (mov r64,r/m64) depending
    if (dst.kind==ParsedOperand::REG && src.kind==ParsedOperand::REG) {
        auto itd = reg64.find(dst.text); auto its = reg64.find(src.text);
        if (itd==reg64.end()||its==reg64.end()) throw std::runtime_error("Unknown register");
        uint8_t rd = itd->second; uint8_t rs = its->second;
        // encoding: 0x48/REX W + 0x89 /r where /r indicates reg=rs, r/m=rd for MOV r/m64, r64
        bool rexW = true;
        bool rexR = (rs & 0x08);
        bool rexB = (rd & 0x08);
        out.bytes.push_back(rex(rexW, rexR, false, rexB));
        out.bytes.push_back(0x89);
        out.bytes.push_back(modrm(3, rs & 0x7, rd & 0x7));
        return;
    }

    // MOV reg, [mem] -> 0x8B /r (mov r64, r/m64)
    if (dst.kind==ParsedOperand::REG && src.kind==ParsedOperand::MEM) {
        auto itd = reg64.find(dst.text);
        if (itd==reg64.end()) throw std::runtime_error("Unknown register");
        uint8_t rd = itd->second;
        // Determine base/index
        uint8_t base=4, index=4; bool hasBase=false, hasIndex=false;
        if (src.base) {
            auto itb = reg64.find(*src.base);
            if (itb==reg64.end()) throw std::runtime_error("Unknown base register: " + *src.base);
            base = itb->second; hasBase=true;
        }
        if (src.index) {
            auto iti = reg64.find(*src.index);
            if (iti==reg64.end()) throw std::runtime_error("Unknown index register: " + *src.index);
            index = iti->second; hasIndex=true;
        }
        bool rexW = true;
        bool rexR = (rd & 0x08);
        bool rexX = hasIndex ? ((index & 0x08)!=0) : false;
        bool rexB = hasBase ? ((base & 0x08)!=0) : false;
        out.bytes.push_back(rex(rexW, rexR, rexX, rexB));
        out.bytes.push_back(0x8B); // MOV r64, r/m64
        // Basic ModR/M handling for common cases: [base + disp], [disp32], [base + index*scale + disp]
        if (hasBase) {
            // no SIB and no disp (disp==0) and base != RSP
            if (!hasIndex && src.disp==0 && (base & 0x7) != 4) {
                out.bytes.push_back(modrm(0, rd & 0x7, base & 0x7));
            } else {
                // need SIB or displacement. Use mod=0/1/2 depending on disp
                if ((base & 0x7) == 4) { // base is RSP -> need SIB always
                    // simple: use mod=0 with SIB base=RSP => special case requires disp32 or disp8 depending
                    if (src.disp==0) {
                        out.bytes.push_back(modrm(0, rd & 0x7, 4));
                        out.bytes.push_back(sib((uint8_t) (std::log2(src.scale)?1:0), index & 0x7, base & 0x7)); // scale guess
                        // but using log2 is unsafe; for simplicity assume scale 1 => scale bits 0
                    } else if (src.disp >= -128 && src.disp <= 127) {
                        out.bytes.push_back(modrm(1, rd & 0x7, 4));
                        out.bytes.push_back(sib(0, index & 0x7, base & 0x7));
                        writeLE(out.bytes, (uint64_t)(int64_t)src.disp, 1);
                    } else {
                        out.bytes.push_back(modrm(2, rd & 0x7, 4));
                        out.bytes.push_back(sib(0, index & 0x7, base & 0x7));
                        writeLE(out.bytes, (uint64_t)(int64_t)src.disp, 4);
                    }
                } else {
                    // base != RSP
                    if (src.disp==0 && !hasIndex) {
                        out.bytes.push_back(modrm(0, rd & 0x7, base & 0x7));
                    } else if (src.disp >= -128 && src.disp <= 127) {
                        out.bytes.push_back(modrm(1, rd & 0x7, base & 0x7));
                        if (hasIndex) out.bytes.push_back(sib(0, index & 0x7, base & 0x7));
                        writeLE(out.bytes, (uint64_t)(int64_t)src.disp, 1);
                    } else {
                        out.bytes.push_back(modrm(2, rd & 0x7, base & 0x7));
                        if (hasIndex) out.bytes.push_back(sib(0, index & 0x7, base & 0x7));
                        writeLE(out.bytes, (uint64_t)(int64_t)src.disp, 4);
                    }
                }
            }
            return;
        } else {
            // no base -> absolute addressing not supported here
            throw std::runtime_error("Memory operand without base not supported in this minimal encoder");
        }
    }

    // MOV [mem], reg -> opcode 0x89 /r
    if (dst.kind==ParsedOperand::MEM && src.kind==ParsedOperand::REG) {
        auto its = reg64.find(src.text);
        if (its==reg64.end()) throw std::runtime_error("Unknown register");
        uint8_t rs = its->second;
        // handle base/index similar to above
        uint8_t base=4, index=4; bool hasBase=false, hasIndex=false;
        if (dst.base) { auto itb = reg64.find(*dst.base); if (itb==reg64.end()) throw std::runtime_error("Unknown base"); base = itb->second; hasBase=true;}
        if (dst.index) { auto iti = reg64.find(*dst.index); if (iti==reg64.end()) throw std::runtime_error("Unknown index"); index = iti->second; hasIndex=true;}
        bool rexW = true;
        bool rexR = (rs & 0x08);
        bool rexX = hasIndex ? ((index & 0x08)!=0) : false;
        bool rexB = hasBase ? ((base & 0x08)!=0) : false;
        out.bytes.push_back(rex(rexW, rexR, rexX, rexB));
        out.bytes.push_back(0x89); // MOV r/m64, r64
        if (hasBase) {
            if (!hasIndex && dst.disp==0 && (base & 0x7) != 4) {
                out.bytes.push_back(modrm(0, rs & 0x7, base & 0x7));
            } else {
                if ((base & 0x7) == 4) {
                    if (dst.disp==0) {
                        out.bytes.push_back(modrm(0, rs & 0x7, 4));
                        out.bytes.push_back(sib(0, index & 0x7, base & 0x7));
                    } else if (dst.disp >= -128 && dst.disp <=127) {
                        out.bytes.push_back(modrm(1, rs & 0x7, 4));
                        out.bytes.push_back(sib(0, index & 0x7, base & 0x7));
                        writeLE(out.bytes, (uint64_t)(int64_t)dst.disp, 1);
                    } else {
                        out.bytes.push_back(modrm(2, rs & 0x7, 4));
                        out.bytes.push_back(sib(0, index & 0x7, base & 0x7));
                        writeLE(out.bytes, (uint64_t)(int64_t)dst.disp, 4);
                    }
                } else {
                    if (dst.disp==0 && !hasIndex) {
                        out.bytes.push_back(modrm(0, rs & 0x7, base & 0x7));
                    } else if (dst.disp >= -128 && dst.disp <=127) {
                        out.bytes.push_back(modrm(1, rs & 0x7, base & 0x7));
                        if (hasIndex) out.bytes.push_back(sib(0, index & 0x7, base & 0x7));
                        writeLE(out.bytes, (uint64_t)(int64_t)dst.disp, 1);
                    } else {
                        out.bytes.push_back(modrm(2, rs & 0x7, base & 0x7));
                        if (hasIndex) out.bytes.push_back(sib(0, index & 0x7, base & 0x7));
                        writeLE(out.bytes, (uint64_t)(int64_t)dst.disp, 4);
                    }
                }
            }
            return;
        } else {
            throw std::runtime_error("Memory operand without base not supported");
        }
    }

    throw std::runtime_error("MOV form not supported in this minimal encoder");
}

// ADD reg, reg or reg, imm (simplified)
void InstructionEncoder::encodeADD(const ParsedInstruction& instr, Encoded& out) {
    if (instr.operands.size() != 2) throw std::runtime_error("ADD requires 2 operands");
    auto &a = instr.operands[0];
    auto &b = instr.operands[1];
    if (a.kind==ParsedOperand::REG && b.kind==ParsedOperand::REG) {
        auto ita = reg64.find(a.text); auto itb = reg64.find(b.text);
        if (ita==reg64.end()||itb==reg64.end()) throw std::runtime_error("Unknown reg");
        uint8_t ra = ita->second, rb = itb->second;
        // encoding: REX.W + 0x01 /r => add r/m64, r64  (so modrm reg=rb, rm=ra)
        out.bytes.push_back(rex(true, (rb&8)!=0, false, (ra&8)!=0));
        out.bytes.push_back(0x01);
        out.bytes.push_back(modrm(3, rb&7, ra&7));
        return;
    }
    if (a.kind==ParsedOperand::REG && b.kind==ParsedOperand::IMM) {
        auto ita = reg64.find(a.text); if (ita==reg64.end()) throw std::runtime_error("Unknown reg");
        uint8_t ra = ita->second;
        // opcode: REX.W + 0x81 /0 for add r/m64, imm32 (we use imm32), reg field=0
        out.bytes.push_back(rex(true, false, false, (ra&8)!=0));
        out.bytes.push_back(0x81);
        out.bytes.push_back(modrm(3, 0, ra&7));
        uint64_t imm = 0;
        if (b.text.size()>1 && b.text[0]=='0' && (b.text[1]=='x' || b.text[1]=='X')) imm = std::stoull(b.text, nullptr, 16);
        else imm = std::stoull(b.text, nullptr, 10);
        writeLE(out.bytes, imm, 4); // imm32
        return;
    }
    throw std::runtime_error("ADD form not supported");
}

void InstructionEncoder::encodeSUB(const ParsedInstruction& instr, Encoded& out) {
    if (instr.operands.size() != 2) throw std::runtime_error("SUB requires 2 operands");
    auto &a = instr.operands[0];
    auto &b = instr.operands[1];
    if (a.kind==ParsedOperand::REG && b.kind==ParsedOperand::REG) {
        auto ita = reg64.find(a.text); auto itb = reg64.find(b.text);
        if (ita==reg64.end()||itb==reg64.end()) throw std::runtime_error("Unknown reg");
        uint8_t ra = ita->second, rb = itb->second;
        out.bytes.push_back(rex(true, (rb&8)!=0, false, (ra&8)!=0));
        out.bytes.push_back(0x29); // sub r/m64, r64 (note reversed from add)
        out.bytes.push_back(modrm(3, rb&7, ra&7));
        return;
    }
    if (a.kind==ParsedOperand::REG && b.kind==ParsedOperand::IMM) {
        auto ita = reg64.find(a.text); if (ita==reg64.end()) throw std::runtime_error("Unknown reg");
        uint8_t ra = ita->second;
        out.bytes.push_back(rex(true, false, false, (ra&8)!=0));
        out.bytes.push_back(0x81);
        out.bytes.push_back(modrm(3, 5, ra&7)); // /5 is SUB
        uint64_t imm = 0;
        if (b.text.size()>1 && b.text[0]=='0' && (b.text[1]=='x' || b.text[1]=='X')) imm = std::stoull(b.text, nullptr, 16);
        else imm = std::stoull(b.text, nullptr, 10);
        writeLE(out.bytes, imm, 4);
        return;
    }
    throw std::runtime_error("SUB form not supported");
}

// JMP label -> E9 rel32
void InstructionEncoder::encodeJMP(const ParsedInstruction& instr, Encoded& out, const std::unordered_map<std::string,uint64_t>& labels, uint64_t addr) {
    if (instr.operands.size()!=1) throw std::runtime_error("JMP requires one operand");
    if (instr.operands[0].kind != ParsedOperand::LABEL) throw std::runtime_error("JMP requires label");
    auto it = labels.find(instr.operands[0].text);
    if (it==labels.end()) throw std::runtime_error("Unknown label in JMP: " + instr.operands[0].text);
    uint64_t target = it->second;
    // rel32 = target - (addr + 5)
    int64_t rel = (int64_t)target - (int64_t)(addr + 5);
    out.bytes.push_back(0xE9);
    writeLE(out.bytes, (uint64_t)(int64_t)rel, 4);
}

// CMP reg, imm or reg, reg (we implement reg, imm)
void InstructionEncoder::encodeCMP(const ParsedInstruction& instr, Encoded& out) {
    if (instr.operands.size()!=2) throw std::runtime_error("CMP requires 2 operands");
    auto &a = instr.operands[0]; auto &b = instr.operands[1];
    if (a.kind==ParsedOperand::REG && b.kind==ParsedOperand::IMM) {
        auto ita = reg64.find(a.text); if (ita==reg64.end()) throw std::runtime_error("Unknown reg");
        uint8_t ra = ita->second;
        // opcode: REX.W + 0x81 /7
        out.bytes.push_back(rex(true, false, false, (ra&8)!=0));
        out.bytes.push_back(0x81);
        out.bytes.push_back(modrm(3, 7, ra&7));
        uint64_t imm = 0;
        if (b.text.size()>1 && b.text[0]=='0' && (b.text[1]=='x' || b.text[1]=='X')) imm = std::stoull(b.text, nullptr, 16);
        else imm = std::stoull(b.text, nullptr, 10);
        writeLE(out.bytes, imm, 4);
        return;
    }
    throw std::runtime_error("CMP form not supported");
}

// JE rel32 -> 0x0F 0x84 rel32
void InstructionEncoder::encodeJE(const ParsedInstruction& instr, Encoded& out, const std::unordered_map<std::string,uint64_t>& labels, uint64_t addr) {
    if (instr.operands.size()!=1) throw std::runtime_error("JE requires one operand");
    if (instr.operands[0].kind!=ParsedOperand::LABEL) throw std::runtime_error("JE needs label");
    auto it = labels.find(instr.operands[0].text);
    if (it==labels.end()) throw std::runtime_error("Unknown label in JE");
    uint64_t target = it->second;
    int64_t rel = (int64_t)target - (int64_t)(addr + 6); // 6 bytes: 0F 84 + rel32
    out.bytes.push_back(0x0F);
    out.bytes.push_back(0x84);
    writeLE(out.bytes, (uint64_t)(int64_t)rel, 4);
}

// CALL label -> E8 rel32
void InstructionEncoder::encodeCALL(const ParsedInstruction& instr, Encoded& out, const std::unordered_map<std::string,uint64_t>& labels, uint64_t addr) {
    if (instr.operands.size()!=1) throw std::runtime_error("CALL requires one operand");
    if (instr.operands[0].kind!=ParsedOperand::LABEL) throw std::runtime_error("CALL needs label");
    auto it = labels.find(instr.operands[0].text);
    if (it==labels.end()) throw std::runtime_error("Unknown label in CALL");
    uint64_t target = it->second;
    int64_t rel = (int64_t)target - (int64_t)(addr + 5);
    out.bytes.push_back(0xE8);
    writeLE(out.bytes, (uint64_t)(int64_t)rel, 4);
}

void InstructionEncoder::encodeRET(const ParsedInstruction& instr, Encoded& out) {
    out.bytes.push_back(0xC3);
}

Encoded InstructionEncoder::encodeInstruction(const ParsedInstruction& instr, const std::unordered_map<std::string,uint64_t>& labels, uint64_t currentAddress) {
    Encoded e;
    
    // If mnemonic is empty (e.g., label-only line), return empty encoding
    if (instr.mnemonic.empty()) {
        return e;
    }
    
    std::string m = instr.mnemonic;
    if (m=="MOV") encodeMOV(instr, e, labels, currentAddress);
    else if (m=="ADD") encodeADD(instr, e);
    else if (m=="SUB") encodeSUB(instr, e);
    else if (m=="JMP") encodeJMP(instr, e, labels, currentAddress);
    else if (m=="CMP") encodeCMP(instr, e);
    else if (m=="JE") encodeJE(instr, e, labels, currentAddress);
    else if (m=="CALL") encodeCALL(instr, e, labels, currentAddress);
    else if (m=="RET") encodeRET(instr, e);
    else throw std::runtime_error("Unsupported mnemonic: " + m);
    return e;
}
