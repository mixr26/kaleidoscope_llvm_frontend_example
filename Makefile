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

CXX = ${PREFIX}clang++
CC  = ${PREFIX}clang
LD  = ${PREFIX}ld
AS  = ${PREFIX}gcc -x assembler-with-cpp
CP  = ${PREFIX}objcopy
OD  = ${PREFIX}objdump
SZ  = ${PREFIX}size
LLVM_CONFIG=/home/mix/Desktop/PL/llvm_install/bin/llvm-config
LLVM_INCLUDE=/home/mix/Desktop/PL/llvm_install/include/

CXXFLAGS = -Wall -std=c++11 -O0 -g
CXXFLAGS += -I./$(INC_DIR) -I$(LLVM_INCLUDE) $(shell $(LLVM_CONFIG) --cxxflags)

LFLAGS  = -L./$(OUT_DIR)/$(LIB_DIR) $(shell $(LLVM_CONFIG) --ldflags --system-libs --libs core mcjit orcjit native) -rdynamic

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
