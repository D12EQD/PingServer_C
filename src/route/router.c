#include <string.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#include "net/connection.h"
#include "net/tcp_server.h"
#include "other/debug.h"
#include "other/def.h"
#include "route/router.h"
#include "protocol/http.h"

#define DEBUG_RT(...) DEBUG(DEBUG_FLAG_ROUTER, ##__VA_ARGS__)

#define MAX_CACHE_COUNT 32
#define MAX_PATH_LEN    256

/* ===================== 文件 fd 缓存 ===================== */
/* 约定: str[0] == 0 视为空槽; fd 只在 str 非空时有效 */
typedef struct {
    int  fd;
    char str[MAX_PATH_LEN];   /* NUL 结尾 */
} FileCache;

static FileCache file_cache[MAX_CACHE_COUNT] = {0};
static uint8_t   cache_idx = 0;

/* ===================== MIME 表 ===================== */
static const char CONTENT_HTML[]  = "text/html; charset=utf-8";
static const char CONTENT_TXT[]   = "text/plain; charset=utf-8";
static const char CONTENT_MD[]    = "text/markdown; charset=utf-8";
static const char CONTENT_CSS[]   = "text/css; charset=utf-8";
static const char CONTENT_JS[]    = "text/javascript; charset=utf-8";
static const char CONTENT_JSON[]  = "application/json; charset=utf-8";
static const char CONTENT_XML[]   = "application/xml; charset=utf-8";
static const char CONTENT_PNG[]   = "image/png";
static const char CONTENT_JPEG[]  = "image/jpeg";
static const char CONTENT_GIF[]   = "image/gif";
static const char CONTENT_WEBP[]  = "image/webp";
static const char CONTENT_SVG[]   = "image/svg+xml";
static const char CONTENT_ICO[]   = "image/x-icon";

#define CONTENT_TYPE_TABLE(X)          \
    X("html",     CONTENT_HTML)        \
    X("htm",      CONTENT_HTML)        \
    X("xhtml",    CONTENT_HTML)        \
    X("txt",      CONTENT_TXT)         \
    X("text",     CONTENT_TXT)         \
    X("markdown", CONTENT_MD)          \
    X("md",       CONTENT_MD)          \
    X("css",      CONTENT_CSS)         \
    X("js",       CONTENT_JS)          \
    X("mjs",      CONTENT_JS)          \
    X("json",     CONTENT_JSON)        \
    X("xml",      CONTENT_XML)         \
    X("svg",      CONTENT_SVG)         \
    X("png",      CONTENT_PNG)         \
    X("jpg",      CONTENT_JPEG)        \
    X("jpeg",     CONTENT_JPEG)        \
    X("gif",      CONTENT_GIF)         \
    X("webp",     CONTENT_WEBP)        \
    X("ico",      CONTENT_ICO)

static const char page_404[] =
    "<!DOCTYPE html>"
    "<html lang=\"zh-Hans\">"
    "<head>"
    "   <meta charset=\"UTF-8\">"
    "   <title>404 页面未找到</title>"
    "   <style>"
    "       body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }"
    "       h1 { color: red; }"
    "       a { color: blue; text-decoration: none; }"
    "   </style>"
    "</head>"
    "<body>"
    "   <h1>404 - 页面未找到</h1>"
    "   <p>抱歉，您访问的页面不存在。</p>"
    "   <a href=\"/\">返回首页</a>"
    "</body>"
    "</html>";

/* ===================== 工具函数 ===================== */

static int ext_eq_ci(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + ('a' - 'A'));
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + ('a' - 'A'));
        if (ca != cb) return 0;
    }
    return 1;
}

/* 传入 path 和长度, 未匹配 → CONTENT_TXT */
static inline const char *content_type_by_name(const char *name, size_t len) {
    for (size_t i = len; i > 0; --i) {
        char c = name[i - 1];
        if (c == '/') return CONTENT_TXT;
        if (c == '.') {
            const char *ext = name + i;
            size_t      ext_len = len - i;
            if (ext_len == 0) return CONTENT_TXT;

            #define CT_LOOKUP(pattern, ct)                                    \
                do {                                                          \
                    if (ext_len == sizeof(pattern) - 1 &&                     \
                        ext_eq_ci(ext, pattern, ext_len)) return ct;          \
                } while (0);
            CONTENT_TYPE_TABLE(CT_LOOKUP)
            #undef CT_LOOKUP
            return CONTENT_TXT;
        }
    }
    return CONTENT_TXT;
}

/* 取 fd, 失败返回 -1。 name 必须 NUL 结尾, 长度 < MAX_PATH_LEN */
static inline int get_fd(const char *name) {
    size_t name_len = strlen(name);
    if (name_len == 0 || name_len >= MAX_PATH_LEN) return -1;

    for (int i = 0; i < MAX_CACHE_COUNT; i++) {
        if (file_cache[i].str[0] == 0) continue;
        if (strcmp(file_cache[i].str, name) == 0)
            return file_cache[i].fd;
    }

    int fd = open(name, O_RDONLY);
    if (fd < 0) return -1;

    struct stat st;
    if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode)) {
        close(fd);
        return -1;
    }

    /* 4. 插入缓存, FIFO 淘汰 */
    if (file_cache[cache_idx].str[0] != 0) {
        close(file_cache[cache_idx].fd);
    }
    file_cache[cache_idx].fd = fd;
    memcpy(file_cache[cache_idx].str, name, name_len);   
    file_cache[cache_idx].str[name_len] = '\0'; 

    cache_idx = (uint8_t)((cache_idx + 1) % MAX_CACHE_COUNT);
    return fd;
}

