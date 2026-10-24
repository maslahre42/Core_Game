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

/* ========================================================================== */
/* UTILITY & GEOMETRY HELPERS                                                 */
/* ========================================================================== */

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

/* Static leak-free per-unit memory pool */
static t_unit_brain g_unit_brains[MAX_TRACKED_UNITS];

static t_unit_brain *get_brain(t_obj *unit)
{
	if (!unit) return NULL;
	size_t idx = unit->id % MAX_TRACKED_UNITS;
	t_unit_brain *brain = &g_unit_brains[idx];

	if (unit->pos.x == brain->last_pos.x && unit->pos.y == brain->last_pos.y && unit->s_unit.action_cooldown == 0)
	{
		brain->stuck_ticks++;
	}
	else if (unit->pos.x != brain->last_pos.x || unit->pos.y != brain->last_pos.y)
	{
		brain->stuck_ticks = 0;
		brain->last_pos = unit->pos;
	}
	return (brain);
}

/* Computes local enemy threat near a position */
static int count_enemies_within(t_pos pos, t_obj **enemies, int radius)
{
	int count = 0;
	for (size_t i = 0; enemies && enemies[i] != NULL; i++)
	{
		if (manhattan_dist(pos, enemies[i]->pos) <= radius) count++;
	}
	return (count);
}

/* ========================================================================== */
/* EXACT STEP-COOLDOWN & ADJACENCY MICRO                                      */
/* ========================================================================== */

static bool should_harvester_return(t_obj *m, t_obj *own_core, size_t res_count, t_obj **enemies)
{
	unsigned int gems = m->s_unit.gems;
	if (gems == 0) return (false);
	if (res_count == 0) return (true);

	int d_core = own_core ? manhattan_dist(m->pos, own_core->pos) : 99;

	/* Emergency retreat with gems if an enemy fighter is closing in */
	if (count_enemies_within(m->pos, enemies, 3) > 0 && gems >= 15) return (true);

	/* Fast economy injection: if close to Core and our gems complete an Interceptor or Breaker */
	if (own_core && own_core->s_core.spawn_cooldown <= (unsigned int)d_core &&
		(own_core->s_core.gems + gems) >= INTERCEPTOR_COST && d_core <= 6)
	{
		return (true);
	}

	return (gems >= MINER_BASE_RETURN_GEMS);
}

