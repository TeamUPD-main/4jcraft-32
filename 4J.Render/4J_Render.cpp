#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "gl3_loader.h"
#include "4J_Render.h"

// undefine macros from header to avoid argument mismatch
#undef glGenTextures
#undef glDeleteTextures
#undef glTexImage2D
#undef glReadPixels
#undef glCallLists
#undef glFog
#undef glLight
#undef glLightModel
#undef glTexGen
#undef glTexCoordPointer
#undef glNormalPointer
#undef glColorPointer
#undef glVertexPointer
#undef glGenQueriesARB
#undef glGetQueryObjectuARB

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// This code is chaotic and horrible, so here's niko from oneshot for good luck:
//                +++++++++%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%=++++++
//              =++=+=+=+=+%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%#=====++=+=+=
//              ++++++++=%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%##==+++++=++++++
//            +=+=+=+=++=%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%+==++++=+=+++=++=+
//            ++++++++++=%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%#=+++=+=+++++=++++++
//          =++=++=++=++=%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%#=+++=+++++=+=+++=+=+=
//          =++++++=++++=%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%#=+++++=+=+++++=++++++
//         =+++=+=+++=++=%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%++++=+=+++++=+=+++=++=+
//        =+=++++++=++++=%%%%###%#%####%#++++++++++#%########%++++++++++=+=+++++=++++++
//        =+++=++=+++=+++##%#++++++++++++###########++++++++++++++=+++=+++++=+=+++=+=+=
//       +=+=++++++++*+****#####################################+*+*++++++=+++++=++++++
//       ++++++****##################################################*****++++++++=++=+
//       ***#*#############################################################*****=++++++
//   ###########################################################################**#++++
//#####################################################################################
//#####################################################################################
//#########################%%%%%%%%%%%%%%%%%%%%%%%%%%%%################################
//#############%%%%%%%%%%%%###**#####################%%%%%%%%%%%%%%%%%%%%%%%###########
//       %%%%################*#*##############****##################%%%%%%%%%%%%%%%####
//       %%%%%#############**#*#############****##***####################%%%%%%%%%%%%% 
//       %%%%%#############%%%#*############**#***#**#***##################%%%%%%%%%%% 
//       %%%%%###########%#:-%%%############*******#%%###%%%%###############%%%%%%%%%% 
//       %%%%%##########%.....=%############**#**#%+:+%##:::=%##**##########%%%%%%%%   
//       %%%%%#########%%.....=%############*###%*.....##:::::#%%###########%%%%%%%%   
//       %%%%   ######%:::::-%#%**##%#######=:-.%#.....*#:::::::%###########%%%%%%%%   
//%@%@%@%%%%%   ######%::::::%#%::%#.+%#####=:*%:::::+%##:::::::%%#########%%%%%%%%%   
//%@%%@%%@@@@@%   ####%::::::%#%::::=-:#####=:#%:::::=%##:::::::::%########%%%%%%%%%%%%
// @%@%@%%%%%%@@% %%##%::::::%#%::::-------%=-:::::::=%##:::::::::%########%%%%%%%%%%%%
//     %@@@%@%%@@@%%%#%::::::%#%::::=--------=:::::::=%##:::::::::%######%%%%%%%%%%%%%%
//          %@@%@%%%%%%=-::::%*#::::-=+---------:::::=%##:::::::::%######%%%%%%%%%%%%%%
//            %%%%%%%%%=--::::::::--------------:::::-#*+:::::::--%######%%%%%%%#%%%%%#
//       %%%%@%@%@@%%%%=---------------------------::::::::::::---#####%%%%%%%%%%%%%%%%
//   %@%%@%@%%@%@% %%%%+=--------------==------------------------=%#####%%%%%#%%%%%%%%%
//   %@%@%@%@@@%% %%%%%%%++----------==++==----------------------=###%%%%@%%%%%#####%%%
//   %%@%%@%%   %@%%%%%%%%#.---------=+--+=---------------------++##%@@@@@@%%%%%%%#### 
//            %@%@%@#%%%%%% =-++++------------------------=+*##*%%#%@@@%%%%%%#%%%%%%%% 
//          %@%@%@%@ %%%%%% ######++++*--------------=++***#%@@@##@%%%%#######%%%%%%%%%
//          %@%@%@%@ %%%% #############%%%%%%%%%##***######@@@@@################%%%%%%%
//          %@%%@%     %%##########%@@@@@@@@@@@@@@#######@@@%#####################%%%%%
//           @%@       %%#########@@@@@@@@@@@@@@@@@@@@@############################%%%%
//                     ##################@@@@@@@@@@%################################## 
//                   ################################################################# 
//                        ##########################################################   
//                            @@##################################################     
//                            %%################################################%%%%   
//                          %%################################################%%%%%%%% 
// okay now goodbye

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cassert>
#include <unordered_map>
#include <vector>
#include <pthread.h>
#include <dlfcn.h>

C4JRender RenderManager;

// Hello SDL and opengl 3.3
static SDL_Window* s_window = nullptr;
static SDL_GLContext s_glContext = nullptr;
static bool s_shouldClose = false;
static int s_windowWidth = 1920;
static int s_windowHeight = 1080;
static int s_reqWidth = 0;
static int s_reqHeight = 0;
// removed initial window size cuz i use hyprland and it causes silly issues.
static bool s_fullscreen = false;

