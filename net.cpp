// Copyright 2012 Ben Longbons
#include "net.hpp"

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/epoll.h>

static_assert(EAGAIN == EWOULDBLOCK, "you have crazy errno values ...");

namespace net
{

SocketSet::SocketSet()
: epfd(epoll_create1(0))
, sockets()
{}

SocketSet::~SocketSet()
{
    close(epfd);
}

SocketSet::operator bool() const
{
    return !sockets.empty();
}

void SocketSet::add(std::unique_ptr<net::Handler> sock)
{
    if (!sock)
        return;
    if (sock->fd == -1)
        return;
    sockets[sock->fd] = std::move(sock);
}

void SocketSet::poll(std::chrono::milliseconds timeout)
{
    ; // TODO
}

void SocketSet::poll()
{
    // ssh, don't tell!
    poll(std::chrono::milliseconds(-1));
}

int _create_listen_socket(const sockaddr *addr, socklen_t addr_len)
{
    int sock = socket(addr->sa_family, SOCK_STREAM, 0);
    if (sock == -1)
    {
        fprintf(stderr, "socket() failed: %m\n");
        return -1;
    }

    if (bind(sock, addr, addr_len) == -1)
    {
        fprintf(stderr, "bind() failed: %m\n");
        close(sock);
        return -1;
    }

    if (listen(sock, SOMAXCONN) == -1)
    {
        fprintf(stderr, "listen() failed: %m\n");
        close(sock);
        return -1;
    }

    if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1)
    {
        fprintf(stderr, "fcntl(O_NONBLOCK) failed: %m\n");
        close(sock);
        return -1;
    }

    return sock;
}

int _create_listen_socket(uint16_t port, IPv4 iface)
{
    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr = iface;

    return _create_listen_socket(reinterpret_cast<sockaddr *>(&addr),
            sizeof(addr));
}

int _create_listen_socket(uint16_t port, IPv6 iface)
{
    sockaddr_in6 addr {};
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port);
    if (0)
        addr.sin6_flowinfo = 0;
    addr.sin6_addr = iface;
    if (0)
        addr.sin6_scope_id = 0;

    return _create_listen_socket(reinterpret_cast<sockaddr *>(&addr),
            sizeof(addr));
}

constexpr size_t UNIX_PATH_MAX = sizeof(sockaddr_un::sun_path);
static_assert(UNIX_PATH_MAX == 108, "UNIX_PATH_MAX not as documented");

int _create_listen_socket(const_string path)
{
    if (path.size() >= UNIX_PATH_MAX)
    {
        fprintf(stderr, "unix path too long(%zu): %s\n",
                path.size(), path.data());
        return -1;
    }

    sockaddr_un addr {};
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, path.data(), path.size());
    memset(addr.sun_path + path.size(), 0, UNIX_PATH_MAX - path.size());

    return _create_listen_socket(reinterpret_cast<sockaddr *>(&addr),
            sizeof(addr));
}

ListenHandler::ListenHandler(Cb c, uint16_t port, IPv4 iface)
: Handler(_create_listen_socket(port, iface), true, false)
, adder(c)
{}

ListenHandler::ListenHandler(Cb c, uint16_t port, IPv6 iface)
: Handler(_create_listen_socket(port, iface), true, false)
, adder(c)
{}

ListenHandler::ListenHandler(Cb c, const_string path)
: Handler(_create_listen_socket(path), true, false)
, adder(c)
{}

Handler::Status ListenHandler::on_readable()
{
    while (true)
    {
        sockaddr_storage addr;
        socklen_t addr_len = sizeof(addr);
        sockaddr *addr_ptr = reinterpret_cast<sockaddr *>(&addr);

        int cfd = accept4(this->fd, addr_ptr, &addr_len, SOCK_NONBLOCK);
        if (cfd == -1)
            return errno == EAGAIN
                ? Handler::Status::KEEP
                : Handler::Status::DROP;

        if (addr_len > sizeof(addr))
        {
            // shouldn't happen - that's what sockaddr_storage is *for*
            close(cfd);
            return Handler::Status::DROP;
        }

        switch(addr_ptr->sa_family)
        {
        case AF_INET:
        case AF_INET6:
        case AF_UNIX:
        default:
            adder(cfd, addr_ptr, addr_len);
        }
    }
}

Handler::Status ListenHandler::on_writable()
{
    return Handler::Status::DROP;
}

BufferHandler::BufferHandler(std::unique_ptr<Parser> p, int fd)
: Handler(fd, true, true)
, parser(std::move(p))
{
}

void BufferHandler::write(const_array<uint8_t> b)
{
    size_t os = outbuf.size();
    size_t ns = b.size();
    outbuf.resize(os + ns);
    std::copy(b.begin(), b.end(), outbuf.begin() + os);
}

Handler::Status BufferHandler::on_readable()
{
    return Handler::Status::KEEP;
}

Handler::Status BufferHandler::on_writable()
{
    ssize_t w = ::write(fd, outbuf.data(), outbuf.size());
    if (w == -1)
        return errno == EAGAIN
            ? Handler::Status::KEEP
            : Handler::Status::DROP;
    outbuf.erase(outbuf.begin(), outbuf.begin() + w);
    return Handler::Status::KEEP;
}

SentinelParser::SentinelParser(Cb f, uint8_t s)
: sentinel(s)
, no_sentinel()
, do_line(std::move(f))
{
}

size_t SentinelParser::parse(Bytes bytes, BufferHandler *wbh)
{
    auto search = bytes.begin() + no_sentinel;
    auto data = bytes.begin();
    const uint8_t c = sentinel;
    while (true)
    {
        search = std::find(search, bytes.end(), c);
        if (search == bytes.end())
        {
            no_sentinel = bytes.end() - data;
            return data - bytes.begin();
        }
        do_line(Bytes(data, search - data), wbh);
        ++search;
        data = search;
    }
}

} // namespace net
