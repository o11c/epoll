// Copyright 2012 Ben Longbons
// GPL3+
#include "conquest.hpp"

#include <cmath>

#include <iostream>

#include "make-unique.hpp"

namespace conquest
{
    Controls::Controls(GalaxyGame *g, Player *p)
    : game(g), who(p)
    {
        game->control_count++;
    }

    Controls::~Controls()
    {
        if (not --game->control_count)
            game->tick();
    }

    void Controls::send_ships(PlanetID from, PlanetID to, FleetSize s)
    {
        Planet *f = &game->planets[from - 'A'];
        Planet *t = &game->planets[to - 'A'];
        if (f->owner != who)
            return;
        if (f->defence_fleet < s)
            return;
        f->defence_fleet -= s;
        Turn arrival = game->turn + Turn(std::ceil(Planet::dist(f, t)));
        AttackFleet fleet{who, s, t, arrival, f->ability};
        game->fleets.push_back(fleet);
    }

    void Controls::resign()
    {
        game->terminate();
        who->controller = nullptr;
    }


    Distance Planet::dist(Planet *f, Planet *t)
    {
        // it's not difficult to make this dimension-independent
        Distance dx = f->coords[0] - t->coords[0];
        Distance dy = f->coords[1] - t->coords[1];
        return std::hypot(dx, dy);
    }


    AttackFleet::AttackFleet(Player *o, FleetSize s, Planet *d, Turn t, Chance c)
    : owner(o), ships(s), destination(d), arrival(t), strength(c) {}


    GalaxyGame::GalaxyGame(Rules r, NewPlayers p)
    : rules(r), turn(), control_count()
    {
        PlayerID i = 0;
        for (auto& pair : p)
            players.push_back(Player(
                pair.first,
                i++,
                std::move(pair.second)
            ));
        tick();
    }

    void GalaxyGame::tick()
    {
        ++turn;
        std::cout << "It is now turn " << turn << std::endl;
        for (Player& p : players)
            if (p.controller)
                p.controller->turn(make_unique<Controls>(this, &p));
    }

    void GalaxyGame::terminate()
    {
        for (Player& p : players)
            p.controller = nullptr;
    }

} // namespace conquest
