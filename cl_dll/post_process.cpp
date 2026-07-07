// post_process.cpp — Unified post-processing pipeline for OpenAG
// Fixed-function OpenGL 2.x (no GLSL).
//
// ==============================================================================
// ==============================================================================

#include "hud.h"
#include "cl_util.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "post_process.h"

#include <math.h>
#include <stdlib.h>

namespace post_process
{

static void CheckGLError(const char* context) {
	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR) {
		gEngfuncs.Con_Printf("OpenGL Error in %s: 0x%X\n", context, err);
	}
}
// ─────────────────────────── CVars ────────────────────────────────────────
static cvar_t* cl_bloom          = nullptr;
static cvar_t* cl_bloom_intensity= nullptr;
static cvar_t* cl_bloom_radius   = nullptr;
static cvar_t* cl_bloom_threshold= nullptr;
static cvar_t* cl_bloom_passes   = nullptr;

static cvar_t* cl_ssao = nullptr;
static cvar_t* cl_ssao_radius = nullptr;
static cvar_t* cl_ssao_intensity = nullptr;

static cvar_t* cl_motion_blur          = nullptr;
static cvar_t* cl_motion_blur_alpha    = nullptr;
static cvar_t* cl_motion_blur_shutter  = nullptr;
static cvar_t* cl_motion_blur_max      = nullptr;
static cvar_t* cl_motion_blur_chromatic= nullptr;

static cvar_t* cl_vignette         = nullptr;
static cvar_t* cl_vignette_radius  = nullptr;
static cvar_t* cl_vignette_softness= nullptr;
static cvar_t* cl_vignette_strength= nullptr;

static cvar_t* cl_film_grain       = nullptr;
static cvar_t* cl_film_grain_amount= nullptr;

// ─────────────────────────── Textures ─────────────────────────────────────
static GLuint g_TexScreen   = 0;   // Clean 3D frame — captured ONCE, never modified
static GLuint g_TexDepth    = 0;   // Depth buffer
static GLuint g_TexBloom[2] = {};  // Bloom ping-pong (512×512)
static int g_LastW = 0;
static int g_LastH = 0;

// FBO pointers
static PFNGLGENFRAMEBUFFERSEXTPROC glGenFramebuffersEXT = nullptr;
static PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT = nullptr;
static PFNGLFRAMEBUFFERTEXTURE2DEXTPROC glFramebufferTexture2DEXT = nullptr;

// Blit FBO (For MSAA Depth Resolve)
typedef void (APIENTRYP PFNGLBLITFRAMEBUFFEREXTPROC)(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
static PFNGLBLITFRAMEBUFFEREXTPROC glBlitFramebufferEXT = nullptr;

// GLSL pointers
static PFNGLCREATESHADERPROC glCreateShader = nullptr;
static PFNGLSHADERSOURCEPROC glShaderSource = nullptr;
static PFNGLCOMPILESHADERPROC glCompileShader = nullptr;
static PFNGLCREATEPROGRAMPROC glCreateProgram = nullptr;
static PFNGLATTACHSHADERPROC glAttachShader = nullptr;
static PFNGLLINKPROGRAMPROC glLinkProgram = nullptr;
static PFNGLUSEPROGRAMPROC glUseProgram = nullptr;
static PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = nullptr;
static PFNGLUNIFORM1IPROC glUniform1i = nullptr;
static PFNGLUNIFORM2FPROC glUniform2f = nullptr;
static PFNGLUNIFORM1FPROC glUniform1f = nullptr;
static PFNGLGETSHADERIVPROC glGetShaderiv = nullptr;
static PFNGLGETPROGRAMIVPROC glGetProgramiv = nullptr;

#ifndef GL_FRAMEBUFFER_EXT
#define GL_FRAMEBUFFER_EXT 0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0_EXT
#define GL_COLOR_ATTACHMENT0_EXT 0x8CE0
#endif
#ifndef GL_DEPTH_COMPONENT
#define GL_DEPTH_COMPONENT 0x1902
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif

// New constants for MSAA Blit
#ifndef GL_FRAMEBUFFER_BINDING_EXT
#define GL_FRAMEBUFFER_BINDING_EXT 0x8CA6
#endif
#ifndef GL_READ_FRAMEBUFFER_EXT
#define GL_READ_FRAMEBUFFER_EXT 0x8CA8
#endif
#ifndef GL_DRAW_FRAMEBUFFER_EXT
#define GL_DRAW_FRAMEBUFFER_EXT 0x8CA9
#endif
#ifndef GL_DEPTH_ATTACHMENT_EXT
#define GL_DEPTH_ATTACHMENT_EXT 0x8D00
#endif

static GLuint g_FBO = 0;
static GLuint g_DepthBlitFBO = 0;
static GLuint g_SSAOProgram = 0;

// Motion blur state
static float g_LastAngles[3] = {};
static bool  g_HasLastAngles = false;

// Film grain seed
static unsigned int g_GrainSeed = 98765u;

// ─────────────────────────── Helpers ──────────────────────────────────────
static int NextPOT(int n)
{
	int v = 1;
	while (v < n) v <<= 1;
	return v;
}

static inline float frand01(unsigned int &seed)
{
	seed = seed * 1664525u + 1013904223u;
	return (float)(seed & 0xFFFFFF) / (float)0x1000000;
}

// Draw a screen-space quad. u1/v1..u2/v2 = UV coords; vx/vy = vertex coords.
static void DrawQuadUV(float u1, float v1, float u2, float v2,
                        float vx1=0.f, float vy1=0.f, float vx2=1.f, float vy2=1.f)
{
	glBegin(GL_QUADS);
	glTexCoord2f(u1, v1); glVertex2f(vx1, vy1); // BL
	glTexCoord2f(u2, v1); glVertex2f(vx2, vy1); // BR
	glTexCoord2f(u2, v2); glVertex2f(vx2, vy2); // TR
	glTexCoord2f(u1, v2); glVertex2f(vx1, vy2); // TL
	glEnd();
}

static void SetupOrtho()
{
	glMatrixMode(GL_PROJECTION); glLoadIdentity();
	glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);  glLoadIdentity();
	glMatrixMode(GL_TEXTURE);    glLoadIdentity();
}

