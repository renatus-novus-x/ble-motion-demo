#ifndef BLE_MOTION_PLATFORM_H
#define BLE_MOTION_PLATFORM_H

#include <stddef.h>
#include <string.h>

#if defined(__human68k__)

#include <x68k/iocs.h>

typedef struct platform_serial { int open; } platform_serial_t;

static int platform_serial_open(platform_serial_t *serial, const char *name, int baud)
{
    static const int baud_table[] = {
        75, 150, 300, 600, 1200, 2400, 4800, 9600, 19200, 38400
    };
    int baud_index;
    if (serial == NULL || name == NULL) return -1;
    serial->open = 0;
    baud_index = 0;
    while (baud_index < 10 && baud_table[baud_index] != baud) ++baud_index;
    if (baud_index == 10) return -1;
    _iocs_set232c((short)(0x4c00 | baud_index));
    while (_iocs_isns232c() != 0) (void)_iocs_inp232c();
    serial->open = 1;
    return 0;
}

static void platform_serial_close(platform_serial_t *serial)
{
    if (serial != NULL) serial->open = 0;
}

static int platform_serial_read(
    platform_serial_t *serial,
    unsigned char *buffer,
    size_t capacity)
{
    size_t count;
    if (serial == NULL || !serial->open) return -1;
    count = 0;
    while (count < capacity && _iocs_isns232c() != 0)
        buffer[count++] = (unsigned char)_iocs_inp232c();
    return (int)count;
}

static int platform_frame_initialize(double *next_frame, double frame_seconds)
{
    (void)next_frame;
    (void)frame_seconds;
    return 0;
}

static int platform_wait_next_frame(double *next_frame, double frame_seconds)
{
    (void)next_frame;
    (void)frame_seconds;
    return 0;
}

#elif defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

typedef struct platform_serial { HANDLE handle; } platform_serial_t;

static int platform_serial_open(platform_serial_t *serial, const char *name, int baud)
{
    char path[64]; DCB state; COMMTIMEOUTS timeouts; size_t length;
    if (serial == NULL || name == NULL) return -1;
    serial->handle = INVALID_HANDLE_VALUE;
    length = strlen(name);
    if (strncmp(name, "\\\\.\\", 4) == 0) {
        if (length >= sizeof(path)) return -1;
        memcpy(path, name, length + 1);
    } else {
        if (length + 4 >= sizeof(path)) return -1;
        memcpy(path, "\\\\.\\", 4);
        memcpy(path + 4, name, length + 1);
    }
    serial->handle = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (serial->handle == INVALID_HANDLE_VALUE) return -1;
    memset(&state, 0, sizeof(state)); state.DCBlength = sizeof(state);
    if (!GetCommState(serial->handle, &state)) goto fail;
    state.BaudRate = (DWORD)baud; state.ByteSize = 8;
    state.Parity = NOPARITY; state.StopBits = ONESTOPBIT; state.fBinary = TRUE;
    state.fParity = FALSE; state.fOutxCtsFlow = FALSE; state.fOutxDsrFlow = FALSE;
    state.fDtrControl = DTR_CONTROL_DISABLE; state.fOutX = FALSE; state.fInX = FALSE;
    state.fRtsControl = RTS_CONTROL_DISABLE;
    if (!SetCommState(serial->handle, &state)) goto fail;
    memset(&timeouts, 0, sizeof(timeouts)); timeouts.ReadIntervalTimeout = MAXDWORD;
    if (!SetCommTimeouts(serial->handle, &timeouts)) goto fail;
    PurgeComm(serial->handle, PURGE_RXCLEAR | PURGE_TXCLEAR); return 0;
fail:
    CloseHandle(serial->handle); serial->handle = INVALID_HANDLE_VALUE; return -1;
}

static void platform_serial_close(platform_serial_t *serial)
{
    if (serial != NULL && serial->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(serial->handle); serial->handle = INVALID_HANDLE_VALUE;
    }
}

static int platform_serial_read(platform_serial_t *serial, unsigned char *buffer, size_t capacity)
{
    DWORD count = 0;
    if (serial == NULL || serial->handle == INVALID_HANDLE_VALUE) return -1;
    if (!ReadFile(serial->handle, buffer, (DWORD)capacity, &count, NULL)) return -1;
    return (int)count;
}

