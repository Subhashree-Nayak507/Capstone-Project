
#include <iostream>
#include <string>
#include "envmon/ipc.hpp"

int main(int argc, char** argv) {
    std::string sock = "/tmp/envmon.sock", cmd;
    int i = 1;
    if (i + 1 < argc && std::string(argv[i]) == "-s") { sock = argv[i + 1]; i += 2; }
    for (; i < argc; ++i) { if (!cmd.empty()) cmd += ' '; cmd += argv[i]; }
    if (cmd.empty() || cmd == "-h" || cmd == "--help") {
        std::cerr << "Usage: envmonctl [-s SOCKET] COMMAND [ARGS]\n"
                     "Commands: PING | STATUS | LAST | SET_PERIOD <ms> | RESET | QUIT\n";
        return cmd.empty() ? 2 : 0;
    }
    std::string reply, err;
    if (!envmon::sendCommand(sock, cmd, reply, err)) { std::cerr << "envmonctl: " << err << '\n'; return 1; }
    std::cout << reply;
    return reply.rfind("ERR", 0) == 0 ? 1 : 0;
}