static void PushGLState()
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
	glMatrixMode(GL_PROJECTION); glPushMatrix();
	glMatrixMode(GL_MODELVIEW);  glPushMatrix();
	glMatrixMode(GL_TEXTURE);    glPushMatrix();
	SetupOrtho();

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glDisable(GL_SCISSOR_TEST);
	glActiveTexture(GL_TEXTURE1); glDisable(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE2); glDisable(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE0); glEnable(GL_TEXTURE_2D);
}

static void PopGLState()
{
	glMatrixMode(GL_TEXTURE);    glPopMatrix();
	glMatrixMode(GL_MODELVIEW);  glPopMatrix();
	glMatrixMode(GL_PROJECTION); glPopMatrix();
	glPopClientAttrib();
	glPopAttrib();
}

static void MakeTexture(GLuint& tex, int w, int h, GLenum fmt = GL_RGB)
{
	if (tex) glDeleteTextures(1, &tex);
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, nullptr);
}

static GLuint CompileShader(GLenum type, const char* source)
{
	if (!glCreateShader || !glShaderSource || !glCompileShader || !glGetShaderiv) return 0;
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);
	
	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		gEngfuncs.Con_Printf("Failed to compile shader\n");
		return 0;
	}
	return shader;
}

static GLuint LinkProgram(GLuint vs, GLuint fs)
{
	if (!glCreateProgram || !glAttachShader || !glLinkProgram || !glGetProgramiv) return 0;
	GLuint prog = glCreateProgram();
	if (vs) glAttachShader(prog, vs);
	if (fs) glAttachShader(prog, fs);
	glLinkProgram(prog);
	
	GLint status;
	glGetProgramiv(prog, GL_LINK_STATUS, &status);
	if (!status) {
		gEngfuncs.Con_Printf("Failed to link shader program\n");
		return 0;
	}
	return prog;
}

