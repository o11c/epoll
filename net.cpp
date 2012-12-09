// Copyright 2012 Ben Longbons
// GPL3+
#include "net.hpp"

#include <cerrno>
#include <cstdlib>
#include <cassert>

#include <unistd.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/epoll.h>

#include <algorithm>
#include <sstream>

static_assert(EAGAIN == EWOULDBLOCK, "you have crazy errno values ...");

namespace net
{

std::string sockaddr_to_string(int fd, const sockaddr *addr, socklen_t)
{
    switch(addr->sa_family)
    {
    case AF_INET:
    {
        char buf[INET_ADDRSTRLEN];
        auto addr4 = reinterpret_cast<const sockaddr_in *>(addr);
        const in_addr& ia = addr4->sin_addr;
        return inet_ntop(AF_INET, &ia, buf, INET_ADDRSTRLEN);
    }
    case AF_INET6:
    {
        char buf[INET6_ADDRSTRLEN];
        auto addr6 = reinterpret_cast<const sockaddr_in6 *>(addr);
        const in6_addr& ia = addr6->sin6_addr;
        return inet_ntop(AF_INET6, &ia, buf, INET6_ADDRSTRLEN);
    }
    case AF_UNIX:
    {
        auto addru = reinterpret_cast<const sockaddr_un *>(addr);
        return addru->sun_path;
    }
    }
    std::ostringstream o;
    o << "/proc/self/fd/" << fd;
    return o.str();
}

epoll_event Handler::create_event()
{
    epoll_event event {};
    if (this->read)
        event.events |= EPOLLIN | EPOLLRDHUP;
    if (this->write)
        event.events |= EPOLLOUT;
    // I don't understand exactly what the following does
    // so I'll set them and see what happens
    event.events |= EPOLLPRI;
    // the following is safe only if all handlers loop until EAGAIN
    // AND you never check writing when it's not needed
    // TODO enable this
    //event.events |= EPOLLET;
    event.data.fd = this->fd;
    return event;
}

void Handler::enable_write()
{
    assert(this->read);
    this->write = true;
    epoll_event event = this->create_event();
    epoll_ctl(set->epfd, EPOLL_CTL_MOD, this->fd, &event);
}

#if 0
void Handler::replace(std::unique_ptr<Handler> h)
{
    // Won't work - fd will be close
    // (need to dup or something)
    // Also would cause problems with the poll() loop.
    // It's probably not worth fixing - instead change at parser level.
    if (h->fd != this->fd)
    {
        fprintf(stderr, "Fatal: replacement handler differs in fd!\n");
        abort();
    }
    set->sockets[fd] = std::move(h);
    // this has been destroyed
}
#endif

bool Handler::add_peer(std::unique_ptr<Handler> h)
{
    return set->add(std::move(h));
}

Handler::~Handler()
{
    if (fd != -1)
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

bool SocketSet::add(std::unique_ptr<net::Handler> sock)
{
    if (epfd == -1)
        return false;
    if (!sock)
        return false;
    int fd = sock->fd;
    bool read = sock->read;
    bool write = sock->write;
    if (fd == -1 or not (read or write))
        return false;

    epoll_event event = sock->create_event();

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event) == -1)
    {
        fprintf(stderr, "Failed to add to epoll: %m\n");
        return false;
    }
    sock->set = this;
    // TODO: ensure that fd was not already in the set - it will be closed!
    sockets[fd] = std::move(sock);
    return true;
}

void SocketSet::wipe()
{
    close(epfd);
    epfd = -1;
    sockets.clear();
}

