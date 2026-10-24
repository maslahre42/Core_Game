# 42 CORE Server Engine Mechanics & Physics Specification Report

**Document**: Spec Miner Engine Survey Handoff Report  
**Author**: Spec Miner Engine Survey Agent  
**Target Citation**: `docs/MECHANICS.md`  
**Working Directory**: `/home/malshare/Evaluations/Core_game/.agents/teamwork/spec_miner_engine_survey/`  
**Date**: 2026-10-24  

---

## 1. Observation

Direct source observations with exact file paths, line numbers, and verbatim code quotes from the authoritative game engine (`server/src/`, `server/inc/`, `server/data/`, and `bots/configs/`):

### 1.1 Action Readiness & Cooldown Mechanics
- **File**: `server/src/object/Unit.cpp`, Lines 19–36:
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
  	// A unit that stands ready for too long is put back on cooldown so it cannot bank readiness forever.
  	if (-action_cooldown_ > static_cast<long long>(unitStandingCooldownLimit))
  		action_cooldown_ = unitStandingCooldownLimit;
  }
  ```
- **File**: `server/src/object/Unit.cpp`, Lines 38–47:
  ```cpp
  unsigned int Unit::calcActionCooldown()
  {
  	unsigned int step = static_cast<unsigned int>(std::max(1, properties_.at(UnitProperty::GEMS_PER_COOLDOWN_STEP)));
  	unsigned long long steps = balance_ / step;
  	unsigned long long cd =
  			static_cast<unsigned int>(std::max(0, properties_.at(UnitProperty::BASE_ACTION_COOLDOWN))) + steps;

  	return static_cast<unsigned int>(
  			std::clamp(cd, 1ull, static_cast<unsigned long long>(std::numeric_limits<unsigned int>::max())));
  }
  ```
- **File**: `server/inc/object/Unit.h`, Lines 38, 44, 52:
  ```cpp
  long long getActionCooldown() const { return action_cooldown_; }
  void resetActionCooldown() { action_cooldown_ = calcActionCooldown(); }
  long long action_cooldown_ = 0;
  ```
- **File**: `server/src/action/MoveAction.cpp`, Lines 31–33:
  ```cpp
  if (unit->getActionCooldown() > 0)
  	return "unit is on action cooldown (action cooldown should be 0 or less to perform an action) or has already "
  		   "moved this tick";
  ```
- **File**: `server/src/action/AttackAction.cpp`, Lines 40–41:
  ```cpp
  if (unit->getActionCooldown() > 0)
  	return "unit is on action cooldown (action cooldown should be 0 or less to perform an action)";
  ```
- **File**: `server/src/action/TransferGemsAction.cpp`, Lines 41–42, 123:
  ```cpp
  if (srcUnit->getActionCooldown() > 0)
  	return "unit is on action cooldown (action cooldown should be 0 or less to perform an action)";
  // ...
  if (srcUnit->getActionCooldown() > 0) return "unit is on action cooldown (action cooldown should be 0 or less)";
  ```
- **File**: `server/src/object/Core.cpp`, Lines 25–28:
  ```cpp
  void Core::tickSpawnCooldown()
  {
  	if (spawn_cooldown_ > 0) spawn_cooldown_--;
  }
  ```

### 1.2 Action Phase Pipeline & Transfer Reset
- **File**: `server/src/utils/Utils.cpp`, Lines 8–53:
  ```cpp
  void shuffle_actions_vector(std::vector<std::pair<std::unique_ptr<Action>, Core *>> &actions, uint64_t seed)
  {
  	std::mt19937_64 generator(seed);

  	auto phase_of = [](ActionType t) -> int
  	{
  		switch (t)
  		{
  		case ActionType::TRANSFER_GEMS:
  			return 0;
  		case ActionType::ATTACK:
  			return 1;
  		case ActionType::MOVE:
  			return 2;
  		case ActionType::CREATE:
  			return 3;
  		default:
  			throw std::runtime_error("Unknown ActionType");
  		}
  	};

  	std::array<std::vector<std::pair<std::unique_ptr<Action>, Core *>>, 5> buckets;
  	for (auto &ele : actions)
  	{
  		const int p = ele.first ? phase_of(ele.first->getActionType()) : 3;
  		buckets[p].push_back(std::move(ele));
  	}
  	for (auto &bucket : buckets)
  	{
  		// Packet arrival order is nondeterministic; establish a stable order before applying seeded randomness.
  		std::stable_sort(bucket.begin(), bucket.end(),
  						 [](const auto &lhs, const auto &rhs)
  						 {
  							 if (!lhs.second) return false;
  							 if (!rhs.second) return true;
  							 return lhs.second->getTeamId() < rhs.second->getTeamId();
  						 });
  		std::shuffle(bucket.begin(), bucket.end(), generator);
  	}
  	actions.clear();
  	for (auto &bucket : buckets)
  	{
  		for (auto &ele : bucket)
  			actions.emplace_back(std::move(ele));
  	}
  }
  ```
- **File**: `server/src/action/TransferGemsAction.cpp`, Lines 49, 128:
  ```cpp
  // dropGems:
  srcUnit->resetActionCooldown();
  // execute (transfer to dstObj):
  srcUnit->resetActionCooldown();
  ```
- **File**: `server/src/game/Game.cpp`, Lines 211–248:
  ```cpp
  shuffle_actions_vector(actions, Config::game().seed + tick);

  for (auto &ele : actions)
  {
  	auto &action = ele.first;
  	Core *core = ele.second;
  	if (!core || !action)
  	{
  		action = nullptr;
  		continue;
  	}

  	std::string err = action->execute(core);
  	if (!err.empty())
  	{
  		json actionJson = action->encodeJSON();
  		std::string fullErr = "Tick " + std::to_string(tick - 1) +
  							  ": Action Failure: " + Action::getActionName(action->getActionType()) + ": " + err +
  							  " (" + actionJson.dump() + ")";
  		// ...
  		failures.emplace_back(core->getTeamId(), fullErr);
  		action = nullptr;
  	}
  }
  ```

### 1.3 Unit Design Validation & Invalidation Rules
- **File**: `server/src/action/CreateAction.cpp`, Lines 80–123:
  ```cpp
  std::string CreateAction::execute(Core *core)
  {
  	if (core->getSpawnCooldown() > 0)
  		return "core spawn cooldown not done - wait " + std::to_string(core->getSpawnCooldown()) + " ticks";

  	Position closestEmptyPos = findFirstEmptyGridCell(Board::instance().getObjectPositionById(core->getId()));
  	if (!closestEmptyPos.isValid(Config::game().gridSize)) return "no valid position found - entire grid is filled up";

  	if (components_.size() > Config::game().maxComponentsPerUnit)
  	{
  		return "too many components - has " + std::to_string(components_.size()) + ", max is " +
  			   std::to_string(Config::game().maxComponentsPerUnit);
  	}

  	std::map<std::string, unsigned int> componentCounts;
  	unsigned int unitCost = Config::game().defaultUnitCost;

  	for (const std::string &componentId : components_)
  	{
  		ComponentConfig *component = Config::getComponentConfig(componentId);
  		if (!component) return "invalid component \"" + componentId + "\"";

  		componentCounts[componentId]++;
  		unitCost += component->cost;
  	}

  	if (core->getBalance() < unitCost)
  		return "insufficient funds - has " + std::to_string(core->getBalance()) + ", needs " + std::to_string(unitCost);

  	std::map<UnitProperty, int> properties = ComponentLogic::getUnitProperties(componentCounts);

  	const std::string invalidConditionMessage = ComponentLogic::getInvalidConditionMessage(properties, componentCounts);
  	if (!invalidConditionMessage.empty()) return invalidConditionMessage;

  	std::string name = name_.empty() ? generateUnitName() : name_;
  	Board::instance().addObject<Unit>(Unit(core->getTeamId(), properties, components_, name), closestEmptyPos);
  	core->setBalance(core->getBalance() - unitCost);

  	Stats::instance().inc(stat_keys::units_spawned);
  	Stats::instance().inc(stat_keys::actions_executed);

  	core->setSpawnCooldown(properties.at(UnitProperty::POST_SPAWN_CORE_COOLDOWN));

  	return "";
  }
  ```
- **File**: `server/src/utils/ComponentLogic.cpp`, Lines 7–29:
  ```cpp
  std::map<UnitProperty, int> ComponentLogic::getUnitProperties(const std::map<std::string, unsigned int> &components)
  {
  	std::map<UnitProperty, int> properties = Config::game().defaultUnitProperties;

  	for (const auto &pair : components)
  	{
  		const std::string &componentId = pair.first;
  		unsigned int count = pair.second;

  		ComponentConfig *config = Config::getComponentConfig(componentId);
  		if (!config) continue;

  		for (const auto &propertyPair : config->properties)
  		{
  			UnitProperty property = propertyPair.first;
  			int value = propertyPair.second * count;

  			properties[property] += value;
  		}
  	}

  	return properties;
  }
  ```
- **File**: `bots/configs/fake_demo/game.config.json`, Lines 132–175:
  ```json
  "invalidConditions": [
  	{
  		"message": "Your action cooldown may not be less than your total attacks added together",
  		"condition": {
  			"type": "less_than",
  			"1": {
  				"type": "property",
  				"property": "baseActionCooldown"
  			},
  			"2": {
  				"type": "sum",
  				"1": {
  					"type": "sum",
  					"1": {
  						"type": "property",
  						"property": "damageUnit"
  					},
  					"2": {
  						"type": "property",
  						"property": "damageCore"
  					}
  				},
  				"2": {
  					"type": "property",
  					"property": "damageObject"
  				}
  			}
  		}
  	},
  	{
  		"message": "You may not have more than 4 armor components",
  		"condition": {
  			"type": "greater_than",
  			"1": {
  				"type": "component_count",
  				"component": "armor"
  			},
  			"2": {
  				"type": "const",
  				"value": 4
  			}
  		}
  	}
  ]
  ```

### 1.4 Economy & Creation Cost
- **Currency Verification**: No references to `energy` or `minerals` exist in the server source code (`server/`). The only currency is `gems` (`balance_` in Core, Unit, Deposit, GemPile).
- **File**: `server/src/object/Core.cpp`, Lines 3–12:
  ```cpp
  Core::Core(unsigned int teamId)
  	: Object(Config::game().coreHp, ObjectType::Core), balance_(Config::game().initialGems), team_id_(teamId),
  	  spawn_cooldown_(0)
  {
  }

  void Core::tick(unsigned long long tickCount)
  {
  	if (tickCount < Config::game().idleIncomeTimeOut) setBalance(getBalance() + Config::game().idleIncome);
  }
  ```
- **File**: `server/src/object/Deposit.cpp`, Lines 3–30:
  ```cpp
  Deposit::Deposit() : Object(Config::game().depositHp, ObjectType::Deposit), balance_(Config::game().depositIncome) {}
  // ...
  void Deposit::damage(Object *attacker, unsigned int damage)
  {
  	// ...
  	this->setHP(this->getHP() - damage);
  	if (this->getHP() > 0) return;
  	int balance = this->getBalance();
  	Position pos = Board::instance().getObjectPositionById(this->getId());
  	Board::instance().removeObjectById(this->getId());
  	Board::instance().addObject<GemPile>(GemPile(balance), pos);
  }
  ```
- **File**: `server/src/object/GemPile.cpp`, Lines 15–32:
  ```cpp
  void GemPile::damage(Object *attacker, unsigned int damage)
  {
  	(void)damage;
  	if (attacker->getType() != ObjectType::Unit) return;
  	Unit *unitAttacker = (Unit *)attacker;

  	const unsigned int collected = std::min(balance_, unitAttacker->getRemainingGemsCapacity());
  	unitAttacker->addBalance(collected);
  	balance_ -= collected;

  	Stats::instance().inc(stat_keys::gems_gained, collected);
  	if (balance_ > 0) return;
  	Board::instance().removeObjectById(id_);
  }
  ```
- **File**: `server/src/game/Game.cpp`, Lines 276–284:
  ```cpp
  if (obj.getType() == ObjectType::Unit)
  {
  	Position objPos = Board::instance().getObjectPositionById(obj.getId());
  	unsigned int unitBalance = ((Unit &)obj).getBalance();

  	Board::instance().removeObjectById(obj.getId());

  	if (unitBalance > 0) Board::instance().addObject<GemPile>(GemPile(unitBalance), objPos);
  }
  ```

### 1.5 Movement, Collision, Combat & Full Gameloop Order
- **Full 8-Step Gameloop Order** in `server/src/game/Game.cpp`, Lines 165–361:
  - Step 0 (Lines 166–207): Handle Debug Info (`debugDataPackets`). Validates unit debug paths.
  - Step 1 (Lines 210–248): Execute Actions in 4 phases (`TRANSFER_GEMS` -> `ATTACK` -> `MOVE` -> `CREATE`). Intra-phase actions sorted by team ID then shuffled using `mt19937_64(seed + tick)`.
  - Step 2 (Lines 250–254): Tick Objects (`obj.tick(tick)`). `Core::tick` applies idle gem income if `tick < idleIncomeTimeOut`.
  - Step 3 (Lines 255–290): Delete Dead Objects (`HP <= 0`). Units drop carried gems as `GemPile` and are removed; walls are removed. **Cores are NOT removed here** (`else if (obj.getType() != ObjectType::Core)`).
  - Step 4 (Lines 292–303): Check Timeout (`tick >= timeoutTicks` or `time >= timeoutMs`). Calls `killWorstPlayerOnTimeout()` using tie-breaker order: (1) least Core HP -> (2) least total Unit HP -> (3) random pick.
  - Step 5 (Lines 305–314): Advance Cooldowns (`Unit::tickActionCooldown()` and `Core::tickSpawnCooldown()`).
  - Step 6 (Lines 316–318): Send Authoritative State (`sendState`) with diffs, replay recording, and team-specific `errors` array.
  - Step 7 (Lines 323–353): Remove Dead Cores (`Core::HP <= 0`). Disconnect bridges and record death reason `CORE_DESTROYED`.
  - Step 8 (Lines 355–360): Clean up debug info and paths on all objects.
- **File**: `server/src/action/MoveAction.cpp`, Lines 36–42:
  ```cpp
  if (!target_.isValid(Config::game().gridSize)) return "target position out of bounds";
  if (Board::instance().getObjectAtPos(target_)) return "invalid target position. should be empty";
  if (target_.distance(Board::instance().getObjectPositionById(unit->getId())) > 1)
  	return "targeted position too far away";

  Board::instance().moveObjectById(unit->getId(), target_);
  unit->resetActionCooldown();
  ```
- **File**: `server/src/action/AttackAction.cpp`, Lines 50–73:
  ```cpp
  unsigned int damage = 1;
  if (obj->getType() == ObjectType::Unit)
  	damage = unit->getProperties().at(UnitProperty::DAMAGE_UNIT);
  else if (obj->getType() == ObjectType::Core)
  	damage = unit->getProperties().at(UnitProperty::DAMAGE_CORE);
  else if (obj->getType() == ObjectType::Deposit)
  	damage = unit->getProperties().at(UnitProperty::DAMAGE_OBJECT);
  else if (obj->getType() == ObjectType::Wall)
  	damage = unit->getProperties().at(UnitProperty::DAMAGE_OBJECT);

  int damageReductionPercent = 0;
  if (obj->getType() == ObjectType::Unit)
  {
  	Unit *targetUnit = (Unit *)obj;
  	damageReductionPercent = targetUnit->getProperties().at(UnitProperty::DAMAGE_REDUCTION_PERCENT);
  }
  const unsigned int baseDamage = damage;
  const int armorPercent = std::clamp(damageReductionPercent, 0, 100);
  const unsigned int roundedDamage =
  		static_cast<unsigned int>(std::round(static_cast<double>(baseDamage) * (100 - armorPercent) / 100.0));
  damage = baseDamage == 0 ? 0 : std::max(1u, roundedDamage);

  obj->damage(unit, damage);
  ```
- **Map Symmetry vs Team Assignment**:
  - `server/src/config/worldgen/SparseWorldGenerator.cpp:123–160`: Upper-half objects mirrored via $(N - 1 - x, N - 1 - y)$ providing 180° rotational symmetry.
  - `server/src/game/Game.cpp:10` & `server/inc/utils/Utils.h:20–25`: `shuffle_vector(team_ids)` uses `std::random_device` (`std::mt19937 g(rd())`), meaning map seed does NOT fix which team starts at `(0, 0)` vs `(19, 19)`.

### 1.6 Server Error Signals & Failure Strings
- **Error Formatting Envelope** (`server/src/game/Game.cpp:229–232`):
  `"Tick " + std::to_string(tick - 1) + ": Action Failure: " + Action::getActionName(type) + ": " + err + " (" + actionJson.dump() + ")"`
- **Schema Parse Error Envelope** (`server/src/action/Action.cpp:47, 66`):
  `"Tick " + std::to_string(tick - 1) + ": Action Failure: Parsing Problem: " + t + ": " + e.what()`
  `"Tick " + std::to_string(tick - 1) + ": Action Failure: Parsing Problem: unknown type '" + t + "' (" + actionJson.dump() + ")"`
- **Debug Error Envelope** (`server/src/game/Game.cpp:189–192`):
  `"Tick " + std::to_string(tick - 1) + ": Debug Error: Only units can have debug paths. Object ID " + std::to_string(objectId) + " is not a unit."`
- **Move Action Errors** (`server/src/action/MoveAction.cpp`):
  1. Line 28: `"invalid or non-existing unit"`
  2. Line 31–33: `"unit is on action cooldown (action cooldown should be 0 or less to perform an action) or has already moved this tick"`
  3. Line 34: `"unit does not belong to your team"`
  4. Line 36: `"target position out of bounds"`
  5. Line 37: `"invalid target position. should be empty"`
  6. Line 39: `"targeted position too far away"`
- **Attack Action Errors** (`server/src/action/AttackAction.cpp`):
  1. Line 32: `"invalid or non-existing unit"`
  2. Line 36: `"invalid or non-existing target"`
  3. Line 39: `"unit is too far away"`
  4. Line 40–41: `"unit is on action cooldown (action cooldown should be 0 or less to perform an action)"`
  5. Line 42: `"unit does not belong to your team"`
  6. Line 45: `"no object at target position"`
- **Transfer Gems Action Errors** (`server/src/action/TransferGemsAction.cpp`):
  1. Line 61: `"invalid amount"`
  2. Line 62: `"target position is out of bounds"`
  3. Line 65: `"no object with source_id " + std::to_string(source_id_)`
  4. Line 33: `"only units can drop gems on the floor"`
  5. Line 35, 37: `"invalid drop position"`
  6. Line 40: `"can't drop gems from another team"`
  7. Line 41–42: `"unit is on action cooldown (action cooldown should be 0 or less to perform an action)"`
  8. Line 45–46: `"insufficient gems - has " + std::to_string(srcUnit->getBalance()) + ", tried to transfer " + std::to_string(amount_)`
  9. Line 70: `"can't transfer gems to yourself"`
  10. Line 74: `"invalid source object type"`
  11. Line 77–78: `"invalid destination object type; transfer gems only to an object that can hold gems (e.g. cores, units, gem piles)."`
  12. Line 86: `"invalid transfer distance; objects aren't as close as possible in Manhattan distance"`
  13. Line 92–94: `"destination unit has insufficient capacity - can receive " + std::to_string(dstUnit->getRemainingGemsCapacity()) + ", tried to transfer " + std::to_string(amount_)`
  14. Line 100: `"destination core has insufficient capacity"`
  15. Line 106: `"destination gem pile has insufficient capacity"`
  16. Line 113: `"can't transfer gems from another team core"`
  17. Line 115–116: `"insufficient gems - has " + std::to_string(srcCore->getBalance()) + ", tried to transfer " + std::to_string(amount_)`
  18. Line 122: `"can't transfer gems from another team unit"`
  19. Line 123: `"unit is on action cooldown (action cooldown should be 0 or less)"`
  20. Line 124–126: `"insufficient gems - has " + std::to_string(srcUnit->getBalance()) + ", tried to transfer " + std::to_string(amount_)`
- **Create Action Errors** (`server/src/action/CreateAction.cpp`):
  1. Line 82–83: `"core spawn cooldown not done - wait " + std::to_string(core->getSpawnCooldown()) + " ticks"`
  2. Line 86: `"no valid position found - entire grid is filled up"`
  3. Line 90–92: `"too many components - has " + std::to_string(components_.size()) + ", max is " + std::to_string(Config::game().maxComponentsPerUnit)`
  4. Line 100: `"invalid component \"" + componentId + "\""`
  5. Line 106–107: `"insufficient funds - has " + std::to_string(core->getBalance()) + ", needs " + std::to_string(unitCost)`
  6. Line 112 / `game.config.json`:
     - `"Your action cooldown may not be less than your total attacks added together"`
     - `"You may not have more than 4 armor components"`

---

## 2. Logic Chain

1. **Idle-Ready Negative Cooldown vs. Standing Cooldown Timeout**:
   - `Unit.cpp:19–36` shows that when `action_cooldown_ <= 0`, `action_cooldown_` continues to decrement by 1 each tick.
   - Guard checks in all actions (`MoveAction.cpp:31`, `AttackAction.cpp:40`, `TransferGemsAction.cpp:41, 123`) check `if (action_cooldown > 0) return error;`. Therefore, negative values (`<= 0`) are legally ready to act.
   - However, line 34–35 bounds this: `if (-action_cooldown_ > (long long)unitStandingCooldownLimit) action_cooldown_ = unitStandingCooldownLimit;`. If a unit is kept idle past `unitStandingCooldownLimit` (configured as 50 ticks in `game.config.json:6`), its cooldown is forcefully set to `+50`. The unit suddenly becomes not ready for 50 ticks!

2. **Why Transfer + Move on the Same Tick Violates Cooldown**:
   - `Utils.cpp:14–27` orders actions into buckets: `TRANSFER_GEMS` (0) -> `ATTACK` (1) -> `MOVE` (2) -> `CREATE` (3).
   - In Step 1 of `Game::tick`, bucket 0 executes before bucket 2.
   - When `TransferGemsAction::execute` runs, line 128 (and line 49 for `dropGems`) executes `srcUnit->resetActionCooldown()`.
   - `resetActionCooldown()` calculates `calcActionCooldown()` which is $\ge \text{baseActionCooldown} \ge 1$ (default 3).
   - When bucket 2 executes on the same tick, `MoveAction::execute` checks `unit->getActionCooldown() > 0`. Because cooldown was reset in Phase 0, it is now positive, triggering verbatim error: `"unit is on action cooldown (action cooldown should be 0 or less to perform an action) or has already moved this tick"`.

3. **Why Speed Cannot Combine with Any Attack Component, and 2x Speed is Always Illegal**:
   - Default `unitDefaultProperties` has `baseActionCooldown: 3`, `damageUnit: 0`, `damageCore: 0`, `damageObject: 0`.
   - Adding `speed` applies property modification `-2` to `baseActionCooldown`, reducing it to 1.
   - Adding any attack component (`unit_attack`, `core_attack`, or `mining`) adds `+3` damage to its respective property.
   - `ComponentLogic::getInvalidConditionMessage` evaluates `game.config.json:132-160`: `baseActionCooldown < (damageUnit + damageCore + damageObject)`.
   - For `speed` + 1 attack: `baseActionCooldown` is 1, total attack is 3. Since $1 < 3$, it triggers `"Your action cooldown may not be less than your total attacks added together"`.
   - For 2x `speed` without attack: `baseActionCooldown` becomes $3 - 2 - 2 = -1$. Total attack is 0. Since $-1 < 0$, it ALSO evaluates to true and triggers the same invalidation error! Thus, no unit can ever have 2 speed components.

4. **Accurate Economy Architecture & Price Derivation**:
   - Grep search confirms zero occurrences of `energy` or `minerals` across engine and client library; the engine uses strictly `gems`.
   - `CreateAction.cpp:95-104` computes unit cost dynamically: `unitCost = defaultUnitCost` (20) $+ \sum \text{component.cost}$.
   - Deposits (HP 50, gems 100) take damage only from `damageObject` (`mining`). Upon destruction, they instantly transform into a `GemPile` of 100 gems at the same tile (`Deposit.cpp:29`).
   - `GemPile.cpp:15–32` ignores attack damage (`(void)damage`) and transfers `min(balance, remainingCapacity)` to the attacking unit. Thus, any unit (even with 0 damage stats) can harvest a gem pile with one attack.
   - Carried gems add integer steps to action cooldown: `steps = balance / step` (`Unit.cpp:41`). With default `gemsPerCooldownStep = 10`, 35 gems add 3 ticks of cooldown penalty.

5. **Timing of Movement onto Destroyed Enemies & Same-Tick Occupancy**:
   - In `Game::tick`, actions execute in Step 1 (Actions), but dead objects with `HP <= 0` are deleted in Step 3 (`DELETE DEAD OBJECTS`, lines 255–290).
   - If an enemy unit or wall is killed during Step 1 Phase 1 (`ATTACK`), it has `HP <= 0`, but remains in `Board::objects_`.
   - When Step 1 Phase 2 (`MOVE`) evaluates, `Board::getObjectAtPos(target_)` still returns the dead unit or wall. `MoveAction.cpp:37` checks `if (Board::instance().getObjectAtPos(target_))` and rejects the move with `"invalid target position. should be empty"`.
   - Therefore, a friendly unit CANNOT advance into a tile of an enemy unit or wall killed on the same tick. It must wait until the next tick after Step 3 has purged the dead object.
   - Conversely, when a `GemPile` is fully depleted in Phase 1, `GemPile.damage` calls `Board::removeObjectById(id_)` immediately (`GemPile.cpp:31`). Thus, a different friendly unit can move onto the vacated gem pile tile in Phase 2 of the same tick.

---

## 3. Caveats

1. **Jigsaw World Generator**: The active configuration in `bots/configs/fake_demo/game.config.json` specifies `"worldGenerator": "sparse"`. The `"jigsaw"` world generator template files exist in `server/data/jigsaw-templates/`, but were not active in the tournament default config.
2. **Web/Live Environment Overrides**: Environment variables `PLAYER_<teamId>_NAME` and `SERVER_PORT` can override team names and port 4444 (`server/src/main.cpp:50, 115`).
3. **Internal ABI vs Client Lib**: Bot code must use public client headers (`bots/c/client_lib/inc/core_lib.h`). Modifying internal struct layouts breaks linkage against pre-compiled tournament archives.

---

## 4. Conclusion

The 42 CORE game engine operates on strict, deterministic, and verifiable mathematical rules:
1. Cooldown readiness is `action_cooldown <= 0`, but standing idle beyond `min(calcActionCooldown, 50)` ticks triggers a reset to positive cooldown penalty.
2. Action resolution has 4 phases: `transfer_gems` (0) -> `attack` (1) -> `move` (2) -> `create` (3). Transfer resets cooldown to $\ge 1$, so any same-tick move fails.
3. Speed (-2 cooldown) cannot pair with any attack (+3 damage) because $1 < 3$ violates $AC \ge \text{Attacks}$; 2x speed is intrinsically invalid because $-1 < 0$. Armor is hard-capped at 4 components.
4. The economy is single-currency (`gems`). Creation cost is dynamic ($20 + \sum \text{comp.cost}$). Deposits require `mining` to break; gem piles can be harvested by any unit in 1 attack.
5. Destroyed units and walls are not cleared from the board until Step 3, blocking movement into their tiles during Step 1 Phase 2.
6. Team corner assignment `(0, 0)` vs `(19, 19)` is non-deterministically randomized per match using `std::random_device`, regardless of seed.

---

## 5. Verification Method

To independently verify these findings on the actual codebase:

1. **Verify 4-Phase Ordering & No Build Action**:
   ```bash
   grep -n "phase_of" server/src/utils/Utils.cpp
   # Inspect lines 12-27: verifies phases 0, 1, 2, 3 and absence of build.
   ```
2. **Verify Standing Cooldown Penalty & Formula**:
   ```bash
   grep -n -A 25 "tickActionCooldown" server/src/object/Unit.cpp
   # Inspect lines 19-50: verifies negative decrement and penalty reset.
   ```
3. **Verify Transfer Cooldown Reset**:
   ```bash
   grep -n -C 3 "resetActionCooldown" server/src/action/TransferGemsAction.cpp
   # Inspect lines 49, 128: confirms both dropGems and transfer reset cooldown.
   ```
4. **Verify Speed + Attack Invalidation & Dynamic AST Evaluator**:
   ```bash
   grep -n -A 35 "invalidConditions" bots/configs/fake_demo/game.config.json
   grep -n -A 25 "getInvalidConditionMessage" server/src/utils/ComponentLogic.cpp
   ```
5. **Verify Single Currency (Gems Only)**:
   ```bash
   grep -rn "mineral" server/
   grep -rn "energy" server/
   # Returns 0 results.
   ```
6. **Verify Server Execution & Help**:
   ```bash
   ./server/server
   # Outputs usage message confirming binary health.
   ```

---

## Features Discovered

| # | Category | Feature | Description | Inputs | Outputs | Error Behavior | Discovered Via |
|---|----------|---------|-------------|--------|---------|----------------|----------------|
| 1 | Action Readiness | Idle Negative Cooldown Decrement | Cooldown decrements below 0 down to `-unitStandingCooldownLimit` when unit stands idle | Elapsed ticks without action | Signed negative `action_cooldown_` | If idle $> \text{limit}$, resets to `+unitStandingCooldownLimit` | `server/src/object/Unit.cpp:19–36` |
| 2 | Action Readiness | Cooldown Legality Guard | Unit can act if and only if `action_cooldown <= 0` | Unit state | Boolean readiness | Rejects action with cooldown error if $> 0$ | `server/src/action/MoveAction.cpp:31`, `AttackAction.cpp:40` |
| 3 | Pipeline | 4-Phase Sequential Action Execution | Actions partitioned and resolved in strict phases: 0: Transfer, 1: Attack, 2: Move, 3: Create | Parsed client actions | Ordered execution stream | Invalid types discarded with parse warning | `server/src/utils/Utils.cpp:12–27` |
| 4 | Pipeline | Deterministic Intra-Phase Shuffle | Actions within each phase sorted by team ID then shuffled using `mt19937_64(seed + tick)` | Seed, tick, team IDs | Shuffled bucket | None | `server/src/utils/Utils.cpp:35–46` |
| 5 | Action Safety | Transfer Gem Cooldown Reset | Transferring gems (or dropping gems) immediately resets unit cooldown to `calcActionCooldown()` | Source unit ID, target pos, amount | Updated balances, cooldown reset | Move/travel on same tick rejected with cooldown error | `server/src/action/TransferGemsAction.cpp:49, 128` |
| 6 | Unit Design | AST Invalid Condition Engine | Arbitrary JSON expressions evaluated dynamically against unit properties and component counts | Config condition JSON, unit properties | Boolean validity | Returns configured custom message on failure | `server/src/utils/ComponentLogic.cpp:31–122` |
| 7 | Unit Design | Attack $\le$ Cooldown Validation Rule | Invalidation rule: `baseActionCooldown < (damageUnit + damageCore + damageObject)` | Unit components | Valid/invalid decision | Returns `"Your action cooldown may not be less than your total attacks added together"` | `bots/configs/fake_demo/game.config.json:132–160` |
| 8 | Unit Design | Armor Quantity Limit | Invalidation rule: `component_count("armor") > 4` | Unit components | Valid/invalid decision | Returns `"You may not have more than 4 armor components"` | `bots/configs/fake_demo/game.config.json:161–175` |
| 9 | Economy | Dynamic Unit Creation Cost | Cost formula: $\text{unitCost} = 20 + \sum \text{cost}(c)$ | Component list | Gem deduction from Core | Rejects with `"insufficient funds..."` if `core.balance < unitCost` | `server/src/action/CreateAction.cpp:95–107` |
| 10 | Economy | Passive Core Idle Income | Core receives $+3$ gems every tick for ticks $0 \le t < 100$ | Current tick count | Added core balance | None | `server/src/object/Core.cpp:9–12` |
| 11 | Economy | Universal GemPile Collection | Attacking a `GemPile` transfers `min(balance, remainingCapacity)` regardless of attacker damage stats | Attacker unit, target GemPile | Transferred gems, GemPile deleted if empty | None; works with 0 damage | `server/src/object/GemPile.cpp:15–32` |
| 12 | Economy | Deposit Breakdown to GemPile | Deposits have 50 HP, take damage from `damageObject`. At 0 HP, destroyed and replaced with 100-gem GemPile | Attack with `damageObject > 0` | Deposit replaced with GemPile | Zero damage if `damageObject == 0` | `server/src/object/Deposit.cpp:15–30` |
| 13 | Economy | Death Gem Drop | Dead units carrying gems drop a `GemPile` of exact carried balance on their death tile | Unit destruction | Spawns `GemPile(balance)` at death pos | None | `server/src/game/Game.cpp:276–284` |
| 14 | Movement | Cardinal Manhattan Movement | Units move strictly distance 1 (`\|dx\| + \|dy\| == 1`) to empty cells | Target position | Updated position, cooldown reset | Returns `"target position out of bounds"` or `"should be empty"` or `"too far away"` | `server/src/action/MoveAction.cpp:36–42` |
| 15 | Movement | Delayed Dead Object Purge | Dead units and walls remain on board through Phase 2 (Move) and are deleted only in Step 3 | Game loop step sequence | Board cleanup | Prevents advancing onto tiles of units/walls killed on same tick | `server/src/game/Game.cpp:255–290` |
| 16 | Combat | Damage & Armor Formula | Base damage selected by target type; armor reduces damage: $\text{round}(\text{base} \cdot (100 - \text{armor}) / 100)$; min damage 1 if base $> 0$ | Attacker, target, armor % | HP reduction | Zero damage dealt if base is 0 | `server/src/action/AttackAction.cpp:50–73` |
| 17 | Core | Core Health & Destruction | Cores have 99 HP, 0% armor, take damage from `damageCore`. HP $\le 0$ kept for 1 state broadcast then disconnected | Attack with `damageCore` | Core HP reduction, death resolution | Pure unit attackers deal 0 damage to Core | `server/src/action/AttackAction.cpp:54`, `Game.cpp:323–353` |
| 18 | Match | Timeout Resolution Tie-Breaker | When tick $\ge 30000$ or ms $\ge 3000000$, kills worst player: (1) least Core HP $\to$ (2) least Unit HP $\to$ (3) random | Match tick / elapsed time | Eliminates one Core per tick | None | `server/src/game/Game.cpp:363–460` |
| 19 | Worldgen | Rotational Map Symmetry | Sparse world generator places upper-half objects and 180° mirrors across center $(N-1-x, N-1-y)$ | Seed, config parameters | Symmetric board entities | None | `server/src/config/worldgen/SparseWorldGenerator.cpp:123–160` |
| 20 | Match | Team Corner Non-Determinism | Teams shuffled onto core positions $(0,0)$ and $(19,19)$ using `std::random_device`, ignoring map seed | Team IDs | Core team positions | None | `server/src/game/Game.cpp:10`, `Utils.h:20–25` |

---

## Edge Cases

| # | Feature | Input | Observed Behavior |
|---|---------|-------|-------------------|
| 1 | Action Readiness | Unit idle for $> 50$ consecutive ticks | Cooldown decrements to -51, exceeding `-unitStandingCooldownLimit` (50). Server sets `action_cooldown_ = 50`. Unit forfeits readiness and is frozen for 50 ticks (`Unit.cpp:34–35`). |
| 2 | Action Readiness | Action issued when `action_cooldown_ < 0` (e.g. -5) | Action executes successfully. `action_cooldown <= 0` passes the server guard (`MoveAction.cpp:31`). |
| 3 | Transfer Reset | Unit issues `transfer_gems` and `move` on same tick | `transfer_gems` executes in Phase 0, resetting cooldown to $\ge 1$. `move` in Phase 2 fails with `"unit is on action cooldown... or has already moved this tick"` (`MoveAction.cpp:31`). |
| 4 | Transfer Reset | Unit A transfers gems to Unit B on same tick Unit B moves | Unit A's cooldown resets to $\ge 1$. Unit B's cooldown is NOT reset by receiving gems (`TransferGemsAction.cpp:136–140`). Unit B moves successfully if it was ready. |
| 5 | Unit Design | Spawning unit with 2x `speed` components | `baseActionCooldown` becomes $3 - 2 - 2 = -1$. Evaluator checks $-1 < 0$ (total attack), which is true. Creation rejected with `"Your action cooldown may not be less than your total attacks added together"` (`game.config.json:134`). |
| 6 | Unit Design | Spawning unit with 1x `speed` + 1x `mining` | `baseActionCooldown` is 1, total attack is 3. $1 < 3$ evaluates to true. Rejected with `"Your action cooldown may not be less than your total attacks added together"`. No combat unit can equip speed. |
| 7 | Unit Design | Spawning unit with 5x `armor` components | Exceeds limit of 4 armor components. Creation rejected with `"You may not have more than 4 armor components"` (`game.config.json:162`). |
| 8 | Economy | Attacking `GemPile` with 0 attack damage | `GemPile::damage` ignores damage argument (`(void)damage`) and transfers `min(balance, remainingCapacity)` to attacker (`GemPile.cpp:17–24`). Gem collection succeeds with 0 damage. |
| 9 | Economy | Attacking `Deposit` with 0 `damageObject` | `baseDamage` is 0 (`AttackAction.cpp:55`). Final damage is 0. Deposit takes 0 damage and is not broken (`Deposit.cpp:21`). Attacker cooldown is still reset. |
| 10 | Movement | Friendly Unit C tries to move into tile of Enemy B killed in same tick Phase 1 | Enemy B takes lethal damage in Phase 1 (`ATTACK`), but dead objects are purged in Step 3. During Phase 2 (`MOVE`), Enemy B still occupies the tile. Unit C's move fails with `"invalid target position. should be empty"` (`MoveAction.cpp:37`). |
| 11 | Movement | Friendly Unit B moves into tile of GemPile depleted in same tick Phase 1 | GemPile depleted in Phase 1 is immediately removed from board (`GemPile.cpp:31`). Tile is empty during Phase 2. Unit B successfully moves into the tile. |
| 12 | Movement | Two opposing units attempt to move into same empty tile on same tick | Server shuffles actions within Phase 2 using `mt19937_64(seed + tick)`. First unit in shuffle moves into the tile; second unit fails with `"invalid target position. should be empty"` (`MoveAction.cpp:37`). |
| 13 | Movement | Two friendly units attempt to swap tiles | Unit 1 tries to move to Tile 2 (occupied by Unit 2) and fails. Unit 2 tries to move to Tile 1 (occupied by Unit 1) and fails. Mutual tile swaps always fail. |
| 14 | Transfer Gems | Transferring gems to empty tile (drop gems) | Server detects `!dstObj` and invokes `dropGems` (`TransferGemsAction.cpp:31–57`). Spawns a new `GemPile(amount)` at adjacent target tile, deducts gems from unit, and resets unit cooldown. Fails if distance $> 1$ or distance $== 0$. |
| 15 | Transfer Gems | Transfer gems distance when adjacent tiles blocked | Server calculates `maxDist = dstPos.distance(findFirstEmptyGridCell(dstPos))` (`TransferGemsAction.cpp:84`). If all 4 cardinal tiles adjacent to destination are blocked, `maxDist` expands to 2, allowing distance-2 transfer! |
| 16 | Combat | Attacking unit with 100% damage reduction (`armor`) | If attacker has `baseDamage > 0`, line 71 enforces `damage = baseDamage == 0 ? 0 : std::max(1u, roundedDamage)`. Attacker deals at least 1 damage. No unit is completely invincible. |
| 17 | Combat | Unit attacking Core with 0 `damageCore` (e.g. pure fighter) | `baseDamage = 0`. Line 71 leaves `damage = 0`. Core takes strictly 0 damage (`AttackAction.cpp:71`). |
| 18 | Combat | Friendly fire attack against ally unit | `AttackAction.cpp` checks distance and cooldown, but does NOT check team ownership of target. Target ally unit takes damage, incrementing `stat_keys::damage_self` (`Unit.cpp:58`). |
| 19 | Core | Transferring gems from Core while `spawn_cooldown > 0` | Core `spawn_cooldown_` only gates `CreateAction.cpp:82`. `TransferGemsAction.cpp:110–118` does NOT check or reset `spawn_cooldown_`. Core can transfer gems freely during spawn cooldown. |
| 20 | Match | Seed consistency vs team corner assignment | Setting a fixed map seed fixes the layout of deposits, gem piles, and walls, but does NOT fix which team starts at `(0, 0)` vs `(19, 19)` because `Game::Game` shuffles team IDs using `std::random_device` (`Utils.h:20–25`). |