static void InitTextures(int w, int h)
{
	if (g_TexScreen && g_LastW == w && g_LastH == h)
		return;

	if (!glGenFramebuffersEXT) {
		glGenFramebuffersEXT = (PFNGLGENFRAMEBUFFERSEXTPROC)SDL_GL_GetProcAddress("glGenFramebuffersEXT");
		glBindFramebufferEXT = (PFNGLBINDFRAMEBUFFEREXTPROC)SDL_GL_GetProcAddress("glBindFramebufferEXT");
		glFramebufferTexture2DEXT = (PFNGLFRAMEBUFFERTEXTURE2DEXTPROC)SDL_GL_GetProcAddress("glFramebufferTexture2DEXT");
	}
	
	if (!glBlitFramebufferEXT) {
		glBlitFramebufferEXT = (PFNGLBLITFRAMEBUFFEREXTPROC)SDL_GL_GetProcAddress("glBlitFramebufferEXT");
		if (!glBlitFramebufferEXT) glBlitFramebufferEXT = (PFNGLBLITFRAMEBUFFEREXTPROC)SDL_GL_GetProcAddress("glBlitFramebuffer");
	}

	if (!g_FBO && glGenFramebuffersEXT) {
		glGenFramebuffersEXT(1, &g_FBO);
	}

	if (!glCreateShader) {
		glCreateShader = (PFNGLCREATESHADERPROC)SDL_GL_GetProcAddress("glCreateShader");
		glShaderSource = (PFNGLSHADERSOURCEPROC)SDL_GL_GetProcAddress("glShaderSource");
		glCompileShader = (PFNGLCOMPILESHADERPROC)SDL_GL_GetProcAddress("glCompileShader");
		glCreateProgram = (PFNGLCREATEPROGRAMPROC)SDL_GL_GetProcAddress("glCreateProgram");
		glAttachShader = (PFNGLATTACHSHADERPROC)SDL_GL_GetProcAddress("glAttachShader");
		glLinkProgram = (PFNGLLINKPROGRAMPROC)SDL_GL_GetProcAddress("glLinkProgram");
		glUseProgram = (PFNGLUSEPROGRAMPROC)SDL_GL_GetProcAddress("glUseProgram");
		glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)SDL_GL_GetProcAddress("glGetUniformLocation");
		glUniform1i = (PFNGLUNIFORM1IPROC)SDL_GL_GetProcAddress("glUniform1i");
		glUniform2f = (PFNGLUNIFORM2FPROC)SDL_GL_GetProcAddress("glUniform2f");
		glUniform1f = (PFNGLUNIFORM1FPROC)SDL_GL_GetProcAddress("glUniform1f");
		glGetShaderiv = (PFNGLGETSHADERIVPROC)SDL_GL_GetProcAddress("glGetShaderiv");
		glGetProgramiv = (PFNGLGETPROGRAMIVPROC)SDL_GL_GetProcAddress("glGetProgramiv");
	}

	if (!g_SSAOProgram && glCreateShader) {
		const char* vs_src = 
			"void main() {\n"
			"  gl_Position = ftransform();\n"
			"  gl_TexCoord[0] = gl_MultiTexCoord0;\n"
			"}\n";
			
		// НОВЫЙ ШЕЙДЕР: Работает в режиме Multiply (затемнение).
		// Не читает цвет экрана, использует только глубину!
		const char* fs_src = 
			"uniform sampler2D texDepth;\n"
			"uniform vec2 pixelSize;\n"
			"uniform float aoRadius;\n"
			"uniform float aoIntensity;\n"
			"float linearize(float d) {\n"
			"  float n = 4.0; float f = 4096.0;\n"
			"  return (2.0 * n) / (f + n - d * (f - n));\n"
			"}\n"
			"void main() {\n"
			"  vec2 uv = gl_TexCoord[0].st;\n"
			"  float raw_depth = texture2D(texDepth, uv).r;\n"
			"  if (raw_depth >= 0.9999) {\n"
			"    gl_FragColor = vec4(1.0);\n" // Небо не трогаем
			"    return;\n"
			"  }\n"
			"  float depth = linearize(raw_depth);\n"
			
			// Масштабируем радиус в зависимости от дистанции (перспективная коррекция)
			"  float radiusScale = aoRadius / (depth * 20.0 + 0.5);\n"
			
			// Используем 4 пары противоположных направлений
			"  vec2 offsets[4];\n"
			"  offsets[0] = vec2( 1.0,  0.0);\n"
			"  offsets[1] = vec2( 0.0,  1.0);\n"
			"  offsets[2] = vec2( 0.707,  0.707);\n"
			"  offsets[3] = vec2(-0.707,  0.707);\n"
			
			"  float ao = 0.0;\n"
			"  for (int i = 0; i < 4; i++) {\n"
			"    vec2 offset = offsets[i] * pixelSize * radiusScale;\n"
			
			// Читаем противоположные сэмплы
			"    float d1 = linearize(texture2D(texDepth, uv + offset).r);\n"
			"    float d2 = linearize(texture2D(texDepth, uv - offset).r);\n"
			
			// Математика: вычисляем идеальную плоскую поверхность между сэмплами (1/z интерполяция)
			"    float expected_depth = 1.0 / ((1.0 / d1 + 1.0 / d2) * 0.5);\n"
			
			// Насколько сильно пиксель "вдавлен" внутрь угла по сравнению с плоской поверхностью
			"    float valley = depth - expected_depth;\n"
			"    float valley_units = valley * 4000.0;\n" // Перевод в игровые юниты движка
			
			// Если углубление больше 1 юнита (защита от погрешностей на ровном полу)
			"    if (valley_units > 1.0) {\n"
			// Проверяем, не слишком ли далеко эти пиксели (чтобы объекты не светились)
			"      float range_units = max(abs(d1 - depth), abs(d2 - depth)) * 4000.0;\n"
			"      float weight = smoothstep(60.0, 15.0, range_units);\n"
			
			// Плавно накладываем тень в зависимости от глубины угла
			"      float shadow = clamp((valley_units - 1.0) * 0.15, 0.0, 1.0);\n"
			"      ao += shadow * weight;\n"
			"    }\n"
			"  }\n"
			
			// Финальный расчет интенсивности (делим на 4 пары)
			"  ao = 1.0 - clamp((ao / 4.0) * aoIntensity, 0.0, 1.0);\n"
			"  gl_FragColor = vec4(ao, ao, ao, 1.0);\n"
			"}\n";
		GLuint vs = CompileShader(GL_VERTEX_SHADER, vs_src);
		GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fs_src);
		g_SSAOProgram = LinkProgram(vs, fs);
		if (g_SSAOProgram) {
			glUseProgram(g_SSAOProgram);
			glUniform1i(glGetUniformLocation(g_SSAOProgram, "texDepth"), 0); // Теперь глубина на TMU 0
			glUseProgram(0);
		}
	}

	int tw = NextPOT(w), th = NextPOT(h);
	
	if (!g_TexScreen) {
		MakeTexture(g_TexScreen, tw, th);
	}
	
	bool resizeDepth = (!g_TexDepth || g_LastW != w || g_LastH != h);
	if (!g_TexDepth) {
		glGenTextures(1, &g_TexDepth);
	}
	
	if (resizeDepth) {
		glBindTexture(GL_TEXTURE_2D, g_TexDepth);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		// Используем базовый формат, чтобы драйвер сам подобрал совместимый с MSAA-буфером
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, tw, th, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

		if (!g_DepthBlitFBO && glGenFramebuffersEXT) {
			glGenFramebuffersEXT(1, &g_DepthBlitFBO);
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, g_DepthBlitFBO);
			glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, g_TexDepth, 0);
			glDrawBuffer(GL_NONE);
			glReadBuffer(GL_NONE);
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		}
	}

	if (!g_TexBloom[0]) MakeTexture(g_TexBloom[0], 512, 512);
	if (!g_TexBloom[1]) MakeTexture(g_TexBloom[1], 512, 512);
	
	g_LastW = w; g_LastH = h;
}

