// Copyright 2012 Ben Longbons
#ifndef NET_HPP
#define NET_HPP

#include <cstdint>

#include <chrono>
#include <map>
#include <memory>
#include <vector>

#include "const_array.hpp"
#include "ip.hpp"

namespace net
{

class Handler
{
    friend class SocketSet;
protected:
    const int fd;
private:
    bool read;
    bool write;
public:
    enum class Status : bool
    {
        DROP,
        KEEP,
    };
    Handler(int f, bool r, bool w)
    : fd(f), read(r), write(w)
    {}
    virtual ~Handler() = default;
private:
    virtual Status on_readable() = 0;
    virtual Status on_writable() = 0;
};

class SocketSet
{
    int epfd;
    std::map<int, std::unique_ptr<Handler>> sockets;
public:
    SocketSet();
    ~SocketSet();
    operator bool() const;
    void add(std::unique_ptr<Handler> handler);
    void poll();
    void poll(std::chrono::milliseconds timeout);
};

class ListenHandler : public Handler
{
    // TODO replace with a class so I can do overridden versions
    // for IPv4, IPv6, and Unix connections
    typedef std::function<void(int, const sockaddr *, socklen_t)> Cb;

    Cb adder;
public:
    ListenHandler(Cb c, uint16_t port, IPv4 iface);
    ListenHandler(Cb c, uint16_t port, IPv6 iface);
    ListenHandler(Cb c, const_string unixpath);
    virtual Handler::Status on_readable() override;
    virtual Handler::Status on_writable() override;
};

class Parser;
class BufferHandler : public Handler
{
    std::vector<uint8_t> inbuf;
    std::vector<uint8_t> outbuf;
    std::unique_ptr<Parser> parser;
public:
    BufferHandler(std::unique_ptr<Parser> p, int fd);
    void write(const_array<uint8_t> b);
    virtual Handler::Status on_readable() override;
    virtual Handler::Status on_writable() override;
};

class Parser
{
protected:
    typedef const_array<uint8_t> Bytes;
public:
    virtual size_t parse(Bytes bytes, BufferHandler *wbh) = 0;
    virtual ~Parser() = default;
};

class SentinelParser : public Parser
{
    typedef std::function<void(Bytes, BufferHandler *)> Cb;

    const uint8_t sentinel;
    size_t no_sentinel;
    Cb do_line;
public:
    SentinelParser(Cb f, uint8_t s='\n');
    virtual size_t parse(Bytes bytes, BufferHandler *wbh) override;
};

} // namespace net

#endif // NET_HPP
