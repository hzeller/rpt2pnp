CXXFLAGS=-Wall
rpt2pnp: main.o rpt-parser.o optimizer.o
	g++ -o $@ $^

clean:
	rm -f *.o rpt2pnp