// ─────────────────────────── SSAO ────────────────────────────────────────
static void DrawSSAO(int w, int h, float u, float v, int vp[4])
{
	if (!cl_ssao || cl_ssao->value == 0.0f || !g_SSAOProgram || !glUseProgram) return;
	if (!g_TexDepth) return; // Убрал проверку g_TexScreen, он нам тут больше не нужен!

	float radius = cl_ssao_radius ? cl_ssao_radius->value : 3.0f;
	float intensity = cl_ssao_intensity ? cl_ssao_intensity->value : 0.6f;
	if (intensity <= 0.0f) return;

	glUseProgram(g_SSAOProgram);

	if (glUniform2f) {
		GLint loc = glGetUniformLocation(g_SSAOProgram, "pixelSize");
		if (loc >= 0) glUniform2f(loc, 1.0f / (float)w, 1.0f / (float)h);
	}
	if (glUniform1f) {
		GLint loc = glGetUniformLocation(g_SSAOProgram, "aoRadius");
		if (loc >= 0) glUniform1f(loc, radius);
		loc = glGetUniformLocation(g_SSAOProgram, "aoIntensity");
		if (loc >= 0) glUniform1f(loc, intensity);
	}

	// Биндим глубину на TMU 0
	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, g_TexDepth);

	// НОВАЯ МАГИЯ: Multiply Blend. 
	// Мы умножаем текущие пиксели экрана на значение тени, которое выдал шейдер.
	glEnable(GL_BLEND);
	glBlendFunc(GL_ZERO, GL_SRC_COLOR); 
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glColor4f(1, 1, 1, 1);

	DrawQuadUV(0, 0, u, v);

	glUseProgram(0);
	glDisable(GL_BLEND); // Отключаем бленд, чтобы не сломать следующие эффекты
}

