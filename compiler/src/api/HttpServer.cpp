#include "api/HttpServer.h"
#include "api/CompilerApi.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
using socket_t = SOCKET;
#define CLOSE_SOCKET(s) closesocket(s)
#define IS_VALID_SOCKET(s) ((s) != INVALID_SOCKET)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
using socket_t = int;
#define CLOSE_SOCKET(s) close(s)
#define IS_VALID_SOCKET(s) ((s) >= 0)
#endif

namespace edge {

static std::string extractJsonField(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return "";

    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";
    pos++; // move past ':'

    // Skip whitespace
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n')) {
        pos++;
    }

    if (pos >= json.size()) return "";

    if (json[pos] == '"') {
        // String value
        pos++;
        std::string result;
        while (pos < json.size()) {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                char esc = json[pos + 1];
                if (esc == '"') { result += '"'; pos += 2; }
                else if (esc == '\\') { result += '\\'; pos += 2; }
                else if (esc == 'n') { result += '\n'; pos += 2; }
                else if (esc == 'r') { result += '\r'; pos += 2; }
                else if (esc == 't') { result += '\t'; pos += 2; }
                else if (esc == 'u' && pos + 5 < json.size()) {
                    std::string hex = json.substr(pos + 2, 4);
                    try {
                        int codePoint = std::stoi(hex, nullptr, 16);
                        if (codePoint < 128) {
                            result += static_cast<char>(codePoint);
                        } else {
                            if (codePoint <= 0x7FF) {
                                result += static_cast<char>(0xC0 | ((codePoint >> 6) & 0x1F));
                                result += static_cast<char>(0x80 | (codePoint & 0x3F));
                            } else {
                                result += static_cast<char>(0xE0 | ((codePoint >> 12) & 0x0F));
                                result += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
                                result += static_cast<char>(0x80 | (codePoint & 0x3F));
                            }
                        }
                        pos += 6;
                    } catch (...) {
                        result += esc;
                        pos += 2;
                    }
                } else {
                    result += esc;
                    pos += 2;
                }
            } else if (json[pos] == '"') {
                break;
            } else {
                result += json[pos++];
            }
        }
        return result;
    } else {
        // Number or boolean
        size_t endPos = json.find_first_of(",}\r\n", pos);
        if (endPos == std::string::npos) endPos = json.size();
        std::string val = json.substr(pos, endPos - pos);
        // Trim
        while (!val.empty() && (val.back() == ' ' || val.back() == '\t')) val.pop_back();
        return val;
    }
}

HttpServer::HttpServer(int port) : port_(port), isRunning_(false) {}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::start() {
    if (isRunning_) return true;

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return false;
    }
#endif

    isRunning_ = true;
    serverThread_ = std::thread(&HttpServer::runServer, this);
    return true;
}

void HttpServer::stop() {
    if (!isRunning_) return;
    isRunning_ = false;

    // Connect to unblock accept
    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (IS_VALID_SOCKET(sock)) {
        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port_);
        inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
        connect(sock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));
        CLOSE_SOCKET(sock);
    }

    if (serverThread_.joinable()) {
        serverThread_.join();
    }

#ifdef _WIN32
    WSACleanup();
#endif
}

std::string HttpServer::handleRequest(const std::string& method, const std::string& path, const std::string& body) {
    if (method == "OPTIONS") {
        return "HTTP/1.1 204 No Content\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
               "Access-Control-Allow-Headers: Content-Type, Accept\r\n"
               "Content-Length: 0\r\n\r\n";
    }

    std::string responseBody;
    std::string contentType = "application/json";
    int statusCode = 200;
    std::string statusText = "OK";

    // Normalize path: strip query parameters, trailing colons, slashes, and whitespace
    std::string cleanPath = path;
    size_t qPos = cleanPath.find('?');
    if (qPos != std::string::npos) {
        cleanPath = cleanPath.substr(0, qPos);
    }
    while (!cleanPath.empty() && (cleanPath.back() == '/' || cleanPath.back() == ':' || cleanPath.back() == ' ' || cleanPath.back() == '\t')) {
        cleanPath.pop_back();
    }

    if (cleanPath == "/api/health") {
        responseBody = "{\"status\":\"ok\",\"service\":\"ResilienceAwareCompiler\",\"version\":\"1.0.0\"}";
    } else if (cleanPath == "/api/examples") {
        auto examples = CompilerApi::getExamples();
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < examples.size(); ++i) {
            if (i > 0) oss << ",";
            oss << examples[i].toJson();
        }
        oss << "]";
        responseBody = oss.str();
    } else if (cleanPath == "/api/compile") {
        std::string source;
        size_t budget = 0;
        if (method == "POST" && !body.empty()) {
            source = extractJsonField(body, "sourceCode");
            std::string budgetStr = extractJsonField(body, "memoryBudget");
            budget = budgetStr.empty() ? 0 : std::stoul(budgetStr);
        }
        if (source.empty()) {
            auto examples = CompilerApi::getExamples();
            source = examples.empty() ? "" : examples[0].sourceCode;
        }

        CompilationResult res = CompilerApi::compile(source, budget);
        responseBody = res.toJson();
    } else if (cleanPath == "/api/simulate") {
        std::string source;
        CheckpointStrategy strat = CheckpointStrategy::RESILIENCE_AWARE;
        size_t budget = 0;
        int32_t faultCycle = -1;
        size_t faultStmt = 0;

        if (method == "POST" && !body.empty()) {
            source = extractJsonField(body, "sourceCode");
            std::string stratStr = extractJsonField(body, "strategy");
            std::string budgetStr = extractJsonField(body, "memoryBudget");
            std::string faultCycleStr = extractJsonField(body, "faultCycle");
            std::string faultStmtStr = extractJsonField(body, "faultStatementId");

            if (!stratStr.empty()) strat = stringToCheckpointStrategy(stratStr);
            if (!budgetStr.empty()) budget = std::stoul(budgetStr);
            if (!faultCycleStr.empty()) faultCycle = std::stoi(faultCycleStr);
            if (!faultStmtStr.empty()) faultStmt = std::stoul(faultStmtStr);
        }

        if (source.empty()) {
            auto examples = CompilerApi::getExamples();
            source = examples.empty() ? "" : examples[0].sourceCode;
        }

        responseBody = CompilerApi::simulate(source, strat, budget, faultCycle, faultStmt);
    } else if (cleanPath == "/api/benchmark") {
        std::string source;
        if (method == "POST" && !body.empty()) {
            source = extractJsonField(body, "sourceCode");
        }
        responseBody = CompilerApi::runBenchmarks(source);
    } else {
        statusCode = 404;
        statusText = "Not Found";
        responseBody = "{\"error\":\"Endpoint not found\",\"path\":\"" + path + "\",\"supportedEndpoints\":[\"/api/health\",\"/api/examples\",\"/api/compile\",\"/api/simulate\",\"/api/benchmark\"]}";
    }

    std::ostringstream resp;
    resp << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n"
         << "Content-Type: " << contentType << "\r\n"
         << "Content-Length: " << responseBody.size() << "\r\n"
         << "Access-Control-Allow-Origin: *\r\n"
         << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
         << "Access-Control-Allow-Headers: Content-Type, Accept\r\n"
         << "Connection: close\r\n\r\n"
         << responseBody;

    return resp.str();
}

