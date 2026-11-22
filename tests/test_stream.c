#include "unity.h"
#include "stream_buffer.h"
#include <string.h>

static ClientSession session;

void setUp(void) {
    stream_buffer_init(&session);
}

void tearDown(void) {
    // Không cần giải phóng bộ nhớ động
}

/* 1. Kiểm tra khởi tạo và xử lý tham số NULL */
void test_init_and_null_arguments(void) {
    stream_buffer_init(NULL); // Không được crash

    char line[MAX_LINE_LENGTH];
    TEST_ASSERT_EQUAL(STREAM_INVALID_ARG, stream_push(NULL, "PING\r\n", 6));
    TEST_ASSERT_EQUAL(STREAM_INVALID_ARG, stream_push(&session, NULL, 5));
    TEST_ASSERT_EQUAL(STREAM_OK, stream_push(&session, "PING\r\n", 0));

    TEST_ASSERT_EQUAL(STREAM_INVALID_ARG, stream_pop_line(NULL, line, sizeof(line)));
    TEST_ASSERT_EQUAL(STREAM_INVALID_ARG, stream_pop_line(&session, NULL, sizeof(line)));
    TEST_ASSERT_EQUAL(STREAM_INVALID_ARG, stream_pop_line(&session, line, 0));
}

/* 2. Kiểm tra nhận 1 câu lệnh hoàn chỉnh thông thường */
void test_single_complete_command(void) {
    const char *msg = "LOGIN|thinh|123456\r\n";
    char line[MAX_LINE_LENGTH];

    TEST_ASSERT_EQUAL(STREAM_OK, stream_push(&session, msg, strlen(msg)));
    TEST_ASSERT_EQUAL(STREAM_OK, stream_pop_line(&session, line, sizeof(line)));
    TEST_ASSERT_EQUAL_STRING("LOGIN|thinh|123456", line);

    // Sau khi lấy hết, lần gọi tiếp theo phải báo thiếu dữ liệu
    TEST_ASSERT_EQUAL(STREAM_NEED_MORE_DATA, stream_pop_line(&session, line, sizeof(line)));
    TEST_ASSERT_EQUAL(0, session.recv_len);
}

/* 3. Giả lập hiện tượng DÍNH GÓI TCP (Sticky Packets): 2 lệnh nằm chung 1 gói recv() */
void test_sticky_packets_multiple_commands_in_one_push(void) {
    const char *sticky_data = "REGISTER|user1|pass1\r\nLIST_ROOMS\r\nJOIN_ROOM|1\r\n";
    char line[MAX_LINE_LENGTH];

    TEST_ASSERT_EQUAL(STREAM_OK, stream_push(&session, sticky_data, strlen(sticky_data)));

    // Rút lệnh 1
    TEST_ASSERT_EQUAL(STREAM_OK, stream_pop_line(&session, line, sizeof(line)));
    TEST_ASSERT_EQUAL_STRING("REGISTER|user1|pass1", line);

    // Rút lệnh 2
    TEST_ASSERT_EQUAL(STREAM_OK, stream_pop_line(&session, line, sizeof(line)));
    TEST_ASSERT_EQUAL_STRING("LIST_ROOMS", line);

    // Rút lệnh 3
    TEST_ASSERT_EQUAL(STREAM_OK, stream_pop_line(&session, line, sizeof(line)));
    TEST_ASSERT_EQUAL_STRING("JOIN_ROOM|1", line);

    // Đã sạch bộ đệm
    TEST_ASSERT_EQUAL(STREAM_NEED_MORE_DATA, stream_pop_line(&session, line, sizeof(line)));
}

/* 4. Giả lập hiện tượng PHÂN MẢNH GÓI TCP (Fragmented Packets): 1 lệnh bị xé làm 3 mảnh */
void test_fragmented_packet_across_three_pushes(void) {
    char line[MAX_LINE_LENGTH];

    // Mảnh 1: "BID|1|5|" (chưa có \r\n)
    TEST_ASSERT_EQUAL(STREAM_OK, stream_push(&session, "BID|1|5|", 8));
    TEST_ASSERT_EQUAL(STREAM_NEED_MORE_DATA, stream_pop_line(&session, line, sizeof(line)));

    // Mảnh 2: "250000\r" (mới có \r, thiếu \n)
    TEST_ASSERT_EQUAL(STREAM_OK, stream_push(&session, "250000\r", 7));
    TEST_ASSERT_EQUAL(STREAM_NEED_MORE_DATA, stream_pop_line(&session, line, sizeof(line)));

    // Mảnh 3: "\n" (hoàn thiện ký tự kết thúc)
    TEST_ASSERT_EQUAL(STREAM_OK, stream_push(&session, "\n", 1));
    TEST_ASSERT_EQUAL(STREAM_OK, stream_pop_line(&session, line, sizeof(line)));
    TEST_ASSERT_EQUAL_STRING("BID|1|5|250000", line);
}