// ─────────────── Depth debug info (exposed to ImGui) ─────────────────────
static DepthDebugInfo g_DepthDebug = {};

const DepthDebugInfo& get_depth_debug()
{
	return g_DepthDebug;
}

void capture_depth()
{
	g_DepthDebug.capture_attempted = false;
	g_DepthDebug.capture_succeeded = false;
	g_DepthDebug.method_used = 0;
	g_DepthDebug.gl_error = 0;

	if (!cl_ssao || cl_ssao->value == 0.0f) return;

	GLint vp[4];
	glGetIntegerv(GL_VIEWPORT, vp);
	int w = vp[2], h = vp[3];
	g_DepthDebug.viewport_w = w;
	g_DepthDebug.viewport_h = h;
	if (w <= 0 || h <= 0) return;

	g_DepthDebug.capture_attempted = true;
	while (glGetError() != GL_NO_ERROR) {} 

	int cx = w / 2, cy = h / 2;

	// Метод 4: MSAA Blit
	if (g_TexDepth && g_DepthBlitFBO && glBlitFramebufferEXT) {
		GLint engineFBO = 0;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &engineFBO);

		glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, engineFBO);
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, g_DepthBlitFBO);
		glBlitFramebufferEXT(0, 0, w, h, 0, 0, w, h, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		
		GLenum err = glGetError();
		if (err == GL_NO_ERROR) {
			g_DepthDebug.method_used = 4;
			g_DepthDebug.capture_succeeded = true;
			
			glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, g_DepthBlitFBO);
			float depthF = 0.0f;
			glReadPixels(cx, cy, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depthF);
			g_DepthDebug.center_depth = depthF;
			
			int offsets[3] = { -40, 0, 40 };
			for (int row = 0; row < 3; row++) {
				for (int col = 0; col < 3; col++) {
					int sx = cx + offsets[col];
					int sy = cy + offsets[row];
					if (sx < 0) sx = 0; if (sx >= w) sx = w - 1;
					if (sy < 0) sy = 0; if (sy >= h) sy = h - 1;
					float d = 0.0f;
					glReadPixels(sx, sy, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &d);
					g_DepthDebug.sample_depths[row * 3 + col] = d;
				}
			}
			
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, engineFBO);
			g_DepthDebug.frame_count++;
			return; 
		} else {
			g_DepthDebug.gl_error = (int)err;
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, engineFBO);
		}
	}

	// Метод 1: glReadPixels 
	float depthF = -1.0f;
	glReadPixels(cx, cy, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depthF);
	GLenum err = glGetError();

	if (err == GL_NO_ERROR && depthF >= 0.0f && depthF <= 1.0f) {
		g_DepthDebug.method_used = 1;
		g_DepthDebug.center_depth = depthF;
		g_DepthDebug.capture_succeeded = true;

		if (g_TexDepth) {
			GLint oldTex = 0;
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTex);
			glBindTexture(GL_TEXTURE_2D, g_TexDepth);
			glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);
			if (glGetError() != GL_NO_ERROR) {
				static GLfloat* depthBuf = nullptr;
				static int bufW = 0, bufH = 0;
				if (bufW != w || bufH != h) {
					delete[] depthBuf;
					depthBuf = new GLfloat[w * h];
					bufW = w; bufH = h;
				}
				glReadPixels(0, 0, w, h, GL_DEPTH_COMPONENT, GL_FLOAT, depthBuf);
				if (glGetError() == GL_NO_ERROR) {
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_DEPTH_COMPONENT, GL_FLOAT, depthBuf);
				}
			}
			glBindTexture(GL_TEXTURE_2D, (GLuint)oldTex);
		}
		g_DepthDebug.frame_count++;
		return;
	}

	g_DepthDebug.gl_error = (int)err;
	g_DepthDebug.capture_succeeded = false;
}

