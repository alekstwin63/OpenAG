#include "hud.h"
#include "cl_util.h"
#include "motion_blur.h"

#include <SDL2/SDL_opengl.h>
#include <math.h>

namespace motion_blur
{
	static cvar_t* cl_motion_blur = nullptr;
	static cvar_t* cl_motion_blur_shutter = nullptr;
	static cvar_t* cl_motion_blur_multiplier = nullptr;
	static cvar_t* cl_motion_blur_max = nullptr;
	static cvar_t* cl_motion_blur_chromatic = nullptr;

	static GLuint g_ScreenTexture = 0;
	static int g_LastWidth = 0;
	static int g_LastHeight = 0;

	static float g_LastAngles[3] = {0.0f, 0.0f, 0.0f};
	static bool g_HasLastAngles = false;

	static int NextPowerOfTwo(int n)
	{
		int val = 1;
		while (val < n)
			val *= 2;
		return val;
	}

	static void InitTextures(int w, int h)
	{
		int tw = NextPowerOfTwo(w);
		int th = NextPowerOfTwo(h);

		if (g_ScreenTexture == 0 || g_LastWidth != w || g_LastHeight != h)
		{
			if (g_ScreenTexture != 0)
			{
				glDeleteTextures(1, &g_ScreenTexture);
			}

			glGenTextures(1, &g_ScreenTexture);
			glBindTexture(GL_TEXTURE_2D, g_ScreenTexture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

			g_LastWidth = w;
			g_LastHeight = h;
		}
	}

	void init()
	{
		cl_motion_blur = CVAR_CREATE("cl_motion_blur", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
		cl_motion_blur_shutter = CVAR_CREATE("cl_motion_blur_shutter", "0.015", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
		cl_motion_blur_multiplier = CVAR_CREATE("cl_motion_blur_multiplier", "1.0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
		cl_motion_blur_max = CVAR_CREATE("cl_motion_blur_max", "30.0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
		cl_motion_blur_chromatic = CVAR_CREATE("cl_motion_blur_chromatic", "0.0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	}

	void draw()
	{
		if (!cl_motion_blur || cl_motion_blur->value == 0.0f)
			return;

		GLint viewport[4];
		glGetIntegerv(GL_VIEWPORT, viewport);
		int w = viewport[2];
		int h = viewport[3];

		if (w <= 0 || h <= 0)
			return;

		float angles[3];
		gEngfuncs.GetViewAngles(angles);

		if (!g_HasLastAngles)
		{
			g_LastAngles[0] = angles[0];
			g_LastAngles[1] = angles[1];
			g_LastAngles[2] = angles[2];
			g_HasLastAngles = true;
			return;
		}

		float dp = angles[0] - g_LastAngles[0];
		float dy = angles[1] - g_LastAngles[1];

		if (dp > 180.0f) dp -= 360.0f;
		if (dp < -180.0f) dp += 360.0f;
		if (dy > 180.0f) dy -= 360.0f;
		if (dy < -180.0f) dy += 360.0f;

		g_LastAngles[0] = angles[0];
		g_LastAngles[1] = angles[1];
		g_LastAngles[2] = angles[2];

		float dt = (float)gHUD.m_flTimeDelta;
		if (dt < 0.001f) dt = 0.001f;
		if (dt > 0.1f) dt = 0.1f;

		float shutter_time = cl_motion_blur_shutter ? cl_motion_blur_shutter->value : 0.015f;
		float multiplier = cl_motion_blur_multiplier ? cl_motion_blur_multiplier->value : 1.0f;

		float vx = (dy / dt) * shutter_time * multiplier;
		float vy = -(dp / dt) * shutter_time * multiplier;

		float max_len = cl_motion_blur_max ? cl_motion_blur_max->value : 30.0f;
		float len = sqrtf(vx * vx + vy * vy);

		if (len < 0.5f)
		{
			// Motion is too small, skip rendering blur to save performance and keep image crisp
			return;
		}

		if (len > max_len)
		{
			vx = (vx / len) * max_len;
			vy = (vy / len) * max_len;
		}

		InitTextures(w, h);

		int tw = NextPowerOfTwo(w);
		int th = NextPowerOfTwo(h);

		// Capture current screen
		glBindTexture(GL_TEXTURE_2D, g_ScreenTexture);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);

		// Push OpenGL state
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);

		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glMatrixMode(GL_TEXTURE);
		glPushMatrix();

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();

		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_NORMAL_ARRAY);

		glDisable(GL_DEPTH_TEST);
		glDisable(GL_ALPHA_TEST);
		glDisable(GL_LIGHTING);
		glDisable(GL_CULL_FACE);
		glDisable(GL_SCISSOR_TEST);

		glActiveTexture(GL_TEXTURE1);
		glDisable(GL_TEXTURE_2D);
		glActiveTexture(GL_TEXTURE2);
		glDisable(GL_TEXTURE_2D);
		glActiveTexture(GL_TEXTURE0);
		glEnable(GL_TEXTURE_2D);

		glBindTexture(GL_TEXTURE_2D, g_ScreenTexture);

		float u = (float)w / tw;
		float v = (float)h / th;

		float u_vel = (vx / w) * u;
		float v_vel = (vy / h) * v;

		float chromatic = cl_motion_blur_chromatic ? cl_motion_blur_chromatic->value : 0.0f;

		// 7 samples is the sweet spot for smooth directional blur on modern GPUs
		const int samples = 7;

		for (int i = 1; i <= samples; i++)
		{
			float t = ((float)(i - 1) / (float)(samples - 1)) - 0.5f; // -0.5 to 0.5

			if (i == 1)
			{
				glDisable(GL_BLEND);
				glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
			}
			else
			{
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				float alpha = 1.0f / (float)i;
				glColor4f(1.0f, 1.0f, 1.0f, alpha);
			}

			if (chromatic > 0.0f)
			{
				// Red Channel Pass (slightly larger offset)
				glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
				float r_scale = 1.0f + (chromatic * 0.05f);
				float offset_u_r = t * u_vel * r_scale;
				float offset_v_r = t * v_vel * r_scale;
				glBegin(GL_QUADS);
				glTexCoord2f(0.0f + offset_u_r, 0.0f + offset_v_r); glVertex2f(0.0f, 0.0f);
				glTexCoord2f(u + offset_u_r,    0.0f + offset_v_r); glVertex2f(1.0f, 0.0f);
				glTexCoord2f(u + offset_u_r,    v + offset_v_r);    glVertex2f(1.0f, 1.0f);
				glTexCoord2f(0.0f + offset_u_r, v + offset_v_r);    glVertex2f(0.0f, 1.0f);
				glEnd();

				// Green Channel Pass (normal offset)
				glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
				float offset_u_g = t * u_vel;
				float offset_v_g = t * v_vel;
				glBegin(GL_QUADS);
				glTexCoord2f(0.0f + offset_u_g, 0.0f + offset_v_g); glVertex2f(0.0f, 0.0f);
				glTexCoord2f(u + offset_u_g,    0.0f + offset_v_g); glVertex2f(1.0f, 0.0f);
				glTexCoord2f(u + offset_u_g,    v + offset_v_g);    glVertex2f(1.0f, 1.0f);
				glTexCoord2f(0.0f + offset_u_g, v + offset_v_g);    glVertex2f(0.0f, 1.0f);
				glEnd();

				// Blue Channel Pass (slightly smaller offset)
				glColorMask(GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE);
				float b_scale = 1.0f - (chromatic * 0.05f);
				float offset_u_b = t * u_vel * b_scale;
				float offset_v_b = t * v_vel * b_scale;
				glBegin(GL_QUADS);
				glTexCoord2f(0.0f + offset_u_b, 0.0f + offset_v_b); glVertex2f(0.0f, 0.0f);
				glTexCoord2f(u + offset_u_b,    0.0f + offset_v_b); glVertex2f(1.0f, 0.0f);
				glTexCoord2f(u + offset_u_b,    v + offset_v_b);    glVertex2f(1.0f, 1.0f);
				glTexCoord2f(0.0f + offset_u_b, v + offset_v_b);    glVertex2f(0.0f, 1.0f);
				glEnd();

				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			}
			else
			{
				float offset_u = t * u_vel;
				float offset_v = t * v_vel;

				glBegin(GL_QUADS);
				glTexCoord2f(0.0f + offset_u, 0.0f + offset_v); glVertex2f(0.0f, 0.0f);
				glTexCoord2f(u + offset_u,    0.0f + offset_v); glVertex2f(1.0f, 0.0f);
				glTexCoord2f(u + offset_u,    v + offset_v);    glVertex2f(1.0f, 1.0f);
				glTexCoord2f(0.0f + offset_u, v + offset_v);    glVertex2f(0.0f, 1.0f);
				glEnd();
			}
		}

		// Restore OpenGL state
		glMatrixMode(GL_TEXTURE);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();

		glPopClientAttrib();
		glPopAttrib();
	}
}
