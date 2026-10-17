#include "ipc_transport.h"

/** Устанавливает errno и возвращает единый код ошибки IPC. */
static int fail(int error) {
    errno = error;
    return -1;
}

/** Проверяет, что адресат существует и отличается от текущего процесса. */
static int valid_peer(const Process* process, local_id peer) {
    return peer >= 0 && peer < process->count && peer != process->id;
}

/** Проверяет указатель, магическую подпись и допустимую длину тела. */
static int valid_message(const Message* message) {
    return message != NULL
        && message->s_header.s_magic == MESSAGE_MAGIC
        && message->s_header.s_payload_len <= MAX_PAYLOAD_LEN;
}

/** Не занимает CPU непрерывным опросом, когда во входящих каналах нет данных. */
void ipc_pause(void) {
    struct timespec delay = {0, 1000000L};
    while (nanosleep(&delay, &delay) < 0 && errno == EINTR) {
        // Продолжаем оставшуюся часть паузы после сигнала.
    }
}

/** Записывает байты напрямую, повторяя только прерванный системный вызов. */
static ssize_t write_bytes(int descriptor, const void* bytes, size_t size) {
    ssize_t written;
    do {
        written = write(descriptor, bytes, size);
    } while (written < 0 && errno == EINTR);
    return written;
}

/** Атомарная отправка; при EAGAIN кадр не записан и его можно повторить. */
int send(void* self, local_id dst, const Message* message) {
    Process* process = self;
    if (!valid_peer(process, dst) || !valid_message(message)) {
        return fail(EINVAL);
    }
    size_t size = sizeof(MessageHeader) + message->s_header.s_payload_len;
    errno = 0;
    long limit = fpathconf(process->ipc.descriptors[process->id][dst][PIPE_WRITE], _PC_PIPE_BUF);
    if (limit < 0) {
        return fail(errno != 0 ? errno : EINVAL);
    }
    if (size > (size_t)limit) {
        return fail(EMSGSIZE);
    }

    ssize_t written = write_bytes(process->ipc.descriptors[process->id][dst][PIPE_WRITE], message, size);
    if (written < 0) {
        return -1;
    }
    if ((size_t)written != size) {
        // Нарушение гарантии атомарности: повторять повреждённый кадр нельзя.
        return fail(EIO);
    }
    return 0;
}

/** После частичной рассылки повторять её целиком нельзя. */
int send_multicast(void* self, const Message* message) {
    Process* process = self;
    if (!valid_message(message)) {
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
static int finish_channel(int* descriptor, const ReceiveState* state) {
    if (state->used > 0) {
        return fail(EBADMSG);
    }
    close(*descriptor);
    *descriptor = -1;
    return RECEIVE_CLOSED;
}

/** Читает только оставшуюся часть заголовка или тела, не затрагивая следующий кадр. */
static int read_fragment(int* descriptor, ReceiveState* state, size_t target) {
    unsigned char* buffer = (unsigned char*)&state->message;
    for (;;) {
        ssize_t received = read(*descriptor, buffer + state->used, target - state->used);
        if (received > 0) {
            state->used += (size_t)received;
            return RECEIVE_COMPLETE;
        }
        if (received == 0) {
            return finish_channel(descriptor, state);
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return RECEIVE_PENDING;
        }

        return RECEIVE_ERROR;
    }
}

/** Собирает полный кадр; неполный кадр остаётся в состоянии его отправителя. */
static int read_frame(Process* process, int sender, Message* message) {
    ReceiveState* state = &process->ipc.rx[sender];
    if (process->ipc.descriptors[sender][process->id][PIPE_READ] < 0) {
        return RECEIVE_CLOSED;
    }

    for (;;) {
        size_t target = sizeof(MessageHeader);
        if (state->used >= target) {
            if (!valid_message(&state->message)) {
                return fail(EBADMSG);
            }
            target += state->message.s_header.s_payload_len;
            if (state->used == target) {
                memcpy(message, &state->message, target);
                state->used = 0;
                return RECEIVE_COMPLETE;
            }
        }

        int result = read_fragment(&process->ipc.descriptors[sender][process->id][PIPE_READ], state, target);
        if (result != RECEIVE_COMPLETE) {
            return result;
        }
    }
}

/** Не ждёт данных: EAGAIN — кадр не собран, EPIPE — канал закрыт. */
int receive(void* self, local_id from, Message* message) {
    Process* process = self;
    if (!valid_peer(process, from) || message == NULL) {
        return fail(EINVAL);
    }

    int result = read_frame(process, from, message);
    if (result == RECEIVE_COMPLETE) {
        return 0;
    }
    if (result == RECEIVE_ERROR) {
        return -1;
    }
    return fail(result == RECEIVE_CLOSED ? EPIPE : EAGAIN);
}

/** Делает один круг по открытым каналам и возвращает сообщение либо новое закрытие. */
int ipc_receive_step(Process* process, local_id* source, Message* message) {
    for (int step = 0; step < process->count; ++step) {
        int sender = (process->ipc.next_peer + step) % process->count;
        if (sender == process->id || process->ipc.descriptors[sender][process->id][PIPE_READ] < 0) {
            continue;
        }

        int result = read_frame(process, sender, message);
        if (result == RECEIVE_COMPLETE || result == RECEIVE_CLOSED) {
            *source = (local_id)sender;
            process->ipc.next_peer = (sender + 1) % process->count;
            return result;
        }
        if (result == RECEIVE_ERROR) {
            return RECEIVE_ERROR;
        }
    }
    return RECEIVE_PENDING;
}

/** Проверяет, может ли публичный receive_any() ещё дождаться сообщения. */
static int has_open_channels(const Process* process) {
    for (int peer = 0; peer < process->count; ++peer) {
        if (peer != process->id && process->ipc.descriptors[peer][process->id][PIPE_READ] >= 0) {
            return 1;
        }
    }
    return 0;
}

/** Не ждёт данных: EAGAIN — нет полного кадра, EPIPE — закрыты все входы. */
int receive_any(void* self, Message* message) {
    Process* process = self;
    if (message == NULL) {
        return fail(EINVAL);
    }

    local_id sender;
    int result;
    // Закрытие одного канала не должно скрывать сообщения остальных.
    do {
        result = ipc_receive_step(process, &sender, message);
    } while (result == RECEIVE_CLOSED);

    if (result == RECEIVE_COMPLETE) {
        return 0;
    }
    if (result == RECEIVE_ERROR) {
        return -1;
    }
    return fail(has_open_channels(process) ? EAGAIN : EPIPE);
}
