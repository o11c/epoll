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

class GameInstance;
class GameShell;

class GameShell : public net::LineHandler, private cli::Shell
{
    friend class GameInstance;

    std::string nick;
    std::unique_ptr<chat::Connection> _chat;
    std::shared_ptr<GameInstance> _game;
    // really should be a subclass of AsynchronousPlayer,
    // just to enable logging
    // (GameShell should not exist)
    conquest::AsynchronousPlayer _player;

    cli::Status cmd_nosuch(const_string);
    cli::Status cmd_say(const_string);
    cli::Status cmd_nick(const_string);
    cli::Status cmd_help(const_string);
    cli::Status cmd_xyzzy(const_string);
    cli::Status cmd_new(const_string);
    cli::Status cmd_begin(const_string);
    cli::Status cmd_quit(const_string);
    cli::Status cmd_turn(const_string);

    void _add_commands()
    {
        this->_default = std::bind(&GameShell::cmd_nosuch, this, _1);
        this->add_command("say", "say a line of text",
                std::bind(&GameShell::cmd_say, this, _1));
        this->add_command("nick", "change your nickname",
                std::bind(&GameShell::cmd_nick, this, _1));
        this->add_command("help", "get help (duh)",
                std::bind(&GameShell::cmd_help, this, _1));
        this->_commands["xyzzy"] =
                std::bind(&GameShell::cmd_xyzzy, this, _1);
        this->add_command("join", "join a new game",
               std::bind(&GameShell::cmd_new, this, _1));
        this->add_command("begin", "actually start the new game",
               std::bind(&GameShell::cmd_begin, this, _1));
        this->add_command("quit", "quit the current game",
               std::bind(&GameShell::cmd_quit, this, _1));
        this->add_command("turn", "end the current turn of the game",
               std::bind(&GameShell::cmd_turn, this, _1));
    }

    GameShell(const GameShell&) = delete;

public:
    GameShell(int fd, const sockaddr *addr, socklen_t addrlen)
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

    ~GameShell()
    {
        cmd_quit(nullptr);
        chat::set_nick(nick, nullptr);
    }

    void handle(const_string line) override
    {
        cli::Shell::operator()(line);
    }

    void writes(const_array<const_string> arr)
    {
        for (const_string s : arr)
            this->wbh->write(s);
    }
};

cli::Status GameShell::cmd_nosuch(const_string argv)
{
    this->writes({"No such command: ", cli::split_first(argv).first, "\r\n"});
    return cli::Status::NOT_FOUND;
}

cli::Status GameShell::cmd_say(const_string argv)
{
    if (not this->_chat)
    {
        auto room = chat::Room::get(const_string(nullptr));
        this->_chat = make_unique<chat::Connection>(room, this->wbh);
    }
    // raw command
    this->_chat->say(this->nick, cli::trim(cli::split_first(argv).second));
    return cli::Status::NORMAL;
}

cli::Status GameShell::cmd_nick(const_string argv)
{
    const_string _ = nullptr, nick = nullptr;
    if (not cli::extract(argv, &_, &nick))
        return cli::Status::ARGS;
    if (not nick)
        return cli::Status::ERROR;
    if (not chat::set_nick(this->nick, nick))
    {
        this->writes({"Error: nick collision\r\n"});
        return cli::Status::ERROR;
    }
    this->nick = std::string(nick.begin(), nick.end());
    return cli::Status::NORMAL;
}

cli::Status GameShell::cmd_help(const_string argv)
{
    const_string _ = nullptr, cmd = nullptr;
    if (cli::extract(argv, &_, &cmd))
    {
        auto it = this->_helps.find(std::string(cmd.begin(), cmd.end()));
        if (it == this->_helps.end())
        {
            this->writes({"no help for command: ", cmd, "\r\n"});
            return cli::Status::ERROR;
        }
        this->writes({it->second, "\r\n"});
        return cli::Status::NORMAL;
    }
    if (not cli::extract(argv, &_))
        this->writes({"'help' takes 0 or 1 arguments, but whatever.\r\n"});
    this->writes({"Type 'help command' for more help.\r\n"});
    this->writes({"Command list:\r\n"});
    for (const auto& pair : this->_helps)
        this->writes({pair.first, "\r\n"});
    return cli::Status::NORMAL;
}

