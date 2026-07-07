#pragma once

namespace post_process
{
	void init();
	void capture_depth();
	void draw();

	// Debug info — exposed to ImGui
	struct DepthDebugInfo {
		bool   capture_attempted;    // Was capture_depth() called this frame?
		bool   capture_succeeded;    // Did depth read actually work?
		int    method_used;          // 0=none, 1=glReadPixels FLOAT, 2=glReadPixels UINT, 3=glCopyTexSubImage2D
		int    gl_error;             // Last GL error (0 = no error)
		int    viewport_w, viewport_h;
		float  center_depth;         // Depth value at screen center [0..1]
		float  sample_depths[9];     // 3x3 grid of depth samples around center
		int    frame_count;          // How many frames we've captured
	};
	const DepthDebugInfo& get_depth_debug();
}