static pthread_key_t s_glCtxKey;
static pthread_once_t s_glCtxKeyOnce = PTHREAD_ONCE_INIT;
static void makeGLCtxKey() { pthread_key_create(&s_glCtxKey, nullptr); }
// actually this number isn't that bad after that one fix.
static const int MAX_SHARED_CTXS = 6;
static SDL_Window* s_sharedWins[MAX_SHARED_CTXS] = {};
static SDL_GLContext s_sharedCtxs[MAX_SHARED_CTXS] = {};
static int s_sharedCtxCount = 0;
static int s_nextSharedCtx = 0;
static pthread_mutex_t s_sharedMtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_glCallMtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_t s_mainThread;
static bool s_mainThreadSet = false;

// Forward declarations
static void onFramebufferResize(int w, int h) {
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    s_windowWidth = w;
    s_windowHeight = h;
    glViewport(0, 0, w, h);
}

// Vertex shader source
static const char* VERT_SRC = R"GLSL(
#version 330 core
layout(location=0) in vec3  aPos;
layout(location=1) in vec2  aUV0;
layout(location=2) in vec4  aColor;
layout(location=3) in vec3  aNormal;
layout(location=4) in ivec2 aLMraw;

uniform mat4  uMVP;
uniform mat4  uMV;
uniform vec4  uBaseColor;
uniform int   uLighting;
uniform vec3  uLight0Dir;
uniform vec3  uLight1Dir;
uniform vec3  uLightDiffuse;
uniform vec3  uLightAmbient;
uniform int   uFogMode;
uniform float uFogStart;
uniform float uFogEnd;
uniform float uFogDensity;
uniform vec4  uLMTransform;
uniform vec2  uGlobalLM;

out vec2  vUV0;
out vec2  vUV1;
out vec4  vColor;
out float vFogFactor;

void main() {
    vec4 eyePos  = uMV  * vec4(aPos, 1.0);
    gl_Position  = uMVP * vec4(aPos, 1.0);
    vUV0 = aUV0;
    
    vec2 lm = (aLMraw.x <= -500) ? uGlobalLM : vec2(aLMraw);
    vUV1 = (lm / 256.0) * uLMTransform.xy + uLMTransform.zw;

    bool sentinel = (aColor == vec4(0.0));
    vec4 col = sentinel ? uBaseColor : aColor.abgr;

    if (uLighting == 1) {
        mat3 normalMatrix = transpose(inverse(mat3(uMV)));
        vec3 n   = normalize(normalMatrix * aNormal);
        float d0 = max(dot(n, uLight0Dir), 0.0);
        float d1 = max(dot(n, uLight1Dir), 0.0);
        vColor = vec4(col.rgb * (uLightAmbient + uLightDiffuse * (d0 + d1)), col.a);
    } else {
        vColor = col;
    }

    float eDist = length(eyePos.xyz);
    if      (uFogMode == 1) vFogFactor = clamp((uFogEnd - eDist) / max(uFogEnd - uFogStart, 1e-4), 0.0, 1.0);
    else if (uFogMode == 2) vFogFactor = clamp(exp(-uFogDensity * eDist), 0.0, 1.0);
    else if (uFogMode == 3) { float d = uFogDensity * eDist; vFogFactor = clamp(exp(-d*d), 0.0, 1.0); }
    else                    vFogFactor = 1.0;
}
)GLSL";
// Fragment shader source
static const char* FRAG_SRC = R"GLSL(
#version 330 core
uniform sampler2D uTex0;
uniform sampler2D uTex1;
uniform int   uUseTexture;
uniform int   uUseLightmap;
uniform float uAlphaRef;
uniform vec4  uFogColor;
uniform int   uFogEnable;

in  vec2  vUV0;
in  vec2  vUV1;
in  vec4  vColor;
in  float vFogFactor;
out vec4  oColor;

void main() {
    vec4 texColor = (uUseTexture != 0) ? texture(uTex0, vUV0) : vec4(1.0);
    vec4 c = texColor * vColor;
    if (uUseLightmap != 0) c.rgb *= texture(uTex1, vUV1).rgb;
    if (c.a < uAlphaRef) discard;
    if (uFogEnable != 0) c.rgb = mix(uFogColor.rgb, c.rgb, vFogFactor);
    oColor = c;
}
)GLSL";

