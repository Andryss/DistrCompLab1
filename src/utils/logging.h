#ifndef PA1_LOGGING_H
#define PA1_LOGGING_H

#include "platform.h"
#include "common.h"
#include "io.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stddef.h>

/** Файлы логов, открытые родителем и наследуемые дочерними процессами. */
typedef struct {
    int events_fd; /**< Дескриптор events.log; -1, если файл не открыт. */
    int pipes_fd; /**< Дескриптор pipes.log; закрывается до создания детей. */
} Logs;

/** Открывает и очищает оба лога; при частичной ошибке ресурсы освобождает logs_close(). */
int logs_open(Logs* logs);

/** Закрывает лог каналов после создания топологии. */
int logs_close_pipes(Logs* logs);

/** Закрывает оставшиеся логи; возвращает -1 при ошибке закрытия. */
int logs_close(Logs* logs);

/** Записывает готовую строку события в events.log и stdout. */
int log_event(const Logs* logs, const char* text, size_t length);

/** Записывает направление канала и оба его дескриптора в pipes.log. */
int log_channel(const Logs* logs, int source, int destination, int read_fd, int write_fd);

#endif // PA1_LOGGING_H
