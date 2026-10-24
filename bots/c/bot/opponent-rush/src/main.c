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

void on_rush_tick(unsigned long tick)
{
	(void)tick;
	t_obj *home = get_core(true);
	t_obj *enemy = get_core(false);
	if (!home || !enemy || home->hp == 0 || enemy->hp == 0) return;

	/* Endless Core Rusher Spawn */
	if (home->s_core.spawn_cooldown == 0 && home->s_core.gems >= 47)
	{
		core_action_createUnit("RushAttacker", "core_attack", "health", NULL);
	}
	else if (home->s_core.spawn_cooldown == 0 && home->s_core.gems >= 35)
	{
		core_action_createUnit("RushLight", "core_attack", NULL);
	}

	t_obj **units = core_get_objs_filter(is_my_unit);
	for (size_t i = 0; units && units[i]; i++)
	{
		t_obj *u = units[i];
		if (u->s_unit.action_cooldown > 0) continue;

		if (manhattan_dist(u->pos, enemy->pos) == 1)
			core_action_attack(u, enemy);
		else
			core_action_travel(u, enemy->pos, NULL);
	}
	if (units) free(units);
}

int main(int argc, char **argv)
{
	return (core_startGame("Opponent_Rush", argc, argv, on_rush_tick, false));
}

