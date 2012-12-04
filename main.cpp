// Copyright 2012 Ben Longbons
#include "make-unique.hpp"
#include "net.hpp"

#include <iostream>
#include <sstream>

// for testing: just reverse input lines
void echo(const_array<uint8_t> line, net::BufferHandler *wbh)
{
    std::vector<uint8_t> rline(line.size() + 1);
    *std::copy(line.rbegin(), line.rend(), rline.begin()) = '\n';
    wbh->write(const_array<uint8_t>{rline.data(), rline.size()});
}

int main(int argc, char **argv)
{
    uint16_t port = 0;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--help")
        {
            std::cout << "Usage: ./main --port <number>\n";
            std::cout << "Then type 'help' for interactive help\n";
            return 0;
        }
        if (arg == "--port")
        {
            std::istringstream(argv[++i]) >> port;
            continue;
        }
        std::cerr << "Error: unknown argument: " << arg << std::endl;
    }
    if (port == 0)
    {
        std::cerr << "Error: --port not specified" << std::endl;
        return 1;
    }
    net::SocketSet pool;
    pool.add(
        make_unique<net::ListenHandler>(
            [](int fd, const sockaddr *, socklen_t)
            {
                return make_unique<net::BufferHandler>(
                        make_unique<net::SentinelParser>(echo),
                        fd);
            },
            port,
            net::ipv4_any));

    while (pool)
        pool.poll();
}
