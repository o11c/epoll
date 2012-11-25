CXX = g++
# CXX = clang++
CXXFLAGS = -g -O

override CC=${CXX} # for linking
override CXXFLAGS += -std=c++0x

main: main.o
main.o: main.cpp net.hpp
