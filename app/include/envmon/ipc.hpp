#pragma once
#include <string>

namespace envmon {

/// Send one command line to the daemon's Unix-domain socket and collect the
/// full reply (until the server closes the connection).
bool sendCommand(const std::string& socket_path, const std::string& command,
                 std::string& reply, std::string& err, int timeout_ms = 2000);

}  // namespace envmon
