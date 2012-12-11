#ifndef CONQUEST_HPP
#define CONQUEST_HPP
// Copyright 2012 Ben Longbons
// GPL3+

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "const_array.hpp"

namespace conquest
{
    typedef unsigned Turn;
    typedef char PlanetID;
    typedef unsigned PlayerID;
    typedef float Distance;
    typedef std::array<Distance, 2> Coord;
    typedef unsigned ProductionRate;
    typedef unsigned FleetSize;
    typedef float Chance;

    struct Rules;
    class Controls;
    class Controller;
    class Player;
    class Planet;
    class AttackFleet;
    class GalaxyGame;

    // not fully implemented
    struct Rules
    {
        // TODO: put AI configuration here (currently all players are human)
        unsigned extra_planets;
        Coord map_size;
        bool blind;
        bool cumulative;
        bool produce_after_capture;
        bool show_neutral_ships;
        bool show_neutral_stats;
        ProductionRate neutral_production;
    };

    // The rest of this file may make more sense if you read it backwards.

    class Controls
    {
        GalaxyGame *game;
        Player *who;

        Controls(const Controls&) = delete;
        Controls& operator = (const Controls&) = delete;
    public:
        // really private
        Controls(GalaxyGame *, Player *);

        // The main thing a player can do.
        // The player *must* own the source planet.
        void send_ships(PlanetID from, PlanetID to, FleetSize);
        // If a human player exits.
        // This will not stop sending most messages, just .turn() ?
        void resign();
        // When this object is destroyed, the player is done with the turn.
        ~Controls();
    };

    class Controller
    {
        friend class GalaxyGame;

        // Called at the beginning of a game, to set size of the board.
        // The controller shall forget everything.
        virtual void reset(Rules rules, PlayerID self_id) = 0;
        // Usually called at the beginning of a game.
        // Discover a new planet.
        virtual void at(PlanetID planet, Coord coords) = 0;
        // Usually called at the beginning of a game.
        // Represents the discovery of a foreign government.
        virtual void player(PlayerID id, std::string name) = 0;
        // Change the ownership of a planet.
        // The controller shall forget the planet's production and defence.
        virtual void owned(PlanetID planet, PlayerID player) = 0;
        // Discover how effective ships are after leaving this planet.
        virtual void ability(PlanetID planet, Chance chance) = 0;
        // Discover a planet's production.
        virtual void produces(PlanetID planet, ProductionRate count) = 0;
        // Log failure to take over a planet.
        virtual void failed(PlanetID planet, PlayerID attacker) = 0;
        // End of the game.
        virtual void victory(PlayerID winner) = 0;

        // Callback in which the player actually decides to move.
        virtual void turn(std::unique_ptr<Controls> controls) = 0;

    public:
        virtual ~Controller() {};
    };

    class Player
    {
        friend class Controls;
        friend class GalaxyGame;

        std::string name;
        PlayerID id;
        // TODO: implement later
        // Stats stats;
        std::unique_ptr<Controller> controller;

        Player(const_string n, PlayerID i, std::unique_ptr<Controller> c)
        : name(n.begin(), n.end()), id(i), controller(std::move(c))
        {}
    };

    class Planet
    {
        friend class Controls;

        PlanetID name; // A-Z
        Player *owner;
        FleetSize defence_fleet;
        ProductionRate base_production;
        Coord coords;
        Chance ability;
        Turn last_conquest;
    public:
        static
        Distance dist(Planet *f, Planet *t);
    };

    class AttackFleet
    {
        Player *owner;
        FleetSize ships;
        Planet *destination;
        Turn arrival;
        Chance strength;
    public:
        AttackFleet(Player *, FleetSize, Planet *, Turn, Chance);
    };

    typedef std::pair<const_string, std::unique_ptr<Controller>> new_player;
    typedef std::vector<new_player> NewPlayers;

    class GalaxyGame
    {
        Rules rules;
        std::vector<Planet> planets;
        std::vector<AttackFleet> fleets;
        std::vector<Player> players;
        Turn turn;

        friend class Controls;
        unsigned control_count;
    public:
        GalaxyGame(Rules r, NewPlayers p);
        void tick();
        void terminate();
    };
}

#endif // CONQUEST_HPP
