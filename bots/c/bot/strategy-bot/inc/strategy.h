#ifndef STRATEGY_H
#define STRATEGY_H

enum { LIGHT, HEALTHY, ARMORED, MINER, GUARD, WAIT };
typedef struct s_strategy {
    int miners, guards, attacker, return_load, economy_until;
    int threat_radius, pile_bonus, home_weight;
} t_strategy;

int strategy_production(t_strategy p, unsigned long bank, unsigned long cooldown,
    unsigned long tick, int miners, int guards, int resources, int threatened);
double strategy_resource_score(t_strategy p, int distance, int home_distance,
    int is_pile, unsigned long hp);
double strategy_guard_score(t_strategy p, int distance, int home_distance);
void strategy_tick(unsigned long tick);

#endif
