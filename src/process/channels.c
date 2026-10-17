#include "channels.h"

void channels_initialize(Process* process) {
    for (int source = 0; source < PROCESS_LIMIT; ++source) {
        for (int destination = 0; destination < PROCESS_LIMIT; ++destination) {
            process->ipc.descriptors[source][destination][PIPE_READ] = -1;
            process->ipc.descriptors[source][destination][PIPE_WRITE] = -1;
        }
    }
}

/** Создаёт pipe и включает неблокирующий режим обоих концов до fork(). */
static int create_channel(int descriptors[2]) {
    if (pipe(descriptors) < 0) {
        return -1;
    }
    for (int end = 0; end < 2; ++end) {
        int flags = fcntl(descriptors[end], F_GETFL);
        if (flags < 0 || fcntl(descriptors[end], F_SETFL, flags | O_NONBLOCK) < 0) {
            return -1;
        }
    }
    return 0;
}

int channels_create(Process* process, const Logs* logs) {
    for (int source = 0; source < process->count; ++source) {
        for (int destination = 0; destination < process->count; ++destination) {
            if (source == destination) {
                continue;
            }

            int* descriptors = process->ipc.descriptors[source][destination];
            if (create_channel(descriptors) < 0) {
                return -1;
            }
            if (log_channel(logs, source, destination,
                            descriptors[PIPE_READ], descriptors[PIPE_WRITE]) < 0) {
                return -1;
            }
        }
    }

    return 0;
}

/** Закрывает дескриптор и исключает его повторное использование при очистке. */
static void close_descriptor(int* descriptor) {
    if (*descriptor >= 0) {
        close(*descriptor);
        *descriptor = -1;
    }
}

void channels_keep_local(Process* process) {
    for (int source = 0; source < process->count; ++source) {
        for (int destination = 0; destination < process->count; ++destination) {
            int* descriptors = process->ipc.descriptors[source][destination];
            if (destination != process->id) {
                close_descriptor(&descriptors[PIPE_READ]);
            }
            if (source != process->id) {
                close_descriptor(&descriptors[PIPE_WRITE]);
            }
        }
    }
}

void channels_close(Process* process) {
    for (int source = 0; source < process->count; ++source) {
        for (int destination = 0; destination < process->count; ++destination) {
            int* descriptors = process->ipc.descriptors[source][destination];
            close_descriptor(&descriptors[PIPE_READ]);
            close_descriptor(&descriptors[PIPE_WRITE]);
        }
    }
}
