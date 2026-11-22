#ifndef STREAM_BUFFER_H
#define STREAM_BUFFER_H

#include "protocol.h"
#include <stddef.h>

/**
 * Mã kết quả trả về khi thao tác với bộ đệm truyền dòng
 */
typedef enum {
    STREAM_OK = 0,              // Thành công (đã nạp dữ liệu hoặc tách được 1 dòng lệnh)
    STREAM_NEED_MORE_DATA = 1,  // Chưa đủ ký tự kết thúc \r\n, cần chờ lần recv() tiếp theo
    STREAM_BUFFER_OVERFLOW = -1,// Bộ đệm nhận bị đầy (client gửi quá dài không có \r\n)
    STREAM_LINE_TOO_LONG = -2,  // Một dòng lệnh vượt quá MAX_LINE_LENGTH
    STREAM_INVALID_ARG = -3     // Tham số con trỏ NULL không hợp lệ
} StreamStatus;

/**
 * Khởi tạo / đặt lại trạng thái bộ đệm nhận của một ClientSession
 */
void stream_buffer_init(ClientSession *session);

/**
 * Nạp dữ liệu thô vừa đọc được từ hàm recv() vào recv_buffer của client.
 *
 * @param session Con trỏ tới phiên làm việc của client
 * @param data    Con trỏ tới mảng byte nhận được từ recv()
 * @param len     Số byte nhận được (len > 0)
 * @return        STREAM_OK nếu nạp thành công,
 *                STREAM_BUFFER_OVERFLOW nếu dữ liệu vượt quá RECV_BUFFER_SIZE,
 *                STREAM_INVALID_ARG nếu tham số NULL.
 */
StreamStatus stream_push(ClientSession *session, const char *data, size_t len);

/**
 * Trích xuất một dòng lệnh hoàn chỉnh kết thúc bằng "\r\n" ra khỏi recv_buffer.
 * Ký tự "\r\n" sẽ được loại bỏ khỏi chuỗi out_line và thay bằng '\0'.
 * Phần dữ liệu còn dư phía sau "\r\n" được dồn lên đầu recv_buffer cho lần gọi kế tiếp.
 *
 * @param session   Con trỏ tới phiên làm việc của client
 * @param out_line  Bộ đệm đầu ra chứa dòng lệnh đã tách
 * @param out_size  Kích thước tối đa của out_line (thường là MAX_LINE_LENGTH)
 * @return          STREAM_OK nếu tách được 1 dòng hoàn chỉnh,
 *                  STREAM_NEED_MORE_DATA nếu chưa thấy "\r\n",
 *                  STREAM_LINE_TOO_LONG nếu dòng lệnh vượt quá out_size,
 *                  STREAM_INVALID_ARG nếu tham số NULL.
 */
StreamStatus stream_pop_line(ClientSession *session, char *out_line, size_t out_size);

#endif /* STREAM_BUFFER_H */
