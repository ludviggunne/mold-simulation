
#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.1415f
#endif

#define TEXTURE_WIDTH   1200
#define TEXTURE_HEIGHT  900
#define PATCH_SIZE_2D   256
#define PATCH_SIZE_1D   (32* 4096)
#define AGENT_COUNT     (4 * 65536)
#define AGENT_SPEED     1

#define LOCAL_INVOCATION_SIZE_2D      32
#define LOCAL_INVOCATION_SIZE_1D      1024
#define TEXTURE_INT_BINDING           0
#define UNIFORM_BUFFER_BINDING        1
#define SHADER_STORAGE_BUFFER_BINDING 2


/* Open Gl error handling */
#if 0
# define GL_CALL(expression) expression; assert(logGLErrors(__FILE__, __LINE__, #expression))
#else
# define GL_CALL(expression) expression;
#endif
int logGLErrors(const char *file, uint32_t line, const char *expression) {

    GLenum error;
    int ok = 1;

    while ((error = glGetError()) != GL_NO_ERROR) {

        printf("[OpenGL error] (0x%04x): %s:%d: %s\n", error, file, line, expression);
        ok = 0;
    }

    return ok;
}




/* Open GL data & handles */
struct {

    GLint id;
} postProcPrgm;

const char *postProcPrgm_srcPath = "shaders/post_proc.comp.glsl"; 


struct {

    GLint id;
} computePrgm;

const char *computePrgm_srcPath = "shaders/compute.comp.glsl";


struct {

    GLint id;
} diffusePrgm;

const char *diffusePrgm_srcPath = "shaders/diffuse.comp.glsl";


struct {

    GLint id;

    struct {

        GLint u_Sampler;
    } uLoc;
} renderPrgm;

const char *renderPrgm_vSrcPath = "shaders/render.vert.glsl";
const char *renderPrgm_fSrcPath = "shaders/render.frag.glsl";


struct {

    GLuint vArr;
    union {
        struct {

            GLuint vBuf;
            GLuint ibuf;
        };
        GLuint bufs[2];
    };
} quad;

const float quad_vData[16] = {

    -1.0f, -1.0f,     0.0f, 0.0f,
     1.0f, -1.0f,     1.0f, 0.0f,
    -1.0f,  1.0f,     0.0f, 1.0f,
     1.0f,  1.0f,     1.0f, 1.0f
};

const uint32_t quad_iData[6] = {
    0, 1, 2,
    1, 2, 3
};


struct {

    GLuint id;
} textureInternal;

clock_t t0;


/* Contains data shared between shaders */
struct {

    struct {

        /* Manually align according to std140 layout */
        uint32_t                  texture_width  __attribute__((aligned(4)));
        uint32_t                  texture_height __attribute__((aligned(4)));
        uint32_t                  patch_size     __attribute__((aligned(4)));
        struct { uint32_t u, v; } patch_offset   __attribute__((aligned(8)));
        uint32_t                  agent_count    __attribute__((aligned(4)));  
        float                     delta_time     __attribute__((aligned(4)));

    } data __attribute__((aligned(16)));

    GLuint id;

} uniformBuffer;

__attribute__((aligned(16))) struct agent {

    struct { float x, y; }    position  __attribute__((aligned(8)));
    float                     angle     __attribute__((aligned(4)));
    struct { float r, g, b; } signature __attribute__((aligned(16)));
};

struct {

    struct agent data[AGENT_COUNT];
    GLuint id;
} shaderStorageBuffer;



/* Forward declarations */
/* Host */
char *loadFile(const char *file_name);
void checkCompileErrors(const char *tag, GLint shader);
GLint createShader(GLenum type, const char *src);
GLint attachAndLinkPrgm(uint32_t count, GLint *shaders);
void setupPrograms();
void setupUniformBuffer();
void updateUniformBuffer();
void setupShaderStorageBuffer();
void setupTextures();
void setupQuad();
void drawQuad();
void dispatchTextureSpace(GLint prgm_id);
void dispatchAgentSpace(GLint prgm_id);
void calculateDeltaTime();
void dumpInfo();




