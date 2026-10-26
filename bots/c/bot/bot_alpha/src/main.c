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
	t_pos flank_target;
	bool has_flank;
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
static unsigned long g_pending_target_dmg[MAX_TRACKED_UNITS];
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

static int count_combat_enemies_within(t_pos pos, t_obj **enemies, int radius)
{
	int count = 0;
	for (size_t i = 0; enemies && enemies[i] != NULL; i++)
	{
		if (enemies[i]->hp > 0 && enemies[i]->s_unit.properties.damage_unit > 0 &&
			manhattan_dist(pos, enemies[i]->pos) <= radius)
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
	memset(g_pending_target_dmg, 0, sizeof(g_pending_target_dmg));
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

	/* Gem Piles: collected via attack by Harvesters only */
	if (object->type == OBJ_GEM_PILE)
	{
		if (unit->s_unit.properties.damage_object > 0)
			return (t_tile_travelability){1, CORE_TRAVEL_ATTACK};
		return (t_tile_travelability){0, CORE_TRAVEL_BLOCK};
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
		size_t o_idx = object->id % MAX_TRACKED_UNITS;
		if (object->hp <= g_pending_target_dmg[o_idx])
			return (t_tile_travelability){0, CORE_TRAVEL_BLOCK};
		unsigned int attacks = (unsigned int)(object->hp / dmg + (object->hp % dmg != 0));
		return (t_tile_travelability){attacks + 1, CORE_TRAVEL_ATTACK};
	}

	return (t_tile_travelability){1, CORE_TRAVEL_PASS};
}

static t_tile_travelability breaker_travel_policy(t_pos pos, const t_obj *unit)
{
	if (pos.x < MAX_GRID_DIM && pos.y < MAX_GRID_DIM && g_reserved_tiles[pos.x][pos.y])
		return (t_tile_travelability){0, CORE_TRAVEL_BLOCK};

	unsigned short max_c = (game.grid_size > 0) ? (game.grid_size - 1) : 19;
	bool is_outer_border = (pos.x == 0 || pos.x == max_c || pos.y == 0 || pos.y == max_c);

	t_obj *object = core_get_obj_from_pos(pos);
	if (!object)
	{
		unsigned int extra_weight = 1;
		/* Outer border hugging: heavily penalize stepping into interior tiles to bypass Beta choke */
		if (!is_outer_border)
			extra_weight += 30;

		/* Avoid stepping into contested tiles or striking zone of active brawlers */
		for (size_t i = 0; game.objects && game.objects[i] != NULL; i++)
		{
			t_obj *o = game.objects[i];
			if (o->type == OBJ_UNIT && o->s_unit.team_id != game.my_team_id && o->hp > 0)
			{
				int d = manhattan_dist(pos, o->pos);
				if (d == 1 && o->s_unit.action_cooldown <= 0)
					return (t_tile_travelability){0, CORE_TRAVEL_BLOCK};
				if (o->s_unit.properties.damage_unit > 0)
				{
					if (d == 1) extra_weight += 25;
					else if (d == 2) extra_weight += 10;
				}
			}
		}

		return (t_tile_travelability){extra_weight, CORE_TRAVEL_PASS};
	}

	/* Friendly objects cannot be traversed or attacked */
	if (object->type == OBJ_UNIT && object->s_unit.team_id == game.my_team_id)
		return (t_tile_travelability){0, CORE_TRAVEL_BLOCK};
	if (object->type == OBJ_CORE && object->s_core.team_id == game.my_team_id)
		return (t_tile_travelability){0, CORE_TRAVEL_BLOCK};

	/* Enemy Core: Breaker's target */
	if (object->type == OBJ_CORE)
	{
		if (unit->s_unit.properties.damage_core > 0)
			return (t_tile_travelability){1, CORE_TRAVEL_ATTACK};
		return (t_tile_travelability){0, CORE_TRAVEL_BLOCK};
	}

	/* Gem Piles: Breakers never attack gem piles */
	if (object->type == OBJ_GEM_PILE)
		return (t_tile_travelability){0, CORE_TRAVEL_BLOCK};

	/* Obstacles or Enemy Units: Breaker has 0 damage_unit and 0 damage_object */
	return (t_tile_travelability){0, CORE_TRAVEL_BLOCK};
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

static void safe_travel_breaker(t_obj *unit, t_pos dest)
{
	if (!unit || unit->s_unit.action_cooldown > 0) return;
	unsigned int prev_count = actions.count;
	core_action_travel(unit, dest, breaker_travel_policy);
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
					/* Collision: tile was reserved this tick! Cancel move */
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

	/* Safety: if carrying gems and hostile combat unit is near, cash in early */
	if (gems >= 10 && count_combat_enemies_within(m->pos, enemies, 2) > 0)
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
		int best_strike_score = -9999;
		for (size_t i = 0; enemies && enemies[i] != NULL; i++)
		{
			t_obj *en = enemies[i];
			if (en->hp > 0 && manhattan_dist(attacker->pos, en->pos) == 1)
			{
				size_t e_idx = en->id % MAX_TRACKED_UNITS;
				if (en->hp <= g_pending_target_dmg[e_idx])
					continue; /* Target already destroyed by friendly unit this tick */

				int s = 0;
				/* Lethal strike bonus: if target will die from this hit */
				if (en->hp <= attacker->s_unit.properties.damage_unit)
					s += 2000;
				/* Miner hunt bonus: destroy enemy economy */
				if (en->s_unit.properties.damage_object > 0 && en->s_unit.properties.damage_unit == 0)
					s += 1500;
				if (en->s_unit.gems > 0)
					s += 500 + (int)(en->s_unit.gems * 10);
				/* Prefer lower hp to focus fire */
				s += (100 - (int)en->hp);

				if (s > best_strike_score)
				{
					best_strike_score = s;
					target = en;
				}
			}
		}
		if (target)
		{
			core_action_attack(attacker, target);
			size_t t_idx = target->id % MAX_TRACKED_UNITS;
			g_pending_target_dmg[t_idx] += attacker->s_unit.properties.damage_unit;
			return true;
		}
	}

	return false;
}

static void run_harvesters(t_obj *own_core, t_obj **enemies, t_obj **miners, t_obj **resources)
{
	size_t miner_count = count_obj_array(miners);
	size_t res_count = count_obj_array(resources);

	if (miner_count == 0) return;

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
			int threat = count_combat_enemies_within(res->pos, enemies, 2);

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
}

/* ========================================================================== */
/* SUBSYSTEM 2: FORWARD HUNTERS & LANCHESTER WOLFPACK                        */
/* ========================================================================== */

static void run_hunters(t_obj *own_core, t_obj *enemy_core, t_obj **enemies, t_obj **miners, t_obj **resources)
{
	t_obj **hunters = core_get_units_byName(HUNTER_NAME);
	t_obj **breakers = core_get_units_byName(BREAKER_NAME);
	size_t hunter_count = count_obj_array(hunters);
	size_t miner_count = count_obj_array(miners);
	size_t breaker_count = count_obj_array(breakers);
	size_t res_count = count_obj_array(resources);

	if (hunter_count == 0)
	{
		if (hunters) free(hunters);
		if (breakers) free(breakers);
		return;
	}

	unsigned short max_c = (game.grid_size > 0) ? (game.grid_size - 1) : 19;
	t_pos anchor_pos;
	t_pos sentry_a;
	t_pos sentry_b;
	if (own_core && own_core->pos.x <= max_c / 2)
	{
		anchor_pos = (t_pos){2, 2};
		sentry_a = (t_pos){2, 0}; /* Physically corks the top outer border corridor */
		sentry_b = (t_pos){0, 2}; /* Physically corks the left outer border corridor */
	}
	else
	{
		anchor_pos = (t_pos){(unsigned short)(max_c - 2), (unsigned short)(max_c - 2)};
		sentry_a = (t_pos){(unsigned short)(max_c - 2), max_c}; /* Corks bottom outer border */
		sentry_b = (t_pos){max_c, (unsigned short)(max_c - 2)}; /* Corks right outer border */
	}

	/* Count active enemy core attackers */
	size_t enemy_core_attackers = 0;
	for (size_t e = 0; enemies && enemies[e] != NULL; e++)
	{
		if (enemies[e]->hp > 0 && enemies[e]->s_unit.properties.damage_core > 0)
			enemy_core_attackers++;
	}

	for (size_t i = 0; i < hunter_count; i++)
	{
		t_obj *h = hunters[i];

		if (own_core && h->s_unit.gems > 0 && manhattan_dist(h->pos, own_core->pos) <= 1)
		{
			core_action_transferGems(h, own_core->pos, h->s_unit.gems);
			if (h->pos.x < MAX_GRID_DIM && h->pos.y < MAX_GRID_DIM)
				g_reserved_tiles[h->pos.x][h->pos.y] = true;
			continue; /* Transfer immediately resets cooldown on server; do not move! */
		}

		if (h->s_unit.action_cooldown > 0) continue;

		/* 1. Pre-move guaranteed cardinal strike */
		if (try_adjacent_combat_strike(h, enemies, enemy_core)) continue;

		/* Hunter 0 and Hunter 1 act as Citadel Sentries when enemy core attackers exist */
		bool is_sentry = (enemy_core_attackers > 0) ? (i == 0 || i == 1) : (i == 0);
		t_pos my_sentry = (enemy_core_attackers > 0) ? ((i == 0) ? sentry_a : sentry_b) : anchor_pos;

		/* 2. EMERGENCY CORE PEEL:
		** If ANY enemy unit is within 5 tiles of our Core, immediately peel back to intercept and neutralize!
		** Border sentries do NOT abandon their cork tile unless the enemy is directly touching our core (d_core <= 1)
		** or the threat is right in front of them (d_me <= 2).
		*/
		t_obj *emergency_threat = NULL;
		int min_threat_dist = 999;
		for (size_t e = 0; enemies && enemies[e] != NULL; e++)
		{
			t_obj *en = enemies[e];
			if (en->hp == 0) continue;
			int d_core = own_core ? manhattan_dist(own_core->pos, en->pos) : 99;
			int d_me = manhattan_dist(h->pos, en->pos);

			if (is_sentry && enemy_core_attackers > 0)
			{
				/* Sentry only peels if threat is touching our core or adjacent to sentry corridor */
				if (d_core > 1 && d_me > 2)
					continue;
			}

			if (d_core <= 5)
			{
				int threat_priority = d_core;
				if (en->s_unit.properties.damage_core > 0)
					threat_priority -= 20; /* Maximum emergency for Core Breakers */
				threat_priority += d_me;
				if (threat_priority < min_threat_dist)
				{
					min_threat_dist = threat_priority;
					emergency_threat = en;
				}
			}
		}

		if (emergency_threat)
		{
			safe_travel(h, emergency_threat->pos);
			continue;
		}

		/* 3. Target Evaluation: Priority scoring with Lanchester Wolfpack Concentration */
		t_obj *best_enemy = NULL;
		int best_score = -999999;

		for (size_t e = 0; enemies && enemies[e] != NULL; e++)
		{
			t_obj *en = enemies[e];
			if (en->hp == 0) continue;
			int d_me = manhattan_dist(h->pos, en->pos);
			int d_core = own_core ? manhattan_dist(own_core->pos, en->pos) : 99;

			int score = -(d_me * SCORE_DIST_PENALTY) - (int)(en->hp * SCORE_HP_FOCUS_WEIGHT);

			/* Priority 1: ENEMY CORE ATTACKERS (Mortal Threat to our Base)
			** Any unit with damage_core > 0 must be intercepted before reaching our Core!
			*/
			if (en->s_unit.properties.damage_core > 0)
			{
				score += SCORE_CORE_ATTACKER_BONUS;
				score += (38 - d_core) * SCORE_CORE_THREAT_WEIGHT;
			}
			/* Priority 2: HARASSMENT & MINER HUNTING
			** Prioritize enemy harvesters / miners to starve enemy economy!
			** Bypasses bodyguards/guardians to eliminate resource gatherers.
			*/
			else if (en->s_unit.properties.damage_object > 0 || en->s_unit.gems > 0)
			{
				score += SCORE_HUNT_MINER_BONUS;
				if (en->s_unit.properties.damage_unit == 0)
					score += 1000; /* Defenseless miner bonus */
				if (en->s_unit.gems > 0)
					score += SCORE_ENEMY_MINER_GEMS + (int)(en->s_unit.gems * 15);
				/* In-range miner bonus: ruthless focus if within engagement distance */
				if (d_me <= 8)
					score += (9 - d_me) * 250;
			}
			/* Priority 3: Protect friendly miners */
			else
			{
				for (size_t m = 0; m < miner_count; m++)
				{
					if (manhattan_dist(miners[m]->pos, en->pos) <= 3)
					{
						score += SCORE_PROTECT_MINER_BONUS;
						break;
					}
				}
				/* Priority 4: Protect friendly Breakers (combat escort) */
				for (size_t b = 0; b < breaker_count; b++)
				{
					if (manhattan_dist(breakers[b]->pos, en->pos) <= 3)
					{
						score += 1500;
						break;
					}
				}
				/* Priority 5: Enemy combat unit */
				if (en->s_unit.properties.damage_unit > 0)
				{
					score += 1500;
					if (d_core <= DEFENSE_PANIC_RADIUS)
						score += (DEFENSE_PANIC_RADIUS - d_core) * 100;
				}
			}

			/* Lanchester Wolfpack Concentration:
			** If another friendly Hunter is already within 3 tiles of this enemy,
			** heavily reward collapsing together on the exact same unit (focus fire)!
			*/
			int allied_hunters_near = 0;
			for (size_t j = 0; j < hunter_count; j++)
			{
				if (j != i && manhattan_dist(hunters[j]->pos, en->pos) <= 3)
					allied_hunters_near++;
			}
			if (allied_hunters_near > 0)
			{
				score += allied_hunters_near * SCORE_WOLFPACK_BONUS;
			}

			/* Lethal blow focus */
			if (en->hp <= h->s_unit.properties.damage_unit && d_me <= 2)
				score += SCORE_LETHAL_BLOW_BONUS;

			if (score > best_score)
			{
				best_score = score;
				best_enemy = en;
			}
		}

		if (is_sentry)
		{
			t_obj *threat_to_base = NULL;
			int min_d_base = 999;
			for (size_t e = 0; enemies && enemies[e] != NULL; e++)
			{
				t_obj *en = enemies[e];
				if (en->hp == 0) continue;
				int d_c = own_core ? manhattan_dist(own_core->pos, en->pos) : 99;
				int d_me = manhattan_dist(h->pos, en->pos);

				/* When corking, sentry holds the choke unless threat is adjacent to sentry or hitting Core */
				if (enemy_core_attackers > 0)
				{
					if (d_c > 1 && d_me > 2)
						continue;
				}
				else if (d_c > 6)
				{
					continue;
				}

				int p = d_c;
				if (en->s_unit.properties.damage_core > 0)
					p -= 15;
				p += d_me;
				if (p < min_d_base)
				{
					min_d_base = p;
					threat_to_base = en;
				}
			}

			if (threat_to_base)
			{
				safe_travel(h, threat_to_base->pos);
			}
			else if (h->pos.x != my_sentry.x || h->pos.y != my_sentry.y)
			{
				safe_travel(h, my_sentry);
			}
			continue;
		}

		if (best_enemy)
		{
			safe_travel(h, best_enemy->pos);
		}
		else
		{

			/* No enemies visible: Forward Map Pressure & Mutual Hunter Cohesion! */
			int min_partner_dist = 999;
			t_pos partner_pos = h->pos;
			size_t start_partner_idx = (enemy_core_attackers > 0) ? 2 : 1;
			for (size_t j = start_partner_idx; j < hunter_count; j++)
			{
				if (j != i)
				{
					int dp = manhattan_dist(h->pos, hunters[j]->pos);
					if (dp < min_partner_dist)
					{
						min_partner_dist = dp;
						partner_pos = hunters[j]->pos;
					}
				}
			}

			/* If trailing far behind another forward Hunter, rally towards partner for wolfpack */
			if (min_partner_dist > 4 && min_partner_dist < 999 && hunter_count > start_partner_idx + 1)
			{
				safe_travel(h, partner_pos);
				continue;
			}

			t_pos forward_target;
			bool found_forward_res = false;

			int best_res_dist = 9999;
			for (size_t r = 0; r < res_count; r++)
			{
				t_obj *res = resources[r];
				if (!res || res->hp == 0) continue;
				int d_res_enemy_core = enemy_core ? manhattan_dist(res->pos, enemy_core->pos) : 99;
				int d_res_own_core = own_core ? manhattan_dist(res->pos, own_core->pos) : 99;

				/* Resource in mid-field or enemy territory */
				if (d_res_enemy_core <= d_res_own_core + 2)
				{
					int d_h_res = manhattan_dist(h->pos, res->pos);
					if (d_h_res < best_res_dist)
					{
						best_res_dist = d_h_res;
						forward_target = res->pos;
						found_forward_res = true;
					}
				}
			}

			if (found_forward_res)
			{
				safe_travel(h, forward_target);
			}
			else if (enemy_core)
			{
				/* Advance toward enemy core to maintain forward pressure & escort Breakers */
				t_pos escort_pos = enemy_core->pos;
				if (escort_pos.x >= 2) escort_pos.x -= 2;
				else escort_pos.x += 2;
				if (escort_pos.y >= 2) escort_pos.y -= 2;
				else escort_pos.y += 2;
				safe_travel(h, escort_pos);
			}
		}
	}

	free(hunters);
	if (breakers) free(breakers);
}

/* ========================================================================== */
/* SUBSYSTEM 3: ARMORED SIEGE BREAKERS & FLANKING PATHFINDING                 */
/* ========================================================================== */

static void run_breakers(t_obj *own_core, t_obj *enemy_core, t_obj **enemies)
{
	t_obj **breakers = core_get_units_byName(BREAKER_NAME);
	if (!breakers) return;

	unsigned short max_c = (game.grid_size > 0) ? (game.grid_size - 1) : 19;

	for (size_t i = 0; breakers[i] != NULL; i++)
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

		if (!enemy_core) continue;

		/* Absolute outer border flank waypoints: strictly on map edges to bypass Beta chokes */
		t_pos flank_a = (t_pos){max_c, 0};
		t_pos flank_b = (t_pos){0, max_c};

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
		if (brain && brain->state != STATE_SIEGE && d_to_flank <= 1)
			brain->state = STATE_SIEGE;

		t_pos dest;
		if (brain && brain->state == STATE_SIEGE)
			dest = enemy_core->pos;
		else
			dest = chosen_flank;

		safe_travel_breaker(b, dest);
	}
	free(breakers);
}

