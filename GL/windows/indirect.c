
#include <GL/gl.h>
#include <glxserver.h>
#include <glxext.h>

#define WINDOWS_LEAN_AND_CLEAN
//typedef int BOOL;
//typedef unsigned short ATOM;
//typedef unsigned char BYTE;
#include <windows.h>
/*
 * GLX implementation that uses Win32's OpenGL
 */

/*
 * Server-side GLX uses these functions which are normally defined
 * in the OpenGL SI.
 */

GLint __glEvalComputeK(GLenum target)
{
    switch (target) {
    case GL_MAP1_VERTEX_4:
    case GL_MAP1_COLOR_4:
    case GL_MAP1_TEXTURE_COORD_4:
    case GL_MAP2_VERTEX_4:
    case GL_MAP2_COLOR_4:
    case GL_MAP2_TEXTURE_COORD_4:
	return 4;
    case GL_MAP1_VERTEX_3:
    case GL_MAP1_TEXTURE_COORD_3:
    case GL_MAP1_NORMAL:
    case GL_MAP2_VERTEX_3:
    case GL_MAP2_TEXTURE_COORD_3:
    case GL_MAP2_NORMAL:
	return 3;
    case GL_MAP1_TEXTURE_COORD_2:
    case GL_MAP2_TEXTURE_COORD_2:
	return 2;
    case GL_MAP1_TEXTURE_COORD_1:
    case GL_MAP2_TEXTURE_COORD_1:
    case GL_MAP1_INDEX:
    case GL_MAP2_INDEX:
	return 1;
    default:
	return 0;
    }
}

GLuint __glFloorLog2(GLuint val)
{
    int c = 0;

    while (val > 1) {
	c++;
	val >>= 1;
    }
    return c;
}


/*
 * Wrapper functions
 */

#define RESOLVE(procname, symbol)                                       \
    static Bool init = TRUE; \
    static procname proc = NULL; \
    if (init) { \
        proc = (procname)wglGetProcAddress(symbol); \
        init = FALSE; \
    } \
    if (proc == NULL) { \
        __glXErrorCallBack(NULL, 0); \
        return; \
    } 
        
        


GLAPI void GLAPIENTRY glGetColorTable( GLenum target, GLenum format,
                                       GLenum type, GLvoid *table )
{
    RESOLVE(PFNGLGETCOLORTABLEPROC, "glGetColorTableEXT");
    proc(target, format, type, table);
}

GLAPI void GLAPIENTRY glGetColorTableParameterfv( GLenum target, GLenum pname,
                                                  GLfloat *params )
{
    RESOLVE(PFNGLGETCOLORTABLEPARAMETERFVPROC, "glGetColorTableParameterfvEXT");
    proc(target, pname, params);
}

GLAPI void GLAPIENTRY glGetColorTableParameteriv( GLenum target, GLenum pname,
                                                  GLint *params )
{
    RESOLVE(PFNGLGETCOLORTABLEPARAMETERIVPROC, "glGetColorTableParameterivEXT");
    proc(target, pname, params);
}

