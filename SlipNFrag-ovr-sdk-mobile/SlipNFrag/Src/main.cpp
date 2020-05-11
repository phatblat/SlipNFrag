#include "AppState.h"
#include <android/window.h>
#include <sys/prctl.h>
#include "VrApi_Helpers.h"
#include <android/log.h>
#include "sys_ovr.h"
#include <EGL/eglext.h>
#include <unistd.h>
#include "vid_ovr.h"
#include "in_ovr.h"
#include "VrApi_Input.h"
#include "stb_image.h"

#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812D
#endif

#ifndef GL_TEXTURE_BORDER_COLOR
#define GL_TEXTURE_BORDER_COLOR 0x1004
#endif

static const int CPU_LEVEL = 2;
static const int GPU_LEVEL = 3;

static const char* eglErrorString()
{
	auto error = eglGetError();
	switch (error)
	{
		case EGL_SUCCESS:
			return "EGL_SUCCESS";
		case EGL_NOT_INITIALIZED:
			return "EGL_NOT_INITIALIZED";
		case EGL_BAD_ACCESS:
			return "EGL_BAD_ACCESS";
		case EGL_BAD_ALLOC:
			return "EGL_BAD_ALLOC";
		case EGL_BAD_ATTRIBUTE:
			return "EGL_BAD_ATTRIBUTE";
		case EGL_BAD_CONTEXT:
			return "EGL_BAD_CONTEXT";
		case EGL_BAD_CONFIG:
			return "EGL_BAD_CONFIG";
		case EGL_BAD_CURRENT_SURFACE:
			return "EGL_BAD_CURRENT_SURFACE";
		case EGL_BAD_DISPLAY:
			return "EGL_BAD_DISPLAY";
		case EGL_BAD_SURFACE:
			return "EGL_BAD_SURFACE";
		case EGL_BAD_MATCH:
			return "EGL_BAD_MATCH";
		case EGL_BAD_PARAMETER:
			return "EGL_BAD_PARAMETER";
		case EGL_BAD_NATIVE_PIXMAP:
			return "EGL_BAD_NATIVE_PIXMAP";
		case EGL_BAD_NATIVE_WINDOW:
			return "EGL_BAD_NATIVE_WINDOW";
		case EGL_CONTEXT_LOST:
			return "EGL_CONTEXT_LOST";
	}
	return "";
}

double getTime()
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (now.tv_sec * 1e9 + now.tv_nsec) * 0.000000001;
}

static void appHandleCommands(struct android_app *app, int32_t cmd)
{
	auto appState = (AppState *)app->userData;
	double delta;
	switch (cmd)
	{
		case APP_CMD_PAUSE:
			appState->Resumed = false;
			appState->PausedTime = getTime();
			break;
		case APP_CMD_RESUME:
			appState->Resumed = true;
			delta = getTime() - appState->PausedTime;
			if (appState->PreviousTime > 0)
			{
				appState->PreviousTime += delta;
			}
			if (appState->CurrentTime > 0)
			{
				appState->CurrentTime += delta;
			}
			break;
		case APP_CMD_INIT_WINDOW:
			appState->NativeWindow = app->window;
			break;
		case APP_CMD_TERM_WINDOW:
			appState->NativeWindow = NULL;
			break;
		case APP_CMD_DESTROY:
			exit(0);
	}
}

bool leftButtonIsDown(AppState& appState, uint32_t button)
{
	return ((appState.LeftButtons & button) != 0 && (appState.PreviousLeftButtons & button) == 0);
}

bool leftButtonIsUp(AppState& appState, uint32_t button)
{
	return ((appState.LeftButtons & button) == 0 && (appState.PreviousLeftButtons & button) != 0);
}

bool rightButtonIsDown(AppState& appState, uint32_t button)
{
	return ((appState.RightButtons & button) != 0 && (appState.PreviousRightButtons & button) == 0);
}

bool rightButtonIsUp(AppState& appState, uint32_t button)
{
	return ((appState.RightButtons & button) == 0 && (appState.PreviousRightButtons & button) != 0);
}