static void DrawBloom(int w, int h, float u, float v, int vp[4])
{
	int passes = cl_bloom_passes ? (int)cl_bloom_passes->value : 5;
	float intensity = cl_bloom_intensity ? cl_bloom_intensity->value : 0.8f;
	int thresh = cl_bloom_threshold ? (int)cl_bloom_threshold->value : 2;
	float radius = cl_bloom_radius ? cl_bloom_radius->value : 2.5f;

	if (passes <= 0 || intensity <= 0.0f || !g_FBO || !glBindFramebufferEXT)
		return;

	GLint old_fbo = 0;
	glGetIntegerv(0x8CA6 /* GL_FRAMEBUFFER_BINDING_EXT */, &old_fbo);

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, g_FBO);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, g_TexBloom[0], 0);
	glViewport(0, 0, 512, 512);

	glBindTexture(GL_TEXTURE_2D, g_TexScreen);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glDisable(GL_BLEND);
	glColor4f(1,1,1,1);
	DrawQuadUV(0, 0, u, v);

	if (thresh > 0)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_DST_COLOR, GL_ZERO);
		for (int i = 0; i < thresh; i++)
			DrawQuadUV(0, 0, u, v);
		glDisable(GL_BLEND);
	}

	int src = 0;
	float step = radius / 512.0f;
	for (int p = 0; p < passes; p++)
	{
		int dst = 1 - src;
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, g_TexBloom[dst], 0);
		
		glBindTexture(GL_TEXTURE_2D, g_TexBloom[src]);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		glDisable(GL_BLEND);
		glColor4f(0.2f, 0.2f, 0.2f, 1.f);
		DrawQuadUV(0, 0, 1, 1);

		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);
		glColor4f(0.2f, 0.2f, 0.2f, 1.f);
		DrawQuadUV(-step, 0,    1.f-step, 1.f);
		DrawQuadUV( step, 0,    1.f+step, 1.f);
		DrawQuadUV(0,    -step, 1.f,      1.f-step);
		DrawQuadUV(0,     step, 1.f,      1.f+step);
		glDisable(GL_BLEND);

		src = dst;
		step *= 1.6f;
	}

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, old_fbo);
	glViewport(vp[0], vp[1], vp[2], vp[3]);
	glBindTexture(GL_TEXTURE_2D, g_TexBloom[src]);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	float pad = 0.006f;
	glColor4f(intensity, intensity, intensity, 1.f);
	DrawQuadUV(-pad, -pad, 1.f+pad, 1.f+pad);

	float cx2 = intensity * 0.35f;
	glColor4f(cx2, cx2, cx2, 1.f);
	DrawQuadUV(0, 0, 1, 1);

	glDisable(GL_BLEND);
}

// ─────────────────────────── Motion Blur ──────────────────────────────────
static void DrawMotionBlur(int w, int h, float u, float v)
{
	float angles[3];
	gEngfuncs.GetViewAngles(angles);

	if (!g_HasLastAngles)
	{
		g_LastAngles[0] = angles[0]; g_LastAngles[1] = angles[1]; g_LastAngles[2] = angles[2];
		g_HasLastAngles = true;
		return;
	}

	float dp = angles[0] - g_LastAngles[0];
	float dy = angles[1] - g_LastAngles[1];
	g_LastAngles[0] = angles[0]; g_LastAngles[1] = angles[1]; g_LastAngles[2] = angles[2];

	if (dp >  180.f) dp -= 360.f; if (dp < -180.f) dp += 360.f;
	if (dy >  180.f) dy -= 360.f; if (dy < -180.f) dy += 360.f;

	float dt = (float)gHUD.m_flTimeDelta;
	if (dt < 0.001f) dt = 0.001f;
	if (dt > 0.1f)   dt = 0.1f;

	float shutter = cl_motion_blur_shutter   ? cl_motion_blur_shutter->value   : 0.015f;
	float maxLen  = cl_motion_blur_max       ? cl_motion_blur_max->value       : 25.f;
	float alpha   = cl_motion_blur_alpha     ? cl_motion_blur_alpha->value     : 0.08f;
	float chrom   = cl_motion_blur_chromatic ? cl_motion_blur_chromatic->value : 0.f;

	float vx = (dy / dt) * shutter;
	float vy = -(dp / dt) * shutter;
	float len = sqrtf(vx*vx + vy*vy);
	if (len < 0.4f) return;
	if (len > maxLen) { vx = vx/len*maxLen; vy = vy/len*maxLen; }

	float uvx = (vx / w) * u;
	float uvy = (vy / h) * v;

	glBindTexture(GL_TEXTURE_2D, g_TexScreen);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	static const float offsets[6] = { -0.5f, -0.3f, -0.12f, 0.12f, 0.3f, 0.5f };
	for (int i = 0; i < 6; i++)
	{
		float t = offsets[i];
		if (chrom > 0.f)
		{
			float rs = 1.f + chrom * 0.035f;
			float bs = 1.f - chrom * 0.035f;
			glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
			glColor4f(1,1,1,alpha);
			DrawQuadUV(t*uvx*rs, t*uvy*rs, u+t*uvx*rs, v+t*uvy*rs);
			glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
			glColor4f(1,1,1,alpha);
			DrawQuadUV(t*uvx,    t*uvy,    u+t*uvx,    v+t*uvy);
			glColorMask(GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE);
			glColor4f(1,1,1,alpha);
			DrawQuadUV(t*uvx*bs, t*uvy*bs, u+t*uvx*bs, v+t*uvy*bs);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		}
		else
		{
			glColor4f(1,1,1,alpha);
			DrawQuadUV(t*uvx, t*uvy, u+t*uvx, v+t*uvy);
		}
	}
	glDisable(GL_BLEND);
}

