#include "api/CompilerApi.h"
#include "api/HttpServer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>

using namespace edge;

void printUsage() {
    std::cout << "Resilience-Aware Compiler for Resource-Constrained Edge Systems (edgec)\n"
              << "Usage: edgec [options] [source_file.edge]\n\n"
              << "Options:\n"
              << "  --compile <file>            Compile and display analysis pipeline summary\n"
              << "  --json <file>               Output complete compiler pipeline artifacts as JSON\n"
              << "  --ast <file>                Print Abstract Syntax Tree\n"
              << "  --cfg <file>                Print Control-Flow Graph and basic blocks\n"
              << "  --liveness <file>           Display backward liveness data-flow analysis\n"
              << "  --classify <file>           Display 4-way variable classification decisions\n"
              << "  --plan <file> [--budget N]  Generate minimal checkpoint recovery plan\n"
              << "  --simulate <file>           Run failure injection and recovery simulation\n"
              << "      --strategy <STRAT>      CHECKPOINT_EVERYTHING | LIVENESS_ONLY | MANUAL_ANNOTATION | RESILIENCE_AWARE\n"
              << "      --budget <bytes>        Configure NVM memory budget constraint (bytes)\n"
              << "      --fault-cycle <N>       Inject crash after N virtual cycles\n"
              << "      --fault-stmt <ID>       Inject crash at statement ID\n"
              << "  --benchmark [<file>]        Run 4-strategy benchmark comparison suite\n"
              << "  --serve [--port <port>]     Launch local embedded REST API server (default: 8080)\n"
              << "  --examples                  List all built-in benchmark programs\n"
              << "  --help                      Show this help message\n";
}

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file '" << path << "'\n";
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string command = argv[1];
    if (command.rfind("--", 0) != 0 && command.size() >= 3 && 
        static_cast<unsigned char>(command[0]) == 0xe2 && 
        static_cast<unsigned char>(command[1]) == 0x80) {
        command = "--" + command.substr(3);
    }

    if (command == "--help" || command == "-h") {
        printUsage();
        return 0;
    }

    if (command == "--examples") {
        auto examples = CompilerApi::getExamples();
        std::cout << "=== Built-in EdgeLang Benchmark Programs ===\n\n";
        for (const auto& ex : examples) {
            std::cout << "[" << ex.id << "] " << ex.title << " (" << ex.category << ")\n"
                      << "  " << ex.description << "\n\n";
        }
        return 0;
    }

    if (command == "--serve") {
        int port = 8080;
        bool isDaemon = false;
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--port" && i + 1 < argc) {
                port = std::stoi(argv[++i]);
            } else if (arg == "--daemon" || arg == "-d") {
                isDaemon = true;
            }
        }
        HttpServer server(port);
        if (!server.start()) {
            std::cerr << "Failed to start HTTP server on port " << port << std::endl;
            return 1;
        }
        std::cout << "Resilience-Aware Compiler API server running at http://localhost:" << port << "\n";
        if (isDaemon) {
            while (server.isRunning()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        } else {
            std::cout << "Press Enter to stop server..." << std::endl;
            std::string line;
            if (std::getline(std::cin, line)) {
                // Interactive user pressed Enter
            } else {
                // Detached or non-interactive stdin: keep serving until isRunning is false
                while (server.isRunning()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
            }
        }
        server.stop();
        return 0;
    }

    if (command == "--benchmark") {
        std::string source;
        if (argc > 2 && argv[2][0] != '-') {
            source = readFile(argv[2]);
        }
        std::cout << CompilerApi::runBenchmarks(source) << std::endl;
        return 0;
    }

    // File-based commands
    std::string filePath;
    size_t budget = 0;
    std::string strategyStr = "RESILIENCE_AWARE";
    int32_t faultCycle = -1;
    size_t faultStmt = 0;

    int fileArgIdx = 2;
    if (command[0] != '-') {
        filePath = command;
        command = "--compile";
        fileArgIdx = 1;
    } else if (argc > 2) {
        filePath = argv[2];
        fileArgIdx = 3;
    }

    for (int i = fileArgIdx; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--budget" && i + 1 < argc) {
            budget = std::stoul(argv[++i]);
        } else if (arg == "--strategy" && i + 1 < argc) {
            strategyStr = argv[++i];
        } else if (arg == "--fault-cycle" && i + 1 < argc) {
            faultCycle = std::stoi(argv[++i]);
        } else if (arg == "--fault-stmt" && i + 1 < argc) {
            faultStmt = std::stoul(argv[++i]);
        }
    }

    if (filePath.empty()) {
        std::cerr << "Error: No input file specified.\n";
        return 1;
    }

    std::string source = readFile(filePath);
    if (source.empty()) return 1;

    if (command == "--json") {
        auto res = CompilerApi::compile(source, budget, filePath);
        std::cout << res.toJson() << std::endl;
        return res.success ? 0 : 1;
    }

    if (command == "--simulate") {
        auto strat = stringToCheckpointStrategy(strategyStr);
        std::string simJson = CompilerApi::simulate(source, strat, budget, faultCycle, faultStmt);
        std::cout << simJson << std::endl;
        return 0;
    }

    // Default compile / inspection commands
    auto res = CompilerApi::compile(source, budget, filePath);
    if (!res.success) {
        std::cerr << "Compilation failed:\n" << res.diagnosticsJson << std::endl;
        return 1;
    }

    if (command == "--ast") {
        std::cout << res.astJson << std::endl;
    } else if (command == "--cfg") {
        std::cout << res.cfgJson << std::endl;
    } else if (command == "--liveness") {
        std::cout << res.livenessJson << std::endl;
    } else if (command == "--classify") {
        std::cout << res.classificationsJson << std::endl;
    } else if (command == "--plan") {
        std::cout << res.recoveryPlanJson << std::endl;
    } else {
        // Standard compile overview
        std::cout << "=== Compilation Successful: " << res.programName << " ===\n"
                  << "Compilation Time: " << res.compilationTimeMs << " ms\n\n"
                  << "Disassembly:\n" << res.disassembled << "\n\n"
                  << "Recovery Plan:\n" << res.recoveryPlanJson << "\n";
    }

    return 0;
}
