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
	unsigned long unit_id;
	bool active;
	t_role_state state;
	unsigned long target_id;
	t_pos last_pos;
	t_pos flank_target;
	bool has_flank;
	unsigned int stuck_ticks;
	unsigned int yield_ticks;
} t_unit_brain;

typedef struct s_pair_score
{
	size_t miner_idx;
	size_t res_idx;
	float score;
} t_pair_score;

/* Zero heap allocations in per-tick loops: static state pool */
static t_unit_brain g_brains[MAX_TRACKED_UNITS];
static bool g_brain_init = false;
static bool g_reserved_tiles[MAX_GRID_DIM][MAX_GRID_DIM];
static bool s_enemy_danger_grid[MAX_GRID_DIM][MAX_GRID_DIM];
static t_pair_score g_pairs[MAX_PAIRS];
static bool g_push_initiated = false;

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
	if ((obj->type == OBJ_DEPOSIT || obj->type == OBJ_GEM_PILE) && obj->s_deposit_gems_pile.gems > 0)
		return true;
	return false;
}

static bool is_enemy_unit(const t_obj *obj)
{
	return (obj && obj->type == OBJ_UNIT && obj->s_unit.team_id != game.my_team_id && obj->hp > 0);
}

static bool is_contested_resource(const t_obj *res, t_obj **enemies)
{
	if (!res || res->type != OBJ_GEM_PILE) return false;
	for (size_t i = 0; enemies && enemies[i] != NULL; i++)
	{
		t_obj *en = enemies[i];
		if (en->hp > 0 && en->s_unit.action_cooldown <= 0 &&
			manhattan_dist(en->pos, res->pos) == 1)
			return true;
	}
	return false;
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
	t_unit_brain *b = &g_brains[idx];
	if (!b->active || b->unit_id != unit->id)
	{
		memset(b, 0, sizeof(*b));
		b->unit_id = unit->id;
		b->active = true;
	}
	return (b);
}

static int count_combat_enemies_within(t_pos pos, t_obj **enemies, int radius)
{
	int count = 0;
	for (size_t i = 0; enemies && enemies[i] != NULL; i++)
	{
		if (enemies[i]->hp > 0 &&
			(enemies[i]->s_unit.properties.damage_unit > 0 || enemies[i]->s_unit.properties.damage_core > 0) &&
			manhattan_dist(pos, enemies[i]->pos) <= radius)
			count++;
	}
	return (count);
}

/* Primary perimeter choke point anchored at (2,1) or (17,18) to lock horizontal entry */
static t_pos get_citadel_choke_point(const t_obj *own_core)
{
	t_pos anchor;
	if (own_core)
	{
		anchor.x = (own_core->pos.x == 0) ? 2 : (own_core->pos.x - 2);
		anchor.y = (own_core->pos.y == 0) ? 1 : (own_core->pos.y - 1);
	}
	else
	{
		anchor = (t_pos){2, 1};
	}
	return (anchor);
}

/* Secondary perimeter choke tile anchored at (1,2) or (18,17) to lock vertical entry */
static t_pos get_citadel_choke_wall(const t_obj *own_core)
{
	t_pos wall;
	if (own_core)
	{
		wall.x = (own_core->pos.x == 0) ? 1 : (own_core->pos.x - 1);
		wall.y = (own_core->pos.y == 0) ? 2 : (own_core->pos.y - 2);
	}
	else
	{
		wall = (t_pos){1, 2};
	}
	return (wall);
}

/* ========================================================================== */
/* TILE RESERVATION & COLLISION-FREE SAFE TRAVEL                              */
/* ========================================================================== */

static void reset_tick_reservations(void)
{
	memset(g_reserved_tiles, 0, sizeof(g_reserved_tiles));
	/* All existing allied objects occupy their current tile at tick start */
	for (size_t i = 0; game.objects && game.objects[i] != NULL; i++)
	{
		t_obj *o = game.objects[i];
		if (!o || o->hp == 0) continue;
		if ((o->type == OBJ_UNIT && o->s_unit.team_id == game.my_team_id) ||
			(o->type == OBJ_CORE && o->s_core.team_id == game.my_team_id))
		{
			if (o->pos.x < MAX_GRID_DIM && o->pos.y < MAX_GRID_DIM)
			{
				g_reserved_tiles[o->pos.x][o->pos.y] = true;
			}
		}
	}
}