void SocketSet::handle_event(epoll_event event)
{
    bool modify = false;

    const int fd = event.data.fd;
    const auto it = sockets.find(fd);
    if (it == sockets.end())
    {
        fprintf(stderr, "epoll event fd not found: %d\n", fd);
        wipe();
        return;
    }

    Handler *p = it->second.get();
    if (event.events & EPOLLIN)
    {
        // for record of unknowns
        event.events &= ~EPOLLIN;
        // Note: p->on_readable() may flip q->write for any q in
        // set, including p. The code below is just fine with that.
        if (p->on_readable() == Handler::Status::DROP)
        {
            modify = true;
            p->read = false;
        }
    }

    if (event.events & EPOLLOUT)
    {
        event.events &= ~EPOLLOUT;
        if (p->on_writable() == Handler::Status::DROP)
        {
            // note: this is normal, when a write completes.
            modify = true;
            p->write = false;
        }
    }

    if (event.events & EPOLLRDHUP)
    {
        event.events &= ~EPOLLRDHUP;
        modify = true;
        // from what I can tell, this only happens in cases
        // where EPOLLIN is also returned. Assuming the on_readable()
        // hook is sensible, it will detect read() returning 0 and
        // disable itself already.
        if (p->read)
            fprintf(stderr, "Got RDHUP without first hanging up in read");
        p->read = false;
    }

    if (event.events & EPOLLERR)
    {
        // I'm not sure when this happens
        event.events &= ~EPOLLERR;
        fprintf(stderr, "fd %d error\n", fd);
        wipe();
        return;
    }

    if (event.events & EPOLLHUP)
    {
        event.events &= ~EPOLLHUP;
        modify = true;
        // This happens when shutdown(fd, SHUT_WR) is called
        if (p->write)
            fprintf(stderr, "Got HUP without first hanging up in write");
        p->write = false;
    }


    if (event.events)
        fprintf(stderr, "epoll got unknown: %x", event.events);

    if (not modify)
        return;

    // Note: p->write may have been enabled by the read callback.
    // In this case, it will have called epoll_ctl itself.
    int op = (p->read or p->write)
        ? EPOLL_CTL_MOD
        : EPOLL_CTL_DEL;
    event = p->create_event();
    if (epoll_ctl(this->epfd, op, fd, &event) == -1)
    {
        fprintf(stderr, "epoll_ctl mod/del failed: %m");
        wipe();
        return;
    }
    if (op == EPOLL_CTL_DEL) // not (read or write)
        sockets.erase(it);
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
            this->handle_event(events[i]);

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
            add_peer(adder(cfd, addr_ptr, addr_len));
        }
    }
}

Handler::Status ListenHandler::on_writable()
{
    return Handler::Status::DROP;
}

BufferHandler::BufferHandler(std::unique_ptr<Parser> p, int fd)
: Handler(fd, true, false) // write enabled as needed
, parser(std::move(p))
{
    parser->init(this);
}

void BufferHandler::write(const_array<uint8_t> b)
{
    size_t os = outbuf.size();
    size_t ns = b.size();
    if (!os && ns)
        this->enable_write();
    outbuf.resize(os + ns);
    std::copy(b.begin(), b.end(), outbuf.begin() + os);
}

Handler::Status BufferHandler::do_readable()
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
        // if I called parse on each iteration, it would probably:
        // + do less memory allocation
        // + remove the need for split methods
        // + behave well when whole packets are read at a time (common)
        // - behave poorly when packets cross the 4096-byte limit (rare)
        // - be less icache-friendly, and possibly less dcache-friendly
    }
}

Handler::Status BufferHandler::on_readable()
{
    Handler::Status rv = this->do_readable();
    size_t n = (this->parser)->parse({inbuf.data(), inbuf.size()});
    inbuf.erase(inbuf.begin(), inbuf.begin() + n);
    return rv;
}

Handler::Status BufferHandler::on_writable()
{
    ssize_t w = ::write(fd, outbuf.data(), outbuf.size());
    if (w == -1)
        return errno == EAGAIN
            ? Handler::Status::KEEP
            : Handler::Status::DROP;
    outbuf.erase(outbuf.begin(), outbuf.begin() + w);
    return outbuf.empty() ? Handler::Status::DROP : Handler::Status::KEEP;
}

void LineHandler::init(BufferHandler *wbh)
{
    this->wbh = wbh;
}

SentinelParser::SentinelParser(std::unique_ptr<LineHandler> lh, uint8_t s)
: sentinel(s)
, no_sentinel()
, line_handler(std::move(lh))
{}

void SentinelParser::init(BufferHandler *wbh)
{
    line_handler->init(wbh);
}

size_t SentinelParser::parse(Bytes bytes)
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
        line_handler->handle(Bytes(data, search - data));
        ++search;
        data = search;
    }
}

} // namespace net