// You might wonder why im not putting them in separate files, well thats
// because im fucking lazy thats all :P

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        fprintf(stderr, "[4J_Render] shader error:\n%s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}
static GLuint linkProgram(GLuint v, GLuint f) {
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        fprintf(stderr, "[4J_Render] link error:\n%s\n", log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}
// Shader struct
struct ShaderUniforms {
    GLuint prog = 0;
    GLint uMVP = -1, uMV = -1, uBaseColor = -1;
    GLint uLighting = -1, uLight0Dir = -1, uLight1Dir = -1;
    GLint uLightDiff = -1, uLightAmb = -1;
    GLint uFogMode = -1, uFogStart = -1, uFogEnd = -1;
    GLint uFogDensity = -1, uFogColor = -1, uFogEnable = -1;
    GLint uLMTransform = -1, uUseLightmap = -1, uAlphaRef = -1;
    GLint uTex0 = -1, uTex1 = -1, uGlobalLM = -1;
    GLint uUseTexture = -1;

    void build(const char* vs, const char* fs) {
        GLuint v = compileShader(GL_VERTEX_SHADER, vs);
        GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
        prog = linkProgram(v, f);
        glDeleteShader(v);
        glDeleteShader(f);
        if (!prog) return;
#define L(x) x = glGetUniformLocation(prog, #x)
        L(uMVP);
        L(uMV);
        L(uBaseColor);
        L(uLighting);
        L(uLight0Dir);
        L(uLight1Dir);
        uLightDiff = glGetUniformLocation(prog, "uLightDiffuse");
        uLightAmb = glGetUniformLocation(prog, "uLightAmbient");
        L(uFogMode);
        L(uFogStart);
        L(uFogEnd);
        L(uFogDensity);
        L(uFogColor);
        L(uFogEnable);
        L(uLMTransform);
        L(uUseLightmap);
        L(uAlphaRef);
        L(uTex0);
        L(uTex1);
        L(uGlobalLM);
        L(uUseTexture);
#undef L
        glUseProgram(prog);
        glUniform1i(uTex0, 0);
        glUniform1i(uTex1, 1);
    }
} s_shader;

// Matrix Stacks.
static const int STACK_DEPTH = 64;
struct MatrixStack {
    glm::mat4 stack[STACK_DEPTH];
    int top = 0;
    MatrixStack() { stack[0] = glm::mat4(1.f); }
    glm::mat4& cur() { return stack[top]; }
    void push() {
        if (top < STACK_DEPTH - 1) {
            stack[top + 1] = stack[top];
            ++top;
        }
    }
    void pop() {
        if (top > 0) --top;
    }
    void load(const glm::mat4& m) { cur() = m; }
    void mul(const glm::mat4& m) { cur() = cur() * m; }
};
static thread_local MatrixStack s_proj, s_mv, s_tex[2];
static thread_local int s_matMode = 0;  // 0=MV 1=proj 2=tex0 3=tex1

static MatrixStack& activeStack() {
    switch (s_matMode) {
        case 1:
            return s_proj;
        case 2:
            return s_tex[0];
        case 3:
            return s_tex[1];
    }
    return s_mv;
}
static void flushMatrices() {
    glm::mat4 mvp = s_proj.cur() * s_mv.cur();
    glUniformMatrix4fv(s_shader.uMVP, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniformMatrix4fv(s_shader.uMV, 1, GL_FALSE, glm::value_ptr(s_mv.cur()));
}

//  render state
struct RS {
    glm::vec4 baseColor = {1, 1, 1, 1};
    glm::vec4 fogColor = {0, 0, 0, 1};
    float fogStart = 0, fogEnd = 1000, fogDensity = 0;
    int fogMode = 0;
    bool fogEnable = false;
    float alphaRef = 0.1f;
    bool useTexture = true, useLightmap = false, lighting = false;
    glm::vec3 l0 = {0.173913f, 0.869565f, -0.608696f};
    glm::vec3 l1 = {-0.173913f, 0.869565f, 0.608696f};
    glm::vec3 ldiff = {0.6f, 0.6f, 0.6f}, lamb = {0.4f, 0.4f, 0.4f};
    glm::vec4 lmt = {1, 1, 0, 0};
    glm::vec2 globalLM = {240.f, 240.f};  // fullbright for
    int activeTexture = 0;
};
static thread_local RS s_rs;

static void pushRenderState() {
    if (!s_shader.prog) return;
    glUseProgram(s_shader.prog);
    glUniform4fv(s_shader.uBaseColor, 1, glm::value_ptr(s_rs.baseColor));
    glUniform1i(s_shader.uLighting, s_rs.lighting ? 1 : 0);
    glUniform3fv(s_shader.uLight0Dir, 1, glm::value_ptr(s_rs.l0));
    glUniform3fv(s_shader.uLight1Dir, 1, glm::value_ptr(s_rs.l1));
    glUniform3fv(s_shader.uLightDiff, 1, glm::value_ptr(s_rs.ldiff));
    glUniform3fv(s_shader.uLightAmb, 1, glm::value_ptr(s_rs.lamb));
    glUniform1i(s_shader.uFogMode, s_rs.fogMode);
    glUniform1f(s_shader.uFogStart, s_rs.fogStart);
    glUniform1f(s_shader.uFogEnd, s_rs.fogEnd);
    glUniform1f(s_shader.uFogDensity, s_rs.fogDensity);
    glUniform4fv(s_shader.uFogColor, 1, glm::value_ptr(s_rs.fogColor));
    glUniform1i(s_shader.uFogEnable, s_rs.fogEnable ? 1 : 0);
    glUniform1i(s_shader.uUseTexture, s_rs.useTexture ? 1 : 0);
    glUniform1i(s_shader.uUseLightmap, s_rs.useLightmap ? 1 : 0);
    glUniform1f(s_shader.uAlphaRef, s_rs.alphaRef);
    glUniform4fv(s_shader.uLMTransform, 1, glm::value_ptr(s_rs.lmt));
    glUniform2fv(s_shader.uGlobalLM, 1, glm::value_ptr(s_rs.globalLM));
    flushMatrices();
}

//  quad IBO
static const int MAX_QUADS = 16384;
static GLuint s_quadIBO = 0;

static void buildQuadIBO() {
    std::vector<GLushort> idx;
    idx.reserve(MAX_QUADS * 6);
    for (int q = 0; q < MAX_QUADS; q++) {
        GLushort b = (GLushort)(q * 4);
        idx.push_back(b);
        idx.push_back(b + 1);
        idx.push_back(b + 2);
        idx.push_back(b);
        idx.push_back(b + 2);
        idx.push_back(b + 3);
    }
    glGenBuffers(1, &s_quadIBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_quadIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)(idx.size() * sizeof(GLushort)), idx.data(),
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

//  streaming VAOs
static GLuint s_sVAO_std = 0, s_sVBO_std = 0;
static GLuint s_sVAO_cmp = 0, s_sVBO_cmp = 0;

static void bindStdAttribs() {
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, (void*)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 32, (void*)12);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, 32, (void*)20);
    glVertexAttribPointer(3, 3, GL_BYTE, GL_TRUE, 32, (void*)24);
    glVertexAttribIPointer(4, 2, GL_SHORT, 32, (void*)28);
}
static void bindCmpAttribs() {
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, 16, (void*)0);
    glVertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, 16, (void*)8);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, 16, (void*)6);
    glVertexAttribPointer(3, 3, GL_BYTE, GL_TRUE, 16, (void*)6);
    glVertexAttribIPointer(4, 2, GL_SHORT, 16, (void*)12);
}
static void initStreamingVAOs() {
    glGenVertexArrays(1, &s_sVAO_std);
    glGenBuffers(1, &s_sVBO_std);
    glBindVertexArray(s_sVAO_std);
    glBindBuffer(GL_ARRAY_BUFFER, s_sVBO_std);
    bindStdAttribs();
    glBindVertexArray(0);

    glGenVertexArrays(1, &s_sVAO_cmp);
    glGenBuffers(1, &s_sVBO_cmp);
    glBindVertexArray(s_sVAO_cmp);
    glBindBuffer(GL_ARRAY_BUFFER, s_sVBO_cmp);
    bindCmpAttribs();
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0); 
}

