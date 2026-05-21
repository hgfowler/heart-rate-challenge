BUILD_DIR := build
EXECUTABLE := heart_rate
VIDEO := codingtest.mov

.PHONY: run build clean configure

run: build
	./$(BUILD_DIR)/$(EXECUTABLE) $(VIDEO)

build: configure
	cmake --build $(BUILD_DIR)

configure:
	cmake -S . -B $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)