static void update_enemy_danger_grid(t_obj **enemies)
{
	memset(s_enemy_danger_grid, 0, sizeof(s_enemy_danger_grid));
	if (!enemies) return;

	for (size_t i = 0; enemies[i] != NULL; i++)
	{
		t_obj *en = enemies[i];
		if (en->hp == 0 || en->s_unit.properties.damage_unit == 0) continue;
		int ex = en->pos.x;
		int ey = en->pos.y;
		if (ex > 0) s_enemy_danger_grid[ex - 1][ey] = true;
		if (ex + 1 < MAX_GRID_DIM) s_enemy_danger_grid[ex + 1][ey] = true;
		if (ey > 0) s_enemy_danger_grid[ex][ey - 1] = true;
		if (ey + 1 < MAX_GRID_DIM) s_enemy_danger_grid[ex][ey + 1] = true;
	}
}

static t_tile_travelability safe_travel_policy(t_pos pos, const t_obj *unit)
{
	if (pos.x < MAX_GRID_DIM && pos.y < MAX_GRID_DIM && g_reserved_tiles[pos.x][pos.y])
		return (t_tile_travelability){0, CORE_TRAVEL_BLOCK};

	t_obj *object = core_get_obj_from_pos(pos);
	if (!object || object->hp == 0)
	{
		/* Dynamic threat cost: If unit has no unit damage (Harvester / Breaker),
		** apply weight 5 to tiles adjacent to ready enemy combat units so Dijkstra
		** naturally routes around brawlers unless assaulting the enemy core.
		** Uses precomputed s_enemy_danger_grid for O(1) performance!
		*/
		if (unit->s_unit.properties.damage_unit == 0 && pos.x < MAX_GRID_DIM && pos.y < MAX_GRID_DIM)
		{
			t_obj *enemy_core = ft_get_core_opponent();
			if (!(enemy_core && manhattan_dist(pos, enemy_core->pos) <= 1 && unit->s_unit.properties.damage_core > 0))
			{
				if (s_enemy_danger_grid[pos.x][pos.y])
					return (t_tile_travelability){5, CORE_TRAVEL_PASS};
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
				bool collision_risk = false;

				/* Check 1: Tile already reserved by an allied unit this tick */
				if (g_reserved_tiles[target.x][target.y])
					collision_risk = true;

				/* Check 2: Contested midpoint tile race condition against advancing enemy */
				if (!collision_risk)
				{
					for (size_t i = 0; game.objects && game.objects[i] != NULL; i++)
					{
						t_obj *o = game.objects[i];
						if (o->type == OBJ_UNIT && o->s_unit.team_id != game.my_team_id && o->hp > 0)
						{
							/* If enemy is ready, adjacent to target, and unit+enemy are 2 steps apart:
							** They are advancing towards each other into the same midpoint tile!
							** Cancel move and hold current tile so enemy steps in without 50/50 race collision.
							*/
							if (o->s_unit.action_cooldown <= 0 &&
								manhattan_dist(target, o->pos) == 1 &&
								manhattan_dist(unit->pos, o->pos) == 2)
							{
								collision_risk = true;
								break;
							}
						}
					}
				}

				if (collision_risk)
				{
					/* Collision: tile reserved or contested! Cancel move; reserve current tile */
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
		else if (act->type == ACTION_ATTACK)
		{
			unsigned long tid = act->data.attack.target_id;
			for (size_t i = 0; game.objects && game.objects[i] != NULL; i++)
			{
				if (game.objects[i]->id == tid)
				{
					t_obj *target = game.objects[i];
					unsigned long dmg = 0;
					if (target->type == OBJ_UNIT)
						dmg = unit->s_unit.properties.damage_unit;
					else if (target->type == OBJ_CORE)
						dmg = unit->s_unit.properties.damage_core;
					else if (target->type == OBJ_DEPOSIT || target->type == OBJ_WALL)
						dmg = unit->s_unit.properties.damage_object;
					else if (target->type == OBJ_GEM_PILE)
						dmg = 1;

					if (target->hp <= dmg)
						target->hp = 0;
					else
						target->hp -= dmg;
					break;
				}
			}
		}
	}
}

/* ========================================================================== */
/* COMBAT RESOLUTION & PRE-MOVE CARDINAL STRIKE                               */
/* ========================================================================== */

static bool try_adjacent_combat_strike(t_obj *attacker, t_obj **enemies, t_obj *enemy_core, t_obj *own_core)
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
		int best_score = -999999;
		for (size_t i = 0; enemies && enemies[i] != NULL; i++)
		{
			t_obj *en = enemies[i];
			if (en->hp > 0 && manhattan_dist(attacker->pos, en->pos) == 1)
			{
				int score = 1000 - (int)en->hp;
				/* Maximum priority: enemy adjacent to our own Core! */
				if (own_core && manhattan_dist(en->pos, own_core->pos) <= 1)
					score += 20000;
				/* Extreme priority: enemy Breaker (damage_core > 0) */
				else if (en->s_unit.properties.damage_core > 0)
					score += 10000;

				if (score > best_score)
				{
					best_score = score;
					target = en;
				}
			}
		}
		if (target)
		{
			core_action_attack(attacker, target);
			unsigned long dmg = attacker->s_unit.properties.damage_unit;
			if (target->hp <= dmg)
				target->hp = 0;
			else
				target->hp -= dmg;
			return true;
		}
	}

	return false;
}

/* ========================================================================== */
/* PERIMETER ANTI-FLANK & EMERGENCY CORE DEFENSE HELPERS                     */
/* ========================================================================== */

/* Returns the highest-threat enemy unit adjacent to our Core (dist <= 1) */
static t_obj *get_core_invader(const t_obj *own_core, t_obj **enemies)
{
	if (!own_core || !enemies) return NULL;
	t_obj *best = NULL;
	int best_score = -999999;

	for (size_t i = 0; enemies[i] != NULL; i++)
	{
		t_obj *en = enemies[i];
		if (en->hp == 0) continue;
		if (manhattan_dist(en->pos, own_core->pos) <= 1)
		{
			int s = 1000 - (int)en->hp;
			if (en->s_unit.properties.damage_core > 0) s += 10000;
			if (s > best_score)
			{
				best_score = s;
				best = en;
			}
		}
	}
	return (best);
}

/* Returns enemy Breaker traversing perimeter within 4 tiles of Core */
static t_obj *get_perimeter_breaker(const t_obj *own_core, t_obj **enemies, size_t guardian_idx, t_obj **guardians, size_t guardian_count)
{
	if (!own_core || !enemies || !guardians) return NULL;
	t_obj *best = NULL;
	int best_dist = 999;

	for (size_t i = 0; enemies[i] != NULL; i++)
	{
		t_obj *en = enemies[i];
		if (en->hp == 0) continue;
		if (en->s_unit.properties.damage_core == 0) continue; /* Breakers only */

		int d_core = manhattan_dist(en->pos, own_core->pos);
		if (d_core > 4) continue; /* Strictly within 4 tiles of Core */

		/* Assign to whichever base guardian is closest to this Breaker */
		int d_me = manhattan_dist(guardians[guardian_idx]->pos, en->pos);
		bool my_responsibility = true;

		if (guardian_count >= 2)
		{
			size_t other_idx = (guardian_idx == 0) ? 1 : 0;
			int d_other = manhattan_dist(guardians[other_idx]->pos, en->pos);
			if (d_other < d_me)
				my_responsibility = false;
		}

		if (my_responsibility && d_core < best_dist)
		{
			best_dist = d_core;
			best = en;
		}
	}
	return (best);
}

/* Sorts guardians by proximity to Core so index 0 & 1 are always the true base defenders */
static void sort_guardians_by_core_dist(t_obj **guardians, size_t count, const t_obj *own_core)
{
	if (!guardians || !own_core || count <= 1) return;
	for (size_t i = 0; i < count - 1; i++)
	{
		for (size_t j = i + 1; j < count; j++)
		{
			int d_i = manhattan_dist(guardians[i]->pos, own_core->pos);
			int d_j = manhattan_dist(guardians[j]->pos, own_core->pos);
			if (d_j < d_i)
			{
				t_obj *tmp = guardians[i];
				guardians[i] = guardians[j];
				guardians[j] = tmp;
			}
		}
	}
}

/* ========================================================================== */
/* SUBSYSTEM 1: MACRO HARVESTERS & SAFE RETURN PATHING                        */
/* ========================================================================== */

static bool should_harvester_return(t_obj *m, t_obj *own_core, size_t res_count, t_obj **enemies)
{
	if (!m || !own_core) return false;
	unsigned long gems = m->s_unit.gems;
	unsigned long cap = m->s_unit.properties.max_gems;

	/* If carrying gems and under threat: emergency evacuation to deposit */
	if (gems > 0 && count_combat_enemies_within(m->pos, enemies, HARVESTER_EVACUATION_RADIUS) > 0)
		return true;

	if (gems == 0) return false;
	if (cap > 0 && gems >= cap) return true;
	if (gems >= MINER_BASE_RETURN_GEMS) return true;
	if (res_count == 0 && gems > 0) return true;

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
	t_pos anchor_pos = get_citadel_choke_point(own_core);

	/* Pass 1: Gem Transfer, Core Unblocking, Tactical Kiting & Return Filtering */
	for (size_t i = 0; i < miner_count && i < MAX_TRACKED_UNITS; i++)
	{
		t_obj *m = miners[i];
		t_unit_brain *brain = get_brain(m);

		if (m->s_unit.action_cooldown > 0)
		{
			miner_handled[i] = true;
			continue;
		}

		/* Safe Gem Transfer: adjacent to Core */
		if (own_core && m->s_unit.gems > 0 && manhattan_dist(m->pos, own_core->pos) <= 1)
		{
			core_action_transferGems(m, own_core->pos, m->s_unit.gems);
			if (brain) brain->state = STATE_GATHER;
			if (m->pos.x < MAX_GRID_DIM && m->pos.y < MAX_GRID_DIM)
				g_reserved_tiles[m->pos.x][m->pos.y] = true;
			miner_handled[i] = true;
			continue; /* Transfer consumes cooldown immediately; do not move on this tick! */
		}

		if (should_harvester_return(m, own_core, res_count, enemies))
		{
			if (brain) brain->state = STATE_RETURN;
			safe_travel(m, own_core->pos);
			miner_handled[i] = true;
			continue;
		}

		/* Immediate Tactical Kiting for Empty Harvesters:
		** If an enemy combat unit is in lethal range (dist <= 2),
		** kite back towards Core rear or hold behind the Core!
		*/
		if (m->s_unit.gems == 0 && own_core &&
			count_combat_enemies_within(m->pos, enemies, HARVESTER_DANGER_RADIUS) > 0)
		{
			if (manhattan_dist(m->pos, own_core->pos) > 1)
				safe_travel(m, own_core->pos);
			else if (m->pos.x < MAX_GRID_DIM && m->pos.y < MAX_GRID_DIM)
				g_reserved_tiles[m->pos.x][m->pos.y] = true;
			miner_handled[i] = true;
			continue;
		}

		if (brain) brain->state = STATE_GATHER;
	}

	/* Pass 2: Global Bipartite Cost-Matrix Assignment */
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
			int threat = count_combat_enemies_within(res->pos, enemies, 4);

			float mult = (res->type == OBJ_GEM_PILE) ? WEIGHT_GEM_PILE_BONUS : 1.0f;
			float score = (WEIGHT_DEPOSIT_BASE * mult) - (WEIGHT_MINER_DIST_PENALTY * (float)d_miner) -
						  (WEIGHT_CORE_DIST_PENALTY * (float)d_core) -
						  (WEIGHT_ENEMY_THREAT_PENALTY * (float)threat);
			if (is_contested_resource(res, enemies))
				score -= 600.0f;

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

		if (is_contested_resource(target_res, enemies))
		{
			/* Contested gem pile: ready enemy unit is already adjacent!
			** Yield to avoid 50/50 coin-flip action failure.
			*/
			if (m->pos.x < MAX_GRID_DIM && m->pos.y < MAX_GRID_DIM)
				g_reserved_tiles[m->pos.x][m->pos.y] = true;
		}
		else if (manhattan_dist(m->pos, target_res->pos) == 1)
		{
			core_action_attack(m, target_res);
			unsigned long dmg = m->s_unit.properties.damage_object;
			if (dmg == 0) dmg = 1;
			if (target_res->hp <= dmg)
				target_res->hp = 0;
			else
				target_res->hp -= dmg;
		}
		else
			safe_travel(m, target_res->pos);
	}

	/* Pass 3: Fallback for unhandled miners */
	for (size_t i = 0; i < miner_count && i < MAX_TRACKED_UNITS; i++)
	{
		if (!miner_handled[i])
		{
			t_obj *m = miners[i];
			if (m->s_unit.action_cooldown > 0) continue;
			if (res_count > 0 && resources[0]->hp > 0)
				safe_travel(m, resources[0]->pos);
			else
				safe_travel(m, anchor_pos);
		}
	}

	free(miners);
	if (resources) free(resources);
}

