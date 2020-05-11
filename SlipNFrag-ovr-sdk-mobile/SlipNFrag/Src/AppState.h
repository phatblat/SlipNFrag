//
// Created by Heriberto Delgado on 5/10/20.
//

#include "VrApi.h"
#include <android_native_app_glue.h>
#include "AppMode.h"
#include <EGL/egl.h>
#include <vector>
#include <GLES3/gl3.h>

struct AppState
{
	ovrJava Java;
	ANativeWindow *NativeWindow;
	AppMode Mode;
	AppMode PreviousMode;
	bool StartupButtonsPressed;
	bool Resumed;
	double PausedTime;
	ovrMobile *Ovr;
	uint64_t FrameIndex;
	double DisplayTime;
	int SwapInterval;
	int CpuLevel;
	int GpuLevel;
	uint32_t ThreadId;
	EGLDisplay Display;
	EGLConfig Config;
	EGLSurface TinySurface;
	EGLContext Context;
	bool CreatedScene;
	ovrTextureSwapChain *CylinderSwapChain;
	int CylinderWidth;
	int CylinderHeight;
	std::vector<uint32_t> CylinderTexData;
	GLuint CylinderTexId;
	bool FirstFrame;
	double PreviousTime;
	double CurrentTime;
	GLuint Framebuffer;
	GLuint Program;
	GLuint VertexShader;
	GLuint FragmentShader;
	GLint InputPositionAttrib;
	GLint InputTexCoordsAttrib;
	GLuint ScreenTexId;
	GLuint ConsoleTexId;
	GLuint PaletteTexId;
	uint32_t PreviousLeftButtons;
	uint32_t PreviousRightButtons;
	uint32_t LeftButtons;
	uint32_t RightButtons;
	ovrVector2f PreviousLeftJoystick;
	ovrVector2f PreviousRightJoystick;
	ovrVector2f LeftJoystick;
	ovrVector2f RightJoystick;
};

