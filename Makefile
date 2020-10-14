.POSIX:

CC = gcc
CFLAGS = -Wall

SRCDIR = src
ODIR = obj

COMMON_SRCS := 
COMMON_SRC  := $(addprefix $(SRCDIR)/, $(COMMON_SRCS))
COMMON_OBJ  := $(addprefix $(ODIR)/, $(COMMON_SRCS:%.c=%.o))

WRITE_TARGET := writenoncanonical.o
WRITE_SRCS := writenoncanonical.c
WRITE_SRC  := $(addprefix $(SRCDIR)/, $(WRITE_SRCS))
WRITE_OBJ  := $(addprefix $(ODIR)/, $(WRITE_SRCS:%.c=%.o))

READ_TARGET := noncanonical.o
READ_SRCS := noncanonical.c
READ_SRC  := $(addprefix $(SRCDIR)/, $(READ_SRCS))
READ_OBJ  := $(addprefix $(ODIR)/, $(READ_SRCS:%.c=%.o))

default_rule: write read

$(COMMON_OBJ): $(ODIR)/%.o : $(SRCDIR)/%.c
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "Compiled "$<" successfully"

$(WRITE_OBJ): $(ODIR)/%.o : $(SRCDIR)/%.c
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "Compiled "$<" successfully"

$(READ_OBJ): $(ODIR)/%.o : $(SRCDIR)/%.c
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "Compiled "$<" successfully"

.PHONY: write
write: $(COMMON_OBJ) $(WRITE_OBJ)
	@$(CC) -o $(WRITE_TARGET) $(COMMON_OBJ) $(WRITE_OBJ) $(CFLAGS)
	@echo "Linking 'write' complete!"

.PHONY: read
read: $(COMMON_OBJ) $(READ_OBJ)
	@$(CC) -o $(READ_TARGET) $(COMMON_OBJ) $(READ_OBJ) $(CFLAGS)
	@echo "Linking 'read' complete!"

.PHONY: clean
clean:
	@rm -f $(COMMON_OBJ) $(WRITE_OBJ) $(READ_OBJ)
	@echo "Cleaned!"

.PHONY: remove
remove: clean
	@rm -f $(WRITE_TARGET) $(READ_TARGET)
	@echo "Everything was cleared"