/* ========================================================================== */
/* SUBSYSTEM 4: DYNAMIC MACRO ECONOMY & COUNTER-SPAWNER                       */
/* ========================================================================== */

static void run_macro_spawner(t_obj *own_core, t_obj **enemies)
{
	if (!own_core || own_core->s_core.spawn_cooldown > 0) return;

	size_t miner_count = core_get_units_byName_count(HARVESTER_NAME);
	size_t hunter_count = core_get_units_byName_count(HUNTER_NAME);
	size_t breaker_count = core_get_units_byName_count(BREAKER_NAME);

	t_obj **resources = core_get_objs_filter(is_resource);
	size_t res_count = count_obj_array(resources);
	if (resources) free(resources);

	/* Count enemy combat threats with unit_attack and core_attack */
	size_t enemy_threats = 0;
	bool base_threat = false;
	for (size_t e = 0; enemies && enemies[e] != NULL; e++)
	{
		t_obj *en = enemies[e];
		if (en->hp == 0) continue;
		if (en->s_unit.properties.damage_unit > 0 || en->s_unit.properties.damage_core > 0)
			enemy_threats++;
		if (en->s_unit.properties.damage_unit > 0)
		{
			if (own_core && manhattan_dist(own_core->pos, en->pos) <= 4)
				base_threat = true;
		}
		if (en->s_unit.properties.damage_core > 0)
		{
			if (own_core && manhattan_dist(own_core->pos, en->pos) <= 14)
				base_threat = true;
		}
	}

	/* Emergency defense: If our core is threatened, prioritize combat Hunters! */
	if (base_threat && hunter_count < 4)
	{
		if (own_core->s_core.gems >= HUNTER_COST)
			core_action_createUnit(HUNTER_NAME, "unit_attack", "health", NULL);
		return;
	}

	/* 1. Fast boot Harvester 1 for immediate mining */
	if (res_count > 0 && miner_count < 1)
	{
		if (own_core->s_core.gems >= HARVESTER_COST)
			core_action_createUnit(HARVESTER_NAME, "mining", NULL);
		return;
	}

	/* 2. Swift Vanguard Hunter (immediate choke security & forward map pressure) */
	if (hunter_count < 1)
	{
		if (own_core->s_core.gems >= HUNTER_COST)
			core_action_createUnit(HUNTER_NAME, "unit_attack", "health", NULL);
		return;
	}

	/* 3. Second Harvester to maximize local economy */
	if (res_count > 0 && miner_count < MAX_MINERS)
	{
		if (own_core->s_core.gems >= HARVESTER_COST)
			core_action_createUnit(HARVESTER_NAME, "mining", NULL);
		return;
	}

	/* 4. Primary Armored Siege Breaker (Primary Win Condition) */
	if (breaker_count < 1)
	{
		if (own_core->s_core.gems >= BREAKER_COST)
			core_action_createUnit(BREAKER_NAME, "core_attack", "armor", "health", NULL);
		return;
	}

	/* 5. Second Hunter for Lanchester wolfpack pairing */
	if (hunter_count < 2)
	{
		if (own_core->s_core.gems >= HUNTER_COST)
			core_action_createUnit(HUNTER_NAME, "unit_attack", "health", NULL);
		return;
	}

	/* 6. Second Armored Siege Breaker (Dual-Angle Flank Squeeze) */
	if (breaker_count < 2)
	{
		if (own_core->s_core.gems >= BREAKER_COST)
			core_action_createUnit(BREAKER_NAME, "core_attack", "armor", "health", NULL);
		return;
	}

	/* 7. Combat Parity: If enemy has mass threats (breakers or fighters), scale hunters up to 3-4 */
	if (hunter_count < 4 && enemy_threats > hunter_count)
	{
		if (own_core->s_core.gems >= HUNTER_COST)
			core_action_createUnit(HUNTER_NAME, "unit_attack", "health", NULL);
		return;
	}

	/* 8. Continuous aggressive swarm: maintain 2-3 Breakers and Hunters */
	if (own_core->s_core.gems >= BREAKER_COST)
	{
		if (breaker_count < 3)
			core_action_createUnit(BREAKER_NAME, "core_attack", "armor", "health", NULL);
		else
			core_action_createUnit(HUNTER_NAME, "unit_attack", "health", NULL);
	}
	else if (own_core->s_core.gems >= HUNTER_COST && breaker_count >= 2)
	{
		core_action_createUnit(HUNTER_NAME, "unit_attack", "health", NULL);
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
	t_obj **resources = core_get_objs_filter(is_resource);

	/* 1. Run Harvesters first so same-tick gem transfers credit the Core */
	run_harvesters(own_core, enemies, miners, resources);

	/* 2. Run Hunters (interceptors/forward pressure) and Siege Breakers */
	run_hunters(own_core, enemy_core, enemies, miners, resources);
	run_breakers(own_core, enemy_core, enemies);

	/* 3. Execute Core Spawning decisions */
	run_macro_spawner(own_core, enemies);

	if (resources) free(resources);
	if (miners) free(miners);
	if (enemies) free(enemies);
}

int main(int argc, char **argv)
{
	return (core_startGame("bot_alpha", argc, argv, ft_on_tick, false));
}
