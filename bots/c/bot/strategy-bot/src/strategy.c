#include "strategy.h"

int strategy_production(t_strategy p, unsigned long bank, unsigned long cooldown,
    unsigned long tick, int miners, int guards, int resources, int threatened)
{
    static const unsigned long cost[] = {35, 47, 65, 35, 47};
    int design = p.attacker;

    if (cooldown) return WAIT;
    if (threatened && guards < p.guards) design = GUARD;
    else if (resources && tick < (unsigned long)p.economy_until && miners < p.miners) design = MINER;
    else if (guards < p.guards) design = GUARD;
    if (bank >= cost[design]) return design;
    /* Save for a specialist; cheaper attacker fallback only for attacker designs. */
    if (design <= ARMORED && bank >= cost[LIGHT]) return LIGHT;
    return WAIT;
}

double strategy_resource_score(t_strategy p, int distance, int home_distance,
    int is_pile, unsigned long hp)
{
    return distance + 0.25 * home_distance + (is_pile ? -p.pile_bonus : (double)hp);
}

double strategy_guard_score(t_strategy p, int distance, int home_distance)
{
    return distance + p.home_weight * home_distance;
}
