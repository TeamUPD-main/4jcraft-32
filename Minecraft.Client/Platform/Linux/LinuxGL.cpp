#include "../../Minecraft.World/Platform/stdafx.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>
#include <dlfcn.h>

#include "../../Minecraft.World/IO/Streams/IntBuffer.h"
#include "../../Minecraft.World/IO/Streams/FloatBuffer.h"
#include "../../Minecraft.World/IO/Streams/ByteBuffer.h"
#include "../../Minecraft.World/Util/ArrayWithLength.h"
#include "../../4J.Render/4J_Render.h"


// Undefine macros from header to avoid argument mismatch during implementation
#undef glGenTextures
#undef glDeleteTextures
#undef glTexCoordPointer
#undef glNormalPointer
#undef glColorPointer
#undef glVertexPointer
#undef glTexImage2D
#undef glCallLists
#undef glReadPixels
#undef glFog
#undef glLight
#undef glLightModel
#undef glTexGen
#undef glGenQueriesARB
#undef glGetQueryObjectuARB

// ---------------------------------------------------------
// 1. The "_4J" versions (Used by files that include 4J_Render.h)
// ---------------------------------------------------------

int glGenTextures_4J() {
    GLuint id = 0;
    ::glGenTextures(1, &id);
    return (int)id;
}

void glGenTextures_4J(int n, unsigned int* textures) {
    ::glGenTextures(n, textures);
}

void glGenTextures_4J(IntBuffer* buf) {
    GLuint id = 0;
    ::glGenTextures(1, &id);
    buf->put((int)id);
    buf->flip();
}

void glDeleteTextures_4J(int id) {
    GLuint uid = (GLuint)id;
    ::glDeleteTextures(1, &uid);
}

void glDeleteTextures_4J(int n, const unsigned int* textures) {
    ::glDeleteTextures(n, textures);
}

void glDeleteTextures_4J(IntBuffer* buf) {
    if (buf->limit() > 0) {
        GLuint id = (GLuint)buf->get(0);
        ::glDeleteTextures(1, &id);
    }
}

// Destroy!
void glTexCoordPointer_4J(int size, int type, FloatBuffer* pointer) {}

void glNormalPointer_4J(int type, ByteBuffer* pointer) {}

void glColorPointer_4J(int size, bool normalized, int stride,
                       ByteBuffer* pointer) {}

void glVertexPointer_4J(int size, int type, FloatBuffer* pointer) {}

void glTexImage2D_4J(int target, int level, int internalformat, int width,
                     int height, int border, int format, int type,
                     void* pixels) {
    ::glTexImage2D((GLenum)target, level, internalformat, width, height, border,
                   (GLenum)format, (GLenum)type, pixels);
}

void glTexImage2D_4J(int target, int level, int internalformat, int width,
                     int height, int border, int format, int type,
                     ByteBuffer* pixels) {
    void* data = pixels ? pixels->getBuffer() : nullptr;
    ::glTexImage2D((GLenum)target, level, internalformat, width, height, border,
                   (GLenum)format, (GLenum)type, data);
}

// forward each list id to CBuffCall
void glCallLists_4J(IntBuffer* lists) {
    int base = lists->position();
    int count = lists->limit() - base;
    for (int i = 0; i < count; i++) {
        RenderManager.CBuffCall(lists->get(base + i));
    }
}

// GL 1.5 promotion
void glGenQueries_4J(IntBuffer* buf) {
    static PFNGLGENQUERIESPROC fn =
        (PFNGLGENQUERIESPROC)dlsym(RTLD_DEFAULT, "glGenQueries");
    if (fn) {
        GLuint id = 0;
        fn(1, &id);
        buf->put((int)id);
        buf->flip();
    }
}

void glGetQueryObjectu_4J(int id, int pname, IntBuffer* params) {
    static PFNGLGETQUERYOBJECTUIVPROC fn =
        (PFNGLGETQUERYOBJECTUIVPROC)dlsym(RTLD_DEFAULT, "glGetQueryObjectuiv");
    if (fn) {
        GLuint val = 0;
        fn((GLuint)id, (GLenum)pname, &val);
        params->put((int)val);
        params->flip();
    }
}

void glReadPixels_4J(int x, int y, int width, int height, int format, int type,
                     ByteBuffer* pixels) {
    ::glReadPixels(x, y, width, height, (GLenum)format, (GLenum)type,
                   pixels->getBuffer());
}

// fog lights model?
// now into rendermanager!
void glFog_4J(int pname, FloatBuffer* params) {
    float* p = params->_getDataPointer();
    if (pname == 0x0B66 /* GL_FOG_COLOR */) {
        RenderManager.StateSetFogColour(p[0], p[1], p[2]);
    }
}

