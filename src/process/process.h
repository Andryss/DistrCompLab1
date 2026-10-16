/** @file
 * Объявления модуля process.
 */
#ifndef DISTR_PROCESS_H
#define DISTR_PROCESS_H

#include "platform.h"
#include "ipc.h"

/** Максимальное число процессов с учётом родителя. */
#define PROCESS_LIMIT (MAX_PROCESS_ID + 1)

/** Состояние сборки одного сообщения из входящего канала. */
typedef struct {
    Message message; /**< Буфер заголовка и тела текущего сообщения. */
    size_t used; /**< Число уже прочитанных байтов, включая заголовок. */
    int closed; /**< Признак EOF: отправитель закрыл свой конец записи. */
} ReceiveState;

/** Состояние процесса в алгоритме обедающих философов. */
typedef enum {
    THINKING, /**< Процесс не запрашивает вход и может отдавать грязные вилки. */
    HUNGRY, /**< Процесс собирает вилки для входа в критическую секцию. */
    EATING /**< Процесс находится в критической секции и не отдаёт вилки. */
} PhilosopherState;

/** Каналы и состояние неблокирующего приёма. */
typedef struct {
    int in[PROCESS_LIMIT]; /**< Дескрипторы чтения по ID отправителя; -1, если закрыт. */
    int out[PROCESS_LIMIT]; /**< Дескрипторы записи по ID получателя; -1, если закрыт. */
    ReceiveState rx[PROCESS_LIMIT]; /**< Сборка кадров и признаки закрытия входящих каналов. */
    int next_peer; /**< ID, с которого начнётся следующий круговой обход отправителей. */
} ProcessIpc;

/** Полученные сообщения каждого соседа для двух барьеров протокола. */
typedef struct {
    int started[PROCESS_LIMIT]; /**< Получен ли STARTED от ребёнка с данным ID. */
    int done[PROCESS_LIMIT]; /**< Получен ли DONE от ребёнка с данным ID. */
} ProcessBarriers;

/** Настройка взаимного исключения и локальные ресурсы философа. */
typedef struct {
    int enabled; /**< Включена ли защита вызовов print() алгоритмом философов. */
    PhilosopherState state; /**< Текущее состояние философа. */
    int fork[PROCESS_LIMIT]; /**< Владеет ли процесс вилкой для пары с данным соседом. */
    int dirty[PROCESS_LIMIT]; /**< Загрязнена ли принадлежащая процессу вилка. */
    int req[PROCESS_LIMIT]; /**< Владеет ли процесс маркером запроса данной вилки. */
} ProcessPhilosopher;

/** Локальное состояние процесса, передаваемое функциям IPC через self.
 * Контекст всегда инициализирован: 0 <= id < count <= PROCESS_LIMIT. */
typedef struct {
    local_id id; /**< Локальный ID; у родителя PARENT_ID. */
    int count; /**< Общее число процессов вместе с родителем. */
    ProcessIpc ipc; /**< Каналы, сборка сообщений и порядок обхода отправителей. */
    ProcessBarriers barriers; /**< Отметки участников стартового и завершающего барьеров. */
    ProcessPhilosopher philosopher; /**< Состояние философа, вилки и маркеры запросов. */
} Process;

#endif // DISTR_PROCESS_H
