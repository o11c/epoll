// Copyright 2012 Ben Longbons
// GPL3+
#include "make-unique.hpp"
#include "net.hpp"
#include "cli.hpp"
#include "chat.hpp"
#include "conquest-player.hpp"

#include <cassert>
#include <iostream>
#include <sstream>

using namespace std::placeholders;

class SimpleShell;
cli::Status cmd_nosuch(SimpleShell *, const_string);
cli::Status cmd_join(SimpleShell *, const_string);
cli::Status cmd_part(SimpleShell *);
cli::Status cmd_say(SimpleShell *, const_string);
cli::Status cmd_nick(SimpleShell *, const_string);
cli::Status cmd_help(SimpleShell *, const_string);
cli::Status cmd_xyzzy(SimpleShell *);

class SimpleShell : public net::LineHandler, private cli::Shell
{
    std::string nick;
    std::unique_ptr<chat::Connection> chat;

    friend cli::Status cmd_nosuch(SimpleShell *, const_string);
    friend cli::Status cmd_join(SimpleShell *, const_string);
    friend cli::Status cmd_part(SimpleShell *);
    friend cli::Status cmd_say(SimpleShell *, const_string);
    friend cli::Status cmd_nick(SimpleShell *, const_string);
    friend cli::Status cmd_help(SimpleShell *, const_string);
    friend cli::Status cmd_xyzzy(SimpleShell *);

    void _add_commands()
    {
        this->_default = std::bind(cmd_nosuch, this, _1);
        this->_commands["join"] = std::bind(cmd_join, this, _2);
        this->_commands["part"] = std::bind(cmd_part, this);
        this->_commands["say"] = std::bind(cmd_say, this, _2);
        this->_commands["nick"] = std::bind(cmd_nick, this, _2);
        this->_commands["help"] = std::bind(cmd_help, this, _2);
        this->_commands["xyzzy"] = std::bind(cmd_xyzzy, this);
    }

    SimpleShell(const SimpleShell&) = delete;

public:
    SimpleShell(int fd, const sockaddr *addr, socklen_t addrlen)
    {
        std::string nick = net::sockaddr_to_string(fd, addr, addrlen);
        this->nick = nick;
        if (not chat::set_nick(nullptr, nick))
        {
            // TODO: refactor into some net class
            // TODO: offer a "last rites" message
            shutdown(fd, SHUT_RD);
        }
        else
            _add_commands();
    }

    ~SimpleShell()
    {
        chat::set_nick(nick, nullptr);
    }

    void handle(const_string line) override
    {
        cli::Shell::operator()(line);
    }
};

cli::Status cmd_nosuch(SimpleShell *sh, const_string c)
{
    std::ostringstream o;
    o << "No such command: " << c << "\r\n";
    std::string s = o.str();
    sh->wbh->write(const_string(s));
    return cli::Status::NOT_FOUND;
}

cli::Status cmd_join(SimpleShell *sh, const_string args)
{
    if (sh->chat)
    {
        sh->wbh->write(const_string("Error: already connected.\r\n"));
        return cli::Status::ERROR;
    }
    auto room = chat::Room::get(cli::trim(args));
    sh->chat = make_unique<chat::Connection>(room, sh->wbh);
    sh->wbh->write(const_string("Hello, "));
    sh->wbh->write(const_string(sh->nick));
    sh->wbh->write(const_string("\r\n"));
    return cli::Status::NORMAL;
}

cli::Status cmd_part(SimpleShell *sh)
{
    if (not sh->chat)
    {
        sh->wbh->write(const_string("Error: not connected.\r\n"));
        return cli::Status::ERROR;
    }
    sh->chat = nullptr;
    return cli::Status::NORMAL;
}

cli::Status cmd_say(SimpleShell *sh, const_string args)
{
    if (not sh->chat)
    {
        sh->wbh->write(const_string("Error: not connected.\r\n"));
        return cli::Status::ERROR;
    }
    sh->chat->say(sh->nick, cli::trim(args));
    return cli::Status::NORMAL;
}

cli::Status cmd_nick(SimpleShell *sh, const_string args)
{
    const_string nick = cli::trim(args);
    if (not nick)
        return cli::Status::ERROR;
    if (not chat::set_nick(sh->nick, nick))
    {
        sh->wbh->write(const_string("Error: nick collision\r\n"));
        return cli::Status::ERROR;
    }
    sh->nick = std::string(nick.begin(), nick.end());
    return cli::Status::NORMAL;
}

cli::Status cmd_help(SimpleShell *sh, const_string)
{
    const_string messages[] =
    {
        "Commands:\r\n",
        "join\r\n",
        "part\r\n",
        "say\r\n",
        "nick\r\n",
        "help\r\n",
    };
    for (const_string s : messages)
        sh->wbh->write(s);
    return cli::Status::NORMAL;
}

cli::Status cmd_xyzzy(SimpleShell *sh)
{
    sh->wbh->write(const_string("Nothing happens.\r\n"));
    return cli::Status::NORMAL;
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
    auto adder =
            [](int fd, const sockaddr *addr, socklen_t addrlen)
            {
                return make_unique<net::BufferHandler>(
                        make_unique<net::SentinelParser>(
                            make_unique<SimpleShell>(fd, addr, addrlen)),
                        fd);
            };
    std::cout << "try IPv6 ..." << std::endl;
    if (pool.add(make_unique<net::ListenHandler>(adder, port, net::ipv6_any)))
        std::cout << "IPv6 okay" << std::endl;
    std::cout << "try IPv4 (may fail if IPv6 succeeded) ..." << std::endl;
    if (pool.add(make_unique<net::ListenHandler>(adder, port, net::ipv4_any)))
        std::cout << "IPv4 okay" << std::endl;

    while (pool)
        pool.poll();
}
