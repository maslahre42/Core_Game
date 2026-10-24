#include "bot.h"
#include "tuned_params.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_TRACKED_UNITS 1024
#define MAX_TRACKED_RES 256

typedef enum e_role_state
{
	STATE_GATHER = 0,
	STATE_RETURN,
	STATE_RALLY,
	STATE_ENGAGE,
	STATE_SIEGE
} t_role_state;

typedef struct s_unit_brain
{
	t_role_state state;
	unsigned long target_id;
	t_pos last_pos;
	unsigned int stuck_ticks;
} t_unit_brain;

typedef struct s_pair_score
{
	size_t miner_idx;
	size_t res_idx;
	float score;
} t_pair_score;

static int manhattan_dist(t_pos a, t_pos b)
{
	int dx = (int)a.x - (int)b.x;
	int dy = (int)a.y - (int)b.y;
	if (dx < 0) dx = -dx;
	if (dy < 0) dy = -dy;
	return (dx + dy);
}

static bool is_resource(const t_obj *obj)
{
	return (obj && (obj->type == OBJ_GEM_PILE || obj->type == OBJ_DEPOSIT));
}

static bool is_enemy_unit(const t_obj *obj)
{
	return (obj && obj->type == OBJ_UNIT && obj->s_unit.team_id != game.my_team_id && obj->hp > 0);
}

static size_t count_obj_array(t_obj **arr)
{
	size_t n = 0;
	while (arr && arr[n] != NULL)
		n++;
	return (n);
}

static t_unit_brain g_unit_brains[MAX_TRACKED_UNITS];

static t_unit_brain *get_brain(t_obj *unit)
{
	if (!unit) return NULL;
	size_t idx = unit->id % MAX_TRACKED_UNITS;
	t_unit_brain *brain = &g_unit_brains[idx];

	if (unit->pos.x == brain->last_pos.x && unit->pos.y == brain->last_pos.y && unit->s_unit.action_cooldown == 0)
		brain->stuck_ticks++;
	else if (unit->pos.x != brain->last_pos.x || unit->pos.y != brain->last_pos.y)
	{
		brain->stuck_ticks = 0;
		brain->last_pos = unit->pos;
	}
	return (brain);
}

static int count_enemies_within(t_pos pos, t_obj **enemies, int radius)
{
	int count = 0;
	for (size_t i = 0; enemies && enemies[i] != NULL; i++)
	{
		if (manhattan_dist(pos, enemies[i]->pos) <= radius) count++;
	}
	return (count);
}

static bool should_harvester_return(t_obj *m, t_obj *own_core, size_t res_count, t_obj **enemies)
{
	unsigned int gems = m->s_unit.gems;
	if (gems == 0) return (false);
	if (res_count == 0) return (true);

	int d_core = own_core ? manhattan_dist(m->pos, own_core->pos) : 99;
	if (count_enemies_within(m->pos, enemies, 2) > 0 && gems >= 15) return (true);

	if (own_core && own_core->s_core.spawn_cooldown <= (unsigned int)d_core && own_core->s_core.gems < HARVESTER_COST &&
		(own_core->s_core.gems + gems) >= HARVESTER_COST && d_core <= 5)
	{
		return (true);
	}

	unsigned int step = (m->s_unit.properties.gems_per_cooldown_step > 0)
							? (unsigned int)m->s_unit.properties.gems_per_cooldown_step
							: DEFAULT_GEMS_PER_STEP;

	unsigned int optimal_cap = (step * 2 > 1) ? (step * 2 - 1) : MINER_BASE_RETURN_GEMS;
	if (optimal_cap > MINER_BASE_RETURN_GEMS) optimal_cap = MINER_BASE_RETURN_GEMS;

	return (gems >= optimal_cap);
}

static bool try_adjacent_combat_strike(t_obj *unit, t_obj **enemies, t_obj *enemy_core)
{
	if (unit->s_unit.action_cooldown != 0) return (false);

	if (enemy_core && unit->s_unit.properties.damage_core > 0 && manhattan_dist(unit->pos, enemy_core->pos) == 1)
	{
		core_action_attack(unit, enemy_core);
		return (true);
	}

	if (unit->s_unit.properties.damage_unit > 0)
	{
		t_obj *best_adj = NULL;
		unsigned int lowest_hp = 999999;

		for (size_t i = 0; enemies && enemies[i] != NULL; i++)
		{
			t_obj *en = enemies[i];
			if (manhattan_dist(unit->pos, en->pos) == 1)
			{
				if (en->hp < lowest_hp)
				{
					lowest_hp = en->hp;
					best_adj = en;
				}
			}
		}

		if (best_adj)
		{
			core_action_attack(unit, best_adj);
			return (true);
		}
	}
	return (false);
}

