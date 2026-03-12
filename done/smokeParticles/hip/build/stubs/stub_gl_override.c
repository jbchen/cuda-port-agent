/* GL function overrides for headless operation.
 * These replace libGL functions that crash without a real GL context.
 * Link BEFORE libGL.so so these take priority.
 */
typedef unsigned int GLenum;
typedef unsigned char GLubyte;
typedef int GLint;
typedef float GLfloat;
typedef unsigned int GLuint;
typedef int GLsizei;
typedef unsigned int GLbitfield;
typedef double GLdouble;
typedef float GLclampf;
typedef void GLvoid;

/* String queries - must return valid strings */
const GLubyte *glGetString(GLenum name) {
    switch (name) {
        case 0x1F03: /* GL_EXTENSIONS */
            return (const GLubyte *)"GL_ARB_multitexture GL_ARB_vertex_buffer_object GL_EXT_geometry_shader4";
        case 0x1F02: /* GL_VERSION */
            return (const GLubyte *)"4.6.0 Stub";
        case 0x1F01: /* GL_RENDERER */
            return (const GLubyte *)"Stub Renderer";
        case 0x1F00: /* GL_VENDOR */
            return (const GLubyte *)"Stub";
        default:
            return (const GLubyte *)"";
    }
}

/* State management - no-ops */
void glEnable(GLenum cap) {}
void glDisable(GLenum cap) {}
void glBlendFunc(GLenum sfactor, GLenum dfactor) {}
void glDepthMask(unsigned char flag) {}
void glColorMask(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {}
void glClear(GLbitfield mask) {}
void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a) {}
void glViewport(GLint x, GLint y, GLsizei w, GLsizei h) {}
void glPixelStorei(GLenum pname, GLint param) {}

/* Matrix operations - no-ops */
void glMatrixMode(GLenum mode) {}
void glLoadIdentity(void) {}
void glLoadMatrixf(const GLfloat *m) {}
void glPushMatrix(void) {}
void glPopMatrix(void) {}
void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {}
void glTranslatef(GLfloat x, GLfloat y, GLfloat z) {}
void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {}
void glScalef(GLfloat x, GLfloat y, GLfloat z) {}

/* Immediate mode drawing - no-ops */
void glBegin(GLenum mode) {}
void glEnd(void) {}
void glVertex2f(GLfloat x, GLfloat y) {}
void glVertex3f(GLfloat x, GLfloat y, GLfloat z) {}
void glVertex3fv(const GLfloat *v) {}
void glNormal3f(GLfloat x, GLfloat y, GLfloat z) {}
void glColor3f(GLfloat r, GLfloat g, GLfloat b) {}
void glColor3fv(const GLfloat *v) {}
void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {}
void glTexCoord2f(GLfloat s, GLfloat t) {}
void glRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2) {}
void glRasterPos2f(GLfloat x, GLfloat y) {}

/* Buffer objects - minimal implementation */
static GLuint next_buffer = 1;
void glGenBuffers(GLsizei n, GLuint *buffers) {
    for (int i = 0; i < n; i++) buffers[i] = next_buffer++;
}
void glDeleteBuffers(GLsizei n, const GLuint *buffers) {}
void glBindBuffer(GLenum target, GLuint buffer) {}
void glBufferData(GLenum target, long size, const void *data, GLenum usage) {}

/* Texture objects - minimal implementation */
static GLuint next_texture = 1;
void glGenTextures(GLsizei n, GLuint *textures) {
    for (int i = 0; i < n; i++) textures[i] = next_texture++;
}
void glDeleteTextures(GLsizei n, const GLuint *textures) {}
void glBindTexture(GLenum target, GLuint texture) {}
void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width,
                  GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels) {}
void glTexParameteri(GLenum target, GLenum pname, GLint param) {}
void glTexParameterf(GLenum target, GLenum pname, GLfloat param) {}
void glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params) {}
void glActiveTexture(GLenum texture) {}
void glClientActiveTexture(GLenum texture) {}

/* Vertex arrays - no-ops */
void glEnableClientState(GLenum array) {}
void glDisableClientState(GLenum array) {}
void glVertexPointer(GLint size, GLenum type, GLsizei stride, const void *pointer) {}
void glColorPointer(GLint size, GLenum type, GLsizei stride, const void *pointer) {}
void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void *pointer) {}
void glDrawArrays(GLenum mode, GLint first, GLsizei count) {}
void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices) {}

/* Queries - return safe defaults */
void glGetIntegerv(GLenum pname, GLint *params) { if (params) *params = 0; }
void glGetFloatv(GLenum pname, GLfloat *params) { if (params) *params = 0.0f; }

