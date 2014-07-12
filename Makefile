CXXFLAGS=-Wall -std=c++11

OBJECTS=main.o rpt-parser.o optimizer.o postscript-printer.o tape.o board.o \
	gcode-dispense-printer.o
rpt2pnp: $(OBJECTS)
	g++ $(CXXFLAGS) -o $@ $^

clean:
	rm -f *.o rpt2pnp
