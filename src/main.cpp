#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include "lexer.hpp"
#include "parser.hpp"
#include "encoder.hpp"

int main(int argc, char** argv) {
    std::string infile = "../test.rae";
    std::string outfile = "out.bin";
    if (argc >= 2) infile = argv[1];
    if (argc >= 3) outfile = argv[2];

    std::ifstream in(infile);
    if (!in) { std::cerr << "Failed to open " << infile << "\n"; return 1; }
    std::stringstream ss; ss << in.rdbuf();
    std::string src = ss.str();

    Lexer lex(src);
    Parser parser(lex);
    auto parsed = parser.parseAll();

    // Two-pass: compute addresses
    InstructionEncoder encoder;
    std::unordered_map<std::string,uint64_t> labels;
    uint64_t addr = 0;
    
    // pass 1: assign label addresses by estimating instruction sizes conservatively
    for (auto &pi : parsed) {
        if (pi.label) {
            labels[*pi.label] = addr;
        }
        if (!pi.mnemonic.empty()) {
            try {
                Encoded e = encoder.encodeInstruction(pi, labels, addr);
            } catch (...) {
                std::string m = pi.mnemonic;
                if (m=="JMP" || m=="CALL") addr += 5;
                else if (m=="JE") addr += 6;
                else if (m=="RET") addr += 1;
                else addr += 1;
                continue;
            }
            addr += 1; // conservative minimal estimate
        }
    }

    // pass 2: actual encoding with resolved labels
    std::vector<uint8_t> outBytes;
    addr = 0;
    for (auto &pi : parsed) {
        if (pi.label) {
            labels[*pi.label] = addr;
        }
        if (!pi.mnemonic.empty()) {
            Encoded e = encoder.encodeInstruction(pi, labels, addr);
            outBytes.insert(outBytes.end(), e.bytes.begin(), e.bytes.end());
            addr += e.bytes.size();
        }
    }

    // write out
    std::ofstream of(outfile, std::ios::binary);
    if (!of) { std::cerr << "Failed to open output file\n"; return 1; }
    of.write(reinterpret_cast<const char*>(outBytes.data()), (std::streamsize)outBytes.size());
    of.close();
    
    std::cout << "Wrote " << outBytes.size() << " bytes to " << outfile << "\n";
    return 0;
}
