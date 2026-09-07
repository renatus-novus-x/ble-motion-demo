#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <x68k/dos.h>
#include <x68k/iocs.h>
#include <GL/glut.h>

#define MATRIX_SIZE 16
#define MATRIX_STACK_DEPTH 8
#define MAX_PAGE_LINES 128
#define CENTISECONDS_PER_DAY 8640000L
#define VDISP_TIMEOUT_CENTISECONDS 200L
#define MFP_GPIP ((const void *)0x00e88001)
#define GPIP_VDISP 0x10

typedef struct matrix4 { GLfloat value[MATRIX_SIZE]; } matrix4_t;
typedef struct screen_line {
    short x0, y0, x1, y1;
} screen_line_t;

static GLUTdisplayCB display_callback;
static GLUTreshapeCB reshape_callback;
static GLUTidleCB idle_callback;
static GLUTkeyboardCB keyboard_callback;
static int requested_width = 512;
static int requested_height = 512;
static int redisplay_requested;
static int running;
static int initialized;
static int old_mode;
static int front_page;
static int back_page;
static screen_line_t page_lines[2][MAX_PAGE_LINES];
static int page_line_count[2];

static matrix4_t modelview_matrix;
static matrix4_t projection_matrix;
static matrix4_t modelview_stack[MATRIX_STACK_DEPTH];
static matrix4_t projection_stack[MATRIX_STACK_DEPTH];
static int modelview_stack_depth;
static int projection_stack_depth;
static GLenum matrix_mode = GL_MODELVIEW;
static GLenum primitive_mode;
static int primitive_vertex_count;
static int first_x, first_y;
static unsigned short drawing_color = 1;
static int viewport_x, viewport_y;
static int viewport_width = 512;
static int viewport_height = 512;

static void matrix_identity(matrix4_t *matrix)
{
    int index;
    memset(matrix->value, 0, sizeof(matrix->value));
    for (index = 0; index < 4; ++index) matrix->value[index * 5] = 1.0f;
}

static void matrix_multiply(matrix4_t *result,
                            const matrix4_t *left,
                            const matrix4_t *right)
{
    matrix4_t temporary;
    int row, column, index;
    for (column = 0; column < 4; ++column) {
        for (row = 0; row < 4; ++row) {
            GLfloat sum = 0.0f;
            for (index = 0; index < 4; ++index)
                sum += left->value[row + index * 4] *
                       right->value[index + column * 4];
            temporary.value[row + column * 4] = sum;
        }
    }
    *result = temporary;
}

static matrix4_t *current_matrix(void)
{
    return matrix_mode == GL_PROJECTION ?
        &projection_matrix : &modelview_matrix;
}

static long ontime_diff(struct iocs_time start, struct iocs_time end)
{
    return ((long)end.day - (long)start.day) * CENTISECONDS_PER_DAY +
           (long)end.sec - (long)start.sec;
}

static int wait_vdisp(void)
{
    struct iocs_time start;
    int level;
    int next;
    start = _iocs_ontime();
    level = _iocs_b_bpeek(MFP_GPIP) & GPIP_VDISP;
    for (;;) {
        next = _iocs_b_bpeek(MFP_GPIP) & GPIP_VDISP;
        if (next != level) {
            level = next;
            if (level != 0) return 0;
        }
        if (ontime_diff(start, _iocs_ontime()) >
            VDISP_TIMEOUT_CENTISECONDS) return -1;
    }
}

static int set_60hz(void)
{
    if (wait_vdisp() != 0) return -1;
    _iocs_b_wpoke((void *)0x00e8000a, 0x0001);
    _iocs_b_wpoke((void *)0x00e8000c, 0x0022);
    _iocs_b_wpoke((void *)0x00e8000e, 0x0202);
    _iocs_b_wpoke((void *)0x00e80008, 0x020c);
    return 0;
}

static void draw_iocs_line(int x0, int y0, int x1, int y1,
                           unsigned short color)
{
    struct iocs_lineptr line;
    line.x1 = (short)x0; line.y1 = (short)y0;
    line.x2 = (short)x1; line.y2 = (short)y1;
    line.color = color;
    line.linestyle = 0xffff;
    _iocs_line(&line);
}

static void clear_page(int page)
{
    struct iocs_fillptr rectangle;
    rectangle.x1 = 0; rectangle.y1 = 0;
    rectangle.x2 = 511; rectangle.y2 = 511;
    rectangle.color = 0;
    _iocs_apage(page);
    _iocs_fill(&rectangle);
}

