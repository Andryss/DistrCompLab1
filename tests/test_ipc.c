#include "platform.h"
#include "process.h"
#include "channels.h"
#include "test_write.h"
#include "test_cases.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static Process peers[3];

/** Создаёт чистые контексты для каждого теста; дескрипторами владеет fixture. */
void test_ipc_setup(void) {
    memset(peers, 0, sizeof(peers));
    for (int id = 0; id < 3; ++id) {
        peers[id].id = (local_id)id;
        peers[id].count = 3;
        channels_initialize(&peers[id]);
    }
}

/** Закрывает каналы также после неуспешного утверждения cmocka. */
void test_ipc_teardown(void) {
    test_write_reset();
    for (int id = 0; id < 3; ++id) {
        channels_close(&peers[id]);
    }
}

/** Соединяет контексты настоящим pipe с неблокирующим чтением. */
static void connect_peers(int sender, int receiver) {
    int descriptors[2];
    assert_int_equal(0, pipe(descriptors));
    peers[sender].ipc.descriptors[sender][receiver][PIPE_WRITE] = descriptors[1];
    peers[receiver].ipc.descriptors[sender][receiver][PIPE_READ] = descriptors[0];
    assert_int_equal(0, fcntl(descriptors[1], F_SETFL, O_NONBLOCK));
    int flags = fcntl(descriptors[0], F_GETFL);
    assert_true(flags >= 0);
    assert_int_equal(0, fcntl(descriptors[0], F_SETFL, flags | O_NONBLOCK));
}

/** Заполняет неиспользуемый хвост, чтобы тест обнаружил его случайную передачу. */
static Message make_message(const char* text) {
    Message message;
    memset(&message, 0x5a, sizeof(message));
    message.s_header.s_magic = MESSAGE_MAGIC;
    message.s_header.s_payload_len = (uint16_t)strlen(text);
    message.s_header.s_type = STARTED;
    message.s_header.s_local_time = 0;
    memcpy(message.s_payload, text, strlen(text));
    return message;
}

/** Проверяет длину и байты тела без предположения о завершающем NUL. */
static void assert_payload(const Message* message, const char* text) {
    assert_int_equal(MESSAGE_MAGIC, message->s_header.s_magic);
    assert_int_equal(strlen(text), message->s_header.s_payload_len);
    if (strlen(text) > 0) {
        assert_memory_equal(text, message->s_payload, strlen(text));
    }
}

void test_ipc_wire_format(void** state) {
    (void)state;
    connect_peers(0, 1);
    Message message = make_message("hello\n");
    unsigned char wire[MAX_MESSAGE_LEN];

    assert_int_equal(0, send(&peers[0], 1, &message));
    ssize_t length = read(peers[1].ipc.descriptors[0][1][PIPE_READ], wire, sizeof(wire));

    assert_int_equal(sizeof(MessageHeader) + 6, length);
    assert_memory_equal(&message, wire, (size_t)length);
}

/** Возвращает максимальное тело, которое можно отправить атомарно в этом pipe. */
static size_t atomic_payload_limit(int descriptor) {
    long limit = fpathconf(descriptor, _PC_PIPE_BUF);
    assert_true(limit >= (long)sizeof(MessageHeader));
    size_t frame = (size_t)limit < MAX_MESSAGE_LEN ? (size_t)limit : MAX_MESSAGE_LEN;
    return frame - sizeof(MessageHeader);
}

void test_ipc_empty_and_maximum_payload(void** state) {
    (void)state;
    connect_peers(0, 1);
    Message message = make_message("");
    Message received;
    assert_int_equal(0, send(&peers[0], 1, &message));
    assert_int_equal(0, receive(&peers[1], 0, &received));
    assert_payload(&received, "");
    size_t payload = atomic_payload_limit(peers[0].ipc.descriptors[0][1][PIPE_WRITE]);
    message.s_header.s_payload_len = (uint16_t)payload;
    memset(message.s_payload, 'x', MAX_PAYLOAD_LEN);
    assert_int_equal(0, send(&peers[0], 1, &message));

    assert_int_equal(0, receive(&peers[1], 0, &received));
    assert_int_equal(payload, received.s_header.s_payload_len);
    assert_memory_equal(message.s_payload, received.s_payload, payload);
}

