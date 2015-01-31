OPT = true
ROOT   = .
VM_ROOT = ../
WITH_SDL = true

ifneq ($(OPT), true)
JIT_FLAGS = -DJIT_LOG -DASMJIT_DEBUG
else
JIT_FLAGS = -DASMJIT_RELEASE
endif

USER_CFLAGS += -Wno-unused -std=c++11 $(JIT_FLAGS)

USER_OBJ = \
   $(JIT_OBJ) \
   $(OBJ)/main$(OBJ_SUFF) \
   $(OBJ)/translator_impl$(OBJ_SUFF) \
   $(OBJ)/interpreter_impl$(OBJ_SUFF) \
   $(OBJ)/jit_builder$(OBJ_SUFF) \
   $(OBJ)/runtime_environment$(OBJ_SUFF) \
   $(OBJ)/building_context$(OBJ_SUFF) \
   $(OBJ)/annotated_bytecode$(OBJ_SUFF) \
   $(OBJ)/stack_machine$(OBJ_SUFF)

include $(VM_ROOT)/common.mk
-include Makefile.dep
Makefile.dep: $(ROOT)/*.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(VM_INCLUDE)  -MM  $(ROOT)/*.cpp | sed 's,^\(.*\$(OBJ_SUFF)\),$(OBJ)/\1,' > $@ 

MATHVM = $(BIN)/mvm

all: $(MATHVM)

tests: all
	./tests/run.py -i interpreter $(OPT)
	./tests/run.py -j interpreter $(OPT)
	./tests/run.py -i given $(OPT)
	./tests/run.py -j given $(OPT)

$(MATHVM): $(OUT) $(MATHVM_OBJ) $(USER_OBJ)
	$(CXX) -o $@ $(MATHVM_OBJ) $(USER_OBJ) $(LIBS)
