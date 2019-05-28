bench: bench.cpp
	g++ -std=c++0x -Wall -Wextra -Weffc++ -O3 -s -o $@ $< -lpthread

clean:
	@echo cleaning...
	@rm -f bench

.PHONY: clean