//  chunk buffer pool (shared, protected by s_glCallMtx)
struct ChunkDrawCall {
    GLenum prim;
    GLint first;
    GLsizei count;
    bool wasQuad;
};
struct ChunkBuffer {
    GLuint vbo = 0;
    std::vector<ChunkDrawCall> draws;
    std::vector<uint8_t> rawVerts;
    bool compact = false;
    bool valid = false;
    bool vboReady = false;
    void destroy() {
        if (vbo) {
            glDeleteBuffers(1, &vbo);
            vbo = 0;
        }
        draws.clear();
        rawVerts.clear();
        valid = false;
        vboReady = false;
        compact = false;
    }
};
static std::unordered_map<int, ChunkBuffer> s_chunkPool;
static int s_nextListBase = 1;

//  per-thread recording state
static thread_local int s_recListId = -1;
static thread_local std::vector<uint8_t> s_recVerts;
static thread_local std::vector<ChunkDrawCall> s_recDraws;
static thread_local bool s_recCompact = false;

//  primitive helpers
static bool isQuadPrim(int pt) {
    return (pt == GL_QUADS || pt == (int)C4JRender::PRIMITIVE_TYPE_QUAD_LIST);
}
static GLenum mapPrim(int pt) {
    if (isQuadPrim(pt)) return GL_TRIANGLES;
    if (pt == GL_TRIANGLES) return GL_TRIANGLES;
    if (pt == GL_LINES) return GL_LINES;
    if (pt == GL_LINE_STRIP) return GL_LINE_STRIP;
    if (pt == GL_TRIANGLE_STRIP) return GL_TRIANGLE_STRIP;
    if (pt == GL_TRIANGLE_FAN) return GL_TRIANGLE_FAN;
    switch (pt) {
        case C4JRender::PRIMITIVE_TYPE_TRIANGLE_LIST:
            return GL_TRIANGLES;
        case C4JRender::PRIMITIVE_TYPE_TRIANGLE_STRIP:
            return GL_TRIANGLE_STRIP;
        case C4JRender::PRIMITIVE_TYPE_TRIANGLE_FAN:
            return GL_TRIANGLE_FAN;
        case C4JRender::PRIMITIVE_TYPE_LINE_LIST:
            return GL_LINES;
        case C4JRender::PRIMITIVE_TYPE_LINE_STRIP:
            return GL_LINE_STRIP;
        default:
            return GL_TRIANGLES;
    }
}
static void drawPrim(GLenum glMode, bool wasQuad, GLint first,
                     GLsizei vertCount) {
    if (wasQuad) {
        GLsizei indexCount = (vertCount / 4) * 6;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_quadIBO);
        glDrawElementsBaseVertex(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT,
                                 nullptr, first);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    } else {
        glDrawArrays(glMode, first, vertCount);
    }
}
//  Initialises the renderer
void C4JRender::Initialise() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "[4J_Render] SDL_Init: %s\n", SDL_GetError());
        return;
    }
    SDL_DisplayMode dm;
    if (s_reqWidth > 0 && s_reqHeight > 0) {
        s_windowWidth = s_reqWidth;
        s_windowHeight = s_reqHeight;
    } else if (SDL_GetCurrentDisplayMode(0, &dm) == 0) {
        s_windowWidth = (int)(dm.w * 0.4f);
        s_windowHeight = (int)(dm.h * 0.4f);
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);

    // Well i changed my mind, i DID rewrite the whole renderer.
    // atleast part of it.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE); // I am sorry for everything that i shall do in the next commits.
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    Uint32 wf = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    if (s_fullscreen) wf |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    s_window = SDL_CreateWindow("Minecraft Console Edition",
                                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                s_windowWidth, s_windowHeight, wf);
    if (!s_window) {
        fprintf(stderr, "[4J_Render] Window: %s\n", SDL_GetError());
        return;
    }
    s_glContext = SDL_GL_CreateContext(s_window);
    if (!s_glContext) {
        fprintf(stderr, "[4J_Render] Context: %s\n", SDL_GetError());
        return;
    }
    gl3_load();
