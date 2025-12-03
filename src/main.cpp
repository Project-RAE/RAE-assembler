#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <limits>
#include <iomanip>
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

    // Debug: print labels resolved in pass1
    std::cerr << "Pass1: assigned " << labels.size() << " labels:\n";
    for (auto &kv : labels) std::cerr << "  " << kv.first << " -> " << kv.second << "\n";

    // pass 2: actual encoding with resolved labels (instrumented)
    std::vector<uint8_t> outBytes;
    addr = 0;
    size_t accum = 0; // diagnostic accumulator
    const size_t HARD_LIMIT = 100ull * 1024 * 1024; // 100 MiB for diagnostic abort
    for (size_t idx = 0; idx < parsed.size(); ++idx) {
        auto &pi = parsed[idx];
        if (pi.label) labels[*pi.label] = addr;
        if (!pi.mnemonic.empty()) {
            try {
                Encoded e = encoder.encodeInstruction(pi, labels, addr);
                std::cerr << "instr[" << idx << "] line=" << pi.sourceLine
                          << " mnemonic=" << pi.mnemonic
                          << " bytes=" << e.bytes.size()
                          << " addr=" << addr << "\n";

                if (e.bytes.size() > 1024*1024) {
                    std::cerr << "ERROR: single instruction produced >1MiB bytes at line "
                              << pi.sourceLine << " mnemonic=" << pi.mnemonic << "\n";
                    return 1;
                }

                outBytes.insert(outBytes.end(), e.bytes.begin(), e.bytes.end());
                accum += e.bytes.size();            // update diagnostic accumulator
                addr += e.bytes.size();

                if (outBytes.size() != accum) {
                    std::cerr << "DIAG: mismatch after instr[" << idx << "]: outBytes.size()="
                              << outBytes.size() << " accum=" << accum << "\n";
                    // dump a few bytes to help identify corruption
                    size_t dumpN = std::min<size_t>(32, outBytes.size());
                    std::cerr << "first " << dumpN << " bytes:";
                    for (size_t i=0;i<dumpN;i++) std::cerr << " " << std::hex << (int)outBytes[i];
                    std::cerr << std::dec << "\n";
                    return 1;
                }

                if (outBytes.size() > HARD_LIMIT) {
                    std::cerr << "ABORT: accumulated output exceeded " << HARD_LIMIT
                              << " bytes after instr[" << idx << "] (total=" << outBytes.size() << ")\n";
                    return 1;
                }
            } catch (const std::exception& ex) {
                std::cerr << "Encoding error at instr[" << idx << "] line " << pi.sourceLine
                          << ": " << ex.what() << "\n";
                return 1;
            }
        }
    }

    std::cerr << "Final accum=" << accum << " outBytes.size()=" << outBytes.size() << "\n";

    // write out using the diagnostic accumulator (accum) as authoritative size
    std::ofstream of(outfile, std::ios::binary);
    if (!of) { std::cerr << "Failed to open output file\n"; return 1; }

    if (accum != outBytes.size()) {
        std::cerr << "WARNING: accum != outBytes.size(), using accum (" << accum << ") for write\n";
    }

    if (accum > static_cast<size_t>(std::numeric_limits<std::streamsize>::max())) {
        std::cerr << "Output too large to write safely\n";
        return 1;
    }

    std::streamsize toWrite = static_cast<std::streamsize>(accum);
    of.write(reinterpret_cast<const char*>(outBytes.data()), toWrite);
    of.close();

    std::cout << "Wrote " << static_cast<unsigned long long>(accum) << " bytes to " << outfile << "\n";
    return 0;
}
