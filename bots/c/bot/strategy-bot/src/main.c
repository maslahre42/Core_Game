#include "core_lib.h"
#include "strategy.h"
#include "strategy_params.h"
#include <float.h>

static int dist(t_pos a, t_pos b)
{
    return abs((int)a.x - b.x) + abs((int)a.y - b.y);
}

static int owned(const t_obj *o)
{
    return o->type == OBJ_UNIT && o->s_unit.team_id == game.my_team_id && o->hp > 0;
}

static int resource(const t_obj *o)
{
    return (o->type == OBJ_DEPOSIT || o->type == OBJ_GEM_PILE) && o->hp > 0 &&
        o->s_deposit_gems_pile.gems > 0;
}

static void spawn(int design)
{
    if (design == LIGHT) core_action_createUnit("StrategyLight", "core_attack", NULL);
    else if (design == HEALTHY) core_action_createUnit("StrategyHealthy", "core_attack", "health", NULL);
    else if (design == ARMORED) core_action_createUnit("StrategyArmored", "core_attack", "armor", NULL);
    else if (design == MINER) core_action_createUnit("StrategyMiner", "mining", NULL);
    else if (design == GUARD) core_action_createUnit("StrategyGuard", "unit_attack", "health", NULL);
}

void strategy_tick(unsigned long tick)
{
    t_obj *home = NULL, *enemy = NULL, *o;
    int miners = 0, guards = 0, resources = 0, threatened = 0;
    size_t i, j;

    if (!game.objects) return;
    for (i = 0; game.objects[i]; i++) {
        o = game.objects[i];
        if (o->type == OBJ_CORE) {
            if (o->s_core.team_id == game.my_team_id) home = o;
            else enemy = o;
        }
        if (resource(o)) resources++;
        if (owned(o) && o->s_unit.properties.damage_object > 0) miners++;
        if (owned(o) && o->s_unit.properties.damage_unit > 0) guards++;
    }
    if (!home || !enemy || !home->hp || !enemy->hp) return;
    for (i = 0; game.objects[i]; i++) {
        o = game.objects[i];
        if (o->type == OBJ_UNIT && o->hp && o->s_unit.team_id != game.my_team_id &&
            dist(o->pos, home->pos) <= strategy_params.threat_radius) threatened = 1;
    }
    spawn(strategy_production(strategy_params, home->s_core.gems, home->s_core.spawn_cooldown,
        tick, miners, guards, resources, threatened));
    for (i = 0; game.objects[i]; i++) {
        t_obj *unit = game.objects[i], *target = NULL;
        double best = DBL_MAX;
        if (!owned(unit) || unit->s_unit.action_cooldown > 0) continue;
        /* Handwritten executor. Search tunes quotas, resource and combat priorities. */
        if (unit->s_unit.properties.damage_object > 0) {
            for (j = 0; game.objects[j]; j++) {
                double score;
                o = game.objects[j];
                if (!resource(o)) continue;
                score = strategy_resource_score(strategy_params, dist(unit->pos, o->pos),
                    dist(home->pos, o->pos), o->type == OBJ_GEM_PILE, o->hp);
                if (score < best || (score == best && target && o->id < target->id)) {
                    best = score; target = o;
                }
            }
            if (unit->s_unit.gems && (unit->s_unit.gems >= (unsigned long)strategy_params.return_load ||
                unit->s_unit.gems >= unit->s_unit.properties.max_gems || !target)) {
                if (dist(unit->pos, home->pos) == 1)
                    core_action_transferGems(unit, home->pos, unit->s_unit.gems);
                else core_action_travel(unit, home->pos, NULL);
            } else if (target) core_action_travel(unit, target->pos, NULL);
            else core_action_travel(unit, enemy->pos, NULL);
        } else if (unit->s_unit.properties.damage_unit > 0) {
            for (j = 0; game.objects[j]; j++) {
                double score;
                o = game.objects[j];
                if (o->type != OBJ_UNIT || !o->hp || o->s_unit.team_id == game.my_team_id) continue;
                score = strategy_guard_score(strategy_params, dist(unit->pos, o->pos), dist(home->pos, o->pos));
                if (score < best || (score == best && target && o->id < target->id)) {
                    best = score; target = o;
                }
            }
            core_action_travel(unit, target ? target->pos : enemy->pos, NULL);
        } else core_action_travel(unit, enemy->pos, NULL);
    }
}

#ifndef STRATEGY_TEST
int main(int argc, char **argv)
{
    printf("Strategy policy: %s\n", STRATEGY_MODEL_SHA256);
    return core_startGame("CORE Strategy", argc, argv, strategy_tick, false);
}
#endif