static void finalize_graphics(void)
{
    int mode;
    if (!initialized) return;
    clear_page(0);
    clear_page(1);
    _iocs_b_curon();
    mode = old_mode;
    if (mode < 0 || mode > 0x7f) mode = 12;
    _iocs_crtmod(mode);
    initialized = 0;
}

static void transform_vertex(GLfloat x, GLfloat y, GLfloat z,
                             int *screen_x, int *screen_y)
{
    GLfloat source[4], eye[4], clip[4];
    GLfloat normalized_x, normalized_y;
    int row, index;
    source[0] = x; source[1] = y; source[2] = z; source[3] = 1.0f;
    for (row = 0; row < 4; ++row) {
        eye[row] = 0.0f;
        for (index = 0; index < 4; ++index)
            eye[row] += modelview_matrix.value[row + index * 4] * source[index];
    }
    for (row = 0; row < 4; ++row) {
        clip[row] = 0.0f;
        for (index = 0; index < 4; ++index)
            clip[row] += projection_matrix.value[row + index * 4] * eye[index];
    }
    if (clip[3] == 0.0f) clip[3] = 1.0f;
    normalized_x = clip[0] / clip[3];
    normalized_y = clip[1] / clip[3];
    *screen_x = viewport_x +
        (int)((normalized_x * 0.5f + 0.5f) * (GLfloat)(viewport_width - 1));
    *screen_y = viewport_y +
        (int)((0.5f - normalized_y * 0.5f) * (GLfloat)(viewport_height - 1));
}

void glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    (void)red; (void)green; (void)blue; (void)alpha;
}

void glClear(GLbitfield mask)
{
    int index;
    if ((mask & GL_COLOR_BUFFER_BIT) == 0) return;
    _iocs_apage(back_page);
    for (index = 0; index < page_line_count[back_page]; ++index) {
        screen_line_t *line = &page_lines[back_page][index];
        draw_iocs_line(line->x0, line->y0, line->x1, line->y1, 0);
    }
    page_line_count[back_page] = 0;
}

void glMatrixMode(GLenum mode)
{
    if (mode == GL_MODELVIEW || mode == GL_PROJECTION) matrix_mode = mode;
}

void glLoadIdentity(void)
{
    matrix_identity(current_matrix());
}

void glPushMatrix(void)
{
    if (matrix_mode == GL_PROJECTION) {
        if (projection_stack_depth < MATRIX_STACK_DEPTH)
            projection_stack[projection_stack_depth++] = projection_matrix;
    } else {
        if (modelview_stack_depth < MATRIX_STACK_DEPTH)
            modelview_stack[modelview_stack_depth++] = modelview_matrix;
    }
}

void glPopMatrix(void)
{
    if (matrix_mode == GL_PROJECTION) {
        if (projection_stack_depth > 0)
            projection_matrix = projection_stack[--projection_stack_depth];
    } else {
        if (modelview_stack_depth > 0)
            modelview_matrix = modelview_stack[--modelview_stack_depth];
    }
}

void glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
    matrix4_t transform;
    matrix_identity(&transform);
    transform.value[12] = x; transform.value[13] = y; transform.value[14] = z;
    matrix_multiply(current_matrix(), current_matrix(), &transform);
}

void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
    matrix4_t rotation;
    GLfloat radians, cosine, sine, inverse_cosine, length;
    length = (GLfloat)sqrt((double)(x * x + y * y + z * z));
    if (length <= 0.000001f) return;
    x /= length; y /= length; z /= length;
    radians = angle * 3.14159265358979323846f / 180.0f;
    cosine = (GLfloat)cos((double)radians);
    sine = (GLfloat)sin((double)radians);
    inverse_cosine = 1.0f - cosine;
    matrix_identity(&rotation);
    rotation.value[0] = x * x * inverse_cosine + cosine;
    rotation.value[4] = x * y * inverse_cosine - z * sine;
    rotation.value[8] = x * z * inverse_cosine + y * sine;
    rotation.value[1] = y * x * inverse_cosine + z * sine;
    rotation.value[5] = y * y * inverse_cosine + cosine;
    rotation.value[9] = y * z * inverse_cosine - x * sine;
    rotation.value[2] = z * x * inverse_cosine - y * sine;
    rotation.value[6] = z * y * inverse_cosine + x * sine;
    rotation.value[10] = z * z * inverse_cosine + cosine;
    matrix_multiply(current_matrix(), current_matrix(), &rotation);
}

void glScalef(GLfloat x, GLfloat y, GLfloat z)
{
    matrix4_t scale;
    matrix_identity(&scale);
    scale.value[0] = x; scale.value[5] = y; scale.value[10] = z;
    matrix_multiply(current_matrix(), current_matrix(), &scale);
}

