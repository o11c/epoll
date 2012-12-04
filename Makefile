CXX = g++
# CXX = clang++
# CXX = clang++-libc++
CXXFLAGS = -g -O

override CC=${CXX} # for linking
override CPPFLAGS += -std=c++0x

main: main.o net.o cli.o chat.o conquest.o conquest-player.o
clean:
	rm -f *.o main
make.deps: $(wildcard *.cpp *.hpp)
	${CXX} ${CPPFLAGS} -MM *.cpp > make.deps
include make.deps
