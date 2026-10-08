#ifndef PA1_RUNTIME_H
#define PA1_RUNTIME_H

#include "platform.h"
#include "process.h"
#include "channels.h"
#include "logging.h"
#include "protocol.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/**
 * Создаёт сеть и дочерние процессы, выполняет обмен и освобождает ресурсы.
 * @param child_count число детей, проверенное при разборе аргументов
 * @return 0 при успехе, -1 при ошибке; диагностика выводится в stderr
 */
int runtime_execute(int child_count);

#endif // PA1_RUNTIME_H
