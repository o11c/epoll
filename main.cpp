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

bool ok_and_eof(std::istream& in)
{
    if (not in)
        return false;
    char c;
    if (in >> c)
        return false;
    return true;
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
            std::cout << "Port number must be between 1 and 65535,\n";
            std::cout << "and you must have appropriate permissions.\n";
            std::cout << '\n';
            std::cout << "Then use an external client to connect.\n";
            std::cout << "e.g.: netcat <IP of localhost> <port>\n";
            std::cout << "You can use telnet in place of netcat,\n";
            std::cout << "but it may be awkward\n";
            return 0;
        }
        if (arg == "--port")
        {
            if (++i == argc)
            {
                std::cerr << "Error: port argument not given\n";
                return 1;
            }
            if (ok_and_eof(std::istringstream(argv[i]) >> port) and port)
                continue;
            std::cerr << "Error: --port argument not integer in range\n";
            return 1;
        }
        std::cerr << "Error: unknown argument: " << arg << '\n';
    }
    if (port == 0)
    {
        std::cerr << "Error: --port not specified, try --help\n";
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
