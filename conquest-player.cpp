// Copyright 2012 Ben Longbons
// GPL3+
#include "conquest-player.hpp"

namespace conquest
{
    void AsyncController::reset(Rules rules, PlayerID self_id)
    {
        kb->rules = rules;
        kb->size = rules.map_size;
        kb->self = self_id;

        kb->names.clear();
        kb->chart.clear();
        kb->planets.clear();
        kb->current_turn = 0;
        kb->past_fleets.clear();
        kb->current_fleets.clear();
        kb->recurring_fleets.clear();
    }

    void AsyncController::at(PlanetID planet, Coord coords)
    {
        kb->chart[coords] = planet;
        kb->planets.insert({planet, coords});
    }

    void AsyncController::player(PlayerID id, std::string name)
    {
        kb->names[id] = std::move(name);
    }

    void AsyncController::owned(PlanetID planet, PlayerID player)
    {
        kb->planets.at(planet).set_owner(player, kb->current_turn);
    }

    void AsyncController::ability(PlanetID planet, Chance chance)
    {
        kb->planets.at(planet).set_ability(chance);
    }

    void AsyncController::produces(PlanetID planet, ProductionRate count)
    {
        kb->planets.at(planet).set_production(count);
    }

    void AsyncController::failed(PlanetID planet, PlayerID attacker)
    {}

    void AsyncController::victory(PlayerID winner)
    {
        // kb->disconnect();
    }

    void AsyncController::turn(std::unique_ptr<Controls> controls)
    {
        kb->controls = std::move(controls);
        ++kb->current_turn;
        // copy
        kb->current_fleets = kb->recurring_fleets;
    }

    AsyncController::AsyncController(AsynchronousPlayer *pl)
    : kb(pl)
    {}
}