void test_ipc_multicast(void** state) {
    (void)state;
    connect_peers(1, 0);
    connect_peers(1, 2);
    Message message = make_message("all");
    Message received;

    assert_int_equal(0, send_multicast(&peers[1], &message));

    assert_int_equal(0, receive(&peers[0], 1, &received));
    assert_payload(&received, "all");
    assert_int_equal(0, receive(&peers[2], 1, &received));
    assert_payload(&received, "all");
}

void test_ipc_receive_pending(void** state) {
    (void)state;
    alarm(5); // Блокирующий приём должен завершить тест по таймауту.
    connect_peers(1, 0);
    Message message = make_message("ready"), received;

    assert_int_equal(-1, receive(&peers[0], 1, &received));
    assert_int_equal(EAGAIN, errno);

    assert_int_equal(0, send(&peers[1], 0, &message));
    Message next = make_message("next");
    assert_int_equal(0, send(&peers[1], 0, &next));
    assert_int_equal(0, receive(&peers[0], 1, &received));
    assert_payload(&received, "ready");
    assert_int_equal(0, receive(&peers[0], 1, &received));
    assert_payload(&received, "next");
    assert_int_equal(-1, receive(&peers[0], 1, &received));
    assert_int_equal(EAGAIN, errno);
}

void test_ipc_receive_any_pending(void** state) {
    (void)state;
    alarm(5); // Отсутствие данных не должно запускать внутреннее ожидание.
    connect_peers(1, 0);
    connect_peers(2, 0);
    Message message = make_message("still open"), received;

    assert_int_equal(-1, receive_any(&peers[0], &received));
    assert_int_equal(EAGAIN, errno);
    assert_int_equal(-1, peers[0].ipc.receive.sender);

    // EOF сообщается отдельно; следующий вызов получает кадр другого отправителя.
    assert_int_equal(0, send(&peers[2], 0, &message));
    close(peers[1].ipc.descriptors[1][0][PIPE_WRITE]);
    peers[1].ipc.descriptors[1][0][PIPE_WRITE] = -1;
    assert_int_equal(-1, receive_any(&peers[0], &received));
    assert_int_equal(EPIPE, errno);
    assert_int_equal(1, peers[0].ipc.receive.sender);
    assert_int_equal(0, receive_any(&peers[0], &received));
    assert_int_equal(2, peers[0].ipc.receive.sender);
    assert_payload(&received, "still open");
    assert_int_equal(-1, receive_any(&peers[0], &received));
    assert_int_equal(EAGAIN, errno);
    assert_int_equal(-1, peers[0].ipc.receive.sender);

    close(peers[2].ipc.descriptors[2][0][PIPE_WRITE]);
    peers[2].ipc.descriptors[2][0][PIPE_WRITE] = -1;
    assert_int_equal(-1, receive_any(&peers[0], &received));
    assert_int_equal(EPIPE, errno);
}

