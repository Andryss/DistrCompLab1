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

/* send(): 0 — кадр целиком записан.
 * Размер кадра ограничен _PC_PIPE_BUF канала; превышение даёт EMSGSIZE.
 * -1/EAGAIN — новый кадр не принят; вызывающий код должен повторить его позже.
 * send_multicast() останавливается на первой ошибке, как требует ipc.h:
 * успешные предыдущие получатели не должны получать повторную рассылку. */

/** Принимает кадр и ID отправителя: 1 — готовый кадр, 0 — пока нет, -1 — ошибка. */
int ipc_receive_step(Process* process, local_id* sender, Message* message);

/** Делает короткую паузу, когда при обработке каналов не было прогресса. */
void ipc_pause(void);

#endif // DISTR_IPC_TRANSPORT_H