/* 5. Kiểm tra kết hợp vừa phân mảnh vừa dính gói */
void test_mixed_fragmented_and_sticky_packets(void) {
    char line[MAX_LINE_LENGTH];

    // Gói 1 chứa trọn lệnh 1 và nửa đầu lệnh 2
    const char *chunk1 = "PING\r\nCREATE_ROOM|Phong";
    TEST_ASSERT_EQUAL(STREAM_OK, stream_push(&session, chunk1, strlen(chunk1)));

    // Phải tách được ngay lệnh 1
    TEST_ASSERT_EQUAL(STREAM_OK, stream_pop_line(&session, line, sizeof(line)));
    TEST_ASSERT_EQUAL_STRING("PING", line);

    // Lệnh 2 chưa đủ \r\n nên phải chờ
    TEST_ASSERT_EQUAL(STREAM_NEED_MORE_DATA, stream_pop_line(&session, line, sizeof(line)));

    // Gói 2 chứa phần còn lại của lệnh 2
    const char *chunk2 = "_VIP_01\r\n";
    TEST_ASSERT_EQUAL(STREAM_OK, stream_push(&session, chunk2, strlen(chunk2)));

    TEST_ASSERT_EQUAL(STREAM_OK, stream_pop_line(&session, line, sizeof(line)));
    TEST_ASSERT_EQUAL_STRING("CREATE_ROOM|Phong_VIP_01", line);
}

/* 6. Kiểm tra dòng rỗng chỉ có "\r\n" */
void test_empty_line_crlf(void) {
    char line[MAX_LINE_LENGTH];
    TEST_ASSERT_EQUAL(STREAM_OK, stream_push(&session, "\r\n", 2));
    TEST_ASSERT_EQUAL(STREAM_OK, stream_pop_line(&session, line, sizeof(line)));
    TEST_ASSERT_EQUAL_STRING("", line);
}

/* 7. Kiểm tra lỗi TRÀN BỘ ĐỆM NHẬN (Buffer Overflow) */
void test_buffer_overflow(void) {
    char big_chunk[RECV_BUFFER_SIZE];
    memset(big_chunk, 'A', sizeof(big_chunk));

    // Nạp đầy 4096 byte đầu tiên -> OK
    TEST_ASSERT_EQUAL(STREAM_OK, stream_push(&session, big_chunk, RECV_BUFFER_SIZE));

    // Nạp thêm 1 byte nữa -> Phải báo STREAM_BUFFER_OVERFLOW
    TEST_ASSERT_EQUAL(STREAM_BUFFER_OVERFLOW, stream_push(&session, "B", 1));
}

/* 8. Kiểm tra lỗi DÒNG LỆNH QUÁ DÀI vượt quá out_size */
void test_line_too_long_with_and_without_delimiter(void) {
    char small_out[16];

    // Trường hợp A: Dữ liệu nạp vào >= small_out nhưng chưa thấy \r\n
    TEST_ASSERT_EQUAL(STREAM_OK, stream_push(&session, "1234567890123456", 16));
    TEST_ASSERT_EQUAL(STREAM_LINE_TOO_LONG, stream_pop_line(&session, small_out, sizeof(small_out)));

    // Trường hợp B: Có \r\n nhưng độ dài dòng vượt quá small_out
    stream_buffer_init(&session);
    const char *long_cmd = "ADD_ITEM|1|Laptop_Gaming|15000000\r\n";
    TEST_ASSERT_EQUAL(STREAM_OK, stream_push(&session, long_cmd, strlen(long_cmd)));
    TEST_ASSERT_EQUAL(STREAM_LINE_TOO_LONG, stream_pop_line(&session, small_out, sizeof(small_out)));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_and_null_arguments);
    RUN_TEST(test_single_complete_command);
    RUN_TEST(test_sticky_packets_multiple_commands_in_one_push);
    RUN_TEST(test_fragmented_packet_across_three_pushes);
    RUN_TEST(test_mixed_fragmented_and_sticky_packets);
    RUN_TEST(test_empty_line_crlf);
    RUN_TEST(test_buffer_overflow);
    RUN_TEST(test_line_too_long_with_and_without_delimiter);
    return UNITY_END();
}
