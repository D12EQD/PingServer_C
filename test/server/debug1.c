#define NOB_IMPLEMENTATION
#define NOB_WARN_DEPRECATED
#define NOB_EXPERIMENTAL_DELETE_OLD

#include <nob.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#define ANSI_RED   "\033[31m"
#define ANSI_GREEN "\033[32m"
#define ANSI_RESET "\033[0m"

#define build_message(start_color, message) \
printf(start_color "============== " message " ==============" ANSI_RESET "\n")

/* =====================================================================
 *  两阶段命令：
 *    1. hey -n 1000 -c 500 http://127.0.0.1:8080/ -o csv test/results.csv
 *    2. curl http://127.0.0.1:8080
 * ===================================================================== */
static inline bool run_bench(int argc, char **argv) {
    (void)argc; (void)argv;

    // 创建结果目录
    if (!nob_mkdir_if_not_exists("test")) {
        nob_log(NOB_ERROR, "Failed to create test directory");
        return false;
    }

    Nob_Cmd cmd = {0};   // nob_cmd_run 会清空 cmd，可复用于第二阶段

    // ============ 第一步：hey 压测，结果写成 csv ============
    build_message(ANSI_GREEN, "Running hey benchmark");
    printf("  hey -n 1000 -c 500 http://127.0.0.1:8080/ -o csv test/results.csv\n");

    nob_cmd_append(&cmd,
        "hey",
        "-n", "1000",
        "-c", "500",
        "http://127.0.0.1:8080/",
        "-o", "csv",
        "test/results.csv");

    if (!nob_cmd_run(&cmd)) {
        build_message(ANSI_RED, "hey benchmark failed");
        goto fail;
    }

    // ============ 第二步：curl 校验服务 ============
    build_message(ANSI_GREEN, "Running curl check");
    printf("curl http://127.0.0.1:8080\n");

    nob_cmd_append(&cmd, "curl", "http://127.0.0.1:8080");

    if (!nob_cmd_run(&cmd)) {
        build_message(ANSI_RED, "curl failed");
        goto fail;
    }

    build_message(ANSI_GREEN, "Benchmark successful");
    return true;

fail:
    return false;
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    run_bench(argc,argv);
    return 0;
}