int main() {

    glfwInit();

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow *window = glfwCreateWindow(TEXTURE_WIDTH, TEXTURE_HEIGHT, "MOLD SIMULATION", NULL, NULL);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);    

    glfwSwapInterval(0);
    glfwMakeContextCurrent(window);    
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    setupPrograms();
    setupUniformBuffer();
    setupTextures();
    setupQuad();
    setupShaderStorageBuffer();

    dumpInfo();

    while (!glfwWindowShouldClose(window)) {

        GL_CALL( glClearColor(1.0f, 0.0f, 0.0f, 1.0f) );
        GL_CALL( glClear(GL_COLOR_BUFFER_BIT) );

        dispatchTextureSpace(diffusePrgm.id);
        dispatchAgentSpace(computePrgm.id);

        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) dispatchTextureSpace(postProcPrgm.id);

        drawQuad();

        glfwSwapBuffers(window);
        glfwPollEvents();

        calculateDeltaTime();
    }

    glfwTerminate();
}


char *loadFile(const char *file_name)
{

    /* TODO: Error handling */
    FILE *f = fopen(file_name, "rb");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET); 

    char *string = malloc(fsize + 1);
    fread(string, fsize, 1, f);
    fclose(f);

    string[fsize] = 0;

    return string;
}

GLint createShader(GLenum type, const char *src)
{

    GLint shader;

    GL_CALL( shader = glCreateShader(type) );
    GL_CALL( glShaderSource(shader, 1, &src, NULL) );
    GL_CALL( glCompileShader(shader) );

    return shader;
}

void checkCompileErrors(const char *tag, GLint shader)
{

    char log[512];
    GLint status;

    GL_CALL( glGetShaderiv(shader, GL_COMPILE_STATUS, &status) );
    if (status == GL_FALSE) {

        GL_CALL( glGetShaderInfoLog(shader, 512, NULL, log) );
        printf("Compile error (tag=%s):\n%s\n\n", tag, log);
    }
}

void checkLinkErrors(const char *tag, GLint prgm)
{

    char log[512];
    GLint status;

    GL_CALL( glGetProgramiv(prgm, GL_LINK_STATUS, &status) );
    if (status == GL_FALSE) {

        GL_CALL( glGetProgramInfoLog(prgm, 512, NULL, log) );
        printf("Linking error (tag=%s):\n%s\n\n", tag, log);
    }
}

void setupPrograms()
{


    char *postProcPrgm_src = loadFile(postProcPrgm_srcPath);
    GLint postProcPrgm_shader = createShader(GL_COMPUTE_SHADER, postProcPrgm_src);
    free(postProcPrgm_src);
    checkCompileErrors("Post Processing", postProcPrgm_shader);
    GL_CALL( postProcPrgm.id = glCreateProgram() );
    GL_CALL( glAttachShader(postProcPrgm.id, postProcPrgm_shader) );
    GL_CALL( glLinkProgram(postProcPrgm.id) );
    GL_CALL( glDeleteShader(postProcPrgm_shader) );
    checkLinkErrors("Post Processing", postProcPrgm.id);


    char *computePrgm_src = loadFile(computePrgm_srcPath);
    GLint computePrgm_shader = createShader(GL_COMPUTE_SHADER, computePrgm_src);
    free(computePrgm_src);
    checkCompileErrors("Compute", computePrgm_shader);
    GL_CALL( computePrgm.id = glCreateProgram() );
    GL_CALL( glAttachShader(computePrgm.id, computePrgm_shader) );
    GL_CALL( glLinkProgram(computePrgm.id) );
    GL_CALL( glDeleteShader(computePrgm_shader) );
    checkLinkErrors("Compute", computePrgm.id);


    char *diffusePrgm_src = loadFile(diffusePrgm_srcPath);
    GLint diffusePrgm_shader = createShader(GL_COMPUTE_SHADER, diffusePrgm_src);
    free(diffusePrgm_src);
    checkCompileErrors("Diffuse", diffusePrgm_shader);
    GL_CALL( diffusePrgm.id = glCreateProgram() );
    GL_CALL( glAttachShader(diffusePrgm.id, diffusePrgm_shader) );
    GL_CALL( glLinkProgram(diffusePrgm.id) );
    GL_CALL( glDeleteShader(diffusePrgm_shader) );
    checkLinkErrors("Diffuse", diffusePrgm.id);


    char *renderPrgm_vSrc = loadFile(renderPrgm_vSrcPath);
    char *renderPrgm_fSrc = loadFile(renderPrgm_fSrcPath);
    GLint renderPrgm_vShader = createShader(GL_VERTEX_SHADER, renderPrgm_vSrc);
    GLint renderPrgm_fShader = createShader(GL_FRAGMENT_SHADER, renderPrgm_fSrc);
    free(renderPrgm_vSrc);
    free(renderPrgm_fSrc);
    checkCompileErrors("Render: Vertex Shader", renderPrgm_vShader);
    checkCompileErrors("Render: Fragment Shader", renderPrgm_fShader);
    GL_CALL( renderPrgm.id = glCreateProgram() );
    GL_CALL( glAttachShader(renderPrgm.id, renderPrgm_vShader) );
    GL_CALL( glAttachShader(renderPrgm.id, renderPrgm_fShader) );
    GL_CALL( glLinkProgram(renderPrgm.id) );
    GL_CALL( glDeleteShader(renderPrgm_vShader) );
    GL_CALL( glDeleteShader(renderPrgm_fShader) );
    checkLinkErrors("Render", renderPrgm.id);
    GL_CALL( renderPrgm.uLoc.u_Sampler = glGetUniformLocation(renderPrgm.id, "u_Sampler") ); 
    assert( renderPrgm.uLoc.u_Sampler != -1 );
    GL_CALL( glUseProgram(renderPrgm.id) );
    GL_CALL( glUniform1i(renderPrgm.uLoc.u_Sampler, 0) );

}

