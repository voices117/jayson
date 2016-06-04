CXX=gcc
CXX_FLAGS=-g -std=c99 -Wall -Wpedantic

# Final binary
BIN=jayson
# Put all auto generated stuff to this build dir.
BUILD_DIR=../target

# List of all .c source files.
SRC_FILES=$(wildcard ../src/*.c)
OBJ_DIR=../int

# All .o files go to build dir.
OBJ=$(SRC_FILES:%.c=$(OBJ_DIR)/%.o)
# Gcc/Clang will create these .d files containing dependencies.
DEP=$(OBJ:%.o=%.d)


# Actual target of the binary - depends on all .o files.
$(BIN) : $(OBJ)
	-@mkdir $(BUILD_DIR)
	-@mkdir $(OBJ_DIR)

	# Just link all the object files.
	$(CXX) $(CXX_FLAGS) $^ -o $(BUILD_DIR)/$@

# Include all .d files
-include $(DEP)

# Build target for every single object file.
# The potential dependency on header files is covered
# by calling `-include $(DEP)`.
%.o : %.c
	mkdir -p $(@D)
	# The -MMD flags additionally creates a .d file with
	# the same name as the .o file.
	$(CXX) $(CXX_FLAGS) -MMD -c $< -o $@
