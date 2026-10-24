#include "bot.h"
#include "core_lib_internal.h"
#include "tuned_params.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TRACKED_UNITS 1024
#define MAX_TRACKED_RES 256
#define MAX_GRID_DIM 64
#define MAX_PAIRS 512

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

/* Zero heap allocations: static buffers for tick loops */
static t_unit_brain g_brains[MAX_TRACKED_UNITS];
static bool g_brain_init = false;
static bool g_reserved_tiles[MAX_GRID_DIM][MAX_GRID_DIM];
static t_pair_score g_pairs[MAX_PAIRS];

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

static size_t count_obj_array(t_obj **arr)
{
	size_t count = 0;
	while (arr && arr[count] != NULL)
		count++;
	return (count);
}

static bool is_resource(const t_obj *obj)
{
	if (!obj || obj->hp == 0) return false;
	if (obj->type == OBJ_DEPOSIT) return true;
	if (obj->type == OBJ_GEM_PILE && obj->s_deposit_gems_pile.gems > 0) return true;
	return false;
}

static bool is_enemy_unit(const t_obj *obj)
{
	return (obj && obj->type == OBJ_UNIT && obj->s_unit.team_id != game.my_team_id && obj->hp > 0);
}

static t_unit_brain *get_brain(const t_obj *unit)
{
	if (!unit) return NULL;
	if (!g_brain_init)
	{
		memset(g_brains, 0, sizeof(g_brains));
		g_brain_init = true;
	}
	size_t idx = unit->id % MAX_TRACKED_UNITS;
	return (&g_brains[idx]);
}

static int count_enemies_within(t_pos pos, t_obj **enemies, int radius)
{
	int count = 0;
	for (size_t i = 0; enemies && enemies[i] != NULL; i++)
	{
		if (enemies[i]->hp > 0 && manhattan_dist(pos, enemies[i]->pos) <= radius)
			count++;
	}
	return (count);
}

/* ========================================================================== */
/* TILE RESERVATION & COLLISION-FREE SAFE TRAVEL                              */
/* ========================================================================== */

static void reset_tick_reservations(void)
{
	memset(g_reserved_tiles, 0, sizeof(g_reserved_tiles));
	/* All existing board objects occupy their current tile at tick start */
	for (size_t i = 0; game.objects && game.objects[i] != NULL; i++)
	{
		t_obj *o = game.objects[i];
		if (!o || o->hp == 0) continue;
		if (o->pos.x < MAX_GRID_DIM && o->pos.y < MAX_GRID_DIM)
		{
			g_reserved_tiles[o->pos.x][o->pos.y] = true;
		}
	}
}

static t_tile_travelability safe_travel_policy(t_pos pos, const t_obj *unit)
{
	if (pos.x < MAX_GRID_DIM && pos.y < MAX_GRID_DIM && g_reserved_tiles[pos.x][pos.y])
		return (t_tile_travelability){0, CORE_TRAVEL_BLOCK};

	t_obj *object = core_get_obj_from_pos(pos);
	if (!object)
	{
		/* If unit deals 0 core damage, do not step into cardinal tiles adjacent to enemy core */
		t_obj *enemy_core = ft_get_core_opponent();
		if (enemy_core && unit->s_unit.properties.damage_core == 0 &&
			manhattan_dist(pos, enemy_core->pos) <= 1)
		{
			return (t_tile_travelability){0, CORE_TRAVEL_BLOCK};
		}

		/* Check if empty tile is contested by a ready enemy unit */
		for (size_t i = 0; game.objects && game.objects[i] != NULL; i++)
		{
			t_obj *o = game.objects[i];
			if (o->type == OBJ_UNIT && o->s_unit.team_id != game.my_team_id && o->hp > 0)
			{
				if (o->s_unit.action_cooldown <= 0 && manhattan_dist(pos, o->pos) == 1)
					return (t_tile_travelability){0, CORE_TRAVEL_BLOCK};
			}
		}

		return (t_tile_travelability){1, CORE_TRAVEL_PASS};
	}

	/* Friendly objects cannot be traversed or attacked */
	if (object->type == OBJ_UNIT && object->s_unit.team_id == game.my_team_id)
		return (t_tile_travelability){0, CORE_TRAVEL_BLOCK};
	if (object->type == OBJ_CORE && object->s_core.team_id == game.my_team_id)
		return (t_tile_travelability){0, CORE_TRAVEL_BLOCK};

	/* Enemy Core */
	if (object->type == OBJ_CORE)
	{
		if (unit->s_unit.properties.damage_core > 0)
			return (t_tile_travelability){1, CORE_TRAVEL_ATTACK};
		return (t_tile_travelability){0, CORE_TRAVEL_BLOCK};
	}

	/* Gem Piles: collected via attack */
	if (object->type == OBJ_GEM_PILE)
	{
		return (t_tile_travelability){1, CORE_TRAVEL_ATTACK};
	}

	/* Obstacles: Deposit or Wall */
	if (object->type == OBJ_DEPOSIT || object->type == OBJ_WALL)
	{
		unsigned long dmg = unit->s_unit.properties.damage_object;
		if (dmg == 0) return (t_tile_travelability){0, CORE_TRAVEL_BLOCK};
		unsigned int attacks = (unsigned int)(object->hp / dmg + (object->hp % dmg != 0));
		return (t_tile_travelability){attacks + 1, CORE_TRAVEL_ATTACK};
	}

	/* Enemy Unit */
	if (object->type == OBJ_UNIT)
	{
		unsigned long dmg = unit->s_unit.properties.damage_unit;
		if (dmg == 0) return (t_tile_travelability){0, CORE_TRAVEL_BLOCK};
		unsigned int attacks = (unsigned int)(object->hp / dmg + (object->hp % dmg != 0));
		return (t_tile_travelability){attacks + 1, CORE_TRAVEL_ATTACK};
	}

	return (t_tile_travelability){1, CORE_TRAVEL_PASS};
}

