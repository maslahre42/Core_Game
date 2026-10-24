#include "core_lib.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static int manhattan_dist(t_pos a, t_pos b)
{
	int dx = (int)a.x - (int)b.x;
	int dy = (int)a.y - (int)b.y;
	if (dx < 0) dx = -dx;
	if (dy < 0) dy = -dy;
	return (dx + dy);
}

static bool is_my_unit(const t_obj *obj)
{
	return (obj && obj->type == OBJ_UNIT && obj->s_unit.team_id == game.my_team_id && obj->hp > 0);
}

static bool is_resource(const t_obj *obj)
{
	return (obj && (obj->type == OBJ_GEM_PILE || obj->type == OBJ_DEPOSIT) && obj->hp > 0);
}

static t_obj *get_core(bool own)
{
	for (size_t i = 0; game.objects && game.objects[i]; i++)
	{
		t_obj *o = game.objects[i];
		if (o->type == OBJ_CORE)
		{
			if (own && o->s_core.team_id == game.my_team_id) return (o);
			if (!own && o->s_core.team_id != game.my_team_id) return (o);
		}
	}
	return (NULL);
}

void on_eco_tick(unsigned long tick)
{
	(void)tick;
	t_obj *home = get_core(true);
	t_obj *enemy = get_core(false);
	if (!home || !enemy || home->hp == 0 || enemy->hp == 0) return;

	t_obj **my_units = core_get_objs_filter(is_my_unit);
	size_t miner_count = 0;
	for (size_t i = 0; my_units && my_units[i]; i++)
	{
		if (my_units[i]->s_unit.properties.damage_object > 0)
			miner_count++;
	}

	/* Macro Spawner */
	if (home->s_core.spawn_cooldown == 0)
	{
		if (miner_count < 4 && home->s_core.gems >= 35)
			core_action_createUnit("EcoMiner", "mining", NULL);
		else if (home->s_core.gems >= 47)
			core_action_createUnit("EcoDefender", "unit_attack", "health", NULL);
	}

	t_obj **resources = core_get_objs_filter(is_resource);

	for (size_t i = 0; my_units && my_units[i]; i++)
	{
		t_obj *u = my_units[i];
		if (u->s_unit.action_cooldown > 0) continue;

		if (u->s_unit.properties.damage_object > 0)
		{
			/* Miner logic */
			if (u->s_unit.gems >= 35)
			{
				if (manhattan_dist(u->pos, home->pos) == 1)
					core_action_transferGems(u, home->pos, u->s_unit.gems);
				else
					core_action_travel(u, home->pos, NULL);
			}
			else
			{
				t_obj *best_res = NULL;
				int best_dist = 999;
				for (size_t r = 0; resources && resources[r]; r++)
				{
					int d = manhattan_dist(u->pos, resources[r]->pos);
					if (d < best_dist)
					{
						best_dist = d;
						best_res = resources[r];
					}
				}
				if (best_res)
				{
					if (best_res->type == OBJ_DEPOSIT && manhattan_dist(u->pos, best_res->pos) == 1)
						core_action_attack(u, best_res);
					else
						core_action_travel(u, best_res->pos, NULL);
				}
				else
					core_action_travel(u, home->pos, NULL);
			}
		}
		else
		{
			/* Defender logic: patrol between miners and home */
			core_action_travel(u, (t_pos){home->pos.x == 0 ? 5 : 14, home->pos.y == 0 ? 5 : 14}, NULL);
		}
	}

	if (resources) free(resources);
	if (my_units) free(my_units);
}

int main(int argc, char **argv)
{
	return (core_startGame("Opponent_Economy", argc, argv, on_eco_tick, false));
}

