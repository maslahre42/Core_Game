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

static bool is_enemy_unit(const t_obj *obj)
{
	return (obj && obj->type == OBJ_UNIT && obj->s_unit.team_id != game.my_team_id && obj->hp > 0);
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

void on_brawler_tick(unsigned long tick)
{
	(void)tick;
	t_obj *home = get_core(true);
	t_obj *enemy_core = get_core(false);
	if (!home || !enemy_core || home->hp == 0 || enemy_core->hp == 0) return;

	if (home->s_core.spawn_cooldown == 0 && home->s_core.gems >= 59)
	{
		core_action_createUnit("HeavyBrawler", "unit_attack", "health", "health", NULL);
	}
	else if (home->s_core.spawn_cooldown == 0 && home->s_core.gems >= 47)
	{
		core_action_createUnit("Brawler", "unit_attack", "health", NULL);
	}

	t_obj **my_units = core_get_objs_filter(is_my_unit);
	t_obj **enemies = core_get_objs_filter(is_enemy_unit);

	for (size_t i = 0; my_units && my_units[i]; i++)
	{
		t_obj *u = my_units[i];
		if (u->s_unit.action_cooldown > 0) continue;

		t_obj *target = NULL;
		int min_d = 999;
		for (size_t e = 0; enemies && enemies[e]; e++)
		{
			int d = manhattan_dist(u->pos, enemies[e]->pos);
			if (d < min_d)
			{
				min_d = d;
				target = enemies[e];
			}
		}

		if (target)
		{
			if (min_d == 1)
				core_action_attack(u, target);
			else
				core_action_travel(u, target->pos, NULL);
		}
		else
		{
			/* No enemy units: advance to enemy core */
			if (manhattan_dist(u->pos, enemy_core->pos) == 1 && u->s_unit.properties.damage_core > 0)
				core_action_attack(u, enemy_core);
			else
				core_action_travel(u, enemy_core->pos, NULL);
		}
	}

	if (enemies) free(enemies);
	if (my_units) free(my_units);
}

int main(int argc, char **argv)
{
	return (core_startGame("Opponent_Brawler", argc, argv, on_brawler_tick, false));
}

