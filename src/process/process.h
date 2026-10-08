#ifndef PA1_PROCESS_H
#define PA1_PROCESS_H

#include "platform.h"
#include "ipc.h"

/** Максимальное число процессов с учётом родителя. */
#define PROCESS_LIMIT (MAX_PROCESS_ID + 1)

/** Состояние сборки одного сообщения из входящего канала. */
typedef struct {
    Message message; /**< Буфер заголовка и тела текущего сообщения. */
    size_t used; /**< Число уже прочитанных байтов, включая заголовок. */
    int error; /**< Сохранённый errno ошибки канала; 0, если ошибки нет. */
    int closed; /**< Признак EOF: отправитель закрыл свой конец записи. */
} ReceiveState;

/** Локальное состояние процесса, передаваемое функциям IPC через self. */
typedef struct {
    local_id id; /**< Локальный ID; у родителя PARENT_ID. */
    int count; /**< Общее число процессов вместе с родителем. */
    int in[PROCESS_LIMIT]; /**< Дескрипторы чтения по ID отправителя; -1, если закрыт. */
    int out[PROCESS_LIMIT]; /**< Дескрипторы записи по ID получателя; -1, если закрыт. */
    ReceiveState rx[PROCESS_LIMIT]; /**< Незавершённое сообщение и состояние каждого отправителя. */
    int next_peer; /**< ID, с которого начнётся следующий обход receive_any(). */
} Process;

#endif // PA1_PROCESS_H
