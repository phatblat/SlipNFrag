#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>
#include <vector>
#include "VrApi_Helpers.h"
#include <android/log.h>
#include "sys_ovr.h"
#include <android_native_app_glue.h>
#include <android/window.h>
#include <sys/prctl.h>
#include "VrApi.h"
#include "VrApi_Vulkan.h"
#include <dlfcn.h>
#include <cerrno>
#include <unistd.h>
#include "VrApi_Input.h"
#include "VrApi_SystemUtils.h"
#include "vid_ovr.h"
#include "in_ovr.h"
#include "stb_image.h"
#include "d_lists.h"

#define DEBUG 1
#define _DEBUG 1

#define VK(func) checkErrors(func, #func);
#define VC(func) func;
#define MAX_UNUSED_COUNT 16

enum AppMode
{
	AppStartupMode,
	AppScreenMode,
	AppWorldMode
};

struct Instance
{
	void *loader;
	VkInstance instance;
	VkBool32 validate;
	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
	PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties;
	PFN_vkCreateInstance vkCreateInstance;
	PFN_vkDestroyInstance vkDestroyInstance;
	PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
	PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures;
	PFN_vkGetPhysicalDeviceFeatures2KHR vkGetPhysicalDeviceFeatures2KHR;
	PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
	PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR;
	PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
	PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
	PFN_vkGetPhysicalDeviceFormatProperties vkGetPhysicalDeviceFormatProperties;
	PFN_vkGetPhysicalDeviceImageFormatProperties vkGetPhysicalDeviceImageFormatProperties;
	PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties;
	PFN_vkEnumerateDeviceLayerProperties vkEnumerateDeviceLayerProperties;
	PFN_vkCreateDevice vkCreateDevice;
	PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
	PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
	PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
	VkDebugReportCallbackEXT debugReportCallback;
};

struct Device
{
	std::vector<const char *> enabledExtensionNames;
	uint32_t enabledLayerCount;
	const char *enabledLayerNames[32];
	VkPhysicalDevice physicalDevice;
	VkPhysicalDeviceFeatures2 physicalDeviceFeatures;
	VkPhysicalDeviceProperties2 physicalDeviceProperties;
	VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
	uint32_t queueFamilyCount;
	VkQueueFamilyProperties *queueFamilyProperties;
	std::vector<uint32_t> queueFamilyUsedQueues;
	pthread_mutex_t queueFamilyMutex;
	int workQueueFamilyIndex;
	int presentQueueFamilyIndex;
	bool supportsMultiview;
	bool supportsFragmentDensity;
	VkDevice device;
	PFN_vkDestroyDevice vkDestroyDevice;
	PFN_vkGetDeviceQueue vkGetDeviceQueue;
	PFN_vkQueueSubmit vkQueueSubmit;
	PFN_vkQueueWaitIdle vkQueueWaitIdle;
	PFN_vkDeviceWaitIdle vkDeviceWaitIdle;
	PFN_vkAllocateMemory vkAllocateMemory;
	PFN_vkFreeMemory vkFreeMemory;
	PFN_vkMapMemory vkMapMemory;
	PFN_vkUnmapMemory vkUnmapMemory;
	PFN_vkFlushMappedMemoryRanges vkFlushMappedMemoryRanges;
	PFN_vkBindBufferMemory vkBindBufferMemory;
	PFN_vkBindImageMemory vkBindImageMemory;
	PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
	PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
	PFN_vkCreateFence vkCreateFence;
	PFN_vkDestroyFence vkDestroyFence;
	PFN_vkResetFences vkResetFences;
	PFN_vkGetFenceStatus vkGetFenceStatus;
	PFN_vkWaitForFences vkWaitForFences;
	PFN_vkCreateBuffer vkCreateBuffer;
	PFN_vkDestroyBuffer vkDestroyBuffer;
	PFN_vkCreateImage vkCreateImage;
	PFN_vkDestroyImage vkDestroyImage;
	PFN_vkCreateImageView vkCreateImageView;
	PFN_vkDestroyImageView vkDestroyImageView;
	PFN_vkCreateShaderModule vkCreateShaderModule;
	PFN_vkDestroyShaderModule vkDestroyShaderModule;
	PFN_vkCreatePipelineCache vkCreatePipelineCache;
	PFN_vkDestroyPipelineCache vkDestroyPipelineCache;
	PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines;
	PFN_vkDestroyPipeline vkDestroyPipeline;
	PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
	PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
	PFN_vkCreateSampler vkCreateSampler;
	PFN_vkDestroySampler vkDestroySampler;
	PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
	PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
	PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
	PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
	PFN_vkResetDescriptorPool vkResetDescriptorPool;
	PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
	PFN_vkFreeDescriptorSets vkFreeDescriptorSets;
	PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
	PFN_vkCreateFramebuffer vkCreateFramebuffer;
	PFN_vkDestroyFramebuffer vkDestroyFramebuffer;
	PFN_vkCreateRenderPass vkCreateRenderPass;
	PFN_vkDestroyRenderPass vkDestroyRenderPass;
	PFN_vkCreateCommandPool vkCreateCommandPool;
	PFN_vkDestroyCommandPool vkDestroyCommandPool;
	PFN_vkResetCommandPool vkResetCommandPool;
	PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
	PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
	PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
	PFN_vkEndCommandBuffer vkEndCommandBuffer;
	PFN_vkResetCommandBuffer vkResetCommandBuffer;
	PFN_vkCmdBindPipeline vkCmdBindPipeline;
	PFN_vkCmdSetViewport vkCmdSetViewport;
	PFN_vkCmdSetScissor vkCmdSetScissor;
	PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
	PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer;
	PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers;
	PFN_vkCmdBlitImage vkCmdBlitImage;
	PFN_vkCmdDrawIndexed vkCmdDrawIndexed;
	PFN_vkCmdCopyBuffer vkCmdCopyBuffer;
	PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage;
	PFN_vkCmdResolveImage vkCmdResolveImage;
	PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
	PFN_vkCmdPushConstants vkCmdPushConstants;
	PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass;
	PFN_vkCmdEndRenderPass vkCmdEndRenderPass;
};

struct Context
{
	Device* device;
	uint32_t queueFamilyIndex;
	uint32_t queueIndex;
	VkQueue queue;
	VkCommandPool commandPool;
	VkPipelineCache pipelineCache;
};

struct Buffer
{
	Buffer* next;
	int unusedCount;
	size_t size;
	VkMemoryPropertyFlags flags;
	VkBuffer buffer;
	VkDeviceMemory memory;
	void* mapped;
};

struct Texture
{
	Texture* next;
	int unusedCount;
	int width;
	int height;
	int layerCount;
	VkImage image;
	VkDeviceMemory memory;
	VkImageView view;
	VkSampler sampler;
};

struct PipelineResources
{
	PipelineResources *next;
	int unusedCount;
	VkDescriptorPool descriptorPool;
	VkDescriptorSet descriptorSet;
};

struct CachedBuffers
{
	Buffer* mapped;
	Buffer* oldMapped;
};

struct CachedTextures
{
	Texture* textures;
	Texture* oldTextures;
};

struct Pipeline
{
	std::vector<VkPipelineShaderStageCreateInfo> stages;
	VkDescriptorSetLayout descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
};

struct Scene
{
	bool createdScene;
	VkShaderModule texturedVertex;
	VkShaderModule texturedFragment;
	VkShaderModule turbulentFragment;
	Pipeline textured;
	Pipeline turbulent;
	int vertexAttributeCount;
	int vertexBindingCount;
	VkVertexInputAttributeDescription vertexAttributes[6];
	VkVertexInputBindingDescription vertexBindings[3];
	VkPipelineVertexInputStateCreateInfo vertexInputState;
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState;
	Buffer matrices;
	int numBuffers;
};

struct PerImage
{
	CachedBuffers sceneMatricesStagingBuffers;
	CachedBuffers texturedVertices;
	CachedBuffers texturedIndices;
	CachedBuffers times;
	CachedBuffers instances;
	CachedBuffers stagingBuffers;
	CachedTextures textures;
	PipelineResources* pipelineResources;
	VkCommandBuffer commandBuffer;
	VkFence fence;
	bool submitted;
};

struct Framebuffer
{
	int swapChainLength;
	ovrTextureSwapChain *colorTextureSwapChain;
	std::vector<Texture> colorTextures;
	std::vector<Texture> fragmentDensityTextures;
	VkImage depthImage;
	VkDeviceMemory depthMemory;
	VkImageView depthImageView;
	Texture renderTexture;
	std::vector<VkFramebuffer> framebuffers;
	int width;
	int height;
	int currentBuffer;
};

struct ColorSwapChain
{
	int SwapChainLength;
	ovrTextureSwapChain *SwapChain;
	std::vector<VkImage> ColorTextures;
	std::vector<VkImage> FragmentDensityTextures;
	std::vector<VkExtent2D> FragmentDensityTextureSizes;
};

struct View
{
	Framebuffer framebuffer;
	ColorSwapChain colorSwapChain;
	int index;
	std::vector<PerImage> perImage;
};

struct Screen
{
	ovrTextureSwapChain* SwapChain;
	int Width;
	int Height;
	std::vector<uint32_t> Data;
	VkImage Image;
	Buffer Buffer;
	VkCommandBuffer CommandBuffer;
	VkSubmitInfo SubmitInfo;
};

struct AppState
{
	ovrJava Java;
	ANativeWindow *NativeWindow;
	AppMode Mode;
	AppMode PreviousMode;
	bool StartupButtonsPressed;
	bool Resumed;
	double PausedTime;
	Device Device;
	Context Context;
	ovrMobile *Ovr;
	int DefaultFOV;
	int FOV;
	Scene Scene;
	long long FrameIndex;
	double DisplayTime;
	int SwapInterval;
	int CpuLevel;
	int GpuLevel;
	int MainThreadTid;
	int RenderThreadTid;
	bool UseFragmentDensity;
	VkRenderPass RenderPass;
	std::vector<View> Views;
	ovrMatrix4f ViewMatrices[VRAPI_FRAME_LAYER_EYE_MAX];
	ovrMatrix4f ProjectionMatrices[VRAPI_FRAME_LAYER_EYE_MAX];
	float Yaw;
	float Pitch;
	Screen Console;
	Screen Screen;
	double PreviousTime;
	double CurrentTime;
	uint32_t PreviousLeftButtons;
	uint32_t PreviousRightButtons;
	uint32_t LeftButtons;
	uint32_t RightButtons;
	ovrVector2f PreviousLeftJoystick;
	ovrVector2f PreviousRightJoystick;
	ovrVector2f LeftJoystick;
	ovrVector2f RightJoystick;
};

static const int queueCount = 1;
static const int CPU_LEVEL = 2;
static const int GPU_LEVEL = 3;

void checkErrors(VkResult result, const char *function)
{
	if (result != VK_SUCCESS)
	{
		const char* errorString;
		switch (result)
		{
			case VK_NOT_READY:
				errorString = "VK_NOT_READY";
				break;
			case VK_TIMEOUT:
				errorString = "VK_TIMEOUT";
				break;
			case VK_EVENT_SET:
				errorString = "VK_EVENT_SET";
				break;
			case VK_EVENT_RESET:
				errorString = "VK_EVENT_RESET";
				break;
			case VK_INCOMPLETE:
				errorString = "VK_INCOMPLETE";
				break;
			case VK_ERROR_OUT_OF_HOST_MEMORY:
				errorString = "VK_ERROR_OUT_OF_HOST_MEMORY";
				break;
			case VK_ERROR_OUT_OF_DEVICE_MEMORY:
				errorString = "VK_ERROR_OUT_OF_DEVICE_MEMORY";
				break;
			case VK_ERROR_INITIALIZATION_FAILED:
				errorString = "VK_ERROR_INITIALIZATION_FAILED";
				break;
			case VK_ERROR_DEVICE_LOST:
				errorString = "VK_ERROR_DEVICE_LOST";
				break;
			case VK_ERROR_MEMORY_MAP_FAILED:
				errorString = "VK_ERROR_MEMORY_MAP_FAILED";
				break;
			case VK_ERROR_LAYER_NOT_PRESENT:
				errorString = "VK_ERROR_LAYER_NOT_PRESENT";
				break;
			case VK_ERROR_EXTENSION_NOT_PRESENT:
				errorString = "VK_ERROR_EXTENSION_NOT_PRESENT";
				break;
			case VK_ERROR_FEATURE_NOT_PRESENT:
				errorString = "VK_ERROR_FEATURE_NOT_PRESENT";
				break;
			case VK_ERROR_INCOMPATIBLE_DRIVER:
				errorString = "VK_ERROR_INCOMPATIBLE_DRIVER";
				break;
			case VK_ERROR_TOO_MANY_OBJECTS:
				errorString = "VK_ERROR_TOO_MANY_OBJECTS";
				break;
			case VK_ERROR_FORMAT_NOT_SUPPORTED:
				errorString = "VK_ERROR_FORMAT_NOT_SUPPORTED";
				break;
			case VK_ERROR_SURFACE_LOST_KHR:
				errorString = "VK_ERROR_SURFACE_LOST_KHR";
				break;
			case VK_SUBOPTIMAL_KHR:
				errorString = "VK_SUBOPTIMAL_KHR";
				break;
			case VK_ERROR_OUT_OF_DATE_KHR:
				errorString = "VK_ERROR_OUT_OF_DATE_KHR";
				break;
			case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
				errorString = "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
				break;
			case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
				errorString = "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
				break;
			case VK_ERROR_VALIDATION_FAILED_EXT:
				errorString = "VK_ERROR_VALIDATION_FAILED_EXT";
				break;
			case VK_ERROR_INVALID_SHADER_NV:
				errorString = "VK_ERROR_INVALID_SHADER_NV";
				break;
			default:
				errorString = "unknown";
		}
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Vulkan error: %s: %s\n", function, errorString);
	}
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallback(VkDebugReportFlagsEXT msgFlags, VkDebugReportObjectTypeEXT objType, uint64_t srcObject, size_t location, int32_t msgCode, const char *pLayerPrefix, const char *pMsg, void *pUserData)
{
	const char *reportType = "Unknown";
	if ((msgFlags & VK_DEBUG_REPORT_ERROR_BIT_EXT) != 0)
	{
		reportType = "Error";
	}
	else if ((msgFlags & VK_DEBUG_REPORT_WARNING_BIT_EXT) != 0)
	{
		reportType = "Warning";
	}
	else if ((msgFlags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) != 0)
	{
		reportType = "Performance Warning";
	}
	else if ((msgFlags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) != 0)
	{
		reportType = "Information";
	}
	else if ((msgFlags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) != 0)
	{
		reportType = "Debug";
	}
	__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "%s: [%s] Code %d : %s", reportType, pLayerPrefix, msgCode, pMsg);
	return VK_FALSE;
}

double getTime()
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (now.tv_sec * 1e9 + now.tv_nsec) * 0.000000001;
}

void appHandleCommands(struct android_app *app, int32_t cmd)
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
			appState->NativeWindow = nullptr;
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
	ovrJava java;
	java.Vm = app->activity->vm;
	java.Vm->AttachCurrentThread(&java.Env, nullptr);
	java.ActivityObject = app->activity->clazz;
	prctl(PR_SET_NAME, (long) "SlipNFrag", 0, 0, 0);
	ovrInitParms initParms = vrapi_DefaultInitParms(&java);
	initParms.GraphicsAPI = VRAPI_GRAPHICS_API_VULKAN_1;
	int32_t initResult = vrapi_Initialize(&initParms);
	if (initResult != VRAPI_INITIALIZE_SUCCESS)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vrapi_Initialize() failed: %i", initResult);
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
	uint32_t instanceExtensionNamesSize;
	if (vrapi_GetInstanceExtensionsVulkan(nullptr, &instanceExtensionNamesSize))
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vrapi_GetInstanceExtensionsVulkan() failed to get size of extensions string.");
		vrapi_Shutdown();
		exit(0);
	}
	std::vector<char> instanceExtensionNames(instanceExtensionNamesSize);
	if (vrapi_GetInstanceExtensionsVulkan(instanceExtensionNames.data(), &instanceExtensionNamesSize))
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vrapi_GetInstanceExtensionsVulkan() failed to get extensions string.");
		vrapi_Shutdown();
		exit(0);
	}
	Instance instance { };
#if defined(_DEBUG)
	instance.validate = VK_TRUE;
#else
	instance.validate = VK_FALSE;
#endif
	auto libraryFilename = "libvulkan.so";
	instance.loader = dlopen(libraryFilename, RTLD_NOW | RTLD_LOCAL);
	if (instance.loader == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): %s not available: %s", libraryFilename, dlerror());
		vrapi_Shutdown();
		exit(0);
	}
	instance.vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) dlsym(instance.loader, "vkGetInstanceProcAddr");
	instance.vkEnumerateInstanceLayerProperties = (PFN_vkEnumerateInstanceLayerProperties) dlsym(instance.loader, "vkEnumerateInstanceLayerProperties");
	instance.vkCreateInstance = (PFN_vkCreateInstance) dlsym(instance.loader, "vkCreateInstance");
	if (instance.validate)
	{
		instanceExtensionNames.resize(instanceExtensionNamesSize + 1 + strlen(VK_EXT_DEBUG_REPORT_EXTENSION_NAME));
		strcpy(instanceExtensionNames.data() + instanceExtensionNamesSize, " ");
		strcpy(instanceExtensionNames.data() + instanceExtensionNamesSize + 1, VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		instanceExtensionNamesSize = instanceExtensionNames.size();
	}
	instanceExtensionNames.resize(instanceExtensionNamesSize + 1 + strlen(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME));
	strcpy(instanceExtensionNames.data() + instanceExtensionNamesSize, " ");
	strcpy(instanceExtensionNames.data() + instanceExtensionNamesSize + 1, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
	std::vector<const char *> enabledExtensionNames;
	std::string extensionName;
	for (auto c : instanceExtensionNames)
	{
		if (c > ' ')
		{
			extensionName += c;
		}
		else if (extensionName.length() > 0)
		{
			char *extensionNameToAdd = new char[extensionName.length() + 1];
			strcpy(extensionNameToAdd, extensionName.c_str());
			enabledExtensionNames.push_back(extensionNameToAdd);
			extensionName = "";
		}
	}
	if (extensionName.length() > 0)
	{
		char *extensionNameToAdd = new char[extensionName.length() + 1];
		strcpy(extensionNameToAdd, extensionName.c_str());
		enabledExtensionNames.push_back(extensionNameToAdd);
	}
#if defined(_DEBUG)
	__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "Enabled Extensions: ");
	for (uint32_t i = 0; i < enabledExtensionNames.size(); i++)
	{
		__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "\t(%d):%s", i, enabledExtensionNames[i]);
	}
#endif
	static const char *requestedLayers[]
	{
		"VK_LAYER_LUNARG_core_validation",
		"VK_LAYER_LUNARG_parameter_validation",
		"VK_LAYER_LUNARG_object_tracker",
		"VK_LAYER_GOOGLE_threading",
		"VK_LAYER_GOOGLE_unique_objects"
#if USE_API_DUMP == 1
		,"VK_LAYER_LUNARG_api_dump"
#endif
	};
	const uint32_t requestedCount = sizeof(requestedLayers) / sizeof(requestedLayers[0]);
	std::vector<const char *> enabledLayerNames;
	if (instance.validate)
	{
		uint32_t availableLayerCount = 0;
		VK(instance.vkEnumerateInstanceLayerProperties(&availableLayerCount, nullptr));
		std::vector<VkLayerProperties> availableLayers(availableLayerCount);
		VK(instance.vkEnumerateInstanceLayerProperties(&availableLayerCount, availableLayers.data()));
		for (uint32_t i = 0; i < requestedCount; i++)
		{
			for (uint32_t j = 0; j < availableLayerCount; j++)
			{
				if (strcmp(requestedLayers[i], availableLayers[j].layerName) == 0)
				{
					enabledLayerNames.push_back(requestedLayers[i]);
					break;
				}
			}
		}
	}
#if defined(_DEBUG)
	__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "Enabled Layers ");
	for (uint32_t i = 0; i < enabledLayerNames.size(); i++)
	{
		__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "\t(%d):%s", i, enabledLayerNames[i]);
	}
