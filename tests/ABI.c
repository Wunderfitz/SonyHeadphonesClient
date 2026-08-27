#include <mdr-c/Headphones.h>

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MDR_ASSERT_U32(type) \
    static_assert(sizeof(type) == sizeof(uint32_t), #type " must be uint32_t")
MDR_ASSERT_U32(MDRResult);
MDR_ASSERT_U32(MDRBoolean);
MDR_ASSERT_U32(MDRFeatureAvailability);
MDR_ASSERT_U32(MDRFeature);
MDR_ASSERT_U32(MDREvent);
MDR_ASSERT_U32(MDRPacketDirection);
MDR_ASSERT_U32(MDRText);
MDR_ASSERT_U32(MDRAudioCodec);
MDR_ASSERT_U32(MDRBatteryPart);
MDR_ASSERT_U32(MDRChargingState);
MDR_ASSERT_U32(MDRPlaybackStatus);
MDR_ASSERT_U32(MDRPlaybackAction);
MDR_ASSERT_U32(MDRNoiseMode);
MDR_ASSERT_U32(MDRAdaptiveSensitivity);
MDR_ASSERT_U32(MDRNoiseButtonMode);
MDR_ASSERT_U32(MDRSpeechSensitivity);
MDR_ASSERT_U32(MDRSpeakTimeout);
MDR_ASSERT_U32(MDRListeningMode);
MDR_ASSERT_U32(MDRRoomSize);
MDR_ASSERT_U32(MDREqualizerPreset);
MDR_ASSERT_U32(MDRDSEEType);
MDR_ASSERT_U32(MDRPairedDeviceCommand);
MDR_ASSERT_U32(MDRGeneralSettingType);
MDR_ASSERT_U32(MDRAssignableAction);
MDR_ASSERT_U32(MDRWearingPowerMode);
MDR_ASSERT_U32(MDRAudioPriority);
#undef MDR_ASSERT_U32

#undef MDR_ASSERT_C_STRUCT

enum
{
    MOCK_BUFFER_CAPACITY = 4096,
    FRAME_BUFFER_CAPACITY = 64,
    MDR_DATA_TYPE_ACK = 1,
    MDR_DATA_TYPE_DATA_MDR = 12
};

typedef struct MockTransport
{
    unsigned char rx[MOCK_BUFFER_CAPACITY];
    size_t rx_size;
    size_t rx_offset;
    unsigned char tx[MOCK_BUFFER_CAPACITY];
    size_t tx_size;
    MDRConnection connection;
} MockTransport;

typedef struct Session
{
    MockTransport transport;
    MDRHeadphones* headphones;
} Session;

static int g_failures;

static void check(int condition, const char* message)
{
    if (condition)
        return;
    fprintf(stderr, "FAIL: %s\n", message);
    ++g_failures;
}

static void check_result(MDRResult actual, MDRResult expected, const char* message)
{
    if (actual == expected)
        return;
    fprintf(
        stderr,
        "FAIL: %s (expected %ld, got %ld)\n",
        message,
        (long)expected,
        (long)actual
    );
    ++g_failures;
}

static MDRResult mock_connect(void* user, const char* address, const char* service)
{
    (void)user;
    (void)address;
    (void)service;
    return MDR_RESULT_OK;
}

static void mock_disconnect(void* user)
{
    (void)user;
}

static MDRResult mock_receive(void* user, char* destination, int size, int* received)
{
    MockTransport* transport = (MockTransport*)user;
    size_t remaining;
    size_t count;

    *received = 0;
    if (transport->rx_offset == transport->rx_size)
        return MDR_RESULT_INPROGRESS;

    remaining = transport->rx_size - transport->rx_offset;
    count = (size_t)size < remaining ? (size_t)size : remaining;
    memcpy(destination, transport->rx + transport->rx_offset, count);
    transport->rx_offset += count;
    *received = (int)count;
    return MDR_RESULT_OK;
}

static MDRResult mock_send(void* user, const char* source, int size, int* sent)
{
    MockTransport* transport = (MockTransport*)user;
    size_t count = (size_t)size;

    if (count > MOCK_BUFFER_CAPACITY - transport->tx_size)
        return MDR_RESULT_ERROR_BUFFER_TOO_SMALL;
    memcpy(transport->tx + transport->tx_size, source, count);
    transport->tx_size += count;
    *sent = size;
    return MDR_RESULT_OK;
}

static MDRResult mock_poll(void* user, int timeout)
{
    (void)user;
    (void)timeout;
    return MDR_RESULT_OK;
}

static MDRResult mock_get_devices(void* user, MDRDeviceInfo** devices, int* count)
{
    (void)user;
    *devices = NULL;
    *count = 0;
    return MDR_RESULT_OK;
}

static MDRResult mock_free_devices(void* user, MDRDeviceInfo** devices)
{
    (void)user;
    *devices = NULL;
    return MDR_RESULT_OK;
}

static const char* mock_get_last_error(void* user)
{
    (void)user;
    return "mock transport";
}

static void mock_init(MockTransport* transport)
{
    memset(transport, 0, sizeof(*transport));
    transport->connection.user = transport;
    transport->connection.connect = mock_connect;
    transport->connection.disconnect = mock_disconnect;
    transport->connection.recv = mock_receive;
    transport->connection.send = mock_send;
    transport->connection.poll = mock_poll;
    transport->connection.getDevicesList = mock_get_devices;
    transport->connection.freeDevicesList = mock_free_devices;
    transport->connection.getLastError = mock_get_last_error;
}

static void mock_load(MockTransport* transport, const unsigned char* data, size_t size)
{
    check(size <= MOCK_BUFFER_CAPACITY, "mock input fits");
    if (size > MOCK_BUFFER_CAPACITY)
        return;
    memcpy(transport->rx, data, size);
    transport->rx_size = size;
    transport->rx_offset = 0;
}

static void mock_append(MockTransport* transport, const unsigned char* data, size_t size)
{
    check(size <= MOCK_BUFFER_CAPACITY - transport->rx_size, "appended mock input fits");
    if (size > MOCK_BUFFER_CAPACITY - transport->rx_size)
        return;
    memcpy(transport->rx + transport->rx_size, data, size);
    transport->rx_size += size;
}

static int session_open(Session* session)
{
    memset(session, 0, sizeof(*session));
    mock_init(&session->transport);
    check_result(
        mdrHeadphonesCreate(MDR_ABI_VERSION, &session->transport.connection, &session->headphones),
        MDR_RESULT_OK,
        "opaque headphones session opens"
    );
    return session->headphones != NULL;
}

static void session_close(Session* session)
{
    mdrHeadphonesDestroy(session->headphones);
    session->headphones = NULL;
}

static size_t append_escaped(unsigned char byte, unsigned char* output, size_t offset)
{
    if (byte == 0x3c || byte == 0x3d || byte == 0x3e)
    {
        output[offset++] = 0x3d;
        output[offset++] = (unsigned char)(byte - 0x10);
    }
    else
    {
        output[offset++] = byte;
    }
    return offset;
}

static size_t pack_frame(
    unsigned char type,
    unsigned char sequence,
    const unsigned char* payload,
    size_t payload_size,
    unsigned char output[FRAME_BUFFER_CAPACITY]
)
{
    unsigned char unescaped[FRAME_BUFFER_CAPACITY];
    unsigned char checksum = 0;
    size_t unescaped_size = 0;
    size_t output_size = 0;
    size_t index;

    check(payload_size <= FRAME_BUFFER_CAPACITY - 7, "test payload fits frame buffer");
    if (payload_size > FRAME_BUFFER_CAPACITY - 7)
        return 0;

    unescaped[unescaped_size++] = type;
    unescaped[unescaped_size++] = sequence;
    unescaped[unescaped_size++] = (unsigned char)(payload_size >> 24);
    unescaped[unescaped_size++] = (unsigned char)(payload_size >> 16);
    unescaped[unescaped_size++] = (unsigned char)(payload_size >> 8);
    unescaped[unescaped_size++] = (unsigned char)payload_size;
    if (payload_size != 0)
    {
        memcpy(unescaped + unescaped_size, payload, payload_size);
        unescaped_size += payload_size;
    }
    for (index = 0; index < unescaped_size; ++index)
        checksum = (unsigned char)(checksum + unescaped[index]);
    unescaped[unescaped_size++] = checksum;

    output[output_size++] = 0x3e;
    for (index = 0; index < unescaped_size; ++index)
        output_size = append_escaped(unescaped[index], output, output_size);
    output[output_size++] = 0x3c;
    return output_size;
}

static size_t pack_data_frame(
    const unsigned char* payload,
    size_t payload_size,
    unsigned char sequence,
    unsigned char output[FRAME_BUFFER_CAPACITY]
)
{
    return pack_frame(
        MDR_DATA_TYPE_DATA_MDR,
        sequence,
        payload,
        payload_size,
        output
    );
}

typedef struct TxFrame
{
    unsigned char type;
    unsigned char sequence;
    unsigned char payload[FRAME_BUFFER_CAPACITY];
    size_t payload_size;
} TxFrame;

/*
 * Decodes the frame beginning at *offset and advances *offset past it, so a caller can walk
 * everything the library has transmitted - to answer it the way a device would, or to assert
 * on what was asked for. Returns 0 at the end of the stream, or on a frame still in flight.
 */
static int next_tx_frame(MockTransport* transport, size_t* offset, TxFrame* frame)
{
    unsigned char unescaped[FRAME_BUFFER_CAPACITY];
    size_t unescaped_size = 0;
    size_t index = *offset;

    while (index < transport->tx_size && transport->tx[index] != 0x3e)
        ++index;
    if (index == transport->tx_size)
        return 0;
    ++index;
    while (index < transport->tx_size && transport->tx[index] != 0x3c)
    {
        unsigned char byte = transport->tx[index++];
        if (byte == 0x3d && index < transport->tx_size)
            byte = (unsigned char)(transport->tx[index++] + 0x10);
        if (unescaped_size < FRAME_BUFFER_CAPACITY)
            unescaped[unescaped_size++] = byte;
    }
    if (index == transport->tx_size)
        return 0;
    ++index;
    /* type, sequence, four size bytes, payload, checksum */
    if (unescaped_size < 7)
        return 0;
    *offset = index;
    frame->type = unescaped[0];
    frame->sequence = unescaped[1];
    frame->payload_size = unescaped_size - 7;
    memcpy(frame->payload, unescaped + 6, frame->payload_size);
    return 1;
}

/* Sequence number of the last non-ACK frame the library actually put on the wire. */
static int last_tx_sequence(MockTransport* transport, unsigned char* sequence)
{
    TxFrame frame;
    size_t offset = 0;
    int found = 0;

    while (next_tx_frame(transport, &offset, &frame))
    {
        /* Our own acknowledgements are not something the device would acknowledge back. */
        if (frame.type != MDR_DATA_TYPE_ACK)
        {
            *sequence = frame.sequence;
            found = 1;
        }
    }
    return found;
}

/*
 * Devices acknowledge a DATA frame by echoing the inverted sequence number of the frame
 * they received, and libmdr only advances its transmit sequence once that acknowledgement
 * arrives. Deriving the ACK from what was actually transmitted - rather than from a fixed
 * constant - is what a device does, and keeps repeated exchanges honest about the toggle.
 */
static size_t pack_ack(MockTransport* transport, unsigned char output[FRAME_BUFFER_CAPACITY])
{
    unsigned char sequence = 0;

    check(
        last_tx_sequence(transport, &sequence),
        "an outbound frame is available to acknowledge"
    );
    return pack_frame(
        MDR_DATA_TYPE_ACK,
        (unsigned char)(1 - sequence),
        NULL,
        0,
        output
    );
}

static int poll_event(MDRHeadphones* headphones, MDREvent* event, const char* message)
{
    MDRResult result = mdrHeadphonesPoll(headphones, event);
    check_result(result, MDR_RESULT_OK, message);
    return result == MDR_RESULT_OK;
}

static char* get_text(MDRHeadphones* headphones, MDRText text)
{
    uint32_t size = 0;
    char* buffer;

    if (mdrHeadphonesGetText(headphones, text, 0, NULL, &size) != MDR_RESULT_OK)
        return NULL;
    buffer = (char*)malloc(size);
    if (buffer == NULL)
        return NULL;
    if (mdrHeadphonesGetText(headphones, text, 0, buffer, &size) != MDR_RESULT_OK)
    {
        free(buffer);
        return NULL;
    }
    return buffer;
}

static const unsigned char k_v2_protocol_info[] = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01
};