/* ========================================================================== */
/* SUBSYSTEM 2: ARMORED CHOKE DEFENSE & BODYGUARD GUARDIANS                   */
/* ========================================================================== */

static void run_guardians(t_obj *own_core, t_obj *enemy_core, t_obj **enemies,
						  t_obj **miners, bool is_pushing)
{
	t_obj **guardians = core_get_units_byName(GUARDIAN_NAME);
	t_obj **breakers = core_get_units_byName(BREAKER_NAME);
	size_t guardian_count = count_obj_array(guardians);
	size_t miner_count = count_obj_array(miners);

	if (guardian_count == 0)
	{
		if (guardians) free(guardians);
		if (breakers) free(breakers);
		return;
	}

	/* Sort guardians by Core proximity so indices 0 and 1 are always the true base defenders */
	sort_guardians_by_core_dist(guardians, guardian_count, own_core);

	t_pos anchor_pos = get_citadel_choke_point(own_core);
	t_pos wall_pos = get_citadel_choke_wall(own_core);

	/* Check if any enemy is within base defensive perimeter (8 tiles) */
	bool threat_near_base = false;
	for (size_t e = 0; enemies && enemies[e] != NULL; e++)
	{
		if (enemies[e]->hp > 0 && own_core &&
			manhattan_dist(enemies[e]->pos, own_core->pos) <= 8)
		{
			threat_near_base = true;
			break;
		}
	}

	/* Check if any allied harvester is actively threatened by raiders */
	bool miner_in_peril = false;
	for (size_t m = 0; m < miner_count; m++)
	{
		if (count_combat_enemies_within(miners[m]->pos, enemies, GUARDIAN_TETHER_RADIUS) > 0)
		{
			miner_in_peril = true;
			break;
		}
	}

	for (size_t i = 0; i < guardian_count; i++)
	{
		t_obj *g = guardians[i];

		if (g->s_unit.action_cooldown > 0) continue;

		/* Transfer gems if carrying any */
		if (own_core && g->s_unit.gems > 0 && manhattan_dist(g->pos, own_core->pos) <= 1)
		{
			core_action_transferGems(g, own_core->pos, g->s_unit.gems);
			if (g->pos.x < MAX_GRID_DIM && g->pos.y < MAX_GRID_DIM)
				g_reserved_tiles[g->pos.x][g->pos.y] = true;
			continue;
		}

		/* Pre-move cardinal strike: prioritizes core invaders & breakers */
		if (try_adjacent_combat_strike(g, enemies, enemy_core, own_core)) continue;

		/* MANDATE 1: EMERGENCY CORE DEFENSE
		** If any enemy unit is adjacent to our Core (dist <= 1),
		** ALL ready combat units within 3 tiles must attack it immediately!
		*/
		t_obj *invader = get_core_invader(own_core, enemies);
		if (invader)
		{
			int d_to_core = own_core ? manhattan_dist(g->pos, own_core->pos) : 99;
			int d_to_invader = manhattan_dist(g->pos, invader->pos);
			if (d_to_core <= 3 || d_to_invader <= 3)
			{
				if (d_to_invader == 1)
				{
					core_action_attack(g, invader);
					unsigned long dmg = g->s_unit.properties.damage_unit;
					if (invader->hp <= dmg)
						invader->hp = 0;
					else
						invader->hp -= dmg;
					continue;
				}
				else
				{
					safe_travel(g, invader->pos);
					continue;
				}
			}
		}

		/* MANDATE 2: PERIMETER ANTI-FLANK INTERCEPTION
		** If an enemy Breaker is detected traversing the outer perimeter within 4 tiles of our Core,
		** Guardian 0 or 1 must step out to intercept and eliminate that Breaker!
		*/
		if (i <= 1)
		{
			t_obj *flanker = get_perimeter_breaker(own_core, enemies, i, guardians, guardian_count);
			if (flanker)
			{
				int d_to_flanker = manhattan_dist(g->pos, flanker->pos);
				if (d_to_flanker == 1)
				{
					core_action_attack(g, flanker);
					unsigned long dmg = g->s_unit.properties.damage_unit;
					if (flanker->hp <= dmg)
						flanker->hp = 0;
					else
						flanker->hp -= dmg;
					continue;
				}
				else
				{
					safe_travel(g, flanker->pos);
					continue;
				}
			}
		}

		/* Threat Evaluation */
		t_obj *best_enemy = NULL;
		int best_score = -999999;

		for (size_t e = 0; enemies && enemies[e] != NULL; e++)
		{
			t_obj *en = enemies[e];
			if (en->hp == 0) continue;
			int d_me = manhattan_dist(g->pos, en->pos);
			int d_core = own_core ? manhattan_dist(own_core->pos, en->pos) : 99;

			int score = -(d_me * SCORE_DIST_PENALTY) - (int)(en->hp * SCORE_HP_FOCUS_WEIGHT);

			/* Priority 0: SNEAK BREAKER INTERCEPTOR
			** If an enemy Breaker approaches within 8 tiles of our Core,
			** intercept and destroy it immediately to safeguard the Citadel!
			*/
			if (en->s_unit.properties.damage_core > 0 && d_core <= 8)
			{
				score += SCORE_CORE_INTERCEPT_BONUS;
				score += (10 - d_core) * 2000;
			}

			/* Mandate 3: Bodyguard Micro - Protect miners from enemy raiders */
			int min_d_harvester = 999;
			for (size_t m = 0; m < miner_count; m++)
			{
				int d = manhattan_dist(miners[m]->pos, en->pos);
				if (d < min_d_harvester)
					min_d_harvester = d;
			}

			if (min_d_harvester <= GUARDIAN_TETHER_RADIUS && en->s_unit.properties.damage_unit > 0)
			{
				score += SCORE_BODYGUARD_MINER_BONUS;
				score += (GUARDIAN_TETHER_RADIUS + 1 - min_d_harvester) * 600;
			}

			/* Priority A: ENEMY CORE ATTACKERS! Top Citadel Threat */
			if (en->s_unit.properties.damage_core > 0)
			{
				score += SCORE_CORE_ATTACKER_BONUS;
				score += (38 - d_core) * SCORE_CORE_THREAT_WEIGHT;
			}
			/* Priority B: Enemy combat units */
			else if (en->s_unit.properties.damage_unit > 0)
			{
				score += 2000;
				score += (38 - d_core) * 150;
			}
			/* Priority C: Enemy trespassing in our defensive quadrant */
			if (d_core <= DEFENSE_PANIC_RADIUS)
			{
				score += (38 - d_core) * SCORE_CORE_THREAT_WEIGHT;
			}
			/* Priority D: Enemy Harvesters */
			if (en->s_unit.properties.damage_object > 0)
			{
				score += SCORE_HUNT_MINER_BONUS;
				if (en->s_unit.gems > 0) score += SCORE_ENEMY_MINER_GEMS;
			}
			/* Priority E: Low HP focus fire */
			if (en->hp <= 3 && d_me <= 2) score += SCORE_LETHAL_BLOW_BONUS;

			if (score > best_score)
			{
				best_score = score;
				best_enemy = en;
			}
		}

		/* Movement Logic: Citadel Double Choke (Guardian 0 & 1) vs Vanguard Strike Force (Guardian 2+) */
		int d_best_core = (best_enemy && own_core) ? manhattan_dist(own_core->pos, best_enemy->pos) : 99;
		int d_me_best = best_enemy ? manhattan_dist(g->pos, best_enemy->pos) : 99;
		t_pos my_station = (i == 0) ? anchor_pos : wall_pos;

		if (i == 0 || i == 1)
		{
			/* Guardian 0 & 1: Citadel Twin Choke Fortress (anchor_pos & wall_pos)
			** Strictly hold station. Only strike if enemy is adjacent (d_me_best <= 1),
			** breaches base perimeter (d_best_core <= 2), or threatens nearby miners (d_best_core <= 5 && d_me_best <= 2).
			*/
			if (best_enemy && (d_best_core <= 2 || d_me_best <= 1 || (d_best_core <= 5 && d_me_best <= 2)))
			{
				safe_travel(g, best_enemy->pos);
			}
			else
			{
				if (g->pos.x != my_station.x || g->pos.y != my_station.y)
					safe_travel(g, my_station);
			}
		}
		else
		{
			/* Guardian 2+: Vanguard Armored Strike Force */
			if (best_enemy && best_enemy->s_unit.properties.damage_core > 0 && d_best_core <= 8)
			{
				/* Emergency intercept: sneak breaker near Citadel! */
				safe_travel(g, best_enemy->pos);
			}
			else if (is_pushing)
			{
				/* Vanguard Escort Advance: Escort lead Breaker and crush enemy base defenders! */
				t_obj *lead_breaker = NULL;
				int best_d_enemy = 999;
				for (size_t b = 0; breakers && breakers[b] != NULL; b++)
				{
					if (breakers[b]->hp > 0 && enemy_core)
					{
						int d = manhattan_dist(breakers[b]->pos, enemy_core->pos);
						if (d < best_d_enemy)
						{
							best_d_enemy = d;
							lead_breaker = breakers[b];
						}
					}
				}

				if (best_enemy && d_me_best <= 4)
				{
					safe_travel(g, best_enemy->pos);
				}
				else if (lead_breaker && best_d_enemy > 2)
				{
					safe_travel(g, lead_breaker->pos);
				}
				else if (enemy_core)
				{
					safe_travel(g, enemy_core->pos);
				}
			}
			else
			{
				if (best_enemy && (d_best_core <= DEFENSE_PANIC_RADIUS || d_me_best <= 4))
				{
					safe_travel(g, best_enemy->pos);
				}
				else if (g->pos.x != my_station.x || g->pos.y != my_station.y)
				{
					safe_travel(g, my_station);
				}
			}
		}
	}

	free(guardians);
	if (breakers) free(breakers);
}

