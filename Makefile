CXXFLAGS=-Wall -std=c++11

OBJECTS=main.o rpt-parser.o optimizer.o tape.o board.o \
        pnp-config.o gcode-machine.o postscript-machine.o

rpt2pnp: $(OBJECTS)
	g++ $(CXXFLAGS) -o $@ $^

clean:
	rm -f *.o rpt2pnp
