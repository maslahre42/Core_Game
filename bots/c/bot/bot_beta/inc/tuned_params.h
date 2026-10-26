#ifndef TUNED_PARAMS_H
# define TUNED_PARAMS_H

/* Unit Role Names */
# define HARVESTER_NAME               "Harvester"
# define GUARDIAN_NAME                "Guardian"
# define INTERCEPTOR_NAME             GUARDIAN_NAME
# define BREAKER_NAME                 "Breaker"

/* Exact Component Costs */
/* Harvester: Base(20) + mining(15) = 35 */
# define HARVESTER_COST               35
/* Armored Guardian: Base(20) + unit_attack(15) + armor(30) + health(12) = 77 */
# define GUARDIAN_COST                77
# define INTERCEPTOR_COST             GUARDIAN_COST
/* Breaker: Base(20) + core_attack(15) + health(12) = 47 */
# define BREAKER_COST                 47

# define DEFAULT_GEMS_PER_STEP        10

/* Strategic Targets: Citadel / Macro & Siege */
# define TARGET_HARVESTERS            3
# define MIN_SUSTAINED_HARVESTERS     2
# define TARGET_GUARDIANS             3
# define TARGET_BREAKERS              2
# define COMBAT_READY_THRESHOLD       2
# define MINER_BASE_RETURN_GEMS       35
# define DEFENSE_PANIC_RADIUS         12
# define HARVESTER_EVACUATION_RADIUS  5
# define HARVESTER_DANGER_RADIUS      4
# define GUARDIAN_TETHER_RADIUS       6

/* Bipartite Allocator Weights */
# define WEIGHT_GEM_PILE_BONUS        2.5f
# define WEIGHT_DEPOSIT_BASE          100.0f
# define WEIGHT_MINER_DIST_PENALTY    2.0f
# define WEIGHT_CORE_DIST_PENALTY     1.0f
# define WEIGHT_ENEMY_THREAT_PENALTY  300.0f

/* Combat Scoring Weights */
# define SCORE_CORE_THREAT_WEIGHT     300
# define SCORE_CORE_INTERCEPT_BONUS   15000
# define SCORE_CORE_ATTACKER_BONUS    6000
# define SCORE_BODYGUARD_MINER_BONUS  4000
# define SCORE_LETHAL_BLOW_BONUS      800
# define SCORE_HUNT_MINER_BONUS       500
# define SCORE_PROTECT_MINER_BONUS    300
# define SCORE_ENEMY_MINER_GEMS       250
# define SCORE_DIST_PENALTY           15
# define SCORE_HP_FOCUS_WEIGHT        5

#endif
