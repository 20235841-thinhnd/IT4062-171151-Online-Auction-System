#include "stream_buffer.h"
#include <string.h>

void stream_buffer_init(ClientSession *session) {
    if (session == NULL) {
        return;
    }
    memset(session->recv_buffer, 0, sizeof(session->recv_buffer));
    session->recv_len = 0;
}

StreamStatus stream_push(ClientSession *session, const char *data, size_t len) {
    if (session == NULL || (data == NULL && len > 0)) {
        return STREAM_INVALID_ARG;
    }

    if (len == 0) {
        return STREAM_OK;
    }

    // Kiểm tra xem bộ đệm còn đủ chỗ chứa thêm len byte hay không
    if (session->recv_len + len > RECV_BUFFER_SIZE) {
        return STREAM_BUFFER_OVERFLOW;
    }

    // Nối dữ liệu mới vào cuối vùng dữ liệu hiện có trong recv_buffer
    memcpy(session->recv_buffer + session->recv_len, data, len);
    session->recv_len += len;

    return STREAM_OK;
}

StreamStatus stream_pop_line(ClientSession *session, char *out_line, size_t out_size) {
    if (session == NULL || out_line == NULL || out_size == 0) {
        return STREAM_INVALID_ARG;
    }

    // Cần ít nhất 2 byte để có thể chứa "\r\n"
    if (session->recv_len < PROTOCOL_DELIMITER_LEN) {
        return STREAM_NEED_MORE_DATA;
    }

    // Tìm vị trí xuất hiện đầu tiên của cặp ký tự "\r\n" trong recv_buffer
    // (Dùng vòng lặp duyệt byte thay vì strstr để an toàn ngay cả khi dữ liệu có byte '\0')
    size_t delim_pos = session->recv_len;
    for (size_t i = 0; i + 1 < session->recv_len; i++) {
        if (session->recv_buffer[i] == '\r' && session->recv_buffer[i + 1] == '\n') {
            delim_pos = i;
            break;
        }
    }

    // Chưa tìm thấy "\r\n" -> Gói tin bị phân mảnh, chờ lần recv() sau
    if (delim_pos == session->recv_len) {
        // Nếu buffer đã đầy hoặc vượt MAX_LINE_LENGTH mà vẫn chưa có "\r\n" -> Lỗi dòng quá dài
        if (session->recv_len >= out_size || session->recv_len >= RECV_BUFFER_SIZE) {
            return STREAM_LINE_TOO_LONG;
        }
        return STREAM_NEED_MORE_DATA;
    }

    // Kiểm tra độ dài dòng lệnh (không tính "\r\n") có vừa với out_line (+1 cho '\0') không
    if (delim_pos + 1 > out_size) {
        return STREAM_LINE_TOO_LONG;
    }

    // Copy nội dung dòng lệnh ra out_line và kết thúc chuỗi bằng '\0'
    memcpy(out_line, session->recv_buffer, delim_pos);
    out_line[delim_pos] = '\0';

    // Tính số byte đã tiêu thụ (nội dung dòng + 2 byte "\r\n")
    size_t consumed = delim_pos + PROTOCOL_DELIMITER_LEN;
    size_t remaining = session->recv_len - consumed;

    // Dồn phần dữ liệu dư (nếu có dính gói phía sau) lên đầu recv_buffer
    if (remaining > 0) {
        memmove(session->recv_buffer, session->recv_buffer + consumed, remaining);
    }
    session->recv_len = remaining;

    return STREAM_OK;
}
