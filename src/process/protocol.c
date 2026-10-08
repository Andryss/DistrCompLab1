#include "protocol.h"

/** Формирует строку один раз: её байты будут и в логе, и в сообщении. */
static int format_message(Message* message, MessageType type, const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    int length = vsnprintf(message->s_payload, sizeof(message->s_payload), format, arguments);
    va_end(arguments);

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
    return send_multicast(process, message);
}

/** FIFO сохраняет ранние DONE в канале до перехода к следующему барьеру. */
static int wait_for_children(Process* process, MessageType type) {
    for (int peer = 1; peer < process->count; ++peer) {
        if (peer == process->id) {
            continue;
        }

        Message message;
        if (receive(process, (local_id)peer, &message) < 0) {
            return -1;
        }
        if (message.s_header.s_type != (int)type) {
            errno = EBADMSG;
            return -1;
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
    if (format_message(&message, STARTED, log_started_fmt,
                       (int)process->id, (int)getpid(), (int)getppid()) < 0) {
        return -1;
    }
    if (publish_message(process, logs, &message) < 0) {
        return -1;
    }
    if (wait_for_children(process, STARTED) < 0) {
        return -1;
    }

    return log_barrier(process, logs, log_received_all_started_fmt);
}

/** Рассылает DONE и ожидает завершения работы всех остальных детей. */
static int synchronize_finish(Process* process, const Logs* logs) {
    Message message;
    if (format_message(&message, DONE, log_done_fmt, (int)process->id) < 0) {
        return -1;
    }
    if (publish_message(process, logs, &message) < 0) {
        return -1;
    }
    if (wait_for_children(process, DONE) < 0) {
        return -1;
    }

    return log_barrier(process, logs, log_received_all_done_fmt);
}

int protocol_run_child(Process* process, const Logs* logs) {
    if (synchronize_start(process, logs) < 0) {
        return -1;
    }

    // На подготовительном этапе полезная работа отсутствует.
    return synchronize_finish(process, logs);
}

int protocol_run_parent(Process* process) {
    if (wait_for_children(process, STARTED) < 0) {
        return -1;
    }
    return wait_for_children(process, DONE);
}
