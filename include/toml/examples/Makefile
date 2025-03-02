CPP := g++ 
GEN_SRC := gen.cpp
PARSE_SRC := parse.cpp

GEN_BIN := gen 
GEN_LIBS := -lavcodec -lavutil -lavformat
PARSE_BIN := parse
MAKE := make

gen:
	$(CPP) -o $(GEN_BIN) $(GEN_SRC) $(GEN_LIBS)

parse:
	$(CPP) -o $(PARSE_BIN) $(PARSE_SRC)

all:
	$(MAKE) gen 
	$(MAKE) parse

clean:
	@rm -f gen parse
