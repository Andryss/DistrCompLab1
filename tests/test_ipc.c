#include "platform.h"
#include "process.h"
#include "ipc_transport.h"
#include "channels.h"
#include "test_write.h"
#include "test_cases.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

static Process peers[3];

/** Создаёт чистые контексты для каждого теста; дескрипторами владеет fixture. */
void test_ipc_setup(void) {
    memset(peers, 0, sizeof(peers));
    for (int id = 0; id < 3; ++id) {
        peers[id].id = (local_id)id;
        peers[id].count = 3;
        for (int other = 0; other < PROCESS_LIMIT; ++other) {
            peers[id].ipc.in[other] = -1;
            peers[id].ipc.out[other] = -1;
        }
    }
}

/** Закрывает каналы также после неуспешного утверждения cmocka. */
void test_ipc_teardown(void) {
    test_write_reset();
    for (int id = 0; id < 3; ++id) {
        for (int other = 0; other < PROCESS_LIMIT; ++other) {
            if (peers[id].ipc.in[other] >= 0) {
                close(peers[id].ipc.in[other]);
            }
            if (peers[id].ipc.out[other] >= 0) {
                close(peers[id].ipc.out[other]);
            }
        }
    }
}

/** Соединяет контексты настоящим pipe с неблокирующим чтением. */
static void connect_peers(int sender, int receiver) {
    int descriptors[2];
    assert_int_equal(0, pipe(descriptors));
    peers[sender].ipc.out[receiver] = descriptors[1];
    peers[receiver].ipc.in[sender] = descriptors[0];
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
    ssize_t length = read(peers[1].ipc.in[0], wire, sizeof(wire));

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
    size_t payload = atomic_payload_limit(peers[0].ipc.out[1]);
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

void test_ipc_partial_reception(void** state) {
    (void)state;
    connect_peers(1, 0);
    connect_peers(2, 0);
    Message partial = make_message("fragment");
    Message ready = make_message("ready");
    Message received;

    assert_int_equal(0, send(&peers[2], 0, &ready));
    assert_int_equal(0, receive_any(&peers[0], &received));
    assert_payload(&received, "ready");

    assert_int_equal(3, write(peers[1].ipc.out[0], &partial, 3));
    assert_int_equal(0, send(&peers[2], 0, &ready));
    assert_int_equal(0, receive_any(&peers[0], &received));
    assert_payload(&received, "ready");

    size_t fragment = sizeof(MessageHeader) - 3 + 2;
    assert_int_equal(fragment, write(peers[1].ipc.out[0], (char*)&partial + 3, fragment));
    assert_int_equal(0, send(&peers[2], 0, &ready));
    assert_int_equal(0, receive_any(&peers[0], &received));
    assert_payload(&received, "ready");

    assert_int_equal(6, write(peers[1].ipc.out[0], partial.s_payload + 2, 6));
    assert_int_equal(0, receive(&peers[0], 1, &received));
    assert_payload(&received, "fragment");
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
    assert_int_equal(0, receive_any(&peers[0], &received));
    assert_payload(&received, "other");
    assert_int_equal(0, receive_any(&peers[0], &received));
    assert_payload(&received, "busy");
}


void test_ipc_backpressure(void** state) {
    (void)state;
    connect_peers(0, 1);
    connect_peers(0, 2);
    Message message = make_message("");
    size_t payload = atomic_payload_limit(peers[0].ipc.out[1]);
    message.s_header.s_payload_len = (uint16_t)payload;
    memset(message.s_payload, 'q', MAX_PAYLOAD_LEN);

    int sent = 0;
    while (sent < 256) {
        memcpy(message.s_payload, &sent, sizeof(sent));
        if (send(&peers[0], 1, &message) < 0) {
            assert_true(errno == EAGAIN || errno == EWOULDBLOCK);
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
                assert_true(errno == EAGAIN || errno == EWOULDBLOCK);
            }
        }
        local_id sender;
        int result = ipc_receive_step(&peers[1], &sender, &received);
        assert_true(result >= 0);
        if (result == RECEIVE_COMPLETE) {
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
    test_write_inject(peers[0].ipc.out[1], MAX_MESSAGE_LEN, 1, 1);
    Message message = make_message("atomic message");
    Message received;
    local_id sender;

    // EINTR повторяется, EAGAIN не оставляет в канале ни одного байта.
    assert_int_equal(-1, send(&peers[0], 1, &message));
    assert_int_equal(EAGAIN, errno);
    assert_int_equal(0, ipc_receive_step(&peers[1], &sender, &received));
    assert_int_equal(0, send(&peers[0], 1, &message));
    assert_int_equal(0, receive(&peers[1], 0, &received));
    assert_payload(&received, "atomic message");
    assert_int_equal(0, ipc_receive_step(&peers[1], &sender, &received));

    // При меньшем платформенном пределе слишком большой кадр отклоняется целиком.
    size_t payload = atomic_payload_limit(peers[0].ipc.out[1]);
    if (payload < MAX_PAYLOAD_LEN) {
        message.s_header.s_payload_len = (uint16_t)(payload + 1);
        assert_int_equal(-1, send(&peers[0], 1, &message));
        assert_int_equal(EMSGSIZE, errno);
        assert_int_equal(0, ipc_receive_step(&peers[1], &sender, &received));
    }

    // Искусственное нарушение гарантии должно завершиться ошибкой, а не дописыванием.
    message = make_message("broken atomicity");
    test_write_inject(peers[0].ipc.out[1], 3, 0, 0);
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
    assert_int_equal(-1, send(&peers[0], 0, &message));
    assert_int_equal(EINVAL, errno);
    assert_int_equal(-1, send(&peers[0], 3, &message));
    assert_int_equal(EINVAL, errno);
    message.s_header.s_magic = 0;
    assert_int_equal(-1, send(&peers[0], 1, &message));
    assert_int_equal(EINVAL, errno);
    message.s_header.s_magic = MESSAGE_MAGIC;
    message.s_header.s_payload_len = MAX_PAYLOAD_LEN + 1;
    assert_int_equal(-1, send(&peers[0], 1, &message));
    message = make_message("broken");
    assert_int_equal(3, write(peers[0].ipc.out[1], &message, 3));
    close(peers[0].ipc.out[1]);
    peers[0].ipc.out[1] = -1;
    assert_int_equal(-1, receive(&peers[1], 0, &received));
    assert_int_equal(EBADMSG, errno);
}

void test_channels_nonblocking(void** state) {
    (void)state;
    Channels channels;
    channels_initialize(&channels);
    int descriptors[2];
    for (int source = 0; source < 3; ++source) {
        for (int destination = 0; destination < 3; ++destination) {
            if (source == destination) {
                continue;
            }
            assert_int_equal(0, pipe(descriptors));
            channels.descriptors[source][destination][0] = descriptors[0];
            channels.descriptors[source][destination][1] = descriptors[1];
        }
    }
    assert_int_equal(0, channels_attach(&channels, &peers[1]));
    for (int peer = 0; peer < 3; ++peer) {
        if (peer == 1) {
            continue;
        }
        assert_true(fcntl(peers[1].ipc.in[peer], F_GETFL) & O_NONBLOCK);
        assert_true(fcntl(peers[1].ipc.out[peer], F_GETFL) & O_NONBLOCK);
    }
}

void test_ipc_closed_channels(void** state) {
    (void)state;
    connect_peers(1, 0);
    connect_peers(2, 0);
    close(peers[1].ipc.out[0]);
    peers[1].ipc.out[0] = -1;

    Message message = make_message("still open"), received;
    local_id sender = -1;
    assert_int_equal(RECEIVE_CLOSED, ipc_receive_step(&peers[0], &sender, &received));
    assert_int_equal(1, sender);
    assert_int_equal(RECEIVE_PENDING, ipc_receive_step(&peers[0], &sender, &received));
    assert_int_equal(0, send(&peers[2], 0, &message));
    assert_int_equal(0, receive_any(&peers[0], &received));
    assert_payload(&received, "still open");

    close(peers[2].ipc.out[0]);
    peers[2].ipc.out[0] = -1;
    assert_int_equal(RECEIVE_CLOSED, ipc_receive_step(&peers[0], &sender, &received));
    assert_int_equal(2, sender);
    assert_int_equal(RECEIVE_PENDING, ipc_receive_step(&peers[0], &sender, &received));
    assert_int_equal(-1, receive(&peers[0], 1, &received));
    assert_int_equal(EPIPE, errno);
    assert_int_equal(-1, receive_any(&peers[0], &received));
    assert_int_equal(EPIPE, errno);
}