/* ========================================================================== */
/* SUBSYSTEM 3: SYNCHRONIZED SIEGE BREAKERS                                  */
/* ========================================================================== */

static void run_breakers(t_obj *own_core, t_obj *enemy_core, t_obj **enemies,
						 bool is_pushing, t_pos anchor_pos)
{
	t_obj **breakers = core_get_units_byName(BREAKER_NAME);
	if (!breakers) return;

	unsigned short max_c = (game.grid_size > 0) ? (game.grid_size - 1) : 19;

	for (size_t i = 0; breakers[i] != NULL; i++)
	{
		t_obj *b = breakers[i];

		if (b->s_unit.action_cooldown > 0) continue;

		if (own_core && b->s_unit.gems > 0 && manhattan_dist(b->pos, own_core->pos) <= 1)
		{
			core_action_transferGems(b, own_core->pos, b->s_unit.gems);
			if (b->pos.x < MAX_GRID_DIM && b->pos.y < MAX_GRID_DIM)
				g_reserved_tiles[b->pos.x][b->pos.y] = true;
			continue;
		}

		/* Pre-move cardinal strike against enemy core */
		if (try_adjacent_combat_strike(b, enemies, enemy_core, own_core)) continue;

		int d_own = own_core ? manhattan_dist(b->pos, own_core->pos) : 99;

		if (is_pushing || d_own > 5)
		{
			if (!enemy_core) continue;

			/* Flanking Siege Pathing: Outer edge corner waypoints bypass central brawler clumps */
			t_pos flank_a;
			t_pos flank_b;

			if (own_core && own_core->pos.x <= max_c / 2)
			{
				flank_a = (t_pos){max_c, 0};
				flank_b = (t_pos){0, max_c};
			}
			else
			{
				flank_a = (t_pos){0, max_c};
				flank_b = (t_pos){max_c, 0};
			}

			t_unit_brain *brain = get_brain(b);
			if (brain && !brain->has_flank)
			{
				int threat_a = count_combat_enemies_within(flank_a, enemies, 4);
				int threat_b = count_combat_enemies_within(flank_b, enemies, 4);
				if (i == 0)
					brain->flank_target = (threat_a <= threat_b) ? flank_a : flank_b;
				else if (i == 1)
					brain->flank_target = (threat_a <= threat_b) ? flank_b : flank_a;
				else
					brain->flank_target = (i % 2 == 0) ? flank_a : flank_b;
				brain->has_flank = true;
			}

			t_pos chosen_flank = (brain && brain->has_flank) ? brain->flank_target : flank_a;

			int d_to_flank = manhattan_dist(b->pos, chosen_flank);
			if (brain && brain->state != STATE_SIEGE && d_to_flank <= 2)
				brain->state = STATE_SIEGE;

			int d_to_core = manhattan_dist(b->pos, enemy_core->pos);
			if (d_to_core <= 4 || (brain && brain->state == STATE_SIEGE))
				safe_travel(b, enemy_core->pos);
			else
				safe_travel(b, chosen_flank);
		}
		else
		{
			/* Rally: Hold formation near choke point behind Guardian */
			if (manhattan_dist(b->pos, anchor_pos) > 1)
				safe_travel(b, anchor_pos);
		}
	}
	free(breakers);
}

