#include "platform.h"
#include "logging.h"
#include "common.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

/** Дописывает блокирующий вывод, повторяя запись после EINTR.
 * При -1 errno: EIO — запись 0 байт; иначе ошибка write(). */
static int io_write_all(int descriptor, const void* data, size_t size) {
    const unsigned char* bytes = data;

    while (size > 0) {
        ssize_t result = write(descriptor, bytes, size);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (result == 0) {
            errno = EIO;
            return -1;
        }
        bytes += (size_t)result;
        size -= (size_t)result;
    }

    return 0;
}

/** Закрывает и сбрасывает дескриптор даже при ошибке.
 * При -1 errno сохраняет ошибку close(). */
static int close_log(int* descriptor) {
    if (*descriptor < 0) {
        return 0;
    }

    int opened = *descriptor;
    *descriptor = -1;
    return close(opened);
}

int logs_open(Logs* logs) {
    // O_APPEND добавляет каждую запись в конец; 0644 задаёт права rw-r--r-- с учётом umask.
    logs->events_fd = open(events_log, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0644);
    if (logs->events_fd < 0) {
        return -1;
    }

    logs->pipes_fd = open(pipes_log, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (logs->pipes_fd < 0) {
        return -1;
    }

    return 0;
}

int logs_close_pipes(Logs* logs) {
    return close_log(&logs->pipes_fd);
}

int logs_close(Logs* logs) {
    int error = 0;
    if (close_log(&logs->events_fd) < 0) {
        error = errno;
    }
    if (close_log(&logs->pipes_fd) < 0 && error == 0) {
        error = errno;
    }
    if (error != 0) {
        errno = error;
        return -1;
    }

    return 0;
}

int log_event(const Logs* logs, const char* text, size_t length) {
    if (io_write_all(logs->events_fd, text, length) < 0) {
        return -1;
    }
    return io_write_all(STDOUT_FILENO, text, length);
}

int log_channel(const Logs* logs, int source, int destination, int read_fd, int write_fd) {
    char line[128];
    int length = snprintf(line, sizeof(line), "Pipe %d -> %d: read=%d write=%d\n",
                          source, destination, read_fd, write_fd);
    if (length < 0 || (size_t)length >= sizeof(line)) {
        errno = EIO;
        return -1;
    }

    return io_write_all(logs->pipes_fd, line, (size_t)length);
}