/* As above, but with the table 2 support byte set to ENABLE rather than DISABLE. */
static const unsigned char k_v2_protocol_info_both_tables[] = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00
};

static void select_v2(Session* session, const char* message)
{
    unsigned char frame[FRAME_BUFFER_CAPACITY];
    MDREvent event;
    size_t frame_size = pack_data_frame(
        k_v2_protocol_info,
        sizeof(k_v2_protocol_info),
        0,
        frame
    );
    mock_load(&session->transport, frame, frame_size);
    poll_event(session->headphones, &event, message);
}

static void test_abi_version_handshake(void)
{
    MockTransport transport;
    MDRHeadphones* headphones = (MDRHeadphones*)0x1;

    mock_init(&transport);
    check_result(
        mdrHeadphonesCreate(MDR_ABI_VERSION + 1u, &transport.connection, &headphones),
        MDR_RESULT_ERROR_ABI_MISMATCH,
        "a newer header is refused by an older library"
    );
    check(headphones == NULL, "a refused handshake leaves no instance behind");

    check_result(
        mdrHeadphonesCreate(0u, &transport.connection, &headphones),
        MDR_RESULT_ERROR_ABI_MISMATCH,
        "an unversioned caller is refused"
    );

    check_result(
        mdrHeadphonesCreate(MDR_ABI_VERSION, &transport.connection, &headphones),
        MDR_RESULT_OK,
        "the matching version is accepted"
    );
    check(headphones != NULL, "an accepted handshake yields an instance");
    mdrHeadphonesDestroy(headphones);
}

