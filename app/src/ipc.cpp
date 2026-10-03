#include "envmon/ipc.hpp"
#include <cerrno>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace envmon {

bool sendCommand(const std::string& path, const std::string& command,
                 std::string& reply, std::string& err, int timeout_ms) {
    int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) { err = std::string("socket: ") + std::strerror(errno); return false; }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (path.size() >= sizeof(addr.sun_path)) { ::close(fd); err = "socket path too long"; return false; }
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        err = "connect(" + path + "): " + std::strerror(errno);
        ::close(fd);
        return false;
    }
    std::string msg = command + "\n";
    if (::send(fd, msg.data(), msg.size(), MSG_NOSIGNAL) < 0) {
        err = std::string("send: ") + std::strerror(errno);
        ::close(fd);
        return false;
    }
    reply.clear();
    char buf[1024];
    for (;;) {
        pollfd p{fd, POLLIN, 0};
        int rc = ::poll(&p, 1, timeout_ms);
        if (rc == 0) { err = "timeout waiting for reply"; ::close(fd); return false; }
        if (rc < 0) { if (errno == EINTR) continue; err = std::strerror(errno); ::close(fd); return false; }
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n < 0) { if (errno == EINTR) continue; err = std::strerror(errno); ::close(fd); return false; }
        if (n == 0) break;
        reply.append(buf, static_cast<std::size_t>(n));
    }
    ::close(fd);
    return true;
}

}  // namespace envmon
