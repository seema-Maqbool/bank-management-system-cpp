CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
SRC      := src/Account.cpp src/Bank.cpp

all: bank

bank: $(SRC) src/main.cpp src/*.h
	$(CXX) $(CXXFLAGS) $(SRC) src/main.cpp -o bank

test_runner: $(SRC) tests/test_bank.cpp src/*.h
	$(CXX) $(CXXFLAGS) $(SRC) tests/test_bank.cpp -o test_runner

test: test_runner
	./test_runner

clean:
	rm -rf bank test_runner data
