#include "test_support.h"
#include "test_cases.h"
#include "process.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Проверяет точные строки событий, их порядок в каждом процессе и общие барьеры. */
static void check_events(const char* path, int children, char started[PROCESS_LIMIT][256]) {
    FILE* file = fopen(path, "r");
    assert_true(file != NULL);
    int phase[PROCESS_LIMIT] = {0};
    int started_count = 0;
    int done_count = 0;
    char line[256];
    while (fgets(line, sizeof(line), file) != NULL) {
        int id, offset = 0;
        assert_true(sscanf(line, "Process %d %n", &id, &offset) == 1);
        assert_true(id >= 1 && id <= children && phase[id] < 4);
        char expected[256];
        switch (phase[id]) {
            case 0: {
                int pid, parent, end = 0;
                assert_true(sscanf(line + offset, "(pid %d, parent %d) has STARTED%n", &pid, &parent, &end) == 2);
                assert_true(pid > 0 && parent > 0 && !strcmp(line + offset + end, "\n"));
                snprintf(expected, sizeof(expected), "Process %1d (pid %5d, parent %5d) has STARTED\n", id, pid, parent);
                if (started[id][0] != '\0') {
                    assert_true(!strcmp(started[id], line));
                }
                strcpy(started[id], line);
                ++started_count;
                break;
            }
            case 1:
                assert_true(started_count == children);
                snprintf(expected, sizeof(expected), "Process %1d received all STARTED messages\n", id);
                break;
            case 2:
                snprintf(expected, sizeof(expected), "Process %1d has DONE its work\n", id);
                ++done_count;
                break;
            default:
                assert_true(done_count == children);
                snprintf(expected, sizeof(expected), "Process %1d received all DONE messages\n", id);
                break;
        }
        assert_true(!strcmp(line, expected));
        ++phase[id];
    }
    assert_true(!ferror(file));
    assert_true(fclose(file) == 0);
    for (int id = 1; id <= children; ++id) {
        assert_true(phase[id] == 4);
    }
}

/** Проверяет наличие всех направлений и отсутствие повторно назначенных дескрипторов. */
static void check_topology(const char* directory, int children) {
    char path[PATH_MAX];
    test_path(path, directory, "pipes.log");
    FILE* file = fopen(path, "r");
    assert_true(file != NULL);
    int pairs[PROCESS_LIMIT][PROCESS_LIMIT] = {{0}};
    int descriptors[2 * PROCESS_LIMIT * PROCESS_LIMIT];
    int descriptor_count = 0;
    char line[256];
    while (fgets(line, sizeof(line), file) != NULL) {
        int source, destination, read_fd, write_fd, end = 0;
        assert_true(sscanf(line, "Pipe %d -> %d: read=%d write=%d%n", &source, &destination,
                      &read_fd, &write_fd, &end) == 4);
        assert_true(!strcmp(line + end, "\n"));
        assert_true(source >= 0 && source <= children && destination >= 0 && destination <= children);
        assert_true(source != destination && pairs[source][destination] == 0);
        assert_true(read_fd >= 0 && write_fd >= 0 && read_fd != write_fd);
        for (int index = 0; index < descriptor_count; ++index) {
            assert_true(descriptors[index] != read_fd && descriptors[index] != write_fd);
        }
        descriptors[descriptor_count++] = read_fd;
        descriptors[descriptor_count++] = write_fd;
        pairs[source][destination] = 1;
    }
    assert_true(!ferror(file));
    assert_true(fclose(file) == 0);
    for (int source = 0; source <= children; ++source) {
        for (int destination = 0; destination <= children; ++destination) {
            assert_true(pairs[source][destination] == (source != destination));
        }
    }
}

/** Проверяет коды завершения, пустой stderr и совпадение событий в двух выводах. */
static void check_success(const char* binary, const char* directory, int children) {
    char count[16];
    snprintf(count, sizeof(count), "%d", children);
    char* arguments[] = {(char*)binary, "-p", count, NULL};
    assert_true(test_command(arguments, directory, 10) == 0);
    char* errors = test_stderr();
    assert_true(errors[0] == '\0');
    free(errors);

    char path[PATH_MAX];
    char started[PROCESS_LIMIT][256] = {{0}};
    test_path(path, directory, "events.log");
    check_events(path, children, started);
    char* output = test_stdout();
    test_path(path, directory, "captured.stdout");
    FILE* captured = fopen(path, "w");
    assert_true(captured != NULL);
    assert_true(fputs(output, captured) >= 0);
    assert_true(fclose(captured) == 0);
    free(output);
    check_events(path, children, started);
    check_topology(directory, children);
}

/** Проверяет ошибки синтаксиса, диапазона и переполнения без создания логов. */
static void check_arguments(const char* binary) {
    const char* cases[][5] = {
        {NULL}, {"-p", NULL}, {"-p", "-1", NULL}, {"-p", "16", NULL},
        {"-p", "1x", NULL}, {"-p", "", NULL}, {"-p", "99999999999999999", NULL},
        {"-p", "999999999999999999999999999999999999", NULL},
        {"-p", "1", "-p", "2", NULL}, {"--mutexl", NULL}, {"-p", "1", "extra", NULL},
        {"-p", "+1", NULL}, {"-p", " 1", NULL}
    };
    for (size_t test = 0; test < sizeof(cases) / sizeof(cases[0]); ++test) {
        char directory[PATH_MAX];
        test_case_directory(directory);
        char* arguments[6] = {(char*)binary, NULL};
        for (size_t index = 0; cases[test][index] != NULL; ++index) {
            arguments[index + 1] = (char*)cases[test][index];
        }
        assert_true(test_command(arguments, directory, 10) == EXIT_FAILURE);
        char* errors = test_stderr();
        assert_true(errors[0] != '\0');
        free(errors);
        DIR* contents = opendir(directory);
        assert_true(contents != NULL);
        struct dirent* entry;
        while ((entry = readdir(contents)) != NULL) {
            assert_true(!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."));
        }
        assert_true(closedir(contents) == 0);
    }
}

void test_program(const char* binary) {
    const int counts[] = {0, 1, 2, 5, 9, 10, 15};
    for (size_t index = 0; index < sizeof(counts) / sizeof(counts[0]); ++index) {
        for (int repetition = 0; repetition < 3; ++repetition) {
            char directory[PATH_MAX];
            test_case_directory(directory);
            check_success(binary, directory, counts[index]);
        }
    }
    check_arguments(binary);

    char directory[PATH_MAX];
    test_case_directory(directory);
    check_success(binary, directory, 5);
    check_success(binary, directory, 1);
    puts("Program tests passed");
}