void setupQuad()
{

    GL_CALL( glGenVertexArrays(1, &quad.vArr) );
    GL_CALL( glBindVertexArray(quad.vArr) );
    
    GL_CALL( glGenBuffers(2, quad.bufs) );
    GL_CALL( glBindBuffer(GL_ARRAY_BUFFER, quad.vBuf) );
    GL_CALL( glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad.ibuf) );

    GL_CALL( glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vData), quad_vData, GL_STATIC_DRAW) );
    GL_CALL( glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quad_iData), quad_iData, GL_STATIC_DRAW) );

    GL_CALL( glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const void*)0) );
    GL_CALL( glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const void*)(2 * sizeof(float))) );
    GL_CALL( glEnableVertexAttribArray(0) );
    GL_CALL( glEnableVertexAttribArray(1) );

}

void setupTextures()
{

    GL_CALL( glGenTextures(1, &textureInternal.id) );
    GL_CALL( glBindTexture(GL_TEXTURE_2D, textureInternal.id) );

    GL_CALL( glTextureParameteri(textureInternal.id, GL_TEXTURE_WRAP_S, GL_REPEAT) );
    GL_CALL( glTextureParameteri(textureInternal.id, GL_TEXTURE_WRAP_T, GL_REPEAT) );
    GL_CALL( glTextureParameteri(textureInternal.id, GL_TEXTURE_MAG_FILTER, GL_NEAREST) );
    GL_CALL( glTextureParameteri(textureInternal.id, GL_TEXTURE_MIN_FILTER, GL_NEAREST) );

    GL_CALL( glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, uniformBuffer.data.texture_width, uniformBuffer.data.texture_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL) );
}

void drawQuad()
{

    GL_CALL( glUseProgram(renderPrgm.id) );
    GL_CALL( glActiveTexture(GL_TEXTURE0) );
    GL_CALL( glBindTexture( GL_TEXTURE_2D, textureInternal.id) );
    GL_CALL( glDrawElements(GL_TRIANGLES, sizeof(quad_iData) / sizeof(quad_iData[0]), GL_UNSIGNED_INT, NULL) );
}

void setupUniformBuffer()
{

    uniformBuffer.data.texture_width  = TEXTURE_WIDTH;
    uniformBuffer.data.texture_height = TEXTURE_HEIGHT;
    uniformBuffer.data.patch_size     = PATCH_SIZE_2D;
    uniformBuffer.data.patch_offset.u = 0;
    uniformBuffer.data.patch_offset.v = 0;
    uniformBuffer.data.agent_count    = AGENT_COUNT;
    uniformBuffer.data.delta_time     = 1.0f / 60.0f;

    GL_CALL( glGenBuffers(1, &uniformBuffer.id) );
    GL_CALL( glBindBuffer(GL_UNIFORM_BUFFER, uniformBuffer.id) );
    GL_CALL( glBufferData(GL_UNIFORM_BUFFER, sizeof(uniformBuffer.data), &uniformBuffer.data, GL_STATIC_DRAW) );
    GL_CALL( glBindBufferBase(GL_UNIFORM_BUFFER, UNIFORM_BUFFER_BINDING, uniformBuffer.id) );
}

void updateUniformBuffer()
{

    GL_CALL( glBindBuffer(GL_UNIFORM_BUFFER, uniformBuffer.id) );
    GL_CALL( glBufferData(GL_UNIFORM_BUFFER, sizeof(uniformBuffer.data), &uniformBuffer.data, GL_STATIC_DRAW) );
}