static void safe_travel(t_obj *unit, t_pos dest)
{
	if (!unit || unit->s_unit.action_cooldown > 0) return;
	unsigned int prev_count = actions.count;
	core_action_travel(unit, dest, safe_travel_policy);
	if (actions.count > prev_count)
	{
		t_action *act = &actions.list[actions.count - 1];
		if (act->type == ACTION_MOVE)
		{
			t_pos target = act->data.move.pos;
			if (target.x < MAX_GRID_DIM && target.y < MAX_GRID_DIM)
			{
				if (g_reserved_tiles[target.x][target.y])
				{
					/* Collision: tile was reserved this tick! Cancel move to avoid server failure */
					actions.count--;
					if (unit->pos.x < MAX_GRID_DIM && unit->pos.y < MAX_GRID_DIM)
						g_reserved_tiles[unit->pos.x][unit->pos.y] = true;
				}
				else
				{
					g_reserved_tiles[target.x][target.y] = true;
				}
			}
		}
	}
}

/* ========================================================================== */
/* SUBSYSTEM 1: RESOURCE GATHERING & DYNAMIC MATCHING                         */
/* ========================================================================== */

static bool should_harvester_return(t_obj *m, t_obj *own_core, size_t res_count, t_obj **enemies)
{
	if (!m || !own_core) return false;
	unsigned long gems = m->s_unit.gems;
	unsigned long cap = m->s_unit.properties.max_gems;

	/* Safety First: if enemy is near, retreat to friendly Core immediately */
	if (count_enemies_within(m->pos, enemies, 4) > 0)
		return true;

	if (gems == 0) return false;
	if (cap > 0 && gems >= cap) return true;
	if (gems >= MINER_BASE_RETURN_GEMS) return true;
	if (res_count == 0 && gems > 0) return true;

	return false;
}

