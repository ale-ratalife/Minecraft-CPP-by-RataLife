#pragma once
#include <windows.h>
#include <GL/gl.h>
#include <string>
#include <unordered_map>

typedef char GLchar;

#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER   0x8B31
#define GL_COMPILE_STATUS  0x8B81
#define GL_LINK_STATUS     0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#endif

#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif

typedef GLuint(APIENTRY* PFN_CreateShader)(GLenum);
typedef void   (APIENTRY* PFN_ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
typedef void   (APIENTRY* PFN_CompileShader)(GLuint);
typedef void   (APIENTRY* PFN_GetShaderiv)(GLuint, GLenum, GLint*);
typedef void   (APIENTRY* PFN_GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef GLuint(APIENTRY* PFN_CreateProgram)(void);
typedef void   (APIENTRY* PFN_AttachShader)(GLuint, GLuint);
typedef void   (APIENTRY* PFN_LinkProgram)(GLuint);
typedef void   (APIENTRY* PFN_GetProgramiv)(GLuint, GLenum, GLint*);
typedef void   (APIENTRY* PFN_GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void   (APIENTRY* PFN_UseProgram)(GLuint);
typedef void   (APIENTRY* PFN_DeleteShader)(GLuint);
typedef void   (APIENTRY* PFN_DeleteProgram)(GLuint);
typedef GLint(APIENTRY* PFN_GetUniformLocation)(GLuint, const GLchar*);
typedef void   (APIENTRY* PFN_Uniform1i)(GLint, GLint);
typedef void   (APIENTRY* PFN_Uniform1f)(GLint, GLfloat);
typedef void   (APIENTRY* PFN_Uniform3f)(GLint, GLfloat, GLfloat, GLfloat);
typedef void   (APIENTRY* PFN_ActiveTexture)(GLenum);

struct GL2
{
    PFN_CreateShader       CreateShader = nullptr;
    PFN_ShaderSource       ShaderSource = nullptr;
    PFN_CompileShader      CompileShader = nullptr;
    PFN_GetShaderiv        GetShaderiv = nullptr;
    PFN_GetShaderInfoLog   GetShaderInfoLog = nullptr;
    PFN_CreateProgram      CreateProgram = nullptr;
    PFN_AttachShader       AttachShader = nullptr;
    PFN_LinkProgram        LinkProgram = nullptr;
    PFN_GetProgramiv       GetProgramiv = nullptr;
    PFN_GetProgramInfoLog  GetProgramInfoLog = nullptr;
    PFN_UseProgram         UseProgram = nullptr;
    PFN_DeleteShader       DeleteShader = nullptr;
    PFN_DeleteProgram      DeleteProgram = nullptr;
    PFN_GetUniformLocation GetUniformLocation = nullptr;
    PFN_Uniform1i          Uniform1i = nullptr;
    PFN_Uniform1f          Uniform1f = nullptr;
    PFN_Uniform3f          Uniform3f = nullptr;
    PFN_ActiveTexture      ActiveTexture = nullptr;
};

extern GL2 gl2;

inline void* glProc(const char* n)
{
    void* p = (void*)wglGetProcAddress(n);
    if (!p || p == (void*)1 || p == (void*)2 || p == (void*)3 || p == (void*)-1)
    {
        static HMODULE gl = LoadLibraryA("opengl32.dll");
        p = gl ? (void*)GetProcAddress(gl, n) : nullptr;
    }
    return p;
}

inline bool GL2_Init()
{
#define L(field, name) gl2.field = (PFN_##field)glProc(name); if (!gl2.field) return false;
    L(CreateShader, "glCreateShader")
        L(ShaderSource, "glShaderSource")
        L(CompileShader, "glCompileShader")
        L(GetShaderiv, "glGetShaderiv")
        L(GetShaderInfoLog, "glGetShaderInfoLog")
        L(CreateProgram, "glCreateProgram")
        L(AttachShader, "glAttachShader")
        L(LinkProgram, "glLinkProgram")
        L(GetProgramiv, "glGetProgramiv")
        L(GetProgramInfoLog, "glGetProgramInfoLog")
        L(UseProgram, "glUseProgram")
        L(DeleteShader, "glDeleteShader")
        L(DeleteProgram, "glDeleteProgram")
        L(GetUniformLocation, "glGetUniformLocation")
        L(Uniform1i, "glUniform1i")
        L(Uniform1f, "glUniform1f")
        L(Uniform3f, "glUniform3f")
        L(ActiveTexture, "glActiveTexture")
#undef L
        return true;
}

struct ShaderProgram
{
    GLuint id = 0;
    bool   ok = false;
    std::unordered_map<std::string, GLint> cache;

    bool build(const char* vs, const char* fs)
    {
        GLuint v = gl2.CreateShader(GL_VERTEX_SHADER);
        gl2.ShaderSource(v, 1, &vs, nullptr);
        gl2.CompileShader(v);
        GLint s = 0;
        gl2.GetShaderiv(v, GL_COMPILE_STATUS, &s);
        if (!s)
        {
            char log[2048];
            gl2.GetShaderInfoLog(v, 2048, nullptr, log);
            OutputDebugStringA("VS: "); OutputDebugStringA(log); OutputDebugStringA("\n");
            gl2.DeleteShader(v);
            return false;
        }

        GLuint f = gl2.CreateShader(GL_FRAGMENT_SHADER);
        gl2.ShaderSource(f, 1, &fs, nullptr);
        gl2.CompileShader(f);
        gl2.GetShaderiv(f, GL_COMPILE_STATUS, &s);
        if (!s)
        {
            char log[2048];
            gl2.GetShaderInfoLog(f, 2048, nullptr, log);
            OutputDebugStringA("FS: "); OutputDebugStringA(log); OutputDebugStringA("\n");
            gl2.DeleteShader(v); gl2.DeleteShader(f);
            return false;
        }

        id = gl2.CreateProgram();
        gl2.AttachShader(id, v);
        gl2.AttachShader(id, f);
        gl2.LinkProgram(id);
        gl2.GetProgramiv(id, GL_LINK_STATUS, &s);
        gl2.DeleteShader(v); gl2.DeleteShader(f);
        if (!s)
        {
            char log[2048];
            gl2.GetProgramInfoLog(id, 2048, nullptr, log);
            OutputDebugStringA("LINK: "); OutputDebugStringA(log); OutputDebugStringA("\n");
            gl2.DeleteProgram(id);
            id = 0;
            return false;
        }

        ok = true;
        return true;
    }

    void use() const { gl2.UseProgram(id); }

    GLint loc(const char* n)
    {
        auto it = cache.find(n);
        if (it != cache.end()) return it->second;
        GLint l = gl2.GetUniformLocation(id, n);
        cache[n] = l;
        return l;
    }

    void seti(const char* n, int v) { gl2.Uniform1i(loc(n), v); }
    void setf(const char* n, float v) { gl2.Uniform1f(loc(n), v); }
    void set3(const char* n, float a, float b, float c) { gl2.Uniform3f(loc(n), a, b, c); }

    void destroy()
    {
        if (id) gl2.DeleteProgram(id);
        id = 0;
        ok = false;
        cache.clear();
    }
};