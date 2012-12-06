#ifndef CHAT_HPP
#define CHAT_HPP

#include <memory>
#include <set>

#include "const_array.hpp"
#include "net.hpp"

namespace chat
{

extern const_string COLOR_RED;
extern const_string COLOR_GREEN;
extern const_string COLOR_YELLOW;
extern const_string COLOR_BLUE;
extern const_string COLOR_MAGENTA;
extern const_string COLOR_CYAN;

class Room;
class Connection
{
    friend class Room;

    std::shared_ptr<Room> room;
    net::BufferHandler *out;
    const_string color;

    Connection(const Connection&) = delete;
public:
    Connection(std::shared_ptr<Room>, net::BufferHandler *);
    ~Connection();
    void say(const_string name, const_string msg);
};

bool set_nick(const_string oldnick, const_string nick);

class Room
{
    friend class Connection;

    std::string name;
    std::set<Connection *> chatters;

    enum privacy_hack {privacy_ok};
public:
    // really private
    Room(std::string name, privacy_hack);

    static std::shared_ptr<Room> get(const_string name);
    ~Room();
};

} // namespace chat

#endif // CHAT_HPP