static bool try_adjacent_combat_strike(t_obj *attacker, t_obj **enemies, t_obj *enemy_core)
{
	if (!attacker || attacker->s_unit.action_cooldown > 0) return false;

	/* Priority 1: Destroy enemy core if adjacent and unit has damage_core */
	if (enemy_core && attacker->s_unit.properties.damage_core > 0 &&
		manhattan_dist(attacker->pos, enemy_core->pos) == 1)
	{
		core_action_attack(attacker, enemy_core);
		return true;
	}

	/* Priority 2: Attack adjacent enemy unit if unit has damage_unit */
	if (attacker->s_unit.properties.damage_unit > 0)
	{
		t_obj *target = NULL;
		int min_hp = 9999;
		for (size_t i = 0; enemies && enemies[i] != NULL; i++)
		{
			t_obj *en = enemies[i];
			if (en->hp > 0 && manhattan_dist(attacker->pos, en->pos) == 1)
			{
				if ((int)en->hp < min_hp)
				{
					min_hp = (int)en->hp;
					target = en;
				}
			}
		}
		if (target)
		{
			core_action_attack(attacker, target);
			return true;
		}
	}

	return false;
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

	/* Pass 1: Gem Transfer & Return-State Filtering */
	for (size_t i = 0; i < miner_count && i < MAX_TRACKED_UNITS; i++)
	{
		t_obj *m = miners[i];
		t_unit_brain *brain = get_brain(m);

		if (m->s_unit.action_cooldown > 0)
		{
			miner_handled[i] = true;
			continue;
		}

		if (own_core && m->s_unit.gems > 0 && manhattan_dist(m->pos, own_core->pos) <= 1)
		{
			core_action_transferGems(m, own_core->pos, m->s_unit.gems);
			if (brain) brain->state = STATE_GATHER;
			if (m->pos.x < MAX_GRID_DIM && m->pos.y < MAX_GRID_DIM)
				g_reserved_tiles[m->pos.x][m->pos.y] = true;
			miner_handled[i] = true;
			continue; /* Transfer immediately resets cooldown on server; do not move! */
		}

		if (should_harvester_return(m, own_core, res_count, enemies))
		{
			if (brain) brain->state = STATE_RETURN;
			if (own_core) safe_travel(m, own_core->pos);
			miner_handled[i] = true;
		}
		else
		{
			if (brain) brain->state = STATE_GATHER;
		}
	}

	/* Pass 2: Global Bipartite Cost-Matrix Assignment using Static Buffer */
	size_t pair_count = 0;
	for (size_t i = 0; i < miner_count && i < MAX_TRACKED_UNITS; i++)
	{
		if (miner_handled[i]) continue;
		t_obj *m = miners[i];
		for (size_t r = 0; r < res_count && r < MAX_TRACKED_RES && pair_count < MAX_PAIRS; r++)
		{
			t_obj *res = resources[r];
			int d_miner = manhattan_dist(m->pos, res->pos);
			int d_core = own_core ? manhattan_dist(res->pos, own_core->pos) : 10;
			int threat = count_enemies_within(res->pos, enemies, 3);

			float mult = (res->type == OBJ_GEM_PILE) ? WEIGHT_GEM_PILE_BONUS : 1.0f;
			float score = (WEIGHT_DEPOSIT_BASE * mult) - (WEIGHT_MINER_DIST_PENALTY * (float)d_miner) -
						  (WEIGHT_CORE_DIST_PENALTY * (float)d_core) -
						  (WEIGHT_ENEMY_THREAT_PENALTY * (float)threat);

			g_pairs[pair_count].miner_idx = i;
			g_pairs[pair_count].res_idx = r;
			g_pairs[pair_count].score = score;
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
			size_t mi = g_pairs[p].miner_idx;
			size_t ri = g_pairs[p].res_idx;
			if (!miner_handled[mi] && !res_claimed[ri] && g_pairs[p].score > best_val)
			{
				best_val = g_pairs[p].score;
				best_p = (int)p;
			}
		}

		if (best_p < 0) break;

		size_t win_m = g_pairs[best_p].miner_idx;
		size_t win_r = g_pairs[best_p].res_idx;
		miner_handled[win_m] = true;
		res_claimed[win_r] = true;

		t_obj *m = miners[win_m];
		t_obj *target_res = resources[win_r];

		if (manhattan_dist(m->pos, target_res->pos) == 1)
			core_action_attack(m, target_res);
		else
			safe_travel(m, target_res->pos);
	}

	/* Fallback: if more miners than resources, send remaining gatherers to nearest resource or Core */
	for (size_t i = 0; i < miner_count && i < MAX_TRACKED_UNITS; i++)
	{
		if (!miner_handled[i])
		{
			t_obj *m = miners[i];
			if (res_count > 0)
				safe_travel(m, resources[0]->pos);
			else if (own_core)
				safe_travel(m, own_core->pos);
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

	/* Count active enemy combat threats */
	size_t enemy_combat_units = 0;
	for (size_t e = 0; enemies && enemies[e] != NULL; e++)
	{
		if (enemies[e]->hp > 0 &&
			(enemies[e]->s_unit.properties.damage_unit > 0 || enemies[e]->s_unit.properties.damage_core > 0))
			enemy_combat_units++;
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
			if (f->pos.x < MAX_GRID_DIM && f->pos.y < MAX_GRID_DIM)
				g_reserved_tiles[f->pos.x][f->pos.y] = true;
			continue; /* Transfer immediately resets cooldown on server; do not move! */
		}

		if (f->s_unit.action_cooldown > 0) continue;

		/* 1. Pre-move guaranteed cardinal strike */
		if (try_adjacent_combat_strike(f, enemies, enemy_core)) continue;

		/* 2. Target Evaluation: Priority scoring */
		t_obj *best_enemy = NULL;
		int best_score = -999999;

		for (size_t e = 0; enemies && enemies[e] != NULL; e++)
		{
			t_obj *en = enemies[e];
			if (en->hp == 0) continue;
			int d_me = manhattan_dist(f->pos, en->pos);
			int d_core = own_core ? manhattan_dist(own_core->pos, en->pos) : 99;

			int score = -(d_me * SCORE_DIST_PENALTY) - (int)(en->hp * SCORE_HP_FOCUS_WEIGHT);

			/* Priority A: ENEMY CORE ATTACKERS! Highest threat on the board */
			if (en->s_unit.properties.damage_core > 0)
			{
				score += SCORE_CORE_ATTACKER_BONUS;
				score += (38 - d_core) * SCORE_CORE_THREAT_WEIGHT;
			}
			/* Priority B: Enemy combat units targeting our army */
			else if (en->s_unit.properties.damage_unit > 0)
			{
				score += 1500;
				score += (38 - d_core) * 100;
			}
			/* Priority C: Any enemy unit trespassing in our defensive half */
			else if (d_core <= DEFENSE_PANIC_RADIUS)
			{
				score += (38 - d_core) * (SCORE_CORE_THREAT_WEIGHT / 2);
			}
			/* Priority D: Enemy Harvesters in the field */
			else if (en->s_unit.properties.damage_object > 0)
			{
				score += SCORE_HUNT_MINER_BONUS;
				if (en->s_unit.gems > 0) score += SCORE_ENEMY_MINER_GEMS;
			}

			/* Priority E: Protecting friendly miners */
			for (size_t m = 0; m < miner_count; m++)
			{
				if (manhattan_dist(miners[m]->pos, en->pos) <= 3)
				{
					score += 1000;
					break;
				}
			}

			/* Priority F: Low HP focus fire */
			if (en->hp <= 3 && d_me <= 2) score += SCORE_LETHAL_BLOW_BONUS;

			if (score > best_score)
			{
				best_score = score;
				best_enemy = en;
			}
		}

		/*
		** Tactical Role Separation & Lanchester Rallying:
		** Fighter 0 is the dedicated DEFENDER (Goalie).
		** If outnumbered, hold anchor together to avoid feeding units one by one.
		*/
		bool is_dedicated_defender = (i == 0);

		if (best_enemy)
		{
			int d_best_core = own_core ? manhattan_dist(own_core->pos, best_enemy->pos) : 99;
			if (enemy_combat_units > fighter_count && d_best_core > DEFENSE_PANIC_RADIUS)
			{
				/* Outnumbered: Hold anchor position together and ambush approaching enemies */
				if (f->pos.x != anchor_pos.x || f->pos.y != anchor_pos.y)
					safe_travel(f, anchor_pos);
			}
			else if (is_dedicated_defender && d_best_core > DEFENSE_PANIC_RADIUS &&
				best_enemy->s_unit.properties.damage_core == 0)
			{
				/* Defender holds the anchor position */
				if (f->pos.x != anchor_pos.x || f->pos.y != anchor_pos.y)
					safe_travel(f, anchor_pos);
			}
			else
			{
				safe_travel(f, best_enemy->pos);
			}
		}
		else
		{
			/* No enemies visible */
			if (is_dedicated_defender)
			{
				if (f->pos.x != anchor_pos.x || f->pos.y != anchor_pos.y)
					safe_travel(f, anchor_pos);
			}
			else
			{
				/* Hunters advance toward enemy core to escort Breakers and clear defenders! */
				if (enemy_core)
				{
					t_pos escort_pos = enemy_core->pos;
					if (escort_pos.x >= 2) escort_pos.x -= 2;
					else escort_pos.x += 2;
					if (escort_pos.y >= 2) escort_pos.y -= 2;
					else escort_pos.y += 2;
					safe_travel(f, escort_pos);
				}
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
		{
			core_action_transferGems(b, own_core->pos, b->s_unit.gems);
			if (b->pos.x < MAX_GRID_DIM && b->pos.y < MAX_GRID_DIM)
				g_reserved_tiles[b->pos.x][b->pos.y] = true;
			continue; /* Transfer immediately resets cooldown on server; do not move! */
		}

		if (b->s_unit.action_cooldown > 0) continue;

		if (try_adjacent_combat_strike(b, enemies, enemy_core)) continue;

		if (enemy_core)
			safe_travel(b, enemy_core->pos);
	}
	if (breakers) free(breakers);
}

/* ========================================================================== */
/* SUBSYSTEM 3: DYNAMIC MACRO ECONOMY & COUNTER-SPAWNER                       */
/* ========================================================================== */

static void run_macro_spawner(t_obj *own_core, t_obj **enemies)
{
	if (!own_core || own_core->s_core.spawn_cooldown > 0) return;

	size_t miner_count = core_get_units_byName_count(HARVESTER_NAME);
	size_t fighter_count = core_get_units_byName_count(INTERCEPTOR_NAME);
	size_t breaker_count = core_get_units_byName_count(BREAKER_NAME);

	t_obj **resources = core_get_objs_filter(is_resource);
	size_t res_count = count_obj_array(resources);
	if (resources) free(resources);

	/* Count active enemy combat threats */
	size_t enemy_combat_units = 0;
	for (size_t e = 0; enemies && enemies[e] != NULL; e++)
	{
		if (enemies[e]->hp > 0 &&
			(enemies[e]->s_unit.properties.damage_unit > 0 || enemies[e]->s_unit.properties.damage_core > 0))
			enemy_combat_units++;
	}

	/* 1. STRONG DUAL-HARVESTER ECONOMY OPENING:
	** Initial 300 gems: build 2 Harvesters immediately to guarantee massive gem flow.
	*/
	if (res_count > 0 && miner_count < 2)
	{
		if (own_core->s_core.gems >= HARVESTER_COST)
			core_action_createUnit(HARVESTER_NAME, "mining", NULL);
		return;
	}

	/* 2. BASELINE DEFENSE: Ensure at least 2 Interceptors (Goalie + Vanguard) exist */
	if (fighter_count < 2)
	{
		if (own_core->s_core.gems >= INTERCEPTOR_COST)
			core_action_createUnit(INTERCEPTOR_NAME, "unit_attack", "health", NULL);
		return;
	}

	/* 3. ENSURE WIN CONDITION: At least 1 Siege Breaker must ALWAYS exist */
	if (breaker_count < 1)
	{
		if (own_core->s_core.gems >= BREAKER_COST)
			core_action_createUnit(BREAKER_NAME, "core_attack", "health", NULL);
		return;
	}

	/* 4. COMBAT DEFENSE / PARITY: If enemy combat units threaten us, match their army! */
	if (enemy_combat_units >= fighter_count)
	{
		if (own_core->s_core.gems >= INTERCEPTOR_COST)
			core_action_createUnit(INTERCEPTOR_NAME, "unit_attack", "health", NULL);
		return;
	}

	/* 5. 2nd Siege Breaker */
	if (breaker_count < 2)
	{
		if (own_core->s_core.gems >= BREAKER_COST)
			core_action_createUnit(BREAKER_NAME, "core_attack", "health", NULL);
		return;
	}

	/* 6. CONTINUOUS PRODUCTION: More Breakers or Interceptors */
	if (own_core->s_core.gems >= BREAKER_COST)
	{
		if (fighter_count <= enemy_combat_units + 1)
			core_action_createUnit(INTERCEPTOR_NAME, "unit_attack", "health", NULL);
		else
			core_action_createUnit(BREAKER_NAME, "core_attack", "health", NULL);
	}
}

/* ========================================================================== */
/* MAIN TICK CALLBACK                                                         */
/* ========================================================================== */

void ft_on_tick(unsigned long tick)
{
	(void)tick;
	reset_tick_reservations();

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
	return (core_startGame("ex_v01", argc, argv, ft_on_tick, false));
}
