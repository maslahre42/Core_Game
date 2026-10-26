SRCDIR = src
OBJDIR = build
SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(addprefix $(OBJDIR)/, $(notdir $(SRCS:.c=.o)))

CC ?= cc
CFLAGS ?= -Wall -Wextra -Werror -g -fsanitize=address
LDFLAGS ?= -lm
CLIENT_LIB_DIR ?= $(if $(wildcard /core/inc/core_lib.h),/core,$(if $(wildcard ../../client_lib/inc/core_lib.h),../../client_lib,/core))
INC = -I inc -I $(CLIENT_LIB_DIR)/inc
EXEC = bot

all: ensure_obj_dir $(EXEC)

run: all
	./$(EXEC)

$(EXEC): $(OBJS) $(CLIENT_LIB_DIR)/core_lib.a
	$(CC) $(CFLAGS) $(OBJS) $(CLIENT_LIB_DIR)/core_lib.a $(LDFLAGS) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INC)

ensure_obj_dir:
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -rf $(EXEC)

re: fclean all

.PHONY: all run clean fclean re $(EXEC) ensure_obj_dir
