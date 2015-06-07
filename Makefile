CXX = g++ -fdiagnostics-color=always
CXXFLAGS = -std=c++14 -Wall -pthread
LDFLAGS = -lboost_system -lboost_coroutine -lstdc++
.PHONY: deploy
main: process.o rapunzel/rapunzel.a
rapunzel/rapunzel.a:
	cd rapunzel && make
clean:
	rm -f main *.o rapunzel/*.o lexy/*.o
