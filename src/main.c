#include "main.h"

/** Проверяет форму команды и возвращает число детей либо -1 при ошибке. */
static int parse_children(int argc, char** argv) {
    if (argc != 3 || strcmp(argv[1], "-p") != 0 || argv[2][0] == '\0') {
        return -1;
    }

    const char* number = argv[2];
    if (number[0] < '0' || number[0] > '9') {
        return -1;
    }

    char* end;
    errno = 0;
    long child_count = strtol(number, &end, 10);
    if (errno == ERANGE || *end != '\0' || child_count < 0 || child_count > MAX_PROCESS_ID) {
        return -1;
    }
    return (int)child_count;
}

int main(int argc, char** argv) {
    int child_count = parse_children(argc, argv);
    if (child_count < 0) {
        fprintf(stderr, "Usage: %s -p X (0 <= X <= %d)\n", argv[0], MAX_PROCESS_ID);
        return EXIT_FAILURE;
    }

    if (runtime_execute(child_count) < 0) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
