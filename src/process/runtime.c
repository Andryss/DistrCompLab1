#include "runtime.h"

/** Ресурсы запуска; после fork каждый процесс работает со своей копией. */
typedef struct {
    Process process; /**< Контекст IPC текущего процесса. */
    Channels channels; /**< Концы каналов, ещё не переданные контексту. */
    Logs logs; /**< Открытые файлы логирования. */
    pid_t children[MAX_PROCESS_ID]; /**< PID созданных детей; 0 после waitpid(). */
    int child_count; /**< Число успешно созданных детей. */
} Runtime;

/** Инициализирует контекст и отмечает все ресурсы как ещё не открытые. */
static void runtime_initialize(Runtime* runtime, int child_count) {
    memset(runtime, 0, sizeof(*runtime));
    runtime->process.count = child_count + 1;
    runtime->process.id = PARENT_ID;
    runtime->logs.events_fd = -1;
    runtime->logs.pipes_fd = -1;

    for (int peer = 0; peer < PROCESS_LIMIT; ++peer) {
        runtime->process.in[peer] = -1;
        runtime->process.out[peer] = -1;
    }
    channels_initialize(&runtime->channels);
}

/** Настраивает сигналы, открывает логи и создаёт сеть до первого fork(). */
static int runtime_prepare(Runtime* runtime) {
    // Запись в канал без читателей должна вернуть EPIPE, а не завершить процесс
    // сигналом: тогда родитель сможет закрыть ресурсы и собрать оставшихся детей.
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        return -1;
    }
    if (logs_open(&runtime->logs) < 0) {
        return -1;
    }
    if (channels_create(&runtime->channels, runtime->process.count, &runtime->logs) < 0) {
        return -1;
    }

    return logs_close_pipes(&runtime->logs);
}

/** Прекращает работу всех ещё не собранных дочерних процессов. */
static void terminate_children(const Runtime* runtime) {
    for (int child = 0; child < runtime->child_count; ++child) {
        if (runtime->children[child] > 0) {
            // SIGKILL гарантирует завершение в том числе остановленного ребёнка.
            kill(runtime->children[child], SIGKILL);
        }
    }
}

/** Ожидает одного ребёнка и обнуляет PID, чтобы не обращаться к нему повторно. */
static int wait_for_child(pid_t* child) {
    if (*child <= 0) {
        return 0;
    }

    int status;
    pid_t result;
    do {
        result = waitpid(*child, &status, 0);
    } while (result < 0 && errno == EINTR);

    *child = 0;
    if (result < 0) {
        return -1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS) {
        errno = ECHILD;
        return -1;
    }
    return 0;
}

/** Собирает каждого созданного ребёнка, сохраняя ошибку любого из них. */
static int wait_for_all_children(Runtime* runtime) {
    int error = 0;
    for (int child = 0; child < runtime->child_count; ++child) {
        if (wait_for_child(&runtime->children[child]) < 0) {
            error = errno;
        }
    }
    if (error != 0) {
        errno = error;
        return -1;
    }
    return 0;
}

/** Единая очистка работает и при частично завершённой подготовке. */
static int runtime_close(Runtime* runtime, int failed) {
    channels_close(&runtime->channels, runtime->process.count);
    channels_close_process(&runtime->process);

    if (failed && runtime->process.id == PARENT_ID) {
        terminate_children(runtime);
        wait_for_all_children(runtime);
    }
    return logs_close(&runtime->logs);
}

/** Подключает каналы ребёнка, выполняет протокол и завершает процесс. */
static void execute_child(Runtime* runtime, local_id id) {
    runtime->process.id = id;
    int result = channels_attach(&runtime->channels, &runtime->process);
    if (result == 0) {
        result = protocol_run_child(&runtime->process, &runtime->logs);
    }
    if (result < 0) {
        perror("pa1 child");
    }
    if (runtime_close(runtime, result < 0) < 0) {
        perror("pa1 child logs");
        result = -1;
    }

    _exit(result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

/** Создаёт детей только из родителя и запоминает каждый успешный fork(). */
static int create_children(Runtime* runtime) {
    for (int id = 1; id < runtime->process.count; ++id) {
        pid_t child = fork();
        if (child < 0) {
            return -1;
        }
        if (child == 0) {
            execute_child(runtime, (local_id)id);
        }

        runtime->children[runtime->child_count] = child;
        ++runtime->child_count;
    }
    return 0;
}

/** Выполняет подготовку, запускает детей и ждёт их сообщений и завершения. */
static int execute_parent(Runtime* runtime) {
    if (runtime_prepare(runtime) < 0) {
        return -1;
    }
    if (create_children(runtime) < 0) {
        return -1;
    }
    if (channels_attach(&runtime->channels, &runtime->process) < 0) {
        return -1;
    }
    if (protocol_run_parent(&runtime->process) < 0) {
        return -1;
    }

    channels_close_process(&runtime->process);
    return wait_for_all_children(runtime);
}

int runtime_execute(int child_count) {
    Runtime runtime;
    runtime_initialize(&runtime, child_count);

    int result = execute_parent(&runtime);
    if (result < 0) {
        perror("pa1");
    }
    if (runtime_close(&runtime, result < 0) < 0) {
        perror("pa1 logs");
        result = -1;
    }
    return result;
}
