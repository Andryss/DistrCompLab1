#ifndef PA1_PROTOCOL_H
#define PA1_PROTOCOL_H

#include "platform.h"
#include "pa1.h"
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include "logging.h"
#include "process.h"

/** Выполняет фазы STARTED и DONE для ребёнка, включая рассылку и логирование. */
int protocol_run_child(Process* process, const Logs* logs);

/** Получает STARTED и DONE от всех детей; родитель ничего не рассылает. */
int protocol_run_parent(Process* process);

#endif // PA1_PROTOCOL_H