static void run_harvesters(t_obj *own_core, t_obj **enemies)
{
	t_obj **miners = core_get_units_byName(HARVESTER_NAME);
	t_obj **resources = core_get_objs_filter(is_resource);

	size_t miner_count = count_obj_array(miners);
	size_t res_count = count_obj_array(resources);

	if (miner_count == 0)
	{
		if (miners) free(miners);
		if (resources) free(resources);
		return;
	}

	bool miner_handled[MAX_TRACKED_UNITS] = {false};
	bool res_claimed[MAX_TRACKED_RES] = {false};

	for (size_t i = 0; i < miner_count && i < MAX_TRACKED_UNITS; i++)
	{
		t_obj *m = miners[i];
		t_unit_brain *brain = get_brain(m);

		if (m->s_unit.action_cooldown != 0)
		{
			miner_handled[i] = true;
			continue;
		}

		if (own_core && m->s_unit.gems > 0 && manhattan_dist(m->pos, own_core->pos) <= 1)
		{
			core_action_transferGems(m, own_core->pos, m->s_unit.gems);
			if (brain) brain->state = STATE_GATHER;
			miner_handled[i] = true;
			continue;
		}

		if (should_harvester_return(m, own_core, res_count, enemies))
		{
			if (brain) brain->state = STATE_RETURN;
			if (own_core) core_action_travel(m, own_core->pos, NULL);
			miner_handled[i] = true;
		}
		else
		{
			if (brain) brain->state = STATE_GATHER;
		}
	}

	size_t pair_cap = miner_count * res_count;
	t_pair_score *pairs = NULL;
	size_t pair_count = 0;

	if (pair_cap > 0)
		pairs = (t_pair_score *)malloc(sizeof(t_pair_score) * pair_cap);

	if (pairs)
	{
		for (size_t i = 0; i < miner_count && i < MAX_TRACKED_UNITS; i++)
		{
			if (miner_handled[i]) continue;
			t_obj *m = miners[i];
			for (size_t r = 0; r < res_count && r < MAX_TRACKED_RES; r++)
			{
				t_obj *res = resources[r];
				int d_miner = manhattan_dist(m->pos, res->pos);
				int d_core = own_core ? manhattan_dist(res->pos, own_core->pos) : 10;
				int threat = count_enemies_within(res->pos, enemies, 3);

				float mult = (res->type == OBJ_GEM_PILE) ? WEIGHT_GEM_PILE_BONUS : 1.0f;
				float score = (WEIGHT_DEPOSIT_BASE * mult) - (WEIGHT_MINER_DIST_PENALTY * (float)d_miner) -
							  (WEIGHT_CORE_DIST_PENALTY * (float)d_core) -
							  (WEIGHT_ENEMY_THREAT_PENALTY * (float)threat);

				pairs[pair_count].miner_idx = i;
				pairs[pair_count].res_idx = r;
				pairs[pair_count].score = score;
				pair_count++;
			}
		}

		for (size_t step = 0; step < miner_count; step++)
		{
			int best_p = -1;
			float best_val = -1e9f;

			for (size_t p = 0; p < pair_count; p++)
			{
				size_t mi = pairs[p].miner_idx;
				size_t ri = pairs[p].res_idx;
				if (!miner_handled[mi] && !res_claimed[ri] && pairs[p].score > best_val)
				{
					best_val = pairs[p].score;
					best_p = (int)p;
				}
			}

			if (best_p < 0) break;

			size_t win_m = pairs[best_p].miner_idx;
			size_t win_r = pairs[best_p].res_idx;
			miner_handled[win_m] = true;
			res_claimed[win_r] = true;

			t_obj *m = miners[win_m];
			t_obj *target_res = resources[win_r];

			if (target_res->type == OBJ_DEPOSIT && manhattan_dist(m->pos, target_res->pos) == 1)
				core_action_attack(m, target_res);
			else
				core_action_travel(m, target_res->pos, NULL);
		}
		free(pairs);
	}

	for (size_t i = 0; i < miner_count && i < MAX_TRACKED_UNITS; i++)
	{
		if (!miner_handled[i])
		{
			t_obj *m = miners[i];
			if (res_count > 0)
				core_action_travel(m, resources[0]->pos, NULL);
			else if (own_core)
				core_action_travel(m, own_core->pos, NULL);
		}
	}

	free(miners);
	if (resources) free(resources);
}

