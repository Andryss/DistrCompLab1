#include "process.h"
#include <errno.h>
#include <sched.h>
#include <unistd.h>

/** Возвращает -1 с указанной причиной ошибки.
 * Записывает error в errno; причину определяет вызывающий код. */
static int fail(int error) {
    errno = error;
    return -1;
}

/** Отправляет кадр размером не больше PIPE_BUF одним неблокирующим write().
 * При -1 errno: EIO — короткая запись; иначе ошибка write(). */
int send(void* self, local_id dst, const Message* message) {
    Process* process = self;
    size_t size = sizeof(MessageHeader) + message->s_header.s_payload_len;
    int descriptor = process->ipc.descriptors[process->id][dst][PIPE_WRITE];

    ssize_t written;
    do {
        written = write(descriptor, message, size);
    } while (written < 0 && errno == EINTR);
    if (written < 0) {
        // EAGAIN — места нет, отправку можно повторить; EPIPE — нет читателя.
        return -1;
    }
    if ((size_t)written != size) {
        // Нарушение гарантии атомарности: повторять повреждённый кадр нельзя.
        return fail(EIO);
    }
    return 0;
}

/** Рассылает сообщение всем другим процессам; частичную рассылку целиком не повторяют.
 * При -1 errno сохраняет первую ошибку send(). */
int send_multicast(void* self, const Message* message) {
    Process* process = self;

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

/** Дочитывает начатый кадр до нужного размера.
 * При -1 errno: EAGAIN — пусто до первого байта, EPIPE — EOF между кадрами, EBADMSG — EOF внутри; иначе ошибка read(). */
static int read_bytes(int* descriptor, Message* message, size_t* used, size_t target) {
    unsigned char* buffer = (unsigned char*)message;
    while (*used < target) {
        ssize_t received = read(*descriptor, buffer + *used, target - *used);
        if (received > 0) {
            *used += (size_t)received;
            continue;
        }
        if (received == 0) {
            if (*used > 0) {
                return fail(EBADMSG);
            }
            close(*descriptor);
            *descriptor = -1;
            return fail(EPIPE);
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN) {
            if (*used == 0) {
                return -1;
            }
            sched_yield();
            continue;
        }

        return -1;
    }
    return 0;
}

/** Читает заголовок и тело, дочитывая начатый кадр целиком.
 * При -1 errno: EBADMSG — неверный заголовок, EPIPE — закрытый вход; иначе ошибка read_bytes(). */
static int read_frame(Process* process, int sender, Message* message) {
    int* descriptor = &process->ipc.descriptors[sender][process->id][PIPE_READ];
    if (*descriptor < 0) {
        return fail(EPIPE);
    }

    size_t used = 0;
    int result = read_bytes(descriptor, message, &used, sizeof(MessageHeader));
    if (result < 0) {
        return result;
    }
    if (message->s_header.s_magic != MESSAGE_MAGIC || message->s_header.s_payload_len > MAX_PAYLOAD_LEN) {
        return fail(EBADMSG);
    }
    return read_bytes(descriptor, message, &used, sizeof(MessageHeader) + message->s_header.s_payload_len);
}

/** Принимает сообщение от указанного отправителя.
 * При -1 errno: EAGAIN — пусто, EPIPE — вход закрыт, EBADMSG — повреждённый/оборванный кадр; иначе ошибка read(). */
int receive(void* self, local_id from, Message* message) {
    Process* process = self;
    return read_frame(process, from, message);
}

/** Принимает сообщение круговым обходом, сохраняя отправителя сообщения/ошибки в ReceiveContext.
 * При -1 errno: EAGAIN — все входы пусты, EPIPE — закрыт вход или все входы, EBADMSG — повреждённый/оборванный кадр; иначе ошибка read(). */
int receive_any(void* self, Message* message) {
    Process* process = self;
    ReceiveContext* context = &process->ipc.receive;
    context->sender = -1;

    int open_channels = 0;
    for (int step = 0; step < process->count; ++step) {
        int sender = (context->next_peer + step) % process->count;
        if (sender == process->id || process->ipc.descriptors[sender][process->id][PIPE_READ] < 0) {
            continue;
        }

        ++open_channels;
        int result = read_frame(process, sender, message);
        if (result < 0 && errno == EAGAIN) {
            continue;
        }
        context->sender = (local_id)sender;
        context->next_peer = (sender + 1) % process->count;
        return result;
    }
    if (open_channels == 0) {
        return fail(EPIPE);
    }
    sched_yield();
    return fail(EAGAIN);
}
