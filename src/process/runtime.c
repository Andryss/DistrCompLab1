// Открывает объявление kill() при сборке с -std=c99.
#define _POSIX_C_SOURCE 200809L

#include "runtime.h"
#include "channels.h"
#include "protocol.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/** Ресурсы запуска; после fork каждый процесс работает со своей копией. */
typedef struct {
    Process process; /**< Контекст IPC текущего процесса. */
    Logs logs; /**< Открытые файлы логирования. */
    pid_t children[MAX_PROCESS_ID]; /**< PID успешно созданных детей. */
    int child_count; /**< Число успешно созданных детей. */
} Runtime;

/** Инициализирует контекст до выделения ресурсов (вызывается только в родителе). */
static void runtime_initialize(Runtime* runtime, int child_count) {
    memset(runtime, 0, sizeof(*runtime));
    runtime->process.count = child_count + 1;
    runtime->process.id = PARENT_ID;
    runtime->logs.events_fd = -1;
    runtime->logs.pipes_fd = -1;

    channels_initialize(&runtime->process);
}

/** Готовит сигналы, логи и каналы (вызывается только в родителе до fork()).
 * При -1 errno сохраняет ошибку настройки сигналов, логов или каналов. */
static int runtime_prepare(Runtime* runtime) {
    // Получаем EPIPE вместо завершения по SIGPIPE; настройку унаследуют дети.
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        return -1;
    }
    if (logs_open(&runtime->logs) < 0) {
        return -1;
    }
    if (channels_create(&runtime->process, &runtime->logs) < 0) {
        return -1;
    }

    return logs_close_pipes(&runtime->logs);
}

/** Завершает созданных детей при ошибке (вызывается только в родителе до waitpid()). */
static void terminate_children(const Runtime* runtime) {
    for (int child = 0; child < runtime->child_count; ++child) {
        // SIGKILL завершает в том числе остановленного ребёнка.
        kill(runtime->children[child], SIGKILL);
    }
}

/** Ожидает завершения ребёнка (вызывается только в родителе).
 * При -1 errno: ECHILD — сигнал или ненулевой код завершения; иначе ошибка waitpid(). */
static int wait_for_child(pid_t child) {
    int status;
    pid_t result;
    // Повторяем ожидание только при его прерывании сигналом.
    do {
        result = waitpid(child, &status, 0);
    } while (result < 0 && errno == EINTR);

    if (result < 0) {
        return -1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS) {
        errno = ECHILD;
        return -1;
    }
    return 0;
}

/** Собирает всех детей (вызывается только в родителе).
 * При -1 errno сохраняет последнюю ошибку wait_for_child(). */
static int wait_for_all_children(const Runtime* runtime) {
    int error = 0;
    for (int child = 0; child < runtime->child_count; ++child) {
        if (wait_for_child(runtime->children[child]) < 0) {
            error = errno;
        }
    }
    if (error != 0) {
        errno = error;
        return -1;
    }
    return 0;
}

/** Выполняет протокол и завершается через _exit() (вызывается только в ребёнке). */
static void execute_child(Runtime* runtime, local_id id) {
    runtime->process.id = id;
    channels_keep_local(&runtime->process);
    int result = protocol_run_child(&runtime->process, &runtime->logs);
    if (result < 0) {
        perror("child process");
    }
    channels_close(&runtime->process);
    if (logs_close(&runtime->logs) < 0) {
        perror("child logs");
        result = -1;
    }

    _exit(result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

/** Создаёт детей и сохраняет их PID (вызывается только в родителе).
 * При -1 errno сохраняет ошибку fork(). */
static int create_children(Runtime* runtime) {
    for (int id = 1; id < runtime->process.count; ++id) {
        pid_t child = fork();
        if (child < 0) {
            return -1;
        }
        if (child == 0) {
            // Выполняет протокол без возврата в этот цикл (ветка ребёнка).
            execute_child(runtime, (local_id)id);
        }

        // Сохраняет PID и продолжает создавать детей (ветка родителя).
        runtime->children[runtime->child_count] = child;
        ++runtime->child_count;
    }
    return 0;
}

/** Готовит систему и ждёт STARTED/DONE (вызывается только в родителе).
 * При -1 errno сохраняет ошибку подготовки, fork() или протокола. */
static int execute_parent(Runtime* runtime) {
    if (runtime_prepare(runtime) < 0) {
        return -1;
    }
    if (create_children(runtime) < 0) {
        return -1;
    }
    channels_keep_local(&runtime->process);
    return protocol_run_parent(&runtime->process);
}

int runtime_execute(int child_count, int mutex_enabled) {
    Runtime runtime;
    runtime_initialize(&runtime, child_count);
    runtime.process.philosopher.enabled = mutex_enabled;

    int error = 0;
    if (execute_parent(&runtime) < 0) {
        error = errno;
        perror("runtime");
    }

    channels_close(&runtime.process);
    if (error != 0) {
        terminate_children(&runtime);
    }
    if (wait_for_all_children(&runtime) < 0 && error == 0) {
        error = errno;
        perror("runtime children");
    }
    if (logs_close(&runtime.logs) < 0 && error == 0) {
        error = errno;
        perror("runtime logs");
    }

    // Ошибки очистки не заменяют причину сбоя основного этапа.
    errno = error;
    return error == 0 ? 0 : -1;
}
