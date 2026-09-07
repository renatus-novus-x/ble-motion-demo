#ifndef BLE_MOTION_X68K_GLUT_H
#define BLE_MOTION_X68K_GLUT_H

#include <GL/gl.h>

#define GLUT_RGB 0
#define GLUT_RGBA 0
#define GLUT_SINGLE 0
#define GLUT_DOUBLE 2

typedef void (*GLUTdisplayCB)(void);
typedef void (*GLUTreshapeCB)(int width, int height);
typedef void (*GLUTidleCB)(void);
typedef void (*GLUTkeyboardCB)(unsigned char key, int x, int y);

void glutInit(int *argc, char **argv);
void glutInitDisplayMode(unsigned int mode);
void glutInitWindowSize(int width, int height);
int glutCreateWindow(const char *title);
void glutDisplayFunc(GLUTdisplayCB callback);
void glutReshapeFunc(GLUTreshapeCB callback);
void glutKeyboardFunc(GLUTkeyboardCB callback);
void glutIdleFunc(GLUTidleCB callback);
void glutPostRedisplay(void);
void glutSwapBuffers(void);
void glutMainLoop(void);
void glutLeaveMainLoop(void);

#endif