static void test_struct_and_buffer_contracts(void)
{
    Session session;
    uint32_t text_size;
    char short_text[1];
    uint32_t short_text_size;
    char* text;
    uint32_t copied_text_size;

    if (!session_open(&session))
        return;

    text_size = 1;
    check_result(
        mdrHeadphonesGetText(
            session.headphones, MDR_TEXT_LAST_ERROR, 0, NULL, &text_size
        ),
        MDR_RESULT_ERROR_INVALID_ARGUMENT,
        "text size query requires a zero input size"
    );
    text_size = 0;
    check_result(
        mdrHeadphonesGetText(
            session.headphones, MDR_TEXT_LAST_ERROR, 0, NULL, &text_size
        ),
        MDR_RESULT_OK,
        "text size query succeeds"
    );
    check(text_size > 1, "text size includes a NUL terminator");

    short_text_size = (uint32_t)sizeof(short_text);
    check_result(
        mdrHeadphonesGetText(
            session.headphones,
            MDR_TEXT_LAST_ERROR,
            0,
            short_text,
            &short_text_size
        ),
        MDR_RESULT_ERROR_BUFFER_TOO_SMALL,
        "text copy reports a short caller buffer"
    );
    check(short_text_size == text_size, "text copy returns the required size");

    text = (char*)malloc(text_size);
    copied_text_size = text_size;
    check(text != NULL, "text test allocation succeeds");
    if (text != NULL)
    {
        check_result(
            mdrHeadphonesGetText(
                session.headphones,
                MDR_TEXT_LAST_ERROR,
                0,
                text,
                &copied_text_size
            ),
            MDR_RESULT_OK,
            "text copy succeeds on the second call"
        );
        check(
            copied_text_size == text_size && text[text_size - 1] == '\0',
            "text copy is NUL terminated"
        );
        free(text);
    }

    session_close(&session);
}

