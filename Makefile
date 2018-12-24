CC=clang++
CFLAGS=-Wall -std=c++11 -g
LDLIBS=-lSDL

OBJ_DIR=objects
OBJS=$(patsubst %.cpp, $(OBJ_DIR)/%.o, $(wildcard *.cpp))
OBJS+=$(patsubst vendor/%.cpp, $(OBJ_DIR)/%.o, $(wildcard vendor/*.cpp))

main: $(OBJS)
	${CC} ${CFLAGS} $^ ${LDLIBS} -o $@

$(OBJ_DIR)/main.o: main.cpp
	@mkdir -p $(@D)
	${CC} ${CFLAGS} $< -c -o $@

$(OBJ_DIR)/%.o: %.cpp %.h
	@mkdir -p $(@D)
	${CC} ${CFLAGS} $< -c -o $@

$(OBJ_DIR)/%.o: vendor/%.cpp vendor/%.h
	@mkdir -p $(@D)
	${CC} ${CFLAGS} $< -c -o $@

.PHONY: run
run: main
	make && ./main

.PHONY: clean
clean:
	-rm -rf $(OBJ_DIR)
	-rm -rf *.bin *.o main
