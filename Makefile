# HybridSim build system

###################################################

CXXFLAGS=-m64 -DNO_STORAGE -Wall -DDEBUG_BUILD
OPTFLAGS=-m64 -O3


ifdef DEBUG
ifeq ($(DEBUG), 1)
OPTFLAGS= -O0 -g
endif
endif
CXXFLAGS+=$(OPTFLAGS)


HYBRID_LIB=../HybridSim

INCLUDES=-I$(HYBRID_LIB)
LIBS=-L${HYBRID_LIB} -lhybridsim -Wl,-rpath=${HYBRID_LIB} 

EXE_NAME=PCI_SSD
LIB_NAME=libpcissd.so

SRC = $(wildcard *.cpp)
OBJ = $(addsuffix .o, $(basename $(SRC)))
POBJ = $(addsuffix .po, $(basename $(SRC)))
REBUILDABLES=$(OBJ) ${POBJ} $(EXE_NAME) $(LIB_NAME)

all: ${EXE_NAME} 

lib: ${LIB_NAME} 

#   $@ target name, $^ target deps, $< matched pattern
$(EXE_NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) ${LIBS} -o $@ $^ 
	@echo "Built $@ successfully" 

${LIB_NAME}: ${POBJ}
	g++ -g -shared -Wl,-soname,$@ $(LIBS) -o $@ $^
	@echo "Built $@ successfully"

#include the autogenerated dependency files for each .o file
-include $(OBJ:.o=.dep)
-include $(POBJ:.po=.dep)

# build dependency list via gcc -M and save to a .dep file
%.dep : %.cpp
	@$(CXX) $(INCLUDES) -std=c++0x -M $(CXXFLAGS) $< > $@

# build all .cpp files to .o files
%.o : %.cpp
	g++ $(CXXFLAGS) $(INCLUDES) -std=c++0x -o $@ -c $<

%.po : %.cpp
	g++ $(INCLUDES) -std=c++0x -O3 -g -ffast-math -fPIC -DNO_OUTPUT -DNO_STORAGE -o $@ -c $<

clean: 
	rm -rf ${REBUILDABLES} *.dep out results *.log callgrind* nvdimm_logs