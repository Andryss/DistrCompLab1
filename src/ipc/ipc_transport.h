/** @file
 * Объявления модуля ipc_transport.
 */
#ifndef DISTR_IPC_TRANSPORT_H
#define DISTR_IPC_TRANSPORT_H

#include "platform.h"
#include "process.h"
#include "io.h"
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/** Результат одного неблокирующего шага приёма. */
enum receive_status {
    RECEIVE_ERROR = -1, /**< Ошибка чтения или повреждённый кадр. */
    RECEIVE_PENDING = 0, /**< Новых сообщений и закрытий каналов пока нет. */
    RECEIVE_COMPLETE = 1, /**< Получено полное сообщение от указанного отправителя. */
    RECEIVE_CLOSED = 2 /**< Канал указанного отправителя закрылся на границе сообщений. */
};

/** Возвращает receive_status; sender — для COMPLETE/CLOSED, message — для COMPLETE. */
int ipc_receive_step(Process* process, local_id* sender, Message* message);

/** Делает короткую паузу, когда нет полного сообщения или события закрытия. */
void ipc_pause(void);

#endif // DISTR_IPC_TRANSPORT_H
