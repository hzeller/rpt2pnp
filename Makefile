CXXFLAGS=-Wall -std=c++11

rpt2pnp: main.o rpt-parser.o optimizer.o postscript-printer.o rpt2part.o
	g++ $(CXXFLAGS) -o $@ $^

clean:
	rm -f *.o rpt2pnp