/*
** Exploits Server Phase 3 (attack) executing BEFORE Phase 4 (move).
** If an enemy or targetable object is at Manhattan distance 1 and our
** action_cooldown == 0, attack immediately for a guaranteed hit.
*/
static bool try_adjacent_combat_strike(t_obj *unit, t_obj **enemies, t_obj *enemy_core)
{
	if (unit->s_unit.action_cooldown != 0) return (false);

	/* 1. Prioritize enemy Core ONLY IF this unit has damage_core > 0 */
	if (enemy_core && unit->s_unit.properties.damage_core > 0 && manhattan_dist(unit->pos, enemy_core->pos) == 1)
	{
		core_action_attack(unit, enemy_core);
		return (true);
	}

	/* 2. Find lowest-HP cardinally adjacent enemy unit if unit has damage_unit > 0 */
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

/* ========================================================================== */
/* SUBSYSTEM 1: GLOBAL BIPARTITE HARVESTER ALLOCATOR                          */
/* ========================================================================== */

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

	/* Pass 1: Gem Transfer & Return-State Filtering */
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

	/* Pass 2: Global Bipartite Cost-Matrix Assignment for Gathering Miners */
	size_t pair_cap = miner_count * res_count;
	t_pair_score *pairs = NULL;
	size_t pair_count = 0;

	if (pair_cap > 0)
	{
		pairs = (t_pair_score *)malloc(sizeof(t_pair_score) * pair_cap);
	}

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

		/* Greedily lock the highest-scoring (Miner, Resource) pairs globally */
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

	/* Fallback: if more miners than resources, send remaining gatherers to nearest resource or Core */
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

/* ========================================================================== */
/* SUBSYSTEM 2: LANCHESTER SQUADRON & TACTICAL INTERCEPTORS                   */
/* ========================================================================== */

static void run_interceptors(t_obj *own_core, t_obj *enemy_core, t_obj **enemies, t_obj **miners)
{
	t_obj **fighters = core_get_units_byName(INTERCEPTOR_NAME);
	size_t fighter_count = count_obj_array(fighters);
	size_t miner_count = count_obj_array(miners);

	if (fighter_count == 0)
	{
		if (fighters) free(fighters);
		return;
	}

	/* Defensive Anchor Point: Choke position between our Core and map center */
	t_pos anchor_pos;
	if (own_core)
	{
		anchor_pos.x = (own_core->pos.x == 0) ? 2 : (own_core->pos.x - 2);
		anchor_pos.y = (own_core->pos.y == 0) ? 2 : (own_core->pos.y - 2);
	}
	else
	{
		anchor_pos = (t_pos){2, 2};
	}

	for (size_t i = 0; i < fighter_count; i++)
	{
		t_obj *f = fighters[i];

		if (own_core && f->s_unit.gems > 0 && manhattan_dist(f->pos, own_core->pos) <= 1)
		{
			core_action_transferGems(f, own_core->pos, f->s_unit.gems);
		}

		if (f->s_unit.action_cooldown != 0) continue;

		/* 1. Pre-move guaranteed cardinal strike */
		if (try_adjacent_combat_strike(f, enemies, enemy_core)) continue;

		/* 2. Target Evaluation: Priority scoring */
		t_obj *best_enemy = NULL;
		int best_score = -999999;

		for (size_t e = 0; enemies && enemies[e] != NULL; e++)
		{
			t_obj *en = enemies[e];
			int d_me = manhattan_dist(f->pos, en->pos);
			int d_core = own_core ? manhattan_dist(own_core->pos, en->pos) : 99;

			int score = -(d_me * SCORE_DIST_PENALTY) - (int)(en->hp * SCORE_HP_FOCUS_WEIGHT);

			/* Priority A: ENEMY CORE ATTACKERS! Highest threat on the board */
			if (en->s_unit.properties.damage_core > 0)
			{
				score += SCORE_CORE_ATTACKER_BONUS;
				score += (38 - d_core) * SCORE_CORE_THREAT_WEIGHT;
			}
			/* Priority B: Any enemy unit trespassing in our defensive half */
			else if (d_core <= DEFENSE_PANIC_RADIUS)
			{
				score += (38 - d_core) * (SCORE_CORE_THREAT_WEIGHT / 2);
			}
			/* Priority C: Enemy Harvesters in the field */
			else if (en->s_unit.properties.damage_object > 0)
			{
				score += SCORE_HUNT_MINER_BONUS;
				if (en->s_unit.gems > 0) score += SCORE_ENEMY_MINER_GEMS;
			}

			/* Priority D: Protecting friendly miners */
			for (size_t m = 0; m < miner_count; m++)
			{
				if (manhattan_dist(miners[m]->pos, en->pos) <= 2)
				{
					score += SCORE_PROTECT_MINER_BONUS;
					break;
				}
			}

			/* Priority E: Low HP focus fire */
			if (en->hp <= 3 && d_me <= 2) score += SCORE_LETHAL_BLOW_BONUS;

			if (score > best_score)
			{
				best_score = score;
				best_enemy = en;
			}
		}

		/*
		** Tactical Role Separation:
		** Fighter 0 is the dedicated DEFENDER (Goalie).
		** If no enemy is threatening our half, Fighter 0 holds the anchor choke point.
		*/
		bool is_dedicated_defender = (i == 0);

		if (best_enemy)
		{
			int d_best_core = own_core ? manhattan_dist(own_core->pos, best_enemy->pos) : 99;
			if (is_dedicated_defender && d_best_core > DEFENSE_PANIC_RADIUS && best_enemy->s_unit.properties.damage_core == 0)
			{
				/* Enemy is far away: Defender holds the anchor position */
				if (f->pos.x != anchor_pos.x || f->pos.y != anchor_pos.y)
					core_action_travel(f, anchor_pos, NULL);
			}
			else
			{
				/* Intercept and destroy the target */
				core_action_travel(f, best_enemy->pos, NULL);
			}
		}
		else
		{
			/* No enemies visible */
			if (is_dedicated_defender)
			{
				if (f->pos.x != anchor_pos.x || f->pos.y != anchor_pos.y)
					core_action_travel(f, anchor_pos, NULL);
			}
			else if (enemy_core)
			{
				/* Hunters advance to map center / scout towards enemy core */
				t_pos mid_pos = {(unsigned short)(game.grid_size / 2), (unsigned short)(game.grid_size / 2)};
				core_action_travel(f, mid_pos, NULL);
			}
		}
	}

	free(fighters);
}

/* Dedicated Core Breakers: focus on melting enemy Core */
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

/* ========================================================================== */
/* SUBSYSTEM 3: DYNAMIC MACRO ECONOMY & COUNTER-SPAWNER                       */
/* ========================================================================== */

static void run_macro_spawner(t_obj *own_core, t_obj **enemies)
{
	if (!own_core || own_core->s_core.spawn_cooldown != 0) return;

	size_t miner_count = core_get_units_byName_count(HARVESTER_NAME);
	size_t fighter_count = core_get_units_byName_count(INTERCEPTOR_NAME);
	size_t breaker_count = core_get_units_byName_count(BREAKER_NAME);

	t_obj **resources = core_get_objs_filter(is_resource);
	size_t res_count = count_obj_array(resources);
	if (resources) free(resources);

	int enemy_threats_near_core = 0;
	for (size_t i = 0; enemies && enemies[i] != NULL; i++)
	{
		if (manhattan_dist(enemies[i]->pos, own_core->pos) <= DEFENSE_PANIC_RADIUS)
			enemy_threats_near_core++;
	}

	/* Urgent Counter: if enemies are approaching our Core and we need more defenders */
	if (enemy_threats_near_core > (int)fighter_count)
	{
		if (own_core->s_core.gems >= INTERCEPTOR_COST)
			core_action_createUnit(INTERCEPTOR_NAME, "unit_attack", "health", NULL);
		return;
	}

	/* Build Order Phase 1: 1st Harvester (fast 35-gem pure miner) */
	if (res_count > 0 && miner_count < 1)
	{
		if (own_core->s_core.gems >= HARVESTER_COST)
			core_action_createUnit(HARVESTER_NAME, "mining", NULL);
		return;
	}

	/* Build Order Phase 2: 1st Interceptor (Dedicated Core Defender / Goalie) */
	if (fighter_count < 1)
	{
		if (own_core->s_core.gems >= INTERCEPTOR_COST)
			core_action_createUnit(INTERCEPTOR_NAME, "unit_attack", "health", NULL);
		return;
	}

	/* Build Order Phase 3: 2nd Interceptor (Hunter / Forward Screen) */
	if (fighter_count < 2)
	{
		if (own_core->s_core.gems >= INTERCEPTOR_COST)
			core_action_createUnit(INTERCEPTOR_NAME, "unit_attack", "health", NULL);
		return;
	}

	/* Build Order Phase 4: Siege Breakers */
	if (breaker_count < 3)
	{
		if (own_core->s_core.gems >= BREAKER_COST)
			core_action_createUnit(BREAKER_NAME, "core_attack", "health", NULL);
		return;
	}

	/* If economy is rich and we have extra gems, add 2nd miner if deposits remain */
	if (res_count > 0 && miner_count < MAX_MINERS && own_core->s_core.gems >= 100)
	{
		core_action_createUnit(HARVESTER_NAME, "mining", NULL);
		return;
	}

	/* Excess gems: spawn additional Breakers */
	if (own_core->s_core.gems >= BREAKER_COST)
	{
		core_action_createUnit(BREAKER_NAME, "core_attack", "health", NULL);
	}
}

/* ========================================================================== */
/* MAIN TICK CALLBACK                                                         */
/* ========================================================================== */

void ft_on_tick(unsigned long tick)
{
	(void)tick;
	t_obj *own_core = ft_get_core_own();
	t_obj *enemy_core = ft_get_core_opponent();
	t_obj **enemies = core_get_objs_filter(is_enemy_unit);
	t_obj **miners = core_get_units_byName(HARVESTER_NAME);

	/* 1. Run Harvesters first so same-tick gem transfers credit the Core */
	run_harvesters(own_core, enemies);

	/* 2. Run Interceptors and Siege Breakers */
	run_interceptors(own_core, enemy_core, enemies, miners);
	run_breakers(own_core, enemy_core, enemies);

	/* 3. Execute Core Spawning decisions */
	run_macro_spawner(own_core, enemies);

	if (miners) free(miners);
	if (enemies) free(enemies);
}

int main(int argc, char **argv)
{
	return (core_startGame("ULTIMATE_CORE", argc, argv, ft_on_tick, false));
}
