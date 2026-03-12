// HIP compatibility replacement for NVIDIA's helper_gl.h
// Provides OpenGL utility functions.

#ifndef HELPER_GL_H
#define HELPER_GL_H

#include <GL/gl.h>
#include <GL/glu.h>
#include <cstdio>
#include <cstring>

// Check for OpenGL errors
#ifndef SDK_CHECK_ERROR_GL
inline bool sdkCheckErrorGL(const char *file, const int line)
{
    bool ret_val = true;
    GLenum gl_error;
    while ((gl_error = glGetError()) != GL_NO_ERROR) {
        fprintf(stderr, "GL Error in file '%s' in line %d :\n", file, line);
        fprintf(stderr, "    %s\n", (const char *)gluErrorString(gl_error));
        ret_val = false;
    }
    return ret_val;
}
#define SDK_CHECK_ERROR_GL() sdkCheckErrorGL(__FILE__, __LINE__)
#endif

inline bool isGLVersionSupported(unsigned reqMajor, unsigned reqMinor)
{
    const char *version = (const char *)glGetString(GL_VERSION);
    if (!version) return false;
    unsigned major = 0, minor = 0;
    if (sscanf(version, "%u.%u", &major, &minor) == 2) {
        return (major > reqMajor) || (major == reqMajor && minor >= reqMinor);
    }
    return false;
}

inline bool areGLExtensionsSupported(const char *extList)
{
    const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
    if (!extensions) return false;

    // Check each requested extension
    char buf[256];
    const char *p = extList;
    while (*p) {
        // skip whitespace
        while (*p == ' ') p++;
        if (!*p) break;
        const char *start = p;
        while (*p && *p != ' ') p++;
        size_t len = p - start;
        if (len >= sizeof(buf)) len = sizeof(buf) - 1;
        memcpy(buf, start, len);
        buf[len] = '\0';
        if (!strstr(extensions, buf)) return false;
    }
    return true;
}

// On-screen text rendering helpers (used in fullscreen mode)
inline void beginWinCoords()
{
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glTranslatef(0.0f, (float)glutGet(0x006A), 0.0f); // GLUT_WINDOW_HEIGHT
    glScalef(1.0f, -1.0f, 1.0f);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, glutGet(0x0069), 0, glutGet(0x006A), -1, 1); // GLUT_WINDOW_WIDTH, GLUT_WINDOW_HEIGHT
    glMatrixMode(GL_MODELVIEW);
}

inline void endWinCoords()
{
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

inline void glPrint(int x, int y, const char *s, void *font)
{
    glRasterPos2f((float)x, (float)y);
    while (*s) {
        glutBitmapCharacter(font, *s);
        s++;
    }
}

#endif // HELPER_GL_H
