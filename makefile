CC = g++

SRC_DIR = src
INC_DIR = inc
MISC_DIR = misc

# all: asembler linker emulator
all: asembler linker emulator

asembler: $(SRC_DIR)/parser.cpp $(SRC_DIR)/lexer.cpp $(SRC_DIR)/assembler.cpp $(INC_DIR)/util.hpp
	$(CC) -o $@ $^

linker:	$(SRC_DIR)/linker.cpp $(INC_DIR)/util.hpp
	$(CC) -o $@ $^

emulator:	$(SRC_DIR)/emulator.cpp $(INC_DIR)/util.hpp
	$(CC) -o $@ $^

$(SRC_DIR)/lexer.cpp: $(MISC_DIR)/lexer.l
	flex -o $@ $<

$(SRC_DIR)/parser.cpp: $(MISC_DIR)/parser.y
	bison -d -o $@ $<

clean:
	rm -rf asembler linker emulator $(SRC_DIR)/lexer.cpp $(SRC_DIR)/parser.cpp $(INC_DIR)/lexer.hpp $(INC_DIR)/parser.hpp *.o *.txt *.hex


