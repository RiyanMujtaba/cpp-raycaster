CXX     = g++
CXXFLAGS = -std=c++17 -O2 -Wall $(shell sdl2-config --cflags)
LIBS     = $(shell sdl2-config --libs)

raycaster: main.cpp
	$(CXX) $(CXXFLAGS) main.cpp -o raycaster $(LIBS)

clean:
	rm -f raycaster
