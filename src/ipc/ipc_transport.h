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

/* self всегда указывает на корректно инициализированный Process.
 * send(): 0 — кадр целиком записан.
 * Размер кадра ограничен _PC_PIPE_BUF канала; превышение даёт EMSGSIZE.
 * -1/EAGAIN — новый кадр не принят; вызывающий код должен повторить его позже.
 * send_multicast() останавливается на первой ошибке, как требует ipc.h:
 * успешные предыдущие получатели не должны получать повторную рассылку. */

/** Результат одного неблокирующего шага приёма. */
enum receive_status {
    RECEIVE_ERROR = -1, /**< Ошибка чтения или повреждённое сообщение; обмен прекращается. */
    RECEIVE_PENDING = 0, /**< Новых сообщений и закрытий каналов пока нет. */
    RECEIVE_COMPLETE = 1, /**< Получено полное сообщение от указанного отправителя. */
    RECEIVE_CLOSED = 2 /**< Канал указанного отправителя закрылся на границе сообщений. */
};

/** Возвращает receive_status; sender задан для COMPLETE/CLOSED, message — только для COMPLETE. */
int ipc_receive_step(Process* process, local_id* sender, Message* message);

/** Делает короткую паузу, когда нет полного сообщения или события закрытия. */
void ipc_pause(void);

#endif // DISTR_IPC_TRANSPORT_H
