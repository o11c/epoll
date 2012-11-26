// Copyright 2012 Ben Longbons
#include "make-unique.hpp"
#include "net.hpp"

constexpr uint16_t PORT = 5000;

void echo(const_array<uint8_t> line, net::BufferHandler *wbh)
{
    std::vector<uint8_t> rline(line.rbegin(), line.rend());
    wbh->write(const_array<uint8_t>{rline.data(), rline.size()});
}

int main(int argc, char **)
{
    net::SocketSet pool;
    pool.add(
        make_unique<net::ListenHandler>(
            [&pool](int fd, const sockaddr *, socklen_t)
            {
                pool.add(
                    make_unique<net::BufferHandler>(
                        make_unique<net::SentinelParser>(echo),
                        fd));
            },
            PORT,
            net::ipv4_any));

    while (pool)
    {
        if (argc--)
            pool.poll();
        else
            pool.poll(std::chrono::seconds(5));
    }
}