void android_main(struct android_app *app)
{
	ANativeActivity_setWindowFlags(app->activity, AWINDOW_FLAG_KEEP_SCREEN_ON, 0);
	ovrJava java{app->activity->vm};
	java.Vm->AttachCurrentThread(&java.Env, nullptr);
	java.ActivityObject = app->activity->clazz;
	prctl(PR_SET_NAME, (long) "SlipNFrag", 0, 0, 0);
	const auto initParams = vrapi_DefaultInitParms(&java);
	auto initResult = vrapi_Initialize(&initParams);
	if (initResult != VRAPI_INITIALIZE_SUCCESS)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main: vrapi_Initialize() failed: %i", initResult);
		exit(0);
	}
	AppState appState { };
	appState.FrameIndex = 1;
	appState.SwapInterval = 1;
	appState.CpuLevel = 2;
	appState.GpuLevel = 2;
	appState.Java = java;
	appState.PausedTime = -1;
	appState.PreviousTime = -1;
	appState.CurrentTime = -1;
	appState.Display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	eglInitialize(appState.Display, nullptr, nullptr);
	const auto MAX_CONFIGS = 1024;
	std::vector<EGLConfig> configs(MAX_CONFIGS);
	EGLint numConfigs = 0;
	if (eglGetConfigs(appState.Display, configs.data(), MAX_CONFIGS, &numConfigs) == EGL_FALSE)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main: eglGetConfigs() failed: %s", eglErrorString());
		exit(0);
	}
	const EGLint configAttributes[] = {EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0, EGL_SAMPLES, 0, EGL_NONE};
	for (int i = 0; i < numConfigs; i++)
	{
		EGLint value = 0;
		eglGetConfigAttrib(appState.Display, configs[i], EGL_RENDERABLE_TYPE, &value);
		if ((value & EGL_OPENGL_ES3_BIT_KHR) != EGL_OPENGL_ES3_BIT_KHR)
		{
			continue;
		}
		eglGetConfigAttrib(appState.Display, configs[i], EGL_SURFACE_TYPE, &value);
		if ((value & (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) != (EGL_WINDOW_BIT | EGL_PBUFFER_BIT))
		{
			continue;
		}
		auto j = 0;
		for (; configAttributes[j] != EGL_NONE; j += 2)
		{
			eglGetConfigAttrib(appState.Display, configs[i], configAttributes[j], &value);
			if (value != configAttributes[j + 1])
			{
				break;
			}
		}
		if (configAttributes[j] == EGL_NONE)
		{
			appState.Config = configs[i];
			break;
		}
	}
	if (appState.Config == 0)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main: eglChooseConfig() failed: %s", eglErrorString());
		exit(0);
	}
	EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
	appState.Context = eglCreateContext(appState.Display, appState.Config, EGL_NO_CONTEXT, contextAttributes);
	if (appState.Context == EGL_NO_CONTEXT)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main: eglCreateContext() failed: %s", eglErrorString());
		exit(0);
	}
	const EGLint surfaceAttributes[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
	appState.TinySurface = eglCreatePbufferSurface(appState.Display, appState.Config, surfaceAttributes);
	if (appState.TinySurface == EGL_NO_SURFACE)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main: eglCreatePbufferSurface() failed: %s", eglErrorString());
		eglDestroyContext(appState.Display, appState.Context);
		exit(0);
	}
	if (eglMakeCurrent(appState.Display, appState.TinySurface, appState.TinySurface, appState.Context) == EGL_FALSE)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main: eglMakeCurrent() failed: %s", eglErrorString());
		eglDestroySurface(appState.Display, appState.TinySurface);
		eglDestroyContext(appState.Display, appState.Context);
		exit(0);
	}
	appState.CpuLevel = CPU_LEVEL;
	appState.GpuLevel = GPU_LEVEL;
	appState.ThreadId = (uint32_t)gettid();
	app->userData = &appState;
	app->onAppCmd = appHandleCommands;
	while (app->destroyRequested == 0)
	{
		for (;;)
		{
			int events;
			struct android_poll_source *source;
			const int timeoutMilliseconds = (appState.Ovr == NULL && app->destroyRequested == 0) ? -1 : 0;
			if (ALooper_pollAll(timeoutMilliseconds, NULL, &events, (void **) &source) < 0)
			{
				break;
			}
			if (source != NULL)
			{
				source->process(app, source);
			}
			if (appState.Resumed && appState.NativeWindow != NULL)
			{
				if (appState.Ovr == NULL)
				{
					ovrModeParms params = vrapi_DefaultModeParms(&appState.Java);
					params.Flags &= ~VRAPI_MODE_FLAG_RESET_WINDOW_FULLSCREEN;
					params.Flags |= VRAPI_MODE_FLAG_NATIVE_WINDOW;
					params.Display = (size_t) appState.Display;
					params.WindowSurface = (size_t) appState.NativeWindow;
					params.ShareContext = (size_t) appState.Context;
					appState.Ovr = vrapi_EnterVrMode(&params);
					if (appState.Ovr == NULL)
					{
						__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main: vrapi_EnterVRMode() failed: invalid ANativeWindow");
						appState.NativeWindow = NULL;
					}
					{
						vrapi_SetClockLevels(appState.Ovr, appState.CpuLevel, appState.GpuLevel);
						vrapi_SetPerfThread(appState.Ovr, VRAPI_PERF_THREAD_TYPE_MAIN, appState.ThreadId);
					}
				}
			}
			else
			{
				if (appState.Ovr != NULL)
				{
					vrapi_LeaveVrMode(appState.Ovr);
					appState.Ovr = NULL;
				}
			}
		}
		auto leftRemoteFound = false;
		auto rightRemoteFound = false;
		appState.LeftButtons = 0;
		appState.RightButtons = 0;
		appState.LeftJoystick.x = 0;
		appState.LeftJoystick.y = 0;
		appState.RightJoystick.x = 0;
		appState.RightJoystick.y = 0;
		for (auto i = 0; ; i++)
		{
			ovrInputCapabilityHeader cap;
			ovrResult result = vrapi_EnumerateInputDevices(appState.Ovr, i, &cap);
			if (result < 0)
			{
				break;
			}
			if (cap.Type == ovrControllerType_TrackedRemote)
			{
				ovrInputStateTrackedRemote trackedRemoteState;
				trackedRemoteState.Header.ControllerType = ovrControllerType_TrackedRemote;
				result = vrapi_GetCurrentInputState(appState.Ovr, cap.DeviceID, &trackedRemoteState.Header);
				if (result == ovrSuccess)
				{
					result = vrapi_GetInputDeviceCapabilities(appState.Ovr, &cap);
					if (result == ovrSuccess)
					{
						auto trCap = (ovrInputTrackedRemoteCapabilities*) &cap;
						if ((trCap->ControllerCapabilities & ovrControllerCaps_LeftHand) != 0)
						{
							leftRemoteFound = true;
							appState.LeftButtons = trackedRemoteState.Buttons;
							appState.LeftJoystick = trackedRemoteState.Joystick;
						}
						else if ((trCap->ControllerCapabilities & ovrControllerCaps_RightHand) != 0)
						{
							rightRemoteFound = true;
							appState.RightButtons = trackedRemoteState.Buttons;
							appState.RightJoystick = trackedRemoteState.Joystick;
						}
					}
				}
			}
		}
		if (leftRemoteFound && rightRemoteFound)
		{
			if (appState.Mode == AppStartupMode)
			{
				if (appState.StartupButtonsPressed)
				{
					if ((appState.LeftButtons & ovrButton_X) == 0 && (appState.RightButtons & ovrButton_A) == 0)
					{
						appState.Mode = AppCylinderMode;
						appState.StartupButtonsPressed = false;
					}
				}
				else if ((appState.LeftButtons & ovrButton_X) != 0 && (appState.RightButtons & ovrButton_A) != 0)
				{
					appState.StartupButtonsPressed = true;
				}
			}
			else if (appState.Mode == AppCylinderMode)
			{
				if (host_initialized)
				{
					if (leftButtonIsDown(appState, ovrButton_Enter))
					{
						Key_Event(K_ESCAPE, true);
					}
					if (leftButtonIsUp(appState, ovrButton_Enter))
					{
						Key_Event(K_ESCAPE, false);
					}
					if (key_dest == key_game)
					{
						if (joy_initialized)
						{
							joy_avail = true;
							pdwRawValue[JOY_AXIS_X] = -appState.LeftJoystick.x;
							pdwRawValue[JOY_AXIS_Y] = -appState.LeftJoystick.y;
							pdwRawValue[JOY_AXIS_Z] = appState.RightJoystick.x;
							pdwRawValue[JOY_AXIS_R] = appState.RightJoystick.y;
						}
						if (leftButtonIsDown(appState, ovrButton_Trigger) || rightButtonIsDown(appState, ovrButton_Trigger))
						{
							Cmd_ExecuteString("+attack", src_command);
						}
						if (leftButtonIsUp(appState, ovrButton_Trigger) || rightButtonIsUp(appState, ovrButton_Trigger))
						{
							Cmd_ExecuteString("-attack", src_command);
						}
						if (leftButtonIsDown(appState, ovrButton_GripTrigger) || rightButtonIsDown(appState, ovrButton_GripTrigger))
						{
							Cmd_ExecuteString("+speed", src_command);
						}
						if (leftButtonIsUp(appState, ovrButton_GripTrigger) || rightButtonIsUp(appState, ovrButton_GripTrigger))
						{
							Cmd_ExecuteString("-speed", src_command);
						}
						if (leftButtonIsDown(appState, ovrButton_Joystick))
						{
							Cmd_ExecuteString("impulse 10", src_command);
						}
						if (leftButtonIsDown(appState, ovrButton_Y) || rightButtonIsDown(appState, ovrButton_B))
						{
							Cmd_ExecuteString("+jump", src_command);
						}
						if (leftButtonIsUp(appState, ovrButton_Y) || rightButtonIsUp(appState, ovrButton_B))
						{
							Cmd_ExecuteString("-jump", src_command);
						}
						if (leftButtonIsDown(appState, ovrButton_X) || rightButtonIsDown(appState, ovrButton_A))
						{
							Cmd_ExecuteString("+movedown", src_command);
						}
						if (leftButtonIsUp(appState, ovrButton_X) || rightButtonIsUp(appState, ovrButton_A))
						{
							Cmd_ExecuteString("-movedown", src_command);
						}
						if (rightButtonIsDown(appState, ovrButton_Joystick))
						{
							Cmd_ExecuteString("+mlook", src_command);
						}
						if (rightButtonIsUp(appState, ovrButton_Joystick))
						{
							Cmd_ExecuteString("-mlook", src_command);
						}
					}
					else
					{
						if ((appState.LeftJoystick.y > 0.5 && appState.PreviousLeftJoystick.y <= 0.5) || (appState.RightJoystick.y > 0.5 && appState.PreviousRightJoystick.y <= 0.5))
						{
							Key_Event(K_UPARROW, true);
						}
						if ((appState.LeftJoystick.y <= 0.5 && appState.PreviousLeftJoystick.y > 0.5) || (appState.RightJoystick.y <= 0.5 && appState.PreviousRightJoystick.y > 0.5))
						{
							Key_Event(K_UPARROW, false);
						}
						if ((appState.LeftJoystick.y < -0.5 && appState.PreviousLeftJoystick.y >= -0.5) || (appState.RightJoystick.y < -0.5 && appState.PreviousRightJoystick.y >= -0.5))
						{
							Key_Event(K_DOWNARROW, true);
						}
						if ((appState.LeftJoystick.y >= -0.5 && appState.PreviousLeftJoystick.y < -0.5) || (appState.RightJoystick.y >= -0.5 && appState.PreviousRightJoystick.y < -0.5))
						{
							Key_Event(K_DOWNARROW, false);
						}
						if ((appState.LeftJoystick.x > 0.5 && appState.PreviousLeftJoystick.x <= 0.5) || (appState.RightJoystick.x > 0.5 && appState.PreviousRightJoystick.x <= 0.5))
						{
							Key_Event(K_RIGHTARROW, true);
						}
						if ((appState.LeftJoystick.x <= 0.5 && appState.PreviousLeftJoystick.x > 0.5) || (appState.RightJoystick.x <= 0.5 && appState.PreviousRightJoystick.x > 0.5))
						{
							Key_Event(K_RIGHTARROW, false);
						}
						if ((appState.LeftJoystick.x < -0.5 && appState.PreviousLeftJoystick.x >= -0.5) || (appState.RightJoystick.x < -0.5 && appState.PreviousRightJoystick.x >= -0.5))
						{
							Key_Event(K_LEFTARROW, true);
						}
						if ((appState.LeftJoystick.x >= -0.5 && appState.PreviousLeftJoystick.x < -0.5) || (appState.RightJoystick.x >= -0.5 && appState.PreviousRightJoystick.x < -0.5))
						{
							Key_Event(K_LEFTARROW, false);
						}
						if (leftButtonIsDown(appState, ovrButton_Trigger) || leftButtonIsDown(appState, ovrButton_X) || rightButtonIsDown(appState, ovrButton_Trigger) || rightButtonIsDown(appState, ovrButton_A))
						{
							Key_Event(K_ENTER, true);
						}
						if (leftButtonIsUp(appState, ovrButton_Trigger) || leftButtonIsUp(appState, ovrButton_X) || rightButtonIsUp(appState, ovrButton_Trigger) || rightButtonIsUp(appState, ovrButton_A))
						{
							Key_Event(K_ENTER, false);
						}
						if (leftButtonIsDown(appState, ovrButton_GripTrigger) || leftButtonIsDown(appState, ovrButton_Y) || rightButtonIsDown(appState, ovrButton_GripTrigger) || rightButtonIsDown(appState, ovrButton_B))
						{
							Key_Event(K_ESCAPE, true);
						}
						if (leftButtonIsUp(appState, ovrButton_GripTrigger) || leftButtonIsUp(appState, ovrButton_Y) || rightButtonIsUp(appState, ovrButton_GripTrigger) || rightButtonIsUp(appState, ovrButton_B))
						{
							Key_Event(K_ESCAPE, false);
						}
					}
				}
			}
		}
		else
		{
			joy_avail = false;
		}
		if (leftRemoteFound)
		{
			appState.PreviousLeftButtons = appState.LeftButtons;
			appState.PreviousLeftJoystick = appState.LeftJoystick;
		}
		else
		{
			appState.PreviousLeftButtons = 0;
			appState.PreviousLeftJoystick.x = 0;
			appState.PreviousLeftJoystick.y = 0;
		}
		if (rightRemoteFound)
		{
			appState.PreviousRightButtons = appState.RightButtons;
			appState.PreviousRightJoystick = appState.RightJoystick;
		}
		else
		{
			appState.PreviousRightButtons = 0;
			appState.PreviousRightJoystick.x = 0;
			appState.PreviousRightJoystick.y = 0;
		}
		if (appState.Ovr == NULL)
		{
			continue;
		}
		if (!appState.CreatedScene)
		{
			auto frameFlags = 0;
			frameFlags |= VRAPI_FRAME_FLAG_FLUSH;
			ovrLayerProjection2 blackLayer = vrapi_DefaultLayerBlackProjection2();
			blackLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;
			ovrLayerLoadingIcon2 iconLayer = vrapi_DefaultLayerLoadingIcon2();
			iconLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;
			const ovrLayerHeader2 *layers[] = { &blackLayer.Header, &iconLayer.Header };
			ovrSubmitFrameDescription2 frameDesc = {0};
			frameDesc.Flags = frameFlags;
			frameDesc.SwapInterval = 1;
			frameDesc.FrameIndex = appState.FrameIndex;
			frameDesc.DisplayTime = appState.DisplayTime;
			frameDesc.LayerCount = 2;
			frameDesc.Layers = layers;
			vrapi_SubmitFrame2(appState.Ovr, &frameDesc);
			appState.CylinderWidth = 640;
			appState.CylinderHeight = 400;
			appState.CylinderSwapChain = vrapi_CreateTextureSwapChain3(VRAPI_TEXTURE_TYPE_2D, GL_RGBA8, appState.CylinderWidth, appState.CylinderHeight, 1, 1);
			appState.CylinderTexData.resize(appState.CylinderWidth * appState.CylinderHeight);
			auto playImageFile = AAssetManager_open(app->activity->assetManager, "play.png", AASSET_MODE_BUFFER);
			auto playImageFileLength = AAsset_getLength(playImageFile);
			std::vector<stbi_uc> playImageSource(playImageFileLength);
			AAsset_read(playImageFile, playImageSource.data(), playImageFileLength);
			int playImageWidth;
			int playImageHeight;
			int playImageComponents;
			auto playImage = stbi_load_from_memory(playImageSource.data(), playImageFileLength, &playImageWidth, &playImageHeight, &playImageComponents, 4);
			auto texIndex = ((appState.CylinderHeight - playImageHeight) * appState.CylinderWidth + appState.CylinderWidth - playImageWidth) / 2;
			auto playIndex = 0;
			for (auto y = 0; y < playImageHeight; y++)
			{
				for (auto x = 0; x < playImageWidth; x++)
				{
					auto r = playImage[playIndex];
					playIndex++;
					auto g = playImage[playIndex];
					playIndex++;
					auto b = playImage[playIndex];
					playIndex++;
					auto a = playImage[playIndex];
					playIndex++;
					auto factor = (double)a / 255;
					r = (unsigned char)((double)r * factor);
					g = (unsigned char)((double)g * factor);
					b = (unsigned char)((double)b * factor);
					appState.CylinderTexData[texIndex] = ((uint32_t)255 << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
					texIndex++;
				}
				texIndex += appState.CylinderWidth - playImageWidth;
			}
			stbi_image_free(playImage);
			for (auto b = 0; b < 5; b++)
			{
				auto i = (unsigned char)(192.0 * sin(M_PI / (double)(b - 1)));
				auto color = ((uint32_t)255 << 24) | ((uint32_t)i << 16) | ((uint32_t)i << 8) | i;
				auto texTopIndex = b * appState.CylinderWidth + b;
				auto texBottomIndex = (appState.CylinderHeight - 1 - b) * appState.CylinderWidth + b;
				for (auto x = 0; x < appState.CylinderWidth - b - b; x++)
				{
					appState.CylinderTexData[texTopIndex] = color;
					texTopIndex++;
					appState.CylinderTexData[texBottomIndex] = color;
					texBottomIndex++;
				}
				auto texLeftIndex = (b + 1) * appState.CylinderWidth + b;
				auto texRightIndex = (b + 1) * appState.CylinderWidth + appState.CylinderWidth - 1 - b;
				for (auto y = 0; y < appState.CylinderHeight - b - 1 - b - 1; y++)
				{
					appState.CylinderTexData[texLeftIndex] = color;
					texLeftIndex += appState.CylinderWidth;
					appState.CylinderTexData[texRightIndex] = color;
					texRightIndex += appState.CylinderWidth;
				}
			}
			appState.CylinderTexId = vrapi_GetTextureSwapChainHandle(appState.CylinderSwapChain, 0);
			glBindTexture(GL_TEXTURE_2D, appState.CylinderTexId);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, appState.CylinderWidth, appState.CylinderHeight, GL_RGBA, GL_UNSIGNED_BYTE, appState.CylinderTexData.data());
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
			GLfloat borderColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
			glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
			glBindTexture(GL_TEXTURE_2D, 0);
			glGenFramebuffers(1, &appState.Framebuffer);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, appState.Framebuffer);
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, appState.CylinderTexId, 0);
			auto status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
			if (status != GL_FRAMEBUFFER_COMPLETE)
			{
				__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main: Incomplete frame buffer object: %i", status);
				exit(0);
			}
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			auto vertexShaderFile = AAssetManager_open(app->activity->assetManager, "VertexShader.vert", AASSET_MODE_BUFFER);
			GLint vertexShaderFileLength = AAsset_getLength(vertexShaderFile);
			std::vector<GLchar> vertexShaderSource(vertexShaderFileLength);
			GLchar* vertexShaderSourcePtr = vertexShaderSource.data();
			AAsset_read(vertexShaderFile, vertexShaderSourcePtr, vertexShaderFileLength);
			auto pixelShaderFile = AAssetManager_open(app->activity->assetManager, "PixelShader.frag", AASSET_MODE_BUFFER);
			GLint pixelShaderFileLength = AAsset_getLength(pixelShaderFile);
			std::vector<GLchar> pixelShaderSource(pixelShaderFileLength);
			GLchar* pixelShaderSourcePtr = pixelShaderSource.data();
			AAsset_read(pixelShaderFile, pixelShaderSourcePtr, pixelShaderFileLength);
			appState.VertexShader = glCreateShader(GL_VERTEX_SHADER);
			glShaderSource(appState.VertexShader, 1, &vertexShaderSourcePtr, &vertexShaderFileLength);
			glCompileShader(appState.VertexShader);
			GLint result;
			glGetShaderiv(appState.VertexShader, GL_COMPILE_STATUS, &result);
			if (result == GL_FALSE)
			{
				GLint infoLogLength;
				glGetShaderiv(appState.VertexShader, GL_INFO_LOG_LENGTH, &infoLogLength);
				std::vector<GLchar> infoLog(infoLogLength);
				glGetShaderInfoLog(appState.VertexShader, infoLogLength, nullptr, infoLog.data());
				__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main: glCompileShader() for vertex shader failed: %s", infoLog.data());
				exit(0);
			}
			appState.FragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
			glShaderSource(appState.FragmentShader, 1, &pixelShaderSourcePtr, &pixelShaderFileLength);
			glCompileShader(appState.FragmentShader);
			glGetShaderiv(appState.FragmentShader, GL_COMPILE_STATUS, &result);
			if (result == GL_FALSE)
			{
				GLint infoLogLength;
				glGetShaderiv(appState.FragmentShader, GL_INFO_LOG_LENGTH, &infoLogLength);
				std::vector<GLchar> infoLog(infoLogLength);
				glGetShaderInfoLog(appState.FragmentShader, infoLogLength, nullptr, infoLog.data());
				__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main: glCompileShader() for pixel shader failed: %s", infoLog.data());
				exit(0);
			}
			appState.Program = glCreateProgram();
			glAttachShader(appState.Program, appState.VertexShader);
			glAttachShader(appState.Program, appState.FragmentShader);
			glLinkProgram(appState.Program);
			glGetProgramiv(appState.Program, GL_LINK_STATUS, &result);
			if (result == GL_FALSE)
			{
				GLint infoLogLength;
				glGetProgramiv(appState.Program, GL_INFO_LOG_LENGTH, &infoLogLength);
				std::vector<GLchar> infoLog(infoLogLength);
				glGetProgramInfoLog(appState.Program, infoLogLength, nullptr, infoLog.data());
				__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main: glLinkShader() failed: %s", infoLog.data());
				exit(0);
			}
			appState.InputPositionAttrib = glGetAttribLocation(appState.Program, "inputPosition");
			appState.InputTexCoordsAttrib = glGetAttribLocation(appState.Program, "inputTexCoords");
			glUseProgram(appState.Program);
			auto consoleUniform = glGetUniformLocation(appState.Program, "console");
			glUniform1i(consoleUniform, 0);
			auto screenUniform = glGetUniformLocation(appState.Program, "screen");
			glUniform1i(screenUniform, 1);
			auto paletteUniform = glGetUniformLocation(appState.Program, "palette");
			glUniform1i(paletteUniform, 2);
			glUseProgram(0);
			appState.FirstFrame = true;
			appState.CreatedScene = true;
		}
		if (appState.PreviousMode != appState.Mode)
		{
			if (appState.PreviousMode == AppStartupMode)
			{
				sys_argc = 5;
				sys_argv = new char *[sys_argc];
				sys_argv[0] = (char *) "SlipNFrag";
				sys_argv[1] = (char *) "-basedir";
				sys_argv[2] = (char *) "/sdcard/android/data/com.heribertodelgado.slipnfrag/files";
				sys_argv[3] = (char *) "+map";
				sys_argv[4] = (char *) "start";
				Sys_Init(sys_argc, sys_argv);
				if (sys_errormessage.length() > 0)
				{
					if (sys_nogamedata)
					{
						__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Game data not found. Upload the game data files to your headset and try again.");
					}
					exit(0);
				}
				appState.PreviousLeftButtons = 0;
				appState.PreviousRightButtons = 0;
			}
			if (appState.Mode == AppCylinderMode)
			{
				Cvar_SetValue("joystick", 1);
				Cvar_SetValue("joyadvanced", 1);
				Cvar_SetValue("joyadvaxisx", AxisSide);
				Cvar_SetValue("joyadvaxisy", AxisForward);
				Cvar_SetValue("joyadvaxisz", AxisTurn);
				Cvar_SetValue("joyadvaxisr", AxisLook);
				Joy_AdvancedUpdate_f();
			}
			appState.PreviousMode = appState.Mode;
		}
		appState.FrameIndex++;
		const auto predictedDisplayTime = vrapi_GetPredictedDisplayTime(appState.Ovr, appState.FrameIndex);
		const auto tracking = vrapi_GetPredictedTracking2(appState.Ovr, predictedDisplayTime);
		appState.DisplayTime = predictedDisplayTime;
		ovrTracking2 updatedTracking = tracking;
		if (host_initialized)
		{
			if (appState.PreviousTime < 0)
			{
				struct timespec now;
				clock_gettime(CLOCK_MONOTONIC, &now);
				appState.PreviousTime = (now.tv_sec * 1e9 + now.tv_nsec) * 0.000000001;
			}
			else if (appState.CurrentTime < 0)
			{
				struct timespec now;
				clock_gettime(CLOCK_MONOTONIC, &now);
				appState.CurrentTime = (now.tv_sec * 1e9 + now.tv_nsec) * 0.000000001;
			}
			else
			{
				appState.PreviousTime = appState.CurrentTime;
				struct timespec now;
				clock_gettime(CLOCK_MONOTONIC, &now);
				appState.CurrentTime = (now.tv_sec * 1e9 + now.tv_nsec) * 0.000000001;
				frame_lapse = (float) (appState.CurrentTime - appState.PreviousTime);
			}
			if (appState.FirstFrame)
			{
				vid_width = appState.CylinderWidth;
				vid_height = appState.CylinderHeight;
				auto factor = (double) vid_width / 320;
				double new_conwidth = 320;
				auto new_conheight = ceil((double) vid_height / factor);
				if (new_conheight < 200)
				{
					factor = (double) vid_height / 200;
					new_conheight = 200;
					new_conwidth = (double) (((int) ceil((double) vid_width / factor) + 3) & ~3);
				}
				con_width = (int) new_conwidth;
				con_height = (int) new_conheight;
				VID_Resize();
				glGenTextures(1, &appState.ScreenTexId);
				glBindTexture(GL_TEXTURE_2D, appState.ScreenTexId);
				glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, vid_width, vid_height);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glGenTextures(1, &appState.ConsoleTexId);
				glBindTexture(GL_TEXTURE_2D, appState.ConsoleTexId);
				glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, con_width, con_height);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glGenTextures(1, &appState.PaletteTexId);
				glBindTexture(GL_TEXTURE_2D, appState.PaletteTexId);
				glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 256, 1);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glBindTexture(GL_TEXTURE_2D, 0);
				appState.FirstFrame = false;
			}
			if (r_cache_thrash)
			{
				VID_ReallocSurfCache();
			}
			Sys_Frame(frame_lapse);
			if (sys_errormessage.length() > 0)
			{
				exit(0);
			}
			if (appState.Mode == AppCylinderMode)
			{
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, appState.Framebuffer);
				glUseProgram(appState.Program);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, appState.ConsoleTexId);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, con_width, con_height, GL_RED, GL_UNSIGNED_BYTE, con_buffer.data());
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, appState.ScreenTexId);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, vid_width, vid_height, GL_RED, GL_UNSIGNED_BYTE, vid_buffer.data());
				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, appState.PaletteTexId);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, d_8to24table);
				glViewport(0, 0, vid_width, vid_height);
				GLfloat vertices[] { -1, -1, 0, 1, 1, -1, 0, 1, -1, 1, 0, 1, 1, 1, 0, 1 };
				GLfloat texCoords[] { 0, 0, 1, 0, 0, 1, 1, 1 };
				glVertexAttribPointer(appState.InputPositionAttrib, 4, GL_FLOAT, GL_FALSE, 0, vertices);
				glVertexAttribPointer(appState.InputTexCoordsAttrib, 2, GL_FLOAT, GL_FALSE, 0, texCoords);
				glEnableVertexAttribArray(appState.InputPositionAttrib);
				glEnableVertexAttribArray(appState.InputTexCoordsAttrib);
				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
				glUseProgram(0);
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			}
		}
		auto screenLayer = vrapi_DefaultLayerCylinder2();
		const float fadeLevel = 1.0f;
		screenLayer.Header.ColorScale.x = screenLayer.Header.ColorScale.y = screenLayer.Header.ColorScale.z =
		screenLayer.Header.ColorScale.w = fadeLevel;
		screenLayer.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_SRC_ALPHA;
		screenLayer.Header.DstBlend = VRAPI_FRAME_LAYER_BLEND_ONE_MINUS_SRC_ALPHA;
		screenLayer.HeadPose = updatedTracking.HeadPose;
		const float density = 4500.0f;
		float rotateYaw = 0.0f;
		float rotatePitch = 0.0f;
		const float radius = 1.0f;
		const ovrMatrix4f scaleMatrix = ovrMatrix4f_CreateScale(radius, radius * (float) appState.CylinderHeight * VRAPI_PI / density, radius);
		const ovrMatrix4f rotXMatrix = ovrMatrix4f_CreateRotation(rotatePitch, 0.0f, 0.0f);
		const ovrMatrix4f rotYMatrix = ovrMatrix4f_CreateRotation(0.0f, rotateYaw, 0.0f);
		const ovrMatrix4f m0 = ovrMatrix4f_Multiply(&rotXMatrix, &scaleMatrix);
		ovrMatrix4f cylinderTransform = ovrMatrix4f_Multiply(&rotYMatrix, &m0);
		const float circScale = density * 0.5f / appState.CylinderWidth;
		const float circBias = -circScale * (0.5f * (1.0f - 1.0f / circScale));
		for (auto i = 0; i < VRAPI_FRAME_LAYER_EYE_MAX; i++)
		{
			ovrMatrix4f modelViewMatrix = ovrMatrix4f_Multiply(&updatedTracking.Eye[i].ViewMatrix, &cylinderTransform);
			screenLayer.Textures[i].TexCoordsFromTanAngles = ovrMatrix4f_Inverse(&modelViewMatrix);
			screenLayer.Textures[i].ColorSwapChain = appState.CylinderSwapChain;
			screenLayer.Textures[i].SwapChainIndex = 0;
			const float texScaleX = circScale;
			const float texBiasX = circBias;
			const float texScaleY = 0.5f;
			const float texBiasY = -texScaleY * (0.5f * (1.0f - (1.0f / texScaleY)));
			screenLayer.Textures[i].TextureMatrix.M[0][0] = texScaleX;
			screenLayer.Textures[i].TextureMatrix.M[0][2] = texBiasX;
			screenLayer.Textures[i].TextureMatrix.M[1][1] = texScaleY;
			screenLayer.Textures[i].TextureMatrix.M[1][2] = texBiasY;
			screenLayer.Textures[i].TextureRect.width = 1.0f;
			screenLayer.Textures[i].TextureRect.height = 1.0f;
		}
		const ovrLayerHeader2 *layers[] = {&screenLayer.Header};
		ovrSubmitFrameDescription2 frameDesc = {0};
		frameDesc.Flags = 0;
		frameDesc.SwapInterval = appState.SwapInterval;
		frameDesc.FrameIndex = appState.FrameIndex;
		frameDesc.DisplayTime = appState.DisplayTime;
		frameDesc.LayerCount = 1;
		frameDesc.Layers = layers;
		vrapi_SubmitFrame2(appState.Ovr, &frameDesc);
	}
	if (appState.Display != 0)
	{
		if (eglMakeCurrent(appState.Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_FALSE)
		{
			__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main: eglMakeCurrent() failed: %s", eglGetError());
		}
	}
	if (appState.Context != EGL_NO_CONTEXT)
	{
		if (eglDestroyContext(appState.Display, appState.Context) == EGL_FALSE)
		{
			__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main: eglDestroyContext() failed: %s", eglGetError());
		}
	}
	if (appState.TinySurface != EGL_NO_SURFACE)
	{
		if (eglDestroySurface(appState.Display, appState.TinySurface) == EGL_FALSE)
		{
			__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main: eglDestroySurface() failed: %s", eglGetError());
		}
	}
	if (appState.Display != 0)
	{
		if (eglTerminate(appState.Display) == EGL_FALSE)
		{
			__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main: eglTerminate() failed: %s", eglGetError());
		}
	}
	vrapi_Shutdown();
	java.Vm->DetachCurrentThread();
}
