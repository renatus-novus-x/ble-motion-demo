#ifndef BLE_MOTION_X68K_GL_H
#define BLE_MOTION_X68K_GL_H

typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef float GLfloat;
typedef unsigned char GLubyte;

#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_MODELVIEW 0x1700
#define GL_PROJECTION 0x1701
#define GL_LINES 0x0001

void glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void glClear(GLbitfield mask);
void glMatrixMode(GLenum mode);
void glLoadIdentity(void);
void glPushMatrix(void);
void glPopMatrix(void);
void glTranslatef(GLfloat x, GLfloat y, GLfloat z);
void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void glScalef(GLfloat x, GLfloat y, GLfloat z);
void glOrtho(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top,
             GLfloat near_value, GLfloat far_value);
void glViewport(int x, int y, int width, int height);
void glColor3ub(GLubyte red, GLubyte green, GLubyte blue);
void glBegin(GLenum mode);
void glVertex3f(GLfloat x, GLfloat y, GLfloat z);
void glEnd(void);
void glFlush(void);

#endif