void dispatchTextureSpace(GLint prgm_id)
{

    uniformBuffer.data.patch_size = PATCH_SIZE_2D;
    GL_CALL( glBindImageTexture(TEXTURE_INT_BINDING, textureInternal.id, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F) );

    GL_CALL( glUseProgram(prgm_id) );

    uint32_t *u = &uniformBuffer.data.patch_offset.u;
    uint32_t *v = &uniformBuffer.data.patch_offset.v;

    for (*u = 0; *u < uniformBuffer.data.texture_width; *u += PATCH_SIZE_2D) {
        
        for (*v = 0; *v < uniformBuffer.data.texture_height; *v += PATCH_SIZE_2D) {

            updateUniformBuffer();
            GL_CALL( glDispatchCompute(PATCH_SIZE_2D / LOCAL_INVOCATION_SIZE_2D, PATCH_SIZE_2D / LOCAL_INVOCATION_SIZE_2D, 1) );
            GL_CALL( glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT) );
            GL_CALL( glFinish() ); 
        }
    }
}

void setupShaderStorageBuffer()
{

    for (size_t i = 0; i < AGENT_COUNT; i++) {

        struct agent a;
        a.position.x = rand() % TEXTURE_WIDTH;
        a.position.y = rand() % TEXTURE_HEIGHT;
        int s = rand() % 3;
        a.signature.r = 1;///*rand() % 2;//*/s == 0 ? 1.0 : 0.0;
        a.signature.g = 1;///*rand() % 2;//*/s == 1 ? 1.0 : 0.0;
        a.signature.b = 1;///*rand() % 2;//*/s == 2 ? 1.0 : 0.0;
        a.angle       = 2.0f * M_PI * (rand() / (float)RAND_MAX);

        shaderStorageBuffer.data[i] = a;
    }

    GL_CALL( glGenBuffers(1, &shaderStorageBuffer.id) );
    GL_CALL( glBindBuffer(GL_SHADER_STORAGE_BUFFER, shaderStorageBuffer.id) );
    GL_CALL( glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(shaderStorageBuffer.data), shaderStorageBuffer.data, GL_STATIC_DRAW) );
    GL_CALL( glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SHADER_STORAGE_BUFFER_BINDING, shaderStorageBuffer.id) );
}

void dispatchAgentSpace(GLint prgm_id)
{

    uniformBuffer.data.patch_size = PATCH_SIZE_1D;
    GL_CALL( glUseProgram(prgm_id) );
    GL_CALL( glBindImageTexture(TEXTURE_INT_BINDING, textureInternal.id, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F) );
    
    uint32_t *u = &uniformBuffer.data.patch_offset.u;
    for (*u = 0; *u < uniformBuffer.data.agent_count; *u += PATCH_SIZE_1D) {

        updateUniformBuffer();
        GL_CALL( glDispatchCompute(PATCH_SIZE_1D / LOCAL_INVOCATION_SIZE_1D, 1, 1) );
        GL_CALL( glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT) );
        GL_CALL( glFinish() );
    }
}

void calculateDeltaTime()
{
    static clock_t t0 = 0.0f, t1;

    t1 = clock();
    uniformBuffer.data.delta_time = ((float)(t1 -t0)) / CLOCKS_PER_SEC;
    t0 = t1;
}


void dumpInfo()
{

    printf("Texture size:                      %d x %d\n", TEXTURE_WIDTH, TEXTURE_HEIGHT);
    printf("Local invocations (texture space): %d\n", LOCAL_INVOCATION_SIZE_2D);
    printf("Patch size (texture space):        %d\n", PATCH_SIZE_2D);
    printf("Dispatch calls (texture space):    %d\n", TEXTURE_WIDTH / (PATCH_SIZE_2D / LOCAL_INVOCATION_SIZE_2D) * TEXTURE_HEIGHT / (PATCH_SIZE_2D / LOCAL_INVOCATION_SIZE_2D));
    printf("Agent count:                       %d\n", AGENT_COUNT);
    printf("Local invocations (agent space):   %d\n", LOCAL_INVOCATION_SIZE_1D);
    printf("Patch size (agent space):          %d\n", PATCH_SIZE_1D);
    printf("Dispatch calls (agent space):      %d\n", AGENT_COUNT / (PATCH_SIZE_1D / LOCAL_INVOCATION_SIZE_1D));
}