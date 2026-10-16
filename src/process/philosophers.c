#include "philosophers.h"
#include "pa2345.h"
#include "dispatcher.h"
#include <errno.h>
#include <stdio.h>

/** Отправляет пустое сообщение, передающее вилку либо маркер запроса. */
static int transfer_resource(Process* process, int peer, MessageType type) {
    Message message = { .s_header = {MESSAGE_MAGIC, 0, type, 0} };
    if (send(process, (local_id)peer, &message) == 0) {
        return 0;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK
        || (errno == EPIPE && process->philosopher.state == THINKING)) {
        // Закрывшийся сосед уже не нуждается в ответе на старый запрос.
        // Продолжаем приём: его DONE ещё может находиться во входящем канале.
        return 1;
    }
    return -1;
}

/** Передаёт запрошенную грязную вилку; владение снимается после успешной записи. */
static int release_requested_fork(Process* process, int peer) {
    if (!process->philosopher.fork[peer] || !process->philosopher.dirty[peer] || !process->philosopher.req[peer]) {
        return 0;
    }
    int result = transfer_resource(process, peer, CS_REPLY);
    if (result != 0) {
        return result;
    }
    process->philosopher.fork[peer] = 0;
    process->philosopher.dirty[peer] = 0;
    return 0;
}

/** Запрашивает отсутствующую вилку только при наличии её маркера запроса. */
static int request_missing_fork(Process* process, int peer) {
    if (process->philosopher.state != HUNGRY ||
        process->philosopher.fork[peer] || !process->philosopher.req[peer]) {
        return 0;
    }
    int result = transfer_resource(process, peer, CS_REQUEST);
    if (result != 0) {
        return result;
    }
    process->philosopher.req[peer] = 0;
    return 0;
}

/** Проверяет принадлежность сообщения ребру графа конфликтов и пустое тело. */
static int valid_control_message(const Process* process, local_id sender, const Message* message) {
    return sender > PARENT_ID && sender < process->count && sender != process->id
        && process->id != PARENT_ID && process->philosopher.enabled
        && message->s_header.s_payload_len == 0;
}

/** Принимает единственный маркер запроса либо чистую вилку. */
static int accept_resource(Process* process, local_id sender, MessageType type) {
    if (type == CS_REQUEST && !process->philosopher.req[sender]) {
        process->philosopher.req[sender] = 1;
    } else if (type == CS_REPLY && !process->philosopher.fork[sender]) {
        process->philosopher.fork[sender] = 1;
        process->philosopher.dirty[sender] = 0;
    } else {
        errno = EBADMSG;
        return -1;
    }
    return 0;
}

/** Отмечает вход в КС и загрязняет все имеющиеся вилки. */
static void enter_critical_section(Process* process) {
    process->philosopher.state = EATING;
    for (int peer = 1; peer < process->count; ++peer) {
        if (process->philosopher.fork[peer]) {
            process->philosopher.dirty[peer] = 1;
        }
    }
}

void philosophers_initialize(Process* process) {
    process->philosopher.state = THINKING;
    for (int peer = 1; peer < process->count; ++peer) {
        if (peer == process->id) {
            continue;
        }
        process->philosopher.fork[peer] = process->id < peer;
        process->philosopher.dirty[peer] = process->philosopher.fork[peer];
        process->philosopher.req[peer] = process->id > peer;
    }
}

int philosophers_service(Process* process) {
    if (!process->philosopher.enabled || process->id == PARENT_ID || process->philosopher.state == EATING) {
        return 0;
    }

    for (int peer = 1; peer < process->count; ++peer) {
        if (peer == process->id) {
            continue;
        }
        if (release_requested_fork(process, peer) < 0 || request_missing_fork(process, peer) < 0) {
            return -1;
        }
    }
    return 0;
}

int philosophers_receive(Process* process, local_id sender, const Message* message) {
    if (!valid_control_message(process, sender, message)) {
        errno = EBADMSG;
        return -1;
    }
    if (accept_resource(process, sender, (MessageType)message->s_header.s_type) < 0) {
        return -1;
    }
    return philosophers_service(process);
}

int philosophers_ready(const Process* process) {
    for (int peer = 1; peer < process->count; ++peer) {
        if (peer == process->id) {
            continue;
        }
        if (!process->philosopher.fork[peer] || (process->philosopher.dirty[peer] && process->philosopher.req[peer])) {
            return 0;
        }
    }
    return 1;
}

int request_cs(const void* self) {
    Process* process = (Process*)self;
    if (process == NULL || process->id == PARENT_ID ||
        process->philosopher.state != THINKING) {
        errno = EINVAL;
        return -1;
    }

    process->philosopher.state = HUNGRY;
    if (philosophers_service(process) < 0) {
        return -1;
    }
    while (!philosophers_ready(process)) {
        if (dispatcher_step(process) < 0) {
            return -1;
        }
    }
    enter_critical_section(process);
    return 0;
}

int release_cs(const void* self) {
    Process* process = (Process*)self;
    if (process == NULL || process->philosopher.state != EATING) {
        errno = EINVAL;
        return -1;
    }

    process->philosopher.state = THINKING;
    return philosophers_service(process);
}

/** Печатает одну строку, при необходимости защищая ровно этот вызов print(). */
static int print_iteration(Process* process, int iteration, int total) {
    char line[128];
    int length = snprintf(line, sizeof(line), log_loop_operation_fmt,
                          process->id, iteration, total);
    if (length < 0 || (size_t)length >= sizeof(line)) {
        errno = EIO;
        return -1;
    }

    if (process->philosopher.enabled && request_cs(process) < 0) {
        return -1;
    }
    print(line);
    if (process->philosopher.enabled && release_cs(process) < 0) {
        return -1;
    }
    return 0;
}

int perform_work(Process* process) {
    int total = 5 * process->id;
    for (int iteration = 1; iteration <= total; ++iteration) {
        if (print_iteration(process, iteration, total) < 0) {
            return -1;
        }
        if (dispatcher_step(process) < 0) {
            return -1;
        }
    }
    return 0;
}