// ─────────────────────────── Vignette ─────────────────────────────────────
static void DrawVignette(int w, int h)
{
	float strength = cl_vignette_strength ? cl_vignette_strength->value : 0.75f;
	float radius   = cl_vignette_radius   ? cl_vignette_radius->value   : 0.65f;
	float softness = cl_vignette_softness ? cl_vignette_softness->value : 0.45f;
	if (strength <= 0.f) return;
	if (radius   < 0.05f) radius = 0.05f;
	if (softness < 0.01f) softness = 0.01f;

	float aspect = (w > 0 && h > 0) ? (float)w / (float)h : 1.f;

	static GLuint texVig = 0;
	static float l_rad = -1, l_soft = -1, l_str = -1, l_asp = -1;
	
	if (!texVig)
		glGenTextures(1, &texVig);

	if (l_rad != radius || l_soft != softness || l_str != strength || l_asp != aspect)
	{
		l_rad = radius; l_soft = softness; l_str = strength; l_asp = aspect;
		unsigned char* data = (unsigned char*)malloc(512 * 512 * 2);
		for (int y = 0; y < 512; y++)
		{
			for (int x = 0; x < 512; x++)
			{
				float px = (x + 0.5f) / 512.f;
				float py = (y + 0.5f) / 512.f;
				float dx = (px - 0.5f) * 2.f * aspect;
				float dy = (py - 0.5f) * 2.f;
				float dist = sqrtf(dx * dx + dy * dy);

				float t = (dist - radius) / softness;
				if (t < 0.f) t = 0.f;
				if (t > 1.f) t = 1.f;
				t = t * t * (3.f - 2.f * t);

				int idx = (y * 512 + x) * 2;
				data[idx] = 0;
				data[idx + 1] = (unsigned char)(t * strength * 255.f);
			}
		}
		glBindTexture(GL_TEXTURE_2D, texVig);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, 512, 512, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, data);
		free(data);
	}

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texVig);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glColor4f(1.f, 1.f, 1.f, 1.f);
	DrawQuadUV(0, 0, 1, 1);

	glDisable(GL_BLEND);
}

// ─────────────────────────── Film Grain ───────────────────────────────────
static void DrawFilmGrain(int w, int h)
{
	float amount = cl_film_grain_amount ? cl_film_grain_amount->value : 0.04f;
	if (amount <= 0.f) return;

	const int gW = w / 4;
	const int gH = h / 4;
	if (gW < 1 || gH < 1) return;

	float px = 1.f / gW;
	float py = 1.f / gH;
	g_GrainSeed += 7757u;

	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBegin(GL_QUADS);
	for (int gy = 0; gy < gH; gy++)
	{
		for (int gx = 0; gx < gW; gx++)
		{
			float n = frand01(g_GrainSeed);
			float delta = (n - 0.5f) * 2.f;
			float a = fabsf(delta) * amount;
			if (a < 0.005f) continue;
			float bright = delta > 0.f ? 1.f : 0.f;
			glColor4f(bright, bright, bright, a);
			float x0 = gx * px, y0 = gy * py;
			glVertex2f(x0,    y0);
			glVertex2f(x0+px, y0);
			glVertex2f(x0+px, y0+py);
			glVertex2f(x0,    y0+py);
		}
	}
	glEnd();

	glDisable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
}

