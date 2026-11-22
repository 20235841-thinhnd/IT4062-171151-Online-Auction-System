# Trình biên dịch và cờ cơ bản
CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude
COV_FLAGS = --coverage -O0

# Các thư mục dự án
SRC_DIR = src
SERVER_DIR = $(SRC_DIR)/server
CLIENT_DIR = $(SRC_DIR)/client_cli
TEST_DIR = tests
BIN_DIR = bin
BUILD_DIR = build
COV_DIR = coverage

# Tự động quét danh sách file nguồn (.c)
SERVER_SRCS = $(wildcard $(SERVER_DIR)/*.c)
CLIENT_SRCS = $(wildcard $(CLIENT_DIR)/*.c) $(SERVER_DIR)/stream_buffer.c
# Lọc bỏ main.c của server khi biên dịch Unit Test để tránh trùng hàm main()
CORE_SRCS = $(filter-out $(SERVER_DIR)/main.c, $(SERVER_SRCS))
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c) $(wildcard $(TEST_DIR)/unity/*.c)

# File thực thi đầu ra
SERVER_BIN = $(BIN_DIR)/server
CLIENT_BIN = $(BIN_DIR)/client
TEST_BIN = $(BIN_DIR)/test_runner

# Mặc định biên dịch cả Server và Client CLI
all: dirs $(SERVER_BIN) $(CLIENT_BIN)

# Tạo sẵn các thư mục cần thiết nếu chưa tồn tại
dirs:
	@mkdir -p $(BIN_DIR) $(BUILD_DIR) logs data

# Biên dịch Server
$(SERVER_BIN): $(SERVER_SRCS)
	$(CC) $(CFLAGS) $(SERVER_SRCS) -o $(SERVER_BIN)

# Biên dịch Client CLI
$(CLIENT_BIN): $(CLIENT_SRCS)
	$(CC) $(CFLAGS) $(CLIENT_SRCS) -o $(CLIENT_BIN)

# Biên dịch và chạy Unit Test kèm cờ đo Coverage
test: dirs
	@if [ -z "$(wildcard $(TEST_DIR)/*.c)" ]; then \
		echo "Chưa có file test nào trong thư mục $(TEST_DIR)/"; \
	else \
		$(CC) $(CFLAGS) $(COV_FLAGS) $(CORE_SRCS) $(TEST_SRCS) -I$(TEST_DIR)/unity -o $(TEST_BIN); \
		./$(TEST_BIN); \
	fi

# Xuất báo cáo độ phủ mã nguồn (Coverage > 90%) bằng gcov và lcov
coverage: test
	@if ! ls $(TEST_DIR)/*.c >/dev/null 2>&1 || [ -z "$(CORE_SRCS)" ]; then \
		echo "Bỏ qua coverage vì chưa có file test hoặc file module lõi."; \
	else \
		mkdir -p $(COV_DIR); \
		gcov -o $(TEST_BIN) $(CORE_SRCS); \
		mv -f *.gcov $(COV_DIR)/ 2>/dev/null || true; \
		if command -v lcov >/dev/null 2>&1; then \
			lcov --capture --directory $(BIN_DIR) --output-file $(COV_DIR)/coverage.info; \
			lcov --remove $(COV_DIR)/coverage.info '/usr/*' '*/tests/*' --output-file $(COV_DIR)/coverage.info; \
			lcov --list $(COV_DIR)/coverage.info; \
			genhtml $(COV_DIR)/coverage.info --output-directory $(COV_DIR)/html; \
			echo "Báo cáo HTML đã được xuất tại: $(COV_DIR)/html/index.html"; \
		else \
			echo "Đã chạy gcov xong. Cài thêm 'lcov' nếu muốn xem báo cáo HTML."; \
		fi; \
	fi

# Dọn dẹp toàn bộ file biên dịch và file coverage tạm
clean:
	rm -rf $(BIN_DIR) $(BUILD_DIR) $(COV_DIR) *.gcov *.gcno *.gcda

.PHONY: all dirs test coverage clean