#ifdef ENABLE_VSYNC
    SDL_GL_SetSwapInterval(1);
#else
    SDL_GL_SetSwapInterval(0);
#endif
    int fw, fh;
    SDL_GetWindowSize(s_window, &fw, &fh);
    onFramebufferResize(fw, fh);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearDepth(1.0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClearColor(0, 0, 0, 1);
    glViewport(0, 0, s_windowWidth, s_windowHeight);
    s_shader.build(VERT_SRC, FRAG_SRC);
    buildQuadIBO();
    initStreamingVAOs();
    pthread_once(&s_glCtxKeyOnce, makeGLCtxKey);
    s_mainThread = pthread_self();
    s_mainThreadSet = true;
    pthread_setspecific(s_glCtxKey, (void*)s_window);
    SDL_GL_MakeCurrent(s_window, s_glContext);
    for (int i = 0; i < MAX_SHARED_CTXS; i++) {
        SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
        SDL_Window* w = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED,
                                         SDL_WINDOWPOS_UNDEFINED, 1, 1,
                                         SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
        if (!w) break;
        SDL_GLContext ctx = SDL_GL_CreateContext(w);
        if (!ctx) {
            SDL_DestroyWindow(w);
            break;
        }
        s_sharedWins[s_sharedCtxCount] = w;
        s_sharedCtxs[s_sharedCtxCount] = ctx;
        s_sharedCtxCount++;
    }
    SDL_GL_MakeCurrent(s_window, s_glContext);
    pushRenderState();
}

void C4JRender::InitialiseContext() {
    if (!s_window) return;
    pthread_once(&s_glCtxKeyOnce, makeGLCtxKey);
    if (s_mainThreadSet && pthread_equal(pthread_self(), s_mainThread)) {
        SDL_GL_MakeCurrent(s_window, s_glContext);
        pthread_setspecific(s_glCtxKey, (void*)s_window);
        return;
    }
    void* cp = pthread_getspecific(s_glCtxKey);
    if (cp) {
        SDL_GLContext ctx = (SDL_GLContext)cp;
        for (int i = 0; i < s_sharedCtxCount; i++)
            if (s_sharedCtxs[i] == ctx) {
                SDL_GL_MakeCurrent(s_sharedWins[i], ctx);
                return;
            }
        return;
    }
    pthread_mutex_lock(&s_sharedMtx);
    SDL_GLContext shared = (s_nextSharedCtx < s_sharedCtxCount)
                               ? s_sharedCtxs[s_nextSharedCtx++]
                               : nullptr;
    pthread_mutex_unlock(&s_sharedMtx);
    if (!shared) return;
    for (int i = 0; i < s_sharedCtxCount; i++)
        if (s_sharedCtxs[i] == shared)
            SDL_GL_MakeCurrent(s_sharedWins[i], shared);
    pthread_setspecific(s_glCtxKey, (void*)shared);
}

void C4JRender::StartFrame() {
    int w, h;
    SDL_GetWindowSize(s_window, &w, &h);
    s_windowWidth = w > 0 ? w : 1;
    s_windowHeight = h > 0 ? h : 1;
    glViewport(0, 0, s_windowWidth, s_windowHeight);
}

void C4JRender::Present() {
    if (!s_window) return;
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT)
            s_shouldClose = true;
        else if (ev.type == SDL_WINDOWEVENT) {
            if (ev.window.event == SDL_WINDOWEVENT_CLOSE)
                s_shouldClose = true;
            else if (ev.window.event == SDL_WINDOWEVENT_RESIZED)
                onFramebufferResize(ev.window.data1, ev.window.data2);
        }
    }
    glFlush();
    SDL_GL_SwapWindow(s_window);
}

void C4JRender::SetWindowSize(int w, int h) {
    s_reqWidth = w;
    s_reqHeight = h;
}
void C4JRender::SetFullscreen(bool fs) { s_fullscreen = fs; }
bool C4JRender::ShouldClose() { return !s_window || s_shouldClose; }
void C4JRender::GetFramebufferSize(int& w, int& h) {
    w = s_windowWidth;
    h = s_windowHeight;
}

void C4JRender::Close() { s_window = nullptr; }

void C4JRender::Shutdown() {
    pthread_mutex_lock(&s_glCallMtx);
    for (auto& kv : s_chunkPool) kv.second.destroy();
    s_chunkPool.clear();
    pthread_mutex_unlock(&s_glCallMtx);
    glDeleteVertexArrays(1, &s_sVAO_std);
    glDeleteBuffers(1, &s_sVBO_std);
    glDeleteVertexArrays(1, &s_sVAO_cmp);
    glDeleteBuffers(1, &s_sVBO_cmp);
    if (s_quadIBO) glDeleteBuffers(1, &s_quadIBO);
    if (s_shader.prog) glDeleteProgram(s_shader.prog);
    if (s_glContext) {
        SDL_GL_DeleteContext(s_glContext);
        s_glContext = nullptr;
    }
    if (s_window) {
        SDL_DestroyWindow(s_window);
        s_window = nullptr;
    }
    for (int i = 0; i < s_sharedCtxCount; i++) {
        if (s_sharedCtxs[i]) SDL_GL_DeleteContext(s_sharedCtxs[i]);
        if (s_sharedWins[i]) SDL_DestroyWindow(s_sharedWins[i]);
    }
    SDL_Quit();
}

