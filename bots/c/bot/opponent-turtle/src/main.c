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

void on_turtle_tick(unsigned long tick)
{
	(void)tick;
	t_obj *home = get_core(true);
	t_obj *enemy_core = get_core(false);
	if (!home || !enemy_core || home->hp == 0 || enemy_core->hp == 0) return;

	if (home->s_core.spawn_cooldown == 0)
	{
		if (home->s_core.gems >= 65)
			core_action_createUnit("ArmoredTurtle", "unit_attack", "armor", NULL);
		else if (home->s_core.gems >= 47)
			core_action_createUnit("GuardTurtle", "unit_attack", "health", NULL);
	}

	t_obj **my_units = core_get_objs_filter(is_my_unit);
	t_obj **enemies = core_get_objs_filter(is_enemy_unit);

	/* Check if any enemy is within perimeter */
	t_obj *intruder = NULL;
	int min_intruder_d = 999;
	for (size_t e = 0; enemies && enemies[e]; e++)
	{
		int d = manhattan_dist(enemies[e]->pos, home->pos);
		if (d <= 7 && d < min_intruder_d)
		{
			min_intruder_d = d;
			intruder = enemies[e];
		}
	}

	for (size_t i = 0; my_units && my_units[i]; i++)
	{
		t_obj *u = my_units[i];
		if (u->s_unit.action_cooldown > 0) continue;

		if (intruder)
		{
			if (manhattan_dist(u->pos, intruder->pos) == 1)
				core_action_attack(u, intruder);
			else
				core_action_travel(u, intruder->pos, NULL);
		}
		else
		{
			/* Hold perimeter at radius 2-3 from home */
			int d_home = manhattan_dist(u->pos, home->pos);
			if (d_home > 3)
				core_action_travel(u, home->pos, NULL);
			else
			{
				/* Spread around home */
				t_pos bunker_pos = {
					(unsigned short)(home->pos.x == 0 ? (i % 3 + 1) : (18 - (i % 3))),
					(unsigned short)(home->pos.y == 0 ? (i / 3 + 1) : (18 - (i / 3)))
				};
				if (u->pos.x != bunker_pos.x || u->pos.y != bunker_pos.y)
					core_action_travel(u, bunker_pos, NULL);
			}
		}
	}

	if (enemies) free(enemies);
	if (my_units) free(my_units);
}

int main(int argc, char **argv)
{
	return (core_startGame("Opponent_Turtle", argc, argv, on_turtle_tick, false));
}