/** Дописывает кадр с задержкой, отправляет следующий и завершается (вызывается только в ребёнке). */
static void send_delayed_tail(const Message* message, size_t used) {
    struct timespec delay = {0, 20000000L};
    while (nanosleep(&delay, &delay) < 0 && errno == EINTR) { }
    int writer = peers[1].ipc.descriptors[1][0][PIPE_WRITE];
    size_t remaining = sizeof(MessageHeader) + message->s_header.s_payload_len - used;
    if (write(writer, (const char*)message + used, remaining) != (ssize_t)remaining) {
        _exit(EXIT_FAILURE);
    }
    Message next = make_message("next");
    _exit(send(&peers[1], 0, &next) == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

void test_ipc_partial_reception(void** state) {
    (void)state;
    alarm(5);
    connect_peers(1, 0);
    Message message = make_message("fragment"), received;
    size_t fragments[] = {3, sizeof(MessageHeader), sizeof(MessageHeader) + 2};
    for (int any = 0; any < 2; ++any) {
        for (size_t index = 0; index < sizeof(fragments) / sizeof(fragments[0]); ++index) {
            size_t used = fragments[index];
            assert_int_equal(used, write(peers[1].ipc.descriptors[1][0][PIPE_WRITE], &message, used));
            pid_t child = fork();
            assert_true(child >= 0);
            if (child == 0) {
                send_delayed_tail(&message, used);
            }

            int result = any ? receive_any(&peers[0], &received) : receive(&peers[0], 1, &received);
            int status;
            pid_t waited;
            do {
                waited = waitpid(child, &status, 0);
            } while (waited < 0 && errno == EINTR);
            assert_int_equal(child, waited);
            assert_true(WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS);
            assert_int_equal(0, result);
            assert_payload(&received, "fragment");
            assert_int_equal(0, receive(&peers[0], 1, &received));
            assert_payload(&received, "next");
            assert_int_equal(-1, receive_any(&peers[0], &received));
            assert_int_equal(EAGAIN, errno);
        }
    }
}

void test_ipc_round_robin(void** state) {
    (void)state;
    connect_peers(1, 0);
    connect_peers(2, 0);
    Message busy = make_message("busy");
    Message other = make_message("other");
    Message received;
    assert_int_equal(0, send(&peers[1], 0, &busy));
    assert_int_equal(0, send(&peers[1], 0, &busy));
    assert_int_equal(0, send(&peers[2], 0, &other));

    assert_int_equal(0, receive_any(&peers[0], &received));
    assert_payload(&received, "busy");
    assert_int_equal(1, peers[0].ipc.receive.sender);
    assert_int_equal(0, receive_any(&peers[0], &received));
    assert_payload(&received, "other");
    assert_int_equal(2, peers[0].ipc.receive.sender);
    assert_int_equal(0, receive_any(&peers[0], &received));
    assert_payload(&received, "busy");
    assert_int_equal(1, peers[0].ipc.receive.sender);
}


void test_ipc_backpressure(void** state) {
    (void)state;
    connect_peers(0, 1);
    connect_peers(0, 2);
    Message message = make_message("");
    size_t payload = atomic_payload_limit(peers[0].ipc.descriptors[0][1][PIPE_WRITE]);
    message.s_header.s_payload_len = (uint16_t)payload;
    memset(message.s_payload, 'q', MAX_PAYLOAD_LEN);

    int sent = 0;
    while (sent < 256) {
        memcpy(message.s_payload, &sent, sizeof(sent));
        if (send(&peers[0], 1, &message) < 0) {
            assert_int_equal(EAGAIN, errno);
            break;
        }
        ++sent;
    }
    assert_true(sent > 0 && sent < 256);
    Message other = make_message("other");
    Message received;
    assert_int_equal(0, send(&peers[0], 2, &other));
    assert_int_equal(0, receive(&peers[2], 0, &received));
    assert_payload(&received, "other");

    // После переполнения вызывающий код повторяет только непринятое сообщение.
    for (int expected = 0; expected < 256;) {
        if (sent < 256) {
            memcpy(message.s_payload, &sent, sizeof(sent));
            if (send(&peers[0], 1, &message) == 0) {
                ++sent;
            } else {
                assert_int_equal(EAGAIN, errno);
            }
        }
        int result = receive_any(&peers[1], &received);
        assert_true(result == 0 || errno == EAGAIN);
        if (result == 0) {
            assert_int_equal(0, peers[1].ipc.receive.sender);
            int actual;
            memcpy(&actual, received.s_payload, sizeof(actual));
            assert_int_equal(expected, actual);
            ++expected;
            assert_int_equal(payload, received.s_header.s_payload_len);
            for (size_t index = sizeof(actual); index < payload; ++index) {
                assert_int_equal('q', received.s_payload[index]);
            }
        }
    }
    assert_int_equal(256, sent);
}

void test_ipc_atomic_writes(void** state) {
    (void)state;
    connect_peers(0, 1);
    test_write_inject(peers[0].ipc.descriptors[0][1][PIPE_WRITE], MAX_MESSAGE_LEN, 1, 1);
    Message message = make_message("atomic message");
    Message received;

    // EINTR повторяется, EAGAIN не оставляет в канале ни одного байта.
    assert_int_equal(-1, send(&peers[0], 1, &message));
    assert_int_equal(EAGAIN, errno);
    assert_int_equal(-1, receive_any(&peers[1], &received));
    assert_int_equal(EAGAIN, errno);
    assert_int_equal(0, send(&peers[0], 1, &message));
    assert_int_equal(0, receive(&peers[1], 0, &received));
    assert_payload(&received, "atomic message");
    assert_int_equal(-1, receive_any(&peers[1], &received));
    assert_int_equal(EAGAIN, errno);

    // Искусственное нарушение гарантии должно завершиться ошибкой, а не дописыванием.
    message = make_message("broken atomicity");
    test_write_inject(peers[0].ipc.descriptors[0][1][PIPE_WRITE], 3, 0, 0);
    assert_int_equal(-1, send(&peers[0], 1, &message));
    assert_int_equal(EIO, errno);
    test_write_reset();
}

void test_ipc_errors(void** state) {
    (void)state;
    connect_peers(0, 1);
    Message message = make_message("broken"), received;
    assert_int_equal(-1, send(&peers[0], 2, &message));
    assert_int_equal(EBADF, errno);

    // Повреждённый заголовок отвергается до чтения тела.
    message.s_header.s_magic = 0;
    assert_int_equal(sizeof(MessageHeader),
                     write(peers[0].ipc.descriptors[0][1][PIPE_WRITE], &message, sizeof(MessageHeader)));
    assert_int_equal(-1, receive(&peers[1], 0, &received));
    assert_int_equal(EBADMSG, errno);
    message.s_header.s_magic = MESSAGE_MAGIC;
    message.s_header.s_payload_len = MAX_PAYLOAD_LEN + 1;
    assert_int_equal(sizeof(MessageHeader),
                     write(peers[0].ipc.descriptors[0][1][PIPE_WRITE], &message, sizeof(MessageHeader)));
    assert_int_equal(-1, receive(&peers[1], 0, &received));
    assert_int_equal(EBADMSG, errno);

    message = make_message("broken");
    assert_int_equal(3, write(peers[0].ipc.descriptors[0][1][PIPE_WRITE], &message, 3));
    close(peers[0].ipc.descriptors[0][1][PIPE_WRITE]);
    peers[0].ipc.descriptors[0][1][PIPE_WRITE] = -1;
    assert_int_equal(-1, receive(&peers[1], 0, &received));
    assert_int_equal(EBADMSG, errno);

    connect_peers(2, 1);
    size_t fragment = sizeof(MessageHeader) + 2;
    assert_int_equal(fragment, write(peers[2].ipc.descriptors[2][1][PIPE_WRITE], &message, fragment));
    close(peers[2].ipc.descriptors[2][1][PIPE_WRITE]);
    peers[2].ipc.descriptors[2][1][PIPE_WRITE] = -1;
    assert_int_equal(-1, receive(&peers[1], 2, &received));
    assert_int_equal(EBADMSG, errno);
}

/** Проверяет фактическое закрытие дескриптора, а не только значение в таблице. */
static void assert_closed(int descriptor) {
    assert_int_equal(-1, fcntl(descriptor, F_GETFD));
    assert_int_equal(EBADF, errno);
}

void test_channels_nonblocking(void** state) {
    (void)state;
    Process* process = &peers[1];
    Logs logs = {.events_fd = -1, .pipes_fd = open("/dev/null", O_WRONLY)};
    assert_true(logs.pipes_fd >= 0);
    assert_int_equal(0, channels_create(process, &logs));
    assert_int_equal(0, logs_close(&logs));
    int opened[3][3][2];
    for (int source = 0; source < 3; ++source) {
        for (int destination = 0; destination < 3; ++destination) {
            for (int end = 0; end < 2; ++end) {
                int descriptor = process->ipc.descriptors[source][destination][end];
                opened[source][destination][end] = descriptor;
                if (source == destination) {
                    assert_int_equal(-1, descriptor);
                } else {
                    int flags = fcntl(descriptor, F_GETFL);
                    assert_true(flags >= 0 && (flags & O_NONBLOCK));
                }
            }
        }
    }

    // У процесса 1 остаются два входа (0 -> 1, 2 -> 1) и два выхода (1 -> 0, 1 -> 2).
    int retained[] = {opened[0][1][0], opened[2][1][0], opened[1][0][1], opened[1][2][1]};
    channels_keep_local(process);
    for (int source = 0; source < 3; ++source) {
        for (int destination = 0; destination < 3; ++destination) {
            for (int end = 0; end < 2; ++end) {
                int original = opened[source][destination][end];
                int current = process->ipc.descriptors[source][destination][end];
                int keep = 0;
                for (size_t index = 0; index < sizeof(retained) / sizeof(retained[0]); ++index) {
                    keep |= original == retained[index];
                }
                if (keep) {
                    assert_int_equal(original, current);
                    int flags = fcntl(current, F_GETFL);
                    assert_true(flags >= 0 && (flags & O_NONBLOCK));
                } else {
                    assert_int_equal(-1, current);
                    assert_closed(original);
                }
            }
        }
    }
    channels_close(process);
    for (size_t index = 0; index < sizeof(retained) / sizeof(retained[0]); ++index) {
        assert_closed(retained[index]);
    }
}

void test_channels_cleanup(void** state) {
    (void)state;
    Process* process = &peers[0];
    // Незавершённая подготовка до fork(): процесс пока владеет обоими концами чужого канала.
    int* descriptors = process->ipc.descriptors[1][2];
    assert_int_equal(0, pipe(descriptors));
    int reader = descriptors[PIPE_READ], writer = descriptors[PIPE_WRITE];
    channels_close(process);
    assert_closed(reader);
    assert_closed(writer);
    assert_int_equal(-1, descriptors[PIPE_READ]);
    assert_int_equal(-1, descriptors[PIPE_WRITE]);

    // Повторная очистка не должна закрыть новый файл с переиспользованным номером.
    int unrelated = open("/dev/null", O_RDONLY);
    assert_true(unrelated >= 0);
    channels_close(process);
    assert_true(fcntl(unrelated, F_GETFD) >= 0);
    assert_int_equal(0, close(unrelated));
}

void test_ipc_closed_channels(void** state) {
    (void)state;
    connect_peers(1, 0);
    connect_peers(2, 0);
    int closed_descriptor = peers[0].ipc.descriptors[1][0][PIPE_READ];
    close(peers[1].ipc.descriptors[1][0][PIPE_WRITE]);
    peers[1].ipc.descriptors[1][0][PIPE_WRITE] = -1;

    Message message = make_message("still open"), received;
    assert_int_equal(-1, receive_any(&peers[0], &received));
    assert_int_equal(EPIPE, errno);
    assert_int_equal(1, peers[0].ipc.receive.sender);
    assert_int_equal(-1, peers[0].ipc.descriptors[1][0][PIPE_READ]);
    assert_int_equal(-1, fcntl(closed_descriptor, F_GETFD));
    assert_int_equal(EBADF, errno);
    assert_int_equal(-1, receive_any(&peers[0], &received));
    assert_int_equal(EAGAIN, errno);
    assert_int_equal(-1, peers[0].ipc.receive.sender);
    assert_int_equal(0, send(&peers[2], 0, &message));
    assert_int_equal(0, receive_any(&peers[0], &received));
    assert_payload(&received, "still open");
    assert_int_equal(2, peers[0].ipc.receive.sender);
    close(peers[2].ipc.descriptors[2][0][PIPE_WRITE]);
    peers[2].ipc.descriptors[2][0][PIPE_WRITE] = -1;
    assert_int_equal(-1, receive_any(&peers[0], &received));
    assert_int_equal(EPIPE, errno);
    assert_int_equal(2, peers[0].ipc.receive.sender);
    assert_int_equal(-1, receive_any(&peers[0], &received));
    assert_int_equal(EPIPE, errno);
    assert_int_equal(-1, peers[0].ipc.receive.sender);
    assert_int_equal(-1, receive(&peers[0], 1, &received));
    assert_int_equal(EPIPE, errno);
    assert_int_equal(-1, receive_any(&peers[0], &received));
    assert_int_equal(EPIPE, errno);
}