static void run_interceptors(t_obj *own_core, t_obj *enemy_core, t_obj **enemies, t_obj **miners)
{
	t_obj **fighters = core_get_units_byName(INTERCEPTOR_NAME);
	size_t fighter_count = count_obj_array(fighters);
	size_t miner_count = count_obj_array(miners);
	size_t enemy_count = count_obj_array(enemies);

	if (fighter_count == 0)
	{
		if (fighters) free(fighters);
		return;
	}

	for (size_t i = 0; i < fighter_count; i++)
	{
		t_obj *f = fighters[i];

		if (own_core && f->s_unit.gems > 0 && manhattan_dist(f->pos, own_core->pos) <= 1)
			core_action_transferGems(f, own_core->pos, f->s_unit.gems);

		if (f->s_unit.action_cooldown != 0) continue;

		if (try_adjacent_combat_strike(f, enemies, enemy_core)) continue;

		t_obj *best_enemy = NULL;
		int best_score = -999999;

		for (size_t e = 0; enemies && enemies[e] != NULL; e++)
		{
			t_obj *en = enemies[e];
			int d_me = manhattan_dist(f->pos, en->pos);
			int d_core = own_core ? manhattan_dist(own_core->pos, en->pos) : 99;

			int score = -(d_me * SCORE_DIST_PENALTY) - (int)(en->hp * SCORE_HP_FOCUS_WEIGHT);

			if (d_core <= DEFENSE_PANIC_RADIUS) score += SCORE_DEFENSE_URGENCY;
			if (en->s_unit.properties.damage_object > 0) score += SCORE_HUNT_MINER_BONUS;
			if (en->s_unit.gems > 0) score += SCORE_ENEMY_MINER_GEMS + (int)(en->s_unit.gems * 2);
			if (en->hp <= 25 && d_me <= 3) score += SCORE_LETHAL_BLOW_BONUS;

			for (size_t m = 0; m < miner_count; m++)
			{
				if (manhattan_dist(miners[m]->pos, en->pos) <= 2)
				{
					score += SCORE_PROTECT_MINER_BONUS;
					break;
				}
			}

			if (score > best_score)
			{
				best_score = score;
				best_enemy = en;
			}
		}

		if (best_enemy)
			core_action_travel(f, best_enemy->pos, NULL);
		else if (enemy_count == 0)
		{
			if (own_core && manhattan_dist(f->pos, own_core->pos) > 3)
				core_action_travel(f, own_core->pos, NULL);
		}
		else if (enemy_core)
			core_action_travel(f, enemy_core->pos, NULL);
	}

	free(fighters);
}

static void run_breakers(t_obj *own_core, t_obj *enemy_core, t_obj **enemies)
{
	t_obj **breakers = core_get_units_byName(BREAKER_NAME);
	for (size_t i = 0; breakers && breakers[i] != NULL; i++)
	{
		t_obj *b = breakers[i];
		if (own_core && b->s_unit.gems > 0 && manhattan_dist(b->pos, own_core->pos) <= 1)
			core_action_transferGems(b, own_core->pos, b->s_unit.gems);

		if (b->s_unit.action_cooldown != 0) continue;

		if (try_adjacent_combat_strike(b, enemies, enemy_core)) continue;

		if (enemy_core)
			core_action_travel(b, enemy_core->pos, NULL);
	}
	if (breakers) free(breakers);
}

static void run_macro_spawner(t_obj *own_core, t_obj **enemies)
{
	if (!own_core || own_core->s_core.spawn_cooldown != 0) return;

	size_t miner_count = core_get_units_byName_count(HARVESTER_NAME);
	size_t fighter_count = core_get_units_byName_count(INTERCEPTOR_NAME);
	size_t enemy_count = count_obj_array(enemies);

	t_obj **resources = core_get_objs_filter(is_resource);
	size_t res_count = count_obj_array(resources);
	if (resources) free(resources);

	bool emergency_defense = (count_enemies_within(own_core->pos, enemies, DEFENSE_PANIC_RADIUS) > 0);

	if (emergency_defense && fighter_count < enemy_count + 1)
	{
		if (own_core->s_core.gems >= INTERCEPTOR_COST)
			core_action_createUnit(INTERCEPTOR_NAME, "unit_attack", "health", NULL);
		return;
	}

	if (res_count > 0 && miner_count < TARGET_EARLY_MINERS)
	{
		if (own_core->s_core.gems >= HARVESTER_COST)
			core_action_createUnit(HARVESTER_NAME, "mining", "gem_carrying_efficiency", NULL);
		return;
	}

	if (fighter_count < 2)
	{
		if (own_core->s_core.gems >= INTERCEPTOR_COST)
			core_action_createUnit(INTERCEPTOR_NAME, "unit_attack", "health", NULL);
		return;
	}

	if (own_core->s_core.gems >= BREAKER_COST)
		core_action_createUnit(BREAKER_NAME, "core_attack", "health", NULL);
}

void ft_on_tick(unsigned long tick)
{
	(void)tick;
	t_obj *own_core = ft_get_core_own();
	t_obj *enemy_core = ft_get_core_opponent();
	t_obj **enemies = core_get_objs_filter(is_enemy_unit);
	t_obj **miners = core_get_units_byName(HARVESTER_NAME);

	run_harvesters(own_core, enemies);
	run_interceptors(own_core, enemy_core, enemies, miners);
	run_breakers(own_core, enemy_core, enemies);
	run_macro_spawner(own_core, enemies);

	if (miners) free(miners);
	if (enemies) free(enemies);
}

int main(int argc, char **argv)
{
	return (core_startGame("ULTIMATE_CORE", argc, argv, ft_on_tick, false));
}

