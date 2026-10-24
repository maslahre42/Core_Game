# 42 CORE Engine Mechanics & Physics Specification

Authoritative reference compiled from inspecting engine source code (`server/src/`, `bots/c/client_lib/`, `bots/configs/fake_demo/game.config.json`).

---

## 1. Action Execution Pipeline (5 Phases)

At the end of each tick, the server resolves all queued actions sequentially in five distinct phases (`server/src/game/Game.cpp`):
1. **Phase 1: `transfer_gems`** (`server/src/action/TransferGemsAction.cpp:71-140`)
   - Resolves transfers between adjacent units, cores, and gem piles.
   - **Crucial Rule**: `TransferGemsAction.cpp:123` executes `srcUnit->resetActionCooldown()`. This resets the unit's cooldown immediately to `baseActionCooldown` (typically 3).
   - **Consequence**: A unit CANNOT transfer gems and move on the same tick. Any subsequent `move` in Phase 4 is rejected with `"unit is on action cooldown"`.
2. **Phase 2: `build`** (`server/src/action/BuildAction.cpp`)
   - Constructs walls.
3. **Phase 3: `attack`** (`server/src/action/AttackAction.cpp:28-70`)
   - Attacks units, cores, deposits, or walls at Manhattan distance 1 (`|dx| + |dy| == 1`).
   - Resets unit cooldown to `baseActionCooldown`.
   - **Pre-Move Cardinal Strike Exploit**: Because Phase 3 resolves **before** Phase 4 (`move`), an attack issued to an adjacent enemy lands before that enemy can move away.
4. **Phase 4: `move`** (`server/src/action/MoveAction.cpp:26-48`)
   - Moves unit to an adjacent empty tile.
   - Fails if destination is occupied (`getObjectAtPos(target_) != NULL`).
   - Requires friendly tile reservations to avoid collision conflicts between allies.
5. **Phase 5: `create`** (`server/src/action/CreateAction.cpp:25-123`)
   - Spawns units adjacent to the friendly Core.
   - Deducts unit cost from Core balance and sets `core->setSpawnCooldown(postSpawnCoreCooldown)` (10 ticks).

---

## 2. Action Readiness & Cooldown Mechanics

In `server/src/object/Unit.cpp:19-36`:
```cpp
void Unit::tickActionCooldown()
{
	const unsigned int gemAdjustedActionCooldown = calcActionCooldown();
	if (action_cooldown_ > 0)
	{
		action_cooldown_--;
		if (gemAdjustedActionCooldown < static_cast<unsigned int>(action_cooldown_))
			action_cooldown_ = gemAdjustedActionCooldown;
		return;
	}

	const unsigned int unitStandingCooldownLimit =
			std::min(gemAdjustedActionCooldown, Config::game().maxUnitStandingCooldown);
	action_cooldown_--;
	if (-action_cooldown_ > static_cast<long long>(unitStandingCooldownLimit))
		action_cooldown_ = unitStandingCooldownLimit;
}
```

* **Idle-Ready Negative Cooldown**: When a unit stands idle and ready, `action_cooldown_` continues to decrement below 0 down to `-unitStandingCooldownLimit`.
* **Action Guard**:
  ```c
  if (unit->s_unit.action_cooldown <= 0) // CORRECT: Handles ready and idle-ready states
  ```
  Checking `action_cooldown == 0` causes idle units with negative cooldown to skip actions!

---

## 3. Unit Component Combinations & Validation Rules

From `server/src/action/CreateAction.cpp:94-113` and `bots/configs/fake_demo/game.config.json`:
* Base unit cost: **20 gems**.
* Component slots: Maximum 5 components per unit.
* Component cost table:
  - `mining`: Cost 15 (`damageObject +3`)
  - `unit_attack`: Cost 15 (`damageUnit +3`)
  - `core_attack`: Cost 15 (`damageCore +3`)
  - `health`: Cost 12 (`hp +3`, default 5)
  - `speed`: Cost 12 (`baseActionCooldown -2`, default 3)
  - `armor`: Cost 30 (`damageReductionPercent +20%`, max 4 armor components)
  - `gem_carrying_efficiency`: Cost 20 (`gemsPerCooldownStep +10`, default 10)
  - `gem_carrying_capacity`: Cost 20 (`maxGems +50`, default 100)

### Unbreakable Invalidation Rule:
$$\text{baseActionCooldown} \ge (\text{damageUnit} + \text{damageCore} + \text{damageObject})$$
* If `speed` is equipped: `baseActionCooldown` becomes $3 - 2 = 1$.
* If ANY attack component is equipped: total damage is at least 3.
* Because $1 < 3$, **no combat unit can ever combine `speed` with attack**! Attempting to spawn `speed` + attack results in creation failure.

---

## 4. Damage & Armor Calculation

In `server/src/action/AttackAction.cpp:45-66`:
* Unit attack on Unit: uses `damage_unit`.
* Unit attack on Core: uses `damage_core`.
* Unit attack on Deposit/Wall: uses `damage_object`.
* Armor calculation:
  $$\text{reduced} = \frac{\text{baseDamage} \cdot (100 - \text{armorPercent}) + 50}{100}$$
  $$\text{finalDamage} = \text{baseDamage} == 0 \ ? \ 0 : \max(1, \text{reduced})$$
* **Zero Damage Immunity**: If `baseDamage == 0` (e.g., `damage_unit == 0`), the attack deals **strictly 0 damage**. Pure Core attackers cannot harm units!

---

## 5. Geometry & Corner Core Access

* Grid is a 20×20 matrix: `(0, 0)` to `(19, 19)`.
* Cores are placed at opposite corners: Team 42 at `(0, 0)`, Team 418 at `(19, 19)`.
* **Corner Bottleneck**: A corner Core at `(19, 19)` has only **two** adjacent cardinal tiles: `(18, 19)` and `(19, 18)`.
* At most 2 units can attack the Core in any single tick. Additional units queue up behind them, creating congestion if wave sizes exceed 2–3 units.

