/* Stub GLUT library for headless linking */
#include <stdio.h>
#include <stdlib.h>

typedef unsigned int GLenum;

void glutReportErrors(void) {}
void glutSwapBuffers(void) {}
void glutPostRedisplay(void) {}
void glutSetWindowTitle(const char *title) {}
int  glutGetModifiers(void) { return 0; }
int  glutGet(GLenum type) { return 0; }
void glutSolidSphere(double radius, int slices, int stacks) {}
void glutInit(int *argc, char **argv) {}
void glutInitDisplayMode(unsigned int mode) {}
void glutInitWindowSize(int width, int height) {}
int  glutCreateWindow(const char *title) { return 1; }
void glutDisplayFunc(void (*func)(void)) {}
void glutReshapeFunc(void (*func)(int, int)) {}
void glutMouseFunc(void (*func)(int, int, int, int)) {}
void glutMotionFunc(void (*func)(int, int)) {}
void glutKeyboardFunc(void (*func)(unsigned char, int, int)) {}
void glutKeyboardUpFunc(void (*func)(unsigned char, int, int)) {}
void glutSpecialFunc(void (*func)(int, int, int)) {}
void glutIdleFunc(void (*func)(void)) {}
void glutMainLoop(void) {}
int  glutCreateMenu(void (*func)(int)) { return 0; }
void glutAddMenuEntry(const char *label, int value) {}
void glutAttachMenu(int button) {}
void glutBitmapCharacter(void *font, int character) {}

/* glutBitmap9By15 is a global font pointer */
void *glutBitmap9By15 = (void*)0;