static void test_one_operation_at_a_time(void)
{
    Session session;

    if (!session_open(&session))
        return;
    check_result(
        mdrHeadphonesRequestInit(session.headphones),
        MDR_RESULT_OK,
        "initialization starts"
    );
    check_result(
        mdrHeadphonesRequestFetch(session.headphones),
        MDR_RESULT_INPROGRESS,
        "a second operation is rejected while initialization is active"
    );
    check(
        mdrHeadphonesIsReady(session.headphones) == MDR_FALSE,
        "status reports the active operation as busy"
    );
    session_close(&session);
}

static void test_committed_state_staging(void)
{
    Session session;
    MDRPlayback staged;
    MDRPlayback current;

    if (!session_open(&session))
        return;
    memset(&staged, 0, sizeof(staged));
    staged.status = MDR_PLAYBACK_UNKNOWN;
    staged.volume = 12;
    check_result(
        mdrHeadphonesSetPlayback(session.headphones, &staged),
        MDR_RESULT_OK,
        "playback volume stages"
    );

    memset(&current, 0, sizeof(current));
    check_result(
        mdrHeadphonesGetPlayback(session.headphones, &current),
        MDR_RESULT_OK,
        "committed playback is readable"
    );
    check(current.volume == 0, "staging does not alter current playback");

    check(
        mdrHeadphonesIsDirty(session.headphones) == MDR_TRUE,
        "staging marks the session dirty"
    );
    session_close(&session);
}

static void test_playback_actions(void)
{
    Session session;
    const MDRPlaybackAction actions[] = {
        MDR_PLAYBACK_PLAY,
        MDR_PLAYBACK_PAUSE,
        MDR_PLAYBACK_NEXT,
        MDR_PLAYBACK_PREVIOUS
    };
    size_t index;
    MDRPlaybackCommand command;
    unsigned char ack[FRAME_BUFFER_CAPACITY];
    size_t ack_size;
    MDRPlayback unsupported_status;
    MDREvent event;

    if (!session_open(&session))
        return;
    select_v2(&session, "V2 protocol is selected for playback actions");

    for (index = 0; index < sizeof(actions) / sizeof(actions[0]); ++index)
    {
        memset(&command, 0, sizeof(command));
        command.action = actions[index];
        check_result(
            mdrHeadphonesPlayback(session.headphones, &command),
            MDR_RESULT_OK,
            "supported playback action stages"
        );

        check(
            mdrHeadphonesIsDirty(session.headphones) == MDR_TRUE,
            "playback action is pending"
        );

        check_result(
            mdrHeadphonesRequestCommit(session.headphones),
            MDR_RESULT_OK,
            "playback action apply starts"
        );
        poll_event(session.headphones, &event, "playback action request flushes");
        ack_size = pack_ack(&session.transport, ack);
        mock_load(&session.transport, ack, ack_size);
        poll_event(session.headphones, &event, "playback action ACK polls");
        poll_event(session.headphones, &event, "playback action completion polls");
        check(event == MDR_EVENT_APPLY_COMPLETE, "playback action apply completes");

        check(
            mdrHeadphonesIsReady(session.headphones) == MDR_TRUE
                && mdrHeadphonesIsDirty(session.headphones) == MDR_FALSE,
            "playback action is consumed as a one-shot"
        );
    }

    memset(&command, 0, sizeof(command));
    command.action = (MDRPlaybackAction)0xff;
    check_result(
        mdrHeadphonesPlayback(session.headphones, &command),
        MDR_RESULT_ERROR_INVALID_ARGUMENT,
        "unknown playback action is rejected"
    );

    memset(&unsupported_status, 0, sizeof(unsupported_status));
    unsupported_status.status = MDR_PLAYBACK_PLAYING;
    unsupported_status.volume = 10;
    check_result(
        mdrHeadphonesSetPlayback(session.headphones, &unsupported_status),
        MDR_RESULT_ERROR_NOT_SUPPORTED,
        "playback status is not misrepresented as a staged volume change"
    );
    session_close(&session);
}

