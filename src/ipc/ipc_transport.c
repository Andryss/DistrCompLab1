#include "ipc_transport.h"

/** Результат одного неблокирующего шага приёма. */
enum receive_status {
    RECEIVE_ERROR = -1,
    RECEIVE_PENDING = 0,
    RECEIVE_COMPLETE = 1
};

/** Устанавливает errno и возвращает единый код ошибки IPC. */
static int fail(int error) {
    errno = error;
    return -1;
}

/** Проверяет наличие контекста и границы его идентификаторов. */
static int valid_process(const Process* process) {
    return process != NULL
        && process->count > 0 && process->count <= PROCESS_LIMIT
        && process->id >= 0 && process->id < process->count;
}

/** Проверяет, что адресат существует и отличается от текущего процесса. */
static int valid_peer(const Process* process, local_id peer) {
    return valid_process(process)
        && peer >= 0 && peer < process->count && peer != process->id;
}

/** Проверяет указатель, магическую подпись и допустимую длину тела. */
static int valid_message(const Message* message) {
    return message != NULL
        && message->s_header.s_magic == MESSAGE_MAGIC
        && message->s_header.s_payload_len <= MAX_PAYLOAD_LEN;
}

/** Не занимает CPU непрерывным опросом, когда во входящих каналах нет данных. */
static void pause_reader(void) {
    struct timespec delay = {0, 1000000L};
    while (nanosleep(&delay, &delay) < 0 && errno == EINTR) {
        // Продолжаем оставшуюся часть паузы после сигнала.
    }
}

int send(void* self, local_id dst, const Message* message) {
    Process* process = self;
    if (!valid_peer(process, dst) || !valid_message(message)) {
        return fail(EINVAL);
    }
    if (process->out[dst] < 0) {
        return fail(EBADF);
    }

    size_t size = sizeof(MessageHeader) + message->s_header.s_payload_len;
    return io_write_all(process->out[dst], message, size);
}

int send_multicast(void* self, const Message* message) {
    Process* process = self;
    if (!valid_process(process) || !valid_message(message)) {
        return fail(EINVAL);
    }

    for (int destination = 0; destination < process->count; ++destination) {
        if (destination == process->id) {
            continue;
        }
        if (send(process, (local_id)destination, message) < 0) {
            return -1;
        }
    }
    return 0;
}

/** EOF посреди кадра означает повреждённое сообщение, а не чистое закрытие. */
static int finish_channel(ReceiveState* state) {
    state->closed = 1;
    if (state->used > 0) {
        state->error = EBADMSG;
        return fail(state->error);
    }
    return fail(EPIPE);
}

/** Читает только оставшуюся часть заголовка или тела, не затрагивая следующий кадр. */
static int read_fragment(int descriptor, ReceiveState* state, size_t target) {
    unsigned char* buffer = (unsigned char*)&state->message;
    for (;;) {
        ssize_t received = read(descriptor, buffer + state->used, target - state->used);
        if (received > 0) {
            state->used += (size_t)received;
            return RECEIVE_COMPLETE;
        }
        if (received == 0) {
            return finish_channel(state);
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return RECEIVE_PENDING;
        }

        state->error = errno;
        return RECEIVE_ERROR;
    }
}

/** Собирает полный кадр; неполный кадр остаётся в состоянии его отправителя. */
static int read_frame(Process* process, int sender, Message* message) {
    ReceiveState* state = &process->rx[sender];
    if (state->error != 0) {
        return fail(state->error);
    }
    if (state->closed || process->in[sender] < 0) {
        return fail(EPIPE);
    }

    for (;;) {
        size_t target = sizeof(MessageHeader);
        if (state->used >= target) {
            if (!valid_message(&state->message)) {
                state->error = EBADMSG;
                return fail(state->error);
            }
            target += state->message.s_header.s_payload_len;
            if (state->used == target) {
                memcpy(message, &state->message, target);
                state->used = 0;
                return RECEIVE_COMPLETE;
            }
        }

        int result = read_fragment(process->in[sender], state, target);
        if (result != RECEIVE_COMPLETE) {
            return result;
        }
    }
}

int receive(void* self, local_id from, Message* message) {
    Process* process = self;
    if (!valid_peer(process, from) || message == NULL) {
        return fail(EINVAL);
    }

    for (;;) {
        int result = read_frame(process, from, message);
        if (result == RECEIVE_COMPLETE) {
            return 0;
        }
        if (result == RECEIVE_ERROR) {
            return -1;
        }
        pause_reader();
    }
}

/** Делает один круг по отправителям и сдвигает начало после успешного приёма. */
static int scan_channels(Process* process, Message* message) {
    int open_peers = 0;
    for (int step = 0; step < process->count; ++step) {
        int sender = (process->next_peer + step) % process->count;
        if (sender == process->id || process->in[sender] < 0) {
            continue;
        }

        int result = read_frame(process, sender, message);
        if (result == RECEIVE_COMPLETE) {
            process->next_peer = (sender + 1) % process->count;
            return RECEIVE_COMPLETE;
        }
        if (result == RECEIVE_ERROR) {
            if (process->rx[sender].closed && process->rx[sender].error == 0) {
                continue;
            }
            return RECEIVE_ERROR;
        }
        ++open_peers;
    }

    if (open_peers == 0) {
        return fail(EPIPE);
    }
    return RECEIVE_PENDING;
}

int receive_any(void* self, Message* message) {
    Process* process = self;
    if (!valid_process(process) || message == NULL) {
        return fail(EINVAL);
    }

    for (;;) {
        int result = scan_channels(process, message);
        if (result == RECEIVE_COMPLETE) {
            return 0;
        }
        if (result == RECEIVE_ERROR) {
            return -1;
        }
        pause_reader();
    }
}
