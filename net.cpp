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

#include <algorithm>

static_assert(EAGAIN == EWOULDBLOCK, "you have crazy errno values ...");

namespace net
{

Handler::~Handler()
{
    close(fd);
}

SocketSet::SocketSet()
: epfd(epoll_create1(0))
, sockets()
{
    if (epfd == -1)
        // default ctor of std::map does no allocation
        fprintf(stderr, "Failed to create epoll instance: %m\n");
}

SocketSet::~SocketSet()
{
    if (epfd != -1)
        close(epfd);
}

SocketSet::operator bool() const
{
    // instead guarantee that if epfd == -1, sockets is always empty
    // return epfd != -1 and !sockets.empty();
    return not sockets.empty();
}

void SocketSet::add(std::unique_ptr<net::Handler> sock)
{
    if (epfd == -1)
        return;
    if (!sock)
        return;
    int fd = sock->fd;
    bool read = sock->read;
    bool write = sock->write;
    if (fd == -1 or not (read or write))
        return;

    epoll_event event {};
    if (sock->read)
        event.events |= EPOLLIN;
    if (sock->write)
        event.events |= EPOLLOUT;
    // I don't understand exactly what the following do
    // so I'll set them and see what happens
    event.events |= EPOLLRDHUP;
    event.events |= EPOLLPRI;
    // the following is safe only if all handlers loop until EAGAIN
    // AND you never check writing when it's not needed
    // TODO enable this when writing check is fixed
    //event.events |= EPOLLET;
    event.data.fd = fd;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event) == -1)
    {
        fprintf(stderr, "Failed to add to epoll: %m\n");
        return;
    }
    sockets[fd] = std::move(sock);
}

void SocketSet::wipe()
{
    close(epfd);
    epfd = -1;
    sockets.clear();
}

void SocketSet::poll(std::chrono::milliseconds timeout)
{
    constexpr static int MAX_EVENTS = 256;
    epoll_event events[MAX_EVENTS];
    int n = MAX_EVENTS;
    while (true)
    {
        n = epoll_wait(epfd, events, MAX_EVENTS, timeout.count());
        if (n == -1)
        {
            fprintf(stderr, "epoll_wait(): %m\n");
            wipe();
            return;
        }

        for (int i = 0; i < n; ++i)
        {
            epoll_event& event = events[i];
            int fd = event.data.fd;
            auto it = sockets.find(fd);
            if (it == sockets.end())
            {
                fprintf(stderr, "epoll event fd not found: %m\n");
                wipe();
                return;
            }

            bool remove = false;
            Handler *p = it->second.get();
            if (event.events & EPOLLIN)
            {
                event.events &= ~EPOLLIN;
                if (p->on_readable() == Handler::Status::DROP)
                {
                    int err;
                    remove = not p->write;
                    p->read = false;
                    if (remove)
                        err = epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &event);
                    else
                        err = epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &event);

                    if (err == -1)
                    {
                        fprintf(stderr, "epoll_ctl(mod/del): %m\n");
                        wipe();
                        return;
                    }
                }
            }

            if (event.events & EPOLLOUT)
            {
                event.events &= ~EPOLLOUT;
                if (p->on_writable() == Handler::Status::DROP)
                {
                    int err;
                    remove = not p->read;
                    p->write = false;
                    if (remove)
                        err = epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &event);
                    else
                        err = epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &event);

                    if (err == -1)
                    {
                        fprintf(stderr, "epoll_ctl(mod/del): %m\n");
                        wipe();
                        return;
                    }
                }
            }

            if (event.events & EPOLLRDHUP)
            {
                event.events &= ~EPOLLRDHUP;
                int err;
                fprintf(stderr, "epoll rdhup");
                remove = not p->write;
                if (remove)
                    err = epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &event);
                else
                    err = epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &event);
                if (err == -1)
                {
                    fprintf(stderr, "epoll_ctl(mod/del): %m\n");
                    wipe();
                    return;
                }
            }

            if (event.events)
                fprintf(stderr, "epoll got unknown: %u", event.events);

            // TODO fix removal
            if (remove)
                sockets.erase(it);
        }

        if (__builtin_expect(n != MAX_EVENTS, true))
            break;

        // in the unlikely event that there are LOTS of events,
        // for the rest don't wait for any time
        timeout = std::chrono::milliseconds::zero();
    }
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
    while (true)
    {
        uint8_t buf[4096];
        ssize_t r = ::read(fd, buf, sizeof(buf));
        if (r == 0)
            return Handler::Status::DROP;
        if (r == -1)
            return errno == EAGAIN
                ? Handler::Status::KEEP
                : Handler::Status::DROP;
        size_t s = inbuf.size();
        inbuf.resize(s + r);
        std::copy(buf, buf + r, inbuf.data() + s);
    }
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