/*
 * Devices may push data before we have negotiated a protocol family. There is no table
 * to decode it against yet, so it is dropped - tearing the session down instead would
 * lose the connection over a frame we simply arrived too early for.
 */
static void test_frames_before_protocol_info_are_not_fatal(void)
{
    Session session;
    const unsigned char notification[] = {0xa9, 0x01};
    unsigned char frame[FRAME_BUFFER_CAPACITY];
    size_t frame_size;
    MDRModel identity;
    MDREvent event;

    if (!session_open(&session))
        return;

    frame_size = pack_data_frame(
        notification,
        sizeof(notification),
        0,
        frame
    );
    mock_load(&session.transport, frame, frame_size);
    poll_event(session.headphones, &event, "pre-handshake frame polls without failing");
    check(
        event == MDR_EVENT_UNHANDLED,
        "pre-handshake frame is reported as unhandled"
    );

    /* The handshake still completes afterwards. */
    select_v2(&session, "protocol info is still accepted after a dropped frame");
    memset(&identity, 0, sizeof(identity));
    check_result(
        mdrHeadphonesGetModel(session.headphones, &identity),
        MDR_RESULT_OK,
        "identity is readable after a dropped pre-handshake frame"
    );
    check(identity.protocol_version == 2, "V2 is selected after a dropped frame");
    session_close(&session);
}

/*
 * Devices interleave unsolicited notifications and late responses into the exchange.
 * None of that may influence the sequence number we transmit with: a frame repeating
 * the sequence number of an already acknowledged one is dropped by the device as a
 * duplicate, and the request is then answered by silence.
 */
static void test_transmit_sequence_ignores_inbound_frames(void)
{
    Session session;
    const unsigned char notification[] = {0xfe};
    unsigned char frame[FRAME_BUFFER_CAPACITY];
    unsigned char ack[FRAME_BUFFER_CAPACITY];
    size_t frame_size;
    size_t ack_size;
    unsigned char first_sequence = 0;
    unsigned char second_sequence = 0;
    MDRPlaybackCommand command;
    MDREvent event;

    if (!session_open(&session))
        return;
    select_v2(&session, "V2 protocol is selected for sequence tracking");

    memset(&command, 0, sizeof(command));
    command.action = MDR_PLAYBACK_PLAY;
    check_result(
        mdrHeadphonesPlayback(session.headphones, &command),
        MDR_RESULT_OK,
        "first action stages"
    );
    check_result(
        mdrHeadphonesRequestCommit(session.headphones),
        MDR_RESULT_OK,
        "first apply starts"
    );
    poll_event(session.headphones, &event, "first request flushes");
    check(
        last_tx_sequence(&session.transport, &first_sequence),
        "first request was transmitted"
    );

    ack_size = pack_ack(&session.transport, ack);
    mock_load(&session.transport, ack, ack_size);
    poll_event(session.headphones, &event, "first apply ACK polls");
    poll_event(session.headphones, &event, "first apply completion polls");
    check(event == MDR_EVENT_APPLY_COMPLETE, "first apply completes");

    /*
     * A frame arriving after the acknowledgement - a lagging response, or a notification
     * the device pushed on its own - carrying the sequence number the acknowledged frame
     * used. Adopting it would make the next request look like a retransmission.
     */
    frame_size = pack_data_frame(
        notification,
        sizeof(notification),
        first_sequence,
        frame
    );
    mock_load(&session.transport, frame, frame_size);
    poll_event(session.headphones, &event, "late inbound frame polls");
    check(event == MDR_EVENT_UNHANDLED, "late inbound frame is reported, not fatal");

    memset(&command, 0, sizeof(command));
    command.action = MDR_PLAYBACK_NEXT;
    check_result(
        mdrHeadphonesPlayback(session.headphones, &command),
        MDR_RESULT_OK,
        "second action stages"
    );
    check_result(
        mdrHeadphonesRequestCommit(session.headphones),
        MDR_RESULT_OK,
        "second apply starts"
    );
    poll_event(session.headphones, &event, "second request flushes");
    check(
        last_tx_sequence(&session.transport, &second_sequence),
        "second request was transmitted"
    );
    check(
        second_sequence != first_sequence,
        "the next request does not repeat the sequence number of an acknowledged frame"
    );
    session_close(&session);
}