static double platform_now_seconds(void)
{
    LARGE_INTEGER count, frequency;
    QueryPerformanceCounter(&count); QueryPerformanceFrequency(&frequency);
    return (double)count.QuadPart / (double)frequency.QuadPart;
}

static void platform_sleep_seconds(double seconds)
{
    double deadline, remaining;
    DWORD milliseconds;
    if (seconds <= 0.0) return;
    deadline = platform_now_seconds() + seconds;
    do {
        remaining = deadline - platform_now_seconds();
        milliseconds = remaining > 0.001 ? (DWORD)(remaining * 1000.0) : 0;
        Sleep(milliseconds);
    } while (platform_now_seconds() < deadline);
}

static int platform_frame_initialize(double *next_frame, double frame_seconds)
{
    *next_frame = platform_now_seconds() + frame_seconds;
    return 0;
}

static int platform_wait_next_frame(double *next_frame, double frame_seconds)
{
    double now, remaining;
    now = platform_now_seconds();
    remaining = *next_frame - now;
    if (remaining > 0.0) platform_sleep_seconds(remaining);
    now = platform_now_seconds();
    do { *next_frame += frame_seconds; } while (*next_frame <= now);
    return 0;
}

#else

#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

typedef struct platform_serial { int fd; } platform_serial_t;

static speed_t platform_baud_value(int baud)
{
    switch (baud) {
    case 9600: return B9600; case 19200: return B19200; case 38400: return B38400;
#ifdef B57600
    case 57600: return B57600;
#endif
#ifdef B115200
    case 115200: return B115200;
#endif
    default: return (speed_t)0;
    }
}

static int platform_serial_open(platform_serial_t *serial, const char *name, int baud)
{
    struct termios state; speed_t speed;
    if (serial == NULL || name == NULL) return -1;
    serial->fd = -1; speed = platform_baud_value(baud);
    if (speed == (speed_t)0) return -1;
    serial->fd = open(name, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (serial->fd < 0) return -1;
    if (tcgetattr(serial->fd, &state) != 0) goto fail;
    state.c_iflag = 0; state.c_oflag = 0; state.c_lflag = 0;
    state.c_cflag = CLOCAL | CREAD | CS8; state.c_cc[VMIN] = 0; state.c_cc[VTIME] = 0;
    if (cfsetispeed(&state, speed) != 0 || cfsetospeed(&state, speed) != 0) goto fail;
    if (tcsetattr(serial->fd, TCSANOW, &state) != 0) goto fail;
    tcflush(serial->fd, TCIOFLUSH); return 0;
fail:
    close(serial->fd); serial->fd = -1; return -1;
}

static void platform_serial_close(platform_serial_t *serial)
{
    if (serial != NULL && serial->fd >= 0) { close(serial->fd); serial->fd = -1; }
}

static int platform_serial_read(platform_serial_t *serial, unsigned char *buffer, size_t capacity)
{
    ssize_t count;
    if (serial == NULL || serial->fd < 0) return -1;
    count = read(serial->fd, buffer, capacity);
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return 0;
    return count < 0 ? -1 : (int)count;
}

static double platform_now_seconds(void)
{
    struct timeval value; gettimeofday(&value, NULL);
    return (double)value.tv_sec + (double)value.tv_usec / 1000000.0;
}

static void platform_sleep_seconds(double seconds)
{
    double deadline, remaining;
    struct timeval timeout;
    if (seconds <= 0.0) return;
    deadline = platform_now_seconds() + seconds;
    do {
        remaining = deadline - platform_now_seconds();
        if (remaining <= 0.0) break;
        timeout.tv_sec = (long)remaining;
        timeout.tv_usec = (long)((remaining - (double)timeout.tv_sec) * 1000000.0);
        select(0, NULL, NULL, NULL, &timeout);
    } while (platform_now_seconds() < deadline);
}

static int platform_frame_initialize(double *next_frame, double frame_seconds)
{
    *next_frame = platform_now_seconds() + frame_seconds;
    return 0;
}

static int platform_wait_next_frame(double *next_frame, double frame_seconds)
{
    double now, remaining;
    now = platform_now_seconds();
    remaining = *next_frame - now;
    if (remaining > 0.0) platform_sleep_seconds(remaining);
    now = platform_now_seconds();
    do { *next_frame += frame_seconds; } while (*next_frame <= now);
    return 0;
}

#endif
#endif
