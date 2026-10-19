#include "test_support.h"
#include "philosophers_model.h"
#include "philosophers.h"
#include "channels.h"

/** Ограничения размера управляемой модели. */
enum {
    MODEL_COUNT = 5, /**< Четыре ребёнка и исключённый из алгоритма родитель. */
    MAILBOX_SIZE = 64 /**< Максимальное число сообщений в одном моделируемом канале. */
};

/** Сообщения, уже прочитанные из pipe, но ещё не доставленные алгоритму. */
typedef struct {
    int type[MAILBOX_SIZE]; /**< Типы сообщений в FIFO-порядке. */
    int count; /**< Число ожидающих доставки сообщений. */
} Mailbox;

static Process processes[MODEL_COUNT];
static Mailbox mail[MODEL_COUNT][MODEL_COUNT];
static int process_count;

/** Инициализирует локальный контекст, не открывая каналы. */
static void initialize_process(int id, int count) {
    Process* process = &processes[id];
    process->id = (local_id)id;
    process->count = count;
    process->philosopher.enabled = 1;
    channels_initialize(process);
    philosophers_initialize(process);
}

/** Соединяет одну пару настоящим неблокирующим pipe. */
static void connect_processes(int sender, int receiver) {
    int descriptors[2];
    assert_int_equal(0, pipe(descriptors));
    for (int end = 0; end < 2; ++end) {
        assert_int_equal(0, fcntl(descriptors[end], F_SETFL, O_NONBLOCK));
    }
    processes[sender].ipc.descriptors[sender][receiver][PIPE_WRITE] = descriptors[1];
    processes[receiver].ipc.descriptors[sender][receiver][PIPE_READ] = descriptors[0];
}

void test_model_open(int count) {
    assert_true(count >= 2 && count <= MODEL_COUNT);
    memset(processes, 0, sizeof(processes));
    memset(mail, 0, sizeof(mail));
    for (int id = 1; id < count; ++id) {
        initialize_process(id, count);
    }
    process_count = count;
    for (int sender = 1; sender < count; ++sender) {
        for (int receiver = 1; receiver < count; ++receiver) {
            if (sender != receiver) {
                connect_processes(sender, receiver);
            }
        }
    }
}

Process* test_model_process(int id) {
    assert_true(id > 0 && id < process_count);
    return &processes[id];
}

void test_model_close(void) {
    for (int id = 1; id < process_count; ++id) {
        channels_close(&processes[id]);
    }
    process_count = 0;
}

Message test_control_message(int type) {
    Message message = { .s_header = {MESSAGE_MAGIC, 0, (int16_t)type, 0} };
    return message;
}

/** Переносит кадры из транспорта в управляемые очереди доставки. */
static void collect_messages(int id) {
    Message message;
    int result;
    while ((result = receive_any(&processes[id], &message)) == 0) {
        local_id sender = processes[id].ipc.receive.sender;
        Mailbox* mailbox = &mail[sender][id];
        assert_true(mailbox->count < MAILBOX_SIZE);
        mailbox->type[mailbox->count++] = message.s_header.s_type;
    }
    assert_int_equal(-1, result);
    assert_int_equal(EAGAIN, errno);
}

/** Проверяет единственность вилки и маркера на ребре, включая сообщения в пути. */
static void check_edge_resources(int first, int second) {
    int forks = processes[first].philosopher.fork[second] + processes[second].philosopher.fork[first];
    int tokens = processes[first].philosopher.req[second] + processes[second].philosopher.req[first];
    const Mailbox* directions[] = {&mail[first][second], &mail[second][first]};
    for (int direction = 0; direction < 2; ++direction) {
        const Mailbox* mailbox = directions[direction];
        for (int index = 0; index < mailbox->count; ++index) {
            forks += mailbox->type[index] == CS_REPLY;
            tokens += mailbox->type[index] == CS_REQUEST;
        }
    }
    assert_int_equal(1, forks);
    assert_int_equal(1, tokens);
}

/** Фиксирует отправленные сообщения и проверяет безопасность всей модели. */
static void collect_and_check(void) {
    int eating = 0;
    for (int id = 1; id < process_count; ++id) {
        collect_messages(id);
        eating += processes[id].philosopher.state == EATING;
    }
    assert_true(eating <= 1);
    for (int first = 1; first < process_count; ++first) {
        for (int second = first + 1; second < process_count; ++second) {
            check_edge_resources(first, second);
        }
    }
}

/** Доставляет один кадр с сохранением FIFO внутри выбранного канала. */
static void deliver_message(int sender, int receiver) {
    Mailbox* mailbox = &mail[sender][receiver];
    if (mailbox->count == 0) {
        return;
    }
    Message message = test_control_message(mailbox->type[0]);
    --mailbox->count;
    memmove(mailbox->type, mailbox->type + 1, (size_t)mailbox->count * sizeof(int));
    assert_int_equal(0, philosophers_receive(&processes[receiver], (local_id)sender, &message));
}

/** Даёт процессу шаг исполнения, удерживая КС несколько шагов для проверки запросов. */
static void advance_process(int id, int* meals, int* hold) {
    Process* process = &processes[id];
    if (process->philosopher.state == EATING) {
        ++*hold;
        if (*hold == 3) {
            assert_int_equal(0, release_cs(process));
            ++*meals;
            *hold = 0;
        }
        return;
    }

    if (*meals < 3 + id) {
        process->philosopher.state = HUNGRY;
    }
    assert_int_equal(0, philosophers_service(process));
    if (process->philosopher.state == HUNGRY && philosophers_ready(process)) {
        process->philosopher.state = THINKING;
        assert_int_equal(0, request_cs(process));
    }
}

/** Проверяет, завершил ли каждый философ все запланированные входы. */
static int all_meals_completed(const int meals[MODEL_COUNT]) {
    for (int id = 1; id < process_count; ++id) {
        if (meals[id] != 3 + id) {
            return 0;
        }
    }
    return 1;
}

void test_model_run(int schedule) {
    test_model_open(MODEL_COUNT);
    int meals[MODEL_COUNT] = {0};
    int hold[MODEL_COUNT] = {0};
    for (int turn = 0; turn < 2000 && !all_meals_completed(meals); ++turn) {
        for (int slot = 0; slot < MODEL_COUNT - 1; ++slot) {
            int id = 1 + (slot + turn + schedule) % (MODEL_COUNT - 1);
            int sender = 1 + (turn + id + schedule) % (MODEL_COUNT - 1);
            collect_and_check();
            deliver_message(sender, id);
            advance_process(id, &meals[id], &hold[id]);
            collect_and_check();
        }
    }
    assert_true(all_meals_completed(meals));
    test_model_close();
}
