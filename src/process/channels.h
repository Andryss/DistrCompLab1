/** @file
 * Объявления модуля channels.
 */
#ifndef DISTR_CHANNELS_H
#define DISTR_CHANNELS_H

#include "platform.h"
#include <fcntl.h>
#include <unistd.h>
#include "logging.h"
#include "process.h"

/** Все каналы до разделения их концов между процессами. */
typedef struct {
    int descriptors[PROCESS_LIMIT][PROCESS_LIMIT][2]; /**< [отправитель][получатель][чтение/запись]. */
} Channels;

/** Устанавливает -1 для каждого дескриптора до начала выделения ресурсов. */
void channels_initialize(Channels* channels);

/** Создаёт и логирует канал для каждой пары различных процессов. */
int channels_create(Channels* channels, int process_count, const Logs* logs);

/** Передаёт процессу его концы каналов, включает неблокирующие чтение и запись, закрывает остальные. */
int channels_attach(Channels* channels, Process* process);

/** Закрывает концы, ещё принадлежащие общей таблице каналов. */
void channels_close(Channels* channels, int process_count);

/** Закрывает входящие и исходящие дескрипторы текущего процесса. */
void channels_close_process(Process* process);

#endif // DISTR_CHANNELS_H