void C4JRender::DrawVertices(ePrimitiveType ptype, int count, void* dataIn,
                             eVertexType vType, ePixelShaderType) {
    if (count <= 0 || !dataIn) return;
    const bool compact = (vType == VERTEX_TYPE_COMPRESSED);
    const bool wasQuad = isQuadPrim((int)ptype);
    GLenum glMode = mapPrim((int)ptype);
    size_t stride = compact ? 16 : 32;
    size_t bytes = (size_t)count * stride;
    if (s_recListId >= 0) {
        int first = (int)(s_recVerts.size() / stride);
        s_recVerts.insert(s_recVerts.end(), (uint8_t*)dataIn,
                          (uint8_t*)dataIn + bytes);
        s_recDraws.push_back({glMode, first, (GLsizei)count, wasQuad});
        return;
    }
    pthread_mutex_lock(&s_glCallMtx);
    pushRenderState();
    GLuint vao = compact ? s_sVAO_cmp : s_sVAO_std;
    GLuint vbo = compact ? s_sVBO_cmp : s_sVBO_std;
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)bytes, dataIn, GL_STREAM_DRAW);
    if (compact)
        bindCmpAttribs();
    else
        bindStdAttribs();
    drawPrim(glMode, wasQuad, 0, (GLsizei)count);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0); 
    pthread_mutex_unlock(&s_glCallMtx);
}

void C4JRender::CBuffLockStaticCreations() {}
int C4JRender::CBuffCreate(int count) {
    pthread_mutex_lock(&s_glCallMtx);
    int b = s_nextListBase;
    s_nextListBase += count;
    pthread_mutex_unlock(&s_glCallMtx);
    return b;
}
void C4JRender::CBuffDelete(int first, int count) {
    pthread_mutex_lock(&s_glCallMtx);
    for (int i = first; i < first + count; i++) {
        auto it = s_chunkPool.find(i);
        if (it != s_chunkPool.end()) {
            it->second.destroy();
            s_chunkPool.erase(it);
        }
    }
    pthread_mutex_unlock(&s_glCallMtx);
}
void C4JRender::CBuffStart(int index, bool) {
    s_recListId = index;
    s_recVerts.clear();
    s_recDraws.clear();
    s_recCompact = false;
}
void C4JRender::CBuffEnd() {
    if (s_recListId < 0) return;
    pthread_mutex_lock(&s_glCallMtx);
    ChunkBuffer& cb = s_chunkPool[s_recListId];
    cb.destroy();
    if (s_recVerts.empty()) {
        s_chunkPool.erase(s_recListId);
        pthread_mutex_unlock(&s_glCallMtx);
        s_recListId = -1;
        return;
    }
    cb.rawVerts = std::move(s_recVerts);
    cb.draws = std::move(s_recDraws);
    cb.compact = s_recCompact;
    cb.valid = true;
    cb.vboReady = false;
    pthread_mutex_unlock(&s_glCallMtx);
    s_recListId = -1;
}
void C4JRender::CBuffClear(int index) {
    pthread_mutex_lock(&s_glCallMtx);
    auto it = s_chunkPool.find(index);
    if (it != s_chunkPool.end()) it->second.destroy();
    pthread_mutex_unlock(&s_glCallMtx);
}
bool C4JRender::CBuffCall(int index, bool) {
    pthread_mutex_lock(&s_glCallMtx);
    auto it = s_chunkPool.find(index);
    if (it == s_chunkPool.end() || !it->second.valid) {
        pthread_mutex_unlock(&s_glCallMtx);
        return false;
    }
    ChunkBuffer& cb = it->second;
    if (!cb.vboReady) {
        if (cb.rawVerts.empty()) {
            pthread_mutex_unlock(&s_glCallMtx);
            return false;
        }
        glGenBuffers(1, &cb.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, cb.vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)cb.rawVerts.size(),
                     cb.rawVerts.data(), GL_STATIC_DRAW);
        cb.rawVerts.clear();
        cb.rawVerts.shrink_to_fit();
        cb.vboReady = true;
    }
    pushRenderState();
    GLuint vao = cb.compact ? s_sVAO_cmp : s_sVAO_std;
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, cb.vbo);
    if (cb.compact)
        bindCmpAttribs();
    else
        bindStdAttribs();
    for (const auto& dc : cb.draws)
        drawPrim(dc.prim, dc.wasQuad, dc.first, dc.count);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0); 
    pthread_mutex_unlock(&s_glCallMtx);
    return true;
}
int C4JRender::CBuffSize(int) { return 0; }
void C4JRender::CBuffTick() {}
void C4JRender::CBuffDeferredModeStart() {}
void C4JRender::CBuffDeferredModeEnd() {}