void glOrtho(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top,
             GLfloat near_value, GLfloat far_value)
{
    matrix4_t projection;
    matrix_identity(&projection);
    projection.value[0] = 2.0f / (right - left);
    projection.value[5] = 2.0f / (top - bottom);
    projection.value[10] = -2.0f / (far_value - near_value);
    projection.value[12] = -(right + left) / (right - left);
    projection.value[13] = -(top + bottom) / (top - bottom);
    projection.value[14] = -(far_value + near_value) /
                           (far_value - near_value);
    matrix_multiply(current_matrix(), current_matrix(), &projection);
}

void glViewport(int x, int y, int width, int height)
{
    viewport_x = x; viewport_y = y;
    viewport_width = width; viewport_height = height;
}

void glColor3ub(GLubyte red, GLubyte green, GLubyte blue)
{
    drawing_color = (red == 0 && green == 0 && blue == 0) ? 0 : 1;
}

void glBegin(GLenum mode)
{
    primitive_mode = mode;
    primitive_vertex_count = 0;
}

void glVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
    int screen_x, screen_y;
    screen_line_t *line;
    if (primitive_mode != GL_LINES) return;
    transform_vertex(x, y, z, &screen_x, &screen_y);
    if ((primitive_vertex_count & 1) == 0) {
        first_x = screen_x; first_y = screen_y;
    } else {
        draw_iocs_line(first_x, first_y, screen_x, screen_y, drawing_color);
        if (page_line_count[back_page] < MAX_PAGE_LINES) {
            line = &page_lines[back_page][page_line_count[back_page]++];
            line->x0 = (short)first_x; line->y0 = (short)first_y;
            line->x1 = (short)screen_x; line->y1 = (short)screen_y;
        }
    }
    ++primitive_vertex_count;
}

void glEnd(void)
{
    primitive_mode = 0;
}

void glFlush(void)
{
}

void glutInit(int *argc, char **argv)
{
    (void)argc; (void)argv;
    matrix_identity(&modelview_matrix);
    matrix_identity(&projection_matrix);
}

void glutInitDisplayMode(unsigned int mode)
{
    (void)mode;
}

void glutInitWindowSize(int width, int height)
{
    requested_width = width;
    requested_height = height;
}

int glutCreateWindow(const char *title)
{
    (void)title;
    if (requested_width != 512 || requested_height != 512) return 0;
    old_mode = _iocs_crtmod(-1);
    _iocs_crtmod(8);
    _iocs_g_clr_on();
    _iocs_window(0, 0, 511, 511);
    _iocs_gpalet(0, 0x0000);
    _iocs_gpalet(1, 0xffff);
    clear_page(0);
    clear_page(1);
    front_page = 0;
    back_page = 1;
    _iocs_apage(front_page);
    _iocs_vpage(1 << front_page);
    _iocs_b_curoff();
    if (set_60hz() != 0) {
        _iocs_b_curon();
        _iocs_crtmod(old_mode);
        return 0;
    }
    _iocs_apage(back_page);
    initialized = 1;
    atexit(finalize_graphics);
    return 1;
}

void glutDisplayFunc(GLUTdisplayCB callback)
{
    display_callback = callback;
}

void glutReshapeFunc(GLUTreshapeCB callback)
{
    reshape_callback = callback;
}

void glutKeyboardFunc(GLUTkeyboardCB callback)
{
    keyboard_callback = callback;
}

void glutIdleFunc(GLUTidleCB callback)
{
    idle_callback = callback;
}

void glutPostRedisplay(void)
{
    redisplay_requested = 1;
}

void glutSwapBuffers(void)
{
    int old_front;
    if (!initialized || wait_vdisp() != 0) {
        running = 0;
        return;
    }
    _iocs_vpage(1 << back_page);
    old_front = front_page;
    front_page = back_page;
    back_page = old_front;
    _iocs_apage(back_page);
}

void glutMainLoop(void)
{
    int key;
    running = initialized;
    redisplay_requested = 1;
    if (reshape_callback != NULL)
        reshape_callback(requested_width, requested_height);
    while (running) {
        if (idle_callback != NULL) idle_callback();
        if (_dos_keysns() != 0) {
            key = _dos_inkey();
            if (keyboard_callback != NULL)
                keyboard_callback((unsigned char)(key & 0xff), 0, 0);
        }
        if (redisplay_requested && display_callback != NULL) {
            redisplay_requested = 0;
            display_callback();
        }
    }
}

void glutLeaveMainLoop(void)
{
    running = 0;
}