#endif
	__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "--------------------------------\n");
	const uint32_t apiMajor = VK_VERSION_MAJOR(VK_API_VERSION_1_0);
	const uint32_t apiMinor = VK_VERSION_MINOR(VK_API_VERSION_1_0);
	const uint32_t apiPatch = VK_VERSION_PATCH(VK_API_VERSION_1_0);
	__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "Instance API version : %d.%d.%d\n", apiMajor, apiMinor, apiPatch);
	__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "--------------------------------\n");
	VkDebugReportCallbackCreateInfoEXT debugReportCallbackCreateInfo { };
	debugReportCallbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
	debugReportCallbackCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
	debugReportCallbackCreateInfo.pfnCallback = debugReportCallback;
	VkApplicationInfo appInfo { };
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Slip & Frag";
	appInfo.applicationVersion = 1;
	appInfo.pEngineName = "Oculus Mobile SDK";
	appInfo.engineVersion = 1;
	appInfo.apiVersion = VK_API_VERSION_1_0;
	VkInstanceCreateInfo instanceCreateInfo { };
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pNext = (instance.validate) ? &debugReportCallbackCreateInfo : nullptr;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	instanceCreateInfo.enabledLayerCount = enabledLayerNames.size();
	instanceCreateInfo.ppEnabledLayerNames = (const char *const *) (enabledLayerNames.size() != 0 ? enabledLayerNames.data() : nullptr);
	instanceCreateInfo.enabledExtensionCount = enabledExtensionNames.size();
	instanceCreateInfo.ppEnabledExtensionNames = (const char *const *) (enabledExtensionNames.size() != 0 ? enabledExtensionNames.data() : nullptr);
	VK(instance.vkCreateInstance(&instanceCreateInfo, nullptr, &instance.instance));
	instance.vkDestroyInstance = (PFN_vkDestroyInstance) (instance.vkGetInstanceProcAddr(instance.instance, "vkDestroyInstance"));
	if (instance.vkDestroyInstance == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetInstanceProcAddr() could not find vkDestroyInstance.");
		vrapi_Shutdown();
		exit(0);
	}
	instance.vkEnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices) (instance.vkGetInstanceProcAddr(instance.instance, "vkEnumeratePhysicalDevices"));
	if (instance.vkEnumeratePhysicalDevices == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetInstanceProcAddr() could not find vkEnumeratePhysicalDevices.");
		vrapi_Shutdown();
		exit(0);
	}
	instance.vkGetPhysicalDeviceFeatures = (PFN_vkGetPhysicalDeviceFeatures) (instance.vkGetInstanceProcAddr(instance.instance, "vkGetPhysicalDeviceFeatures"));
	if (instance.vkGetPhysicalDeviceFeatures == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetInstanceProcAddr() could not find vkGetPhysicalDeviceFeatures.");
		vrapi_Shutdown();
		exit(0);
	}
	instance.vkGetPhysicalDeviceFeatures2KHR = (PFN_vkGetPhysicalDeviceFeatures2KHR) (instance.vkGetInstanceProcAddr(instance.instance, "vkGetPhysicalDeviceFeatures2KHR"));
	if (instance.vkGetPhysicalDeviceFeatures2KHR == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetInstanceProcAddr() could not find vkGetPhysicalDeviceFeatures2KHR.");
		vrapi_Shutdown();
		exit(0);
	}
	instance.vkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties) (instance.vkGetInstanceProcAddr(instance.instance, "vkGetPhysicalDeviceProperties"));
	if (instance.vkGetPhysicalDeviceProperties == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetInstanceProcAddr() could not find vkGetPhysicalDeviceProperties.");
		vrapi_Shutdown();
		exit(0);
	}
	instance.vkGetPhysicalDeviceProperties2KHR = (PFN_vkGetPhysicalDeviceProperties2KHR) (instance.vkGetInstanceProcAddr(instance.instance, "vkGetPhysicalDeviceProperties2KHR"));
	if (instance.vkGetPhysicalDeviceProperties2KHR == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetInstanceProcAddr() could not find vkGetPhysicalDeviceProperties2KHR.");
		vrapi_Shutdown();
		exit(0);
	}
	instance.vkGetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties) (instance.vkGetInstanceProcAddr(instance.instance, "vkGetPhysicalDeviceMemoryProperties"));
	if (instance.vkGetPhysicalDeviceMemoryProperties == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetInstanceProcAddr() could not find vkGetPhysicalDeviceMemoryProperties.");
		vrapi_Shutdown();
		exit(0);
	}
	instance.vkGetPhysicalDeviceQueueFamilyProperties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties) (instance.vkGetInstanceProcAddr(instance.instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
	if (instance.vkGetPhysicalDeviceQueueFamilyProperties == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetInstanceProcAddr() could not find vkGetPhysicalDeviceQueueFamilyProperties.");
		vrapi_Shutdown();
		exit(0);
	}
	instance.vkGetPhysicalDeviceFormatProperties = (PFN_vkGetPhysicalDeviceFormatProperties) (instance.vkGetInstanceProcAddr(instance.instance, "vkGetPhysicalDeviceFormatProperties"));
	if (instance.vkGetPhysicalDeviceFormatProperties == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetInstanceProcAddr() could not find vkGetPhysicalDeviceFormatProperties.");
		vrapi_Shutdown();
		exit(0);
	}
	instance.vkGetPhysicalDeviceImageFormatProperties = (PFN_vkGetPhysicalDeviceImageFormatProperties) (instance.vkGetInstanceProcAddr(instance.instance, "vkGetPhysicalDeviceImageFormatProperties"));
	if (instance.vkGetPhysicalDeviceImageFormatProperties == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetInstanceProcAddr() could not find vkGetPhysicalDeviceImageFormatProperties.");
		vrapi_Shutdown();
		exit(0);
	}
	instance.vkEnumerateDeviceExtensionProperties = (PFN_vkEnumerateDeviceExtensionProperties) (instance.vkGetInstanceProcAddr(instance.instance, "vkEnumerateDeviceExtensionProperties"));
	if (instance.vkEnumerateDeviceExtensionProperties == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetInstanceProcAddr() could not find vkEnumerateDeviceExtensionProperties.");
		vrapi_Shutdown();
		exit(0);
	}
	instance.vkEnumerateDeviceLayerProperties = (PFN_vkEnumerateDeviceLayerProperties) (instance.vkGetInstanceProcAddr(instance.instance, "vkEnumerateDeviceLayerProperties"));
	if (instance.vkEnumerateDeviceLayerProperties == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetInstanceProcAddr() could not find vkEnumerateDeviceLayerProperties.");
		vrapi_Shutdown();
		exit(0);
	}
	instance.vkCreateDevice = (PFN_vkCreateDevice) (instance.vkGetInstanceProcAddr(instance.instance, "vkCreateDevice"));
	if (instance.vkCreateDevice == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetInstanceProcAddr() could not find vkCreateDevice.");
		vrapi_Shutdown();
		exit(0);
	}
	instance.vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr) (instance.vkGetInstanceProcAddr(instance.instance, "vkGetDeviceProcAddr"));
	if (instance.vkGetDeviceProcAddr == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetInstanceProcAddr() could not find vkGetDeviceProcAddr.");
		vrapi_Shutdown();
		exit(0);
	}
	if (instance.validate)
	{
		instance.vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT) (instance.vkGetInstanceProcAddr(instance.instance, "vkCreateDebugReportCallbackEXT"));
		if (instance.vkCreateDebugReportCallbackEXT == nullptr)
		{
			__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetInstanceProcAddr() could not find vkCreateDebugReportCallbackEXT.");
			vrapi_Shutdown();
			exit(0);
		}
		instance.vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT) (instance.vkGetInstanceProcAddr(instance.instance, "vkDestroyDebugReportCallbackEXT"));
		if (instance.vkDestroyDebugReportCallbackEXT == nullptr)
		{
			__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetInstanceProcAddr() could not find vkDestroyDebugReportCallbackEXT.");
			vrapi_Shutdown();
			exit(0);
		}
		VK(instance.vkCreateDebugReportCallbackEXT(instance.instance, &debugReportCallbackCreateInfo, nullptr, &instance.debugReportCallback));
	}
	uint32_t deviceExtensionNamesSize;
	if (vrapi_GetDeviceExtensionsVulkan(nullptr, &deviceExtensionNamesSize))
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vrapi_GetDeviceExtensionsVulkan() failed to get size of extensions string.");
		vrapi_Shutdown();
		exit(0);
	}
	std::vector<char> deviceExtensionNames(deviceExtensionNamesSize);
	if (vrapi_GetDeviceExtensionsVulkan(deviceExtensionNames.data(), &deviceExtensionNamesSize))
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vrapi_GetDeviceExtensionsVulkan() failed to get extensions string.");
		vrapi_Shutdown();
		exit(0);
	}
	extensionName.clear();
	for (auto c : deviceExtensionNames)
	{
		if (c > ' ')
		{
			extensionName += c;
		}
		else if (extensionName.length() > 0)
		{
			char *extensionNameToAdd = new char[extensionName.length() + 1];
			strcpy(extensionNameToAdd, extensionName.c_str());
			appState.Device.enabledExtensionNames.push_back(extensionNameToAdd);
			extensionName = "";
		}
	}
	if (extensionName.length() > 0)
	{
		char *extensionNameToAdd = new char[extensionName.length() + 1];
		strcpy(extensionNameToAdd, extensionName.c_str());
		appState.Device.enabledExtensionNames.push_back(extensionNameToAdd);
	}
#if defined(_DEBUG)
	__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "Requested Extensions: ");
	for (uint32_t i = 0; i < appState.Device.enabledExtensionNames.size(); i++)
	{
		__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "\t(%d):%s", i, appState.Device.enabledExtensionNames[i]);
	}
#endif
	uint32_t physicalDeviceCount = 0;
	VK(instance.vkEnumeratePhysicalDevices(instance.instance, &physicalDeviceCount, nullptr));
	std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
	VK(instance.vkEnumeratePhysicalDevices(instance.instance, &physicalDeviceCount, physicalDevices.data()));
	for (uint32_t physicalDeviceIndex = 0; physicalDeviceIndex < physicalDeviceCount; physicalDeviceIndex++)
	{
		VkPhysicalDeviceProperties physicalDeviceProperties;
		VC(instance.vkGetPhysicalDeviceProperties(physicalDevices[physicalDeviceIndex], &physicalDeviceProperties));
		const uint32_t driverMajor = VK_VERSION_MAJOR(physicalDeviceProperties.driverVersion);
		const uint32_t driverMinor = VK_VERSION_MINOR(physicalDeviceProperties.driverVersion);
		const uint32_t driverPatch = VK_VERSION_PATCH(physicalDeviceProperties.driverVersion);
		const uint32_t apiMajor = VK_VERSION_MAJOR(physicalDeviceProperties.apiVersion);
		const uint32_t apiMinor = VK_VERSION_MINOR(physicalDeviceProperties.apiVersion);
		const uint32_t apiPatch = VK_VERSION_PATCH(physicalDeviceProperties.apiVersion);
		__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "--------------------------------\n");
		__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "Device Name          : %s\n", physicalDeviceProperties.deviceName);
		__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "Device Type          : %s\n", ((physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) ? "integrated GPU" : ((physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) ? "discrete GPU" : ((physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) ? "virtual GPU" : ((physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) ? "CPU" : "unknown")))));
		__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "Vendor ID            : 0x%04X\n", physicalDeviceProperties.vendorID);
		__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "Device ID            : 0x%04X\n", physicalDeviceProperties.deviceID);
		__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "Driver Version       : %d.%d.%d\n", driverMajor, driverMinor, driverPatch);
		__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "API Version          : %d.%d.%d\n", apiMajor, apiMinor, apiPatch);
		uint32_t queueFamilyCount = 0;
		VC(instance.vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[physicalDeviceIndex], &queueFamilyCount, nullptr));
		std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
		VC(instance.vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[physicalDeviceIndex], &queueFamilyCount, queueFamilyProperties.data()));
		for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilyCount; queueFamilyIndex++)
		{
			const VkQueueFlags queueFlags = queueFamilyProperties[queueFamilyIndex].queueFlags;
			__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "%-21s%c %d =%s%s (%d queues, %d priorities)\n", (queueFamilyIndex == 0 ? "Queue Families" : ""), (queueFamilyIndex == 0 ? ':' : ' '), queueFamilyIndex, (queueFlags & VK_QUEUE_GRAPHICS_BIT) ? " graphics" : "", (queueFlags & VK_QUEUE_TRANSFER_BIT) ? " transfer" : "", queueFamilyProperties[queueFamilyIndex].queueCount, physicalDeviceProperties.limits.discreteQueuePriorities);
		}
		int workQueueFamilyIndex = -1;
		int presentQueueFamilyIndex = -1;
		for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilyCount; queueFamilyIndex++)
		{
			if ((queueFamilyProperties[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT)
			{
				if ((int) queueFamilyProperties[queueFamilyIndex].queueCount >= queueCount)
				{
					workQueueFamilyIndex = queueFamilyIndex;
				}
			}
			if (workQueueFamilyIndex != -1 && (presentQueueFamilyIndex != -1))
			{
				break;
			}
		}
		presentQueueFamilyIndex = workQueueFamilyIndex;
		if (workQueueFamilyIndex == -1)
		{
			__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "Required work queue family not supported.\n");
			continue;
		}
		__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "Work Queue Family    : %d\n", workQueueFamilyIndex);
		__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "Present Queue Family : %d\n", presentQueueFamilyIndex);
		uint32_t availableExtensionCount = 0;
		VK(instance.vkEnumerateDeviceExtensionProperties(physicalDevices[physicalDeviceIndex], nullptr, &availableExtensionCount, nullptr));
		std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
		VK(instance.vkEnumerateDeviceExtensionProperties(physicalDevices[physicalDeviceIndex], nullptr, &availableExtensionCount, availableExtensions.data()));
		for (int extensionIdx = 0; extensionIdx < availableExtensionCount; extensionIdx++)
		{
			if (strcmp(availableExtensions[extensionIdx].extensionName, VK_KHR_MULTIVIEW_EXTENSION_NAME) == 0)
			{
				appState.Device.supportsMultiview = true;
			}
			if (strcmp(availableExtensions[extensionIdx].extensionName, VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME) == 0)
			{
				appState.Device.supportsFragmentDensity = true;
			}
		}
		if (instance.validate)
		{
			uint32_t availableLayerCount = 0;
			VK(instance.vkEnumerateDeviceLayerProperties(physicalDevices[physicalDeviceIndex], &availableLayerCount, nullptr));
			std::vector<VkLayerProperties> availableLayers(availableLayerCount);
			VK(instance.vkEnumerateDeviceLayerProperties(physicalDevices[physicalDeviceIndex], &availableLayerCount, availableLayers.data()));
			for (uint32_t i = 0; i < requestedCount; i++)
			{
				for (uint32_t j = 0; j < availableLayerCount; j++)
				{
					if (strcmp(requestedLayers[i], availableLayers[j].layerName) == 0)
					{
						appState.Device.enabledLayerNames[appState.Device.enabledLayerCount++] = requestedLayers[i];
						break;
					}
				}
			}
#if defined(_DEBUG)
			__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "Enabled Layers ");
			for (uint32_t i = 0; i < appState.Device.enabledLayerCount; i++)
			{
				__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "\t(%d):%s", i, appState.Device.enabledLayerNames[i]);
			}