void C4JRender::MatrixMode(int t) {
    if (t == GL_PROJECTION)
        s_matMode = 1;
    else if (t == GL_TEXTURE)
        s_matMode = 2;
    else
        s_matMode = 0;
}
void C4JRender::MatrixSetIdentity() { activeStack().load(glm::mat4(1.f)); }
void C4JRender::MatrixPush() { activeStack().push(); }
void C4JRender::MatrixPop() { activeStack().pop(); }
void C4JRender::MatrixTranslate(float x, float y, float z) {
    activeStack().mul(glm::translate(glm::mat4(1.f), {x, y, z}));
}
void C4JRender::MatrixRotate(float a, float x, float y, float z) {
    activeStack().mul(glm::rotate(glm::mat4(1.f), a, {x, y, z}));
}
void C4JRender::MatrixScale(float x, float y, float z) {
    activeStack().mul(glm::scale(glm::mat4(1.f), {x, y, z}));
}
void C4JRender::MatrixPerspective(float fovy, float asp, float zn, float zf) {
    s_proj.cur() = glm::perspective(glm::radians(fovy), asp, zn, zf);
}
void C4JRender::MatrixOrthogonal(float l, float r, float b, float t, float zn,
                                 float zf) {
    s_proj.cur() = glm::ortho(l, r, b, t, zn, zf);
}
void C4JRender::MatrixMult(float* m) { activeStack().mul(glm::make_mat4(m)); }
const float* C4JRender::MatrixGet(int t) {
    static float buf[16];
    glm::mat4* m = (t == GL_MODELVIEW_MATRIX)
                       ? &s_mv.cur()
                       : (t == GL_PROJECTION_MATRIX ? &s_proj.cur() : nullptr);
    if (m) memcpy(buf, glm::value_ptr(*m), 64);
    return buf;
}
void C4JRender::Set_matrixDirty() {}

void C4JRender::Clear(int f) { glClear(f); }
void C4JRender::SetClearColour(const float c[4]) {
    glClearColor(c[0], c[1], c[2], c[3]);
}
bool C4JRender::IsWidescreen() { return true; }
bool C4JRender::IsHiDef() { return true; }
void C4JRender::StateSetColour(float r, float g, float b, float a) {
    s_rs.baseColor = {r, g, b, a};
}
void C4JRender::StateSetDepthMask(bool e) {
    glDepthMask(e ? GL_TRUE : GL_FALSE);
}
void C4JRender::StateSetTextureEnable(bool e) {
    if (s_rs.activeTexture == 0) {
        s_rs.useTexture = e;
        if (s_shader.prog) {
            glUseProgram(s_shader.prog);
            glUniform1i(s_shader.uUseTexture, e ? 1 : 0);
        }
    } else {
        s_rs.useLightmap = e;
        if (s_shader.prog) {
            glUseProgram(s_shader.prog);
            glUniform1i(s_shader.uUseLightmap, e ? 1 : 0);
        }
    }
}
void C4JRender::StateSetActiveTexture(int tex) {
    s_rs.activeTexture = (tex == 0x84C1 /*GL_TEXTURE1*/) ? 1 : 0;
}
void C4JRender::StateSetBlendEnable(bool e) {
    if (e)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
}
void C4JRender::StateSetBlendFunc(int s, int d) { glBlendFunc(s, d); }
void C4JRender::StateSetDepthFunc(int f) { glDepthFunc(f); }
void C4JRender::StateSetFaceCull(bool e) {
    if (e)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
}
void C4JRender::StateSetFaceCullCW(bool e) { glFrontFace(e ? GL_CW : GL_CCW); }
void C4JRender::StateSetLineWidth(float w) { glLineWidth(w); }
void C4JRender::StateSetWriteEnable(bool r, bool g, bool b, bool a) {
    glColorMask(r, g, b, a);
}
void C4JRender::StateSetDepthTestEnable(bool e) {
    if (e)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
}
void C4JRender::StateSetAlphaTestEnable(bool e) {
    s_rs.alphaRef = e ? 0.1f : 0.f;
}
void C4JRender::StateSetAlphaFunc(int, float p) { s_rs.alphaRef = p; }
void C4JRender::StateSetDepthSlopeAndBias(float s, float b) {
    if (s || b) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(s, b);
    } else
        glDisable(GL_POLYGON_OFFSET_FILL);
}
void C4JRender::StateSetBlendFactor(unsigned int col) {
    float a = ((col >> 24) & 0xFF) / 255.f, r = ((col >> 16) & 0xFF) / 255.f,
          g = ((col >> 8) & 0xFF) / 255.f, b = (col & 0xFF) / 255.f;
    glBlendColor(r, g, b, a);
}
void C4JRender::StateSetFogEnable(bool e) { s_rs.fogEnable = e; }
void C4JRender::StateSetFogMode(int mode) {
    s_rs.fogMode = (mode == GL_LINEAR) ? 1
                   : (mode == GL_EXP)  ? 2
                   : (mode == 0x0801)  ? 3
                                       : 0;
}
void C4JRender::StateSetFogNearDistance(float d) { s_rs.fogStart = d; }
void C4JRender::StateSetFogFarDistance(float d) { s_rs.fogEnd = d; }
void C4JRender::StateSetFogDensity(float d) { s_rs.fogDensity = d; }
void C4JRender::StateSetFogColour(float r, float g, float b) {
    s_rs.fogColor = {r, g, b, 1};
}
void C4JRender::StateSetLightingEnable(bool e) { s_rs.lighting = e; }
void C4JRender::StateSetLightColour(int, float r, float g, float b) {
    s_rs.ldiff = {r, g, b};
}
void C4JRender::StateSetLightAmbientColour(float r, float g, float b) {
    s_rs.lamb = {r, g, b};
}
void C4JRender::StateSetLightDirection(int light, float x, float y, float z) {
    glm::vec3 d = glm::normalize(glm::vec3(x, y, z));
    // so i did check, java does NOT reverse, this is CORRECT.
    if (light == 0)
        s_rs.l0 = d;
    else
        s_rs.l1 = d;
}
void C4JRender::StateSetLightEnable(int light, bool enable) {}
void C4JRender::StateSetViewport(eViewportType) {
    glViewport(0, 0, s_windowWidth, s_windowHeight);
}
void C4JRender::StateSetEnableViewportClipPlanes(bool e) {}
void C4JRender::StateSetVertexTextureUV(float u, float v) {
    s_rs.globalLM = {u, v};
}
void C4JRender::StateSetForceLOD(int) {}
void C4JRender::StateSetTexGenCol(int, float, float, float, float, bool) {}
void C4JRender::StateSetStencil(int fn, uint8_t ref, uint8_t fmask,
                                uint8_t wmask) {
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(fn, ref, fmask);
    glStencilMask(wmask);
}

