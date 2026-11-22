#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>
#include <stdbool.h>
#include <time.h>

/* =========================================================================
 * 1. CẤU HÌNH MẠNG & GIỚI HẠN HỆ THỐNG
 * ========================================================================= */
#define DEFAULT_PORT            8080
#define MAX_CLIENTS             100     // Số lượng client tối đa kết nối đồng thời
#define RECV_BUFFER_SIZE        4096    // Kích thước bộ đệm nhận cho mỗi client
#define MAX_LINE_LENGTH         1024    // Độ dài tối đa của 1 dòng lệnh hoàn chỉnh
#define MAX_ARGS                8       // Số tham số tối đa trong 1 lệnh

#define MAX_USERNAME_LEN        32
#define MAX_PASSWORD_LEN        64
#define MAX_ROOM_NAME_LEN       64
#define MAX_ITEM_NAME_LEN       64
#define MAX_ITEM_DESC_LEN       256
#define MAX_IP_LEN              46      // Hỗ trợ cả IPv4 và IPv6

/* =========================================================================
 * 2. ĐỊNH DẠNG GIAO THỨC TẦNG ỨNG DỤNG (STREAM FRAMING & DELIMITERS)
 * ========================================================================= */
#define PROTOCOL_DELIMITER      "\r\n"
#define PROTOCOL_DELIMITER_LEN  2
#define FIELD_SEPARATOR         "|"
#define FIELD_SEPARATOR_CHAR    '|'

/* =========================================================================
 * 3. MÃ LỆNH TỪ CLIENT GỬI LÊN SERVER (REQUEST COMMANDS)
 * ========================================================================= */
// Quản lý tài khoản & phiên
#define CMD_REGISTER            "REGISTER"      // REGISTER|username|password\r\n
#define CMD_LOGIN               "LOGIN"         // LOGIN|username|password\r\n
#define CMD_LOGOUT              "LOGOUT"        // LOGOUT\r\n

// Quản lý phòng đấu giá
#define CMD_CREATE_ROOM         "CREATE_ROOM"   // CREATE_ROOM|room_name\r\n
#define CMD_LIST_ROOMS          "LIST_ROOMS"    // LIST_ROOMS\r\n
#define CMD_JOIN_ROOM           "JOIN_ROOM"     // JOIN_ROOM|room_id\r\n
#define CMD_LEAVE_ROOM          "LEAVE_ROOM"    // LEAVE_ROOM\r\n

// Quản lý vật phẩm đấu giá
#define CMD_ADD_ITEM            "ADD_ITEM"      // ADD_ITEM|room_id|name|start_price|buy_now_price|desc\r\n
#define CMD_DEL_ITEM            "DEL_ITEM"      // DEL_ITEM|room_id|item_id\r\n
#define CMD_LIST_ITEMS          "LIST_ITEMS"    // LIST_ITEMS|room_id\r\n
#define CMD_START_AUCTION       "START_AUCTION" // START_AUCTION|room_id\r\n

// Đấu giá & Mua ngay
#define CMD_BID                 "BID"           // BID|room_id|item_id|amount\r\n
#define CMD_BUY_NOW             "BUY_NOW"       // BUY_NOW|room_id|item_id\r\n

// Tiện ích kiểm tra kết nối
#define CMD_PING                "PING"          // PING\r\n

/* =========================================================================
 * 4. MÃ PHẢN HỒI & THÔNG BÁO ĐẨY TỪ SERVER (RESPONSE & PUSH NOTIFY)
 * ========================================================================= */
#define RESP_OK                 "OK"            // OK|code|data...\r\n
#define RESP_ERR                "ERR"           // ERR|code|message\r\n
#define RESP_NOTIFY             "NOTIFY"        // NOTIFY|event_type|data...\r\n

// Các loại sự kiện Broadcast (Push Notification) trong phòng đấu giá
#define EVENT_USER_JOINED       "USER_JOINED"   // NOTIFY|USER_JOINED|username\r\n
#define EVENT_USER_LEFT         "USER_LEFT"     // NOTIFY|USER_LEFT|username\r\n
#define EVENT_NEW_ITEM          "NEW_ITEM"      // NOTIFY|NEW_ITEM|item_id|name|start_price|buy_now_price|desc\r\n
#define EVENT_NEW_BID           "NEW_BID"       // NOTIFY|NEW_BID|item_id|username|new_price\r\n
#define EVENT_COUNTDOWN         "COUNTDOWN"     // NOTIFY|COUNTDOWN|attempt|item_id|current_price|leader\r\n
#define EVENT_ITEM_SOLD         "ITEM_SOLD"     // NOTIFY|ITEM_SOLD|item_id|winner|final_price|reason\r\n
#define EVENT_ROOM_CLOSED       "ROOM_CLOSED"   // NOTIFY|ROOM_CLOSED|room_id\r\n