/*
 * A device that understands the framing and acknowledges everything, but answers only the
 * two requests initialization cannot proceed without. Every other request is accepted and
 * dropped, which is what a real device does with a command it does not implement - so what
 * matters is that libmdr never asks for anything the advertised function list rules out.
 */
enum { MDR_DATA_TYPE_DATA_MDR_NO2 = 14, REQUEST_LOG_CAPACITY = 128 };

typedef struct RequestLog
{
    unsigned char table;
    unsigned char command;
    unsigned char inquired;
    int has_inquired;
} RequestLog;

typedef struct Device
{
    MockTransport* transport;
    size_t tx_cursor;
    unsigned char sequence;
    const unsigned char* table1;
    size_t table1_size;
    const unsigned char* table2;
    size_t table2_size;
    RequestLog log[REQUEST_LOG_CAPACITY];
    size_t log_size;
} Device;

static void device_send(Device* device, unsigned char type, const unsigned char* payload, size_t payload_size)
{
    unsigned char frame[FRAME_BUFFER_CAPACITY];
    const size_t size = pack_frame(type, device->sequence, payload, payload_size, frame);

    device->sequence = (unsigned char)(1 - device->sequence);
    mock_append(device->transport, frame, size);
}

/* Acknowledge and, where the handshake requires it, answer everything transmitted so far. */
static void device_pump(Device* device)
{
    TxFrame frame;

    while (next_tx_frame(device->transport, &device->tx_cursor, &frame))
    {
        unsigned char ack[FRAME_BUFFER_CAPACITY];
        size_t ack_size;
        unsigned char table;

        if (frame.type == MDR_DATA_TYPE_ACK || frame.payload_size == 0)
            continue;
        table = (unsigned char)(frame.type == MDR_DATA_TYPE_DATA_MDR_NO2 ? 2 : 1);

        if (device->log_size < REQUEST_LOG_CAPACITY)
        {
            RequestLog* entry = &device->log[device->log_size++];
            entry->table = table;
            entry->command = frame.payload[0];
            entry->has_inquired = frame.payload_size > 1;
            entry->inquired = entry->has_inquired ? frame.payload[1] : 0;
        }

        ack_size = pack_frame(MDR_DATA_TYPE_ACK, (unsigned char)(1 - frame.sequence), NULL, 0, ack);
        mock_append(device->transport, ack, ack_size);

        if (table == 1 && frame.payload[0] == 0x00) /* CONNECT_GET_PROTOCOL_INFO */
        {
            device_send(
                device,
                MDR_DATA_TYPE_DATA_MDR,
                k_v2_protocol_info_both_tables,
                sizeof(k_v2_protocol_info_both_tables)
            );
        }
        else if (frame.payload[0] == 0x06) /* CONNECT_GET_SUPPORT_FUNCTION */
        {
            if (table == 1)
                device_send(device, MDR_DATA_TYPE_DATA_MDR, device->table1, device->table1_size);
            else
                device_send(device, MDR_DATA_TYPE_DATA_MDR_NO2, device->table2, device->table2_size);
        }
    }
}

static int device_requested(const Device* device, unsigned char table, unsigned char command, int inquired)
{
    size_t index;

    for (index = 0; index < device->log_size; ++index)
    {
        const RequestLog* entry = &device->log[index];
        if (entry->table != table || entry->command != command)
            continue;
        if (inquired < 0 || (entry->has_inquired && entry->inquired == (unsigned char)inquired))
            return 1;
    }
    return 0;
}

/* Runs initialization to completion against `device`, or reports why it could not. */
static void device_run_init(Session* session, Device* device)
{
    int iteration;

    check_result(
        mdrHeadphonesRequestInit(session->headphones),
        MDR_RESULT_OK,
        "initialization starts"
    );
    for (iteration = 0; iteration < 4096; ++iteration)
    {
        MDREvent event = MDR_EVENT_NONE;
        if (mdrHeadphonesPoll(session->headphones, &event) != MDR_RESULT_OK)
        {
            check(0, "initialization polls without failing");
            return;
        }
        device_pump(device);
        if (event == MDR_EVENT_INITIALIZE_COMPLETE)
            return;
    }
    check(0, "initialization completes");
}

/*
 * Requests for functions the device does not advertise are worse than useless: the device
 * acknowledges and ignores them, so the state never arrives, and a device that ignores
 * unknown commands outright would stall initialization until the retry budget runs out.
 */
