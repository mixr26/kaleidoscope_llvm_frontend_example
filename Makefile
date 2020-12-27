TARGET	= kaleidoscope_cc

Q ?= @
PREFIX ?= 

INC_DIR = inc
OUT_DIR = out
BIN_DIR = bin
SRC_DIR = src
LIB_DIR = lib
OBJ_DIR = obj

SRC 	= $(wildcard $(SRC_DIR)/*.cpp)
OBJ 	= $(patsubst $(SRC_DIR)/%, $(OBJ_DIR)/%, $(SRC:.cpp=.o))

CXX = ${PREFIX}g++
CC  = ${PREFIX}gcc
LD  = ${PREFIX}ld
AS  = ${PREFIX}gcc -x assembler-with-cpp
CP  = ${PREFIX}objcopy
OD  = ${PREFIX}objdump
SZ  = ${PREFIX}size

CXXFLAGS = -Wall -std=c++17 -O0 -g
CXXFLAGS += -I./$(INC_DIR)

LFLAGS  = -L./$(OUT_DIR)/$(LIB_DIR)

all : mkobjdir $(TARGET)

$(TARGET) : $(OBJ)
	@echo "  [LD]      $@"
	$(Q)$(CXX) -o $(OUT_DIR)/$(BIN_DIR)/$@ $^ $(LFLAGS)

$(OBJ_DIR)/%.o : $(SRC_DIR)/%.cpp
	@echo "  [CXX]     $<"
	$(Q)$(CXX) $(CXXFLAGS) -c  $< -o $@

help :
	@echo "  [SRC]:      $(SRC)"
	@echo
	@echo "  [OBJ]:     $(OBJ)"

clean :
	@echo "  [RM]    $(OBJ)"
	@$(RM) $(OBJ)
	@echo
	@echo "  [RM]     $(TARGET) "
	@$(RM) $(OUT_DIR)/$(BIN_DIR)/$(TARGET)

mkobjdir :
	@mkdir -p obj

.PHONY : all run deploy help clean formatsource mkobjdir