cli::Status GameShell::cmd_xyzzy(const_string argv)
{
    const_string _ = nullptr;
    if (not cli::extract(argv, &_))
        return cli::Status::ARGS;
    this->wbh->write(const_string("Nothing happens.\r\n"));
    return cli::Status::NORMAL;
}

// should this be merged with conquest::GalaxyGame?
// in the real world, GameShell would not even exist ...
class GameInstance
{
    friend class GameShell;
    std::string name;
    std::set<GameShell *> connections;
    std::unique_ptr<conquest::GalaxyGame> game;
    enum privacy_hack {privacy_ok};
    void connect(GameShell *);
    void disconnect(GameShell *);
    void start();
public:
    // really private
    GameInstance(const_string name, privacy_hack);
    ~GameInstance();

    static
    std::shared_ptr<GameInstance> get(const_string name);
    void broadcast(const_array<const_string> arr);
};

static
std::map<std::string, std::weak_ptr<GameInstance>> games;

GameInstance::GameInstance(const_string name, privacy_hack)
: name(name.begin(), name.end())
{}

GameInstance::~GameInstance()
{
    games.erase(name);
}

std::shared_ptr<GameInstance> GameInstance::get(const_string name)
{
    std::string s(name.begin(), name.end());
    auto it = games.find(s);
    if (it != games.end())
        return it->second.lock();
    auto n = std::make_shared<GameInstance>(s, privacy_ok);
    games.insert({s, n});
    return n;
}

void GameInstance::connect(GameShell *sh)
{
    connections.insert(sh);
}

void GameInstance::broadcast(const_array<const_string> arr)
{
    for (GameShell *c : connections)
        c->writes(arr);
}

void GameInstance::disconnect(GameShell *sh)
{
    if (connections.erase(sh) and game)
    {
        this->broadcast({"Uh-oh, somebody left during a game\r\n",});
        game->terminate();
        for (GameShell *s : connections)
            s->_game = nullptr;
        // (*this) has not been deleted - sh still has a reference
    }
}

void GameInstance::start()
{
    if (connections.size() < 2 or connections.size() > 6)
    {
        this->broadcast({"There must be 2-6 players to start!\r\n"});
        return;
    }
    conquest::Rules rules;
    conquest::NewPlayers players;
    for (GameShell *c : connections)
    {
        players.push_back({
            c->nick,
            make_unique<conquest::AsyncController>(&c->_player)
        });
    }
    this->game = make_unique<conquest::GalaxyGame>(rules, std::move(players));
}

cli::Status GameShell::cmd_begin(const_string argv)
{
    if (not this->_game)
        return cli::Status::ERROR;
    this->_game->start();
    return cli::Status::NORMAL;
}

cli::Status GameShell::cmd_quit(const_string argv)
{
    if (not this->_game)
        return cli::Status::ERROR;
    this->_game->disconnect(this);
    this->_game = nullptr;

    auto room = chat::Room::get(const_string(nullptr));
    this->_chat = make_unique<chat::Connection>(room, this->wbh);
    return cli::Status::NORMAL;
}

cli::Status GameShell::cmd_new(const_string argv)
{
    const_string _ = nullptr, gamename = nullptr;
    if (not cli::extract(argv, &_, &gamename))
    {
        if (not cli::extract(argv, &_))
            return cli::Status::ARGS;
        gamename = this->nick;
    }

    this->_game = GameInstance::get(gamename);
    this->_game->connect(this);
    auto room = chat::Room::get(gamename);
    this->_chat = make_unique<chat::Connection>(room, this->wbh);
    return cli::Status::NORMAL;
}

cli::Status GameShell::cmd_turn(const_string argv)
{
    if (not _player.controls)
        return cli::Status::ERROR;
    _player.controls = nullptr;
    return cli::Status::NORMAL;
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
            if (cli::extract(argv[i], &port) and port)
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
                            make_unique<GameShell>(fd, addr, addrlen)),
                        fd,
                        const_string("Type 'help' for command list.\r\n"));
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
