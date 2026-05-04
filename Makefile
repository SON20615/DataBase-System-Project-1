CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
SRC_DIR  := src
BIN      := btree_exp

SRCS := $(SRC_DIR)/main.cpp $(SRC_DIR)/btree.cpp $(SRC_DIR)/bplustree.cpp \
        $(SRC_DIR)/student.cpp
HDRS := $(SRC_DIR)/btree.h  $(SRC_DIR)/bplustree.h $(SRC_DIR)/student.h

all: $(BIN)

$(BIN): $(SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) -o $(BIN) $(SRCS)

run: $(BIN)
	./$(BIN) student.csv

clean:
	rm -f $(BIN)

.PHONY: all run clean
