#ifndef TUNED_PARAMS_H
# define TUNED_PARAMS_H

# define HARVESTER_NAME               "Harvester"
# define HUNTER_NAME                  "Hunter"
# define INTERCEPTOR_NAME             HUNTER_NAME
# define BREAKER_NAME                 "Breaker"

# define HARVESTER_COST               35
# define HUNTER_COST                  47
# define INTERCEPTOR_COST             HUNTER_COST
# define BREAKER_COST                 77

# define DEFAULT_GEMS_PER_STEP        10

# define TARGET_EARLY_MINERS          2
# define MAX_MINERS                   2
# define MINER_BASE_RETURN_GEMS       35
# define DEFENSE_PANIC_RADIUS         8

# define WEIGHT_GEM_PILE_BONUS        2.5f
# define WEIGHT_DEPOSIT_BASE          100.0f
# define WEIGHT_MINER_DIST_PENALTY    2.0f
# define WEIGHT_CORE_DIST_PENALTY     1.0f
# define WEIGHT_ENEMY_THREAT_PENALTY  200.0f

# define SCORE_CORE_THREAT_WEIGHT     300
# define SCORE_CORE_ATTACKER_BONUS    4000
# define SCORE_LETHAL_BLOW_BONUS      1000
# define SCORE_HUNT_MINER_BONUS       3200
# define SCORE_PROTECT_MINER_BONUS    1500
# define SCORE_ENEMY_MINER_GEMS       1200
# define SCORE_DIST_PENALTY           15
# define SCORE_HP_FOCUS_WEIGHT        6
# define SCORE_WOLFPACK_BONUS         2500

#endif