#endif
		}
		appState.Device.physicalDevice = physicalDevices[physicalDeviceIndex];
		appState.Device.queueFamilyCount = queueFamilyCount;
		appState.Device.queueFamilyProperties = queueFamilyProperties.data();
		appState.Device.workQueueFamilyIndex = workQueueFamilyIndex;
		appState.Device.presentQueueFamilyIndex = presentQueueFamilyIndex;
		appState.Device.physicalDeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		appState.Device.physicalDeviceFeatures.pNext = nullptr;
		appState.Device.physicalDeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		appState.Device.physicalDeviceProperties.pNext = nullptr;
		VkPhysicalDeviceMultiviewFeatures deviceMultiviewFeatures { };
		VkPhysicalDeviceMultiviewProperties deviceMultiviewProperties { };
		if (appState.Device.supportsMultiview)
		{
			deviceMultiviewFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES;
			appState.Device.physicalDeviceFeatures.pNext = &deviceMultiviewFeatures;
			deviceMultiviewProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES;
			appState.Device.physicalDeviceProperties.pNext = &deviceMultiviewProperties;
		}
		VC(instance.vkGetPhysicalDeviceFeatures2KHR(physicalDevices[physicalDeviceIndex], &appState.Device.physicalDeviceFeatures));
		VC(instance.vkGetPhysicalDeviceProperties2KHR(physicalDevices[physicalDeviceIndex], &appState.Device.physicalDeviceProperties));
		VC(instance.vkGetPhysicalDeviceMemoryProperties(physicalDevices[physicalDeviceIndex], &appState.Device.physicalDeviceMemoryProperties));
		if (appState.Device.supportsMultiview)
		{
			__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "Device %s multiview rendering, with %d views and %u max instances", deviceMultiviewFeatures.multiview ? "supports" : "does not support", deviceMultiviewProperties.maxMultiviewViewCount, deviceMultiviewProperties.maxMultiviewInstanceIndex);
			appState.Device.supportsMultiview = deviceMultiviewFeatures.multiview;
		}
		if (appState.Device.supportsMultiview)
		{
			char *extensionNameToAdd = new char[strlen(VK_KHR_MULTIVIEW_EXTENSION_NAME) + 1];
			strcpy(extensionNameToAdd, VK_KHR_MULTIVIEW_EXTENSION_NAME);
			appState.Device.enabledExtensionNames.push_back(extensionNameToAdd);
		}
		if (appState.Device.supportsFragmentDensity)
		{
			char *extensionNameToAdd = new char[strlen(VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME) + 1];
			strcpy(extensionNameToAdd, VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME);
			appState.Device.enabledExtensionNames.push_back(extensionNameToAdd);
		}
		break;
	}
	__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "--------------------------------\n");
	if (appState.Device.physicalDevice == VK_NULL_HANDLE)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): No capable Vulkan physical device found.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.queueFamilyUsedQueues.resize(appState.Device.queueFamilyCount);
	for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < appState.Device.queueFamilyCount; queueFamilyIndex++)
	{
		appState.Device.queueFamilyUsedQueues[queueFamilyIndex] = 0xFFFFFFFF << appState.Device.queueFamilyProperties[queueFamilyIndex].queueCount;
	}
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&appState.Device.queueFamilyMutex, &attr);
	const uint32_t discreteQueuePriorities = appState.Device.physicalDeviceProperties.properties.limits.discreteQueuePriorities;
	const float queuePriority = (discreteQueuePriorities <= 2) ? 0.0f : 0.5f;
	VkDeviceQueueCreateInfo deviceQueueCreateInfo[2] { };
	deviceQueueCreateInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	deviceQueueCreateInfo[0].queueFamilyIndex = appState.Device.workQueueFamilyIndex;
	deviceQueueCreateInfo[0].queueCount = queueCount;
	deviceQueueCreateInfo[0].pQueuePriorities = &queuePriority;
	deviceQueueCreateInfo[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	deviceQueueCreateInfo[1].queueFamilyIndex = appState.Device.presentQueueFamilyIndex;
	deviceQueueCreateInfo[1].queueCount = 1;
	VkDeviceCreateInfo deviceCreateInfo { };
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	if (appState.Device.presentQueueFamilyIndex == -1 || appState.Device.presentQueueFamilyIndex == appState.Device.workQueueFamilyIndex)
	{
		deviceCreateInfo.queueCreateInfoCount = 1;
	}
	deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfo;
	deviceCreateInfo.enabledLayerCount = appState.Device.enabledLayerCount;
	deviceCreateInfo.ppEnabledLayerNames = (const char *const *) (appState.Device.enabledLayerCount != 0 ? appState.Device.enabledLayerNames : nullptr);
	deviceCreateInfo.enabledExtensionCount = appState.Device.enabledExtensionNames.size();
	deviceCreateInfo.ppEnabledExtensionNames = (const char *const *) (appState.Device.enabledExtensionNames.size() != 0 ? appState.Device.enabledExtensionNames.data() : nullptr);
	VK(instance.vkCreateDevice(appState.Device.physicalDevice, &deviceCreateInfo, nullptr, &appState.Device.device));
	appState.Device.vkDestroyDevice = (PFN_vkDestroyDevice)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkDestroyDevice"));
	if (appState.Device.vkDestroyDevice == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyDevice.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkGetDeviceQueue = (PFN_vkGetDeviceQueue)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkGetDeviceQueue"));
	if (appState.Device.vkGetDeviceQueue == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkGetDeviceQueue.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkQueueSubmit = (PFN_vkQueueSubmit)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkQueueSubmit"));
	if (appState.Device.vkQueueSubmit == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkQueueSubmit.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkQueueWaitIdle = (PFN_vkQueueWaitIdle)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkQueueWaitIdle"));
	if (appState.Device.vkQueueWaitIdle == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkQueueWaitIdle.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDeviceWaitIdle = (PFN_vkDeviceWaitIdle)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkDeviceWaitIdle"));
	if (appState.Device.vkDeviceWaitIdle == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDeviceWaitIdle.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkAllocateMemory = (PFN_vkAllocateMemory)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkAllocateMemory"));
	if (appState.Device.vkAllocateMemory == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkAllocateMemory.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkFreeMemory = (PFN_vkFreeMemory)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkFreeMemory"));
	if (appState.Device.vkFreeMemory == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkFreeMemory.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkMapMemory = (PFN_vkMapMemory)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkMapMemory"));
	if (appState.Device.vkMapMemory == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkMapMemory.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkUnmapMemory = (PFN_vkUnmapMemory)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkUnmapMemory"));
	if (appState.Device.vkUnmapMemory == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkUnmapMemory.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkFlushMappedMemoryRanges = (PFN_vkFlushMappedMemoryRanges)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkFlushMappedMemoryRanges"));
	if (appState.Device.vkFlushMappedMemoryRanges == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkFlushMappedMemoryRanges.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkBindBufferMemory = (PFN_vkBindBufferMemory)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkBindBufferMemory"));
	if (appState.Device.vkBindBufferMemory == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkBindBufferMemory.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkBindImageMemory = (PFN_vkBindImageMemory)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkBindImageMemory"));
	if (appState.Device.vkBindImageMemory == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkBindImageMemory.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkGetBufferMemoryRequirements = (PFN_vkGetBufferMemoryRequirements)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkGetBufferMemoryRequirements"));
	if (appState.Device.vkGetBufferMemoryRequirements == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkGetBufferMemoryRequirements.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkGetImageMemoryRequirements = (PFN_vkGetImageMemoryRequirements)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkGetImageMemoryRequirements"));
	if (appState.Device.vkGetImageMemoryRequirements == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkGetImageMemoryRequirements.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreateFence = (PFN_vkCreateFence)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCreateFence"));
	if (appState.Device.vkCreateFence == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreateFence.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroyFence = (PFN_vkDestroyFence)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkDestroyFence"));
	if (appState.Device.vkDestroyFence == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyFence.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkResetFences = (PFN_vkResetFences)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkResetFences"));
	if (appState.Device.vkResetFences == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkResetFences.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkGetFenceStatus = (PFN_vkGetFenceStatus)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkGetFenceStatus"));
	if (appState.Device.vkGetFenceStatus == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkGetFenceStatus.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkWaitForFences = (PFN_vkWaitForFences)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkWaitForFences"));
	if (appState.Device.vkWaitForFences == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkWaitForFences.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreateBuffer = (PFN_vkCreateBuffer)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCreateBuffer"));
	if (appState.Device.vkCreateBuffer == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreateBuffer.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroyBuffer = (PFN_vkDestroyBuffer)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkDestroyBuffer"));
	if (appState.Device.vkDestroyBuffer == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyBuffer.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreateImage = (PFN_vkCreateImage)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCreateImage"));
	if (appState.Device.vkCreateImage == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreateImage.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroyImage = (PFN_vkDestroyImage)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkDestroyImage"));
	if (appState.Device.vkDestroyImage == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyImage.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreateImageView = (PFN_vkCreateImageView)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCreateImageView"));
	if (appState.Device.vkCreateImageView == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreateImageView.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroyImageView = (PFN_vkDestroyImageView)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkDestroyImageView"));
	if (appState.Device.vkDestroyImageView == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyImageView.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreateShaderModule = (PFN_vkCreateShaderModule)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCreateShaderModule"));
	if (appState.Device.vkCreateShaderModule == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreateShaderModule.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroyShaderModule = (PFN_vkDestroyShaderModule)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkDestroyShaderModule"));
	if (appState.Device.vkDestroyShaderModule == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyShaderModule.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreatePipelineCache = (PFN_vkCreatePipelineCache)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCreatePipelineCache"));
	if (appState.Device.vkCreatePipelineCache == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreatePipelineCache.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroyPipelineCache = (PFN_vkDestroyPipelineCache)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkDestroyPipelineCache"));
	if (appState.Device.vkDestroyPipelineCache == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyPipelineCache.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreateGraphicsPipelines = (PFN_vkCreateGraphicsPipelines)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCreateGraphicsPipelines"));
	if (appState.Device.vkCreateGraphicsPipelines == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreateGraphicsPipelines.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroyPipeline = (PFN_vkDestroyPipeline)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkDestroyPipeline"));
	if (appState.Device.vkDestroyPipeline == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyPipeline.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreatePipelineLayout = (PFN_vkCreatePipelineLayout)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCreatePipelineLayout"));
	if (appState.Device.vkCreatePipelineLayout == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreatePipelineLayout.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroyPipelineLayout = (PFN_vkDestroyPipelineLayout)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkDestroyPipelineLayout"));
	if (appState.Device.vkDestroyPipelineLayout == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyPipelineLayout.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreateSampler = (PFN_vkCreateSampler)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCreateSampler"));
	if (appState.Device.vkCreateSampler == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreateSampler.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroySampler = (PFN_vkDestroySampler)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkDestroySampler"));
	if (appState.Device.vkDestroySampler == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroySampler.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreateDescriptorSetLayout = (PFN_vkCreateDescriptorSetLayout)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCreateDescriptorSetLayout"));
	if (appState.Device.vkCreateDescriptorSetLayout == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreateDescriptorSetLayout.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroyDescriptorSetLayout = (PFN_vkDestroyDescriptorSetLayout)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkDestroyDescriptorSetLayout"));
	if (appState.Device.vkDestroyDescriptorSetLayout == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyDescriptorSetLayout.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreateDescriptorPool = (PFN_vkCreateDescriptorPool)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCreateDescriptorPool"));
	if (appState.Device.vkCreateDescriptorPool == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreateDescriptorPool.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroyDescriptorPool = (PFN_vkDestroyDescriptorPool)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkDestroyDescriptorPool"));
	if (appState.Device.vkDestroyDescriptorPool == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyDescriptorPool.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkResetDescriptorPool = (PFN_vkResetDescriptorPool)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkResetDescriptorPool"));
	if (appState.Device.vkResetDescriptorPool == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkResetDescriptorPool.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkAllocateDescriptorSets = (PFN_vkAllocateDescriptorSets)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkAllocateDescriptorSets"));
	if (appState.Device.vkAllocateDescriptorSets == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkAllocateDescriptorSets.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkFreeDescriptorSets = (PFN_vkFreeDescriptorSets)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkFreeDescriptorSets"));
	if (appState.Device.vkFreeDescriptorSets == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkFreeDescriptorSets.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkUpdateDescriptorSets = (PFN_vkUpdateDescriptorSets)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkUpdateDescriptorSets"));
	if (appState.Device.vkUpdateDescriptorSets == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkUpdateDescriptorSets.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreateFramebuffer = (PFN_vkCreateFramebuffer)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCreateFramebuffer"));
	if (appState.Device.vkCreateFramebuffer == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreateFramebuffer.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroyFramebuffer = (PFN_vkDestroyFramebuffer)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkDestroyFramebuffer"));
	if (appState.Device.vkDestroyFramebuffer == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyFramebuffer.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreateRenderPass = (PFN_vkCreateRenderPass)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCreateRenderPass"));
	if (appState.Device.vkCreateRenderPass == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreateRenderPass.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroyRenderPass = (PFN_vkDestroyRenderPass)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkDestroyRenderPass"));
	if (appState.Device.vkDestroyRenderPass == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyRenderPass.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreateCommandPool = (PFN_vkCreateCommandPool)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCreateCommandPool"));
	if (appState.Device.vkCreateCommandPool == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreateCommandPool.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroyCommandPool = (PFN_vkDestroyCommandPool)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkDestroyCommandPool"));
	if (appState.Device.vkDestroyCommandPool == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyCommandPool.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkResetCommandPool = (PFN_vkResetCommandPool)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkResetCommandPool"));
	if (appState.Device.vkResetCommandPool == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkResetCommandPool.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkAllocateCommandBuffers = (PFN_vkAllocateCommandBuffers)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkAllocateCommandBuffers"));
	if (appState.Device.vkAllocateCommandBuffers == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkAllocateCommandBuffers.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkFreeCommandBuffers = (PFN_vkFreeCommandBuffers)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkFreeCommandBuffers"));
	if (appState.Device.vkFreeCommandBuffers == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkFreeCommandBuffers.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkBeginCommandBuffer = (PFN_vkBeginCommandBuffer)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkBeginCommandBuffer"));
	if (appState.Device.vkBeginCommandBuffer == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkBeginCommandBuffer.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkEndCommandBuffer = (PFN_vkEndCommandBuffer)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkEndCommandBuffer"));
	if (appState.Device.vkEndCommandBuffer == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkEndCommandBuffer.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkResetCommandBuffer = (PFN_vkResetCommandBuffer)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkResetCommandBuffer"));
	if (appState.Device.vkResetCommandBuffer == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkResetCommandBuffer.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdBindPipeline = (PFN_vkCmdBindPipeline)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCmdBindPipeline"));
	if (appState.Device.vkCmdBindPipeline == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdBindPipeline.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdSetViewport = (PFN_vkCmdSetViewport)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCmdSetViewport"));
	if (appState.Device.vkCmdSetViewport == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdSetViewport.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdSetScissor = (PFN_vkCmdSetScissor)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCmdSetScissor"));
	if (appState.Device.vkCmdSetScissor == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdSetScissor.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdBindDescriptorSets = (PFN_vkCmdBindDescriptorSets)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCmdBindDescriptorSets"));
	if (appState.Device.vkCmdBindDescriptorSets == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdBindDescriptorSets.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdBindIndexBuffer = (PFN_vkCmdBindIndexBuffer)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCmdBindIndexBuffer"));
	if (appState.Device.vkCmdBindIndexBuffer == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdBindIndexBuffer.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdBindVertexBuffers = (PFN_vkCmdBindVertexBuffers)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCmdBindVertexBuffers"));
	if (appState.Device.vkCmdBindVertexBuffers == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdBindVertexBuffers.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdBlitImage = (PFN_vkCmdBlitImage)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCmdBlitImage"));
	if (appState.Device.vkCmdBlitImage == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdBlitImage.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdDrawIndexed = (PFN_vkCmdDrawIndexed)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCmdDrawIndexed"));
	if (appState.Device.vkCmdDrawIndexed == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdDrawIndexed.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdCopyBuffer = (PFN_vkCmdCopyBuffer)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCmdCopyBuffer"));
	if (appState.Device.vkCmdCopyBuffer == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdCopyBuffer.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdCopyBufferToImage = (PFN_vkCmdCopyBufferToImage)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCmdCopyBufferToImage"));
	if (appState.Device.vkCmdCopyBufferToImage == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdCopyBufferToImage.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdResolveImage = (PFN_vkCmdResolveImage)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCmdResolveImage"));
	if (appState.Device.vkCmdResolveImage == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdResolveImage.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdPipelineBarrier = (PFN_vkCmdPipelineBarrier)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCmdPipelineBarrier"));
	if (appState.Device.vkCmdPipelineBarrier == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdPipelineBarrier.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdPushConstants = (PFN_vkCmdPushConstants)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCmdPushConstants"));
	if (appState.Device.vkCmdPushConstants == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdPushConstants.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdBeginRenderPass = (PFN_vkCmdBeginRenderPass)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCmdBeginRenderPass"));
	if (appState.Device.vkCmdBeginRenderPass == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdBeginRenderPass.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdEndRenderPass = (PFN_vkCmdEndRenderPass)(instance.vkGetDeviceProcAddr(appState.Device.device, "vkCmdEndRenderPass"));
	if (appState.Device.vkCmdEndRenderPass == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdEndRenderPass.");
		vrapi_Shutdown();
		exit(0);
	}
	if (pthread_mutex_trylock(&appState.Device.queueFamilyMutex) == EBUSY)
	{
		pthread_mutex_lock(&appState.Device.queueFamilyMutex);
	}
	if ((appState.Device.queueFamilyUsedQueues[appState.Device.workQueueFamilyIndex] & 1) != 0)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): queue 0 in queueFamilyUsedQueues is already in use.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.queueFamilyUsedQueues[appState.Device.workQueueFamilyIndex] |= 1;
	pthread_mutex_unlock(&appState.Device.queueFamilyMutex);
	appState.Context.device = &appState.Device;
	appState.Context.queueFamilyIndex = appState.Device.workQueueFamilyIndex;
	appState.Context.queueIndex = 0;
	VC(appState.Device.vkGetDeviceQueue(appState.Device.device, appState.Context.queueFamilyIndex, appState.Context.queueIndex, &appState.Context.queue));
	VkCommandPoolCreateInfo commandPoolCreateInfo { };
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolCreateInfo.queueFamilyIndex = appState.Context.queueFamilyIndex;
	VK(appState.Device.vkCreateCommandPool(appState.Device.device, &commandPoolCreateInfo, nullptr, &appState.Context.commandPool));
	VkFenceCreateInfo fenceCreateInfo { };
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VkCommandBufferAllocateInfo commandBufferAllocateInfo { };
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.commandPool = appState.Context.commandPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = 1;
	VkCommandBufferBeginInfo setupCommandBufferBeginInfo { };
	setupCommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	setupCommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VkCommandBufferBeginInfo commandBufferBeginInfo { };
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	VkCommandBuffer setupCommandBuffer;
	VkSubmitInfo setupSubmitInfo { };
	setupSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	setupSubmitInfo.commandBufferCount = 1;
	setupSubmitInfo.pCommandBuffers = &setupCommandBuffer;
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo { };
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	VK(appState.Device.vkCreatePipelineCache(appState.Device.device, &pipelineCacheCreateInfo, nullptr, &appState.Context.pipelineCache));
	ovrSystemCreateInfoVulkan systemInfo;
	systemInfo.Instance = instance.instance;
	systemInfo.Device = appState.Device.device;
	systemInfo.PhysicalDevice = appState.Device.physicalDevice;
	initResult = vrapi_CreateSystemVulkan(&systemInfo);
	if (initResult != ovrSuccess)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Failed to create VrApi Vulkan system.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.CpuLevel = CPU_LEVEL;
	appState.GpuLevel = GPU_LEVEL;
	appState.MainThreadTid = gettid();
	auto isMultiview = appState.Device.supportsMultiview;
	auto useFragmentDensity = appState.Device.supportsFragmentDensity;
	appState.Views.resize(isMultiview ? 1 : VRAPI_FRAME_LAYER_EYE_MAX);
	auto eyeTextureWidth = vrapi_GetSystemPropertyInt(&appState.Java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH);
	auto eyeTextureHeight = vrapi_GetSystemPropertyInt(&appState.Java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT);
	if (eyeTextureWidth > appState.Device.physicalDeviceProperties.properties.limits.maxFramebufferWidth)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Eye texture width exceeds the physical device's limits.");
		vrapi_Shutdown();
		exit(0);
	}
	if (eyeTextureHeight > appState.Device.physicalDeviceProperties.properties.limits.maxFramebufferHeight)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Eye texture height exceeds the physical device's limits.");
		vrapi_Shutdown();
		exit(0);
	}
	auto horizontalFOV = vrapi_GetSystemPropertyInt(&appState.Java, VRAPI_SYS_PROP_SUGGESTED_EYE_FOV_DEGREES_X);
	auto verticalFOV = vrapi_GetSystemPropertyInt(&appState.Java, VRAPI_SYS_PROP_SUGGESTED_EYE_FOV_DEGREES_Y);
	appState.FOV = std::max(horizontalFOV, verticalFOV);
	for (auto& view : appState.Views)
	{
		view.colorSwapChain.SwapChain = vrapi_CreateTextureSwapChain3(isMultiview ? VRAPI_TEXTURE_TYPE_2D_ARRAY : VRAPI_TEXTURE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, eyeTextureWidth, eyeTextureHeight, 1, 3);
		view.colorSwapChain.SwapChainLength = vrapi_GetTextureSwapChainLength(view.colorSwapChain.SwapChain);
		view.colorSwapChain.ColorTextures.resize(view.colorSwapChain.SwapChainLength);
		view.colorSwapChain.FragmentDensityTextures.resize(view.colorSwapChain.SwapChainLength);
		view.colorSwapChain.FragmentDensityTextureSizes.resize(view.colorSwapChain.SwapChainLength);
		for (auto i = 0; i < view.colorSwapChain.SwapChainLength; i++)
		{
			view.colorSwapChain.ColorTextures[i] = vrapi_GetTextureSwapChainBufferVulkan(view.colorSwapChain.SwapChain, i);
			if (view.colorSwapChain.FragmentDensityTextures.size() > 0)
			{
				auto result = vrapi_GetTextureSwapChainBufferFoveationVulkan(view.colorSwapChain.SwapChain, i, &view.colorSwapChain.FragmentDensityTextures[i], &view.colorSwapChain.FragmentDensityTextureSizes[i].width, &view.colorSwapChain.FragmentDensityTextureSizes[i].height);
				if (result != ovrSuccess)
				{
					view.colorSwapChain.FragmentDensityTextures.clear();
					view.colorSwapChain.FragmentDensityTextureSizes.clear();
				}
			}
		}
		useFragmentDensity = useFragmentDensity && (view.colorSwapChain.FragmentDensityTextures.size() > 0);
	}
	if ((appState.Device.physicalDeviceProperties.properties.limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_4_BIT) == 0)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Requested color buffer sample count not available.");
		vrapi_Shutdown();
		exit(0);
	}
	if ((appState.Device.physicalDeviceProperties.properties.limits.framebufferDepthSampleCounts & VK_SAMPLE_COUNT_4_BIT) == 0)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Requested depth buffer sample count not available.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.UseFragmentDensity = useFragmentDensity;
	uint32_t attachmentCount = 0;
	VkAttachmentDescription attachments[4] { };
	attachments[attachmentCount].format = VK_FORMAT_R8G8B8A8_UNORM;
	attachments[attachmentCount].samples = VK_SAMPLE_COUNT_4_BIT;
	attachments[attachmentCount].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[attachmentCount].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[attachmentCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[attachmentCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[attachmentCount].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachments[attachmentCount].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachmentCount++;
	attachments[attachmentCount].format = VK_FORMAT_R8G8B8A8_UNORM;
	attachments[attachmentCount].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[attachmentCount].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[attachmentCount].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[attachmentCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[attachmentCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[attachmentCount].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachments[attachmentCount].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachmentCount++;
	attachments[attachmentCount].format = VK_FORMAT_D24_UNORM_S8_UINT;
	attachments[attachmentCount].samples = VK_SAMPLE_COUNT_4_BIT;
	attachments[attachmentCount].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[attachmentCount].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[attachmentCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[attachmentCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[attachmentCount].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[attachmentCount].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachmentCount++;
	uint32_t sampleMapAttachment = 0;
	if (appState.UseFragmentDensity)
	{
		sampleMapAttachment = attachmentCount;
		attachments[attachmentCount].format = VK_FORMAT_R8G8_UNORM;
		attachments[attachmentCount].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[attachmentCount].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[attachmentCount].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[attachmentCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[attachmentCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[attachmentCount].initialLayout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
		attachments[attachmentCount].finalLayout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
		attachmentCount++;
	}
	VkAttachmentReference colorAttachmentReference;
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkAttachmentReference resolveAttachmentReference;
	resolveAttachmentReference.attachment = 1;
	resolveAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkAttachmentReference depthAttachmentReference;
	depthAttachmentReference.attachment = 2;
	depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	VkAttachmentReference fragmentDensityAttachmentReference;
	if (appState.UseFragmentDensity)
	{
		fragmentDensityAttachmentReference.attachment = sampleMapAttachment;
		fragmentDensityAttachmentReference.layout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
	}
	VkSubpassDescription subpassDescription { };
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorAttachmentReference;
	subpassDescription.pResolveAttachments = &resolveAttachmentReference;
	subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;
	VkRenderPassCreateInfo renderPassCreateInfo { };
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = attachmentCount;
	renderPassCreateInfo.pAttachments = attachments;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpassDescription;
	VkRenderPassMultiviewCreateInfo multiviewCreateInfo { };
	const uint32_t viewMask = 3;
	if (isMultiview)
	{
		multiviewCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO;
		multiviewCreateInfo.subpassCount = 1;
		multiviewCreateInfo.pViewMasks = &viewMask;
		multiviewCreateInfo.correlationMaskCount = 1;
		multiviewCreateInfo.pCorrelationMasks = &viewMask;
		renderPassCreateInfo.pNext = &multiviewCreateInfo;
	}
	VkRenderPassFragmentDensityMapCreateInfoEXT fragmentDensityMapCreateInfoEXT;
	if (appState.UseFragmentDensity)
	{
		fragmentDensityMapCreateInfoEXT.sType = VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT;
		fragmentDensityMapCreateInfoEXT.fragmentDensityMapAttachment = fragmentDensityAttachmentReference;
		fragmentDensityMapCreateInfoEXT.pNext = renderPassCreateInfo.pNext;
		renderPassCreateInfo.pNext = &fragmentDensityMapCreateInfoEXT;
	}
	VK(appState.Device.vkCreateRenderPass(appState.Device.device, &renderPassCreateInfo, nullptr, &appState.RenderPass));
	for (auto& view : appState.Views)
	{
		view.framebuffer.colorTextureSwapChain = view.colorSwapChain.SwapChain;
		view.framebuffer.swapChainLength = view.colorSwapChain.SwapChainLength;
		view.framebuffer.colorTextures.resize(view.framebuffer.swapChainLength);
		if (view.colorSwapChain.FragmentDensityTextures.size() > 0)
		{
			view.framebuffer.fragmentDensityTextures.resize(view.framebuffer.swapChainLength);
		}
		view.framebuffer.framebuffers.resize(view.framebuffer.swapChainLength);
		view.framebuffer.width = eyeTextureWidth;
		view.framebuffer.height = eyeTextureHeight;
		view.framebuffer.currentBuffer = 0;
		for (auto i = 0; i < view.framebuffer.swapChainLength; i++)
		{
			auto& texture = view.framebuffer.colorTextures[i];
			texture.width = eyeTextureWidth;
			texture.height = eyeTextureHeight;
			texture.layerCount = isMultiview ? 2 : 1;
			texture.image = view.colorSwapChain.ColorTextures[i];
			VK(appState.Device.vkAllocateCommandBuffers(appState.Device.device, &commandBufferAllocateInfo, &setupCommandBuffer));
			VK(appState.Device.vkBeginCommandBuffer(setupCommandBuffer, &setupCommandBufferBeginInfo));
			VkImageMemoryBarrier imageMemoryBarrier { };
			imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageMemoryBarrier.image = view.colorSwapChain.ColorTextures[i];
			imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageMemoryBarrier.subresourceRange.levelCount = 1;
			imageMemoryBarrier.subresourceRange.layerCount = texture.layerCount;
			VC(appState.Device.vkCmdPipelineBarrier(setupCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
			VK(appState.Device.vkEndCommandBuffer(setupCommandBuffer));
			VK(appState.Device.vkQueueSubmit(appState.Context.queue, 1, &setupSubmitInfo, VK_NULL_HANDLE));
			VK(appState.Device.vkQueueWaitIdle(appState.Context.queue));
			VC(appState.Device.vkFreeCommandBuffers(appState.Device.device, appState.Context.commandPool, 1, &setupCommandBuffer));
			VkImageViewCreateInfo imageViewCreateInfo { };
			imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCreateInfo.image = texture.image;
			imageViewCreateInfo.viewType = isMultiview ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
			imageViewCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
			imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageViewCreateInfo.subresourceRange.levelCount = 1;
			imageViewCreateInfo.subresourceRange.layerCount = texture.layerCount;
			VK(appState.Device.vkCreateImageView(appState.Device.device, &imageViewCreateInfo, nullptr, &texture.view));
			VkSamplerCreateInfo samplerCreateInfo { };
			samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
			samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
			samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			samplerCreateInfo.maxLod = 1;
			samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
			VK(appState.Device.vkCreateSampler(appState.Device.device, &samplerCreateInfo, nullptr, &texture.sampler));
			if (view.framebuffer.fragmentDensityTextures.size() > 0)
			{
				auto& texture = view.framebuffer.fragmentDensityTextures[i];
				texture.width = view.colorSwapChain.FragmentDensityTextureSizes[i].width;
				texture.height = view.colorSwapChain.FragmentDensityTextureSizes[i].height;
				texture.layerCount = isMultiview ? 2 : 1;
				texture.image = view.colorSwapChain.FragmentDensityTextures[i];
				VK(appState.Device.vkAllocateCommandBuffers(appState.Device.device, &commandBufferAllocateInfo, &setupCommandBuffer));
				VK(appState.Device.vkBeginCommandBuffer(setupCommandBuffer, &setupCommandBufferBeginInfo));
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT;
				imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
				imageMemoryBarrier.image = view.colorSwapChain.FragmentDensityTextures[i];
				VC(appState.Device.vkCmdPipelineBarrier(setupCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
				VK(appState.Device.vkEndCommandBuffer(setupCommandBuffer));
				VK(appState.Device.vkQueueSubmit(appState.Context.queue, 1, &setupSubmitInfo, VK_NULL_HANDLE));
				VK(appState.Device.vkQueueWaitIdle(appState.Context.queue));
				VC(appState.Device.vkFreeCommandBuffers(appState.Device.device, appState.Context.commandPool, 1, &setupCommandBuffer));
				VkImageViewCreateInfo imageViewCreateInfoFragmentDensity { };
				imageViewCreateInfoFragmentDensity.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				imageViewCreateInfoFragmentDensity.image = texture.image;
				imageViewCreateInfoFragmentDensity.viewType = isMultiview ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
				imageViewCreateInfoFragmentDensity.format = VK_FORMAT_R8G8_UNORM;
				imageViewCreateInfoFragmentDensity.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageViewCreateInfoFragmentDensity.subresourceRange.levelCount = 1;
				imageViewCreateInfoFragmentDensity.subresourceRange.layerCount = texture.layerCount;
				VK(appState.Device.vkCreateImageView(appState.Device.device, &imageViewCreateInfoFragmentDensity, nullptr, &texture.view));
				VkSamplerCreateInfo samplerCreateInfo { };
				samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
				samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
				samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
				samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
				samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
				samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
				samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
				samplerCreateInfo.maxLod = 1;
				samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
				VK(appState.Device.vkCreateSampler(appState.Device.device, &samplerCreateInfo, nullptr, &texture.sampler));
			}
		}
		VkFormatProperties props;
		VC(instance.vkGetPhysicalDeviceFormatProperties(appState.Device.physicalDevice, VK_FORMAT_R8G8B8A8_UNORM, &props));
		if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0)
		{
			__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Color attachment bit in texture is not defined.");
			vrapi_Shutdown();
			exit(0);
		}
		auto& texture = view.framebuffer.renderTexture;
		texture.width = view.framebuffer.width;
		texture.height = view.framebuffer.height;
		texture.layerCount = (isMultiview ? 2 : 1);
		VkImageCreateInfo imageCreateInfo { };
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		imageCreateInfo.extent.width = texture.width;
		imageCreateInfo.extent.height = texture.height;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = texture.layerCount;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_4_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK(appState.Device.vkCreateImage(appState.Device.device, &imageCreateInfo, nullptr, &texture.image));
		VkMemoryRequirements memoryRequirements;
		VC(appState.Device.vkGetImageMemoryRequirements(appState.Device.device, texture.image, &memoryRequirements));
		VkMemoryAllocateInfo memoryAllocateInfo { };
		memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memoryAllocateInfo.allocationSize = memoryRequirements.size;
		auto requiredProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		auto typeFound = false;
		for (uint32_t type = 0; type < appState.Context.device->physicalDeviceMemoryProperties.memoryTypeCount; type++)
		{
			if ((memoryRequirements.memoryTypeBits & (1 << type)) != 0)
			{
				const VkFlags propertyFlags = appState.Context.device->physicalDeviceMemoryProperties.memoryTypes[type].propertyFlags;
				if ((propertyFlags & requiredProperties) == requiredProperties)
				{
					typeFound = true;
					memoryAllocateInfo.memoryTypeIndex = type;
					break;
				}
			}
		}
		if (!typeFound)
		{
			__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Memory type %d with properties %d not found.", memoryRequirements.memoryTypeBits, requiredProperties);
			vrapi_Shutdown();
			exit(0);
		}
		VK(appState.Device.vkAllocateMemory(appState.Device.device, &memoryAllocateInfo, nullptr, &texture.memory));
		VK(appState.Device.vkBindImageMemory(appState.Device.device, texture.image, texture.memory, 0));
		VK(appState.Device.vkAllocateCommandBuffers(appState.Device.device, &commandBufferAllocateInfo, &setupCommandBuffer));
		VK(appState.Device.vkBeginCommandBuffer(setupCommandBuffer, &setupCommandBufferBeginInfo));
		VkImageMemoryBarrier imageMemoryBarrier { };
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageMemoryBarrier.image = texture.image;
		imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageMemoryBarrier.subresourceRange.levelCount = 1;
		imageMemoryBarrier.subresourceRange.layerCount = texture.layerCount;
		VC(appState.Device.vkCmdPipelineBarrier(setupCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
		VK(appState.Device.vkEndCommandBuffer(setupCommandBuffer));
		VK(appState.Device.vkQueueSubmit(appState.Context.queue, 1, &setupSubmitInfo, VK_NULL_HANDLE));
		VK(appState.Device.vkQueueWaitIdle(appState.Context.queue));
		VC(appState.Device.vkFreeCommandBuffers(appState.Device.device, appState.Context.commandPool, 1, &setupCommandBuffer));
		VkImageViewCreateInfo imageViewCreateInfo { };
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = texture.image;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		imageViewCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.layerCount = texture.layerCount;
		VK(appState.Device.vkCreateImageView(appState.Device.device, &imageViewCreateInfo, nullptr, &texture.view));
		VkSamplerCreateInfo samplerCreateInfo { };
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.maxLod = 1;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
		VK(appState.Device.vkCreateSampler(appState.Device.device, &samplerCreateInfo, nullptr, &texture.sampler));
		VK(appState.Device.vkAllocateCommandBuffers(appState.Device.device, &commandBufferAllocateInfo, &setupCommandBuffer));
		VK(appState.Device.vkBeginCommandBuffer(setupCommandBuffer, &setupCommandBufferBeginInfo));
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		imageMemoryBarrier.image = texture.image;
		imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageMemoryBarrier.subresourceRange.levelCount = 1;
		imageMemoryBarrier.subresourceRange.layerCount = texture.layerCount;
		VC(appState.Device.vkCmdPipelineBarrier(setupCommandBuffer, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
		VK(appState.Device.vkEndCommandBuffer(setupCommandBuffer));
		VK(appState.Device.vkQueueSubmit(appState.Context.queue, 1, &setupSubmitInfo, VK_NULL_HANDLE));
		VK(appState.Device.vkQueueWaitIdle(appState.Context.queue));
		VC(appState.Device.vkFreeCommandBuffers(appState.Device.device, appState.Context.commandPool, 1, &setupCommandBuffer));
		const auto numLayers = (isMultiview ? 2 : 1);
		imageCreateInfo.format = VK_FORMAT_D24_UNORM_S8_UINT;
		imageCreateInfo.extent.width = view.framebuffer.width;
		imageCreateInfo.extent.height = view.framebuffer.height;
		imageCreateInfo.arrayLayers = numLayers;
		imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		VK(appState.Device.vkCreateImage(appState.Device.device, &imageCreateInfo, nullptr, &view.framebuffer.depthImage));
		VC(appState.Device.vkGetImageMemoryRequirements(appState.Device.device, view.framebuffer.depthImage, &memoryRequirements));
		memoryAllocateInfo.allocationSize = memoryRequirements.size;
		typeFound = false;
		for (uint32_t type = 0; type < appState.Context.device->physicalDeviceMemoryProperties.memoryTypeCount; type++)
		{
			if ((memoryRequirements.memoryTypeBits & (1 << type)) != 0)
			{
				const VkFlags propertyFlags = appState.Context.device->physicalDeviceMemoryProperties.memoryTypes[type].propertyFlags;
				if ((propertyFlags & requiredProperties) == requiredProperties)
				{
					typeFound = true;
					memoryAllocateInfo.memoryTypeIndex = type;
					break;
				}
			}
		}
		if (!typeFound)
		{
			__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Memory type %d with properties %d not found.", memoryRequirements.memoryTypeBits, requiredProperties);
			vrapi_Shutdown();
			exit(0);
		}
		VK(appState.Device.vkAllocateMemory(appState.Device.device, &memoryAllocateInfo, nullptr, &view.framebuffer.depthMemory));
		VK(appState.Device.vkBindImageMemory(appState.Device.device, view.framebuffer.depthImage, view.framebuffer.depthMemory, 0));
		imageViewCreateInfo.image = view.framebuffer.depthImage;
		imageViewCreateInfo.viewType = (numLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D);
		imageViewCreateInfo.format = imageCreateInfo.format;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		imageViewCreateInfo.subresourceRange.layerCount = numLayers;
		VK(appState.Device.vkCreateImageView(appState.Device.device, &imageViewCreateInfo, nullptr, &view.framebuffer.depthImageView));
		VK(appState.Device.vkAllocateCommandBuffers(appState.Device.device, &commandBufferAllocateInfo, &setupCommandBuffer));
		VK(appState.Device.vkBeginCommandBuffer(setupCommandBuffer, &setupCommandBufferBeginInfo));
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		imageMemoryBarrier.image = view.framebuffer.depthImage;
		imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		imageMemoryBarrier.subresourceRange.layerCount = numLayers;
		VC(appState.Device.vkCmdPipelineBarrier(setupCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
		VK(appState.Device.vkEndCommandBuffer(setupCommandBuffer));
		VK(appState.Device.vkQueueSubmit(appState.Context.queue, 1, &setupSubmitInfo, VK_NULL_HANDLE));
		VK(appState.Device.vkQueueWaitIdle(appState.Context.queue));
		VC(appState.Device.vkFreeCommandBuffers(appState.Device.device, appState.Context.commandPool, 1, &setupCommandBuffer));
		for (auto i = 0; i < view.framebuffer.swapChainLength; i++)
		{
			uint32_t attachmentCount = 0;
			VkImageView attachments[4];
			attachments[attachmentCount++] = view.framebuffer.renderTexture.view;
			attachments[attachmentCount++] = view.framebuffer.colorTextures[i].view;
			attachments[attachmentCount++] = view.framebuffer.depthImageView;
			if (view.framebuffer.fragmentDensityTextures.size() > 0 && appState.UseFragmentDensity)
			{
				attachments[attachmentCount++] = view.framebuffer.fragmentDensityTextures[i].view;
			}
			VkFramebufferCreateInfo framebufferCreateInfo { };
			framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferCreateInfo.renderPass = appState.RenderPass;
			framebufferCreateInfo.attachmentCount = attachmentCount;
			framebufferCreateInfo.pAttachments = attachments;
			framebufferCreateInfo.width = view.framebuffer.width;
			framebufferCreateInfo.height = view.framebuffer.height;
			framebufferCreateInfo.layers = 1;
			VK(appState.Device.vkCreateFramebuffer(appState.Device.device, &framebufferCreateInfo, nullptr, &view.framebuffer.framebuffers[i]));
		}
		view.perImage.resize(view.framebuffer.swapChainLength);
		for (auto& perImage : view.perImage)
		{
			VK(appState.Device.vkAllocateCommandBuffers(appState.Device.device, &commandBufferAllocateInfo, &perImage.commandBuffer));
			VK(appState.Device.vkCreateFence(appState.Device.device, &fenceCreateInfo, nullptr, &perImage.fence));
		}
	}
	app->userData = &appState;
	app->onAppCmd = appHandleCommands;
	while (app->destroyRequested == 0)
	{
		for (;;)
		{
			int events;
			struct android_poll_source *source;
			const int timeoutMilliseconds = (appState.Ovr == nullptr && app->destroyRequested == 0) ? -1 : 0;
			if (ALooper_pollAll(timeoutMilliseconds, nullptr, &events, (void **) &source) < 0)
			{
				break;
			}
			if (source != nullptr)
			{
				source->process(app, source);
			}
			if (appState.Resumed && appState.NativeWindow != nullptr)
			{
				if (appState.Ovr == nullptr)
				{
					ovrModeParmsVulkan parms = vrapi_DefaultModeParmsVulkan(&appState.Java, (unsigned long long)appState.Context.queue);
					parms.ModeParms.Flags &= ~VRAPI_MODE_FLAG_RESET_WINDOW_FULLSCREEN;
					parms.ModeParms.Flags |= VRAPI_MODE_FLAG_NATIVE_WINDOW;
					parms.ModeParms.WindowSurface = (size_t)appState.NativeWindow;
					parms.ModeParms.Display = 0;
					parms.ModeParms.ShareContext = 0;
					appState.Ovr = vrapi_EnterVrMode((ovrModeParms*)&parms);
					if (appState.Ovr == nullptr)
					{
						__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main: vrapi_EnterVRMode() failed: invalid ANativeWindow");
						appState.NativeWindow = nullptr;
					}
					if (appState.Ovr != nullptr)
					{
						vrapi_SetClockLevels(appState.Ovr, appState.CpuLevel, appState.GpuLevel);
						vrapi_SetPerfThread(appState.Ovr, VRAPI_PERF_THREAD_TYPE_MAIN, appState.MainThreadTid);
						vrapi_SetPerfThread(appState.Ovr, VRAPI_PERF_THREAD_TYPE_RENDERER, appState.RenderThreadTid);
					}
				}
			}
			else if (appState.Ovr != nullptr)
			{
				vrapi_LeaveVrMode(appState.Ovr);
				appState.Ovr = nullptr;
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
						appState.Mode = AppWorldMode;
						appState.StartupButtonsPressed = false;
					}
				}
				else if ((appState.LeftButtons & ovrButton_X) != 0 && (appState.RightButtons & ovrButton_A) != 0)
				{
					appState.StartupButtonsPressed = true;
				}
			}
			else if (appState.Mode == AppScreenMode)
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
			else if (appState.Mode == AppWorldMode)
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
		if (appState.Ovr == nullptr)
		{
			continue;
		}
		if (!appState.Scene.createdScene)
		{
			int frameFlags = 0;
			frameFlags |= VRAPI_FRAME_FLAG_FLUSH;
			ovrLayerProjection2 blackLayer = vrapi_DefaultLayerBlackProjection2();
			blackLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;
			ovrLayerLoadingIcon2 iconLayer = vrapi_DefaultLayerLoadingIcon2();
			iconLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;
			const ovrLayerHeader2* layers[] { &blackLayer.Header, &iconLayer.Header };
			ovrSubmitFrameDescription2 frameDesc { };
			frameDesc.Flags = frameFlags;
			frameDesc.SwapInterval = 1;
			frameDesc.FrameIndex = appState.FrameIndex;
			frameDesc.DisplayTime = appState.DisplayTime;
			frameDesc.LayerCount = 2;
			frameDesc.Layers = layers;
			vrapi_SubmitFrame2(appState.Ovr, &frameDesc);
			appState.Console.Width = 960;
			appState.Console.Height = 600;
			appState.Console.SwapChain = vrapi_CreateTextureSwapChain3(VRAPI_TEXTURE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, appState.Console.Width, appState.Console.Height, 1, 1);
			appState.Console.Data.resize(appState.Console.Width * appState.Console.Height);
			appState.Console.Image = vrapi_GetTextureSwapChainBufferVulkan(appState.Console.SwapChain, 0);
			appState.Console.Buffer.size = appState.Console.Data.size() * sizeof(uint32_t);
			VkBufferCreateInfo bufferCreateInfo { };
			bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferCreateInfo.size = appState.Console.Buffer.size;
			bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			VK(appState.Device.vkCreateBuffer(appState.Device.device, &bufferCreateInfo, nullptr, &appState.Console.Buffer.buffer));
			appState.Console.Buffer.flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			VkMemoryRequirements memoryRequirements;
			VC(appState.Device.vkGetBufferMemoryRequirements(appState.Device.device, appState.Console.Buffer.buffer, &memoryRequirements));
			VkMemoryAllocateInfo memoryAllocateInfo { };
			memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			memoryAllocateInfo.allocationSize = memoryRequirements.size;
			auto requiredProperties = appState.Console.Buffer.flags;
			auto typeFound = false;
			for (uint32_t type = 0; type < appState.Context.device->physicalDeviceMemoryProperties.memoryTypeCount; type++)
			{
				if ((memoryRequirements.memoryTypeBits & (1 << type)) != 0)
				{
					const VkFlags propertyFlags = appState.Context.device->physicalDeviceMemoryProperties.memoryTypes[type].propertyFlags;
					if ((propertyFlags & requiredProperties) == requiredProperties)
					{
						typeFound = true;
						memoryAllocateInfo.memoryTypeIndex = type;
						break;
					}
				}
			}
			if (!typeFound)
			{
				__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Memory type %d with properties %d not found.", memoryRequirements.memoryTypeBits, requiredProperties);
				vrapi_Shutdown();
				exit(0);
			}
			VK(appState.Device.vkAllocateMemory(appState.Device.device, &memoryAllocateInfo, nullptr, &appState.Console.Buffer.memory));
			VK(appState.Device.vkBindBufferMemory(appState.Device.device, appState.Console.Buffer.buffer, appState.Console.Buffer.memory, 0));
			VK(appState.Device.vkAllocateCommandBuffers(appState.Device.device, &commandBufferAllocateInfo, &appState.Console.CommandBuffer));
			appState.Console.SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			appState.Console.SubmitInfo.commandBufferCount = 1;
			appState.Console.SubmitInfo.pCommandBuffers = &appState.Console.CommandBuffer;
			appState.Screen.Width = appState.Console.Width;
			appState.Screen.Height = appState.Console.Height;
			appState.Screen.SwapChain = vrapi_CreateTextureSwapChain3(VRAPI_TEXTURE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, appState.Screen.Width, appState.Screen.Height, 1, 1);
			appState.Screen.Data.resize(appState.Screen.Width * appState.Screen.Height, 255 << 24);
			appState.Screen.Image = vrapi_GetTextureSwapChainBufferVulkan(appState.Screen.SwapChain, 0);
			auto playImageFile = AAssetManager_open(app->activity->assetManager, "play.png", AASSET_MODE_BUFFER);
			auto playImageFileLength = AAsset_getLength(playImageFile);
			std::vector<stbi_uc> playImageSource(playImageFileLength);
			AAsset_read(playImageFile, playImageSource.data(), playImageFileLength);
			int playImageWidth;
			int playImageHeight;
			int playImageComponents;
			auto playImage = stbi_load_from_memory(playImageSource.data(), playImageFileLength, &playImageWidth, &playImageHeight, &playImageComponents, 4);
			auto texIndex = ((appState.Screen.Height - playImageHeight) * appState.Screen.Width + appState.Screen.Width - playImageWidth) / 2;
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
					appState.Screen.Data[texIndex] = ((uint32_t)255 << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
					texIndex++;
				}
				texIndex += appState.Screen.Width - playImageWidth;
			}
			stbi_image_free(playImage);
			for (auto b = 0; b < 5; b++)
			{
				auto i = (unsigned char)(192.0 * sin(M_PI / (double)(b - 1)));
				auto color = ((uint32_t)255 << 24) | ((uint32_t)i << 16) | ((uint32_t)i << 8) | i;
				auto texTopIndex = b * appState.Screen.Width + b;
				auto texBottomIndex = (appState.Screen.Height - 1 - b) * appState.Screen.Width + b;
				for (auto x = 0; x < appState.Screen.Width - b - b; x++)
				{
					appState.Screen.Data[texTopIndex] = color;
					texTopIndex++;
					appState.Screen.Data[texBottomIndex] = color;
					texBottomIndex++;
				}
				auto texLeftIndex = (b + 1) * appState.Screen.Width + b;
				auto texRightIndex = (b + 1) * appState.Screen.Width + appState.Screen.Width - 1 - b;
				for (auto y = 0; y < appState.Screen.Height - b - 1 - b - 1; y++)
				{
					appState.Screen.Data[texLeftIndex] = color;
					texLeftIndex += appState.Screen.Width;
					appState.Screen.Data[texRightIndex] = color;
					texRightIndex += appState.Screen.Width;
				}
			}
			appState.Screen.Buffer.size = appState.Screen.Data.size() * sizeof(uint32_t);
			bufferCreateInfo.size = appState.Screen.Buffer.size;
			VK(appState.Device.vkCreateBuffer(appState.Device.device, &bufferCreateInfo, nullptr, &appState.Screen.Buffer.buffer));
			appState.Screen.Buffer.flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			VC(appState.Device.vkGetBufferMemoryRequirements(appState.Device.device, appState.Screen.Buffer.buffer, &memoryRequirements));
			memoryAllocateInfo.allocationSize = memoryRequirements.size;
			requiredProperties = appState.Screen.Buffer.flags;
			typeFound = false;
			for (uint32_t type = 0; type < appState.Context.device->physicalDeviceMemoryProperties.memoryTypeCount; type++)
			{
				if ((memoryRequirements.memoryTypeBits & (1 << type)) != 0)
				{
					const VkFlags propertyFlags = appState.Context.device->physicalDeviceMemoryProperties.memoryTypes[type].propertyFlags;
					if ((propertyFlags & requiredProperties) == requiredProperties)
					{
						typeFound = true;
						memoryAllocateInfo.memoryTypeIndex = type;
						break;
					}
				}
			}
			if (!typeFound)
			{
				__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Memory type %d with properties %d not found.", memoryRequirements.memoryTypeBits, requiredProperties);
				vrapi_Shutdown();
				exit(0);
			}
			VK(appState.Device.vkAllocateMemory(appState.Device.device, &memoryAllocateInfo, nullptr, &appState.Screen.Buffer.memory));
			VK(appState.Device.vkBindBufferMemory(appState.Device.device, appState.Screen.Buffer.buffer, appState.Screen.Buffer.memory, 0));
			VK(appState.Device.vkMapMemory(appState.Device.device, appState.Screen.Buffer.memory, 0, memoryRequirements.size, 0, &appState.Screen.Buffer.mapped));
			memcpy(appState.Screen.Buffer.mapped, appState.Screen.Data.data(), appState.Screen.Buffer.size);
			VC(appState.Device.vkUnmapMemory(appState.Device.device, appState.Screen.Buffer.memory));
			appState.Screen.Buffer.mapped = nullptr;
			VkMappedMemoryRange mappedMemoryRange { };
			mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			mappedMemoryRange.memory = appState.Screen.Buffer.memory;
			VC(appState.Device.vkFlushMappedMemoryRanges(appState.Device.device, 1, &mappedMemoryRange));
			VK(appState.Device.vkAllocateCommandBuffers(appState.Device.device, &commandBufferAllocateInfo, &setupCommandBuffer));
			VK(appState.Device.vkBeginCommandBuffer(setupCommandBuffer, &setupCommandBufferBeginInfo));
			VkBufferImageCopy region { };
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.layerCount = 1;
			region.imageExtent.width = appState.Screen.Width;
			region.imageExtent.height = appState.Screen.Height;
			region.imageExtent.depth = 1;
			VC(appState.Device.vkCmdCopyBufferToImage(setupCommandBuffer, appState.Screen.Buffer.buffer, appState.Screen.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region));
			VK(appState.Device.vkEndCommandBuffer(setupCommandBuffer));
			VK(appState.Device.vkQueueSubmit(appState.Context.queue, 1, &setupSubmitInfo, VK_NULL_HANDLE));
			VK(appState.Device.vkQueueWaitIdle(appState.Context.queue));
			VC(appState.Device.vkFreeCommandBuffers(appState.Device.device, appState.Context.commandPool, 1, &setupCommandBuffer));
			VK(appState.Device.vkAllocateCommandBuffers(appState.Device.device, &commandBufferAllocateInfo, &appState.Screen.CommandBuffer));
			appState.Screen.SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			appState.Screen.SubmitInfo.commandBufferCount = 1;
			appState.Screen.SubmitInfo.pCommandBuffers = &appState.Screen.CommandBuffer;
			appState.Scene.numBuffers = (isMultiview) ? VRAPI_FRAME_LAYER_EYE_MAX : 1;
			auto texturedVertexFilename = (isMultiview ? "shaders/textured_multiview.vert.spv" : "shaders/textured.vert.spv");
			auto texturedVertexFile = AAssetManager_open(app->activity->assetManager, texturedVertexFilename, AASSET_MODE_BUFFER);
			size_t length = AAsset_getLength(texturedVertexFile);
			if ((length % 4) != 0)
			{
				__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): %s is not 4-byte aligned.", texturedVertexFilename);
				exit(0);
			}
			std::vector<unsigned char> buffer(length);
			AAsset_read(texturedVertexFile, buffer.data(), length);
			VkShaderModuleCreateInfo moduleCreateInfo { };
			moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			moduleCreateInfo.pCode = (uint32_t *)buffer.data();
			moduleCreateInfo.codeSize = length;
			VK(appState.Device.vkCreateShaderModule(appState.Device.device, &moduleCreateInfo, nullptr, &appState.Scene.texturedVertex));
			auto texturedFragmentFilename = "shaders/textured.frag.spv";
			auto texturedFragmentFile = AAssetManager_open(app->activity->assetManager, texturedFragmentFilename, AASSET_MODE_BUFFER);
			length = AAsset_getLength(texturedFragmentFile);
			if ((length % 4) != 0)
			{
				__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): %s is not 4-byte aligned.", texturedFragmentFilename);
				exit(0);
			}
			buffer.resize(length);
			AAsset_read(texturedFragmentFile, buffer.data(), length);
			moduleCreateInfo.pCode = (uint32_t *)buffer.data();
			moduleCreateInfo.codeSize = length;
			VK(appState.Device.vkCreateShaderModule(appState.Device.device, &moduleCreateInfo, nullptr, &appState.Scene.texturedFragment));
			auto turbulentFragmentFilename = "shaders/turbulent.frag.spv";
			auto turbulentFragmentFile = AAssetManager_open(app->activity->assetManager, turbulentFragmentFilename, AASSET_MODE_BUFFER);
			length = AAsset_getLength(turbulentFragmentFile);
			if ((length % 4) != 0)
			{
				__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): %s is not 4-byte aligned.", turbulentFragmentFilename);
				exit(0);
			}
			buffer.resize(length);
			AAsset_read(turbulentFragmentFile, buffer.data(), length);
			moduleCreateInfo.pCode = (uint32_t *)buffer.data();
			moduleCreateInfo.codeSize = length;
			VK(appState.Device.vkCreateShaderModule(appState.Device.device, &moduleCreateInfo, nullptr, &appState.Scene.turbulentFragment));
			appState.Scene.textured.stages.resize(2);
			appState.Scene.textured.stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			appState.Scene.textured.stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
			appState.Scene.textured.stages[0].module = appState.Scene.texturedVertex;
			appState.Scene.textured.stages[0].pName = "main";
			appState.Scene.textured.stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			appState.Scene.textured.stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			appState.Scene.textured.stages[1].module = appState.Scene.texturedFragment;
			appState.Scene.textured.stages[1].pName = "main";
			VkDescriptorSetLayoutBinding descriptorSetBindings[3] { };
			descriptorSetBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorSetBindings[0].descriptorCount = 1;
			descriptorSetBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
			descriptorSetBindings[1].binding = 1;
			descriptorSetBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorSetBindings[1].descriptorCount = 1;
			descriptorSetBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo { };
			descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			descriptorSetLayoutCreateInfo.bindingCount = 2;
			descriptorSetLayoutCreateInfo.pBindings = descriptorSetBindings;
			VK(appState.Device.vkCreateDescriptorSetLayout(appState.Device.device, &descriptorSetLayoutCreateInfo, nullptr, &appState.Scene.textured.descriptorSetLayout));
			VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo { };
			pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutCreateInfo.setLayoutCount = 1;
			pipelineLayoutCreateInfo.pSetLayouts = &appState.Scene.textured.descriptorSetLayout;
			VK(appState.Device.vkCreatePipelineLayout(appState.Device.device, &pipelineLayoutCreateInfo, nullptr, &appState.Scene.textured.pipelineLayout));
			appState.Scene.vertexAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			appState.Scene.vertexBindings[0].stride = 5 * sizeof(float);
			appState.Scene.vertexBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			appState.Scene.vertexAttributes[1].location = 1;
			appState.Scene.vertexAttributes[1].binding = 1;
			appState.Scene.vertexAttributes[1].format = VK_FORMAT_R32G32_SFLOAT;
			appState.Scene.vertexAttributes[1].offset = 3 * sizeof(float);;
			appState.Scene.vertexBindings[1].binding = 1;
			appState.Scene.vertexBindings[1].stride = 5 * sizeof(float);
			appState.Scene.vertexBindings[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			appState.Scene.vertexAttributes[2].location = 2;
			appState.Scene.vertexAttributes[2].binding = 2;
			appState.Scene.vertexAttributes[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			appState.Scene.vertexAttributes[3].location = 3;
			appState.Scene.vertexAttributes[3].binding = 2;
			appState.Scene.vertexAttributes[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			appState.Scene.vertexAttributes[3].offset = 4 * sizeof(float);
			appState.Scene.vertexAttributes[4].location = 4;
			appState.Scene.vertexAttributes[4].binding = 2;
			appState.Scene.vertexAttributes[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			appState.Scene.vertexAttributes[4].offset = 8 * sizeof(float);
			appState.Scene.vertexAttributes[5].location = 5;
			appState.Scene.vertexAttributes[5].binding = 2;
			appState.Scene.vertexAttributes[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			appState.Scene.vertexAttributes[5].offset = 12 * sizeof(float);
			appState.Scene.vertexBindings[2].binding = 2;
			appState.Scene.vertexBindings[2].stride = 16 * sizeof(float);
			appState.Scene.vertexBindings[2].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
			appState.Scene.vertexAttributeCount = 6;
			appState.Scene.vertexBindingCount = 3;
			appState.Scene.vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			appState.Scene.vertexInputState.vertexBindingDescriptionCount = appState.Scene.vertexBindingCount;
			appState.Scene.vertexInputState.pVertexBindingDescriptions = appState.Scene.vertexBindings;
			appState.Scene.vertexInputState.vertexAttributeDescriptionCount = appState.Scene.vertexAttributeCount;
			appState.Scene.vertexInputState.pVertexAttributeDescriptions = appState.Scene.vertexAttributes;
			appState.Scene.inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			appState.Scene.inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			VkPipelineTessellationStateCreateInfo tessellationStateCreateInfo { };
			tessellationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
			VkPipelineViewportStateCreateInfo viewportStateCreateInfo { };
			viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportStateCreateInfo.viewportCount = 1;
			viewportStateCreateInfo.scissorCount = 1;
			VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo { };
			rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
			rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
			rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
			rasterizationStateCreateInfo.lineWidth = 1.0f;
			VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo { };
			multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_4_BIT;
			multisampleStateCreateInfo.minSampleShading = 1.0f;
			VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo { };
			depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			depthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
			depthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
			depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
			depthStencilStateCreateInfo.front.failOp = VK_STENCIL_OP_KEEP;
			depthStencilStateCreateInfo.front.passOp = VK_STENCIL_OP_KEEP;
			depthStencilStateCreateInfo.front.depthFailOp = VK_STENCIL_OP_KEEP;
			depthStencilStateCreateInfo.front.compareOp = VK_COMPARE_OP_ALWAYS;
			depthStencilStateCreateInfo.back.failOp = VK_STENCIL_OP_KEEP;
			depthStencilStateCreateInfo.back.passOp = VK_STENCIL_OP_KEEP;
			depthStencilStateCreateInfo.back.depthFailOp = VK_STENCIL_OP_KEEP;
			depthStencilStateCreateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
			depthStencilStateCreateInfo.minDepthBounds = 0.0f;
			depthStencilStateCreateInfo.maxDepthBounds = 1.0f;
			VkPipelineColorBlendAttachmentState colorBlendAttachmentState { };
			colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
			colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
			colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
			colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
			VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo { };
			colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_CLEAR;
			colorBlendStateCreateInfo.attachmentCount = 1;
			colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;
			VkDynamicState dynamicStateEnables[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
			VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo { };
			pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			pipelineDynamicStateCreateInfo.dynamicStateCount = 2;
			pipelineDynamicStateCreateInfo.pDynamicStates = dynamicStateEnables;
			VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo { };
			graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			graphicsPipelineCreateInfo.stageCount = appState.Scene.textured.stages.size();
			graphicsPipelineCreateInfo.pStages = appState.Scene.textured.stages.data();
			graphicsPipelineCreateInfo.pVertexInputState = &appState.Scene.vertexInputState;
			graphicsPipelineCreateInfo.pInputAssemblyState = &appState.Scene.inputAssemblyState;
			graphicsPipelineCreateInfo.pTessellationState = &tessellationStateCreateInfo;
			graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
			graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
			graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
			graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
			graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
			graphicsPipelineCreateInfo.pDynamicState = &pipelineDynamicStateCreateInfo;
			graphicsPipelineCreateInfo.layout = appState.Scene.textured.pipelineLayout;
			graphicsPipelineCreateInfo.renderPass = appState.RenderPass;
			VK(appState.Device.vkCreateGraphicsPipelines(appState.Device.device, appState.Context.pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &appState.Scene.textured.pipeline));
			appState.Scene.turbulent.stages.resize(2);
			appState.Scene.turbulent.stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			appState.Scene.turbulent.stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
			appState.Scene.turbulent.stages[0].module = appState.Scene.texturedVertex;
			appState.Scene.turbulent.stages[0].pName = "main";
			appState.Scene.turbulent.stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			appState.Scene.turbulent.stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			appState.Scene.turbulent.stages[1].module = appState.Scene.turbulentFragment;
			appState.Scene.turbulent.stages[1].pName = "main";
			descriptorSetBindings[2].binding = 2;
			descriptorSetBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorSetBindings[2].descriptorCount = 1;
			descriptorSetBindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			descriptorSetLayoutCreateInfo.bindingCount = 3;
			VK(appState.Device.vkCreateDescriptorSetLayout(appState.Device.device, &descriptorSetLayoutCreateInfo, nullptr, &appState.Scene.turbulent.descriptorSetLayout));
			pipelineLayoutCreateInfo.pSetLayouts = &appState.Scene.turbulent.descriptorSetLayout;
			VK(appState.Device.vkCreatePipelineLayout(appState.Device.device, &pipelineLayoutCreateInfo, nullptr, &appState.Scene.turbulent.pipelineLayout));
			graphicsPipelineCreateInfo.stageCount = appState.Scene.turbulent.stages.size();
			graphicsPipelineCreateInfo.pStages = appState.Scene.turbulent.stages.data();
			graphicsPipelineCreateInfo.layout = appState.Scene.turbulent.pipelineLayout;
			VK(appState.Device.vkCreateGraphicsPipelines(appState.Device.device, appState.Context.pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &appState.Scene.turbulent.pipeline));
			appState.Scene.matrices.size = appState.Scene.numBuffers * 2 * sizeof(ovrMatrix4f);
			bufferCreateInfo.size = appState.Scene.matrices.size;
			bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			VK(appState.Device.vkCreateBuffer(appState.Device.device, &bufferCreateInfo, nullptr, &appState.Scene.matrices.buffer));
			appState.Scene.matrices.flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			VC(appState.Device.vkGetBufferMemoryRequirements(appState.Device.device, appState.Scene.matrices.buffer, &memoryRequirements));
			memoryAllocateInfo.allocationSize = memoryRequirements.size;
			requiredProperties = appState.Scene.matrices.flags;
			typeFound = false;
			for (uint32_t type = 0; type < appState.Context.device->physicalDeviceMemoryProperties.memoryTypeCount; type++)
			{
				if ((memoryRequirements.memoryTypeBits & (1 << type)) != 0)
				{
					const VkFlags propertyFlags = appState.Context.device->physicalDeviceMemoryProperties.memoryTypes[type].propertyFlags;
					if ((propertyFlags & requiredProperties) == requiredProperties)
					{
						typeFound = true;
						memoryAllocateInfo.memoryTypeIndex = type;
						break;
					}
				}
			}
			if (!typeFound)
			{
				__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Memory type %d with properties %d not found.", memoryRequirements.memoryTypeBits, requiredProperties);
				vrapi_Shutdown();
				exit(0);
			}
			VK(appState.Device.vkAllocateMemory(appState.Device.device, &memoryAllocateInfo, nullptr, &appState.Scene.matrices.memory));
			VK(appState.Device.vkBindBufferMemory(appState.Device.device, appState.Scene.matrices.buffer, appState.Scene.matrices.memory, 0));
			appState.Scene.createdScene = true;
		}
		appState.FrameIndex++;
		const double predictedDisplayTime = vrapi_GetPredictedDisplayTime(appState.Ovr, appState.FrameIndex);
		const ovrTracking2 tracking = vrapi_GetPredictedTracking2(appState.Ovr, predictedDisplayTime);
		appState.DisplayTime = predictedDisplayTime;
		if (appState.Mode != AppStartupMode)
		{
			if (cls.demoplayback || cl.intermission)
			{
				appState.Mode = AppScreenMode;
			}
			else
			{
				appState.Mode = AppWorldMode;
			}
		}
		if (appState.PreviousMode != appState.Mode)
		{
			if (appState.PreviousMode == AppStartupMode)
			{
				sys_argc = 3;
				sys_argv = new char *[sys_argc];
				sys_argv[0] = (char *) "SlipNFrag";
				sys_argv[1] = (char *) "-basedir";
				sys_argv[2] = (char *) "/sdcard/android/data/com.heribertodelgado.slipnfrag/files";
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
				appState.DefaultFOV = (int)Cvar_VariableValue("fov");
			}
			if (appState.Mode == AppScreenMode)
			{
				d_uselists = false;
				Cvar_SetValue("joystick", 1);
				Cvar_SetValue("joyadvanced", 1);
				Cvar_SetValue("joyadvaxisx", AxisSide);
				Cvar_SetValue("joyadvaxisy", AxisForward);
				Cvar_SetValue("joyadvaxisz", AxisTurn);
				Cvar_SetValue("joyadvaxisr", AxisLook);
				Joy_AdvancedUpdate_f();
				vid_width = appState.Screen.Width;
				vid_height = appState.Screen.Height;
				con_width = appState.Console.Width / 3;
				con_height = appState.Console.Height / 3;
				Cvar_SetValue("fov", appState.DefaultFOV);
				VID_Resize();
			}
			else if (appState.Mode == AppWorldMode)
			{
				d_uselists = true;
				Cvar_SetValue("joystick", 1);
				Cvar_SetValue("joyadvanced", 1);
				Cvar_SetValue("joyadvaxisx", AxisSide);
				Cvar_SetValue("joyadvaxisy", AxisForward);
				Cvar_SetValue("joyadvaxisz", AxisNada);
				Cvar_SetValue("joyadvaxisr", AxisNada);
				Joy_AdvancedUpdate_f();
				vid_width = appState.Screen.Width;
				vid_height = appState.Screen.Width;
				con_width = appState.Console.Width / 3;
				con_height = appState.Console.Height / 3;
				Cvar_SetValue("fov", appState.FOV);
				VID_Resize();
			}
			appState.PreviousMode = appState.Mode;
		}
		for (auto i = 0; i < VRAPI_FRAME_LAYER_EYE_MAX; i++)
		{
			appState.ViewMatrices[i] = ovrMatrix4f_Transpose(&tracking.Eye[i].ViewMatrix);
			appState.ProjectionMatrices[i] = ovrMatrix4f_Transpose(&tracking.Eye[i].ProjectionMatrix);
		}
		auto& orientation = tracking.HeadPose.Pose.Orientation;
		auto x = orientation.x;
		auto y = orientation.y;
		auto z = orientation.z;
		auto w = orientation.w;
		float Q[3] = { x, y, z };
		float ww = w * w;
		float Q11 = Q[1] * Q[1];
		float Q22 = Q[0] * Q[0];
		float Q33 = Q[2] * Q[2];
		const float psign = -1;
		float s2 = psign * 2 * (psign * w * Q[0] + Q[1] * Q[2]);
		const float singularityRadius = 1e-12;
		if (s2 < singularityRadius - 1)
		{
			appState.Yaw = 0;
			appState.Pitch = -M_PI / 2;
		}
		else if (s2 > 1 - singularityRadius)
		{
			appState.Yaw = 0;
			appState.Pitch = M_PI / 2;
		}
		else
		{
			appState.Yaw = -(atan2(-2 * (w * Q[1] - psign * Q[0] * Q[2]), ww + Q33 - Q11 - Q22));
			appState.Pitch = -asin(s2);
		}
		bool host_updated = false;
		if (host_initialized)
		{
			if (appState.Mode == AppWorldMode)
			{
				cl.viewangles[YAW] = appState.Yaw * 180 / M_PI + 90;
				cl.viewangles[PITCH] = appState.Pitch * 180 / M_PI;
			}
			if (appState.PreviousTime < 0)
			{
				appState.PreviousTime = getTime();
			}
			else if (appState.CurrentTime < 0)
			{
				appState.CurrentTime = getTime();
			}
			else
			{
				appState.PreviousTime = appState.CurrentTime;
				appState.CurrentTime = getTime();
				frame_lapse = (float) (appState.CurrentTime - appState.PreviousTime);
			}
			if (r_cache_thrash)
			{
				VID_ReallocSurfCache();
			}
			host_updated = Host_FrameUpdate(frame_lapse);
			if (sys_errormessage.length() > 0)
			{
				exit(0);
			}
			if (host_updated)
			{
				if (d_uselists)
				{
					d_lists.last_textured = -1;
					d_lists.last_turbulent = -1;
					d_lists.last_alias = -1;
					d_lists.last_vertex = -1;
					d_lists.last_index = -1;
					d_lists.clear_color = -1;
					auto nodrift = cl.nodrift;
					cl.nodrift = true;
					Host_FrameRender();
					cl.nodrift = nodrift;
				}
				else
				{
					Host_FrameRender();
				}
			}
			Host_FrameFinish(host_updated);
		}
		auto matrixIndex = 0;
		for (auto& view : appState.Views)
		{
			VkRect2D screenRect { };
			screenRect.extent.width = view.framebuffer.width;
			screenRect.extent.height = view.framebuffer.height;
			view.index = (view.index + 1) % view.framebuffer.swapChainLength;
			auto& perImage = view.perImage[view.index];
			if (perImage.submitted)
			{
				VK(appState.Device.vkWaitForFences(appState.Device.device, 1, &perImage.fence, VK_TRUE, 1ULL * 1000 * 1000 * 1000));
				VK(appState.Device.vkResetFences(appState.Device.device, 1, &perImage.fence));
				perImage.submitted = false;
			}
			for (Buffer **b = &perImage.sceneMatricesStagingBuffers.oldMapped; *b != nullptr; )
			{
				(*b)->unusedCount++;
				if ((*b)->unusedCount >= MAX_UNUSED_COUNT)
				{
					Buffer *next = (*b)->next;
					if ((*b)->mapped != nullptr)
					{
						VC(appState.Device.vkUnmapMemory(appState.Device.device, (*b)->memory));
					}
					VC(appState.Device.vkDestroyBuffer(appState.Device.device, (*b)->buffer, nullptr));
					VC(appState.Device.vkFreeMemory(appState.Device.device, (*b)->memory, nullptr));
					delete *b;
					*b = next;
				}
				else
				{
					b = &(*b)->next;
				}
			}
			for (Buffer *b = perImage.sceneMatricesStagingBuffers.mapped, *next = nullptr; b != nullptr; b = next)
			{
				next = b->next;
				b->next = perImage.sceneMatricesStagingBuffers.oldMapped;
				perImage.sceneMatricesStagingBuffers.oldMapped = b;
			}
			perImage.sceneMatricesStagingBuffers.mapped = nullptr;
			for (Buffer **b = &perImage.texturedVertices.oldMapped; *b != nullptr; )
			{
				(*b)->unusedCount++;
				if ((*b)->unusedCount >= MAX_UNUSED_COUNT)
				{
					Buffer *next = (*b)->next;
					if ((*b)->mapped != nullptr)
					{
						VC(appState.Device.vkUnmapMemory(appState.Device.device, (*b)->memory));
					}
					VC(appState.Device.vkDestroyBuffer(appState.Device.device, (*b)->buffer, nullptr));
					VC(appState.Device.vkFreeMemory(appState.Device.device, (*b)->memory, nullptr));
					delete *b;
					*b = next;
				}
				else
				{
					b = &(*b)->next;
				}
			}
			for (Buffer *b = perImage.texturedVertices.mapped, *next = nullptr; b != nullptr; b = next)
			{
				next = b->next;
				b->next = perImage.texturedVertices.oldMapped;
				perImage.texturedVertices.oldMapped = b;
			}
			perImage.texturedVertices.mapped = nullptr;
			for (Buffer **b = &perImage.texturedIndices.oldMapped; *b != nullptr; )
			{
				(*b)->unusedCount++;
				if ((*b)->unusedCount >= MAX_UNUSED_COUNT)
				{
					Buffer *next = (*b)->next;
					if ((*b)->mapped != nullptr)
					{
						VC(appState.Device.vkUnmapMemory(appState.Device.device, (*b)->memory));
					}
					VC(appState.Device.vkDestroyBuffer(appState.Device.device, (*b)->buffer, nullptr));
					VC(appState.Device.vkFreeMemory(appState.Device.device, (*b)->memory, nullptr));
					delete *b;
					*b = next;
				}
				else
				{
					b = &(*b)->next;
				}
			}
			for (Buffer *b = perImage.texturedIndices.mapped, *next = nullptr; b != nullptr; b = next)
			{
				next = b->next;
				b->next = perImage.texturedIndices.oldMapped;
				perImage.texturedIndices.oldMapped = b;
			}
			perImage.texturedIndices.mapped = nullptr;
			for (Buffer **b = &perImage.times.oldMapped; *b != nullptr; )
			{
				(*b)->unusedCount++;
				if ((*b)->unusedCount >= MAX_UNUSED_COUNT)
				{
					Buffer *next = (*b)->next;
					if ((*b)->mapped != nullptr)
					{
						VC(appState.Device.vkUnmapMemory(appState.Device.device, (*b)->memory));
					}
					VC(appState.Device.vkDestroyBuffer(appState.Device.device, (*b)->buffer, nullptr));
					VC(appState.Device.vkFreeMemory(appState.Device.device, (*b)->memory, nullptr));
					delete *b;
					*b = next;
				}
				else
				{
					b = &(*b)->next;
				}
			}
			for (Buffer *b = perImage.times.mapped, *next = nullptr; b != nullptr; b = next)
			{
				next = b->next;
				b->next = perImage.times.oldMapped;
				perImage.times.oldMapped = b;
			}
			perImage.times.mapped = nullptr;
			for (Buffer **b = &perImage.instances.oldMapped; *b != nullptr; )
			{
				(*b)->unusedCount++;
				if ((*b)->unusedCount >= MAX_UNUSED_COUNT)
				{
					Buffer *next = (*b)->next;
					if ((*b)->mapped != nullptr)
					{
						VC(appState.Device.vkUnmapMemory(appState.Device.device, (*b)->memory));
					}
					VC(appState.Device.vkDestroyBuffer(appState.Device.device, (*b)->buffer, nullptr));
					VC(appState.Device.vkFreeMemory(appState.Device.device, (*b)->memory, nullptr));
					delete *b;
					*b = next;
				}
				else
				{
					b = &(*b)->next;
				}
			}
			for (Buffer *b = perImage.instances.mapped, *next = nullptr; b != nullptr; b = next)
			{
				next = b->next;
				b->next = perImage.instances.oldMapped;
				perImage.instances.oldMapped = b;
			}
			perImage.instances.mapped = nullptr;
			for (Buffer **b = &perImage.stagingBuffers.oldMapped; *b != nullptr; )
			{
				(*b)->unusedCount++;
				if ((*b)->unusedCount >= MAX_UNUSED_COUNT)
				{
					Buffer *next = (*b)->next;
					if ((*b)->mapped != nullptr)
					{
						VC(appState.Device.vkUnmapMemory(appState.Device.device, (*b)->memory));
					}
					VC(appState.Device.vkDestroyBuffer(appState.Device.device, (*b)->buffer, nullptr));
					VC(appState.Device.vkFreeMemory(appState.Device.device, (*b)->memory, nullptr));
					delete *b;
					*b = next;
				}
				else
				{
					b = &(*b)->next;
				}
			}
			for (Buffer *b = perImage.stagingBuffers.mapped, *next = nullptr; b != nullptr; b = next)
			{
				next = b->next;
				b->next = perImage.stagingBuffers.oldMapped;
				perImage.stagingBuffers.oldMapped = b;
			}
			perImage.stagingBuffers.mapped = nullptr;
			for (Texture **t = &perImage.textures.oldTextures; *t != nullptr; )
			{
				(*t)->unusedCount++;
				if ((*t)->unusedCount >= MAX_UNUSED_COUNT)
				{
					Texture *next = (*t)->next;
					VC(appState.Device.vkDestroyImageView(appState.Device.device, (*t)->view, nullptr));
					VC(appState.Device.vkDestroyImage(appState.Device.device, (*t)->image, nullptr));
					VC(appState.Device.vkFreeMemory(appState.Device.device, (*t)->memory, nullptr));
					VC(appState.Device.vkDestroySampler(appState.Device.device, (*t)->sampler, nullptr));
					delete *t;
					*t = next;
				}
				else
				{
					t = &(*t)->next;
				}
			}
			for (Texture *t = perImage.textures.textures, *next = nullptr; t != nullptr; t = next)
			{
				next = t->next;
				t->next = perImage.textures.oldTextures;
				perImage.textures.oldTextures = t;
			}
			perImage.textures.textures = nullptr;
			for (PipelineResources **r = &perImage.pipelineResources; *r != nullptr; )
			{
				(*r)->unusedCount++;
				if ((*r)->unusedCount >= MAX_UNUSED_COUNT)
				{
					PipelineResources *next = (*r)->next;
					VC(appState.Device.vkFreeDescriptorSets(appState.Device.device, (*r)->descriptorPool, 1, &(*r)->descriptorSet));
					VC(appState.Device.vkDestroyDescriptorPool(appState.Device.device, (*r)->descriptorPool, nullptr));
					delete *r;
					*r = next;
				}
				else
				{
					r = &(*r)->next;
				}
			}
			VK(appState.Device.vkResetCommandBuffer(perImage.commandBuffer, 0));
			VK(appState.Device.vkBeginCommandBuffer(perImage.commandBuffer, &commandBufferBeginInfo));
			VkMemoryBarrier memoryBarrier { };
			memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
			memoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
			VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr));
			view.framebuffer.currentBuffer = (view.framebuffer.currentBuffer + 1) % view.framebuffer.swapChainLength;
			auto& texture = view.framebuffer.colorTextures[view.framebuffer.currentBuffer];
			VkImageMemoryBarrier imageMemoryBarrier { };
			imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			imageMemoryBarrier.image = texture.image;
			imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageMemoryBarrier.subresourceRange.levelCount = 1;
			imageMemoryBarrier.subresourceRange.layerCount = texture.layerCount;
			VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
			Buffer* stagingBuffer;
			if (perImage.sceneMatricesStagingBuffers.oldMapped != nullptr)
			{
				stagingBuffer = perImage.sceneMatricesStagingBuffers.oldMapped;
				perImage.sceneMatricesStagingBuffers.oldMapped = perImage.sceneMatricesStagingBuffers.oldMapped->next;
			}
			else
			{
				stagingBuffer = new Buffer();
				memset(stagingBuffer, 0, sizeof(Buffer));
				stagingBuffer->size = appState.Scene.matrices.size;
				VkBufferCreateInfo bufferCreateInfo { };
				bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				bufferCreateInfo.size = stagingBuffer->size;
				bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
				bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
				VK(appState.Device.vkCreateBuffer(appState.Device.device, &bufferCreateInfo, nullptr, &stagingBuffer->buffer));
				stagingBuffer->flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
				VkMemoryRequirements memoryRequirements;
				VC(appState.Device.vkGetBufferMemoryRequirements(appState.Device.device, stagingBuffer->buffer, &memoryRequirements));
				VkMemoryAllocateInfo memoryAllocateInfo { };
				memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
				memoryAllocateInfo.allocationSize = memoryRequirements.size;
				auto requiredProperties = stagingBuffer->flags;
				auto typeFound = false;
				for (uint32_t type = 0; type < appState.Context.device->physicalDeviceMemoryProperties.memoryTypeCount; type++)
				{
					if ((memoryRequirements.memoryTypeBits & (1 << type)) != 0)
					{
						const VkFlags propertyFlags = appState.Context.device->physicalDeviceMemoryProperties.memoryTypes[type].propertyFlags;
						if ((propertyFlags & requiredProperties) == requiredProperties)
						{
							typeFound = true;
							memoryAllocateInfo.memoryTypeIndex = type;
							break;
						}
					}
				}
				if (!typeFound)
				{
					__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Memory type %d with properties %d not found.", memoryRequirements.memoryTypeBits, requiredProperties);
					vrapi_Shutdown();
					exit(0);
				}
				VK(appState.Device.vkAllocateMemory(appState.Device.device, &memoryAllocateInfo, nullptr, &stagingBuffer->memory));
				VK(appState.Device.vkBindBufferMemory(appState.Device.device, stagingBuffer->buffer, stagingBuffer->memory, 0));
			}
			stagingBuffer->unusedCount = 0;
			stagingBuffer->next = perImage.sceneMatricesStagingBuffers.mapped;
			perImage.sceneMatricesStagingBuffers.mapped = stagingBuffer;
			VK(appState.Device.vkMapMemory(appState.Device.device, stagingBuffer->memory, 0, stagingBuffer->size, 0, &stagingBuffer->mapped));
			ovrMatrix4f *sceneMatrices = nullptr;
			*((void**)&sceneMatrices) = stagingBuffer->mapped;
			memcpy(sceneMatrices, &appState.ViewMatrices[matrixIndex], appState.Scene.numBuffers * sizeof(ovrMatrix4f));
			memcpy(sceneMatrices + appState.Scene.numBuffers, &appState.ProjectionMatrices[matrixIndex], appState.Scene.numBuffers * sizeof(ovrMatrix4f));
			VC(appState.Device.vkUnmapMemory(appState.Device.device, stagingBuffer->memory));
			stagingBuffer->mapped = nullptr;
			VkBufferMemoryBarrier bufferMemoryBarrier { };
			bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			bufferMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
			bufferMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			bufferMemoryBarrier.buffer = stagingBuffer->buffer;
			bufferMemoryBarrier.size = stagingBuffer->size;
			VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &bufferMemoryBarrier, 0, nullptr));
			VkBufferCopy bufferCopy { };
			bufferCopy.size = appState.Scene.matrices.size;
			VC(appState.Device.vkCmdCopyBuffer(perImage.commandBuffer, stagingBuffer->buffer, appState.Scene.matrices.buffer, 1, &bufferCopy));
			bufferMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			bufferMemoryBarrier.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT;
			bufferMemoryBarrier.buffer = appState.Scene.matrices.buffer;
			bufferMemoryBarrier.size = appState.Scene.matrices.size;
			VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr, 1, &bufferMemoryBarrier, 0, nullptr));
			double clearR;
			double clearG;
			double clearB;
			double clearA;
			if (d_lists.clear_color >= 0)
			{
				auto color = d_8to24table[d_lists.clear_color];
				clearR = (color & 255) / 255.0f;
				clearG = (color >> 8 & 255) / 255.0f;
				clearB = (color >> 16 & 255) / 255.0f;
				clearA = (color >> 24) / 255.0f;
			}
			else
			{
				clearR = 0;
				clearG = 0;
				clearB = 0;
				clearA = 1;
			}
			uint32_t clearValueCount = 0;
			VkClearValue clearValues[3] { };
			clearValues[clearValueCount].color.float32[0] = clearR;
			clearValues[clearValueCount].color.float32[1] = clearG;
			clearValues[clearValueCount].color.float32[2] = clearB;
			clearValues[clearValueCount].color.float32[3] = clearA;
			clearValueCount++;
			clearValues[clearValueCount].color.float32[0] = clearR;
			clearValues[clearValueCount].color.float32[1] = clearG;
			clearValues[clearValueCount].color.float32[2] = clearB;
			clearValues[clearValueCount].color.float32[3] = clearA;
			clearValueCount++;
			clearValues[clearValueCount].depthStencil.depth = 1;
			clearValueCount++;
			VkRenderPassBeginInfo renderPassBeginInfo { };
			renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassBeginInfo.renderPass = appState.RenderPass;
			renderPassBeginInfo.framebuffer = view.framebuffer.framebuffers[view.framebuffer.currentBuffer];
			renderPassBeginInfo.renderArea = screenRect;
			renderPassBeginInfo.clearValueCount = clearValueCount;
			renderPassBeginInfo.pClearValues = clearValues;
			VC(appState.Device.vkCmdBeginRenderPass(perImage.commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE));
			VkViewport viewport;
			viewport.x = (float) screenRect.offset.x;
			viewport.y = (float) screenRect.offset.y;
			viewport.width = (float) screenRect.extent.width;
			viewport.height = (float) screenRect.extent.height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			VC(appState.Device.vkCmdSetViewport(perImage.commandBuffer, 0, 1, &viewport));
			VC(appState.Device.vkCmdSetScissor(perImage.commandBuffer, 0, 1, &screenRect));
			if (d_uselists && host_initialized)
			{
				auto size = (d_lists.last_vertex + 1) * sizeof(float);
				if (size > 0)
				{
					Buffer *texturedVertices = nullptr;
					for (Buffer **b = &perImage.texturedVertices.oldMapped; *b != nullptr; b = &(*b)->next)
					{
						if ((*b)->size >= size)
						{
							texturedVertices = *b;
							*b = (*b)->next;
							break;
						}
					}
					if (texturedVertices == nullptr)
					{
						texturedVertices = new Buffer();
						memset(texturedVertices, 0, sizeof(Buffer));
						texturedVertices->size = size;
						VkBufferCreateInfo bufferCreateInfo { };
						bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
						bufferCreateInfo.size = texturedVertices->size;
						bufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
						bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
						VK(appState.Device.vkCreateBuffer(appState.Device.device, &bufferCreateInfo, nullptr, &texturedVertices->buffer));
						texturedVertices->flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
						VkMemoryRequirements memoryRequirements;
						VC(appState.Device.vkGetBufferMemoryRequirements(appState.Device.device, texturedVertices->buffer, &memoryRequirements));
						VkMemoryAllocateInfo memoryAllocateInfo { };
						memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
						memoryAllocateInfo.allocationSize = memoryRequirements.size;
						auto requiredProperties = texturedVertices->flags;
						auto typeFound = false;
						for (uint32_t type = 0; type < appState.Context.device->physicalDeviceMemoryProperties.memoryTypeCount; type++)
						{
							if ((memoryRequirements.memoryTypeBits & (1 << type)) != 0)
							{
								const VkFlags propertyFlags = appState.Context.device->physicalDeviceMemoryProperties.memoryTypes[type].propertyFlags;
								if ((propertyFlags & requiredProperties) == requiredProperties)
								{
									typeFound = true;
									memoryAllocateInfo.memoryTypeIndex = type;
									break;
								}
							}
						}
						if (!typeFound)
						{
							__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Memory type %d with properties %d not found.", memoryRequirements.memoryTypeBits, requiredProperties);
							vrapi_Shutdown();
							exit(0);
						}
						VK(appState.Device.vkAllocateMemory(appState.Device.device, &memoryAllocateInfo, nullptr, &texturedVertices->memory));
						VK(appState.Device.vkBindBufferMemory(appState.Device.device, texturedVertices->buffer, texturedVertices->memory, 0));
					}
					texturedVertices->unusedCount = 0;
					texturedVertices->next = perImage.texturedVertices.mapped;
					perImage.texturedVertices.mapped = texturedVertices;
					VK(appState.Device.vkMapMemory(appState.Device.device, texturedVertices->memory, 0, texturedVertices->size, 0, &texturedVertices->mapped));
					memcpy(texturedVertices->mapped, d_lists.vertices.data(), size);
					VC(appState.Device.vkUnmapMemory(appState.Device.device, texturedVertices->memory));
					texturedVertices->mapped = nullptr;
					bufferMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
					bufferMemoryBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
					bufferMemoryBarrier.buffer = texturedVertices->buffer;
					bufferMemoryBarrier.size = texturedVertices->size;
					VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &bufferMemoryBarrier, 0, nullptr));
					Buffer *texturedIndices = nullptr;
					size = (d_lists.last_index + 1) * sizeof(int);
					for (Buffer **b = &perImage.texturedIndices.oldMapped; *b != nullptr; b = &(*b)->next)
					{
						if ((*b)->size >= size)
						{
							texturedIndices = *b;
							*b = (*b)->next;
							break;
						}
					}
					if (texturedIndices == nullptr)
					{
						texturedIndices = new Buffer();
						memset(texturedIndices, 0, sizeof(Buffer));
						texturedIndices->size = size;
						VkBufferCreateInfo bufferCreateInfo { };
						bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
						bufferCreateInfo.size = texturedIndices->size;
						bufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
						bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
						VK(appState.Device.vkCreateBuffer(appState.Device.device, &bufferCreateInfo, nullptr, &texturedIndices->buffer));
						texturedIndices->flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
						VkMemoryRequirements memoryRequirements;
						VC(appState.Device.vkGetBufferMemoryRequirements(appState.Device.device, texturedIndices->buffer, &memoryRequirements));
						VkMemoryAllocateInfo memoryAllocateInfo { };
						memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
						memoryAllocateInfo.allocationSize = memoryRequirements.size;
						auto requiredProperties = texturedIndices->flags;
						auto typeFound = false;
						for (uint32_t type = 0; type < appState.Context.device->physicalDeviceMemoryProperties.memoryTypeCount; type++)
						{
							if ((memoryRequirements.memoryTypeBits & (1 << type)) != 0)
							{
								const VkFlags propertyFlags = appState.Context.device->physicalDeviceMemoryProperties.memoryTypes[type].propertyFlags;
								if ((propertyFlags & requiredProperties) == requiredProperties)
								{
									typeFound = true;
									memoryAllocateInfo.memoryTypeIndex = type;
									break;
								}
							}
						}
						if (!typeFound)
						{
							__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Memory type %d with properties %d not found.", memoryRequirements.memoryTypeBits, requiredProperties);
							vrapi_Shutdown();
							exit(0);
						}
						VK(appState.Device.vkAllocateMemory(appState.Device.device, &memoryAllocateInfo, nullptr, &texturedIndices->memory));
						VK(appState.Device.vkBindBufferMemory(appState.Device.device, texturedIndices->buffer, texturedIndices->memory, 0));
					}
					texturedIndices->unusedCount = 0;
					texturedIndices->next = perImage.texturedIndices.mapped;
					perImage.texturedIndices.mapped = texturedIndices;
					VK(appState.Device.vkMapMemory(appState.Device.device, texturedIndices->memory, 0, texturedIndices->size, 0, &texturedIndices->mapped));
					memcpy(texturedIndices->mapped, d_lists.indices.data(), size);
					VC(appState.Device.vkUnmapMemory(appState.Device.device, texturedIndices->memory));
					texturedIndices->mapped = nullptr;
					bufferMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
					bufferMemoryBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
					bufferMemoryBarrier.buffer = texturedIndices->buffer;
					bufferMemoryBarrier.size = texturedIndices->size;
					VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &bufferMemoryBarrier, 0, nullptr));
					Buffer *instance = nullptr;
					if (perImage.instances.oldMapped != nullptr)
					{
						instance = perImage.instances.oldMapped;
						perImage.instances.oldMapped = perImage.instances.oldMapped->next;
					}
					else
					{
						instance = new Buffer();
						memset(instance, 0, sizeof(Buffer));
						instance->size = 16 * sizeof(float);
						VkBufferCreateInfo bufferCreateInfo { };
						bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
						bufferCreateInfo.size = instance->size;
						bufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
						bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
						VK(appState.Device.vkCreateBuffer(appState.Device.device, &bufferCreateInfo, nullptr, &instance->buffer));
						instance->flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
						VkMemoryRequirements memoryRequirements;
						VC(appState.Device.vkGetBufferMemoryRequirements(appState.Device.device, instance->buffer, &memoryRequirements));
						VkMemoryAllocateInfo memoryAllocateInfo { };
						memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
						memoryAllocateInfo.allocationSize = memoryRequirements.size;
						auto requiredProperties = instance->flags;
						auto typeFound = false;
						for (uint32_t type = 0; type < appState.Context.device->physicalDeviceMemoryProperties.memoryTypeCount; type++)
						{
							if ((memoryRequirements.memoryTypeBits & (1 << type)) != 0)
							{
								const VkFlags propertyFlags = appState.Context.device->physicalDeviceMemoryProperties.memoryTypes[type].propertyFlags;
								if ((propertyFlags & requiredProperties) == requiredProperties)
								{
									typeFound = true;
									memoryAllocateInfo.memoryTypeIndex = type;
									break;
								}
							}
						}
						if (!typeFound)
						{
							__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Memory type %d with properties %d not found.", memoryRequirements.memoryTypeBits, requiredProperties);
							vrapi_Shutdown();
							exit(0);
						}
						VK(appState.Device.vkAllocateMemory(appState.Device.device, &memoryAllocateInfo, nullptr, &instance->memory));
						VK(appState.Device.vkBindBufferMemory(appState.Device.device, instance->buffer, instance->memory, 0));
					}
					instance->unusedCount = 0;
					instance->next = perImage.instances.mapped;
					perImage.instances.mapped = instance;
					VK(appState.Device.vkMapMemory(appState.Device.device, instance->memory, 0, instance->size, 0, &instance->mapped));
					auto matrix = (float*)instance->mapped;
					memset(matrix, 0, 16 * sizeof(float));
					auto pose = vrapi_LocateTrackingSpace(appState.Ovr, VRAPI_TRACKING_SPACE_LOCAL_FLOOR);
					auto playerHeight = 32;
					if (cl.viewentity >= 0 && cl.viewentity < cl_entities.size())
					{
						auto player = &cl_entities[cl.viewentity];
						if (player != nullptr)
						{
							auto model = player->model;
							if (model != nullptr)
							{
								playerHeight = model->maxs[1] - model->mins[1];
							}
						}
					}
					auto scale = -pose.Position.y / playerHeight;
					matrix[0] = scale;
					matrix[5] = scale;
					matrix[10] = scale;
					matrix[12] = -r_refdef.vieworg[0] * scale;
					matrix[13] = -r_refdef.vieworg[2] * scale;
					matrix[14] = r_refdef.vieworg[1] * scale;
					matrix[15] = 1;
					VC(appState.Device.vkUnmapMemory(appState.Device.device, instance->memory));
					instance->mapped = nullptr;
					bufferMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
					bufferMemoryBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
					bufferMemoryBarrier.buffer = instance->buffer;
					bufferMemoryBarrier.size = instance->size;
					VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &bufferMemoryBarrier, 0, nullptr));
					VkDeviceSize noOffset = 0;
					VC(appState.Device.vkCmdBindVertexBuffers(perImage.commandBuffer, 0, 1, &texturedVertices->buffer, &noOffset));
					VC(appState.Device.vkCmdBindVertexBuffers(perImage.commandBuffer, 1, 1, &texturedVertices->buffer, &noOffset));
					VC(appState.Device.vkCmdBindVertexBuffers(perImage.commandBuffer, 2, 1, &instance->buffer, &noOffset));
					VC(appState.Device.vkCmdBindIndexBuffer(perImage.commandBuffer, texturedIndices->buffer, 0, VK_INDEX_TYPE_UINT32));
					VC(appState.Device.vkCmdBindPipeline(perImage.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, appState.Scene.textured.pipeline));
					auto resources = new PipelineResources();
					memset(resources, 0, sizeof(PipelineResources));
					VkDescriptorPoolSize poolSizes[2];
					poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					poolSizes[0].descriptorCount = 1;
					poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					poolSizes[1].descriptorCount = 1;
					VkDescriptorPoolCreateInfo descriptorPoolCreateInfo { };
					descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
					descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
					descriptorPoolCreateInfo.maxSets = 1;
					descriptorPoolCreateInfo.poolSizeCount = 2;
					descriptorPoolCreateInfo.pPoolSizes = poolSizes;
					VK(appState.Device.vkCreateDescriptorPool(appState.Device.device, &descriptorPoolCreateInfo, nullptr, &resources->descriptorPool));
					VkDescriptorSetAllocateInfo descriptorSetAllocateInfo { };
					descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
					descriptorSetAllocateInfo.descriptorPool = resources->descriptorPool;
					descriptorSetAllocateInfo.descriptorSetCount = 1;
					descriptorSetAllocateInfo.pSetLayouts = &appState.Scene.textured.descriptorSetLayout;
					VK(appState.Device.vkAllocateDescriptorSets(appState.Device.device, &descriptorSetAllocateInfo, &resources->descriptorSet));
					VkDescriptorBufferInfo bufferInfo[2] { };
					bufferInfo[0].buffer = appState.Scene.matrices.buffer;
					bufferInfo[0].range = appState.Scene.matrices.size;
					VkWriteDescriptorSet writes[2] { };
					writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					writes[0].dstSet = resources->descriptorSet;
					writes[0].descriptorCount = 1;
					writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					writes[0].pBufferInfo = bufferInfo;
					VC(appState.Device.vkUpdateDescriptorSets(appState.Device.device, 1, writes, 0, nullptr));
					resources->next = perImage.pipelineResources;
					perImage.pipelineResources = resources;
					for (auto i = 0; i <= d_lists.last_textured; i++)
					{
						auto& textured = d_lists.textured[i];
						auto mipCount = (int)(std::floor(std::log2(std::max(textured.width, textured.height)))) + 1;
						Texture* texture = nullptr;
						for (Texture **t = &perImage.textures.oldTextures; *t != nullptr; t = &(*t)->next)
						{
							if ((*t)->width == textured.width && (*t)->height == textured.height)
							{
								texture = *t;
								*t = (*t)->next;
								break;
							}
						}
						if (texture == nullptr)
						{
							texture = new Texture();
							memset(texture, 0, sizeof(Texture));
							texture->width = textured.width;
							texture->height = textured.height;
							texture->layerCount = 1;
							VkImageCreateInfo imageCreateInfo { };
							imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
							imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
							imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
							imageCreateInfo.extent.width = texture->width;
							imageCreateInfo.extent.height = texture->height;
							imageCreateInfo.extent.depth = 1;
							imageCreateInfo.mipLevels = mipCount;
							imageCreateInfo.arrayLayers = texture->layerCount;
							imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
							imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
							imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
							imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
							VK(appState.Device.vkCreateImage(appState.Device.device, &imageCreateInfo, nullptr, &texture->image));
							VkMemoryRequirements memoryRequirements;
							VC(appState.Device.vkGetImageMemoryRequirements(appState.Device.device, texture->image, &memoryRequirements));
							VkMemoryAllocateInfo memoryAllocateInfo { };
							memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
							memoryAllocateInfo.allocationSize = memoryRequirements.size;
							auto requiredProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
							auto typeFound = false;
							for (uint32_t type = 0; type < appState.Context.device->physicalDeviceMemoryProperties.memoryTypeCount; type++)
							{
								if ((memoryRequirements.memoryTypeBits & (1 << type)) != 0)
								{
									const VkFlags propertyFlags = appState.Context.device->physicalDeviceMemoryProperties.memoryTypes[type].propertyFlags;
									if ((propertyFlags & requiredProperties) == requiredProperties)
									{
										typeFound = true;
										memoryAllocateInfo.memoryTypeIndex = type;
										break;
									}
								}
							}
							if (!typeFound)
							{
								__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Memory type %d with properties %d not found.", memoryRequirements.memoryTypeBits, requiredProperties);
								vrapi_Shutdown();
								exit(0);
							}
							VK(appState.Device.vkAllocateMemory(appState.Device.device, &memoryAllocateInfo, nullptr, &texture->memory));
							VK(appState.Device.vkBindImageMemory(appState.Device.device, texture->image, texture->memory, 0));
							VkImageViewCreateInfo imageViewCreateInfo { };
							imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
							imageViewCreateInfo.image = texture->image;
							imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
							imageViewCreateInfo.format = imageCreateInfo.format;
							imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
							imageViewCreateInfo.subresourceRange.levelCount = mipCount;
							imageViewCreateInfo.subresourceRange.layerCount = texture->layerCount;
							VK(appState.Device.vkCreateImageView(appState.Device.device, &imageViewCreateInfo, nullptr, &texture->view));
							VkImageMemoryBarrier imageMemoryBarrier { };
							imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
							imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
							imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
							imageMemoryBarrier.image = texture->image;
							imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
							imageMemoryBarrier.subresourceRange.levelCount = mipCount;
							imageMemoryBarrier.subresourceRange.layerCount = 1;
							VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
							VkSamplerCreateInfo samplerCreateInfo { };
							samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
							samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
							samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
							samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
							samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
							samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
							samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
							samplerCreateInfo.maxLod = mipCount;
							samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
							VK(appState.Device.vkCreateSampler(appState.Device.device, &samplerCreateInfo, nullptr, &texture->sampler));
						}
						Buffer *stagingBuffer = nullptr;
						size = textured.width * textured.height * 4;
						for (Buffer **b = &perImage.stagingBuffers.oldMapped; *b != nullptr; b = &(*b)->next)
						{
							if ((*b)->size >= size)
							{
								stagingBuffer = *b;
								*b = (*b)->next;
								break;
							}
						}
						if (stagingBuffer == nullptr)
						{
							stagingBuffer = new Buffer();
							memset(stagingBuffer, 0, sizeof(Buffer));
							stagingBuffer->size = size;
							VkBufferCreateInfo bufferCreateInfo { };
							bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
							bufferCreateInfo.size = stagingBuffer->size;
							bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
							bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
							VK(appState.Device.vkCreateBuffer(appState.Device.device, &bufferCreateInfo, nullptr, &stagingBuffer->buffer));
							stagingBuffer->flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
							VkMemoryRequirements memoryRequirements;
							VC(appState.Device.vkGetBufferMemoryRequirements(appState.Device.device, stagingBuffer->buffer, &memoryRequirements));
							VkMemoryAllocateInfo memoryAllocateInfo { };
							memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
							memoryAllocateInfo.allocationSize = memoryRequirements.size;
							auto requiredProperties = stagingBuffer->flags;
							auto typeFound = false;
							for (uint32_t type = 0; type < appState.Context.device->physicalDeviceMemoryProperties.memoryTypeCount; type++)
							{
								if ((memoryRequirements.memoryTypeBits & (1 << type)) != 0)
								{
									const VkFlags propertyFlags = appState.Context.device->physicalDeviceMemoryProperties.memoryTypes[type].propertyFlags;
									if ((propertyFlags & requiredProperties) == requiredProperties)
									{
										typeFound = true;
										memoryAllocateInfo.memoryTypeIndex = type;
										break;
									}
								}
							}
							if (!typeFound)
							{
								__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Memory type %d with properties %d not found.", memoryRequirements.memoryTypeBits, requiredProperties);
								vrapi_Shutdown();
								exit(0);
							}
							VK(appState.Device.vkAllocateMemory(appState.Device.device, &memoryAllocateInfo, nullptr, &stagingBuffer->memory));
							VK(appState.Device.vkBindBufferMemory(appState.Device.device, stagingBuffer->buffer, stagingBuffer->memory, 0));
						}
						stagingBuffer->unusedCount = 0;
						stagingBuffer->next = perImage.stagingBuffers.mapped;
						perImage.stagingBuffers.mapped = stagingBuffer;
						VK(appState.Device.vkMapMemory(appState.Device.device, stagingBuffer->memory, 0, size, 0, &stagingBuffer->mapped));
						auto index = 0;
						auto target = (unsigned char*)stagingBuffer->mapped;
						for (auto y = 0; y < textured.height; y++)
						{
							for (auto x = 0; x < textured.width; x++)
							{
								auto entry = textured.data[index];
								auto color = d_8to24table[entry];
								*target++ = color & 255;
								*target++ = (color >> 8) & 255;
								*target++ = (color >> 16) & 255;
								*target++ = color >> 24;
								index++;
							}
						}
						VC(appState.Device.vkUnmapMemory(appState.Device.device, stagingBuffer->memory));
						VkMappedMemoryRange mappedMemoryRange { };
						mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
						mappedMemoryRange.memory = stagingBuffer->memory;
						VC(appState.Device.vkFlushMappedMemoryRanges(appState.Device.device, 1, &mappedMemoryRange));
						imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
						imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
						imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
						imageMemoryBarrier.image = texture->image;
						imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
						imageMemoryBarrier.subresourceRange.levelCount = 1;
						imageMemoryBarrier.subresourceRange.layerCount = 1;
						VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
						VkBufferImageCopy region { };
						region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
						region.imageSubresource.layerCount = 1;
						region.imageExtent.width = texture->width;
						region.imageExtent.height = texture->height;
						region.imageExtent.depth = 1;
						VC(appState.Device.vkCmdCopyBufferToImage(perImage.commandBuffer, stagingBuffer->buffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region));
						imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
						imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
						imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
						imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
						VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
						auto width = textured.width;
						auto height = textured.height;
						for (auto k = 1; k < mipCount; k++)
						{
							imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
							imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
							imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
							imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
							imageMemoryBarrier.subresourceRange.baseMipLevel = k;
							VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
							VkImageBlit blit { };
							blit.srcOffsets[1].x = width;
							blit.srcOffsets[1].y = height;
							blit.srcOffsets[1].z = 1;
							blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
							blit.srcSubresource.mipLevel = k - 1;
							blit.srcSubresource.layerCount = 1;
							width /= 2;
							if (width < 1)
							{
								width = 1;
							}
							height /= 2;
							if (height < 1)
							{
								height = 1;
							}
							blit.dstOffsets[1].x = width;
							blit.dstOffsets[1].y = height;
							blit.dstOffsets[1].z = 1;
							blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
							blit.dstSubresource.mipLevel = k;
							blit.dstSubresource.layerCount = 1;
							VC(appState.Device.vkCmdBlitImage(perImage.commandBuffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR));
							imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
							imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
							imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
							imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
							VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
						}
						imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
						imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
						imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
						imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
						imageMemoryBarrier.subresourceRange.levelCount = mipCount;
						VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
						texture->unusedCount = 0;
						texture->next = perImage.textures.textures;
						perImage.textures.textures = texture;
						VkDescriptorImageInfo textureInfo { };
						textureInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						textureInfo.sampler = texture->sampler;
						textureInfo.imageView = texture->view;
						writes[0].dstBinding = 1;
						writes[0].dstSet = resources->descriptorSet;
						writes[0].descriptorCount = 1;
						writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
						writes[0].pImageInfo = &textureInfo;
						VC(appState.Device.vkUpdateDescriptorSets(appState.Device.device, 1, writes, 0, nullptr));
						VC(appState.Device.vkCmdBindDescriptorSets(perImage.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, appState.Scene.textured.pipelineLayout, 0, 1, &resources->descriptorSet, 0, nullptr));
						VC(appState.Device.vkCmdDrawIndexed(perImage.commandBuffer, textured.count, 1, textured.first_index, 0, 0));
					}
					for (auto i = 0; i <= d_lists.last_alias; i++)
					{
						auto& alias = d_lists.alias[i];
						auto mipCount = (int)(std::floor(std::log2(std::max(alias.width, alias.height)))) + 1;
						Texture* texture = nullptr;
						for (Texture **t = &perImage.textures.oldTextures; *t != nullptr; t = &(*t)->next)
						{
							if ((*t)->width == alias.width && (*t)->height == alias.height)
							{
								texture = *t;
								*t = (*t)->next;
								break;
							}
						}
						if (texture == nullptr)
						{
							texture = new Texture();
							memset(texture, 0, sizeof(Texture));
							texture->width = alias.width;
							texture->height = alias.height;
							texture->layerCount = 1;
							VkImageCreateInfo imageCreateInfo { };
							imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
							imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
							imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
							imageCreateInfo.extent.width = texture->width;
							imageCreateInfo.extent.height = texture->height;
							imageCreateInfo.extent.depth = 1;
							imageCreateInfo.mipLevels = mipCount;
							imageCreateInfo.arrayLayers = texture->layerCount;
							imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
							imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
							imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
							imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
							VK(appState.Device.vkCreateImage(appState.Device.device, &imageCreateInfo, nullptr, &texture->image));
							VkMemoryRequirements memoryRequirements;
							VC(appState.Device.vkGetImageMemoryRequirements(appState.Device.device, texture->image, &memoryRequirements));
							VkMemoryAllocateInfo memoryAllocateInfo { };
							memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
							memoryAllocateInfo.allocationSize = memoryRequirements.size;
							auto requiredProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
							auto typeFound = false;
							for (uint32_t type = 0; type < appState.Context.device->physicalDeviceMemoryProperties.memoryTypeCount; type++)
							{
								if ((memoryRequirements.memoryTypeBits & (1 << type)) != 0)
								{
									const VkFlags propertyFlags = appState.Context.device->physicalDeviceMemoryProperties.memoryTypes[type].propertyFlags;
									if ((propertyFlags & requiredProperties) == requiredProperties)
									{
										typeFound = true;
										memoryAllocateInfo.memoryTypeIndex = type;
										break;
									}
								}
							}
							if (!typeFound)
							{
								__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Memory type %d with properties %d not found.", memoryRequirements.memoryTypeBits, requiredProperties);
								vrapi_Shutdown();
								exit(0);
							}
							VK(appState.Device.vkAllocateMemory(appState.Device.device, &memoryAllocateInfo, nullptr, &texture->memory));
							VK(appState.Device.vkBindImageMemory(appState.Device.device, texture->image, texture->memory, 0));
							VkImageViewCreateInfo imageViewCreateInfo { };
							imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
							imageViewCreateInfo.image = texture->image;
							imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
							imageViewCreateInfo.format = imageCreateInfo.format;
							imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
							imageViewCreateInfo.subresourceRange.levelCount = mipCount;
							imageViewCreateInfo.subresourceRange.layerCount = texture->layerCount;
							VK(appState.Device.vkCreateImageView(appState.Device.device, &imageViewCreateInfo, nullptr, &texture->view));
							VkImageMemoryBarrier imageMemoryBarrier { };
							imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
							imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
							imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
							imageMemoryBarrier.image = texture->image;
							imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
							imageMemoryBarrier.subresourceRange.levelCount = mipCount;
							imageMemoryBarrier.subresourceRange.layerCount = 1;
							VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
							VkSamplerCreateInfo samplerCreateInfo { };
							samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
							samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
							samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
							samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
							samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
							samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
							samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
							samplerCreateInfo.maxLod = mipCount;
							samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
							VK(appState.Device.vkCreateSampler(appState.Device.device, &samplerCreateInfo, nullptr, &texture->sampler));
						}
						Buffer *stagingBuffer = nullptr;
						size = alias.width * alias.height * 4;
						for (Buffer **b = &perImage.stagingBuffers.oldMapped; *b != nullptr; b = &(*b)->next)
						{
							if ((*b)->size >= size)
							{
								stagingBuffer = *b;
								*b = (*b)->next;
								break;
							}
						}
						if (stagingBuffer == nullptr)
						{
							stagingBuffer = new Buffer();
							memset(stagingBuffer, 0, sizeof(Buffer));
							stagingBuffer->size = size;
							VkBufferCreateInfo bufferCreateInfo { };
							bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
							bufferCreateInfo.size = stagingBuffer->size;
							bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
							bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
							VK(appState.Device.vkCreateBuffer(appState.Device.device, &bufferCreateInfo, nullptr, &stagingBuffer->buffer));
							stagingBuffer->flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
							VkMemoryRequirements memoryRequirements;
							VC(appState.Device.vkGetBufferMemoryRequirements(appState.Device.device, stagingBuffer->buffer, &memoryRequirements));
							VkMemoryAllocateInfo memoryAllocateInfo { };
							memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
							memoryAllocateInfo.allocationSize = memoryRequirements.size;
							auto requiredProperties = stagingBuffer->flags;
							auto typeFound = false;
							for (uint32_t type = 0; type < appState.Context.device->physicalDeviceMemoryProperties.memoryTypeCount; type++)
							{
								if ((memoryRequirements.memoryTypeBits & (1 << type)) != 0)
								{
									const VkFlags propertyFlags = appState.Context.device->physicalDeviceMemoryProperties.memoryTypes[type].propertyFlags;
									if ((propertyFlags & requiredProperties) == requiredProperties)
									{
										typeFound = true;
										memoryAllocateInfo.memoryTypeIndex = type;
										break;
									}
								}
							}
							if (!typeFound)
							{
								__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Memory type %d with properties %d not found.", memoryRequirements.memoryTypeBits, requiredProperties);
								vrapi_Shutdown();
								exit(0);
							}
							VK(appState.Device.vkAllocateMemory(appState.Device.device, &memoryAllocateInfo, nullptr, &stagingBuffer->memory));
							VK(appState.Device.vkBindBufferMemory(appState.Device.device, stagingBuffer->buffer, stagingBuffer->memory, 0));
						}
						stagingBuffer->unusedCount = 0;
						stagingBuffer->next = perImage.stagingBuffers.mapped;
						perImage.stagingBuffers.mapped = stagingBuffer;
						VK(appState.Device.vkMapMemory(appState.Device.device, stagingBuffer->memory, 0, size, 0, &stagingBuffer->mapped));
						auto index = 0;
						auto target = (unsigned char*)stagingBuffer->mapped;
						for (auto y = 0; y < alias.height; y++)
						{
							for (auto x = 0; x < alias.width; x++)
							{
								auto entry = alias.data[index];
								auto color = d_8to24table[entry];
								*target++ = color & 255;
								*target++ = (color >> 8) & 255;
								*target++ = (color >> 16) & 255;
								*target++ = color >> 24;
								index++;
							}
						}
						VC(appState.Device.vkUnmapMemory(appState.Device.device, stagingBuffer->memory));
						VkMappedMemoryRange mappedMemoryRange { };
						mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
						mappedMemoryRange.memory = stagingBuffer->memory;
						VC(appState.Device.vkFlushMappedMemoryRanges(appState.Device.device, 1, &mappedMemoryRange));
						imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
						imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
						imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
						imageMemoryBarrier.image = texture->image;
						imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
						imageMemoryBarrier.subresourceRange.levelCount = 1;
						imageMemoryBarrier.subresourceRange.layerCount = 1;
						VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
						VkBufferImageCopy region { };
						region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
						region.imageSubresource.layerCount = 1;
						region.imageExtent.width = texture->width;
						region.imageExtent.height = texture->height;
						region.imageExtent.depth = 1;
						VC(appState.Device.vkCmdCopyBufferToImage(perImage.commandBuffer, stagingBuffer->buffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region));
						imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
						imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
						imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
						imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
						VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
						auto width = alias.width;
						auto height = alias.height;
						for (auto k = 1; k < mipCount; k++)
						{
							imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
							imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
							imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
							imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
							imageMemoryBarrier.subresourceRange.baseMipLevel = k;
							VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
							VkImageBlit blit { };
							blit.srcOffsets[1].x = width;
							blit.srcOffsets[1].y = height;
							blit.srcOffsets[1].z = 1;
							blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
							blit.srcSubresource.mipLevel = k - 1;
							blit.srcSubresource.layerCount = 1;
							width /= 2;
							if (width < 1)
							{
								width = 1;
							}
							height /= 2;
							if (height < 1)
							{
								height = 1;
							}
							blit.dstOffsets[1].x = width;
							blit.dstOffsets[1].y = height;
							blit.dstOffsets[1].z = 1;
							blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
							blit.dstSubresource.mipLevel = k;
							blit.dstSubresource.layerCount = 1;
							VC(appState.Device.vkCmdBlitImage(perImage.commandBuffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR));
							imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
							imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
							imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
							imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
							VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
						}
						imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
						imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
						imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
						imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
						imageMemoryBarrier.subresourceRange.levelCount = mipCount;
						VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
						texture->unusedCount = 0;
						texture->next = perImage.textures.textures;
						perImage.textures.textures = texture;
						VkDescriptorImageInfo textureInfo { };
						textureInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						textureInfo.sampler = texture->sampler;
						textureInfo.imageView = texture->view;
						writes[0].dstBinding = 1;
						writes[0].dstSet = resources->descriptorSet;
						writes[0].descriptorCount = 1;
						writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
						writes[0].pImageInfo = &textureInfo;
						VC(appState.Device.vkUpdateDescriptorSets(appState.Device.device, 1, writes, 0, nullptr));
						VC(appState.Device.vkCmdBindDescriptorSets(perImage.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, appState.Scene.textured.pipelineLayout, 0, 1, &resources->descriptorSet, 0, nullptr));
						VC(appState.Device.vkCmdDrawIndexed(perImage.commandBuffer, alias.count, 1, alias.first_index, 0, 0));
					}
					Buffer* time = nullptr;
					if (perImage.times.oldMapped != nullptr)
					{
						time = perImage.times.oldMapped;
						perImage.times.oldMapped = perImage.times.oldMapped->next;
					}
					else
					{
						time = new Buffer();
						memset(time, 0, sizeof(Buffer));
						time->size = sizeof(float);
						VkBufferCreateInfo bufferCreateInfo { };
						bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
						bufferCreateInfo.size = time->size;
						bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
						bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
						VK(appState.Device.vkCreateBuffer(appState.Device.device, &bufferCreateInfo, nullptr, &time->buffer));
						time->flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
						VkMemoryRequirements memoryRequirements;
						VC(appState.Device.vkGetBufferMemoryRequirements(appState.Device.device, time->buffer, &memoryRequirements));
						VkMemoryAllocateInfo memoryAllocateInfo { };
						memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
						memoryAllocateInfo.allocationSize = memoryRequirements.size;
						auto requiredProperties = time->flags;
						auto typeFound = false;
						for (uint32_t type = 0; type < appState.Context.device->physicalDeviceMemoryProperties.memoryTypeCount; type++)
						{
							if ((memoryRequirements.memoryTypeBits & (1 << type)) != 0)
							{
								const VkFlags propertyFlags = appState.Context.device->physicalDeviceMemoryProperties.memoryTypes[type].propertyFlags;
								if ((propertyFlags & requiredProperties) == requiredProperties)
								{
									typeFound = true;
									memoryAllocateInfo.memoryTypeIndex = type;
									break;
								}
							}
						}
						if (!typeFound)
						{
							__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Memory type %d with properties %d not found.", memoryRequirements.memoryTypeBits, requiredProperties);
							vrapi_Shutdown();
							exit(0);
						}
						VK(appState.Device.vkAllocateMemory(appState.Device.device, &memoryAllocateInfo, nullptr, &time->memory));
						VK(appState.Device.vkBindBufferMemory(appState.Device.device, time->buffer, time->memory, 0));
					}
					time->unusedCount = 0;
					time->next = perImage.times.mapped;
					perImage.times.mapped = time;
					VK(appState.Device.vkMapMemory(appState.Device.device, time->memory, 0, time->size, 0, &time->mapped));
					*(float*)(time->mapped) = cl.time;
					VC(appState.Device.vkUnmapMemory(appState.Device.device, instance->memory));
					instance->mapped = nullptr;
					bufferMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
					bufferMemoryBarrier.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT;
					bufferMemoryBarrier.buffer = instance->buffer;
					bufferMemoryBarrier.size = instance->size;
					VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr, 1, &bufferMemoryBarrier, 0, nullptr));
					VC(appState.Device.vkCmdBindPipeline(perImage.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, appState.Scene.turbulent.pipeline));
					resources = new PipelineResources();
					memset(resources, 0, sizeof(PipelineResources));
					VK(appState.Device.vkCreateDescriptorPool(appState.Device.device, &descriptorPoolCreateInfo, nullptr, &resources->descriptorPool));
					descriptorSetAllocateInfo.descriptorPool = resources->descriptorPool;
					descriptorSetAllocateInfo.pSetLayouts = &appState.Scene.turbulent.descriptorSetLayout;
					VK(appState.Device.vkAllocateDescriptorSets(appState.Device.device, &descriptorSetAllocateInfo, &resources->descriptorSet));
					bufferInfo[0].buffer = appState.Scene.matrices.buffer;
					bufferInfo[0].range = appState.Scene.matrices.size;
					bufferInfo[1].buffer = time->buffer;
					bufferInfo[1].range = time->size;
					writes[0].dstBinding = 0;
					writes[0].dstSet = resources->descriptorSet;
					writes[0].descriptorCount = 1;
					writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					writes[0].pBufferInfo = bufferInfo;
					writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					writes[1].dstBinding = 2;
					writes[1].dstSet = resources->descriptorSet;
					writes[1].descriptorCount = 1;
					writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					writes[1].pBufferInfo = bufferInfo + 1;
					VC(appState.Device.vkUpdateDescriptorSets(appState.Device.device, 2, writes, 0, nullptr));
					resources->next = perImage.pipelineResources;
					perImage.pipelineResources = resources;
					for (auto i = 0; i <= d_lists.last_turbulent; i++)
					{
						auto& turbulent = d_lists.turbulent[i];
						auto mipCount = (int)(std::floor(std::log2(std::max(turbulent.width, turbulent.height)))) + 1;
						Texture* texture = nullptr;
						for (Texture **t = &perImage.textures.oldTextures; *t != nullptr; t = &(*t)->next)
						{
							if ((*t)->width == turbulent.width && (*t)->height == turbulent.height)
							{
								texture = *t;
								*t = (*t)->next;
								break;
							}
						}
						if (texture == nullptr)
						{
							texture = new Texture();
							memset(texture, 0, sizeof(Texture));
							texture->width = turbulent.width;
							texture->height = turbulent.height;
							texture->layerCount = 1;
							VkImageCreateInfo imageCreateInfo { };
							imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
							imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
							imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
							imageCreateInfo.extent.width = texture->width;
							imageCreateInfo.extent.height = texture->height;
							imageCreateInfo.extent.depth = 1;
							imageCreateInfo.mipLevels = mipCount;
							imageCreateInfo.arrayLayers = texture->layerCount;
							imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
							imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
							imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
							imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
							VK(appState.Device.vkCreateImage(appState.Device.device, &imageCreateInfo, nullptr, &texture->image));
							VkMemoryRequirements memoryRequirements;
							VC(appState.Device.vkGetImageMemoryRequirements(appState.Device.device, texture->image, &memoryRequirements));
							VkMemoryAllocateInfo memoryAllocateInfo { };
							memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
							memoryAllocateInfo.allocationSize = memoryRequirements.size;
							auto requiredProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
							auto typeFound = false;
							for (uint32_t type = 0; type < appState.Context.device->physicalDeviceMemoryProperties.memoryTypeCount; type++)
							{
								if ((memoryRequirements.memoryTypeBits & (1 << type)) != 0)
								{
									const VkFlags propertyFlags = appState.Context.device->physicalDeviceMemoryProperties.memoryTypes[type].propertyFlags;
									if ((propertyFlags & requiredProperties) == requiredProperties)
									{
										typeFound = true;
										memoryAllocateInfo.memoryTypeIndex = type;
										break;
									}
								}
							}
							if (!typeFound)
							{
								__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Memory type %d with properties %d not found.", memoryRequirements.memoryTypeBits, requiredProperties);
								vrapi_Shutdown();
								exit(0);
							}
							VK(appState.Device.vkAllocateMemory(appState.Device.device, &memoryAllocateInfo, nullptr, &texture->memory));
							VK(appState.Device.vkBindImageMemory(appState.Device.device, texture->image, texture->memory, 0));
							VkImageViewCreateInfo imageViewCreateInfo { };
							imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
							imageViewCreateInfo.image = texture->image;
							imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
							imageViewCreateInfo.format = imageCreateInfo.format;
							imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
							imageViewCreateInfo.subresourceRange.levelCount = mipCount;
							imageViewCreateInfo.subresourceRange.layerCount = texture->layerCount;
							VK(appState.Device.vkCreateImageView(appState.Device.device, &imageViewCreateInfo, nullptr, &texture->view));
							VkImageMemoryBarrier imageMemoryBarrier { };
							imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
							imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
							imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
							imageMemoryBarrier.image = texture->image;
							imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
							imageMemoryBarrier.subresourceRange.levelCount = mipCount;
							imageMemoryBarrier.subresourceRange.layerCount = 1;
							VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
							VkSamplerCreateInfo samplerCreateInfo { };
							samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
							samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
							samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
							samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
							samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
							samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
							samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
							samplerCreateInfo.maxLod = mipCount;
							samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
							VK(appState.Device.vkCreateSampler(appState.Device.device, &samplerCreateInfo, nullptr, &texture->sampler));
						}
						Buffer *stagingBuffer = nullptr;
						size = turbulent.width * turbulent.height * 4;
						for (Buffer **b = &perImage.stagingBuffers.oldMapped; *b != nullptr; b = &(*b)->next)
						{
							if ((*b)->size >= size)
							{
								stagingBuffer = *b;
								*b = (*b)->next;
								break;
							}
						}
						if (stagingBuffer == nullptr)
						{
							stagingBuffer = new Buffer();
							memset(stagingBuffer, 0, sizeof(Buffer));
							stagingBuffer->size = size;
							VkBufferCreateInfo bufferCreateInfo { };
							bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
							bufferCreateInfo.size = stagingBuffer->size;
							bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
							bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
							VK(appState.Device.vkCreateBuffer(appState.Device.device, &bufferCreateInfo, nullptr, &stagingBuffer->buffer));
							stagingBuffer->flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
							VkMemoryRequirements memoryRequirements;
							VC(appState.Device.vkGetBufferMemoryRequirements(appState.Device.device, stagingBuffer->buffer, &memoryRequirements));
							VkMemoryAllocateInfo memoryAllocateInfo { };
							memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
							memoryAllocateInfo.allocationSize = memoryRequirements.size;
							auto requiredProperties = stagingBuffer->flags;
							auto typeFound = false;
							for (uint32_t type = 0; type < appState.Context.device->physicalDeviceMemoryProperties.memoryTypeCount; type++)
							{
								if ((memoryRequirements.memoryTypeBits & (1 << type)) != 0)
								{
									const VkFlags propertyFlags = appState.Context.device->physicalDeviceMemoryProperties.memoryTypes[type].propertyFlags;
									if ((propertyFlags & requiredProperties) == requiredProperties)
									{
										typeFound = true;
										memoryAllocateInfo.memoryTypeIndex = type;
										break;
									}
								}
							}
							if (!typeFound)
							{
								__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Memory type %d with properties %d not found.", memoryRequirements.memoryTypeBits, requiredProperties);
								vrapi_Shutdown();
								exit(0);
							}
							VK(appState.Device.vkAllocateMemory(appState.Device.device, &memoryAllocateInfo, nullptr, &stagingBuffer->memory));
							VK(appState.Device.vkBindBufferMemory(appState.Device.device, stagingBuffer->buffer, stagingBuffer->memory, 0));
						}
						stagingBuffer->unusedCount = 0;
						stagingBuffer->next = perImage.stagingBuffers.mapped;
						perImage.stagingBuffers.mapped = stagingBuffer;
						VK(appState.Device.vkMapMemory(appState.Device.device, stagingBuffer->memory, 0, size, 0, &stagingBuffer->mapped));
						auto index = 0;
						auto target = (unsigned char*)stagingBuffer->mapped;
						for (auto y = 0; y < turbulent.height; y++)
						{
							for (auto x = 0; x < turbulent.width; x++)
							{
								auto entry = turbulent.data[index];
								auto color = d_8to24table[entry];
								*target++ = color & 255;
								*target++ = (color >> 8) & 255;
								*target++ = (color >> 16) & 255;
								*target++ = color >> 24;
								index++;
							}
						}
						VC(appState.Device.vkUnmapMemory(appState.Device.device, stagingBuffer->memory));
						VkMappedMemoryRange mappedMemoryRange { };
						mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
						mappedMemoryRange.memory = stagingBuffer->memory;
						VC(appState.Device.vkFlushMappedMemoryRanges(appState.Device.device, 1, &mappedMemoryRange));
						imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
						imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
						imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
						imageMemoryBarrier.image = texture->image;
						imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
						imageMemoryBarrier.subresourceRange.levelCount = 1;
						imageMemoryBarrier.subresourceRange.layerCount = 1;
						VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
						VkBufferImageCopy region { };
						region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
						region.imageSubresource.layerCount = 1;
						region.imageExtent.width = texture->width;
						region.imageExtent.height = texture->height;
						region.imageExtent.depth = 1;
						VC(appState.Device.vkCmdCopyBufferToImage(perImage.commandBuffer, stagingBuffer->buffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region));
						imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
						imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
						imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
						imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
						VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
						auto width = turbulent.width;
						auto height = turbulent.height;
						for (auto k = 1; k < mipCount; k++)
						{
							imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
							imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
							imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
							imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
							imageMemoryBarrier.subresourceRange.baseMipLevel = k;
							VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
							VkImageBlit blit { };
							blit.srcOffsets[1].x = width;
							blit.srcOffsets[1].y = height;
							blit.srcOffsets[1].z = 1;
							blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
							blit.srcSubresource.mipLevel = k - 1;
							blit.srcSubresource.layerCount = 1;
							width /= 2;
							if (width < 1)
							{
								width = 1;
							}
							height /= 2;
							if (height < 1)
							{
								height = 1;
							}
							blit.dstOffsets[1].x = width;
							blit.dstOffsets[1].y = height;
							blit.dstOffsets[1].z = 1;
							blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
							blit.dstSubresource.mipLevel = k;
							blit.dstSubresource.layerCount = 1;
							VC(appState.Device.vkCmdBlitImage(perImage.commandBuffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR));
							imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
							imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
							imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
							imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
							VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
						}
						imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
						imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
						imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
						imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
						imageMemoryBarrier.subresourceRange.levelCount = mipCount;
						VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
						texture->unusedCount = 0;
						texture->next = perImage.textures.textures;
						perImage.textures.textures = texture;
						VkDescriptorImageInfo textureInfo { };
						textureInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						textureInfo.sampler = texture->sampler;
						textureInfo.imageView = texture->view;
						writes[0].dstBinding = 1;
						writes[0].dstSet = resources->descriptorSet;
						writes[0].descriptorCount = 1;
						writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
						writes[0].pImageInfo = &textureInfo;
						VC(appState.Device.vkUpdateDescriptorSets(appState.Device.device, 1, writes, 0, nullptr));
						VC(appState.Device.vkCmdBindDescriptorSets(perImage.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, appState.Scene.turbulent.pipelineLayout, 0, 1, &resources->descriptorSet, 0, nullptr));
						VC(appState.Device.vkCmdDrawIndexed(perImage.commandBuffer, turbulent.count, 1, turbulent.first_index, 0, 0));
					}
				}
			}
			VC(appState.Device.vkCmdEndRenderPass(perImage.commandBuffer));
			auto& colorTexture = view.framebuffer.colorTextures[view.index];
			VkImageMemoryBarrier colorImageMemoryBarrier { };
			colorImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			colorImageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			colorImageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			colorImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			colorImageMemoryBarrier.image = colorTexture.image;
			colorImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			colorImageMemoryBarrier.subresourceRange.levelCount = 1;
			colorImageMemoryBarrier.subresourceRange.layerCount = colorTexture.layerCount;
			VC(appState.Device.vkCmdPipelineBarrier(perImage.commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &colorImageMemoryBarrier));
			VK(appState.Device.vkEndCommandBuffer(perImage.commandBuffer));
			VkSubmitInfo submitInfo { };
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &perImage.commandBuffer;
			VK(appState.Device.vkQueueSubmit(appState.Context.queue, 1, &submitInfo, perImage.fence));
			perImage.submitted = true;
			matrixIndex++;
		}
		if (appState.Mode == AppScreenMode)
		{
			auto consoleIndex = 0;
			auto consoleIndexCache = 0;
			auto screenIndex = 0;
			auto targetIndex = 0;
			auto y = 0;
			while (y < vid_height)
			{
				auto x = 0;
				while (x < vid_width)
				{
					auto entry = con_buffer[consoleIndex];
					if (entry == 255)
					{
						do
						{
							appState.Screen.Data[targetIndex] = d_8to24table[vid_buffer[screenIndex]];
							screenIndex++;
							targetIndex++;
							x++;
						} while ((x % 3) != 0);
					}
					else
					{
						do
						{
							appState.Screen.Data[targetIndex] = d_8to24table[entry];
							screenIndex++;
							targetIndex++;
							x++;
						} while ((x % 3) != 0);
					}
					consoleIndex++;
				}
				y++;
				if ((y % 3) == 0)
				{
					consoleIndexCache = consoleIndex;
				}
				else
				{
					consoleIndex = consoleIndexCache;
				}
			}
			VK(appState.Device.vkMapMemory(appState.Device.device, appState.Screen.Buffer.memory, 0, appState.Screen.Buffer.size, 0, &appState.Screen.Buffer.mapped));
			memcpy(appState.Screen.Buffer.mapped, appState.Screen.Data.data(), appState.Screen.Data.size() * sizeof(uint32_t));
			VC(appState.Device.vkUnmapMemory(appState.Device.device, appState.Screen.Buffer.memory));
			appState.Screen.Buffer.mapped = nullptr;
			VK(appState.Device.vkBeginCommandBuffer(appState.Screen.CommandBuffer, &commandBufferBeginInfo));
			VkBufferImageCopy region { };
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.layerCount = 1;
			region.imageExtent.width = appState.Screen.Width;
			region.imageExtent.height = appState.Screen.Height;
			region.imageExtent.depth = 1;
			VC(appState.Device.vkCmdCopyBufferToImage(appState.Screen.CommandBuffer, appState.Screen.Buffer.buffer, appState.Screen.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region));
			VK(appState.Device.vkEndCommandBuffer(appState.Screen.CommandBuffer));
			VK(appState.Device.vkQueueSubmit(appState.Context.queue, 1, &appState.Screen.SubmitInfo, VK_NULL_HANDLE));
		}
		else if (appState.Mode == AppWorldMode)
		{
			auto width = appState.Console.Width;
			auto targetWidth = width * 4;
			auto sourceIndex = 0;
			auto targetIndex = 0;
			auto y = 0;
			while (y < appState.Console.Height)
			{
				auto x = 0;
				while (x < width)
				{
					auto pixel = d_8to24table[con_buffer[sourceIndex]];
					do
					{
						appState.Console.Data[targetIndex] = pixel;
						targetIndex++;
						x++;
					} while ((x % 3) != 0);
					sourceIndex++;
				}
				y++;
				while ((y % 3) != 0)
				{
					memcpy(appState.Console.Data.data() + targetIndex, appState.Console.Data.data() + targetIndex - width, targetWidth);
					targetIndex += width;
					y++;
				}
			}
			VK(appState.Device.vkMapMemory(appState.Device.device, appState.Console.Buffer.memory, 0, appState.Console.Buffer.size, 0, &appState.Console.Buffer.mapped));
			memcpy(appState.Console.Buffer.mapped, appState.Console.Data.data(), appState.Console.Data.size() * sizeof(uint32_t));
			VC(appState.Device.vkUnmapMemory(appState.Device.device, appState.Console.Buffer.memory));
			appState.Console.Buffer.mapped = nullptr;
			VK(appState.Device.vkBeginCommandBuffer(appState.Console.CommandBuffer, &commandBufferBeginInfo));
			VkBufferImageCopy region { };
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.layerCount = 1;
			region.imageExtent.width = appState.Console.Width;
			region.imageExtent.height = appState.Console.Height;
			region.imageExtent.depth = 1;
			VC(appState.Device.vkCmdCopyBufferToImage(appState.Console.CommandBuffer, appState.Console.Buffer.buffer, appState.Console.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region));
			VK(appState.Device.vkEndCommandBuffer(appState.Console.CommandBuffer));
			VK(appState.Device.vkQueueSubmit(appState.Context.queue, 1, &appState.Console.SubmitInfo, VK_NULL_HANDLE));
		}
		ovrLayerProjection2 worldLayer = vrapi_DefaultLayerProjection2();
		worldLayer.HeadPose = tracking.HeadPose;
		for (auto i = 0; i < VRAPI_FRAME_LAYER_EYE_MAX; i++)
		{
			auto view = (appState.Views.size() == 1 ? 0 : i);
			worldLayer.Textures[i].ColorSwapChain = appState.Views[view].framebuffer.colorTextureSwapChain;
			worldLayer.Textures[i].SwapChainIndex = appState.Views[view].framebuffer.currentBuffer;
			worldLayer.Textures[i].TexCoordsFromTanAngles = ovrMatrix4f_TanAngleMatrixFromProjection(&tracking.Eye[i].ProjectionMatrix);
		}
		worldLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;
		ovrLayerCylinder2 cylinderLayer;
		if (appState.Mode == AppWorldMode)
		{
			auto console = vrapi_DefaultLayerCylinder2();
			console.Header.ColorScale.x = 1;
			console.Header.ColorScale.y = 1;
			console.Header.ColorScale.z = 1;
			console.Header.ColorScale.w = 1;
			console.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_SRC_ALPHA;
			console.Header.DstBlend = VRAPI_FRAME_LAYER_BLEND_ONE_MINUS_SRC_ALPHA;
			console.HeadPose = tracking.HeadPose;
			const float density = 4500;
			float rotateYaw = appState.Yaw;
			float rotatePitch = -appState.Pitch;
			const float radius = 1;
			const ovrVector3f translation = { tracking.HeadPose.Pose.Position.x, tracking.HeadPose.Pose.Position.y, tracking.HeadPose.Pose.Position.z };
			const ovrMatrix4f scaleMatrix = ovrMatrix4f_CreateScale(radius, radius * (float) appState.Console.Height * VRAPI_PI / density, radius);
			const ovrMatrix4f transMatrix = ovrMatrix4f_CreateTranslation(translation.x, translation.y, translation.z);
			const ovrMatrix4f rotXMatrix = ovrMatrix4f_CreateRotation(rotatePitch, 0, 0);
			const ovrMatrix4f rotYMatrix = ovrMatrix4f_CreateRotation(0, rotateYaw, 0);
			const ovrMatrix4f m0 = ovrMatrix4f_Multiply(&rotXMatrix, &scaleMatrix);
			const ovrMatrix4f m1 = ovrMatrix4f_Multiply(&rotYMatrix, &m0);
			ovrMatrix4f cylinderTransform = ovrMatrix4f_Multiply(&transMatrix, &m1);
			float circScale = density * 0.5 / appState.Console.Width;
			float circBias = -circScale * (0.5 * (1 - 1 / circScale));
			for (auto i = 0; i < VRAPI_FRAME_LAYER_EYE_MAX; i++)
			{
				ovrMatrix4f modelViewMatrix = ovrMatrix4f_Multiply(&tracking.Eye[i].ViewMatrix, &cylinderTransform);
				console.Textures[i].TexCoordsFromTanAngles = ovrMatrix4f_Inverse(&modelViewMatrix);
				console.Textures[i].ColorSwapChain = appState.Console.SwapChain;
				console.Textures[i].SwapChainIndex = 0;
				const float texScaleX = circScale;
				const float texBiasX = circBias;
				const float texScaleY = 0.5;
				const float texBiasY = -texScaleY * (0.5 * (1 - 1 / texScaleY));
				console.Textures[i].TextureMatrix.M[0][0] = texScaleX;
				console.Textures[i].TextureMatrix.M[0][2] = texBiasX;
				console.Textures[i].TextureMatrix.M[1][1] = texScaleY;
				console.Textures[i].TextureMatrix.M[1][2] = texBiasY;
				console.Textures[i].TextureRect.width = 1;
				console.Textures[i].TextureRect.height = 1;
			}
			cylinderLayer = console;
		}
		else
		{
			auto screen = vrapi_DefaultLayerCylinder2();
			screen.Header.ColorScale.x = 1;
			screen.Header.ColorScale.y = 1;
			screen.Header.ColorScale.z = 1;
			screen.Header.ColorScale.w = 1;
			screen.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_SRC_ALPHA;
			screen.Header.DstBlend = VRAPI_FRAME_LAYER_BLEND_ONE_MINUS_SRC_ALPHA;
			screen.HeadPose = tracking.HeadPose;
			const float density = 4500;
			const float rotateYaw = 0;
			float rotatePitch = 0;
			const float radius = 1;
			const ovrMatrix4f scaleMatrix = ovrMatrix4f_CreateScale(radius, radius * (float) appState.Screen.Height * VRAPI_PI / density, radius);
			const ovrMatrix4f rotXMatrix = ovrMatrix4f_CreateRotation(rotatePitch, 0.0f, 0.0f);
			const ovrMatrix4f rotYMatrix = ovrMatrix4f_CreateRotation(0.0f, rotateYaw, 0.0f);
			const ovrMatrix4f m0 = ovrMatrix4f_Multiply(&rotXMatrix, &scaleMatrix);
			const ovrMatrix4f cylinderTransform = ovrMatrix4f_Multiply(&rotYMatrix, &m0);
			float circScale = density * 0.5f / appState.Screen.Width;
			float circBias = -circScale * (0.5f * (1.0f - 1.0f / circScale));
			for (auto i = 0; i < VRAPI_FRAME_LAYER_EYE_MAX; i++)
			{
				ovrMatrix4f modelViewMatrix = ovrMatrix4f_Multiply(&tracking.Eye[i].ViewMatrix, &cylinderTransform);
				screen.Textures[i].TexCoordsFromTanAngles = ovrMatrix4f_Inverse(&modelViewMatrix);
				screen.Textures[i].ColorSwapChain = appState.Screen.SwapChain;
				screen.Textures[i].SwapChainIndex = 0;
				const float texScaleX = circScale;
				const float texBiasX = circBias;
				const float texScaleY = 0.5;
				const float texBiasY = -texScaleY * (0.5 * (1 - 1 / texScaleY));
				screen.Textures[i].TextureMatrix.M[0][0] = texScaleX;
				screen.Textures[i].TextureMatrix.M[0][2] = texBiasX;
				screen.Textures[i].TextureMatrix.M[1][1] = texScaleY;
				screen.Textures[i].TextureMatrix.M[1][2] = texBiasY;
				screen.Textures[i].TextureRect.width = 1;
				screen.Textures[i].TextureRect.height = 1;
			}
			cylinderLayer = screen;
		}
		const ovrLayerHeader2* layers[] = { &worldLayer.Header, &cylinderLayer.Header };
		ovrSubmitFrameDescription2 frameDesc = { };
		frameDesc.SwapInterval = appState.SwapInterval;
		frameDesc.FrameIndex = appState.FrameIndex;
		frameDesc.DisplayTime = appState.DisplayTime;
		frameDesc.LayerCount = 2;
		frameDesc.Layers = layers;
		vrapi_SubmitFrame2(appState.Ovr, &frameDesc);
	}
	for (auto& view : appState.Views)
	{
		for (auto i = 0; i < view.framebuffer.swapChainLength; i++)
		{
			if (view.framebuffer.framebuffers.size() > 0)
			{
				VC(appState.Device.vkDestroyFramebuffer(appState.Device.device, view.framebuffer.framebuffers[i], nullptr));
			}
			if (view.framebuffer.colorTextures.size() > 0)
			{
				VC(appState.Device.vkDestroySampler(appState.Device.device, view.framebuffer.colorTextures[i].sampler, nullptr));
				if (view.framebuffer.colorTextures[i].memory != VK_NULL_HANDLE)
				{
					VC(appState.Device.vkDestroyImageView(appState.Device.device, view.framebuffer.colorTextures[i].view, nullptr));
					VC(appState.Device.vkDestroyImage(appState.Device.device, view.framebuffer.colorTextures[i].image, nullptr));
					VC(appState.Device.vkFreeMemory(appState.Device.device, view.framebuffer.colorTextures[i].memory, nullptr));
				}
			}
			if (view.framebuffer.fragmentDensityTextures.size() > 0)
			{
				VC(appState.Device.vkDestroySampler(appState.Device.device, view.framebuffer.fragmentDensityTextures[i].sampler, nullptr));
				if (view.framebuffer.fragmentDensityTextures[i].memory != VK_NULL_HANDLE)
				{
					VC(appState.Device.vkDestroyImageView(appState.Device.device, view.framebuffer.fragmentDensityTextures[i].view, nullptr));
					VC(appState.Device.vkDestroyImage(appState.Device.device, view.framebuffer.fragmentDensityTextures[i].image, nullptr));
					VC(appState.Device.vkFreeMemory(appState.Device.device, view.framebuffer.fragmentDensityTextures[i].memory, nullptr));
				}
			}
		}
		if (view.framebuffer.depthImage != VK_NULL_HANDLE)
		{
			VC(appState.Device.vkDestroyImageView(appState.Device.device, view.framebuffer.depthImageView, nullptr));
			VC(appState.Device.vkDestroyImage(appState.Device.device, view.framebuffer.depthImage, nullptr));
			VC(appState.Device.vkFreeMemory(appState.Device.device, view.framebuffer.depthMemory, nullptr));
		}
		if (view.framebuffer.renderTexture.image != VK_NULL_HANDLE)
		{
			VC(appState.Device.vkDestroySampler(appState.Device.device, view.framebuffer.renderTexture.sampler, nullptr));
			if (view.framebuffer.renderTexture.memory != VK_NULL_HANDLE)
			{
				VC(appState.Device.vkDestroyImageView(appState.Device.device, view.framebuffer.renderTexture.view, nullptr));
				VC(appState.Device.vkDestroyImage(appState.Device.device, view.framebuffer.renderTexture.image, nullptr));
				VC(appState.Device.vkFreeMemory(appState.Device.device, view.framebuffer.renderTexture.memory, nullptr));
			}
		}
		vrapi_DestroyTextureSwapChain(view.framebuffer.colorTextureSwapChain);
		for (auto& perImage : view.perImage)
		{
			VC(appState.Device.vkFreeCommandBuffers(appState.Device.device, appState.Context.commandPool, 1, &perImage.commandBuffer));
			VC(appState.Device.vkDestroyFence(appState.Device.device, perImage.fence, nullptr));
			for (PipelineResources *pipelineResources = perImage.pipelineResources, *next = nullptr; pipelineResources != nullptr; pipelineResources = next)
			{
				next = pipelineResources->next;
				VC(appState.Device.vkFreeDescriptorSets(appState.Device.device, pipelineResources->descriptorPool, 1, &pipelineResources->descriptorSet));
				VC(appState.Device.vkDestroyDescriptorPool(appState.Device.device, pipelineResources->descriptorPool, nullptr));
				delete pipelineResources;
			}
			for (Buffer *b = perImage.sceneMatricesStagingBuffers.mapped, *next = nullptr; b != nullptr; b = next)
			{
				next = b->next;
				if (b->mapped != nullptr)
				{
					VC(appState.Device.vkUnmapMemory(appState.Device.device, b->memory));
				}
				VC(appState.Device.vkDestroyBuffer(appState.Device.device, b->buffer, nullptr));
				VC(appState.Device.vkFreeMemory(appState.Device.device, b->memory, nullptr));
				delete b;
			}
			for (Buffer *b = perImage.sceneMatricesStagingBuffers.oldMapped, *next = nullptr; b != nullptr; b = next)
			{
				next = b->next;
				if (b->mapped != nullptr)
				{
					VC(appState.Device.vkUnmapMemory(appState.Device.device, b->memory));
				}
				VC(appState.Device.vkDestroyBuffer(appState.Device.device, b->buffer, nullptr));
				VC(appState.Device.vkFreeMemory(appState.Device.device, b->memory, nullptr));
				delete b;
			}
			for (Buffer *b = perImage.texturedVertices.mapped, *next = nullptr; b != nullptr; b = next)
			{
				next = b->next;
				if (b->mapped != nullptr)
				{
					VC(appState.Device.vkUnmapMemory(appState.Device.device, b->memory));
				}
				VC(appState.Device.vkDestroyBuffer(appState.Device.device, b->buffer, nullptr));
				VC(appState.Device.vkFreeMemory(appState.Device.device, b->memory, nullptr));
				delete b;
			}
			for (Buffer *b = perImage.texturedVertices.oldMapped, *next = nullptr; b != nullptr; b = next)
			{
				next = b->next;
				if (b->mapped != nullptr)
				{
					VC(appState.Device.vkUnmapMemory(appState.Device.device, b->memory));
				}
				VC(appState.Device.vkDestroyBuffer(appState.Device.device, b->buffer, nullptr));
				VC(appState.Device.vkFreeMemory(appState.Device.device, b->memory, nullptr));
				delete b;
			}
			for (Buffer *b = perImage.texturedIndices.mapped, *next = nullptr; b != nullptr; b = next)
			{
				next = b->next;
				if (b->mapped != nullptr)
				{
					VC(appState.Device.vkUnmapMemory(appState.Device.device, b->memory));
				}
				VC(appState.Device.vkDestroyBuffer(appState.Device.device, b->buffer, nullptr));
				VC(appState.Device.vkFreeMemory(appState.Device.device, b->memory, nullptr));
				delete b;
			}
			for (Buffer *b = perImage.texturedIndices.oldMapped, *next = nullptr; b != nullptr; b = next)
			{
				next = b->next;
				if (b->mapped != nullptr)
				{
					VC(appState.Device.vkUnmapMemory(appState.Device.device, b->memory));
				}
				VC(appState.Device.vkDestroyBuffer(appState.Device.device, b->buffer, nullptr));
				VC(appState.Device.vkFreeMemory(appState.Device.device, b->memory, nullptr));
				delete b;
			}
			for (Buffer *b = perImage.times.mapped, *next = nullptr; b != nullptr; b = next)
			{
				next = b->next;
				if (b->mapped != nullptr)
				{
					VC(appState.Device.vkUnmapMemory(appState.Device.device, b->memory));
				}
				VC(appState.Device.vkDestroyBuffer(appState.Device.device, b->buffer, nullptr));
				VC(appState.Device.vkFreeMemory(appState.Device.device, b->memory, nullptr));
				delete b;
			}
			for (Buffer *b = perImage.times.oldMapped, *next = nullptr; b != nullptr; b = next)
			{
				next = b->next;
				if (b->mapped != nullptr)
				{
					VC(appState.Device.vkUnmapMemory(appState.Device.device, b->memory));
				}
				VC(appState.Device.vkDestroyBuffer(appState.Device.device, b->buffer, nullptr));
				VC(appState.Device.vkFreeMemory(appState.Device.device, b->memory, nullptr));
				delete b;
			}
			for (Buffer *b = perImage.stagingBuffers.mapped, *next = nullptr; b != nullptr; b = next)
			{
				next = b->next;
				if (b->mapped != nullptr)
				{
					VC(appState.Device.vkUnmapMemory(appState.Device.device, b->memory));
				}
				VC(appState.Device.vkDestroyBuffer(appState.Device.device, b->buffer, nullptr));
				VC(appState.Device.vkFreeMemory(appState.Device.device, b->memory, nullptr));
				delete b;
			}
			for (Buffer *b = perImage.stagingBuffers.oldMapped, *next = nullptr; b != nullptr; b = next)
			{
				next = b->next;
				if (b->mapped != nullptr)
				{
					VC(appState.Device.vkUnmapMemory(appState.Device.device, b->memory));
				}
				VC(appState.Device.vkDestroyBuffer(appState.Device.device, b->buffer, nullptr));
				VC(appState.Device.vkFreeMemory(appState.Device.device, b->memory, nullptr));
				delete b;
			}
			for (Texture *t = perImage.textures.textures, *next = nullptr; t != nullptr; t = next)
			{
				next = t->next;
				if (t->memory != VK_NULL_HANDLE)
				{
					VC(appState.Device.vkDestroyImageView(appState.Device.device, t->view, nullptr));
					VC(appState.Device.vkDestroyImage(appState.Device.device, t->image, nullptr));
					VC(appState.Device.vkFreeMemory(appState.Device.device, t->memory, nullptr));
				}
				VC(appState.Device.vkDestroySampler(appState.Device.device, t->sampler, nullptr));
				delete t;
			}
			for (Texture *t = perImage.textures.oldTextures, *next = nullptr; t != nullptr; t = next)
			{
				next = t->next;
				if (t->memory != VK_NULL_HANDLE)
				{
					VC(appState.Device.vkDestroyImageView(appState.Device.device, t->view, nullptr));
					VC(appState.Device.vkDestroyImage(appState.Device.device, t->image, nullptr));
					VC(appState.Device.vkFreeMemory(appState.Device.device, t->memory, nullptr));
				}
				VC(appState.Device.vkDestroySampler(appState.Device.device, t->sampler, nullptr));
				delete t;
			}
		}
	}
	VC(appState.Device.vkFreeCommandBuffers(appState.Device.device, appState.Context.commandPool, 1, &appState.Screen.CommandBuffer));
	vrapi_DestroyTextureSwapChain(appState.Screen.SwapChain);
	VC(appState.Device.vkFreeCommandBuffers(appState.Device.device, appState.Context.commandPool, 1, &appState.Console.CommandBuffer));
	vrapi_DestroyTextureSwapChain(appState.Console.SwapChain);
	VC(appState.Device.vkDestroyRenderPass(appState.Device.device, appState.RenderPass, nullptr));
	VK(appState.Device.vkQueueWaitIdle(appState.Context.queue));
	VC(appState.Device.vkDestroyPipeline(appState.Device.device, appState.Scene.turbulent.pipeline, nullptr));
	VC(appState.Device.vkDestroyPipelineLayout(appState.Device.device, appState.Scene.turbulent.pipelineLayout, nullptr));
	VC(appState.Device.vkDestroyDescriptorSetLayout(appState.Device.device, appState.Scene.turbulent.descriptorSetLayout, nullptr));
	VC(appState.Device.vkDestroyPipeline(appState.Device.device, appState.Scene.textured.pipeline, nullptr));
	VC(appState.Device.vkDestroyPipelineLayout(appState.Device.device, appState.Scene.textured.pipelineLayout, nullptr));
	VC(appState.Device.vkDestroyDescriptorSetLayout(appState.Device.device, appState.Scene.textured.descriptorSetLayout, nullptr));
	VC(appState.Device.vkDestroyShaderModule(appState.Device.device, appState.Scene.turbulentFragment, nullptr));
	VC(appState.Device.vkDestroyShaderModule(appState.Device.device, appState.Scene.texturedFragment, nullptr));
	VC(appState.Device.vkDestroyShaderModule(appState.Device.device, appState.Scene.texturedVertex, nullptr));
	if (appState.Scene.matrices.mapped != nullptr)
	{
		VC(appState.Device.vkUnmapMemory(appState.Device.device, appState.Scene.matrices.memory));
	}
	VC(appState.Device.vkDestroyBuffer(appState.Device.device, appState.Scene.matrices.buffer, nullptr));
	VC(appState.Device.vkFreeMemory(appState.Device.device, appState.Scene.matrices.memory, nullptr));
	appState.Scene.createdScene = false;
	vrapi_DestroySystemVulkan();
	if (appState.Context.device != nullptr)
	{
		if (pthread_mutex_trylock(&appState.Device.queueFamilyMutex) == EBUSY)
		{
			pthread_mutex_lock(&appState.Device.queueFamilyMutex);
		}
		if ((appState.Device.queueFamilyUsedQueues[appState.Context.queueFamilyIndex] & (1 << appState.Context.queueIndex)) != 0)
		{
			appState.Device.queueFamilyUsedQueues[appState.Context.queueFamilyIndex] &= ~(1 << appState.Context.queueIndex);
			pthread_mutex_unlock(&appState.Device.queueFamilyMutex);
			VC(appState.Device.vkDestroyCommandPool(appState.Device.device, appState.Context.commandPool, nullptr));
			VC(appState.Device.vkDestroyPipelineCache(appState.Device.device, appState.Context.pipelineCache, nullptr));
		}
	}
	VK(appState.Device.vkDeviceWaitIdle(appState.Device.device));
	pthread_mutex_destroy(&appState.Device.queueFamilyMutex);
	VC(appState.Device.vkDestroyDevice(appState.Device.device, nullptr));
	if (instance.validate && instance.vkDestroyDebugReportCallbackEXT != nullptr)
	{
		VC(instance.vkDestroyDebugReportCallbackEXT(instance.instance, instance.debugReportCallback, nullptr));
	}
	VC(instance.vkDestroyInstance(instance.instance, nullptr));
	if (instance.loader != nullptr)
	{
		dlclose(instance.loader);
	}
	vrapi_Shutdown();
	java.Vm->DetachCurrentThread();
}
