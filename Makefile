CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pthread
SRC = rw.cpp monitor_rw.cpp control_atomico_rw.cpp verificador.cpp

rw: $(SRC) *.h
	$(CXX) $(CXXFLAGS) $(SRC) -o rw

tsan: $(SRC) *.h
	$(CXX) -std=c++17 -O1 -g -Wall -Wextra -pthread -fsanitize=thread $(SRC) -o rw_tsan

clean:
	rm -f rw rw_tsan

.PHONY: clean