/* ========================================================================== */
/* SUBSYSTEM 4: CITADEL MACRO-ECONOMY & TIMED SIEGE SPANWER                   */
/* ========================================================================== */

static void run_macro_spawner(t_obj *own_core, t_obj **enemies)
{
	if (!own_core || own_core->s_core.spawn_cooldown > 0) return;

	size_t miner_count = core_get_units_byName_count(HARVESTER_NAME);
	size_t guardian_count = core_get_units_byName_count(GUARDIAN_NAME);
	size_t breaker_count = core_get_units_byName_count(BREAKER_NAME);

	t_obj **resources = core_get_objs_filter(is_resource);
	size_t res_count = count_obj_array(resources);
	if (resources) free(resources);

	/* Threat detection */
	size_t enemy_combat_units = 0;
	size_t enemy_threats_near_base = 0;
	for (size_t e = 0; enemies && enemies[e] != NULL; e++)
	{
		if (enemies[e]->hp > 0 &&
			(enemies[e]->s_unit.properties.damage_unit > 0 || enemies[e]->s_unit.properties.damage_core > 0))
		{
			enemy_combat_units++;
			if (own_core && manhattan_dist(own_core->pos, enemies[e]->pos) <= DEFENSE_PANIC_RADIUS)
				enemy_threats_near_base++;
		}
	}

	/* Mandate 2: MINER AUTO-RECONSTITUTION
	** If miner workforce drops below MIN_SUSTAINED_HARVESTERS (2) while resources remain,
	** IMMEDIATELY spawn a Harvester (cost 35) before building expensive Breakers or Guardians.
	** Prevents the economic death spiral permanently!
	*/
	if (res_count > 0 && miner_count < MIN_SUSTAINED_HARVESTERS)
	{
		if (own_core->s_core.gems >= HARVESTER_COST)
			core_action_createUnit(HARVESTER_NAME, "mining", NULL);
		return;
	}

	/* Priority 1: Primary Armored Choke Anchor - First combat unit is an Armored Guardian */
	if (guardian_count < 1)
	{
		if (own_core->s_core.gems >= GUARDIAN_COST)
			core_action_createUnit(GUARDIAN_NAME, "unit_attack", "armor", "health", NULL);
		else if (res_count == 0 || enemy_threats_near_base > 0)
		{
			if (own_core->s_core.gems >= 47)
				core_action_createUnit(GUARDIAN_NAME, "unit_attack", "health", NULL);
			else if (own_core->s_core.gems >= 35)
				core_action_createUnit(GUARDIAN_NAME, "unit_attack", NULL);
		}
		return;
	}

	/* Priority 3: Citadel Armored Wall - Second Guardian locks the 2-tile choke! */
	if (guardian_count < 2)
	{
		if (own_core->s_core.gems >= GUARDIAN_COST)
			core_action_createUnit(GUARDIAN_NAME, "unit_attack", "armor", "health", NULL);
		else if (res_count == 0 || enemy_threats_near_base > 0)
		{
			if (own_core->s_core.gems >= 47)
				core_action_createUnit(GUARDIAN_NAME, "unit_attack", "health", NULL);
			else if (own_core->s_core.gems >= 35)
				core_action_createUnit(GUARDIAN_NAME, "unit_attack", NULL);
		}
		return;
	}

	/* Priority 4: Win Condition - Must ALWAYS maintain at least 1 Siege Breaker! */
	if (breaker_count < 1)
	{
		if (own_core->s_core.gems >= BREAKER_COST)
			core_action_createUnit(BREAKER_NAME, "core_attack", "health", NULL);
		else if (own_core->s_core.gems >= 35)
			core_action_createUnit(BREAKER_NAME, "core_attack", NULL);
		return;
	}

	/* Priority 5: Macro Economy Expansion - Complete 3rd Harvester once Citadel is locked! */
	if (res_count > 0 && miner_count < TARGET_HARVESTERS && enemy_threats_near_base == 0)
	{
		if (own_core->s_core.gems >= HARVESTER_COST)
			core_action_createUnit(HARVESTER_NAME, "mining", NULL);
		return;
	}

	/* Priority 6: Dual Flank Siege - Second Breaker */
	if (breaker_count < TARGET_BREAKERS)
	{
		if (own_core->s_core.gems >= BREAKER_COST)
			core_action_createUnit(BREAKER_NAME, "core_attack", "health", NULL);
		else if (own_core->s_core.gems >= 35)
			core_action_createUnit(BREAKER_NAME, "core_attack", NULL);
		return;
	}

	/* Priority 7: Combined-Arms Vanguard Escort (Maintain 1:1 Fighter-to-Breaker Ratio)
	** Mobile fighters escort Breakers to eliminate enemy hunters and base defenders!
	*/
	size_t mobile_guardians = (guardian_count >= 2) ? (guardian_count - 2) : 0;
	if (mobile_guardians < breaker_count)
	{
		if (own_core->s_core.gems >= GUARDIAN_COST)
			core_action_createUnit(GUARDIAN_NAME, "unit_attack", "armor", "health", NULL);
		else if (own_core->s_core.gems >= 47)
			core_action_createUnit(GUARDIAN_NAME, "unit_attack", "health", NULL);
		else if (own_core->s_core.gems >= 35)
			core_action_createUnit(GUARDIAN_NAME, "unit_attack", NULL);
		return;
	}

	/* Priority 8: Sustained Siege - Continuous Breaker production */
	if (own_core->s_core.gems >= BREAKER_COST)
	{
		core_action_createUnit(BREAKER_NAME, "core_attack", "health", NULL);
		return;
	}
	else if (own_core->s_core.gems >= 35)
	{
		core_action_createUnit(BREAKER_NAME, "core_attack", NULL);
		return;
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

	update_enemy_danger_grid(enemies);

	/* Synchronized Combined-Arms Timing Push:
	** Once 2+ combat units are ready, initiate the push!
	*/
	size_t guardian_count = core_get_units_byName_count(GUARDIAN_NAME);
	size_t breaker_count = core_get_units_byName_count(BREAKER_NAME);
	if ((guardian_count + breaker_count) >= COMBAT_READY_THRESHOLD)
		g_push_initiated = true;

	t_pos anchor_pos = get_citadel_choke_point(own_core);

	/* 1. Run Harvesters first so same-tick gem transfers credit the Core */
	run_harvesters(own_core, enemies);

	/* 2. Run Armored Guardians and Synchronized Breakers */
	run_guardians(own_core, enemy_core, enemies, miners, g_push_initiated);
	run_breakers(own_core, enemy_core, enemies, g_push_initiated, anchor_pos);

	/* 3. Execute Macro Spawner decisions */
	run_macro_spawner(own_core, enemies);

	if (miners) free(miners);
	if (enemies) free(enemies);
}

int main(int argc, char **argv)
{
	return (core_startGame("bot_beta", argc, argv, ft_on_tick, false));
}