static void test_init_skips_unadvertised_functions(void)
{
    /* POWER_OFF and LR_BATTERY_LEVEL_WITH_THRESHOLD only. */
    static const unsigned char table1[] = {0x07, 0x00, 0x02, 0x23, 0xff, 0x29, 0xff};
    /* SAFE_LISTENING_TWS_1 only - table 2 is present, but carries no voice guidance. */
    static const unsigned char table2[] = {0x07, 0x00, 0x01, 0x51, 0xff};

    Session session;
    Device device;

    if (!session_open(&session))
        return;
    memset(&device, 0, sizeof(device));
    device.transport = &session.transport;
    device.table1 = table1;
    device.table1_size = sizeof(table1);
    device.table2 = table2;
    device.table2_size = sizeof(table2);

    device_run_init(&session, &device);

    check(!device_requested(&device, 1, 0x52, -1), "no EQEBB_GET_STATUS without an equalizer");
    check(!device_requested(&device, 1, 0x56, -1), "no EQEBB_GET_PARAM without an equalizer");
    check(!device_requested(&device, 1, 0xa6, -1), "no PLAY_GET_PARAM without a playback controller");
    check(!device_requested(&device, 1, 0xa2, -1), "no PLAY_GET_STATUS without a playback controller");
    check(!device_requested(&device, 1, 0xf6, 0x01), "no PLAYBACK_CONTROL_BY_WEARING without wearing control");
    check(!device_requested(&device, 1, 0xe6, 0x09), "no BGM_MODE without a listening option");
    check(!device_requested(&device, 1, 0xe6, 0x04), "no UPMIX_CINEMA without a listening option");
    check(!device_requested(&device, 2, 0x46, -1), "no VOICE_GUIDANCE_GET_PARAM on a table 2 device without it");
    session_close(&session);
}

/*
 * The mirror image, plus the case that motivated splitting the listening-mode gate:
 * LISTENING_OPTION with only the background-music half implemented.
 */
static void test_init_requests_advertised_functions(void)
{
    static const unsigned char table1[] = {
        0x07, 0x00, 0x06,
        0x50, 0xff, /* PRESET_EQ */
        0xa1, 0xff, /* PLAYBACK_CONTROLLER_WITH_CALL_VOLUME_ADJUSTMENT */
        0xf1, 0xff, /* PLAYBACK_CONTROL_BY_WEARING_REMOVING_HEADPHONE_ON_OFF */
        0xe6, 0xff, /* LISTENING_OPTION */
        0xeb, 0xff, /* BGM_MODE_SMALL_MIDDLE_LARGE_AND_ERRORCODE - but no UPMIX_CINEMA */
        0x23, 0xff  /* POWER_OFF */
    };
    static const unsigned char table2[] = {
        0x07, 0x00, 0x01,
        0x42, 0xff /* VOICE_GUIDANCE_..._SUPPORT_LANGUAGE_SWITCH_AND_VOLUME_ADJUSTMENT */
    };

    Session session;
    Device device;

    if (!session_open(&session))
        return;
    memset(&device, 0, sizeof(device));
    device.transport = &session.transport;
    device.table1 = table1;
    device.table1_size = sizeof(table1);
    device.table2 = table2;
    device.table2_size = sizeof(table2);

    device_run_init(&session, &device);

    check(device_requested(&device, 1, 0x52, -1), "EQEBB_GET_STATUS for an advertised equalizer");
    check(device_requested(&device, 1, 0xa6, -1), "PLAY_GET_PARAM for an advertised playback controller");
    check(device_requested(&device, 1, 0xa2, -1), "PLAY_GET_STATUS for an advertised playback controller");
    check(device_requested(&device, 1, 0xf6, 0x01), "PLAYBACK_CONTROL_BY_WEARING for advertised wearing control");
    check(device_requested(&device, 1, 0xe6, 0x09), "BGM_MODE for an advertised background-music mode");
    check(device_requested(&device, 2, 0x46, -1), "VOICE_GUIDANCE_GET_PARAM for advertised voice guidance");
    check(
        !device_requested(&device, 1, 0xe6, 0x04),
        "no UPMIX_CINEMA when only the background-music half of LISTENING_OPTION is advertised"
    );
    session_close(&session);
}

static void test_poll_events(void)
{
    Session session;
    const unsigned char unknown_payload[] = {0xfe};
    unsigned char protocol_frame[FRAME_BUFFER_CAPACITY];
    unsigned char unknown_frame[FRAME_BUFFER_CAPACITY];
    size_t protocol_size;
    size_t unknown_size;
    MDREvent first;
    MDREvent second;

    if (!session_open(&session))
        return;
    protocol_size = pack_data_frame(
        k_v2_protocol_info,
        sizeof(k_v2_protocol_info),
        0,
        protocol_frame
    );
    unknown_size = pack_data_frame(
        unknown_payload,
        sizeof(unknown_payload),
        1,
        unknown_frame
    );
    mock_load(&session.transport, protocol_frame, protocol_size);
    mock_append(&session.transport, unknown_frame, unknown_size);

    poll_event(session.headphones, &first, "first frame polls");
    poll_event(session.headphones, &second, "second frame polls");
    check(
        first == MDR_EVENT_IDENTITY_CHANGED,
        "protocol state change is reported by its poll"
    );
    check(
        second == MDR_EVENT_UNHANDLED,
        "unhandled frame is reported by its poll"
    );
    session_close(&session);
}