int C4JRender::TextureCreate() {
    GLuint id;
    glGenTextures(1, &id);
    return (int)id;
}
void C4JRender::TextureFree(int i) {
    GLuint id = (GLuint)i;
    glDeleteTextures(1, &id);
}
void C4JRender::TextureBind(int idx) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, idx < 0 ? 0 : (GLuint)idx);
}
void C4JRender::TextureBindVertex(int idx, bool scaleLight) {
    if (idx < 0) {
        s_rs.useLightmap = false;
        glActiveTexture(GL_TEXTURE0);
        return;
    }
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, (GLuint)idx);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glActiveTexture(GL_TEXTURE0);
    s_rs.useLightmap = true;
    s_rs.lmt = scaleLight ? glm::vec4{1.f, 1.f, 8.f / 256.f, 8.f / 256.f}
                          : glm::vec4{1.f, 1.f, 0.f, 0.f};
}
void C4JRender::TextureSetTextureLevels(int l) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, l > 0 ? l - 1 : 0);

    if (l > 1) {
        // Listen i tried ansio but it looked worse cuz there was this HORRIBLE black line artifact on mip levels,
        // so here we are with this weird combo of filters that seems to work ok
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        GL_NEAREST_MIPMAP_LINEAR);
    } else {
        // If there is no mipmaps, just use nearest.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }
}

int C4JRender::TextureGetTextureLevels() { return 1; }

void C4JRender::TextureData(int w, int h, void* d, int lvl, eTextureFormat) {
    glTexImage2D(GL_TEXTURE_2D, lvl, GL_RGBA, w, h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, d);

    if (lvl == 0) {
        // Same as above
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        GLint maxLvl = 0;
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, &maxLvl);

        if (maxLvl == 0) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        }
    }
}
void C4JRender::TextureDataUpdate(int xo, int yo, int w, int h, void* d,
                                  int lvl) {
    glTexSubImage2D(GL_TEXTURE_2D, lvl, xo, yo, w, h, GL_RGBA, GL_UNSIGNED_BYTE,
                    d);
}
void C4JRender::TextureSetParam(int p, int v) {
    glTexParameteri(GL_TEXTURE_2D, p, v);
}
void C4JRender::TextureDynamicUpdateStart() {}
void C4JRender::TextureDynamicUpdateEnd() {}
void C4JRender::TextureGetStats() {}
void* C4JRender::TextureGetTexture(int) { return nullptr; }

static HRESULT stbLoad(unsigned char* data, int w, int h, D3DXIMAGE_INFO* info,
                       int** out) {
    int* px = new int[w * h];
    for (int i = 0; i < w * h; i++) {
        unsigned char r = data[i * 4], g = data[i * 4 + 1], b = data[i * 4 + 2],
                      a = data[i * 4 + 3];
        px[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
    if (info) {
        info->Width = w;
        info->Height = h;
    }
    *out = px;
    return S_OK;
}
HRESULT C4JRender::LoadTextureData(const char* fn, D3DXIMAGE_INFO* i, int** o) {
    int w, h, c;
    unsigned char* d = stbi_load(fn, &w, &h, &c, 4);
    if (!d) return E_FAIL;
    HRESULT hr = stbLoad(d, w, h, i, o);
    stbi_image_free(d);
    return hr;
}
HRESULT C4JRender::LoadTextureData(uint8_t* pb, uint32_t nb, D3DXIMAGE_INFO* i,
                                   int** o) {
    int w, h, c;
    unsigned char* d = stbi_load_from_memory(pb, (int)nb, &w, &h, &c, 4);
    if (!d) return E_FAIL;
    HRESULT hr = stbLoad(d, w, h, i, o);
    stbi_image_free(d);
    return hr;
}
HRESULT C4JRender::SaveTextureData(const char*, D3DXIMAGE_INFO*, int*) {
    return S_OK;
}
HRESULT C4JRender::SaveTextureDataToMemory(void*, int, int*, int, int, int*) {
    return S_OK;
}

void C4JRender::DoScreenGrabOnNextPresent() {}
void C4JRender::CaptureThumbnail(ImageFileBuffer*) {}
void C4JRender::CaptureScreen(ImageFileBuffer*, XSOCIAL_PREVIEWIMAGE*) {}
void C4JRender::BeginConditionalSurvey(int) {}
void C4JRender::EndConditionalSurvey() {}
void C4JRender::BeginConditionalRendering(int) {}
void C4JRender::EndConditionalRendering() {}
void C4JRender::Tick() {}
void C4JRender::UpdateGamma(unsigned short) {}
void C4JRender::BeginEvent(LPCWSTR) {}
void C4JRender::EndEvent() {}
void C4JRender::Suspend() {}
bool C4JRender::Suspended() { return false; }
void C4JRender::Resume() {}