static inline void http_response_set(httpResponse *res, int st_code,
                                     const char *file_name, size_t file_len) {
    res->status_code = st_code;
    if (unlikely(file_name == NULL)) {
        res->content_type = CONTENT_TXT;
        res->body         = page_404;
        /* res->body_len 若存在, 可以在这里填 sizeof(page_404) - 1 */
    } else {
        res->content_type = content_type_by_name(file_name, file_len);
        res->body         = NULL;
    }
}

void main_home(Connection *conn);
void main_static(Connection *conn);
void main_not_found(Connection *conn);

Router router_table[] = {
    {"/static/", main_static,    MethodGet, MatchPrefix},
    {"/",        main_home,      MethodGet, MatchExact},
    {"",         main_not_found, MethodGet, MatchPrefix},
};

void router_layer_init(tcpServer *server) {
    (void)server;
}

routerFunction get_router_function(const char *path, size_t path_len, const char *method, size_t method_len) {
    size_t n = sizeof(router_table) / sizeof(router_table[0]);
    int    is_get = str_check(method, method_len, "GET");

    DEBUG_RT(LV_INFO, "get_router_function start\n");

    for (size_t i = 0; i < n; i++) {
        if (router_table[i].method == MethodGet && !is_get) continue;

        size_t r_len = strlen(router_table[i].path);
        int    hit   = 0;

        if (router_table[i].match_rule == MatchExact) {
            hit = str_check(path, path_len, router_table[i].path);
        } else {
            if (path_len >= r_len &&
                memcmp(path, router_table[i].path, r_len) == 0)
                hit = 1;
        }

        if (hit) {
            DEBUG_RT(LV_INFO, "route -> %s\n", router_table[i].path);
            return router_table[i].func;
        }
    }

    DEBUG_RT(LV_INFO, "no route matched, fallback 404\n");
    return main_not_found;
}

void main_home(Connection *conn) {
    HttpContext  *ctx = conn->protocol_ctx;
    httpResponse *res = &ctx->http_res;

    DEBUG_RT(LV_INFO, "main_home start\n");

    static const char INDEX_PATH[] = "static/index.html";

    int fd = get_fd(INDEX_PATH);
    if (unlikely(fd < 0)) {
        DEBUG_RT(LV_WARN, "main_home, 500\n");
        http_response_set(res, 500, NULL, 0);
        return;
    }

    res->body_fd = fd;
    http_response_set(res, 200, INDEX_PATH, sizeof(INDEX_PATH) - 1);
    DEBUG_RT(LV_INFO, "main_home, 200\n");
}

/* /static/...  →  对应磁盘文件 */
void main_static(Connection *conn) {
    HttpContext  *ctx = conn->protocol_ctx;
    httpResponse *res = &ctx->http_res;
    httpRequest  *req = &ctx->http_req;

    DEBUG_RT(LV_INFO, "main_static start\n");

    if (req->path_len < 1 || req->path[0] != '/') {
        http_response_set(res, 400, NULL, 0);
        return;
    }

    const char *raw = req->path + 1;
    size_t len = req->path_len - 1;

    /* 截断 query string */
    for (size_t i = 0; i < len; i++) {
        if (raw[i] == '?') { len = i; break; }
    }

    if (len == 0 || len >= MAX_PATH_LEN) {
        http_response_set(res, 400, NULL, 0);
        return;
    }

    // temp 路径构造
    char path_buf[MAX_PATH_LEN];
    memcpy(path_buf, raw, len);
    path_buf[len] = '\0';

    int fd = get_fd(path_buf);
    if (unlikely(fd < 0)) {
        DEBUG_RT(LV_INFO, "static not found: %s\n", path_buf);
        http_response_set(res, 404, NULL, 0);
        return;
    }

    res->body_fd = fd;
    http_response_set(res, 200, path_buf, len);
}

void main_not_found(Connection *conn) {
    HttpContext  *ctx = conn->protocol_ctx;
    httpResponse *res = &ctx->http_res;

    DEBUG_RT(LV_INFO, "main_not_found\n");
    http_response_set(res, 404, NULL, 0);
}

void tcpserver_close_router(tcpServer *server) {
    (void)server;
    for (int i = 0; i < MAX_CACHE_COUNT; i++) {
        if (file_cache[i].str[0] == 0) continue;
        if (file_cache[i].fd > 0) {
            close(file_cache[i].fd);
            file_cache[i].fd = -1;
        }
        file_cache[i].str[0] = '\0';
    }
    cache_idx = 0;
}