static void test_v2_bootstrap(void)
{
    Session session;
    unsigned char frame[FRAME_BUFFER_CAPACITY];
    size_t frame_size;
    MDRModel identity;
    MDREvent event;

    if (!session_open(&session))
        return;
    check_result(
        mdrHeadphonesRequestInit(session.headphones),
        MDR_RESULT_OK,
        "automatic initialization starts"
    );
    poll_event(session.headphones, &event, "protocol-info request flushes");
    frame_size = pack_ack(&session.transport, frame);
    mock_load(&session.transport, frame, frame_size);
    poll_event(session.headphones, &event, "protocol-info request ACK polls");

    frame_size = pack_data_frame(
        k_v2_protocol_info,
        sizeof(k_v2_protocol_info),
        0,
        frame
    );
    mock_load(&session.transport, frame, frame_size);
    poll_event(session.headphones, &event, "eight-byte V2 protocol-info polls");

    memset(&identity, 0, sizeof(identity));
    check_result(
        mdrHeadphonesGetModel(session.headphones, &identity),
        MDR_RESULT_OK,
        "V2 identity is readable"
    );
    check(identity.protocol_version == 2, "eight-byte payload selects MDR V2");
    check(
        event == MDR_EVENT_IDENTITY_CHANGED,
        "V2 protocol selection reports identity change"
    );

    check(
        mdrHeadphonesIsInitialized(session.headphones) == MDR_FALSE
            && mdrHeadphonesIsReady(session.headphones) == MDR_FALSE,
        "V2 bootstrap automatically continues into backend initialization"
    );
    session_close(&session);
}

static void test_newer_staging_survives_apply(void)
{
    Session session;
    MDRPlayback first;
    MDRPlayback newer;
    MDRPlayback current;
    unsigned char ack[FRAME_BUFFER_CAPACITY];
    size_t ack_size;
    MDREvent ack_event;
    MDREvent completion;

    if (!session_open(&session))
        return;
    select_v2(&session, "V2 protocol is selected for apply");

    memset(&first, 0, sizeof(first));
    first.status = MDR_PLAYBACK_UNKNOWN;
    first.volume = 10;
    check_result(
        mdrHeadphonesSetPlayback(session.headphones, &first),
        MDR_RESULT_OK,
        "first playback value stages"
    );
    check_result(
        mdrHeadphonesRequestCommit(session.headphones),
        MDR_RESULT_OK,
        "apply starts"
    );

    memset(&newer, 0, sizeof(newer));
    newer.status = MDR_PLAYBACK_UNKNOWN;
    newer.volume = 20;
    check_result(
        mdrHeadphonesSetPlayback(session.headphones, &newer),
        MDR_RESULT_OK,
        "newer playback value stages during apply"
    );

    poll_event(session.headphones, &ack_event, "apply request flushes");
    ack_size = pack_ack(&session.transport, ack);
    mock_load(&session.transport, ack, ack_size);
    poll_event(session.headphones, &ack_event, "apply ACK polls");
    poll_event(session.headphones, &completion, "apply completion polls");
    check(
        ack_event == MDR_EVENT_UNHANDLED
            && completion == MDR_EVENT_APPLY_COMPLETE,
        "ACK and completion are reported in poll order"
    );

    memset(&current, 0, sizeof(current));
    check_result(
        mdrHeadphonesGetPlayback(session.headphones, &current),
        MDR_RESULT_OK,
        "applied playback is readable"
    );
    check(current.volume == first.volume, "apply commits its original snapshot");

    check(
        mdrHeadphonesIsReady(session.headphones) == MDR_TRUE
            && mdrHeadphonesIsDirty(session.headphones) == MDR_TRUE,
        "newer value remains pending after apply completes"
    );
    session_close(&session);
}

int main(void)
{
    test_abi_version_handshake();
    test_struct_and_buffer_contracts();
    test_one_operation_at_a_time();
    test_committed_state_staging();
    test_playback_actions();
    test_poll_events();
    test_frames_before_protocol_info_are_not_fatal();
    test_init_skips_unadvertised_functions();
    test_init_requests_advertised_functions();
    test_transmit_sequence_ignores_inbound_frames();
    test_v2_bootstrap();
    test_newer_staging_survives_apply();

    if (g_failures != 0)
        fprintf(stderr, "%d test assertion(s) failed\n", g_failures);
    return g_failures != 0 ? 1 : 0;
}
