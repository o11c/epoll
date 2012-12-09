#ifndef CONQUEST_PLAYER_HPP
#define CONQUEST_PLAYER_HPP
// Copyright 2012 Ben Longbons
// GPL3+

#include "conquest.hpp"

namespace conquest
{
    class AsynchronousPlayer;

    class ClientPlanet
    {
        const Coord _coords;

        // optionals
        // TODO implement something à la boost::optional with my Variant?
        bool _has_owner;
        bool _has_ability;
        bool _has_ships;
        bool _has_production;
        PlayerID _owner;
        Chance _ability;
        FleetSize _ships;
        ProductionRate _production;

        Turn _last_conquest;
    public:
        ClientPlanet(Coord c)
        : _coords(c)
        , _has_owner(), _owner('?')
        , _has_ability(), _ability()
        , _has_ships(), _ships()
        , _has_production(), _production()
        , _last_conquest()
        {}

        const PlayerID *get_owner() const { return _has_owner ? &_owner : nullptr; }
        const Chance *get_ability() const { return _has_ability ? &_ability : nullptr; }
        const FleetSize *get_ships() const { return _has_ships ? &_ships : nullptr; }
        const ProductionRate *get_production() const { return _has_production ? &_production : nullptr; }

        void set_owner_(PlayerID *p, Turn t) { _has_owner = bool(p); _owner = p ? *p : '?'; _last_conquest = t; }
        void set_ability_(Chance *a) { _has_ability = bool(a); _owner = a ? *a : 0.0; }
        void set_ships_(FleetSize *s) { _has_ships = bool(s); _owner = s ? *s : 0; }
        void set_production_(ProductionRate *r) { _has_production = bool(r); _owner = r ? *r : 0; }

        void set_owner(PlayerID p, Turn t) { set_owner_(&p, t); }
        void set_ability(Chance a) { set_ability_(&a); }
        void set_ships(FleetSize s) { set_ships_(&s); }
        void set_production(ProductionRate r) { set_production_(&r); }
    };

    class AsyncController : public Controller
    {
        AsynchronousPlayer *kb;

        virtual void reset(Rules rules, PlayerID self_id) override;
        virtual void at(PlanetID planet, Coord coords) override;
        virtual void player(PlayerID id, std::string name) override;
        virtual void owned(PlanetID planet, PlayerID player) override;
        virtual void ability(PlanetID planet, Chance chance) override;
        virtual void produces(PlanetID planet, ProductionRate count) override;
        virtual void failed(PlanetID planet, PlayerID attacker);
        virtual void victory(PlayerID winner) override;

        virtual void turn(std::unique_ptr<Controls> controls) override;

    public:
        AsyncController(AsynchronousPlayer *);
        AsyncController(const AsyncController&) = delete;
        AsyncController& operator = (const AsyncController&) = delete;
    };

    struct LaunchedFleet
    {
        PlanetID dest;
        FleetSize ships;
        Chance ability;
        Turn arrival;
    };

    class AsynchronousPlayer
    {
        friend class AsyncController;

        std::unique_ptr<Controls> controls;

        AsynchronousPlayer(const AsynchronousPlayer&) = delete;
        AsynchronousPlayer& operator = (const AsynchronousPlayer&) = delete;
    protected:
        Rules rules;
        PlayerID self;
        std::map<PlayerID, std::string> names;
        Coord size;
        std::map<Coord, PlanetID> chart;
        std::map<PlanetID, ClientPlanet> planets;
        Turn current_turn;
        std::vector<LaunchedFleet> past_fleets;
        std::map<std::pair<PlanetID, PlanetID>, FleetSize> current_fleets;
        std::map<std::pair<PlanetID, PlanetID>, FleetSize> recurring_fleets;
    };
}

#endif // CONQUEST_PLAYER_HPP
