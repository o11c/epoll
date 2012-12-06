#include "chat.hpp"

#include <map>

namespace chat
{

const_string COLOR_RED      = "\e[31m";
const_string COLOR_GREEN    = "\e[32m";
const_string COLOR_YELLOW   = "\e[33m";
const_string COLOR_BLUE     = "\e[34m";
const_string COLOR_MAGENTA  = "\e[35m";
const_string COLOR_CYAN     = "\e[36m";

static
const_string colors[] =
{
    COLOR_RED,
    COLOR_GREEN,
    COLOR_YELLOW,
    COLOR_BLUE,
    COLOR_MAGENTA,
    COLOR_CYAN,
};

static
size_t color_index = 0;

Connection::Connection(
        std::shared_ptr<Room> r,
        net::BufferHandler *b)
: room(r), out(b), color(colors[color_index++])
{
    if (color_index == 6)
        color_index = 0;
    room->chatters.insert(this);
}

Connection::~Connection()
{
    room->chatters.erase(this);
}

void Connection::say(const_string nick, const_string msg)
{
    const_string colon = ": ";
    const_string eol = "\e[m\r\n";
    for (Connection *c : room->chatters)
    {
        c->out->write(color);
        c->out->write(nick);
        c->out->write(colon);
        c->out->write(msg);
        c->out->write(eol);
    }
}

bool set_nick(const_string oldname, const_string name)
{
    static
    std::set<std::string> nicks;

    if (not name)
    {
        nicks.erase(std::string(oldname.begin(), oldname.end()));
        return false;
    }
    std::string nick(name.begin(), name.end());
    if (not nicks.insert(nick).second)
        return false;
    nicks.erase(std::string(oldname.begin(), oldname.end()));
    return true;
}

static
std::map<std::string, std::weak_ptr<Room>> rooms;

Room::Room(std::string name, privacy_hack)
: name(name)
{}

Room::~Room()
{
    rooms.erase(name);
}

std::shared_ptr<Room> Room::get(const_string room)
{
    std::string s(room.begin(), room.end());
    auto it = rooms.find(s);
    if (it != rooms.end())
        return it->second.lock();
    auto n = std::make_shared<Room>(s, privacy_ok);
    rooms[s] = n;
    return n;
}

} // namespace chat
