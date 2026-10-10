CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall
SRC      = main.cpp train/train.cpp
# Windows (MinGW) needs: make LIBS="-lws2_32 -lwsock32"
LIBS     = -pthread

railway: $(SRC) $(wildcard ds/*.h train/*.h)
	$(CXX) $(CXXFLAGS) $(SRC) -o railway $(LIBS)

run: railway
	./railway 8080

clean:
	rm -f railway railway.exe
