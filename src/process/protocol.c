#include "protocol.h"
#include "pa1.h"
#include "ipc_transport.h"
#include "philosophers.h"

/** Направляет полученный кадр обработчику барьеров или философов. */
static int dispatch_message(Process* process, local_id sender, const Message* message) {
    MessageType type = (MessageType)message->s_header.s_type;
    switch (type) {
        case STARTED:
            process->barriers.started[sender] = 1;
            return 0;
        case DONE:
            process->barriers.done[sender] = 1;
            return 0;
        case CS_REQUEST:
        case CS_REPLY:
            return philosophers_receive(process, sender, message);
        default:
            errno = EBADMSG;
            return -1;
    }
}

/** Проверяет получение события от каждого другого дочернего процесса. */
static int barrier_complete(const Process* process, MessageType type) {
    const int* received = type == STARTED ? process->barriers.started : process->barriers.done;
    for (int peer = 1; peer < process->count; ++peer) {
        if (peer != process->id && !received[peer]) {
            return 0;
        }
    }
    return 1;
}

int protocol_step(Process* process) {
    Message message;
    local_id sender;
    int received = ipc_receive_step(process, &sender, &message);
    if (received == RECEIVE_ERROR) {
        return -1;
    }
    if (received == RECEIVE_CLOSED && sender != PARENT_ID && !process->barriers.done[sender]) {
        errno = EPIPE;
        return -1;
    }
    if (received == RECEIVE_COMPLETE && dispatch_message(process, sender, &message) < 0) {
        return -1;
    }
    if (philosophers_service(process) < 0) {
        return -1;
    }

    if (received == RECEIVE_PENDING) {
        ipc_pause();
    }
    return 0;
}

int protocol_wait(Process* process, MessageType type) {
    while (!barrier_complete(process, type)) {
        if (protocol_step(process) < 0) {
            return -1;
        }
    }
    return 0;
}

/** Проверяет результат snprintf и заполняет заголовок готового сообщения. */
static int prepare_message(Message* message, MessageType type, int length) {
    if (length < 0 || (size_t)length >= sizeof(message->s_payload)) {
        errno = EIO;
        return -1;
    }

    message->s_header.s_magic = MESSAGE_MAGIC;
    message->s_header.s_payload_len = (uint16_t)length;
    message->s_header.s_type = (int16_t)type;
    message->s_header.s_local_time = 0;
    return 0;
}

/** Логирует подготовленную строку и рассылает её всем остальным процессам. */
static int publish_message(Process* process, const Logs* logs, const Message* message) {
    if (log_event(logs, message->s_payload, message->s_header.s_payload_len) < 0) {
        return -1;
    }
    for (int peer = 0; peer < process->count; ++peer) {
        if (peer == process->id) {
            continue;
        }
        while (send(process, (local_id)peer, message) < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                return -1;
            }
            if (protocol_step(process) < 0) {
                return -1;
            }
        }
    }
    return 0;
}

/** Формирует и записывает сообщение о завершении барьера. */
static int log_barrier(const Process* process, const Logs* logs, const char* format) {
    char line[128];
    int length = snprintf(line, sizeof(line), format, (int)process->id);
    if (length < 0 || (size_t)length >= sizeof(line)) {
        errno = EIO;
        return -1;
    }
    return log_event(logs, line, (size_t)length);
}

/** Рассылает STARTED и ожидает запуска всех остальных детей. */
static int synchronize_start(Process* process, const Logs* logs) {
    Message message;
    int length = snprintf(message.s_payload, sizeof(message.s_payload), log_started_fmt,
                          (int)process->id, (int)getpid(), (int)getppid());
    if (prepare_message(&message, STARTED, length) < 0) {
        return -1;
    }
    if (publish_message(process, logs, &message) < 0) {
        return -1;
    }
    if (protocol_wait(process, STARTED) < 0) {
        return -1;
    }

    return log_barrier(process, logs, log_received_all_started_fmt);
}

/** Рассылает DONE и ожидает завершения работы всех остальных детей. */
static int synchronize_finish(Process* process, const Logs* logs) {
    Message message;
    int length = snprintf(message.s_payload, sizeof(message.s_payload), log_done_fmt,
                          (int)process->id);
    if (prepare_message(&message, DONE, length) < 0) {
        return -1;
    }
    if (publish_message(process, logs, &message) < 0) {
        return -1;
    }
    if (protocol_wait(process, DONE) < 0) {
        return -1;
    }

    return log_barrier(process, logs, log_received_all_done_fmt);
}

int protocol_run_child(Process* process, const Logs* logs) {
    philosophers_initialize(process);
    if (synchronize_start(process, logs) < 0) {
        return -1;
    }

    if (perform_work(process) < 0 || synchronize_finish(process, logs) < 0) {
        return -1;
    }
    return 0;
}

int protocol_run_parent(Process* process) {
    if (protocol_wait(process, STARTED) < 0) {
        return -1;
    }
    return protocol_wait(process, DONE);
}
