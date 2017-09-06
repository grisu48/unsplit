

default:	all
all:	unsplit

unsplit:	unsplit.cpp
	g++ -Wall -Wextra -std=c++11 -O3 -o unsplit unsplit.cpp

install:	unsplit
	install ./unsplit /usr/local/bin
