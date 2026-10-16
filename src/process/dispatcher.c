#include "dispatcher.h"
#include "ipc_transport.h"
#include "philosophers.h"
#include <errno.h>

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

int dispatcher_step(Process* process) {
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

int dispatcher_wait(Process* process, MessageType type) {
    while (!barrier_complete(process, type)) {
        if (dispatcher_step(process) < 0) {
            return -1;
        }
    }
    return 0;
}
