# SmartRailway : simple Makefile (Linux, macOS, MinGW/MSYS2).  CMake users: see CMakeLists.txt
#
#   make            build the server       ->  build/railway_server
#   make run        build and start it     ->  http://127.0.0.1:8080
#   make test       build and run all unit tests
#   make -j4        build faster (the web library takes a while to compile the first time)
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
INC      := -Ibackend
LDLIBS   := -pthread
ifeq ($(OS),Windows_NT)
LDLIBS   += -lws2_32
endif

BUILD := build

# Add your own module's .cpp files to this list (see CONTRIBUTING.md)
TRAIN_SRC := backend/train/train.cpp backend/train/train_json.cpp backend/train/seed_data.cpp backend/train/train_contract.cpp
API_SRC   := backend/api/router.cpp backend/api/routes_train.cpp backend/api/routes_meta.cpp backend/api/server.cpp
SERVER_SRC := backend/main.cpp $(API_SRC) $(TRAIN_SRC)

SERVER_OBJ := $(SERVER_SRC:%.cpp=$(BUILD)/%.o)
TRAIN_OBJ  := $(TRAIN_SRC:%.cpp=$(BUILD)/%.o)

EXE := $(BUILD)/railway_server
ifeq ($(OS),Windows_NT)
EXE := $(EXE).exe
endif

.PHONY: all run test clean
all: $(EXE)

$(EXE): $(SERVER_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INC) -MMD -MP -c $< -o $@

run: $(EXE)
	./$(EXE)

# ---- tests --------------------------------------------------------------------------
$(BUILD)/test_ds: tests/test_ds.cpp
	$(CXX) $(CXXFLAGS) $(INC) $< -o $@

$(BUILD)/test_train: tests/test_train.cpp $(TRAIN_OBJ)
	$(CXX) $(CXXFLAGS) $(INC) $^ -o $@

test: $(BUILD)/test_ds $(BUILD)/test_train
	./$(BUILD)/test_ds
	./$(BUILD)/test_train

clean:
	rm -rf $(BUILD)

-include $(SERVER_OBJ:.o=.d)