void HttpServer::runServer() {
    socket_t serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (!IS_VALID_SOCKET(serverSocket)) {
        std::cerr << "Failed to create server socket." << std::endl;
        isRunning_ = false;
        return;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port_);

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) != 0) {
        std::cerr << "Failed to bind server socket to port " << port_ << std::endl;
        CLOSE_SOCKET(serverSocket);
        isRunning_ = false;
        return;
    }

    if (listen(serverSocket, 10) != 0) {
        std::cerr << "Failed to listen on server socket." << std::endl;
        CLOSE_SOCKET(serverSocket);
        isRunning_ = false;
        return;
    }

    std::cout << "[API Server] Listening on http://localhost:" << port_ << std::endl;

    while (isRunning_) {
        sockaddr_in clientAddr{};
#ifdef _WIN32
        int clientLen = sizeof(clientAddr);
#else
        socklen_t clientLen = sizeof(clientAddr);
#endif
        socket_t clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);

        if (!isRunning_) {
            if (IS_VALID_SOCKET(clientSocket)) CLOSE_SOCKET(clientSocket);
            break;
        }

        if (!IS_VALID_SOCKET(clientSocket)) continue;

        std::string req;
        std::vector<char> buffer(4096, 0);
        int contentLength = -1;
        size_t headerEnd = std::string::npos;

        while (true) {
            int bytesRead = recv(clientSocket, buffer.data(), static_cast<int>(buffer.size()), 0);
            if (bytesRead <= 0) break;
            req.append(buffer.data(), bytesRead);

            if (headerEnd == std::string::npos) {
                headerEnd = req.find("\r\n\r\n");
                if (headerEnd != std::string::npos) {
                    std::string headers = req.substr(0, headerEnd);
                    std::string clHeader = "Content-Length:";
                    size_t clPos = headers.find(clHeader);
                    if (clPos == std::string::npos) {
                        clHeader = "content-length:";
                        clPos = headers.find(clHeader);
                    }
                    if (clPos != std::string::npos) {
                        size_t valStart = clPos + clHeader.size();
                        size_t lineEnd = headers.find("\r\n", valStart);
                        std::string clVal = headers.substr(valStart, (lineEnd != std::string::npos ? lineEnd - valStart : std::string::npos));
                        try {
                            contentLength = std::stoi(clVal);
                        } catch (...) {
                            contentLength = 0;
                        }
                    } else {
                        contentLength = 0;
                    }
                }
            }

            if (headerEnd != std::string::npos) {
                size_t bodyLen = req.size() - (headerEnd + 4);
                if (contentLength <= 0 || bodyLen >= static_cast<size_t>(contentLength)) {
                    break;
                }
            }
        }

        if (!req.empty()) {
            std::istringstream stream(req);
            std::string method, path, version;
            stream >> method >> path >> version;

            std::string body;
            if (headerEnd != std::string::npos && (headerEnd + 4) <= req.size()) {
                body = req.substr(headerEnd + 4);
                if (contentLength > 0 && body.size() > static_cast<size_t>(contentLength)) {
                    body = body.substr(0, contentLength);
                }
            }

            std::string resp = handleRequest(method, path, body);

            int totalSent = 0;
            int toSend = static_cast<int>(resp.size());
            const char* ptr = resp.c_str();
            while (totalSent < toSend) {
                int sent = send(clientSocket, ptr + totalSent, toSend - totalSent, 0);
                if (sent <= 0) break;
                totalSent += sent;
            }
        }

#ifdef _WIN32
        shutdown(clientSocket, SD_SEND);
#else
        shutdown(clientSocket, SHUT_WR);
#endif
        CLOSE_SOCKET(clientSocket);
    }

    CLOSE_SOCKET(serverSocket);
}

} // namespace edge
