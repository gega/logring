# logring

Single-header logging library for embedded systems.

logring stores formatted log messages in a fixed-size circular buffer and provides an API to retrieve complete log lines one by one. When the buffer becomes full, the oldest messages are discarded automatically so that the most recent messages are always preserved.

The intended use case is exporting logs over a communication channel such as:

* BLE characteristic
* UART command shell
* USB CDC console
* TCP socket
* SWO or RTT

The library is designed for constrained embedded targets and has deterministic memory usage with no dynamic allocation.

---

## Features

* Header only library
* `printf`-style logging API
* Fixed-size ring buffer
* No heap allocation
* Thread-safe through user-provided lock macros
* Callback notification after each log message
* Preserves only complete messages (never partial fragments)
* Automatically discards oldest messages when space is needed

---

## Dependencies

LogRing depends on three external libraries:

* [LwRB](https://github.com/MaJerle/lwrb.git)
  Lightweight ring buffer implementation.

* [nanoprintf](https://github.com/charlesnicholson/nanoprintf.git)
  Small `printf` implementation used to format log messages.

* [mmfl](https://github.com/gega/mmfl.git)
  Minimalist message framing library

Make sure these headers are available in your include path, these are provided as submodules in this repo as reference.

---

## Configuration

All configuration is done with preprocessor macros before including `logring.h`.

### `LGR_MAX_LOG_LEN`

Maximum length of a single formatted log message, excluding the terminating `'\0'`.

```c
#define LGR_MAX_LOG_LEN 240
```

Messages longer than this are truncated before being stored.

---

### `LGR_MSG_SIZE_TYPE`

Integer type used to store message lengths.

```c
#define LGR_MSG_SIZE_TYPE unsigned char
```

This type must be large enough to represent `LGR_MAX_LOG_LEN`.

Typical choices:

| Type            | Maximum Value |
| --------------- | ------------- |
| `unsigned char` | 255           |
| `uint16_t`      | 65535         |
| `uint32_t`      | 4294967295    |

---

### `LGR_MUTEX_LOCK()` and `LGR_MUTEX_UNLOCK()`

Function-style macros used to protect internal data structures.

```c
#define LGR_MUTEX_LOCK()   my_mutex_lock()
#define LGR_MUTEX_UNLOCK() my_mutex_unlock()
```

If omitted, they default to empty macros and no locking is performed.

Use these when:

* Logging from multiple tasks
* Logging from interrupt and thread context
* Retrieving logs while new messages are being written

---

### `LGR_IMPLEMENTATION`

Must be defined in exactly one `.c` file before including `logring.h`.

```c
#define LGR_IMPLEMENTATION
#include "logring.h"
```

All other translation units should include the header normally.

---

## Memory Requirements

The total RAM usage is approximately:

```text
user ring buffer
+ sizeof(struct lgr_s)
+ small framing overhead
```

More precisely:

* `wrb[]`: `LGR_MAX_LOG_LEN + 1`
* `rdb[]`: `LGR_MAX_LOG_LEN + MMFL_HDR_MAX`
* User-provided ring buffer
* `struct lgr_s` bookkeeping fields

For the default configuration (`LGR_MAX_LOG_LEN = 240`), the internal buffers consume roughly 485 bytes in addition to the user ring buffer.

---

## Data Structure

```c
struct lgr_s;
```

Opaque state structure that must remain allocated for the lifetime of the logger.

---

## Callback Type

```c
typedef void (*notify_cb_t)(struct lgr_s *log, void *ud);
```

Called after each successful `lgr_printf()` invocation.

Parameters:

* `log` – pointer to the logger instance
* `ud` – user-defined pointer supplied to `lgr_init()`

Typical uses:

* Signal a task or semaphore
* Wake a BLE notification handler
* Trigger UART transmission

The callback is invoked after the internal lock has been released.

---

## API Reference

### `int lgr_init(...)`

```c
int lgr_init(
    struct lgr_s *log,
    uint8_t *buf,
    int size,
    notify_cb_t ncb,
    void *ud
);
```

Initializes a logger instance.

Parameters:

* `log` – uninitialized logger structure
* `buf` – user-provided storage for the ring buffer
* `size` – size of `buf` in bytes
* `ncb` – optional callback invoked after each new message
* `ud` – user-defined pointer passed to the callback

Returns:

* `0` on success
* `-1` if the buffer is too small

The buffer must be large enough to hold at least one maximum-sized message plus framing overhead.

---

### `void lgr_printf(...)`

```c
void lgr_printf(struct lgr_s *log, char const *fmt, ...);
```

Formats a message and stores it in the ring buffer.

Behavior:

* Uses `nanoprintf` formatting
* Truncates messages longer than `LGR_MAX_LOG_LEN`
* Automatically removes oldest complete messages when the buffer is full
* Calls the notification callback after storing the message

---

### `int lgr_get_line(...)`

```c
int lgr_get_line(
    struct lgr_s *log,
    char *buf,
    lgr_msg_size_t len
);
```

Retrieves the oldest available message.

Parameters:

* `log` – logger instance
* `buf` – destination buffer
* `len` – size of destination buffer

Returns:

* Message length in bytes
* `0` if no message is available

Behavior:

* Copies up to `len - 1` bytes
* Always null-terminates when `buf != NULL` and `len > 0`
* If the destination buffer is too small, the message is truncated
* The remainder of the truncated message is discarded
* The next call returns the next stored message

---

## Basic Usage

### Implementation File

```c
#define LGR_IMPLEMENTATION
#include "logring.h"
```

### Application Code

```c
#include "logring.h"

static struct lgr_s logger;
static uint8_t log_storage[2048];

static void log_notify(struct lgr_s *log, void *ud)
{
    /* Signal a task, set an event flag, or trigger BLE notification */
}

void app_init(void)
{
    lgr_init(
        &logger,
        log_storage,
        sizeof(log_storage),
        log_notify,
        NULL
    );
}
```

### Writing Logs

```c
for (int i = 0; i < 100; i++) {
    lgr_printf(&logger,
               "msg %d: 0x%02x %d\n",
               i,
               (i % 0xff),
               __LINE__);
}
```

### Reading Logs

```c
char line[128];

while (lgr_get_line(&logger, line, sizeof(line)) > 0) {
    send_over_ble(line);
}
```

---

## Buffer Overflow Behavior

When a new message does not fit:

1. The oldest complete messages are removed.
2. Enough space is freed.
3. The new message is written.

This guarantees:

* Only complete messages are stored.
* The newest messages are always retained.
* No partially overwritten records appear.

---

## Thread Safety

The library itself does not implement synchronization.

Use `LGR_MUTEX_LOCK()` and `LGR_MUTEX_UNLOCK()` to protect:

* Concurrent `lgr_printf()` calls
* Concurrent `lgr_get_line()` calls
* Simultaneous producers and consumers

---

## Notes

* `lgr_printf()` ignores empty formatting results.
* Message lengths are stored using `LGR_MSG_SIZE_TYPE`.
* Messages may include newline characters; they are treated as ordinary text.
* `lgr_get_line()` removes messages from the buffer as they are read.
* No dynamic memory allocation is used.