void glLight_4J(int light, int pname, FloatBuffer* params) {
    float* p = params->_getDataPointer();
    if (pname == 0x1203 /* GL_POSITION */) {
        RenderManager.StateSetLightDirection(light == 0x4000 ? 0 : 1, p[0],
                                             p[1], p[2]);
    } else if (pname == 0x1200 /* GL_AMBIENT */) {
        RenderManager.StateSetLightAmbientColour(p[0], p[1], p[2]);
    } else if (pname == 0x1201 /* GL_DIFFUSE */) {
        RenderManager.StateSetLightColour(light == 0x4000 ? 0 : 1, p[0], p[1],
                                          p[2]);
    }
}

void glLightModel_4J(int pname, FloatBuffer* params) {
    float* p = params->_getDataPointer();
    if (pname == 0x0B53 /* GL_LIGHT_MODEL_AMBIENT */) {
        RenderManager.StateSetLightAmbientColour(p[0], p[1], p[2]);
    }
}

// TexGen is removed in core profile
void glTexGen_4J(int coord, int pname, FloatBuffer* params) {}

// fallbacks
int glGenTextures() { return glGenTextures_4J(); }
void glGenTextures(IntBuffer* buf) { glGenTextures_4J(buf); }

void glDeleteTextures(int id) { glDeleteTextures_4J(id); }
void glDeleteTextures(IntBuffer* buf) { glDeleteTextures_4J(buf); }

void glTexCoordPointer(int size, int type, FloatBuffer* pointer) {
    glTexCoordPointer_4J(size, type, pointer);
}
void glNormalPointer(int type, ByteBuffer* pointer) {
    glNormalPointer_4J(type, pointer);
}
void glColorPointer(int size, bool normalized, int stride,
                    ByteBuffer* pointer) {
    glColorPointer_4J(size, normalized, stride, pointer);
}
void glVertexPointer(int size, int type, FloatBuffer* pointer) {
    glVertexPointer_4J(size, type, pointer);
}

void glTexImage2D(int target, int level, int internalformat, int width,
                  int height, int border, int format, int type,
                  ByteBuffer* pixels) {
    glTexImage2D_4J(target, level, internalformat, width, height, border,
                    format, type, pixels);
}

void glCallLists(IntBuffer* lists) { glCallLists_4J(lists); }
void glGenQueriesARB(IntBuffer* buf) { glGenQueries_4J(buf); }
void glGetQueryObjectuARB(int id, int pname, IntBuffer* params) {
    glGetQueryObjectu_4J(id, pname, params);
}
void glReadPixels(int x, int y, int width, int height, int format, int type,
                  ByteBuffer* pixels) {
    glReadPixels_4J(x, y, width, height, format, type, pixels);
}
void glFog(int pname, FloatBuffer* params) { glFog_4J(pname, params); }
void glLight(int light, int pname, FloatBuffer* params) {
    glLight_4J(light, pname, params);
}
void glLightModel(int pname, FloatBuffer* params) {
    glLightModel_4J(pname, params);
}
void glTexGen(int coord, int pname, FloatBuffer* params) {
    glTexGen_4J(coord, pname, params);
}

// c hooks, disgust.
// These intercept any code that bypasses the macro system and calls the
// fixed-function GL functions directly. All roads goes to RenderManager;
extern "C" {
    void glFogfv(GLenum pname, const GLfloat* params) {
        if (pname == 0x0B66)
            RenderManager.StateSetFogColour(params[0], params[1], params[2]);
    }
    void glLightfv(GLenum light, GLenum pname, const GLfloat* params) {
        if (pname == 0x1203)
            RenderManager.StateSetLightDirection(light == 0x4000 ? 0 : 1, params[0],
                                                 params[1], params[2]);
        else if (pname == 0x1200)
            RenderManager.StateSetLightAmbientColour(params[0], params[1],
                                                     params[2]);
        else if (pname == 0x1201)
            RenderManager.StateSetLightColour(light == 0x4000 ? 0 : 1, params[0],
                                              params[1], params[2]);
    }
    void glLightModelfv(GLenum pname, const GLfloat* params) {
        if (pname == 0x0B53)
            RenderManager.StateSetLightAmbientColour(params[0], params[1],
                                                     params[2]);
    }
    
    void glShadeModel(GLenum) {}
    
    void glColorMaterial(GLenum face, GLenum mode) {
        // shader handles that
    }

    void glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz) {
        // shader handles that
    }
}