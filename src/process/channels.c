#include "channels.h"

enum { PIPE_READ = 0, PIPE_WRITE = 1 };

void channels_initialize(Channels* channels) {
    for (int source = 0; source < PROCESS_LIMIT; ++source) {
        for (int destination = 0; destination < PROCESS_LIMIT; ++destination) {
            channels->descriptors[source][destination][PIPE_READ] = -1;
            channels->descriptors[source][destination][PIPE_WRITE] = -1;
        }
    }
}

int channels_create(Channels* channels, int process_count, const Logs* logs) {
    for (int source = 0; source < process_count; ++source) {
        for (int destination = 0; destination < process_count; ++destination) {
            if (source == destination) {
                continue;
            }

            int* descriptors = channels->descriptors[source][destination];
            if (pipe(descriptors) < 0) {
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

/** Забирает нужные концы из общей таблицы без дублирования владения. */
static int attach_peer(Channels* channels, Process* process, int peer) {
    process->ipc.in[peer] = channels->descriptors[peer][process->id][PIPE_READ];
    process->ipc.out[peer] = channels->descriptors[process->id][peer][PIPE_WRITE];
    channels->descriptors[peer][process->id][PIPE_READ] = -1;
    channels->descriptors[process->id][peer][PIPE_WRITE] = -1;

    int descriptors[] = {process->ipc.in[peer], process->ipc.out[peer]};
    for (int i = 0; i < 2; ++i) {
        int flags = fcntl(descriptors[i], F_GETFL);
        if (flags < 0 || fcntl(descriptors[i], F_SETFL, flags | O_NONBLOCK) < 0) {
            return -1;
        }
    }
    return 0;
}

int channels_attach(Channels* channels, Process* process) {
    for (int peer = 0; peer < process->count; ++peer) {
        if (peer == process->id) {
            continue;
        }
        if (attach_peer(channels, process, peer) < 0) {
            return -1;
        }
    }

    channels_close(channels, process->count);
    return 0;
}

void channels_close(Channels* channels, int process_count) {
    for (int source = 0; source < process_count; ++source) {
        for (int destination = 0; destination < process_count; ++destination) {
            for (int end = PIPE_READ; end <= PIPE_WRITE; ++end) {
                int* descriptor = &channels->descriptors[source][destination][end];
                if (*descriptor >= 0) {
                    close(*descriptor);
                    *descriptor = -1;
                }
            }
        }
    }
}

void channels_close_process(Process* process) {
    for (int peer = 0; peer < process->count; ++peer) {
        if (process->ipc.in[peer] >= 0) {
            close(process->ipc.in[peer]);
            process->ipc.in[peer] = -1;
        }
        if (process->ipc.out[peer] >= 0) {
            close(process->ipc.out[peer]);
            process->ipc.out[peer] = -1;
        }
    }
}