// ─────────────────────────── Public API ───────────────────────────────────
void init()
{
	cl_bloom           = CVAR_CREATE("cl_bloom",           "0",   FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	cl_bloom_intensity = CVAR_CREATE("cl_bloom_intensity", "0.5", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	cl_bloom_radius    = CVAR_CREATE("cl_bloom_radius",    "2.5", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	cl_bloom_threshold = CVAR_CREATE("cl_bloom_threshold", "2",   FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	cl_bloom_passes = gEngfuncs.pfnRegisterVariable("cl_bloom_passes", "5", FCVAR_ARCHIVE);

	cl_ssao = gEngfuncs.pfnRegisterVariable("cl_ssao", "0", FCVAR_ARCHIVE);
	cl_ssao_radius = gEngfuncs.pfnRegisterVariable("cl_ssao_radius", "3.0", FCVAR_ARCHIVE);
	cl_ssao_intensity = gEngfuncs.pfnRegisterVariable("cl_ssao_intensity", "0.6", FCVAR_ARCHIVE);

	cl_motion_blur           = CVAR_CREATE("cl_motion_blur",           "0",     FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	cl_motion_blur_alpha     = CVAR_CREATE("cl_motion_blur_alpha",     "0.08",  FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	cl_motion_blur_shutter   = CVAR_CREATE("cl_motion_blur_shutter",   "0.015", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	cl_motion_blur_max       = CVAR_CREATE("cl_motion_blur_max",       "25.0",  FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	cl_motion_blur_chromatic = CVAR_CREATE("cl_motion_blur_chromatic", "0.0",   FCVAR_CLIENTDLL | FCVAR_ARCHIVE);

	cl_vignette          = CVAR_CREATE("cl_vignette",          "1",    FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	cl_vignette_radius   = CVAR_CREATE("cl_vignette_radius",   "0.6",  FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	cl_vignette_softness = CVAR_CREATE("cl_vignette_softness", "0.4",  FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	cl_vignette_strength = CVAR_CREATE("cl_vignette_strength", "0.7",  FCVAR_CLIENTDLL | FCVAR_ARCHIVE);

	cl_film_grain        = CVAR_CREATE("cl_film_grain",        "0",    FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	cl_film_grain_amount = CVAR_CREATE("cl_film_grain_amount", "0.04", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
}

void draw()
{
	bool bloomOn = cl_bloom       && cl_bloom->value       != 0.f;
	bool blurOn  = cl_motion_blur && cl_motion_blur->value != 0.f;
	bool vigOn   = cl_vignette    && cl_vignette->value    != 0.f;
	bool grainOn = cl_film_grain  && cl_film_grain->value  != 0.f;
	bool ssaoOn  = cl_ssao        && cl_ssao->value        != 0.f && g_SSAOProgram != 0;

	if (!bloomOn && !blurOn && !vigOn && !grainOn && !ssaoOn) return;

	GLint vp[4];
	glGetIntegerv(GL_VIEWPORT, vp);
	int w = vp[2], h = vp[3];
	if (w <= 0 || h <= 0) return;

	InitTextures(w, h);

	int tw = NextPOT(w), th = NextPOT(h);
	float u = (float)w / tw;
	float v = (float)h / th;

	PushGLState();

	if (bloomOn || blurOn || ssaoOn)
	{
		glBindTexture(GL_TEXTURE_2D, g_TexScreen);
		// УБРАНО glReadBuffer(GL_BACK), теперь это не крашит стейт OpenGL!
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, vp[0], vp[1], w, h);
		
		if (ssaoOn) {
			DrawSSAO(w, h, u, v, vp);
			// ВТОРОЕ КОПИРОВАНИЕ ТОЖЕ УДАЛЕНО. Feedback loop (петля призраков) физически невозможна!
		}

		if (bloomOn)
			DrawBloom(w, h, u, v, vp);

		if (blurOn)
			DrawMotionBlur(w, h, u, v);
	}

	// ── Vignette ─────────────────────────────────────────────────────────
	if (vigOn)
	{
		glViewport(vp[0], vp[1], vp[2], vp[3]);
		DrawVignette(w, h);
	}

	// ── Film Grain ───────────────────────────────────────────────────────
	if (grainOn)
	{
		glViewport(vp[0], vp[1], vp[2], vp[3]);
		DrawFilmGrain(w, h);
	}

	PopGLState();
}

} // namespace post_process