/* Generic no-op function that all glXGetProcAddress lookups resolve to.
 * On x86_64 SysV ABI, this is safe for any signature with <=6 args
 * (all passed in registers, no stack corruption). */
static int gl_noop_func(void) { return 0; }

/* GLX - return pointer to no-op instead of NULL to prevent segfaults */
void (*glXGetProcAddress(const GLubyte *procName))(void) { return (void(*)(void))gl_noop_func; }
void (*glXGetProcAddressARB(const GLubyte *procName))(void) { return (void(*)(void))gl_noop_func; }

/* GL extension functions that helper_gl.h initializes via glXGetProcAddress.
 * Provide them directly so they override the function-pointer globals. */
void glBufferSubData(GLenum target, long offset, long size, const void *data) {}
GLuint glCreateProgram(void) { return 1; }
void glBindProgramARB(GLenum target, GLuint program) {}
void glGenProgramsARB(GLsizei n, GLuint *programs) { for (int i = 0; i < n; i++) programs[i] = 1; }
void glDeleteProgramsARB(GLsizei n, const GLuint *programs) {}
void glDeleteProgram(GLuint program) {}
void glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, char *infoLog) {
    if (length) *length = 0; if (bufSize > 0 && infoLog) infoLog[0] = '\0';
}
void glGetProgramiv(GLuint program, GLenum pname, GLint *params) { if (params) *params = 1; /* GL_TRUE for link status */ }
void glProgramParameteriEXT(GLuint program, GLenum pname, GLint value) {}
void glProgramStringARB(GLenum target, GLenum format, GLsizei len, const void *string) {}
unsigned char glUnmapBuffer(GLenum target) { return 1; }
void *glMapBuffer(GLenum target, GLenum access) { return (void*)0; }
void glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params) { if (params) *params = 0; }
void glLinkProgram(GLuint program) {}
void glUseProgram(GLuint program) {}
void glAttachShader(GLuint program, GLuint shader) {}
GLuint glCreateShader(GLenum type) { return 1; }
void glShaderSource(GLuint shader, GLsizei count, const char **string, const GLint *length) {}
void glCompileShader(GLuint shader) {}
void glDeleteShader(GLuint shader) {}
void glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, char *infoLog) {
    if (length) *length = 0; if (bufSize > 0 && infoLog) infoLog[0] = '\0';
}
void glGetShaderiv(GLuint shader, GLenum pname, GLint *params) { if (params) *params = 1; }
void glUniform1i(GLint location, GLint v0) {}
void glUniform1f(GLint location, GLfloat v0) {}
void glUniform2f(GLint location, GLfloat v0, GLfloat v1) {}
void glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {}
void glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {}
void glUniform1fv(GLint location, GLsizei count, const GLfloat *value) {}
void glUniform2fv(GLint location, GLsizei count, const GLfloat *value) {}
void glUniform3fv(GLint location, GLsizei count, const GLfloat *value) {}
void glUniform4fv(GLint location, GLsizei count, const GLfloat *value) {}
void glUniformMatrix4fv(GLint location, GLsizei count, unsigned char transpose, const GLfloat *value) {}
void glSecondaryColor3fv(const GLfloat *v) {}
GLint glGetUniformLocation(GLuint program, const char *name) { return 0; }
void glGenFramebuffersEXT(GLsizei n, GLuint *framebuffers) { for (int i = 0; i < n; i++) framebuffers[i] = 1; }
void glBindFramebufferEXT(GLenum target, GLuint framebuffer) {}
void glDeleteFramebuffersEXT(GLsizei n, const GLuint *framebuffers) {}
GLenum glCheckFramebufferStatusEXT(GLenum target) { return 0x8CD5; /* GL_FRAMEBUFFER_COMPLETE_EXT */ }
void glGetFramebufferAttachmentParameterivEXT(GLenum target, GLenum attachment, GLenum pname, GLint *params) { if (params) *params = 0; }
void glFramebufferTexture1DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {}
void glFramebufferTexture2DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {}
void glFramebufferTexture3DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset) {}
void glGenerateMipmapEXT(GLenum target) {}
void glGenRenderbuffersEXT(GLsizei n, GLuint *renderbuffers) { for (int i = 0; i < n; i++) renderbuffers[i] = 1; }
void glDeleteRenderbuffersEXT(GLsizei n, const GLuint *renderbuffers) {}
void glBindRenderbufferEXT(GLenum target, GLuint renderbuffer) {}
void glRenderbufferStorageEXT(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {}
void glFramebufferRenderbufferEXT(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {}
void glClampColorARB(GLenum target, GLenum clamp) {}
void glBindFragDataLocationEXT(GLuint program, GLuint color, const char *name) {}