/* =========================================================================
 * 5. MÃ TRẠNG THÁI & MÃ LỖI CHI TIẾT (STATUS & ERROR CODES)
 * ========================================================================= */
typedef enum {
    STATUS_SUCCESS              = 200,
    STATUS_CREATED              = 201,

    // Lỗi cú pháp & giao thức (400x)
    ERR_BAD_REQUEST             = 4000, // Sai cú pháp hoặc thiếu tham số
    ERR_UNKNOWN_COMMAND         = 4001, // Lệnh không tồn tại
    ERR_LINE_TOO_LONG           = 4002, // Dòng lệnh vượt quá MAX_LINE_LENGTH

    // Lỗi xác thực & phiên (401x)
    ERR_UNAUTHORIZED            = 4010, // Chưa đăng nhập
    ERR_INVALID_CREDENTIALS     = 4011, // Sai tên đăng nhập hoặc mật khẩu
    ERR_USER_ALREADY_EXISTS     = 4012, // Tên đăng nhập đã tồn tại khi đăng ký
    ERR_ALREADY_LOGGED_IN       = 4013, // Tài khoản đang đăng nhập ở phiên khác

    // Lỗi phân quyền & phòng đấu giá (403x - 404x)
    ERR_FORBIDDEN               = 4030, // Không có quyền thực hiện (không phải chủ phòng)
    ERR_ALREADY_IN_ROOM         = 4031, // Mỗi thời điểm chỉ được tham gia 1 phòng
    ERR_NOT_IN_ROOM             = 4032, // Chưa tham gia phòng nào
    ERR_ROOM_NOT_FOUND          = 4040, // Không tìm thấy phòng đấu giá
    ERR_ITEM_NOT_FOUND          = 4041, // Không tìm thấy vật phẩm

    // Lỗi nghiệp vụ đấu giá (409x)
    ERR_ITEM_ALREADY_ACTIVE     = 4090, // Không thể xóa vật phẩm đang/đã đấu giá
    ERR_NO_ACTIVE_ITEM          = 4091, // Phòng hiện chưa có vật phẩm nào đang đấu giá
    ERR_BID_TOO_LOW             = 4092, // Giá tố phải lớn hơn giá hiện tại ít nhất 10.000đ
    ERR_BID_SELF_LEADING        = 4093, // Bạn đang là người giữ giá cao nhất
    ERR_OWNER_CANNOT_BID        = 4094, // Chủ phòng không được tự đấu giá vật phẩm của mình
    ERR_INVALID_PRICE           = 4095, // Giá khởi điểm / giá mua ngay không hợp lệ

    // Lỗi hệ thống (500x)
    ERR_SERVER_FULL             = 5000, // Đạt giới hạn MAX_CLIENTS
    ERR_INTERNAL_ERROR          = 5001  // Lỗi xử lý nội bộ server
} StatusCode;

/* =========================================================================
 * 6. QUY TẮC NGHIỆP VỤ ĐẤU GIÁ (BUSINESS CONSTANTS)
 * ========================================================================= */
#define MIN_BID_INCREMENT       10000L  // Bước giá tối thiểu: 10.000 VNĐ
#define DEFAULT_BID_TIMEOUT_SEC 180     // Thời gian chờ sau mỗi lượt tố giá: 3 phút (180s)
#define DEFAULT_WARN_INTERVAL   15      // Khoảng cách giữa 3 lần thông báo cảnh báo: 15s
#define MAX_WARNING_COUNT       3       // Số lần gửi thông báo trước khi chốt đơn

/* =========================================================================
 * 7. CẤU TRÚC PHIÊN KẾT NỐI CLIENT (CLIENT SESSION)
 * ========================================================================= */
typedef struct {
    int socket_fd;                          // File descriptor của socket client (-1 nếu slot trống)
    char ip_addr[MAX_IP_LEN];               // Địa chỉ IP của client (dùng cho ghi log)
    int port;                               // Cổng kết nối của client

    // Bộ đệm phục vụ xử lý truyền dòng (TCP Stream Framing)
    char recv_buffer[RECV_BUFFER_SIZE];     // Bộ đệm tích lũy byte nhận từ recv()
    size_t recv_len;                        // Số byte hiện có trong recv_buffer

    // Trạng thái xác thực & phiên làm việc
    bool is_authenticated;                  // true nếu đã đăng nhập thành công
    char username[MAX_USERNAME_LEN];        // Tên tài khoản đang đăng nhập
    int current_room_id;                    // ID phòng đang tham gia (-1 nếu chưa vào phòng nào)

    time_t connected_at;                    // Thời điểm kết nối
    time_t last_active_at;                  // Thời điểm tương tác gần nhất
} ClientSession;

#endif /* PROTOCOL_H */
