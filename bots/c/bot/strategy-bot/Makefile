CC = cc
CFLAGS = -Wall -Wextra -Werror -g -fsanitize=address
CLIENT_LIB_DIR ?= /core
EXEC ?= bot
SOURCES = $(wildcard src/*.c)
OBJECTS = $(patsubst src/%.c,build/%.o,$(SOURCES))

all: $(EXEC)

$(EXEC): $(OBJECTS) $(CLIENT_LIB_DIR)/core_lib.a
	$(CC) $(CFLAGS) $(OBJECTS) $(CLIENT_LIB_DIR)/core_lib.a -lm -o $@

build/%.o: src/%.c inc/strategy.h inc/strategy_params.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinc -I$(CLIENT_LIB_DIR)/inc -MMD -MP -c $< -o $@

-include $(OBJECTS:.o=.d)

clean:
	rm -rf build

fclean: clean
	rm -f $(EXEC)

.PHONY: all clean fclean
