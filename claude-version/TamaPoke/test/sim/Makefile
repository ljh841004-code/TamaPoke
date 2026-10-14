# Simulador de equilibrio de combate (no es un test): make -C test/sim [N=800]
CXX ?= g++
N ?= 800
SRC := balance_sim.cpp ../shim/shim.cpp ../../battle.cpp
build/sim: $(SRC) ../../battle.h ../../dex.h
	@mkdir -p build
	$(CXX) -std=c++17 -O2 -I ../shim -o $@ $(SRC)
run: build/sim
	@./build/sim $(N)
.DEFAULT_GOAL := run
