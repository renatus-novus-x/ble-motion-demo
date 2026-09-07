#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif
#include "platform.h"

#define FRAME_SECONDS (1.0 / 60.0)
#define SERIAL_READ_SIZE 256
#define SERIAL_FRAME_BUDGET 1024
#define QUATERNION_PACKET_SIZE 19

typedef struct quaternion { float x, y, z, w; } quaternion_t;
typedef struct packet_decoder {
    unsigned char bytes[QUATERNION_PACKET_SIZE];
    int length;
} packet_decoder_t;

static platform_serial_t serial_port;
static int serial_open;
static int serial_failed;
static packet_decoder_t decoder;
static quaternion_t orientation = {0.0f, 0.0f, 0.0f, 1.0f};
static int have_orientation;
static double next_frame;

static const GLfloat vertices[8][3] = {
    {-1.0f, -1.0f, -1.0f}, { 1.0f, -1.0f, -1.0f},
    { 1.0f,  1.0f, -1.0f}, {-1.0f,  1.0f, -1.0f},
    {-1.0f, -1.0f,  1.0f}, { 1.0f, -1.0f,  1.0f},
    { 1.0f,  1.0f,  1.0f}, {-1.0f,  1.0f,  1.0f}
};
static const unsigned char edges[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0},
    {4, 5}, {5, 6}, {6, 7}, {7, 4},
    {0, 4}, {1, 5}, {2, 6}, {3, 7}
};

static float read_float_le(const unsigned char *bytes)
{
    union { float value; unsigned char bytes[4]; } result;
    unsigned short marker = 1;
    int little_endian = *((unsigned char *)&marker) != 0;
    int index;
    for (index = 0; index < 4; ++index)
        result.bytes[index] = bytes[little_endian ? index : 3 - index];
    return result.value;
}

static int packet_checksum_valid(const unsigned char *bytes)
{
    unsigned int sum = 0;
    int index;
    for (index = 0; index < QUATERNION_PACKET_SIZE - 1; ++index)
        sum = (sum + bytes[index]) & 0xffU;
    return ((sum + bytes[QUATERNION_PACKET_SIZE - 1]) & 0xffU) == 0xffU;
}

static void accept_quaternion_packet(const unsigned char *bytes)
{
    quaternion_t value;
    float length;
    value.x = read_float_le(bytes + 2); value.y = read_float_le(bytes + 6);
    value.z = read_float_le(bytes + 10); value.w = read_float_le(bytes + 14);
    length = (float)sqrt((double)(value.x * value.x + value.y * value.y +
                                 value.z * value.z + value.w * value.w));
    if (length < 0.0001f) return;
    orientation.x = value.x / length; orientation.y = value.y / length;
    orientation.z = value.z / length; orientation.w = value.w / length;
    have_orientation = 1;
}

static void decode_byte(unsigned char value)
{
    if (decoder.length == 0) {
        if (value == '!') decoder.bytes[decoder.length++] = value;
        return;
    }
    if (decoder.length == 1) {
        if (value == 'Q') decoder.bytes[decoder.length++] = value;
        else decoder.length = value == '!' ? 1 : 0;
        return;
    }
    decoder.bytes[decoder.length++] = value;
    if (decoder.length == QUATERNION_PACKET_SIZE) {
        if (packet_checksum_valid(decoder.bytes)) accept_quaternion_packet(decoder.bytes);
        decoder.length = 0;
    }
}

static void poll_serial(void)
{
    unsigned char buffer[SERIAL_READ_SIZE];
    int total = 0, count, index;
    if (!serial_open || serial_failed) return;
    do {
        count = platform_serial_read(&serial_port, buffer, sizeof(buffer));
        if (count < 0) {
            serial_failed = 1; fprintf(stderr, "Serial communication failed.\n"); return;
        }
        for (index = 0; index < count; ++index) decode_byte(buffer[index]);
        total += count;
    } while (count > 0 && total < SERIAL_FRAME_BUDGET);
}

