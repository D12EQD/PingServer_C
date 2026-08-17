#define NOB_IMPLEMENTATION
#define NOB_WARN_DEPRECATED
#define NOB_EXPERIMENTAL_DELETE_OLD

#include "nob.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define ANSI_RED "\033[31m"
#define ANSI_GREEN "\033[32m"
#define ANSI_RESET "\033[0m"

/*
* using X macro to define build_ctl
*/
#define BUILD_X \
    X(BUILD_MAIN, main) \
    X(BUILD_TEST, test) \

// build type count
#define BUILD_TYPE_COUNT 2

// build type enum
#define X(a, b) a,
    enum BuildType { BUILD_X };
#undef X

// build type names
#define X(a, b) #b,      // # 意思是""包裹
    static const char *build_names[] = { BUILD_X };
#undef X

const char *src_list[] = {
    "src/core/ping_alloc.c",
    "src/ds/linklist.c",
    "src/other/debug.c",
};

const char *gcc_cmd_list[] = {
    "-Wall", "-Wextra", "-g", "-fsanitize=address", "-Iinclude",
    "-DPINGNET_DEBUG_ENABLE", // 开启debug mode
    "-Wno-unused-parameter"
};

static inline enum BuildType get_build_type(const char *name) {
    int len = BUILD_TYPE_COUNT;
    for (int i = 0; i < len; i++) {
        if (strcmp(name, build_names[i]) == 0)
            return (enum BuildType)i;
    }
    return -1;
}

#define build_message(start_color, message) \
    printf(start_color "============== " message " ==============" ANSI_RESET "\n" )

static inline bool run_main(int, char **) {
    build_message(ANSI_GREEN, "building main");
    Nob_Cmd cmd = {0};
    int len;

    nob_cmd_append(&cmd, "gcc");

    len = sizeof(gcc_cmd_list) / sizeof(gcc_cmd_list[0]);
    for (int i = 0; i < len; i ++) nob_cmd_append(&cmd, gcc_cmd_list[i]);
    
    len = sizeof(src_list) / sizeof(src_list[0]);
    for (int i = 0; i < len; i ++) nob_cmd_append(&cmd, src_list[i]);

    nob_cmd_append(&cmd, "src/main.c");
    nob_cmd_append(&cmd, "-o", "build/main");

    if (!nob_cmd_run(&cmd)) return false;

    nob_cmd_free(cmd);
    build_message(ANSI_GREEN, "build sucess");
    return true;

    nob_cmd_free(cmd);
    return false;
}

static inline bool run_test(int argc, char **argv) {
    if (argc == 0){
        build_message(ANSI_RED, "no test file name");
        return false;
    }

    char *test_file = argv[0];
    if (!nob_file_exists(test_file)){
        build_message(ANSI_RED, "test file not exists");
        goto clean;
    }

    int len;
    build_message(ANSI_GREEN, "building test");
    Nob_Cmd cmd = {0};

    nob_cmd_append(&cmd, "gcc");
    len = sizeof(gcc_cmd_list) / sizeof(gcc_cmd_list[0]);
    for (int i = 0; i < len; i ++) nob_cmd_append(&cmd, gcc_cmd_list[i]);
    
    len = sizeof(src_list) / sizeof(src_list[0]);
    for (int i = 0; i < len; i ++) nob_cmd_append(&cmd, src_list[i]);


    nob_cmd_append(&cmd, test_file, "-o", "build/test");
    
    if (!nob_cmd_run(&cmd)){
        build_message(ANSI_RED, "gcc error");
        goto clean;
    }

    nob_cmd_free(cmd);
    build_message(ANSI_GREEN, "build sucess");
    return true;

clean:
    nob_cmd_free(cmd);
    return false;
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    if (argc < 2){
        build_message(ANSI_RED, "Wrong argument!");
        return 0;
    }

    int build_type = get_build_type(argv[1]);
    argc -= 2; argv += 2;

    switch (build_type) {
        #define X(a, b) case a: if (!run_##b(argc, argv)) { build_message(ANSI_RED, "Building error"); } break;
            BUILD_X
        #undef X
        
        default: // 未知类型
            printf("Unknown build target\n");
            return 1;
    }

    return 0;
}