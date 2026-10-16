#include "test_support.h"

static char workspace[PATH_MAX];
static char stdout_path[PATH_MAX];
static char stderr_path[PATH_MAX];
static int case_number;

void test_path(char result[PATH_MAX], const char* directory, const char* name) {
    int size = snprintf(result, PATH_MAX, "%s/%s", directory, name);
    assert_true(size >= 0 && size < PATH_MAX);
}

/** Удаляет дерево, не переходя по символическим ссылкам. */
static void remove_tree(const char* path) {
    struct stat info;
    assert_true(lstat(path, &info) == 0);
    if (!S_ISDIR(info.st_mode)) {
        assert_true(unlink(path) == 0);
        return;
    }

    DIR* directory = opendir(path);
    assert_non_null(directory);
    struct dirent* entry;
    while ((entry = readdir(directory)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
            continue;
        }
        char child[PATH_MAX];
        test_path(child, path, entry->d_name);
        remove_tree(child);
    }
    assert_true(closedir(directory) == 0);
    assert_true(rmdir(path) == 0);
}

void test_workspace_open(void) {
    const char* temporary = getenv("TMPDIR");
    test_path(workspace, temporary != NULL ? temporary : "/tmp", "distributed-tests-XXXXXX");
    assert_non_null(mkdtemp(workspace));
    test_path(stdout_path, workspace, "stdout");
    test_path(stderr_path, workspace, "stderr");
}

void test_workspace_close(void) {
    remove_tree(workspace);
}

void test_case_directory(char path[PATH_MAX]) {
    char name[32];
    snprintf(name, sizeof(name), "case-%d", ++case_number);
    test_path(path, workspace, name);
    assert_true(mkdir(path, 0700) == 0);
}

/** Возвращает монотонное время, чтобы смена системных часов не влияла на тайм-аут. */
static double monotonic_time(void) {
    struct timespec value;
    assert_true(clock_gettime(CLOCK_MONOTONIC, &value) == 0);
    return (double)value.tv_sec + (double)value.tv_nsec / 1000000000.0;
}

/** Дожидается команды; при тайм-ауте завершает всю группу, включая потомков. */
static int wait_for_command(pid_t child, int timeout) {
    double deadline = monotonic_time() + timeout;
    int status;
    for (;;) {
        pid_t result = waitpid(child, &status, WNOHANG);
        if (result == child) {
            break;
        }
        assert_true(result == 0 || (result < 0 && errno == EINTR));
        if (monotonic_time() >= deadline) {
            kill(-child, SIGKILL);
            kill(child, SIGKILL);
            while (waitpid(child, &status, 0) < 0 && errno == EINTR) { }
            fprintf(stderr, "Command timed out after %d seconds\n", timeout);
            fail();
        }
        struct timespec pause = {0, 10000000L};
        nanosleep(&pause, NULL);
    }

    errno = 0;
    int alive = kill(-child, 0);
    int error = errno;
    if (alive == 0) {
        kill(-child, SIGKILL);
    }
    assert_true(alive == -1 && error == ESRCH);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}

int test_command(char* const arguments[], const char* directory, int timeout) {
    int output = open(stdout_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    int errors = open(stderr_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    assert_true(output >= 0 && errors >= 0);
    pid_t child = fork();
    assert_true(child >= 0);
    if (child == 0) {
        if (setsid() < 0 || chdir(directory) < 0
            || dup2(output, STDOUT_FILENO) < 0 || dup2(errors, STDERR_FILENO) < 0) {
            _exit(126);
        }
        close(output);
        close(errors);
        execvp(arguments[0], arguments);
        perror(arguments[0]);
        _exit(127);
    }
    close(output);
    close(errors);
    return wait_for_command(child, timeout);
}

char* test_read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    assert_non_null(file);
    assert_true(fseek(file, 0, SEEK_END) == 0);
    long length = ftell(file);
    assert_true(length >= 0);
    rewind(file);
    char* text = malloc((size_t)length + 1);
    if (text == NULL) {
        fclose(file);
        fail();
        return NULL;
    }
    assert_true(fread(text, 1, (size_t)length, file) == (size_t)length);
    text[length] = '\0';
    assert_true(fclose(file) == 0);
    return text;
}

char* test_stdout(void) {
    return test_read_file(stdout_path);
}

char* test_stderr(void) {
    return test_read_file(stderr_path);
}