static void apply_orientation(void)
{
    float w, angle, axis_length;
    if (!have_orientation) {
        glRotatef(25.0f, 1.0f, 0.0f, 0.0f);
        glRotatef(-35.0f, 0.0f, 1.0f, 0.0f); return;
    }
    w = orientation.w;
    if (w < -1.0f) w = -1.0f;
    if (w > 1.0f) w = 1.0f;
    angle = (float)(2.0 * acos((double)w) * 180.0 / 3.14159265358979323846);
    axis_length = (float)sqrt((double)(orientation.x * orientation.x +
                                      orientation.y * orientation.y +
                                      orientation.z * orientation.z));
    if (axis_length > 0.0001f)
        glRotatef(angle, orientation.x / axis_length,
                 orientation.y / axis_length, orientation.z / axis_length);
}

static void display(void)
{
    int i, j;
    const GLfloat *vertex;
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    apply_orientation();
    glColor3ub(255, 255, 255); glBegin(GL_LINES);
    for (i = 0; i < 12; ++i) for (j = 0; j < 2; ++j) {
        vertex = vertices[edges[i][j]];
        glVertex3f(vertex[0], vertex[1], vertex[2]);
    }
    glEnd();
    glFlush();
    glutSwapBuffers();
}

static void reshape(int width, int height)
{
    GLfloat aspect;
    if (width < 1) width = 1;
    if (height < 1) height = 1;
    aspect = (GLfloat)width / (GLfloat)height;
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    if (aspect >= 1.0f) glOrtho(-2.0 * aspect, 2.0 * aspect, -2.0, 2.0, -4.0, 4.0);
    else glOrtho(-2.0, 2.0, -2.0 / aspect, 2.0 / aspect, -4.0, 4.0);
    glMatrixMode(GL_MODELVIEW);
}

static void idle(void)
{
    poll_serial();
    if (platform_wait_next_frame(&next_frame, FRAME_SECONDS) != 0) {
        fprintf(stderr, "Frame timing failed.\n");
        exit(EXIT_FAILURE);
    }
    glutPostRedisplay();
}

static void close_serial(void)
{
    if (serial_open) { platform_serial_close(&serial_port); serial_open = 0; }
}

static void keyboard(unsigned char key, int x, int y)
{
    (void)x; (void)y;
    if (key == 27 || key == 'q' || key == 'Q') exit(EXIT_SUCCESS);
}

static void usage(const char *name)
{
#ifdef __human68k__
    fprintf(stderr, "Usage: %s\n", name);
#else
    fprintf(stderr, "Usage: %s [serial-port] [baud]\nExample: %s COM9 38400\n", name, name);
#endif
}

int main(int argc, char **argv)
{
    int baud = 38400;
    int glut_argc;
    int window;
    char *glut_argv[2];
    if (sizeof(float) != 4) {
        fprintf(stderr, "This program requires a 32-bit float type.\n");
        return EXIT_FAILURE;
    }
#ifdef __human68k__
    if (argc != 1) { usage(argv[0]); return EXIT_FAILURE; }
    if (platform_serial_open(&serial_port, "AUX", baud) != 0) {
        fprintf(stderr, "Cannot initialize RS-232C.\n");
        return EXIT_FAILURE;
    }
    serial_open = 1;
    atexit(close_serial);
#else
    if (argc > 3) { usage(argv[0]); return EXIT_FAILURE; }
    if (argc >= 3) baud = atoi(argv[2]);
    if (argc >= 2) {
        if (platform_serial_open(&serial_port, argv[1], baud) != 0) {
            fprintf(stderr, "Cannot open serial port %s at %d bps.\n", argv[1], baud);
            return EXIT_FAILURE;
        }
        serial_open = 1; atexit(close_serial);
        printf("Serial port: %s, %d bps, 8N1, no flow control\n", argv[1], baud);
    }
#endif
    glut_argc = 1; glut_argv[0] = argv[0]; glut_argv[1] = NULL;
    glutInit(&glut_argc, glut_argv); glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
    glutInitWindowSize(512, 512);
    window = glutCreateWindow("ble-motion-demo");
    if (window == 0) {
        fprintf(stderr, "Cannot initialize graphics.\n");
        return EXIT_FAILURE;
    }
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glutDisplayFunc(display); glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard); glutIdleFunc(idle);
    if (platform_frame_initialize(&next_frame, FRAME_SECONDS) != 0) {
        fprintf(stderr, "Cannot initialize frame timing.\n");
        return EXIT_FAILURE;
    }
    glutMainLoop();
    return EXIT_SUCCESS;
}
