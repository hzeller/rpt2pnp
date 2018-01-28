CXXFLAGS=-O3 -Wall -Wextra -std=c++11 -Wno-unused-parameter -fno-exceptions

OBJECTS=main.o rpt-parser.o optimizer.o tape.o board.o \
        pnp-config.o gcode-machine.o postscript-machine.o

rpt2pnp: $(OBJECTS)
	g++ $(CXXFLAGS) -o $@ $^

clean:
	rm -f *.o rpt2pnp
