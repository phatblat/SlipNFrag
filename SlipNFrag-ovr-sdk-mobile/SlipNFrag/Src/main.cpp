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

#define DEBUG 1
#define _DEBUG 1

#define OFFSETOF_MEMBER(type, member) (size_t) & ((type*)0)->member
#define SIZEOF_MEMBER(type, member) sizeof(((type*)0)->member)
#define VK(func) checkErrors(func, #func);
#define VC(func) func;
#define MAX_PROGRAM_PARMS 16
#define MAX_VERTEX_ATTRIBUTES 16
#define ICD_SPV_MAGIC 0x07230203
#define PROGRAM(name) name##SPIRV
#define ARRAY_SIZE(a) (sizeof((a)) / sizeof((a)[0]))
#define MAX_SAVED_PUSH_CONSTANT_BYTES 512
#define MAX_VERTEX_BUFFER_UNUSED_COUNT 16
#define MAX_PIPELINE_RESOURCES_UNUSED_COUNT 16
#define VULKAN_LIBRARY "libvulkan.so"

enum BufferType
{
	BUFFER_TYPE_VERTEX,
	BUFFER_TYPE_INDEX,
	BUFFER_TYPE_UNIFORM
};

enum DefaultVertexAttributeFlags
{
	VERTEX_ATTRIBUTE_FLAG_POSITION = 1 << 0, // vec3 vertexPosition
	VERTEX_ATTRIBUTE_FLAG_NORMAL = 1 << 1, // vec3 vertexNormal
	VERTEX_ATTRIBUTE_FLAG_TANGENT = 1 << 2, // vec3 vertexTangent
	VERTEX_ATTRIBUTE_FLAG_BINORMAL = 1 << 3, // vec3 vertexBinormal
	VERTEX_ATTRIBUTE_FLAG_COLOR = 1 << 4, // vec4 vertexColor
	VERTEX_ATTRIBUTE_FLAG_UV0 = 1 << 5, // vec2 vertexUv0
	VERTEX_ATTRIBUTE_FLAG_UV1 = 1 << 6, // vec2 vertexUv1
	VERTEX_ATTRIBUTE_FLAG_UV2 = 1 << 7, // vec2 vertexUv2
	VERTEX_ATTRIBUTE_FLAG_JOINT_INDICES = 1 << 8, // vec4 jointIndices
	VERTEX_ATTRIBUTE_FLAG_JOINT_WEIGHTS = 1 << 9, // vec4 jointWeights
	VERTEX_ATTRIBUTE_FLAG_TRANSFORM = 1 << 10 // mat4 vertexTransform (NOTE this mat4 takes up 4 attribute locations)
};

enum ProgramParmType
{
	PROGRAM_PARM_TYPE_TEXTURE_SAMPLED, // texture plus sampler bound together		(GLSL:
	// sampler*, isampler*, usampler*)
			PROGRAM_PARM_TYPE_TEXTURE_STORAGE, // not sampled, direct read-write storage	(GLSL:
	// image*, iimage*, uimage*)
			PROGRAM_PARM_TYPE_BUFFER_UNIFORM, // read-only uniform buffer					(GLSL:
	// uniform)
			PROGRAM_PARM_TYPE_PUSH_CONSTANT_INT, // int										(GLSL: int)
	PROGRAM_PARM_TYPE_PUSH_CONSTANT_INT_VECTOR2, // int[2] (GLSL: ivec2)
	PROGRAM_PARM_TYPE_PUSH_CONSTANT_INT_VECTOR3, // int[3] (GLSL: ivec3)
	PROGRAM_PARM_TYPE_PUSH_CONSTANT_INT_VECTOR4, // int[4] (GLSL: ivec4)
	PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT, // float									(GLSL:
	// float)
			PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_VECTOR2, // float[2] (GLSL: vec2)
	PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_VECTOR3, // float[3] (GLSL: vec3)
	PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_VECTOR4, // float[4] (GLSL: vec4)
	PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX2X2, // float[2][2]
	// (GLSL: mat2x2 or mat2)
			PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX2X3, // float[2][3] (GLSL: mat2x3)
	PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX2X4, // float[2][4] (GLSL: mat2x4)
	PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX3X2, // float[3][2] (GLSL: mat3x2)
	PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX3X3, // float[3][3]
	// (GLSL: mat3x3 or mat3)
			PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX3X4, // float[3][4] (GLSL: mat3x4)
	PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX4X2, // float[4][2] (GLSL: mat4x2)
	PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX4X3, // float[4][3] (GLSL: mat4x3)
	PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX4X4, // float[4][4]
	// (GLSL: mat4x4 or mat4)
			PROGRAM_PARM_TYPE_MAX
};

enum ProgramParmAccess
{
	PROGRAM_PARM_ACCESS_READ_ONLY,
	PROGRAM_PARM_ACCESS_WRITE_ONLY,
	PROGRAM_PARM_ACCESS_READ_WRITE
};

enum ProgramStageFlags
{
	PROGRAM_STAGE_FLAG_VERTEX = 1 << 0,
	PROGRAM_STAGE_FLAG_FRAGMENT = 1 << 1,
	PROGRAM_STAGE_MAX = 2
};

enum TextureUsage
{
	TEXTURE_USAGE_UNDEFINED = 1 << 0,
	TEXTURE_USAGE_GENERAL = 1 << 1,
	TEXTURE_USAGE_TRANSFER_SRC = 1 << 2,
	TEXTURE_USAGE_TRANSFER_DST = 1 << 3,
	TEXTURE_USAGE_SAMPLED = 1 << 4,
	TEXTURE_USAGE_STORAGE = 1 << 5,
	TEXTURE_USAGE_COLOR_ATTACHMENT = 1 << 6,
	TEXTURE_USAGE_PRESENTATION = 1 << 7,
	TEXTURE_USAGE_FRAG_DENSITY = 1 << 8,
};

enum SurfaceDepthFormat
{
	SURFACE_DEPTH_FORMAT_NONE,
	SURFACE_DEPTH_FORMAT_D16,
	SURFACE_DEPTH_FORMAT_D24,
	SURFACE_DEPTH_FORMAT_MAX
};

struct Instance
{
	void *loader;
	VkInstance instance;
	VkBool32 validate;

	// Global functions.
	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
	PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties;
	PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties;
	PFN_vkCreateInstance vkCreateInstance;

	// Instance functions.
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

	// Debug callback.
	PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
	PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
	VkDebugReportCallbackEXT debugReportCallback;

};

struct Device
{
	Instance *instance;
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

	// The logical device.
	VkDevice device;

	// Device functions
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
	Device *device;
	uint32_t queueFamilyIndex;
	uint32_t queueIndex;
	VkQueue queue;
	VkCommandPool commandPool;
	VkPipelineCache pipelineCache;
	VkCommandBuffer setupCommandBuffer;
};

struct VertexAttribute
{
	int attributeFlag; // VERTEX_ATTRIBUTE_FLAG_
	size_t attributeOffset; // Offset in bytes to the pointer in VertexAttributeArrays
	size_t attributeSize; // Size in bytes of a single attribute
	VkFormat attributeFormat; // Format of the attribute
	int locationCount; // Number of attribute locations
	const char *name; // Name in vertex program
};

struct Buffer
{
	Buffer *next;
	int unusedCount;
	BufferType type;
	size_t size;
	VkMemoryPropertyFlags flags;
	VkBuffer buffer;
	VkDeviceMemory memory;
	void *mapped;
	bool owner;
};

struct Geometry
{
	const VertexAttribute *layout;
	int vertexAttribsFlags;
	int instanceAttribsFlags;
	int vertexCount;
	int instanceCount;
	int indexCount;
	Buffer vertexBuffer;
	Buffer instanceBuffer;
	Buffer indexBuffer;
};

typedef unsigned short TriangleIndex;

struct VertexAttributeArrays
{
	const Buffer *buffer;
	const VertexAttribute *layout;
	void *data;
	size_t dataSize;
	int vertexCount;
	int attribsFlags;
};

struct DefaultVertexAttributeArrays
{
	VertexAttributeArrays base;
	ovrVector3f *position;
	ovrVector3f *normal;
	ovrVector3f *tangent;
	ovrVector3f *binormal;
	ovrVector4f *color;
	ovrVector2f *uv0;
	ovrVector2f *uv1;
	ovrVector2f *uv2;
	ovrVector4f *jointIndices;
	ovrVector4f *jointWeights;
	ovrMatrix4f *transform;
};

struct TriangleIndexArray
{
	const Buffer *buffer;
	TriangleIndex *indexArray;
	int indexCount;
};

struct ProgramParm
{
	int stageFlags; // vertex, fragment
	ProgramParmType type; // texture, buffer or push constant
	ProgramParmAccess access; // read and/or write
	int index; // index into ProgramParmState::parms
	const char *name; // GLSL name
	int binding; // Vulkan texture/buffer binding, or push constant offset
	// Note that all Vulkan bindings must be unique per descriptor set across all
	// stages of the pipeline. Note that all Vulkan push constant ranges must be unique
	// across all stages of the pipeline.
};

typedef unsigned int TextureUsageFlags;

struct Texture
{
	int width;
	int height;
	int depth;
	int layerCount;
	int mipCount;
	VkSampleCountFlagBits sampleCount;
	TextureUsage usage;
	TextureUsageFlags usageFlags;
	VkSamplerAddressMode wrapMode;
	VkSamplerMipmapMode filter;
	float maxAnisotropy;
	VkFormat format;
	VkImageLayout imageLayout;
	VkImage image;
	VkDeviceMemory memory;
	VkImageView view;
	VkSampler sampler;
};

struct DepthBuffer
{
	SurfaceDepthFormat format;
	VkFormat internalFormat;
	VkImageLayout imageLayout;
	VkImage image;
	VkDeviceMemory memory;
	VkImageView view;
};

struct RenderPass
{
	VkSubpassContents type;
	SurfaceDepthFormat depthFormat;
	bool useFragmentDensity;
	VkSampleCountFlagBits sampleCount;
	VkFormat internalColorFormat;
	VkFormat internalDepthFormat;
	VkFormat internalFragmentDensityFormat;
	VkRenderPass renderPass;
	ovrVector4f clearColor;
};

struct FramebufferTextures
{
	std::vector<Texture> colorTextures;
	std::vector<Texture> fragmentDensityTextures;
	Texture renderTexture;
	DepthBuffer depthBuffer;
	std::vector<VkFramebuffer> framebuffers;
	RenderPass *renderPass;
	int width;
	int height;
	int numLayers;
	int numBuffers;
	int currentBuffer;
	int currentLayer;
};

struct ProgramParmLayout
{
	int numParms;
	const ProgramParm *parms;
	VkDescriptorSetLayout descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	std::vector<int> offsetForIndex;
	std::vector<ProgramParm*> bindings;
	std::vector<ProgramParm*> pushConstants;
	int numBindings;
	unsigned int hash;
};

struct GraphicsProgram
{
	VkShaderModule vertexShaderModule;
	VkShaderModule fragmentShaderModule;
	VkPipelineShaderStageCreateInfo pipelineStages[2];
	ProgramParmLayout parmLayout;
	int vertexAttribsFlags;
};

struct RasterOperations
{
	bool blendEnable;
	bool redWriteEnable;
	bool blueWriteEnable;
	bool greenWriteEnable;
	bool alphaWriteEnable;
	bool depthTestEnable;
	bool depthWriteEnable;
	VkFrontFace frontFace;
	VkCullModeFlagBits cullMode;
	VkCompareOp depthCompare;
	ovrVector4f blendColor;
	VkBlendOp blendOpColor;
	VkBlendFactor blendSrcColor;
	VkBlendFactor blendDstColor;
	VkBlendOp blendOpAlpha;
	VkBlendFactor blendSrcAlpha;
	VkBlendFactor blendDstAlpha;
};

struct GraphicsPipeline
{
	RasterOperations rop;
	const GraphicsProgram *program;
	const Geometry *geometry;
	int vertexAttributeCount;
	int vertexBindingCount;
	int firstInstanceBinding;
	VkVertexInputAttributeDescription vertexAttributes[MAX_VERTEX_ATTRIBUTES];
	VkVertexInputBindingDescription vertexBindings[MAX_VERTEX_ATTRIBUTES];
	VkDeviceSize vertexBindingOffsets[MAX_VERTEX_ATTRIBUTES];
	VkPipelineVertexInputStateCreateInfo vertexInputState;
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState;
	VkPipeline pipeline;
};

struct GraphicsPipelineParms
{
	RasterOperations rop;
	const RenderPass *renderPass;
	const GraphicsProgram *program;
	const Geometry *geometry;
};

struct Fence
{
	VkFence fence;
	bool submitted;
};

struct ProgramParmState
{
	const void *parms[MAX_PROGRAM_PARMS];
	unsigned char data[MAX_SAVED_PUSH_CONSTANT_BYTES];
};

struct PipelineResources
{
	PipelineResources *next;
	int unusedCount; // Number of frames these resources have not been used.
	const ProgramParmLayout *parmLayout;
	ProgramParmState parms;
	VkDescriptorPool descriptorPool;
	VkDescriptorSet descriptorSet;
};

struct GraphicsCommand
{
	const GraphicsPipeline *pipeline;
	const Buffer* vertexBuffer;
	const Buffer* instanceBuffer;
	ProgramParmState parmState;
	int numInstances;
};

struct CommandBuffer
{
	int numBuffers;
	int currentBuffer;
	std::vector<VkCommandBuffer> cmdBuffers;
	Context *context;
	std::vector<Fence> fences;
	std::vector<Buffer*> mappedBuffers;
	std::vector<Buffer*> oldMappedBuffers;
	std::vector<PipelineResources*> pipelineResources;
	GraphicsCommand currentGraphicsState;
	FramebufferTextures *currentFramebuffer;
	RenderPass *currentRenderPass;
};

static const VertexAttribute DefaultVertexAttributeLayout[] = {
		{VERTEX_ATTRIBUTE_FLAG_POSITION,
				OFFSETOF_MEMBER(DefaultVertexAttributeArrays, position),
				SIZEOF_MEMBER(DefaultVertexAttributeArrays, position[0]),
				  VK_FORMAT_R32G32B32_SFLOAT,
								1,
								   "vertexPosition"},
		{VERTEX_ATTRIBUTE_FLAG_NORMAL,
				OFFSETOF_MEMBER(DefaultVertexAttributeArrays, normal),
				SIZEOF_MEMBER(DefaultVertexAttributeArrays, normal[0]),
				  VK_FORMAT_R32G32B32_SFLOAT,
								1,
								   "vertexNormal"},
		{VERTEX_ATTRIBUTE_FLAG_TANGENT,
				OFFSETOF_MEMBER(DefaultVertexAttributeArrays, tangent),
				SIZEOF_MEMBER(DefaultVertexAttributeArrays, tangent[0]),
				  VK_FORMAT_R32G32B32_SFLOAT,
								1,
								   "vertexTangent"},
		{VERTEX_ATTRIBUTE_FLAG_BINORMAL,
				OFFSETOF_MEMBER(DefaultVertexAttributeArrays, binormal),
				SIZEOF_MEMBER(DefaultVertexAttributeArrays, binormal[0]),
				  VK_FORMAT_R32G32B32_SFLOAT,
								1,
								   "vertexBinormal"},
		{VERTEX_ATTRIBUTE_FLAG_COLOR,
				OFFSETOF_MEMBER(DefaultVertexAttributeArrays, color),
				SIZEOF_MEMBER(DefaultVertexAttributeArrays, color[0]),
				  VK_FORMAT_R32G32B32A32_SFLOAT,
								1,
								   "vertexColor"},
		{VERTEX_ATTRIBUTE_FLAG_UV0,
				OFFSETOF_MEMBER(DefaultVertexAttributeArrays, uv0),
				SIZEOF_MEMBER(DefaultVertexAttributeArrays, uv0[0]),
				  VK_FORMAT_R32G32_SFLOAT,
								1,
								   "vertexUv0"},
		{VERTEX_ATTRIBUTE_FLAG_UV1,
				OFFSETOF_MEMBER(DefaultVertexAttributeArrays, uv1),
				SIZEOF_MEMBER(DefaultVertexAttributeArrays, uv1[0]),
				  VK_FORMAT_R32G32_SFLOAT,
								1,
								   "vertexUv1"},
		{VERTEX_ATTRIBUTE_FLAG_UV2,
				OFFSETOF_MEMBER(DefaultVertexAttributeArrays, uv2),
				SIZEOF_MEMBER(DefaultVertexAttributeArrays, uv2[0]),
				  VK_FORMAT_R32G32_SFLOAT,
								1,
								   "vertexUv2"},
		{VERTEX_ATTRIBUTE_FLAG_JOINT_INDICES,
				OFFSETOF_MEMBER(DefaultVertexAttributeArrays, jointIndices),
				SIZEOF_MEMBER(DefaultVertexAttributeArrays, jointIndices[0]),
				  VK_FORMAT_R32G32B32A32_SFLOAT,
								1,
								   "vertexJointIndices"},
		{VERTEX_ATTRIBUTE_FLAG_JOINT_WEIGHTS,
				OFFSETOF_MEMBER(DefaultVertexAttributeArrays, jointWeights),
				SIZEOF_MEMBER(DefaultVertexAttributeArrays, jointWeights[0]),
				  VK_FORMAT_R32G32B32A32_SFLOAT,
								1,
								   "vertexJointWeights"},
		{VERTEX_ATTRIBUTE_FLAG_TRANSFORM,
				OFFSETOF_MEMBER(DefaultVertexAttributeArrays, transform),
				SIZEOF_MEMBER(DefaultVertexAttributeArrays, transform[0]),
				  VK_FORMAT_R32G32B32A32_SFLOAT,
								4,
								   "vertexTransform"},
		{0, 0, 0, (VkFormat) 0, 0, ""}};

static const VkQueueFlags requiredQueueFlags = VK_QUEUE_GRAPHICS_BIT;

static const int queueCount = 1;

static void checkErrors(VkResult result, const char *function)
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

static uint32_t getMemoryTypeIndex(Device *device, const uint32_t typeBits, const VkMemoryPropertyFlags requiredProperties)
{
	// Search memory types to find the index with the requested properties.
	for (uint32_t type = 0; type < device->physicalDeviceMemoryProperties.memoryTypeCount; type++)
	{
		if ((typeBits & (1 << type)) != 0)
		{
			// Test if this memory type has the required properties.
			const VkFlags propertyFlags =
					device->physicalDeviceMemoryProperties.memoryTypes[type].propertyFlags;
			if ((propertyFlags & requiredProperties) == requiredProperties)
			{
				return type;
			}
		}
	}
	__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Memory type %d with properties %d not found.", typeBits, requiredProperties);
	return 0;
}

VkPipelineStageFlags pipelineStagesForTextureUsage(const TextureUsage usage, const bool from)
{
	return (
			(usage == TEXTURE_USAGE_UNDEFINED)
			? (VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)
			: ((usage == TEXTURE_USAGE_GENERAL)
			   ? (VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)
			   : ((usage == TEXTURE_USAGE_TRANSFER_SRC)
				  ? (VK_PIPELINE_STAGE_TRANSFER_BIT)
				  : ((usage == TEXTURE_USAGE_TRANSFER_DST)
					 ? (VK_PIPELINE_STAGE_TRANSFER_BIT)
					 : ((usage == TEXTURE_USAGE_SAMPLED)
						? (VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
						   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
						: ((usage == TEXTURE_USAGE_STORAGE)
						   ? (VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
							  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
						   : ((usage == TEXTURE_USAGE_COLOR_ATTACHMENT)
							  ? (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT)
							  : ((usage == TEXTURE_USAGE_PRESENTATION)
								 ? (from
									? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
									: VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
								 : 0))))))));
}

void beginSetupCommandBuffer(Context *context)
{
	if (context->setupCommandBuffer != VK_NULL_HANDLE)
	{
		return;
	}
	VkCommandBufferAllocateInfo commandBufferAllocateInfo { };
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.commandPool = context->commandPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = 1;
	VK(context->device->vkAllocateCommandBuffers(context->device->device, &commandBufferAllocateInfo, &context->setupCommandBuffer));
	VkCommandBufferBeginInfo commandBufferBeginInfo { };
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VK(context->device->vkBeginCommandBuffer(context->setupCommandBuffer, &commandBufferBeginInfo));
}

void flushSetupCommandBuffer(Context *context)
{
	if (context->setupCommandBuffer == VK_NULL_HANDLE)
	{
		return;
	}
	VK(context->device->vkEndCommandBuffer(context->setupCommandBuffer));
	VkSubmitInfo submitInfo { };
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &context->setupCommandBuffer;
	VK(context->device->vkQueueSubmit(context->queue, 1, &submitInfo, VK_NULL_HANDLE));
	VK(context->device->vkQueueWaitIdle(context->queue));
	VC(context->device->vkFreeCommandBuffers(context->device->device, context->commandPool, 1, &context->setupCommandBuffer));
	context->setupCommandBuffer = VK_NULL_HANDLE;
}

bool createBuffer(Context *context, Buffer *buffer, const BufferType type, const size_t dataSize, const void *data, const bool hostVisible)
{
	memset(buffer, 0, sizeof(Buffer));

	assert(
			dataSize <=
			context->device->physicalDeviceProperties.properties.limits.maxStorageBufferRange);

	buffer->type = type;
	buffer->size = dataSize;
	buffer->owner = true;

	VkBufferCreateInfo bufferCreateInfo { };
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = dataSize;
	switch (type)
	{
		case BUFFER_TYPE_VERTEX:
			bufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			break;
		case BUFFER_TYPE_INDEX:
			bufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
			break;
		case BUFFER_TYPE_UNIFORM:
			bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			break;
	}
	if (hostVisible)
	{
		bufferCreateInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	}
	else
	{
		bufferCreateInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferCreateInfo.queueFamilyIndexCount = 0;
	bufferCreateInfo.pQueueFamilyIndices = NULL;

	VK(context->device->vkCreateBuffer(context->device->device, &bufferCreateInfo, nullptr, &buffer->buffer));

	buffer->flags =
			hostVisible ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	VkMemoryRequirements memoryRequirements;
	VC(context->device->vkGetBufferMemoryRequirements(
			context->device->device, buffer->buffer, &memoryRequirements));

	VkMemoryAllocateInfo memoryAllocateInfo;
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.pNext = NULL;
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = getMemoryTypeIndex(context->device, memoryRequirements.memoryTypeBits, buffer->flags);

	VK(context->device->vkAllocateMemory(
			context->device->device, &memoryAllocateInfo, nullptr, &buffer->memory));
	VK(context->device->vkBindBufferMemory(
			context->device->device, buffer->buffer, buffer->memory, 0));

	if (data != NULL)
	{
		if (hostVisible)
		{
			void *mapped;
			VK(context->device->vkMapMemory(
					context->device->device, buffer->memory, 0, memoryRequirements.size, 0, &mapped));
			memcpy(mapped, data, dataSize);
			VC(context->device->vkUnmapMemory(context->device->device, buffer->memory));

			VkMappedMemoryRange mappedMemoryRange;
			mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			mappedMemoryRange.pNext = NULL;
			mappedMemoryRange.memory = buffer->memory;
			mappedMemoryRange.offset = 0;
			mappedMemoryRange.size = VK_WHOLE_SIZE;
			VC(context->device->vkFlushMappedMemoryRanges(
					context->device->device, 1, &mappedMemoryRange));
		}
		else
		{
			VkBufferCreateInfo stagingBufferCreateInfo;
			stagingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			stagingBufferCreateInfo.pNext = NULL;
			stagingBufferCreateInfo.flags = 0;
			stagingBufferCreateInfo.size = dataSize;
			stagingBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			stagingBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			stagingBufferCreateInfo.queueFamilyIndexCount = 0;
			stagingBufferCreateInfo.pQueueFamilyIndices = NULL;

			VkBuffer srcBuffer;
			VK(context->device->vkCreateBuffer(
					context->device->device, &stagingBufferCreateInfo, nullptr, &srcBuffer));

			VkMemoryRequirements stagingMemoryRequirements;
			VC(context->device->vkGetBufferMemoryRequirements(
					context->device->device, srcBuffer, &stagingMemoryRequirements));

			VkMemoryAllocateInfo stagingMemoryAllocateInfo;
			stagingMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			stagingMemoryAllocateInfo.pNext = NULL;
			stagingMemoryAllocateInfo.allocationSize = stagingMemoryRequirements.size;
			stagingMemoryAllocateInfo.memoryTypeIndex = getMemoryTypeIndex(context->device, stagingMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

			VkDeviceMemory srcMemory;
			VK(context->device->vkAllocateMemory(
					context->device->device, &stagingMemoryAllocateInfo, nullptr, &srcMemory));
			VK(context->device->vkBindBufferMemory(
					context->device->device, srcBuffer, srcMemory, 0));

			void *mapped;
			VK(context->device->vkMapMemory(
					context->device->device, srcMemory, 0, stagingMemoryRequirements.size, 0, &mapped));
			memcpy(mapped, data, dataSize);
			VC(context->device->vkUnmapMemory(context->device->device, srcMemory));

			VkMappedMemoryRange mappedMemoryRange;
			mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			mappedMemoryRange.pNext = NULL;
			mappedMemoryRange.memory = srcMemory;
			mappedMemoryRange.offset = 0;
			mappedMemoryRange.size = VK_WHOLE_SIZE;
			VC(context->device->vkFlushMappedMemoryRanges(
					context->device->device, 1, &mappedMemoryRange));

			beginSetupCommandBuffer(context);

			VkBufferCopy bufferCopy;
			bufferCopy.srcOffset = 0;
			bufferCopy.dstOffset = 0;
			bufferCopy.size = dataSize;

			VC(context->device->vkCmdCopyBuffer(
					context->setupCommandBuffer, srcBuffer, buffer->buffer, 1, &bufferCopy));

			flushSetupCommandBuffer(context);

			VC(context->device->vkDestroyBuffer(context->device->device, srcBuffer, nullptr));
			VC(context->device->vkFreeMemory(context->device->device, srcMemory, nullptr));
		}
	}

	return true;
}

void updateTextureSampler(Context *context, Texture *texture)
{
	if (texture->sampler != VK_NULL_HANDLE)
	{
		VC(context->device->vkDestroySampler(context->device->device, texture->sampler, nullptr));
	}
	const VkSamplerMipmapMode mipmapMode = texture->filter;
	const VkSamplerAddressMode addressMode = texture->wrapMode;
	VkSamplerCreateInfo samplerCreateInfo { };
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter = (texture->filter == VK_SAMPLER_MIPMAP_MODE_NEAREST) ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = (texture->filter == VK_SAMPLER_MIPMAP_MODE_NEAREST) ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode = mipmapMode;
	samplerCreateInfo.addressModeU = addressMode;
	samplerCreateInfo.addressModeV = addressMode;
	samplerCreateInfo.addressModeW = addressMode;
	samplerCreateInfo.anisotropyEnable = (texture->maxAnisotropy > 1.0f);
	samplerCreateInfo.maxAnisotropy = texture->maxAnisotropy;
	samplerCreateInfo.maxLod = (float) texture->mipCount;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	VK(context->device->vkCreateSampler(context->device->device, &samplerCreateInfo, nullptr, &texture->sampler));
}

int integerLog2(int i)
{
	int r = 0;
	int t;
	t = ((~((i >> 16) + ~0U)) >> 27) & 0x10;
	r |= t;
	i >>= t;
	t = ((~((i >> 8) + ~0U)) >> 28) & 0x08;
	r |= t;
	i >>= t;
	t = ((~((i >> 4) + ~0U)) >> 29) & 0x04;
	r |= t;
	i >>= t;
	t = ((~((i >> 2) + ~0U)) >> 30) & 0x02;
	r |= t;
	i >>= t;
	return (r | (i >> 1));
}

VkImageLayout layoutForTextureUsage(const TextureUsage usage)
{
	return (
			(usage == TEXTURE_USAGE_UNDEFINED)
			? VK_IMAGE_LAYOUT_UNDEFINED
			: ((usage == TEXTURE_USAGE_GENERAL)
			   ? VK_IMAGE_LAYOUT_GENERAL
			   : ((usage == TEXTURE_USAGE_TRANSFER_SRC)
				  ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
				  : ((usage == TEXTURE_USAGE_TRANSFER_DST)
					 ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
					 : ((usage == TEXTURE_USAGE_SAMPLED)
						? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
						: ((usage == TEXTURE_USAGE_STORAGE)
						   ? VK_IMAGE_LAYOUT_GENERAL
						   : ((usage == TEXTURE_USAGE_COLOR_ATTACHMENT)
							  ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
							  : ((usage == TEXTURE_USAGE_PRESENTATION)
								 ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
								 : (VkImageLayout) 0))))))));
}

VkAccessFlags accessForTextureUsage(const TextureUsage usage)
{
	return (
			(usage == TEXTURE_USAGE_UNDEFINED)
			? (0)
			: ((usage == TEXTURE_USAGE_GENERAL)
			   ? (0)
			   : ((usage == TEXTURE_USAGE_TRANSFER_SRC)
				  ? (VK_ACCESS_TRANSFER_READ_BIT)
				  : ((usage == TEXTURE_USAGE_TRANSFER_DST)
					 ? (VK_ACCESS_TRANSFER_WRITE_BIT)
					 : ((usage == TEXTURE_USAGE_SAMPLED)
						? (VK_ACCESS_SHADER_READ_BIT)
						: ((usage == TEXTURE_USAGE_STORAGE)
						   ? (VK_ACCESS_SHADER_READ_BIT |
							  VK_ACCESS_SHADER_WRITE_BIT)
						   : ((usage == TEXTURE_USAGE_COLOR_ATTACHMENT)
							  ? (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
								 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
							  : ((usage == TEXTURE_USAGE_PRESENTATION)
								 ? (VK_ACCESS_MEMORY_READ_BIT)
								 : 0))))))));
}

void createShader(Device *device, VkShaderModule *shaderModule, const VkShaderStageFlagBits stage, const void *code, size_t codeSize)
{
	VkShaderModuleCreateInfo moduleCreateInfo;
	moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleCreateInfo.pNext = NULL;
	moduleCreateInfo.flags = 0;
	moduleCreateInfo.codeSize = 0;
	moduleCreateInfo.pCode = NULL;

	if (*(uint32_t *) code == ICD_SPV_MAGIC)
	{
		moduleCreateInfo.codeSize = codeSize;
		moduleCreateInfo.pCode = (uint32_t *) code;

		VK(device->vkCreateShaderModule(
				device->device, &moduleCreateInfo, nullptr, shaderModule));
	}
	else
	{
		// Create fake SPV structure to feed GLSL to the driver "under the covers".
		size_t tempCodeSize = 3 * sizeof(uint32_t) + codeSize + 1;
		uint32_t *tempCode = (uint32_t *) malloc(tempCodeSize);
		tempCode[0] = ICD_SPV_MAGIC;
		tempCode[1] = 0;
		tempCode[2] = stage;
		memcpy(tempCode + 3, code, codeSize + 1);

		moduleCreateInfo.codeSize = tempCodeSize;
		moduleCreateInfo.pCode = tempCode;

		VK(device->vkCreateShaderModule(
				device->device, &moduleCreateInfo, nullptr, shaderModule));

		free(tempCode);
	}
}

VkDescriptorType getDescriptorType(const ProgramParmType type)
{
	return (
			(type == PROGRAM_PARM_TYPE_TEXTURE_SAMPLED)
			? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
			: ((type == PROGRAM_PARM_TYPE_TEXTURE_STORAGE)
			   ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
			   : ((type == PROGRAM_PARM_TYPE_BUFFER_UNIFORM)
				  ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
				  : VK_DESCRIPTOR_TYPE_MAX_ENUM)));
}

int getPushConstantSize(ProgramParmType type)
{
	static const int parmSize[PROGRAM_PARM_TYPE_MAX] = {(unsigned int) 0,
															(unsigned int) 0,
															(unsigned int) 0,
															(unsigned int) sizeof(int),
															(unsigned int) sizeof(int[2]),
															(unsigned int) sizeof(int[3]),
															(unsigned int) sizeof(int[4]),
															(unsigned int) sizeof(float),
															(unsigned int) sizeof(float[2]),
															(unsigned int) sizeof(float[3]),
															(unsigned int) sizeof(float[4]),
															(unsigned int) sizeof(float[2][2]),
															(unsigned int) sizeof(float[2][3]),
															(unsigned int) sizeof(float[2][4]),
															(unsigned int) sizeof(float[3][2]),
															(unsigned int) sizeof(float[3][3]),
															(unsigned int) sizeof(float[3][4]),
															(unsigned int) sizeof(float[4][2]),
															(unsigned int) sizeof(float[4][3]),
															(unsigned int) sizeof(float[4][4])};
	assert((sizeof(parmSize) / sizeof(parmSize[0])) == PROGRAM_PARM_TYPE_MAX);
	return parmSize[type];
}

static VkAccessFlags getBufferAccess(const BufferType type)
{
	return (
			(type == BUFFER_TYPE_INDEX)
			? VK_ACCESS_INDEX_READ_BIT
			: ((type == BUFFER_TYPE_VERTEX)
			   ? VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
			   : ((type == BUFFER_TYPE_UNIFORM) ? VK_ACCESS_UNIFORM_READ_BIT : 0)));
}

static VkPipelineStageFlags pipelineStagesForBufferUsage(const BufferType type)
{
	return (
			(type == BUFFER_TYPE_INDEX)
			? VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT
			: ((type == BUFFER_TYPE_VERTEX)
			   ? VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
			   : ((type == BUFFER_TYPE_UNIFORM) ? VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT : 0)));
}

bool descriptorsMatch(const ProgramParmLayout *layout1, const ProgramParmState *parmState1, const ProgramParmLayout *layout2, const ProgramParmState *parmState2)
{
	if (layout1 == nullptr || layout2 == nullptr)
	{
		return false;
	}
	if (layout1->hash != layout2->hash)
	{
		return false;
	}
	for (int i = 0; i < layout1->numBindings; i++)
	{
		if (parmState1->parms[layout1->bindings[i]->index] != parmState2->parms[layout2->bindings[i]->index])
		{
			return false;
		}
	}
	return true;
}

static const int CPU_LEVEL = 2;
static const int GPU_LEVEL = 3;
static VkSampleCountFlagBits SAMPLE_COUNT = VK_SAMPLE_COUNT_4_BIT;

enum
{
	PROGRAM_UNIFORM_SCENE_MATRICES,
};

static ProgramParm colorOnlyProgramParms[] =
{
	{
		PROGRAM_STAGE_FLAG_VERTEX,
		PROGRAM_PARM_TYPE_BUFFER_UNIFORM,
		PROGRAM_PARM_ACCESS_READ_ONLY,
		PROGRAM_UNIFORM_SCENE_MATRICES,
		"SceneMatrices",
		0
	}
};

#define GLSL_VERSION "440 core" // maintain precision decorations: "310 es"
#define GLSL_EXTENSIONS                             \
    "#extension GL_EXT_shader_io_blocks : enable\n" \
    "#extension GL_ARB_enhanced_layouts : enable\n"

// clang-format off
static const char colorOnlyVertexProgramGLSL[] =
		"#version " GLSL_VERSION "\n"
		GLSL_EXTENSIONS
		"layout( std140, binding = 0 ) uniform SceneMatrices\n"
		"{\n"
		"	layout( offset =   0 ) mat4 ViewMatrix;\n"
		"	layout( offset =  64 ) mat4 ProjectionMatrix;\n"
		"};\n"
		"layout( location = 0 ) in vec3 vertexPosition;\n"
		"layout( location = 1 ) in vec4 vertexColor;\n"
		"layout( location = 2 ) in mat4 vertexTransform;\n"
		"layout( location = 0 ) out lowp vec4 fragmentColor;\n"
		"out gl_PerVertex { vec4 gl_Position; };\n"
		"void main( void )\n"
		"{\n"
		"	gl_Position = ProjectionMatrix * ( ViewMatrix * ( vertexTransform * vec4( vertexPosition * 0.1, 1.0 ) ) );\n"
		"	fragmentColor = vertexColor;\n"
		"}\n";

static const unsigned int colorOnlyVertexProgramSPIRV[] = {
		// 7.11.3057
		0x07230203, 0x00010000, 0x00080007, 0x0000002e, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
		0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
		0x000a000f, 0x00000000, 0x00000004, 0x6e69616d, 0x00000000, 0x0000000a, 0x00000018, 0x0000001c,
		0x0000002a, 0x0000002c, 0x00030003, 0x00000002, 0x000001b8, 0x00070004, 0x415f4c47, 0x655f4252,
		0x6e61686e, 0x5f646563, 0x6f79616c, 0x00737475, 0x00070004, 0x455f4c47, 0x735f5458, 0x65646168,
		0x6f695f72, 0x6f6c625f, 0x00736b63, 0x00040005, 0x00000004, 0x6e69616d, 0x00000000, 0x00060005,
		0x00000008, 0x505f6c67, 0x65567265, 0x78657472, 0x00000000, 0x00060006, 0x00000008, 0x00000000,
		0x505f6c67, 0x7469736f, 0x006e6f69, 0x00030005, 0x0000000a, 0x00000000, 0x00060005, 0x0000000e,
		0x6e656353, 0x74614d65, 0x65636972, 0x00000073, 0x00060006, 0x0000000e, 0x00000000, 0x77656956,
		0x7274614d, 0x00007869, 0x00080006, 0x0000000e, 0x00000001, 0x6a6f7250, 0x69746365, 0x614d6e6f,
		0x78697274, 0x00000000, 0x00030005, 0x00000010, 0x00000000, 0x00060005, 0x00000018, 0x74726576,
		0x72547865, 0x66736e61, 0x006d726f, 0x00060005, 0x0000001c, 0x74726576, 0x6f507865, 0x69746973,
		0x00006e6f, 0x00060005, 0x0000002a, 0x67617266, 0x746e656d, 0x6f6c6f43, 0x00000072, 0x00050005,
		0x0000002c, 0x74726576, 0x6f437865, 0x00726f6c, 0x00050048, 0x00000008, 0x00000000, 0x0000000b,
		0x00000000, 0x00030047, 0x00000008, 0x00000002, 0x00040048, 0x0000000e, 0x00000000, 0x00000005,
		0x00050048, 0x0000000e, 0x00000000, 0x00000023, 0x00000000, 0x00050048, 0x0000000e, 0x00000000,
		0x00000007, 0x00000010, 0x00040048, 0x0000000e, 0x00000001, 0x00000005, 0x00050048, 0x0000000e,
		0x00000001, 0x00000023, 0x00000040, 0x00050048, 0x0000000e, 0x00000001, 0x00000007, 0x00000010,
		0x00030047, 0x0000000e, 0x00000002, 0x00040047, 0x00000010, 0x00000022, 0x00000000, 0x00040047,
		0x00000010, 0x00000021, 0x00000000, 0x00040047, 0x00000018, 0x0000001e, 0x00000002, 0x00040047,
		0x0000001c, 0x0000001e, 0x00000000, 0x00030047, 0x0000002a, 0x00000000, 0x00040047, 0x0000002a,
		0x0000001e, 0x00000000, 0x00040047, 0x0000002c, 0x0000001e, 0x00000001, 0x00020013, 0x00000002,
		0x00030021, 0x00000003, 0x00000002, 0x00030016, 0x00000006, 0x00000020, 0x00040017, 0x00000007,
		0x00000006, 0x00000004, 0x0003001e, 0x00000008, 0x00000007, 0x00040020, 0x00000009, 0x00000003,
		0x00000008, 0x0004003b, 0x00000009, 0x0000000a, 0x00000003, 0x00040015, 0x0000000b, 0x00000020,
		0x00000001, 0x0004002b, 0x0000000b, 0x0000000c, 0x00000000, 0x00040018, 0x0000000d, 0x00000007,
		0x00000004, 0x0004001e, 0x0000000e, 0x0000000d, 0x0000000d, 0x00040020, 0x0000000f, 0x00000002,
		0x0000000e, 0x0004003b, 0x0000000f, 0x00000010, 0x00000002, 0x0004002b, 0x0000000b, 0x00000011,
		0x00000001, 0x00040020, 0x00000012, 0x00000002, 0x0000000d, 0x00040020, 0x00000017, 0x00000001,
		0x0000000d, 0x0004003b, 0x00000017, 0x00000018, 0x00000001, 0x00040017, 0x0000001a, 0x00000006,
		0x00000003, 0x00040020, 0x0000001b, 0x00000001, 0x0000001a, 0x0004003b, 0x0000001b, 0x0000001c,
		0x00000001, 0x0004002b, 0x00000006, 0x0000001e, 0x3dcccccd, 0x0004002b, 0x00000006, 0x00000020,
		0x3f800000, 0x00040020, 0x00000028, 0x00000003, 0x00000007, 0x0004003b, 0x00000028, 0x0000002a,
		0x00000003, 0x00040020, 0x0000002b, 0x00000001, 0x00000007, 0x0004003b, 0x0000002b, 0x0000002c,
		0x00000001, 0x00050036, 0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200f8, 0x00000005,
		0x00050041, 0x00000012, 0x00000013, 0x00000010, 0x00000011, 0x0004003d, 0x0000000d, 0x00000014,
		0x00000013, 0x00050041, 0x00000012, 0x00000015, 0x00000010, 0x0000000c, 0x0004003d, 0x0000000d,
		0x00000016, 0x00000015, 0x0004003d, 0x0000000d, 0x00000019, 0x00000018, 0x0004003d, 0x0000001a,
		0x0000001d, 0x0000001c, 0x0005008e, 0x0000001a, 0x0000001f, 0x0000001d, 0x0000001e, 0x00050051,
		0x00000006, 0x00000021, 0x0000001f, 0x00000000, 0x00050051, 0x00000006, 0x00000022, 0x0000001f,
		0x00000001, 0x00050051, 0x00000006, 0x00000023, 0x0000001f, 0x00000002, 0x00070050, 0x00000007,
		0x00000024, 0x00000021, 0x00000022, 0x00000023, 0x00000020, 0x00050091, 0x00000007, 0x00000025,
		0x00000019, 0x00000024, 0x00050091, 0x00000007, 0x00000026, 0x00000016, 0x00000025, 0x00050091,
		0x00000007, 0x00000027, 0x00000014, 0x00000026, 0x00050041, 0x00000028, 0x00000029, 0x0000000a,
		0x0000000c, 0x0003003e, 0x00000029, 0x00000027, 0x0004003d, 0x00000007, 0x0000002d, 0x0000002c,
		0x0003003e, 0x0000002a, 0x0000002d, 0x000100fd, 0x00010038
};

static const char colorOnlyMultiviewVertexProgramGLSL[] =
		"#version " GLSL_VERSION "\n"
		GLSL_EXTENSIONS
		"#extension GL_multiview2 : enable\n"
		"layout( std140, binding = 0 ) uniform SceneMatrices\n"
		"{\n"
		"	layout( offset =   0 ) mat4 ViewMatrix[2];\n"
		"	layout( offset =  128 ) mat4 ProjectionMatrix[2];\n"
		"};\n"
		"layout( location = 0 ) in vec3 vertexPosition;\n"
		"layout( location = 1 ) in vec4 vertexColor;\n"
		"layout( location = 2 ) in mat4 vertexTransform;\n"
		"layout( location = 0 ) out lowp vec4 fragmentColor;\n"
		"out gl_PerVertex { vec4 gl_Position; };\n"
		"void main( void )\n"
		"{\n"
		"	gl_Position = ProjectionMatrix[gl_ViewID_OVR] * ( ViewMatrix[gl_ViewID_OVR] * ( vertexTransform * vec4( vertexPosition * 0.1, 1.0 ) ) );\n"
		"	fragmentColor = vertexColor;\n"
		"}\n";

static const unsigned int colorOnlyMultiviewVertexProgramSPIRV[] = {
		// 7.11.3057
		0x07230203, 0x00010000, 0x00080007, 0x00000036, 0x00000000, 0x00020011, 0x00000001, 0x00020011,
		0x00001157, 0x0006000a, 0x5f565053, 0x5f52484b, 0x746c756d, 0x65697669, 0x00000077, 0x0006000b,
		0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
		0x000b000f, 0x00000000, 0x00000004, 0x6e69616d, 0x00000000, 0x0000000a, 0x00000017, 0x00000020,
		0x00000024, 0x00000032, 0x00000034, 0x00030003, 0x00000002, 0x000001b8, 0x00070004, 0x415f4c47,
		0x655f4252, 0x6e61686e, 0x5f646563, 0x6f79616c, 0x00737475, 0x00070004, 0x455f4c47, 0x735f5458,
		0x65646168, 0x6f695f72, 0x6f6c625f, 0x00736b63, 0x00060004, 0x4f5f4c47, 0x6d5f5256, 0x69746c75,
		0x77656976, 0x00000032, 0x00040005, 0x00000004, 0x6e69616d, 0x00000000, 0x00060005, 0x00000008,
		0x505f6c67, 0x65567265, 0x78657472, 0x00000000, 0x00060006, 0x00000008, 0x00000000, 0x505f6c67,
		0x7469736f, 0x006e6f69, 0x00030005, 0x0000000a, 0x00000000, 0x00060005, 0x00000012, 0x6e656353,
		0x74614d65, 0x65636972, 0x00000073, 0x00060006, 0x00000012, 0x00000000, 0x77656956, 0x7274614d,
		0x00007869, 0x00080006, 0x00000012, 0x00000001, 0x6a6f7250, 0x69746365, 0x614d6e6f, 0x78697274,
		0x00000000, 0x00030005, 0x00000014, 0x00000000, 0x00060005, 0x00000017, 0x565f6c67, 0x49776569,
		0x564f5f44, 0x00000052, 0x00060005, 0x00000020, 0x74726576, 0x72547865, 0x66736e61, 0x006d726f,
		0x00060005, 0x00000024, 0x74726576, 0x6f507865, 0x69746973, 0x00006e6f, 0x00060005, 0x00000032,
		0x67617266, 0x746e656d, 0x6f6c6f43, 0x00000072, 0x00050005, 0x00000034, 0x74726576, 0x6f437865,
		0x00726f6c, 0x00050048, 0x00000008, 0x00000000, 0x0000000b, 0x00000000, 0x00030047, 0x00000008,
		0x00000002, 0x00040047, 0x00000010, 0x00000006, 0x00000040, 0x00040047, 0x00000011, 0x00000006,
		0x00000040, 0x00040048, 0x00000012, 0x00000000, 0x00000005, 0x00050048, 0x00000012, 0x00000000,
		0x00000023, 0x00000000, 0x00050048, 0x00000012, 0x00000000, 0x00000007, 0x00000010, 0x00040048,
		0x00000012, 0x00000001, 0x00000005, 0x00050048, 0x00000012, 0x00000001, 0x00000023, 0x00000080,
		0x00050048, 0x00000012, 0x00000001, 0x00000007, 0x00000010, 0x00030047, 0x00000012, 0x00000002,
		0x00040047, 0x00000014, 0x00000022, 0x00000000, 0x00040047, 0x00000014, 0x00000021, 0x00000000,
		0x00040047, 0x00000017, 0x0000000b, 0x00001158, 0x00040047, 0x00000020, 0x0000001e, 0x00000002,
		0x00040047, 0x00000024, 0x0000001e, 0x00000000, 0x00030047, 0x00000032, 0x00000000, 0x00040047,
		0x00000032, 0x0000001e, 0x00000000, 0x00040047, 0x00000034, 0x0000001e, 0x00000001, 0x00020013,
		0x00000002, 0x00030021, 0x00000003, 0x00000002, 0x00030016, 0x00000006, 0x00000020, 0x00040017,
		0x00000007, 0x00000006, 0x00000004, 0x0003001e, 0x00000008, 0x00000007, 0x00040020, 0x00000009,
		0x00000003, 0x00000008, 0x0004003b, 0x00000009, 0x0000000a, 0x00000003, 0x00040015, 0x0000000b,
		0x00000020, 0x00000001, 0x0004002b, 0x0000000b, 0x0000000c, 0x00000000, 0x00040018, 0x0000000d,
		0x00000007, 0x00000004, 0x00040015, 0x0000000e, 0x00000020, 0x00000000, 0x0004002b, 0x0000000e,
		0x0000000f, 0x00000002, 0x0004001c, 0x00000010, 0x0000000d, 0x0000000f, 0x0004001c, 0x00000011,
		0x0000000d, 0x0000000f, 0x0004001e, 0x00000012, 0x00000010, 0x00000011, 0x00040020, 0x00000013,
		0x00000002, 0x00000012, 0x0004003b, 0x00000013, 0x00000014, 0x00000002, 0x0004002b, 0x0000000b,
		0x00000015, 0x00000001, 0x00040020, 0x00000016, 0x00000001, 0x0000000e, 0x0004003b, 0x00000016,
		0x00000017, 0x00000001, 0x00040020, 0x00000019, 0x00000002, 0x0000000d, 0x00040020, 0x0000001f,
		0x00000001, 0x0000000d, 0x0004003b, 0x0000001f, 0x00000020, 0x00000001, 0x00040017, 0x00000022,
		0x00000006, 0x00000003, 0x00040020, 0x00000023, 0x00000001, 0x00000022, 0x0004003b, 0x00000023,
		0x00000024, 0x00000001, 0x0004002b, 0x00000006, 0x00000026, 0x3dcccccd, 0x0004002b, 0x00000006,
		0x00000028, 0x3f800000, 0x00040020, 0x00000030, 0x00000003, 0x00000007, 0x0004003b, 0x00000030,
		0x00000032, 0x00000003, 0x00040020, 0x00000033, 0x00000001, 0x00000007, 0x0004003b, 0x00000033,
		0x00000034, 0x00000001, 0x00050036, 0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200f8,
		0x00000005, 0x0004003d, 0x0000000e, 0x00000018, 0x00000017, 0x00060041, 0x00000019, 0x0000001a,
		0x00000014, 0x00000015, 0x00000018, 0x0004003d, 0x0000000d, 0x0000001b, 0x0000001a, 0x0004003d,
		0x0000000e, 0x0000001c, 0x00000017, 0x00060041, 0x00000019, 0x0000001d, 0x00000014, 0x0000000c,
		0x0000001c, 0x0004003d, 0x0000000d, 0x0000001e, 0x0000001d, 0x0004003d, 0x0000000d, 0x00000021,
		0x00000020, 0x0004003d, 0x00000022, 0x00000025, 0x00000024, 0x0005008e, 0x00000022, 0x00000027,
		0x00000025, 0x00000026, 0x00050051, 0x00000006, 0x00000029, 0x00000027, 0x00000000, 0x00050051,
		0x00000006, 0x0000002a, 0x00000027, 0x00000001, 0x00050051, 0x00000006, 0x0000002b, 0x00000027,
		0x00000002, 0x00070050, 0x00000007, 0x0000002c, 0x00000029, 0x0000002a, 0x0000002b, 0x00000028,
		0x00050091, 0x00000007, 0x0000002d, 0x00000021, 0x0000002c, 0x00050091, 0x00000007, 0x0000002e,
		0x0000001e, 0x0000002d, 0x00050091, 0x00000007, 0x0000002f, 0x0000001b, 0x0000002e, 0x00050041,
		0x00000030, 0x00000031, 0x0000000a, 0x0000000c, 0x0003003e, 0x00000031, 0x0000002f, 0x0004003d,
		0x00000007, 0x00000035, 0x00000034, 0x0003003e, 0x00000032, 0x00000035, 0x000100fd, 0x00010038
};

static const char colorOnlyFragmentProgramGLSL[] =
		"#version " GLSL_VERSION "\n"
		GLSL_EXTENSIONS
		"layout( location = 0 ) in lowp vec4 fragmentColor;\n"
		"layout( location = 0 ) out lowp vec4 outColor;\n"
		"void main()\n"
		"{\n"
		"	outColor = fragmentColor;\n"
		"}\n";

static const unsigned int colorOnlyFragmentProgramSPIRV[] = {
		// 7.11.3057
		0x07230203, 0x00010000, 0x00080007, 0x0000000d, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
		0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
		0x0007000f, 0x00000004, 0x00000004, 0x6e69616d, 0x00000000, 0x00000009, 0x0000000b, 0x00030010,
		0x00000004, 0x00000007, 0x00030003, 0x00000002, 0x000001b8, 0x00070004, 0x415f4c47, 0x655f4252,
		0x6e61686e, 0x5f646563, 0x6f79616c, 0x00737475, 0x00070004, 0x455f4c47, 0x735f5458, 0x65646168,
		0x6f695f72, 0x6f6c625f, 0x00736b63, 0x00040005, 0x00000004, 0x6e69616d, 0x00000000, 0x00050005,
		0x00000009, 0x4374756f, 0x726f6c6f, 0x00000000, 0x00060005, 0x0000000b, 0x67617266, 0x746e656d,
		0x6f6c6f43, 0x00000072, 0x00030047, 0x00000009, 0x00000000, 0x00040047, 0x00000009, 0x0000001e,
		0x00000000, 0x00030047, 0x0000000b, 0x00000000, 0x00040047, 0x0000000b, 0x0000001e, 0x00000000,
		0x00030047, 0x0000000c, 0x00000000, 0x00020013, 0x00000002, 0x00030021, 0x00000003, 0x00000002,
		0x00030016, 0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000004, 0x00040020,
		0x00000008, 0x00000003, 0x00000007, 0x0004003b, 0x00000008, 0x00000009, 0x00000003, 0x00040020,
		0x0000000a, 0x00000001, 0x00000007, 0x0004003b, 0x0000000a, 0x0000000b, 0x00000001, 0x00050036,
		0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200f8, 0x00000005, 0x0004003d, 0x00000007,
		0x0000000c, 0x0000000b, 0x0003003e, 0x00000009, 0x0000000c, 0x000100fd, 0x00010038
};
// clang-format on

struct ColorSwapChain
{
	int SwapChainLength;
	ovrTextureSwapChain *SwapChain;
	std::vector<VkImage> ColorTextures;
	std::vector<VkImage> FragmentDensityTextures;
	std::vector<VkExtent2D> FragmentDensityTextureSizes;
};

struct Framebuffer
{
	int Width;
	int Height;
	VkSampleCountFlagBits SampleCount;
	int TextureSwapChainLength;
	int TextureSwapChainIndex;
	ovrTextureSwapChain *ColorTextureSwapChain;
	FramebufferTextures Framebuffer;
};

#define NUM_INSTANCES 1500
#define NUM_ROTATIONS 16

struct Scene
{
	bool CreatedScene;
	unsigned int Random;
	GraphicsProgram Program;
	Geometry Cube;
	GraphicsPipeline Pipeline;
	Buffer SceneMatrices;
	int NumViews;
	ovrVector3f Rotations[NUM_ROTATIONS];
	ovrVector3f CubePositions[NUM_INSTANCES];
	int CubeRotations[NUM_INSTANCES];
};

float randomFloat(Scene& scene)
{
	scene.Random = 1664525L * scene.Random + 1013904223L;
	unsigned int rf = 0x3F800000 | (scene.Random & 0x007FFFFF);
	return (*(float *) &rf) - 1.0f;
}

void initVertexAttributes(const bool instance, const VertexAttribute *vertexLayout, const int numAttribs, const int storedAttribsFlags, const int usedAttribsFlags, GraphicsPipeline& pipeline)
{
	size_t offset = 0;
	for (int i = 0; vertexLayout[i].attributeFlag != 0; i++)
	{
		const VertexAttribute *v = &vertexLayout[i];
		if ((v->attributeFlag & storedAttribsFlags) != 0)
		{
			if ((v->attributeFlag & usedAttribsFlags) != 0)
			{
				for (int location = 0; location < v->locationCount; location++)
				{
					pipeline.vertexAttributes[pipeline.vertexAttributeCount + location].location = pipeline.vertexAttributeCount + location;
					pipeline.vertexAttributes[pipeline.vertexAttributeCount + location].binding = pipeline.vertexBindingCount;
					pipeline.vertexAttributes[pipeline.vertexAttributeCount + location].format = (VkFormat) v->attributeFormat;
					pipeline.vertexAttributes[pipeline.vertexAttributeCount + location].offset = (uint32_t) (location * v->attributeSize / v->locationCount);
				}

				pipeline.vertexBindings[pipeline.vertexBindingCount].binding = pipeline.vertexBindingCount;
				pipeline.vertexBindings[pipeline.vertexBindingCount].stride = (uint32_t) v->attributeSize;
				pipeline.vertexBindings[pipeline.vertexBindingCount].inputRate = instance ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;

				pipeline.vertexBindingOffsets[pipeline.vertexBindingCount] = (VkDeviceSize)offset;

				pipeline.vertexAttributeCount += v->locationCount;
				pipeline.vertexBindingCount += 1;
			}
			offset += numAttribs * v->attributeSize;
		}
	}
}

struct Simulation
{
	ovrVector3f CurrentRotation;
};

struct Renderer
{
	RenderPass RenderPassSingleView;
	CommandBuffer EyeCommandBuffer[VRAPI_FRAME_LAYER_EYE_MAX];
	Framebuffer Framebuffer[VRAPI_FRAME_LAYER_EYE_MAX];
	ovrMatrix4f ViewMatrix[VRAPI_FRAME_LAYER_EYE_MAX];
	ovrMatrix4f ProjectionMatrix[VRAPI_FRAME_LAYER_EYE_MAX];
	int NumEyes;
};

enum AppMode
{
	AppStartupMode,
	AppCylinderMode,
	AppProjectionMode
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
	Scene Scene;
	Simulation Simulation;
	long long FrameIndex;
	double DisplayTime;
	int SwapInterval;
	int CpuLevel;
	int GpuLevel;
	int MainThreadTid;
	int RenderThreadTid;
	Renderer Renderer;
	ovrTextureSwapChain* CylinderSwapChain;
	int CylinderWidth;
	int CylinderHeight;
	std::vector<uint32_t> CylinderTexData;
	VkImage CylinderTexImage;
	Buffer CylinderTexBuffer;
	VkCommandBuffer CylinderCommandBuffer;
	bool FirstFrame;
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
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main: vrapi_Initialize() failed: %i", initResult);
		exit(0);
	}
	AppState appState { };
	appState.FrameIndex = 1;
	appState.SwapInterval = 1;
	appState.CpuLevel = 2;
	appState.GpuLevel = 2;
	appState.Scene.Random = 2;
	appState.Scene.NumViews = 1;
	for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++)
	{
		appState.Renderer.Framebuffer[eye].SampleCount = VK_SAMPLE_COUNT_1_BIT;
	}
	for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++)
	{
		appState.Renderer.ViewMatrix[eye] = ovrMatrix4f_CreateIdentity();
		appState.Renderer.ProjectionMatrix[eye] = ovrMatrix4f_CreateIdentity();
	}
	appState.Renderer.NumEyes = VRAPI_FRAME_LAYER_EYE_MAX;
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
	instance.loader = dlopen(VULKAN_LIBRARY, RTLD_NOW | RTLD_LOCAL);
	if (instance.loader == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): %s not available: %s", VULKAN_LIBRARY, dlerror());
		vrapi_Shutdown();
		exit(0);
	}
	instance.vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) dlsym(instance.loader, "vkGetInstanceProcAddr");
	instance.vkEnumerateInstanceLayerProperties = (PFN_vkEnumerateInstanceLayerProperties) dlsym(instance.loader, "vkEnumerateInstanceLayerProperties");
	instance.vkEnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties) dlsym(instance.loader, "vkEnumerateInstanceExtensionProperties");
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
					"VK_LAYER_LUNARG_api_dump",
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
	appState.Device.instance = &instance;
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
			if ((queueFamilyProperties[queueFamilyIndex].queueFlags & requiredQueueFlags) == requiredQueueFlags)
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
	appState.Device.vkDestroyDevice = (PFN_vkDestroyDevice) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkDestroyDevice"));
	if (appState.Device.vkDestroyDevice == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyDevice.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkGetDeviceQueue = (PFN_vkGetDeviceQueue) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkGetDeviceQueue"));
	if (appState.Device.vkGetDeviceQueue == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkGetDeviceQueue.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkQueueSubmit = (PFN_vkQueueSubmit) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkQueueSubmit"));
	if (appState.Device.vkQueueSubmit == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkQueueSubmit.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkQueueWaitIdle = (PFN_vkQueueWaitIdle) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkQueueWaitIdle"));
	if (appState.Device.vkQueueWaitIdle == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkQueueWaitIdle.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDeviceWaitIdle = (PFN_vkDeviceWaitIdle) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkDeviceWaitIdle"));
	if (appState.Device.vkDeviceWaitIdle == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDeviceWaitIdle.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkAllocateMemory = (PFN_vkAllocateMemory) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkAllocateMemory"));
	if (appState.Device.vkAllocateMemory == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkAllocateMemory.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkFreeMemory = (PFN_vkFreeMemory) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkFreeMemory"));
	if (appState.Device.vkFreeMemory == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkFreeMemory.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkMapMemory = (PFN_vkMapMemory) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkMapMemory"));
	if (appState.Device.vkMapMemory == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkMapMemory.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkUnmapMemory = (PFN_vkUnmapMemory) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkUnmapMemory"));
	if (appState.Device.vkUnmapMemory == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkUnmapMemory.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkFlushMappedMemoryRanges = (PFN_vkFlushMappedMemoryRanges) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkFlushMappedMemoryRanges"));
	if (appState.Device.vkFlushMappedMemoryRanges == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkFlushMappedMemoryRanges.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkBindBufferMemory = (PFN_vkBindBufferMemory) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkBindBufferMemory"));
	if (appState.Device.vkBindBufferMemory == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkBindBufferMemory.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkBindImageMemory = (PFN_vkBindImageMemory) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkBindImageMemory"));
	if (appState.Device.vkBindImageMemory == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkBindImageMemory.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkGetBufferMemoryRequirements = (PFN_vkGetBufferMemoryRequirements) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkGetBufferMemoryRequirements"));
	if (appState.Device.vkGetBufferMemoryRequirements == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkGetBufferMemoryRequirements.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkGetImageMemoryRequirements = (PFN_vkGetImageMemoryRequirements) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkGetImageMemoryRequirements"));
	if (appState.Device.vkGetImageMemoryRequirements == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkGetImageMemoryRequirements.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreateFence = (PFN_vkCreateFence) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCreateFence"));
	if (appState.Device.vkCreateFence == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreateFence.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroyFence = (PFN_vkDestroyFence) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkDestroyFence"));
	if (appState.Device.vkDestroyFence == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyFence.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkResetFences = (PFN_vkResetFences) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkResetFences"));
	if (appState.Device.vkResetFences == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkResetFences.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkGetFenceStatus = (PFN_vkGetFenceStatus) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkGetFenceStatus"));
	if (appState.Device.vkGetFenceStatus == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkGetFenceStatus.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkWaitForFences = (PFN_vkWaitForFences) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkWaitForFences"));
	if (appState.Device.vkWaitForFences == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkWaitForFences.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreateBuffer = (PFN_vkCreateBuffer) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCreateBuffer"));
	if (appState.Device.vkCreateBuffer == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreateBuffer.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroyBuffer = (PFN_vkDestroyBuffer) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkDestroyBuffer"));
	if (appState.Device.vkDestroyBuffer == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyBuffer.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreateImage = (PFN_vkCreateImage) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCreateImage"));
	if (appState.Device.vkCreateImage == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreateImage.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroyImage = (PFN_vkDestroyImage) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkDestroyImage"));
	if (appState.Device.vkDestroyImage == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyImage.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreateImageView = (PFN_vkCreateImageView) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCreateImageView"));
	if (appState.Device.vkCreateImageView == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreateImageView.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroyImageView = (PFN_vkDestroyImageView) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkDestroyImageView"));
	if (appState.Device.vkDestroyImageView == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyImageView.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreateShaderModule = (PFN_vkCreateShaderModule) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCreateShaderModule"));
	if (appState.Device.vkCreateShaderModule == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreateShaderModule.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroyShaderModule = (PFN_vkDestroyShaderModule) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkDestroyShaderModule"));
	if (appState.Device.vkDestroyShaderModule == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyShaderModule.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreatePipelineCache = (PFN_vkCreatePipelineCache) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCreatePipelineCache"));
	if (appState.Device.vkCreatePipelineCache == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreatePipelineCache.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroyPipelineCache = (PFN_vkDestroyPipelineCache) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkDestroyPipelineCache"));
	if (appState.Device.vkDestroyPipelineCache == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyPipelineCache.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreateGraphicsPipelines = (PFN_vkCreateGraphicsPipelines) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCreateGraphicsPipelines"));
	if (appState.Device.vkCreateGraphicsPipelines == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreateGraphicsPipelines.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroyPipeline = (PFN_vkDestroyPipeline) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkDestroyPipeline"));
	if (appState.Device.vkDestroyPipeline == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyPipeline.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreatePipelineLayout = (PFN_vkCreatePipelineLayout) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCreatePipelineLayout"));
	if (appState.Device.vkCreatePipelineLayout == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreatePipelineLayout.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroyPipelineLayout = (PFN_vkDestroyPipelineLayout) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkDestroyPipelineLayout"));
	if (appState.Device.vkDestroyPipelineLayout == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyPipelineLayout.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreateSampler = (PFN_vkCreateSampler) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCreateSampler"));
	if (appState.Device.vkCreateSampler == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreateSampler.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroySampler = (PFN_vkDestroySampler) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkDestroySampler"));
	if (appState.Device.vkDestroySampler == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroySampler.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreateDescriptorSetLayout = (PFN_vkCreateDescriptorSetLayout) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCreateDescriptorSetLayout"));
	if (appState.Device.vkCreateDescriptorSetLayout == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreateDescriptorSetLayout.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroyDescriptorSetLayout = (PFN_vkDestroyDescriptorSetLayout) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkDestroyDescriptorSetLayout"));
	if (appState.Device.vkDestroyDescriptorSetLayout == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyDescriptorSetLayout.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreateDescriptorPool = (PFN_vkCreateDescriptorPool) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCreateDescriptorPool"));
	if (appState.Device.vkCreateDescriptorPool == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreateDescriptorPool.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroyDescriptorPool = (PFN_vkDestroyDescriptorPool) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkDestroyDescriptorPool"));
	if (appState.Device.vkDestroyDescriptorPool == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyDescriptorPool.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkResetDescriptorPool = (PFN_vkResetDescriptorPool) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkResetDescriptorPool"));
	if (appState.Device.vkResetDescriptorPool == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkResetDescriptorPool.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkAllocateDescriptorSets = (PFN_vkAllocateDescriptorSets) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkAllocateDescriptorSets"));
	if (appState.Device.vkAllocateDescriptorSets == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkAllocateDescriptorSets.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkFreeDescriptorSets = (PFN_vkFreeDescriptorSets) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkFreeDescriptorSets"));
	if (appState.Device.vkFreeDescriptorSets == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkFreeDescriptorSets.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkUpdateDescriptorSets = (PFN_vkUpdateDescriptorSets) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkUpdateDescriptorSets"));
	if (appState.Device.vkUpdateDescriptorSets == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkUpdateDescriptorSets.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreateFramebuffer = (PFN_vkCreateFramebuffer) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCreateFramebuffer"));
	if (appState.Device.vkCreateFramebuffer == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreateFramebuffer.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroyFramebuffer = (PFN_vkDestroyFramebuffer) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkDestroyFramebuffer"));
	if (appState.Device.vkDestroyFramebuffer == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyFramebuffer.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreateRenderPass = (PFN_vkCreateRenderPass) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCreateRenderPass"));
	if (appState.Device.vkCreateRenderPass == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreateRenderPass.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroyRenderPass = (PFN_vkDestroyRenderPass) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkDestroyRenderPass"));
	if (appState.Device.vkDestroyRenderPass == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyRenderPass.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCreateCommandPool = (PFN_vkCreateCommandPool) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCreateCommandPool"));
	if (appState.Device.vkCreateCommandPool == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCreateCommandPool.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkDestroyCommandPool = (PFN_vkDestroyCommandPool) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkDestroyCommandPool"));
	if (appState.Device.vkDestroyCommandPool == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkDestroyCommandPool.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkResetCommandPool = (PFN_vkResetCommandPool) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkResetCommandPool"));
	if (appState.Device.vkResetCommandPool == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkResetCommandPool.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkAllocateCommandBuffers = (PFN_vkAllocateCommandBuffers) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkAllocateCommandBuffers"));
	if (appState.Device.vkAllocateCommandBuffers == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkAllocateCommandBuffers.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkFreeCommandBuffers = (PFN_vkFreeCommandBuffers) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkFreeCommandBuffers"));
	if (appState.Device.vkFreeCommandBuffers == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkFreeCommandBuffers.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkBeginCommandBuffer = (PFN_vkBeginCommandBuffer) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkBeginCommandBuffer"));
	if (appState.Device.vkBeginCommandBuffer == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkBeginCommandBuffer.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkEndCommandBuffer = (PFN_vkEndCommandBuffer) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkEndCommandBuffer"));
	if (appState.Device.vkEndCommandBuffer == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkEndCommandBuffer.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkResetCommandBuffer = (PFN_vkResetCommandBuffer) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkResetCommandBuffer"));
	if (appState.Device.vkResetCommandBuffer == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkResetCommandBuffer.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdBindPipeline = (PFN_vkCmdBindPipeline) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCmdBindPipeline"));
	if (appState.Device.vkCmdBindPipeline == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdBindPipeline.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdSetViewport = (PFN_vkCmdSetViewport) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCmdSetViewport"));
	if (appState.Device.vkCmdSetViewport == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdSetViewport.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdSetScissor = (PFN_vkCmdSetScissor) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCmdSetScissor"));
	if (appState.Device.vkCmdSetScissor == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdSetScissor.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdBindDescriptorSets = (PFN_vkCmdBindDescriptorSets) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCmdBindDescriptorSets"));
	if (appState.Device.vkCmdBindDescriptorSets == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdBindDescriptorSets.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdBindIndexBuffer = (PFN_vkCmdBindIndexBuffer) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCmdBindIndexBuffer"));
	if (appState.Device.vkCmdBindIndexBuffer == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdBindIndexBuffer.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdBindVertexBuffers = (PFN_vkCmdBindVertexBuffers) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCmdBindVertexBuffers"));
	if (appState.Device.vkCmdBindVertexBuffers == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdBindVertexBuffers.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdDrawIndexed = (PFN_vkCmdDrawIndexed) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCmdDrawIndexed"));
	if (appState.Device.vkCmdDrawIndexed == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdDrawIndexed.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdCopyBuffer = (PFN_vkCmdCopyBuffer) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCmdCopyBuffer"));
	if (appState.Device.vkCmdCopyBuffer == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdCopyBuffer.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdCopyBufferToImage = (PFN_vkCmdCopyBufferToImage) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCmdCopyBufferToImage"));
	if (appState.Device.vkCmdCopyBufferToImage == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdCopyBufferToImage.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdResolveImage = (PFN_vkCmdResolveImage) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCmdResolveImage"));
	if (appState.Device.vkCmdResolveImage == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdResolveImage.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdPipelineBarrier = (PFN_vkCmdPipelineBarrier) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCmdPipelineBarrier"));
	if (appState.Device.vkCmdPipelineBarrier == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdPipelineBarrier.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdPushConstants = (PFN_vkCmdPushConstants) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCmdPushConstants"));
	if (appState.Device.vkCmdPushConstants == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdPushConstants.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdBeginRenderPass = (PFN_vkCmdBeginRenderPass) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCmdBeginRenderPass"));
	if (appState.Device.vkCmdBeginRenderPass == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): vkGetDeviceProcAddr() could not find vkCmdBeginRenderPass.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Device.vkCmdEndRenderPass = (PFN_vkCmdEndRenderPass) (appState.Device.instance->vkGetDeviceProcAddr(appState.Device.device, "vkCmdEndRenderPass"));
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
	auto isMultiview = appState.Context.device->supportsMultiview;
	auto useFragmentDensity = appState.Context.device->supportsFragmentDensity;
	appState.Renderer.NumEyes = isMultiview ? 1 : VRAPI_FRAME_LAYER_EYE_MAX;
	auto eyeTextureWidth = vrapi_GetSystemPropertyInt(&appState.Java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH);
	auto eyeTextureHeight = vrapi_GetSystemPropertyInt(&appState.Java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT);
	std::vector<ColorSwapChain> colorSwapChains(appState.Renderer.NumEyes);
	for (auto &colorSwapChain : colorSwapChains)
	{
		colorSwapChain.SwapChain = vrapi_CreateTextureSwapChain3(isMultiview ? VRAPI_TEXTURE_TYPE_2D_ARRAY : VRAPI_TEXTURE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, eyeTextureWidth, eyeTextureHeight, 1, 3);
		colorSwapChain.SwapChainLength = vrapi_GetTextureSwapChainLength(colorSwapChain.SwapChain);
		colorSwapChain.ColorTextures.resize(colorSwapChain.SwapChainLength);
		colorSwapChain.FragmentDensityTextures.resize(colorSwapChain.SwapChainLength);
		colorSwapChain.FragmentDensityTextureSizes.resize(colorSwapChain.SwapChainLength);
		for (auto i = 0; i < colorSwapChain.SwapChainLength; i++)
		{
			colorSwapChain.ColorTextures[i] = vrapi_GetTextureSwapChainBufferVulkan(colorSwapChain.SwapChain, i);
			if (colorSwapChain.FragmentDensityTextures.size() > 0)
			{
				auto result = vrapi_GetTextureSwapChainBufferFoveationVulkan(colorSwapChain.SwapChain, i, &colorSwapChain.FragmentDensityTextures[i], &colorSwapChain.FragmentDensityTextureSizes[i].width, &colorSwapChain.FragmentDensityTextureSizes[i].height);
				if (result != ovrSuccess)
				{
					colorSwapChain.FragmentDensityTextures.clear();
					colorSwapChain.FragmentDensityTextureSizes.clear();
				}
			}
		}
		useFragmentDensity = useFragmentDensity && (colorSwapChain.FragmentDensityTextures.size() > 0);
	}
	if ((appState.Context.device->physicalDeviceProperties.properties.limits.framebufferColorSampleCounts & SAMPLE_COUNT) == 0)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Requested color buffer sample count not available.");
		vrapi_Shutdown();
		exit(0);
	}
	if ((appState.Context.device->physicalDeviceProperties.properties.limits.framebufferDepthSampleCounts & SAMPLE_COUNT) == 0)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Requested color buffer sample count not available.");
		vrapi_Shutdown();
		exit(0);
	}
	appState.Renderer.RenderPassSingleView.type = VK_SUBPASS_CONTENTS_INLINE;
	appState.Renderer.RenderPassSingleView.useFragmentDensity = useFragmentDensity;
	appState.Renderer.RenderPassSingleView.depthFormat = SURFACE_DEPTH_FORMAT_D24;
	appState.Renderer.RenderPassSingleView.sampleCount = SAMPLE_COUNT;
	appState.Renderer.RenderPassSingleView.internalColorFormat = VK_FORMAT_R8G8B8A8_UNORM;
	appState.Renderer.RenderPassSingleView.internalDepthFormat = VK_FORMAT_D24_UNORM_S8_UINT;
	appState.Renderer.RenderPassSingleView.internalFragmentDensityFormat = VK_FORMAT_R8G8_UNORM;
	appState.Renderer.RenderPassSingleView.clearColor = {0.125f, 0.0f, 0.125f, 1.0f};
	uint32_t attachmentCount = 0;
	VkAttachmentDescription attachments[4];
	attachments[attachmentCount].flags = 0;
	attachments[attachmentCount].format = appState.Renderer.RenderPassSingleView.internalColorFormat;
	attachments[attachmentCount].samples = SAMPLE_COUNT;
	attachments[attachmentCount].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[attachmentCount].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[attachmentCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[attachmentCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[attachmentCount].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachments[attachmentCount].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachmentCount++;
	attachments[attachmentCount].flags = 0;
	attachments[attachmentCount].format = appState.Renderer.RenderPassSingleView.internalColorFormat;
	attachments[attachmentCount].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[attachmentCount].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[attachmentCount].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[attachmentCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[attachmentCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[attachmentCount].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachments[attachmentCount].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachmentCount++;
	if (appState.Renderer.RenderPassSingleView.internalDepthFormat != VK_FORMAT_UNDEFINED)
	{
		attachments[attachmentCount].flags = 0;
		attachments[attachmentCount].format = appState.Renderer.RenderPassSingleView.internalDepthFormat;
		attachments[attachmentCount].samples = SAMPLE_COUNT;
		attachments[attachmentCount].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[attachmentCount].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[attachmentCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[attachmentCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[attachmentCount].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[attachmentCount].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachmentCount++;
	}
	uint32_t sampleMapAttachment = 0;
	if (appState.Renderer.RenderPassSingleView.useFragmentDensity)
	{
		sampleMapAttachment = attachmentCount;
		attachments[attachmentCount].flags = 0;
		attachments[attachmentCount].format = appState.Renderer.RenderPassSingleView.internalFragmentDensityFormat;
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
	if (appState.Renderer.RenderPassSingleView.useFragmentDensity)
	{
		fragmentDensityAttachmentReference.attachment = sampleMapAttachment;
		fragmentDensityAttachmentReference.layout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
	}
	VkSubpassDescription subpassDescription { };
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorAttachmentReference;
	subpassDescription.pResolveAttachments = &resolveAttachmentReference;
	subpassDescription.pDepthStencilAttachment = (appState.Renderer.RenderPassSingleView.internalDepthFormat != VK_FORMAT_UNDEFINED) ? &depthAttachmentReference : nullptr;
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
	if (appState.Renderer.RenderPassSingleView.useFragmentDensity)
	{
		fragmentDensityMapCreateInfoEXT.sType = VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT;
		fragmentDensityMapCreateInfoEXT.fragmentDensityMapAttachment = fragmentDensityAttachmentReference;
		fragmentDensityMapCreateInfoEXT.pNext = renderPassCreateInfo.pNext;
		renderPassCreateInfo.pNext = &fragmentDensityMapCreateInfoEXT;
	}
	VK(appState.Context.device->vkCreateRenderPass(appState.Context.device->device, &renderPassCreateInfo, nullptr, &appState.Renderer.RenderPassSingleView.renderPass));
	for (auto i = 0; i < appState.Renderer.NumEyes; i++)
	{
		if (eyeTextureWidth > appState.Context.device->physicalDeviceProperties.properties.limits.maxFramebufferWidth)
		{
			__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Eye texture width exceeds the physical device's limits.");
			vrapi_Shutdown();
			exit(0);
		}
		if (eyeTextureHeight > appState.Context.device->physicalDeviceProperties.properties.limits.maxFramebufferHeight)
		{
			__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Eye texture height exceeds the physical device's limits.");
			vrapi_Shutdown();
			exit(0);
		}
		auto& framebuffer = appState.Renderer.Framebuffer[i];
		framebuffer.Width = eyeTextureWidth;
		framebuffer.Height = eyeTextureHeight;
		framebuffer.SampleCount = appState.Renderer.RenderPassSingleView.sampleCount;
		framebuffer.ColorTextureSwapChain = colorSwapChains[i].SwapChain;
		framebuffer.TextureSwapChainLength = colorSwapChains[i].SwapChainLength;
		framebuffer.Framebuffer.colorTextures.resize(framebuffer.TextureSwapChainLength);
		if (colorSwapChains[i].FragmentDensityTextures.size() > 0)
		{
			framebuffer.Framebuffer.fragmentDensityTextures.resize(framebuffer.TextureSwapChainLength);
		}
		framebuffer.Framebuffer.framebuffers.resize(framebuffer.TextureSwapChainLength);
		framebuffer.Framebuffer.renderPass = &appState.Renderer.RenderPassSingleView;
		framebuffer.Framebuffer.width = eyeTextureWidth;
		framebuffer.Framebuffer.height = eyeTextureHeight;
		framebuffer.Framebuffer.numLayers = isMultiview ? 2 : 1;
		framebuffer.Framebuffer.numBuffers = framebuffer.TextureSwapChainLength;
		framebuffer.Framebuffer.currentBuffer = 0;
		framebuffer.Framebuffer.currentLayer = 0;
		for (auto j = 0; j < framebuffer.TextureSwapChainLength; j++)
		{
			auto texture = &framebuffer.Framebuffer.colorTextures[j];
			texture->width = eyeTextureWidth;
			texture->height = eyeTextureHeight;
			texture->depth = 1;
			texture->layerCount = isMultiview ? 2 : 1;
			texture->mipCount = 1;
			texture->sampleCount = VK_SAMPLE_COUNT_1_BIT;
			texture->usage = TEXTURE_USAGE_SAMPLED;
			texture->usageFlags = TEXTURE_USAGE_COLOR_ATTACHMENT | TEXTURE_USAGE_SAMPLED | TEXTURE_USAGE_STORAGE;
			texture->wrapMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			texture->filter = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			texture->maxAnisotropy = 1.0f;
			texture->format = VK_FORMAT_R8G8B8A8_UNORM;
			texture->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			texture->image = colorSwapChains[i].ColorTextures[j];
			texture->memory = VK_NULL_HANDLE;
			texture->sampler = VK_NULL_HANDLE;
			texture->view = VK_NULL_HANDLE;
			{
				beginSetupCommandBuffer(&appState.Context);
				VkImageMemoryBarrier imageMemoryBarrier { };
				imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageMemoryBarrier.image = colorSwapChains[i].ColorTextures[j];
				imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageMemoryBarrier.subresourceRange.levelCount = 1;
				imageMemoryBarrier.subresourceRange.layerCount = texture->layerCount;
				const VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
				const VkPipelineStageFlags dst_stages = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
				const VkDependencyFlags flags = 0;
				VC(appState.Context.device->vkCmdPipelineBarrier(appState.Context.setupCommandBuffer, src_stages, dst_stages, flags, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
				flushSetupCommandBuffer(&appState.Context);
			}
			VkImageViewCreateInfo imageViewCreateInfo { };
			imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCreateInfo.image = texture->image;
			imageViewCreateInfo.viewType = isMultiview ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
			imageViewCreateInfo.format = texture->format;
			imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
			imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
			imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
			imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
			imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageViewCreateInfo.subresourceRange.levelCount = 1;
			imageViewCreateInfo.subresourceRange.layerCount = texture->layerCount;
			VK(appState.Context.device->vkCreateImageView(appState.Context.device->device, &imageViewCreateInfo, nullptr, &texture->view));
			updateTextureSampler(&appState.Context, texture);
			if (framebuffer.Framebuffer.fragmentDensityTextures.size() > 0)
			{
				auto& textureFragmentDensity = framebuffer.Framebuffer.fragmentDensityTextures[j];
				textureFragmentDensity.width = colorSwapChains[i].FragmentDensityTextureSizes[j].width;
				textureFragmentDensity.height = colorSwapChains[i].FragmentDensityTextureSizes[j].height;
				textureFragmentDensity.depth = 1;
				textureFragmentDensity.layerCount = isMultiview ? 2 : 1;
				textureFragmentDensity.mipCount = 1;
				textureFragmentDensity.sampleCount = VK_SAMPLE_COUNT_1_BIT;
				textureFragmentDensity.usage = TEXTURE_USAGE_FRAG_DENSITY;
				textureFragmentDensity.wrapMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
				textureFragmentDensity.filter = VK_SAMPLER_MIPMAP_MODE_LINEAR;
				textureFragmentDensity.maxAnisotropy = 1.0f;
				textureFragmentDensity.format = VK_FORMAT_R8G8_UNORM;
				textureFragmentDensity.imageLayout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
				textureFragmentDensity.image = colorSwapChains[i].FragmentDensityTextures[j];
				textureFragmentDensity.memory = VK_NULL_HANDLE;
				textureFragmentDensity.sampler = VK_NULL_HANDLE;
				textureFragmentDensity.view = VK_NULL_HANDLE;
				{
					beginSetupCommandBuffer(&appState.Context);
					VkImageMemoryBarrier imageMemoryBarrier { };
					imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
					imageMemoryBarrier.dstAccessMask = VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT;
					imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
					imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
					imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					imageMemoryBarrier.image = colorSwapChains[i].FragmentDensityTextures[i];
					imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					imageMemoryBarrier.subresourceRange.levelCount = 1;
					imageMemoryBarrier.subresourceRange.layerCount = textureFragmentDensity.layerCount;
					const VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
					const VkPipelineStageFlags dst_stages = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
					const VkDependencyFlags flags = 0;
					VC(appState.Context.device->vkCmdPipelineBarrier(appState.Context.setupCommandBuffer, src_stages, dst_stages, flags, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
					flushSetupCommandBuffer(&appState.Context);
				}
				VkImageViewCreateInfo imageViewCreateInfoFragmentDensity { };
				imageViewCreateInfoFragmentDensity.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				imageViewCreateInfoFragmentDensity.image = textureFragmentDensity.image;
				imageViewCreateInfoFragmentDensity.viewType = isMultiview ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
				imageViewCreateInfoFragmentDensity.format = VK_FORMAT_R8G8_UNORM;
				imageViewCreateInfoFragmentDensity.components.r = VK_COMPONENT_SWIZZLE_R;
				imageViewCreateInfoFragmentDensity.components.g = VK_COMPONENT_SWIZZLE_G;
				imageViewCreateInfoFragmentDensity.components.b = VK_COMPONENT_SWIZZLE_B;
				imageViewCreateInfoFragmentDensity.components.a = VK_COMPONENT_SWIZZLE_A;
				imageViewCreateInfoFragmentDensity.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageViewCreateInfoFragmentDensity.subresourceRange.levelCount = 1;
				imageViewCreateInfoFragmentDensity.subresourceRange.layerCount = textureFragmentDensity.layerCount;
				VK(appState.Context.device->vkCreateImageView(appState.Context.device->device, &imageViewCreateInfoFragmentDensity, nullptr, &textureFragmentDensity.view));
				updateTextureSampler(&appState.Context, &textureFragmentDensity);
			}
		}
		if (appState.Renderer.RenderPassSingleView.sampleCount > VK_SAMPLE_COUNT_1_BIT)
		{
			const int depth = 0;
			const int faceCount = 1;
			const int width = framebuffer.Width;
			const int height = framebuffer.Height;
			const int maxDimension = width > height ? (width > depth ? width : depth) : (height > depth ? height : depth);
			const int maxMipLevels = (1 + integerLog2(maxDimension));
			const int layerCount = (isMultiview ? 2 : 1);
			VkFormatProperties props;
			VC(appState.Context.device->instance->vkGetPhysicalDeviceFormatProperties(appState.Context.device->physicalDevice, appState.Renderer.RenderPassSingleView.internalColorFormat, &props));
			if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0)
			{
				__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Color attachment bit in texture is not defined.");
				vrapi_Shutdown();
				exit(0);
			}
			const int numStorageLevels = 1;
			const int arrayLayerCount = faceCount * layerCount;
			auto texture = &framebuffer.Framebuffer.renderTexture;
			texture->width = width;
			texture->height = height;
			texture->depth = depth;
			texture->layerCount = arrayLayerCount;
			texture->mipCount = numStorageLevels;
			texture->sampleCount = appState.Renderer.RenderPassSingleView.sampleCount;
			texture->usage = TEXTURE_USAGE_UNDEFINED;
			texture->usageFlags = TEXTURE_USAGE_COLOR_ATTACHMENT;
			texture->wrapMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			texture->filter = (numStorageLevels > 1) ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
			texture->maxAnisotropy = 1.0f;
			texture->format = appState.Renderer.RenderPassSingleView.internalColorFormat;
			const VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
			VkImageCreateInfo imageCreateInfo { };
			imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
			imageCreateInfo.format = appState.Renderer.RenderPassSingleView.internalColorFormat;
			imageCreateInfo.extent.width = width;
			imageCreateInfo.extent.height = height;
			imageCreateInfo.extent.depth = std::max(depth, 1);
			imageCreateInfo.mipLevels = numStorageLevels;
			imageCreateInfo.arrayLayers = arrayLayerCount;
			imageCreateInfo.samples = appState.Renderer.RenderPassSingleView.sampleCount;
			imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCreateInfo.usage = usage;
			imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			VK(appState.Context.device->vkCreateImage(appState.Context.device->device, &imageCreateInfo, nullptr, &texture->image));
			VkMemoryRequirements memoryRequirements;
			VC(appState.Context.device->vkGetImageMemoryRequirements(appState.Context.device->device, texture->image, &memoryRequirements));
			VkMemoryAllocateInfo memoryAllocateInfo { };
			memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			memoryAllocateInfo.allocationSize = memoryRequirements.size;
			memoryAllocateInfo.memoryTypeIndex = getMemoryTypeIndex(appState.Context.device, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK(appState.Context.device->vkAllocateMemory(appState.Context.device->device, &memoryAllocateInfo, nullptr, &texture->memory));
			VK(appState.Context.device->vkBindImageMemory(appState.Context.device->device, texture->image, texture->memory, 0));
			beginSetupCommandBuffer(&appState.Context);
			{
				VkImageMemoryBarrier imageMemoryBarrier { };
				imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageMemoryBarrier.image = texture->image;
				imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageMemoryBarrier.subresourceRange.levelCount = numStorageLevels;
				imageMemoryBarrier.subresourceRange.layerCount = arrayLayerCount;
				const VkPipelineStageFlags src_stages = pipelineStagesForTextureUsage(texture->usage, true);
				const VkPipelineStageFlags dst_stages = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
				const VkDependencyFlags flags = 0;
				VC(appState.Context.device->vkCmdPipelineBarrier(appState.Context.setupCommandBuffer, src_stages, dst_stages, flags, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
			}
			flushSetupCommandBuffer(&appState.Context);
			texture->usage = TEXTURE_USAGE_SAMPLED;
			texture->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			const VkImageViewType viewType = ((layerCount > 0) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D);
			VkImageViewCreateInfo imageViewCreateInfo { };
			imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCreateInfo.image = texture->image;
			imageViewCreateInfo.viewType = viewType;
			imageViewCreateInfo.format = appState.Renderer.RenderPassSingleView.internalColorFormat;
			imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
			imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
			imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
			imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
			imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageViewCreateInfo.subresourceRange.levelCount = numStorageLevels;
			imageViewCreateInfo.subresourceRange.layerCount = arrayLayerCount;
			VK(appState.Context.device->vkCreateImageView(appState.Context.device->device, &imageViewCreateInfo, nullptr, &texture->view));
			updateTextureSampler(&appState.Context, texture);
			{
				beginSetupCommandBuffer(&appState.Context);
				const VkImageLayout newImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				VkImageMemoryBarrier imageMemoryBarrier { };
				imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				imageMemoryBarrier.srcAccessMask = accessForTextureUsage(texture->usage);
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				imageMemoryBarrier.oldLayout = texture->imageLayout;
				imageMemoryBarrier.newLayout = newImageLayout;
				imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageMemoryBarrier.image = texture->image;
				imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageMemoryBarrier.subresourceRange.levelCount = texture->mipCount;
				imageMemoryBarrier.subresourceRange.layerCount = texture->layerCount;
				const VkPipelineStageFlags src_stages = pipelineStagesForTextureUsage(texture->usage, true);
				const VkPipelineStageFlags dst_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				const VkDependencyFlags flags = 0;
				VC(appState.Context.device->vkCmdPipelineBarrier(appState.Context.setupCommandBuffer, src_stages, dst_stages, flags, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
				texture->usage = TEXTURE_USAGE_COLOR_ATTACHMENT;
				texture->imageLayout = newImageLayout;
				flushSetupCommandBuffer(&appState.Context);
			}
		}
		if (appState.Renderer.RenderPassSingleView.internalDepthFormat != VK_FORMAT_UNDEFINED)
		{
			const auto depthBuffer = &framebuffer.Framebuffer.depthBuffer;
			const auto depthFormat = appState.Renderer.RenderPassSingleView.depthFormat;
			const auto width = framebuffer.Width;
			const auto height = framebuffer.Height;
			const auto numLayers = (isMultiview ? 2 : 1);
			depthBuffer->format = depthFormat;
			if (depthFormat == SURFACE_DEPTH_FORMAT_NONE)
			{
				depthBuffer->internalFormat = VK_FORMAT_UNDEFINED;
			}
			else
			{
				depthBuffer->internalFormat = ((depthFormat == SURFACE_DEPTH_FORMAT_D16) ? VK_FORMAT_D16_UNORM : ((depthFormat == SURFACE_DEPTH_FORMAT_D24) ? VK_FORMAT_D24_UNORM_S8_UINT : VK_FORMAT_UNDEFINED));
				VkImageCreateInfo imageCreateInfo { };
				imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
				imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
				imageCreateInfo.format = depthBuffer->internalFormat;
				imageCreateInfo.extent.width = width;
				imageCreateInfo.extent.height = height;
				imageCreateInfo.extent.depth = 1;
				imageCreateInfo.mipLevels = 1;
				imageCreateInfo.arrayLayers = numLayers;
				imageCreateInfo.samples = appState.Renderer.RenderPassSingleView.sampleCount;
				imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
				imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
				imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
				imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				VK(appState.Context.device->vkCreateImage(appState.Context.device->device, &imageCreateInfo, nullptr, &depthBuffer->image));
				VkMemoryRequirements memoryRequirements;
				VC(appState.Context.device->vkGetImageMemoryRequirements(appState.Context.device->device, depthBuffer->image, &memoryRequirements));
				VkMemoryAllocateInfo memoryAllocateInfo { };
				memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
				memoryAllocateInfo.allocationSize = memoryRequirements.size;
				memoryAllocateInfo.memoryTypeIndex = getMemoryTypeIndex(appState.Context.device, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
				VK(appState.Context.device->vkAllocateMemory(appState.Context.device->device, &memoryAllocateInfo, nullptr, &depthBuffer->memory));
				VK(appState.Context.device->vkBindImageMemory(appState.Context.device->device, depthBuffer->image, depthBuffer->memory, 0));
				VkImageViewCreateInfo imageViewCreateInfo { };
				imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				imageViewCreateInfo.image = depthBuffer->image;
				imageViewCreateInfo.viewType = (numLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D);
				imageViewCreateInfo.format = depthBuffer->internalFormat;
				imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
				imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
				imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
				imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
				imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
				imageViewCreateInfo.subresourceRange.levelCount = 1;
				imageViewCreateInfo.subresourceRange.layerCount = numLayers;
				VK(appState.Context.device->vkCreateImageView(appState.Context.device->device, &imageViewCreateInfo, nullptr, &depthBuffer->view));
				{
					beginSetupCommandBuffer(&appState.Context);
					VkImageMemoryBarrier imageMemoryBarrier { };
					imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
					imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
					imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
					imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					imageMemoryBarrier.image = depthBuffer->image;
					imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
					imageMemoryBarrier.subresourceRange.levelCount = 1;
					imageMemoryBarrier.subresourceRange.layerCount = numLayers;
					const VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
					const VkPipelineStageFlags dst_stages = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
					const VkDependencyFlags flags = 0;
					VC(appState.Context.device->vkCmdPipelineBarrier(appState.Context.setupCommandBuffer, src_stages, dst_stages, flags, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
					flushSetupCommandBuffer(&appState.Context);
				}
				depthBuffer->imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			}
		}
		for (auto j = 0; j < framebuffer.TextureSwapChainLength; j++)
		{
			uint32_t attachmentCount = 0;
			VkImageView attachments[4];
			if (appState.Renderer.RenderPassSingleView.sampleCount > VK_SAMPLE_COUNT_1_BIT)
			{
				attachments[attachmentCount++] = framebuffer.Framebuffer.renderTexture.view;
			}
			attachments[attachmentCount++] = framebuffer.Framebuffer.colorTextures[j].view;
			if (appState.Renderer.RenderPassSingleView.internalDepthFormat != VK_FORMAT_UNDEFINED)
			{
				attachments[attachmentCount++] = framebuffer.Framebuffer.depthBuffer.view;
			}
			if (framebuffer.Framebuffer.fragmentDensityTextures.size() > 0 && appState.Renderer.RenderPassSingleView.useFragmentDensity)
			{
				attachments[attachmentCount++] = framebuffer.Framebuffer.fragmentDensityTextures[j].view;
			}
			VkFramebufferCreateInfo framebufferCreateInfo { };
			framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferCreateInfo.renderPass = appState.Renderer.RenderPassSingleView.renderPass;
			framebufferCreateInfo.attachmentCount = attachmentCount;
			framebufferCreateInfo.pAttachments = attachments;
			framebufferCreateInfo.width = framebuffer.Framebuffer.width;
			framebufferCreateInfo.height = framebuffer.Framebuffer.height;
			framebufferCreateInfo.layers = 1;
			VK(appState.Context.device->vkCreateFramebuffer(appState.Context.device->device, &framebufferCreateInfo, nullptr, &framebuffer.Framebuffer.framebuffers[j]));
		}
		const auto numBuffers = framebuffer.Framebuffer.numBuffers;
		auto& commandBuffer = appState.Renderer.EyeCommandBuffer[i];
		commandBuffer.numBuffers = numBuffers;
		commandBuffer.currentBuffer = 0;
		commandBuffer.context = &appState.Context;
		commandBuffer.cmdBuffers.resize(numBuffers);
		commandBuffer.fences.resize(numBuffers);
		commandBuffer.mappedBuffers.resize(numBuffers);
		commandBuffer.oldMappedBuffers.resize(numBuffers);
		commandBuffer.pipelineResources.resize(numBuffers);
		for (auto j = 0; j < numBuffers; j++)
		{
			VkCommandBufferAllocateInfo commandBufferAllocateInfo { };
			commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			commandBufferAllocateInfo.commandPool = appState.Context.commandPool;
			commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			commandBufferAllocateInfo.commandBufferCount = 1;
			VK(appState.Context.device->vkAllocateCommandBuffers(appState.Context.device->device, &commandBufferAllocateInfo, &commandBuffer.cmdBuffers[j]));
			VkFenceCreateInfo fenceCreateInfo { };
			fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			VK(appState.Context.device->vkCreateFence(appState.Context.device->device, &fenceCreateInfo, nullptr, &commandBuffer.fences[j].fence));
			commandBuffer.fences[j].submitted = false;
		}
	}
	for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++)
	{
		appState.Renderer.ViewMatrix[eye] = ovrMatrix4f_CreateIdentity();
		appState.Renderer.ProjectionMatrix[eye] = ovrMatrix4f_CreateIdentity();
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
		if (appState.Ovr == nullptr)
		{
			continue;
		}
		if (!appState.Scene.CreatedScene)
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
			appState.CylinderWidth = 640;
			appState.CylinderHeight = 400;
			appState.CylinderSwapChain = vrapi_CreateTextureSwapChain3(VRAPI_TEXTURE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, appState.CylinderWidth, appState.CylinderHeight, 1, 1);
			appState.CylinderTexData.resize(appState.CylinderWidth * appState.CylinderHeight, 255 << 24);
			appState.CylinderTexImage = vrapi_GetTextureSwapChainBufferVulkan(appState.CylinderSwapChain, 0);
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
			createBuffer(&appState.Context, &appState.CylinderTexBuffer, (BufferType)0, appState.CylinderTexData.size() * sizeof(uint32_t), appState.CylinderTexData.data(), true);
			{
				beginSetupCommandBuffer(&appState.Context);
				VkBufferImageCopy region { };
				region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.imageSubresource.layerCount = 1;
				region.imageExtent.width = appState.CylinderWidth;
				region.imageExtent.height = appState.CylinderHeight;
				region.imageExtent.depth = 1;
				VC(appState.Device.vkCmdCopyBufferToImage(appState.Context.setupCommandBuffer, appState.CylinderTexBuffer.buffer, appState.CylinderTexImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region));
				flushSetupCommandBuffer(&appState.Context);
			}
			VkCommandBufferAllocateInfo commandBufferAllocateInfo { };
			commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			commandBufferAllocateInfo.commandPool = appState.Context.commandPool;
			commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			commandBufferAllocateInfo.commandBufferCount = 1;
			VK(appState.Device.vkAllocateCommandBuffers(appState.Context.device->device, &commandBufferAllocateInfo, &appState.CylinderCommandBuffer));
			const bool isMultiview = appState.Context.device->supportsMultiview;
			appState.Scene.NumViews = (isMultiview) ? 2 : 1;
			static const ovrVector3f cubePositions[8] = {
					{-1.0f, 1.0f,  -1.0f},
					{1.0f,  1.0f,  -1.0f},
					{1.0f,  1.0f,  1.0f},
					{-1.0f, 1.0f,  1.0f}, // top
					{-1.0f, -1.0f, -1.0f},
					{-1.0f, -1.0f, 1.0f},
					{1.0f,  -1.0f, 1.0f},
					{1.0f,  -1.0f, -1.0f} // bottom
			};
			static const ovrVector4f cubeColors[8] = {
					{1.0f, 0.0f, 1.0f, 1.0f},
					{0.0f, 1.0f, 0.0f, 1.0f},
					{0.0f, 0.0f, 1.0f, 1.0f},
					{1.0f, 0.0f, 0.0f, 1.0f}, // top
					{0.0f, 0.0f, 1.0f, 1.0f},
					{0.0f, 1.0f, 0.0f, 1.0f},
					{1.0f, 0.0f, 1.0f, 1.0f},
					{1.0f, 0.0f, 0.0f, 1.0f} // bottom
			};
			static const TriangleIndex cubeIndices[36] = {
					0, 2, 1, 2, 0, 3, // top
					4, 6, 5, 6, 4, 7, // bottom
					2, 6, 7, 7, 1, 2, // right
					0, 4, 5, 5, 3, 0, // left
					3, 5, 6, 6, 2, 3, // front
					0, 1, 7, 7, 4, 0 // back
			};
			const int attribsFlags = VERTEX_ATTRIBUTE_FLAG_POSITION | VERTEX_ATTRIBUTE_FLAG_COLOR;
			DefaultVertexAttributeArrays cubeAttributeArrays;
			size_t totalSize = 0;
			for (int i = 0; DefaultVertexAttributeLayout[i].attributeFlag != 0; i++)
			{
				const VertexAttribute *v = &DefaultVertexAttributeLayout[i];
				if ((v->attributeFlag & attribsFlags) != 0)
				{
					totalSize += v->attributeSize;
				}
			}
			std::vector<unsigned char> attribData(8 * totalSize);
			cubeAttributeArrays.base.buffer = nullptr;
			cubeAttributeArrays.base.layout = DefaultVertexAttributeLayout;
			cubeAttributeArrays.base.data = attribData.data();
			cubeAttributeArrays.base.dataSize = attribData.size();
			cubeAttributeArrays.base.vertexCount = 8;
			cubeAttributeArrays.base.attribsFlags = attribsFlags;
			auto dataBytePtr = attribData.data();
			size_t offset = 0;
			for (int i = 0; cubeAttributeArrays.base.layout[i].attributeFlag != 0; i++)
			{
				const VertexAttribute *v = &cubeAttributeArrays.base.layout[i];
				void **attribPtr = (void **) (((char *)&cubeAttributeArrays.base) + v->attributeOffset);
				if ((v->attributeFlag & attribsFlags) != 0)
				{
					*attribPtr = (dataBytePtr + offset);
					offset += 8 * v->attributeSize;
				}
				else
				{
					*attribPtr = nullptr;
				}
			}
			for (int i = 0; i < 8; i++)
			{
				cubeAttributeArrays.position[i].x = cubePositions[i].x;
				cubeAttributeArrays.position[i].y = cubePositions[i].y;
				cubeAttributeArrays.position[i].z = cubePositions[i].z;
				cubeAttributeArrays.color[i].x = cubeColors[i].x;
				cubeAttributeArrays.color[i].y = cubeColors[i].y;
				cubeAttributeArrays.color[i].z = cubeColors[i].z;
				cubeAttributeArrays.color[i].w = cubeColors[i].w;
			}
			std::vector<unsigned short> indexData(36);
			TriangleIndexArray cubeIndexArray;
			cubeIndexArray.indexCount = indexData.size();
			cubeIndexArray.indexArray = indexData.data();
			memcpy(cubeIndexArray.indexArray, cubeIndices, 36 * sizeof(unsigned short));
			cubeIndexArray.buffer = nullptr;
			appState.Scene.Cube.layout = cubeAttributeArrays.base.layout;
			appState.Scene.Cube.vertexAttribsFlags = cubeAttributeArrays.base.attribsFlags;
			appState.Scene.Cube.vertexCount = cubeAttributeArrays.base.vertexCount;
			appState.Scene.Cube.indexCount = cubeIndexArray.indexCount;
			createBuffer(&appState.Context, &appState.Scene.Cube.vertexBuffer, BUFFER_TYPE_VERTEX, cubeAttributeArrays.base.dataSize, cubeAttributeArrays.base.data, false);
			createBuffer(&appState.Context, &appState.Scene.Cube.indexBuffer, BUFFER_TYPE_INDEX, cubeIndexArray.indexCount * sizeof(cubeIndexArray.indexArray[0]), cubeIndexArray.indexArray, false);
			appState.Scene.Cube.instanceCount = NUM_INSTANCES;
			appState.Scene.Cube.instanceAttribsFlags = VERTEX_ATTRIBUTE_FLAG_TRANSFORM;
			totalSize = 0;
			for (int i = 0; appState.Scene.Cube.layout[i].attributeFlag != 0; i++)
			{
				const VertexAttribute *v = &appState.Scene.Cube.layout[i];
				if ((v->attributeFlag & appState.Scene.Cube.instanceAttribsFlags) != 0)
				{
					totalSize += v->attributeSize;
				}
			}
			createBuffer(&appState.Context, &appState.Scene.Cube.instanceBuffer, BUFFER_TYPE_VERTEX, NUM_INSTANCES * totalSize, nullptr, false);
			appState.Scene.Program.vertexAttribsFlags = VERTEX_ATTRIBUTE_FLAG_POSITION | VERTEX_ATTRIBUTE_FLAG_COLOR | VERTEX_ATTRIBUTE_FLAG_TRANSFORM;
			createShader(appState.Context.device, &appState.Scene.Program.vertexShaderModule, VK_SHADER_STAGE_VERTEX_BIT, isMultiview ? PROGRAM(colorOnlyMultiviewVertexProgram) : PROGRAM(colorOnlyVertexProgram), isMultiview ? sizeof(PROGRAM(colorOnlyMultiviewVertexProgram)) : sizeof(PROGRAM(colorOnlyVertexProgram)));
			createShader(appState.Context.device, &appState.Scene.Program.fragmentShaderModule, VK_SHADER_STAGE_FRAGMENT_BIT, PROGRAM(colorOnlyFragmentProgram), sizeof(PROGRAM(colorOnlyFragmentProgram)));
			appState.Scene.Program.pipelineStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			appState.Scene.Program.pipelineStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
			appState.Scene.Program.pipelineStages[0].module = appState.Scene.Program.vertexShaderModule;
			appState.Scene.Program.pipelineStages[0].pName = "main";
			appState.Scene.Program.pipelineStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			appState.Scene.Program.pipelineStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			appState.Scene.Program.pipelineStages[1].module = appState.Scene.Program.fragmentShaderModule;
			appState.Scene.Program.pipelineStages[1].pName = "main";
			auto numParms = ARRAY_SIZE(colorOnlyProgramParms);
			appState.Scene.Program.parmLayout.numParms = numParms;
			appState.Scene.Program.parmLayout.parms = colorOnlyProgramParms;
			int numSampledTextureBindings[PROGRAM_STAGE_MAX] { };
			int numStorageTextureBindings[PROGRAM_STAGE_MAX] { };
			int numUniformBufferBindings[PROGRAM_STAGE_MAX] { };
			int numStorageBufferBindings[PROGRAM_STAGE_MAX] { };
			offset = 0;
			appState.Scene.Program.parmLayout.offsetForIndex.resize(MAX_PROGRAM_PARMS);
			appState.Scene.Program.parmLayout.bindings.resize(MAX_PROGRAM_PARMS);
			std::fill(appState.Scene.Program.parmLayout.offsetForIndex.begin(), appState.Scene.Program.parmLayout.offsetForIndex.end(), -1);
			for (int i = 0; i < numParms; i++)
			{
				if (colorOnlyProgramParms[i].type == PROGRAM_PARM_TYPE_TEXTURE_SAMPLED || colorOnlyProgramParms[i].type == PROGRAM_PARM_TYPE_TEXTURE_STORAGE || colorOnlyProgramParms[i].type == PROGRAM_PARM_TYPE_BUFFER_UNIFORM)
				{
					for (int stageIndex = 0; stageIndex < PROGRAM_STAGE_MAX; stageIndex++)
					{
						if ((colorOnlyProgramParms[i].stageFlags & (1 << stageIndex)) != 0)
						{
							numSampledTextureBindings[stageIndex] += (colorOnlyProgramParms[i].type == PROGRAM_PARM_TYPE_TEXTURE_SAMPLED);
							numStorageTextureBindings[stageIndex] += (colorOnlyProgramParms[i].type == PROGRAM_PARM_TYPE_TEXTURE_STORAGE);
							numUniformBufferBindings[stageIndex] += (colorOnlyProgramParms[i].type == PROGRAM_PARM_TYPE_BUFFER_UNIFORM);
						}
					}
					appState.Scene.Program.parmLayout.bindings[colorOnlyProgramParms[i].binding] = &colorOnlyProgramParms[i];
					if (colorOnlyProgramParms[i].binding >= appState.Scene.Program.parmLayout.numBindings)
					{
						appState.Scene.Program.parmLayout.numBindings = colorOnlyProgramParms[i].binding + 1;
					}
				}
				else
				{
					appState.Scene.Program.parmLayout.pushConstants.push_back(&colorOnlyProgramParms[i]);
					appState.Scene.Program.parmLayout.offsetForIndex[colorOnlyProgramParms[i].index] = offset;
					offset += getPushConstantSize(colorOnlyProgramParms[i].type);
				}
			}
			int numTotalSampledTextureBindings = 0;
			int numTotalStorageTextureBindings = 0;
			int numTotalUniformBufferBindings = 0;
			int numTotalStorageBufferBindings = 0;
			for (int stage = 0; stage < PROGRAM_STAGE_MAX; stage++)
			{
				numTotalSampledTextureBindings += numSampledTextureBindings[stage];
				numTotalStorageTextureBindings += numStorageTextureBindings[stage];
				numTotalUniformBufferBindings += numUniformBufferBindings[stage];
				numTotalStorageBufferBindings += numStorageBufferBindings[stage];
			}
			std::vector<VkDescriptorSetLayoutBinding> descriptorSetBindings;
			std::vector<VkPushConstantRange> pushConstantRanges;
			for (int i = 0; i < numParms; i++)
			{
				VkShaderStageFlags stageFlags = ((colorOnlyProgramParms[i].stageFlags & PROGRAM_STAGE_FLAG_VERTEX) ? VK_SHADER_STAGE_VERTEX_BIT : 0) | ((colorOnlyProgramParms[i].stageFlags & PROGRAM_STAGE_FLAG_FRAGMENT) ? VK_SHADER_STAGE_FRAGMENT_BIT : 0);
				if (colorOnlyProgramParms[i].type == PROGRAM_PARM_TYPE_TEXTURE_SAMPLED || colorOnlyProgramParms[i].type == PROGRAM_PARM_TYPE_TEXTURE_STORAGE || colorOnlyProgramParms[i].type == PROGRAM_PARM_TYPE_BUFFER_UNIFORM)
				{
					descriptorSetBindings.emplace_back();
					descriptorSetBindings.back().binding = colorOnlyProgramParms[i].binding;
					switch (colorOnlyProgramParms[i].type)
					{
						case PROGRAM_PARM_TYPE_TEXTURE_SAMPLED:
							descriptorSetBindings.back().descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
							break;
						case PROGRAM_PARM_TYPE_TEXTURE_STORAGE:
							descriptorSetBindings.back().descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
							break;
						case PROGRAM_PARM_TYPE_BUFFER_UNIFORM:
							descriptorSetBindings.back().descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
							break;
						default:
							descriptorSetBindings.back().descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
					}
					descriptorSetBindings.back().descriptorCount = 1;
					descriptorSetBindings.back().stageFlags = stageFlags;
				}
				else
				{
					pushConstantRanges.emplace_back();
					pushConstantRanges.back().stageFlags = stageFlags;
					pushConstantRanges.back().offset = colorOnlyProgramParms[i].binding;
					pushConstantRanges.back().size = getPushConstantSize(colorOnlyProgramParms[i].type);
				}
			}
			VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo { };
			descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			descriptorSetLayoutCreateInfo.bindingCount = descriptorSetBindings.size();
			descriptorSetLayoutCreateInfo.pBindings = (descriptorSetBindings.size() > 0) ? descriptorSetBindings.data() : nullptr;
			VK(appState.Context.device->vkCreateDescriptorSetLayout(appState.Context.device->device, &descriptorSetLayoutCreateInfo, nullptr, &appState.Scene.Program.parmLayout.descriptorSetLayout));
			VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo { };
			pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutCreateInfo.setLayoutCount = 1;
			pipelineLayoutCreateInfo.pSetLayouts = &appState.Scene.Program.parmLayout.descriptorSetLayout;
			pipelineLayoutCreateInfo.pushConstantRangeCount = pushConstantRanges.size();
			pipelineLayoutCreateInfo.pPushConstantRanges = (pushConstantRanges.size() > 0) ? pushConstantRanges.data() : nullptr;
			VK(appState.Context.device->vkCreatePipelineLayout(appState.Context.device->device, &pipelineLayoutCreateInfo, nullptr, &appState.Scene.Program.parmLayout.pipelineLayout));
			unsigned int hash = 5381;
			for (int i = 0; i < numParms * (int) sizeof(colorOnlyProgramParms[0]); i++)
			{
				hash = ((hash << 5) - hash) + ((const char *) colorOnlyProgramParms)[i];
			}
			appState.Scene.Program.parmLayout.hash = hash;
			GraphicsPipelineParms pipelineParms { };
			pipelineParms.rop.redWriteEnable = true;
			pipelineParms.rop.blueWriteEnable = true;
			pipelineParms.rop.greenWriteEnable = true;
			pipelineParms.rop.depthTestEnable = true;
			pipelineParms.rop.depthWriteEnable = true;
			pipelineParms.rop.frontFace = VK_FRONT_FACE_CLOCKWISE;
			pipelineParms.rop.cullMode = VK_CULL_MODE_BACK_BIT;
			pipelineParms.rop.depthCompare = VK_COMPARE_OP_LESS_OR_EQUAL;
			pipelineParms.rop.blendOpColor = VK_BLEND_OP_ADD;
			pipelineParms.rop.blendSrcColor = VK_BLEND_FACTOR_ONE;
			pipelineParms.rop.blendDstColor = VK_BLEND_FACTOR_ZERO;
			pipelineParms.rop.blendOpAlpha = VK_BLEND_OP_ADD;
			pipelineParms.rop.blendSrcAlpha = VK_BLEND_FACTOR_ONE;
			pipelineParms.rop.blendDstAlpha = VK_BLEND_FACTOR_ZERO;
			pipelineParms.renderPass = &appState.Renderer.RenderPassSingleView;
			pipelineParms.program = &appState.Scene.Program;
			pipelineParms.geometry = &appState.Scene.Cube;
			appState.Scene.Pipeline.rop = pipelineParms.rop;
			appState.Scene.Pipeline.program = pipelineParms.program;
			appState.Scene.Pipeline.geometry = pipelineParms.geometry;
			initVertexAttributes(false, pipelineParms.geometry->layout, pipelineParms.geometry->vertexCount, pipelineParms.geometry->vertexAttribsFlags, pipelineParms.program->vertexAttribsFlags, appState.Scene.Pipeline);
			appState.Scene.Pipeline.firstInstanceBinding = appState.Scene.Pipeline.vertexBindingCount;
			initVertexAttributes(true, pipelineParms.geometry->layout, pipelineParms.geometry->instanceCount, pipelineParms.geometry->instanceAttribsFlags, pipelineParms.program->vertexAttribsFlags, appState.Scene.Pipeline);
			appState.Scene.Pipeline.vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			appState.Scene.Pipeline.vertexInputState.vertexBindingDescriptionCount = appState.Scene.Pipeline.vertexBindingCount;
			appState.Scene.Pipeline.vertexInputState.pVertexBindingDescriptions = appState.Scene.Pipeline.vertexBindings;
			appState.Scene.Pipeline.vertexInputState.vertexAttributeDescriptionCount = appState.Scene.Pipeline.vertexAttributeCount;
			appState.Scene.Pipeline.vertexInputState.pVertexAttributeDescriptions = appState.Scene.Pipeline.vertexAttributes;
			appState.Scene.Pipeline.inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			appState.Scene.Pipeline.inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			VkPipelineTessellationStateCreateInfo tessellationStateCreateInfo { };
			tessellationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
			VkPipelineViewportStateCreateInfo viewportStateCreateInfo { };
			viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportStateCreateInfo.viewportCount = 1;
			viewportStateCreateInfo.scissorCount = 1;
			VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo { };
			rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
			rasterizationStateCreateInfo.cullMode = (VkCullModeFlags)pipelineParms.rop.cullMode;
			rasterizationStateCreateInfo.frontFace = (VkFrontFace)pipelineParms.rop.frontFace;
			rasterizationStateCreateInfo.lineWidth = 1.0f;
			VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo { };
			multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multisampleStateCreateInfo.rasterizationSamples = (VkSampleCountFlagBits)pipelineParms.renderPass->sampleCount;
			multisampleStateCreateInfo.minSampleShading = 1.0f;
			VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo { };
			depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			depthStencilStateCreateInfo.depthTestEnable = pipelineParms.rop.depthTestEnable ? VK_TRUE : VK_FALSE;
			depthStencilStateCreateInfo.depthWriteEnable = pipelineParms.rop.depthWriteEnable ? VK_TRUE : VK_FALSE;
			depthStencilStateCreateInfo.depthCompareOp = (VkCompareOp)pipelineParms.rop.depthCompare;
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
			VkPipelineColorBlendAttachmentState colorBlendAttachementState[1] { };
			colorBlendAttachementState[0].blendEnable = pipelineParms.rop.blendEnable ? VK_TRUE : VK_FALSE;
			colorBlendAttachementState[0].srcColorBlendFactor = (VkBlendFactor) pipelineParms.rop.blendSrcColor;
			colorBlendAttachementState[0].dstColorBlendFactor = (VkBlendFactor) pipelineParms.rop.blendDstColor;
			colorBlendAttachementState[0].colorBlendOp = (VkBlendOp) pipelineParms.rop.blendOpColor;
			colorBlendAttachementState[0].srcAlphaBlendFactor = (VkBlendFactor) pipelineParms.rop.blendSrcAlpha;
			colorBlendAttachementState[0].dstAlphaBlendFactor = (VkBlendFactor) pipelineParms.rop.blendDstAlpha;
			colorBlendAttachementState[0].alphaBlendOp = (VkBlendOp) pipelineParms.rop.blendOpAlpha;
			colorBlendAttachementState[0].colorWriteMask = (pipelineParms.rop.redWriteEnable ? VK_COLOR_COMPONENT_R_BIT : 0) | (pipelineParms.rop.blueWriteEnable ? VK_COLOR_COMPONENT_G_BIT : 0) | (pipelineParms.rop.greenWriteEnable ? VK_COLOR_COMPONENT_B_BIT : 0) | (pipelineParms.rop.alphaWriteEnable ? VK_COLOR_COMPONENT_A_BIT : 0);
			VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo { };
			colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_CLEAR;
			colorBlendStateCreateInfo.attachmentCount = 1;
			colorBlendStateCreateInfo.pAttachments = colorBlendAttachementState;
			colorBlendStateCreateInfo.blendConstants[0] = pipelineParms.rop.blendColor.x;
			colorBlendStateCreateInfo.blendConstants[1] = pipelineParms.rop.blendColor.y;
			colorBlendStateCreateInfo.blendConstants[2] = pipelineParms.rop.blendColor.z;
			colorBlendStateCreateInfo.blendConstants[3] = pipelineParms.rop.blendColor.w;
			VkDynamicState dynamicStateEnables[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
			VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo { };
			pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			pipelineDynamicStateCreateInfo.dynamicStateCount = ARRAY_SIZE(dynamicStateEnables);
			pipelineDynamicStateCreateInfo.pDynamicStates = dynamicStateEnables;
			VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo { };
			graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			graphicsPipelineCreateInfo.stageCount = 2;
			graphicsPipelineCreateInfo.pStages = pipelineParms.program->pipelineStages;
			graphicsPipelineCreateInfo.pVertexInputState = &appState.Scene.Pipeline.vertexInputState;
			graphicsPipelineCreateInfo.pInputAssemblyState = &appState.Scene.Pipeline.inputAssemblyState;
			graphicsPipelineCreateInfo.pTessellationState = &tessellationStateCreateInfo;
			graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
			graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
			graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
			graphicsPipelineCreateInfo.pDepthStencilState = (pipelineParms.renderPass->internalDepthFormat != VK_FORMAT_UNDEFINED) ? &depthStencilStateCreateInfo : nullptr;
			graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
			graphicsPipelineCreateInfo.pDynamicState = &pipelineDynamicStateCreateInfo;
			graphicsPipelineCreateInfo.layout = pipelineParms.program->parmLayout.pipelineLayout;
			graphicsPipelineCreateInfo.renderPass = pipelineParms.renderPass->renderPass;
			VK(appState.Context.device->vkCreateGraphicsPipelines(appState.Context.device->device, appState.Context.pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &appState.Scene.Pipeline.pipeline));
			createBuffer(&appState.Context, &appState.Scene.SceneMatrices, BUFFER_TYPE_UNIFORM, 2 * appState.Scene.NumViews * sizeof(ovrMatrix4f), nullptr, false);
			for (int i = 0; i < NUM_ROTATIONS; i++)
			{
				appState.Scene.Rotations[i].x = randomFloat(appState.Scene);
				appState.Scene.Rotations[i].y = randomFloat(appState.Scene);
				appState.Scene.Rotations[i].z = randomFloat(appState.Scene);
			}
			for (int i = 0; i < NUM_INSTANCES; i++)
			{
				volatile float rx, ry, rz;
				for (;;)
				{
					rx = (randomFloat(appState.Scene) - 0.5f) * (50.0f + sqrt(NUM_INSTANCES));
					ry = (randomFloat(appState.Scene) - 0.5f) * (50.0f + sqrt(NUM_INSTANCES));
					rz = (randomFloat(appState.Scene) - 0.5f) * (50.0f + sqrt(NUM_INSTANCES));
					if (fabsf(rx) < 4.0f && fabsf(ry) < 4.0f && fabsf(rz) < 4.0f)
					{
						continue;
					}
					bool overlap = false;
					for (int j = 0; j < i; j++)
					{
						if (fabsf(rx - appState.Scene.CubePositions[j].x) < 4.0f &&
							fabsf(ry - appState.Scene.CubePositions[j].y) < 4.0f &&
							fabsf(rz - appState.Scene.CubePositions[j].z) < 4.0f)
						{
							overlap = true;
							break;
						}
					}
					if (!overlap)
					{
						break;
					}
				}
				rx *= 0.1f;
				ry *= 0.1f;
				rz *= 0.1f;
				int insert = 0;
				const float distSqr = rx * rx + ry * ry + rz * rz;
				for (int j = i; j > 0; j--)
				{
					const ovrVector3f *otherPos = &appState.Scene.CubePositions[j - 1];
					const float otherDistSqr = otherPos->x * otherPos->x + otherPos->y * otherPos->y + otherPos->z * otherPos->z;
					if (distSqr > otherDistSqr)
					{
						insert = j;
						break;
					}
					appState.Scene.CubePositions[j] = appState.Scene.CubePositions[j - 1];
					appState.Scene.CubeRotations[j] = appState.Scene.CubeRotations[j - 1];
				}
				appState.Scene.CubePositions[insert].x = rx;
				appState.Scene.CubePositions[insert].y = ry;
				appState.Scene.CubePositions[insert].z = rz;
				appState.Scene.CubeRotations[insert] = (int) (randomFloat(appState.Scene) * (NUM_ROTATIONS - 0.1f));
			}
			appState.FirstFrame = true;
			appState.Scene.CreatedScene = true;
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
		const double predictedDisplayTime = vrapi_GetPredictedDisplayTime(appState.Ovr, appState.FrameIndex);
		const ovrTracking2 tracking = vrapi_GetPredictedTracking2(appState.Ovr, predictedDisplayTime);
		appState.DisplayTime = predictedDisplayTime;
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
				auto index = 0;
				for (auto y = 0; y < vid_height; y++)
				{
					for (auto x = 0; x < vid_width; x++)
					{
						appState.CylinderTexData[index] = d_8to24table[vid_buffer[index]];
						index++;
					}
				}
				VK(appState.Context.device->vkMapMemory(appState.Context.device->device, appState.CylinderTexBuffer.memory, 0, appState.CylinderTexBuffer.size, 0, &appState.CylinderTexBuffer.mapped));
				memcpy(appState.CylinderTexBuffer.mapped, appState.CylinderTexData.data(), appState.CylinderTexData.size() * sizeof(uint32_t));
				VC(appState.Context.device->vkUnmapMemory(appState.Context.device->device, appState.CylinderTexBuffer.memory));
				VkCommandBufferBeginInfo commandBufferBeginInfo { };
				commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				VK(appState.Device.vkBeginCommandBuffer(appState.CylinderCommandBuffer, &commandBufferBeginInfo));
				VkBufferImageCopy region { };
				region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.imageSubresource.layerCount = 1;
				region.imageExtent.width = appState.CylinderWidth;
				region.imageExtent.height = appState.CylinderHeight;
				region.imageExtent.depth = 1;
				VC(appState.Device.vkCmdCopyBufferToImage(appState.CylinderCommandBuffer, appState.CylinderTexBuffer.buffer, appState.CylinderTexImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region));
				VK(appState.Device.vkEndCommandBuffer(appState.CylinderCommandBuffer));
				VkSubmitInfo submitInfo { };
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.commandBufferCount = 1;
				submitInfo.pCommandBuffers = &appState.CylinderCommandBuffer;
				VK(appState.Device.vkQueueSubmit(appState.Context.queue, 1, &submitInfo, VK_NULL_HANDLE));
			}
		}
		appState.Simulation.CurrentRotation.x = (float)predictedDisplayTime;
		appState.Simulation.CurrentRotation.y = (float)predictedDisplayTime;
		appState.Simulation.CurrentRotation.z = (float)predictedDisplayTime;
		ovrMatrix4f eyeViewMatrixTransposed[2];
		eyeViewMatrixTransposed[0] = ovrMatrix4f_Transpose(&tracking.Eye[0].ViewMatrix);
		eyeViewMatrixTransposed[1] = ovrMatrix4f_Transpose(&tracking.Eye[1].ViewMatrix);
		appState.Renderer.ViewMatrix[0] = eyeViewMatrixTransposed[0];
		appState.Renderer.ViewMatrix[1] = eyeViewMatrixTransposed[1];
		ovrMatrix4f projectionMatrixTransposed[2];
		projectionMatrixTransposed[0] = ovrMatrix4f_Transpose(&tracking.Eye[0].ProjectionMatrix);
		projectionMatrixTransposed[1] = ovrMatrix4f_Transpose(&tracking.Eye[1].ProjectionMatrix);
		appState.Renderer.ProjectionMatrix[0] = projectionMatrixTransposed[0];
		appState.Renderer.ProjectionMatrix[1] = projectionMatrixTransposed[1];
		for (auto i = 0; i < appState.Renderer.NumEyes; i++)
		{
			auto& framebuffer = appState.Renderer.Framebuffer[i];
			VkRect2D screenRect { };
			screenRect.extent.width = framebuffer.Framebuffer.width;
			screenRect.extent.height = framebuffer.Framebuffer.height;
			auto& commandBuffer = appState.Renderer.EyeCommandBuffer[i];
			auto device = commandBuffer.context->device;
			commandBuffer.currentBuffer = (commandBuffer.currentBuffer + 1) % commandBuffer.numBuffers;
			auto fence = &commandBuffer.fences[commandBuffer.currentBuffer];
			if (fence->submitted)
			{
				VK(device->vkWaitForFences(device->device, 1, &fence->fence, VK_TRUE, 1ULL * 1000 * 1000 * 1000));
				VK(device->vkResetFences(device->device, 1, &fence->fence));
				fence->submitted = false;
			}
			{
				for (Buffer **b = &commandBuffer.oldMappedBuffers[commandBuffer.currentBuffer]; *b != nullptr; )
				{
					if ((*b)->unusedCount++ >= MAX_VERTEX_BUFFER_UNUSED_COUNT)
					{
						Buffer *next = (*b)->next;
						if ((*b)->mapped != nullptr)
						{
							VC(device->vkUnmapMemory(device->device, (*b)->memory));
						}
						if ((*b)->owner)
						{
							VC(device->vkDestroyBuffer(device->device, (*b)->buffer, nullptr));
							VC(device->vkFreeMemory(device->device, (*b)->memory, nullptr));
						}
						free(*b);
						*b = next;
					}
					else
					{
						b = &(*b)->next;
					}
				}
				for (Buffer *b = commandBuffer.mappedBuffers[commandBuffer.currentBuffer], *next = nullptr; b != nullptr; b = next)
				{
					next = b->next;
					b->next = commandBuffer.oldMappedBuffers[commandBuffer.currentBuffer];
					commandBuffer.oldMappedBuffers[commandBuffer.currentBuffer] = b;
				}
				commandBuffer.mappedBuffers[commandBuffer.currentBuffer] = nullptr;
			}
			{
				for (PipelineResources **r = &commandBuffer.pipelineResources[commandBuffer.currentBuffer]; *r != nullptr; )
				{
					if ((*r)->unusedCount++ >= MAX_PIPELINE_RESOURCES_UNUSED_COUNT)
					{
						PipelineResources *next = (*r)->next;
						VC(commandBuffer.context->device->vkFreeDescriptorSets(commandBuffer.context->device->device, (*r)->descriptorPool, 1, &(*r)->descriptorSet));
						VC(commandBuffer.context->device->vkDestroyDescriptorPool(commandBuffer.context->device->device, (*r)->descriptorPool, nullptr));
						free(*r);
						*r = next;
					}
					else
					{
						r = &(*r)->next;
					}
				}
			}
			commandBuffer.currentGraphicsState.pipeline = nullptr;
			commandBuffer.currentGraphicsState.vertexBuffer = nullptr;
			commandBuffer.currentGraphicsState.instanceBuffer = nullptr;
			memset((void *)&commandBuffer.currentGraphicsState.parmState, 0, sizeof(ProgramParmState));
			commandBuffer.currentGraphicsState.numInstances = 1;
			VK(device->vkResetCommandBuffer(commandBuffer.cmdBuffers[commandBuffer.currentBuffer], 0));
			VkCommandBufferBeginInfo commandBufferBeginInfo { };
			commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			VK(device->vkBeginCommandBuffer(commandBuffer.cmdBuffers[commandBuffer.currentBuffer], &commandBufferBeginInfo));
			{
				VkMemoryBarrier memoryBarrier { };
				memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
				memoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
				const VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_HOST_BIT;
				const VkPipelineStageFlags dst_stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
				const VkDependencyFlags flags = 0;
				VC(device->vkCmdPipelineBarrier(commandBuffer.cmdBuffers[commandBuffer.currentBuffer], src_stages, dst_stages, flags, 1, &memoryBarrier, 0, nullptr, 0, nullptr));
			}
			framebuffer.Framebuffer.currentBuffer = (framebuffer.Framebuffer.currentBuffer + 1) % framebuffer.Framebuffer.numBuffers;
			framebuffer.Framebuffer.currentLayer = 0;
			const VkImageLayout newImageLayout = layoutForTextureUsage(TEXTURE_USAGE_COLOR_ATTACHMENT);
			VkImageMemoryBarrier imageMemoryBarrier { };
			imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrier.srcAccessMask = accessForTextureUsage(framebuffer.Framebuffer.colorTextures[framebuffer.Framebuffer.currentBuffer].usage);
			imageMemoryBarrier.dstAccessMask = accessForTextureUsage(TEXTURE_USAGE_COLOR_ATTACHMENT);
			imageMemoryBarrier.oldLayout = framebuffer.Framebuffer.colorTextures[framebuffer.Framebuffer.currentBuffer].imageLayout;
			imageMemoryBarrier.newLayout = newImageLayout;
			imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageMemoryBarrier.image = framebuffer.Framebuffer.colorTextures[framebuffer.Framebuffer.currentBuffer].image;
			imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageMemoryBarrier.subresourceRange.levelCount = framebuffer.Framebuffer.colorTextures[framebuffer.Framebuffer.currentBuffer].mipCount;
			imageMemoryBarrier.subresourceRange.layerCount = framebuffer.Framebuffer.colorTextures[framebuffer.Framebuffer.currentBuffer].layerCount;
			const VkPipelineStageFlags src_stages = pipelineStagesForTextureUsage(framebuffer.Framebuffer.colorTextures[framebuffer.Framebuffer.currentBuffer].usage, true);
			const VkPipelineStageFlags dst_stages = pipelineStagesForTextureUsage(TEXTURE_USAGE_COLOR_ATTACHMENT, false);
			const VkDependencyFlags flags = 0;
			VC(appState.Context.device->vkCmdPipelineBarrier(commandBuffer.cmdBuffers[commandBuffer.currentBuffer], src_stages, dst_stages, flags, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
			framebuffer.Framebuffer.colorTextures[framebuffer.Framebuffer.currentBuffer].usage = TEXTURE_USAGE_COLOR_ATTACHMENT;
			framebuffer.Framebuffer.colorTextures[framebuffer.Framebuffer.currentBuffer].imageLayout = newImageLayout;
			commandBuffer.currentFramebuffer = &framebuffer.Framebuffer;
			ovrMatrix4f *sceneMatrices = nullptr;
			Buffer *newBuffer = nullptr;
			for (Buffer **b = &commandBuffer.oldMappedBuffers[commandBuffer.currentBuffer]; *b != nullptr; b = &(*b)->next)
			{
				if ((*b)->size == appState.Scene.SceneMatrices.size && (*b)->type == appState.Scene.SceneMatrices.type)
				{
					newBuffer = *b;
					*b = (*b)->next;
					break;
				}
			}
			if (newBuffer == nullptr)
			{
				newBuffer = (Buffer *)malloc(sizeof(Buffer));
				createBuffer(commandBuffer.context, newBuffer, appState.Scene.SceneMatrices.type, appState.Scene.SceneMatrices.size, nullptr, true);
			}
			newBuffer->unusedCount = 0;
			newBuffer->next = commandBuffer.mappedBuffers[commandBuffer.currentBuffer];
			commandBuffer.mappedBuffers[commandBuffer.currentBuffer] = newBuffer;
			VK(device->vkMapMemory(commandBuffer.context->device->device, newBuffer->memory, 0, newBuffer->size, 0, &newBuffer->mapped));
			*((void **)&sceneMatrices) = newBuffer->mapped;
			Buffer *sceneMatricesBuffer = newBuffer;
			memcpy(sceneMatrices, &appState.Renderer.ViewMatrix[i], appState.Scene.NumViews * sizeof(ovrMatrix4f));
			memcpy(sceneMatrices + appState.Scene.NumViews, &appState.Renderer.ProjectionMatrix[i], appState.Scene.NumViews * sizeof(ovrMatrix4f));
			VC(device->vkUnmapMemory(commandBuffer.context->device->device, sceneMatricesBuffer->memory));
			sceneMatricesBuffer->mapped = nullptr;
			{
				VkBufferMemoryBarrier bufferMemoryBarrier { };
				bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
				bufferMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
				bufferMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				bufferMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				bufferMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				bufferMemoryBarrier.buffer = sceneMatricesBuffer->buffer;
				bufferMemoryBarrier.size = sceneMatricesBuffer->size;
				const VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_HOST_BIT;
				const VkPipelineStageFlags dst_stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
				const VkDependencyFlags flags = 0;
				VC(device->vkCmdPipelineBarrier(commandBuffer.cmdBuffers[commandBuffer.currentBuffer], src_stages, dst_stages, flags, 0, nullptr, 1, &bufferMemoryBarrier, 0, nullptr));
			}
			{
				VkBufferCopy bufferCopy;
				bufferCopy.srcOffset = 0;
				bufferCopy.dstOffset = 0;
				bufferCopy.size = appState.Scene.SceneMatrices.size;
				VC(device->vkCmdCopyBuffer(commandBuffer.cmdBuffers[commandBuffer.currentBuffer], sceneMatricesBuffer->buffer, appState.Scene.SceneMatrices.buffer, 1, &bufferCopy));
			}
			{
				VkBufferMemoryBarrier bufferMemoryBarrier { };
				bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
				bufferMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				bufferMemoryBarrier.dstAccessMask = getBufferAccess(appState.Scene.SceneMatrices.type);
				bufferMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				bufferMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				bufferMemoryBarrier.buffer = appState.Scene.SceneMatrices.buffer;
				bufferMemoryBarrier.size = appState.Scene.SceneMatrices.size;
				const VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
				const VkPipelineStageFlags dst_stages = pipelineStagesForBufferUsage(appState.Scene.SceneMatrices.type);
				const VkDependencyFlags flags = 0;
				VC(device->vkCmdPipelineBarrier(commandBuffer.cmdBuffers[commandBuffer.currentBuffer], src_stages, dst_stages, flags, 0, nullptr, 1, &bufferMemoryBarrier, 0, nullptr));
			}
			ovrMatrix4f rotationMatrices[NUM_ROTATIONS];
			for (auto j = 0; j < NUM_ROTATIONS; j++)
			{
				rotationMatrices[j] = ovrMatrix4f_CreateRotation(appState.Scene.Rotations[j].x * appState.Simulation.CurrentRotation.x, appState.Scene.Rotations[j].y * appState.Simulation.CurrentRotation.y, appState.Scene.Rotations[j].z * appState.Simulation.CurrentRotation.z);
			}
			DefaultVertexAttributeArrays attribs;
			void *data = nullptr;
			newBuffer = nullptr;
			for (Buffer **b = &commandBuffer.oldMappedBuffers[commandBuffer.currentBuffer]; *b != nullptr; b = &(*b)->next)
			{
				if ((*b)->size == appState.Scene.Cube.instanceBuffer.size && (*b)->type == appState.Scene.Cube.instanceBuffer.type)
				{
					newBuffer = *b;
					*b = (*b)->next;
					break;
				}
			}
			if (newBuffer == nullptr)
			{
				newBuffer = (Buffer *)malloc(sizeof(Buffer));
				createBuffer(commandBuffer.context, newBuffer, appState.Scene.Cube.instanceBuffer.type, appState.Scene.Cube.instanceBuffer.size, nullptr, true);
			}
			newBuffer->unusedCount = 0;
			newBuffer->next = commandBuffer.mappedBuffers[commandBuffer.currentBuffer];
			commandBuffer.mappedBuffers[commandBuffer.currentBuffer] = newBuffer;
			VK(device->vkMapMemory(commandBuffer.context->device->device, newBuffer->memory, 0, newBuffer->size, 0, &newBuffer->mapped));
			*(&data) = newBuffer->mapped;
			Buffer *buffer = newBuffer;
			attribs.base.layout = appState.Scene.Cube.layout;
			unsigned char *dataBytePtr = (unsigned char *)data;
			size_t offset = 0;
			for (auto j = 0; attribs.base.layout[j].attributeFlag != 0; j++)
			{
				const VertexAttribute *v = &attribs.base.layout[j];
				void **attribPtr = (void **)(((char *)&attribs.base) + v->attributeOffset);
				if ((v->attributeFlag & appState.Scene.Cube.instanceAttribsFlags) != 0)
				{
					*attribPtr = (dataBytePtr + offset);
					offset += appState.Scene.Cube.instanceCount * v->attributeSize;
				}
				else
				{
					*attribPtr = nullptr;
				}
			}
			Buffer *instanceBuffer = buffer;
			ovrMatrix4f *transforms = &attribs.transform[0];
			for (auto j = 0; j < NUM_INSTANCES; j++)
			{
				const int index = appState.Scene.CubeRotations[j];
				transforms[j].M[0][0] = rotationMatrices[index].M[0][0];
				transforms[j].M[0][1] = rotationMatrices[index].M[0][1];
				transforms[j].M[0][2] = rotationMatrices[index].M[0][2];
				transforms[j].M[0][3] = rotationMatrices[index].M[0][3];
				transforms[j].M[1][0] = rotationMatrices[index].M[1][0];
				transforms[j].M[1][1] = rotationMatrices[index].M[1][1];
				transforms[j].M[1][2] = rotationMatrices[index].M[1][2];
				transforms[j].M[1][3] = rotationMatrices[index].M[1][3];
				transforms[j].M[2][0] = rotationMatrices[index].M[2][0];
				transforms[j].M[2][1] = rotationMatrices[index].M[2][1];
				transforms[j].M[2][2] = rotationMatrices[index].M[2][2];
				transforms[j].M[2][3] = rotationMatrices[index].M[2][3];
				transforms[j].M[3][0] = appState.Scene.CubePositions[j].x;
				transforms[j].M[3][1] = appState.Scene.CubePositions[j].y;
				transforms[j].M[3][2] = appState.Scene.CubePositions[j].z;
				transforms[j].M[3][3] = 1.0f;
			}
			VC(device->vkUnmapMemory(commandBuffer.context->device->device, instanceBuffer->memory));
			instanceBuffer->mapped = NULL;
			{
				VkBufferMemoryBarrier bufferMemoryBarrier { };
				bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
				bufferMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
				bufferMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				bufferMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				bufferMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				bufferMemoryBarrier.buffer = instanceBuffer->buffer;
				bufferMemoryBarrier.size = instanceBuffer->size;
				const VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_HOST_BIT;
				const VkPipelineStageFlags dst_stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
				const VkDependencyFlags flags = 0;

				VC(device->vkCmdPipelineBarrier(commandBuffer.cmdBuffers[commandBuffer.currentBuffer], src_stages, dst_stages, flags, 0, nullptr, 1, &bufferMemoryBarrier, 0, nullptr));
			}
			{
				VkBufferCopy bufferCopy;
				bufferCopy.srcOffset = 0;
				bufferCopy.dstOffset = 0;
				bufferCopy.size = appState.Scene.Cube.instanceBuffer.size;
				VC(device->vkCmdCopyBuffer(commandBuffer.cmdBuffers[commandBuffer.currentBuffer], instanceBuffer->buffer, appState.Scene.Cube.instanceBuffer.buffer, 1, &bufferCopy));
			}
			{
				VkBufferMemoryBarrier bufferMemoryBarrier { };
				bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
				bufferMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				bufferMemoryBarrier.dstAccessMask = getBufferAccess(buffer->type);
				bufferMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				bufferMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				bufferMemoryBarrier.buffer = appState.Scene.Cube.instanceBuffer.buffer;
				bufferMemoryBarrier.size = appState.Scene.Cube.instanceBuffer.size;
				const VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
				const VkPipelineStageFlags dst_stages = pipelineStagesForBufferUsage(buffer->type);
				const VkDependencyFlags flags = 0;
				VC(device->vkCmdPipelineBarrier(commandBuffer.cmdBuffers[commandBuffer.currentBuffer], src_stages, dst_stages, flags, 0, nullptr, 1, &bufferMemoryBarrier, 0, nullptr));
			}
			auto cmdBuffer = commandBuffer.cmdBuffers[commandBuffer.currentBuffer];
			uint32_t clearValueCount = 0;
			VkClearValue clearValues[3] { };
			clearValues[clearValueCount].color.float32[0] = appState.Renderer.RenderPassSingleView.clearColor.x;
			clearValues[clearValueCount].color.float32[1] = appState.Renderer.RenderPassSingleView.clearColor.y;
			clearValues[clearValueCount].color.float32[2] = appState.Renderer.RenderPassSingleView.clearColor.z;
			clearValues[clearValueCount].color.float32[3] = appState.Renderer.RenderPassSingleView.clearColor.w;
			clearValueCount++;
			if (appState.Renderer.RenderPassSingleView.sampleCount > VK_SAMPLE_COUNT_1_BIT)
			{
				clearValues[clearValueCount].color.float32[0] = appState.Renderer.RenderPassSingleView.clearColor.x;
				clearValues[clearValueCount].color.float32[1] = appState.Renderer.RenderPassSingleView.clearColor.y;
				clearValues[clearValueCount].color.float32[2] = appState.Renderer.RenderPassSingleView.clearColor.z;
				clearValues[clearValueCount].color.float32[3] = appState.Renderer.RenderPassSingleView.clearColor.w;
				clearValueCount++;
			}
			if (appState.Renderer.RenderPassSingleView.internalDepthFormat != VK_FORMAT_UNDEFINED)
			{
				clearValues[clearValueCount].depthStencil.depth = 1.0f;
				clearValueCount++;
			}
			VkRenderPassBeginInfo renderPassBeginInfo { };
			renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassBeginInfo.renderPass = appState.Renderer.RenderPassSingleView.renderPass;
			renderPassBeginInfo.framebuffer = framebuffer.Framebuffer.framebuffers[framebuffer.Framebuffer.currentBuffer + framebuffer.Framebuffer.currentLayer];
			renderPassBeginInfo.renderArea = screenRect;
			renderPassBeginInfo.clearValueCount = clearValueCount;
			renderPassBeginInfo.pClearValues = clearValues;
			VkSubpassContents contents = appState.Renderer.RenderPassSingleView.type;
			VC(device->vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, contents));
			commandBuffer.currentRenderPass = &appState.Renderer.RenderPassSingleView;
			VkViewport viewport;
			viewport.x = (float) screenRect.offset.x;
			viewport.y = (float) screenRect.offset.y;
			viewport.width = (float) screenRect.extent.width;
			viewport.height = (float) screenRect.extent.height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			VC(device->vkCmdSetViewport(cmdBuffer, 0, 1, &viewport));
			VC(device->vkCmdSetScissor(cmdBuffer, 0, 1, &screenRect));
			GraphicsCommand command { };
			command.numInstances = 1;
			command.pipeline = &appState.Scene.Pipeline;
			command.parmState.parms[PROGRAM_UNIFORM_SCENE_MATRICES] = &appState.Scene.SceneMatrices;
			command.numInstances = NUM_INSTANCES;
			auto state = &commandBuffer.currentGraphicsState;
			if (command.pipeline != state->pipeline)
			{
				VC(device->vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, command.pipeline->pipeline));
			}
			const ProgramParmLayout *commandLayout = &command.pipeline->program->parmLayout;
			const ProgramParmLayout *stateLayout = (state->pipeline != nullptr) ? &state->pipeline->program->parmLayout : nullptr;
			if (!descriptorsMatch(commandLayout, &command.parmState, stateLayout, &state->parmState))
			{
				PipelineResources *resources = nullptr;
				for (PipelineResources *r = commandBuffer.pipelineResources[commandBuffer.currentBuffer]; r != nullptr; r = r->next)
				{
					if (descriptorsMatch(commandLayout, &command.parmState, r->parmLayout, &r->parms))
					{
						r->unusedCount = 0;
						resources = r;
						break;
					}
				}
				if (resources == nullptr)
				{
					resources = (PipelineResources *)malloc(sizeof(PipelineResources));
					memset(resources, 0, sizeof(PipelineResources));
					resources->parmLayout = commandLayout;
					memcpy((void *)&resources->parms, &command.parmState, sizeof(ProgramParmState));
					{
						VkDescriptorPoolSize typeCounts[MAX_PROGRAM_PARMS];
						auto count = 0;
						for (auto j = 0; j < commandLayout->numBindings; j++)
						{
							VkDescriptorType type = getDescriptorType(commandLayout->bindings[j]->type);
							for (auto k = 0; k < count; k++)
							{
								if (typeCounts[k].type == type)
								{
									typeCounts[k].descriptorCount++;
									type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
									break;
								}
							}
							if (type != VK_DESCRIPTOR_TYPE_MAX_ENUM)
							{
								typeCounts[count].type = type;
								typeCounts[count].descriptorCount = 1;
								count++;
							}
						}
						if (count == 0)
						{
							typeCounts[count].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
							typeCounts[count].descriptorCount = 1;
							count++;
						}
						VkDescriptorPoolCreateInfo descriptorPoolCreateInfo { };
						descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
						descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
						descriptorPoolCreateInfo.maxSets = 1;
						descriptorPoolCreateInfo.poolSizeCount = count;
						descriptorPoolCreateInfo.pPoolSizes = (count != 0) ? typeCounts : nullptr;
						VK(appState.Context.device->vkCreateDescriptorPool(appState.Context.device->device, &descriptorPoolCreateInfo, nullptr, &resources->descriptorPool));
					}
					{
						VkDescriptorSetAllocateInfo descriptorSetAllocateInfo { };
						descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
						descriptorSetAllocateInfo.descriptorPool = resources->descriptorPool;
						descriptorSetAllocateInfo.descriptorSetCount = 1;
						descriptorSetAllocateInfo.pSetLayouts = &commandLayout->descriptorSetLayout;
						VK(appState.Context.device->vkAllocateDescriptorSets(appState.Context.device->device, &descriptorSetAllocateInfo, &resources->descriptorSet));
						VkWriteDescriptorSet writes[MAX_PROGRAM_PARMS] { };
						VkDescriptorImageInfo imageInfo[MAX_PROGRAM_PARMS] { };
						VkDescriptorBufferInfo bufferInfo[MAX_PROGRAM_PARMS] { };
						int numWrites = 0;
						for (auto j = 0; j < commandLayout->numBindings; j++)
						{
							const ProgramParm *binding = commandLayout->bindings[j];
							writes[numWrites].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
							writes[numWrites].dstSet = resources->descriptorSet;
							writes[numWrites].dstBinding = binding->binding;
							writes[numWrites].dstArrayElement = 0;
							writes[numWrites].descriptorCount = 1;
							writes[numWrites].descriptorType = getDescriptorType(commandLayout->bindings[j]->type);
							writes[numWrites].pImageInfo = &imageInfo[numWrites];
							writes[numWrites].pBufferInfo = &bufferInfo[numWrites];
							if (binding->type == PROGRAM_PARM_TYPE_TEXTURE_SAMPLED)
							{
								const Texture *texture = (const Texture *)command.parmState.parms[binding->index];
								imageInfo[numWrites].sampler = texture->sampler;
								imageInfo[numWrites].imageView = texture->view;
								imageInfo[numWrites].imageLayout = texture->imageLayout;
							}
							else if (binding->type == PROGRAM_PARM_TYPE_TEXTURE_STORAGE)
							{
								const Texture *texture = (const Texture *)command.parmState.parms[binding->index];
								imageInfo[numWrites].sampler = VK_NULL_HANDLE;
								imageInfo[numWrites].imageView = texture->view;
								imageInfo[numWrites].imageLayout = texture->imageLayout;
							}
							else if (binding->type == PROGRAM_PARM_TYPE_BUFFER_UNIFORM)
							{
								const Buffer *buffer = (const Buffer *)command.parmState.parms[binding->index];
								bufferInfo[numWrites].buffer = buffer->buffer;
								bufferInfo[numWrites].offset = 0;
								bufferInfo[numWrites].range = buffer->size;
							}
							numWrites++;
						}
						if (numWrites > 0)
						{
							VC(appState.Context.device->vkUpdateDescriptorSets(appState.Context.device->device, numWrites, writes, 0, nullptr));
						}
					}
					resources->next = commandBuffer.pipelineResources[commandBuffer.currentBuffer];
					commandBuffer.pipelineResources[commandBuffer.currentBuffer] = resources;
				}
				VC(device->vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, commandLayout->pipelineLayout, 0, 1, &resources->descriptorSet, 0, nullptr));
			}
			for (auto j = 0; j < commandLayout->pushConstants.size(); j++)
			{
				void* data = nullptr;
				const ProgramParm *newParm = commandLayout->pushConstants[j];
				const unsigned char *newData = &command.parmState.data[commandLayout->offsetForIndex[newParm->index]];
				if (stateLayout == nullptr || j >= stateLayout->pushConstants.size())
				{
					data = (void*)newData;
				}
				const ProgramParm *oldParm = stateLayout->pushConstants[j];
				const unsigned char *oldData = &state->parmState.data[stateLayout->offsetForIndex[oldParm->index]];
				if (newParm->type != oldParm->type || newParm->binding != oldParm->binding)
				{
					data = (void*)newData;
				}
				const int pushConstantSize = getPushConstantSize(newParm->type);
				if (memcmp(newData, oldData, pushConstantSize) != 0)
				{
					data = (void*)newData;
				}
				if (data != nullptr)
				{
					const auto newParm = commandLayout->pushConstants[j];
					const VkShaderStageFlags stageFlags = ((newParm->stageFlags & PROGRAM_STAGE_FLAG_VERTEX) ? VK_SHADER_STAGE_VERTEX_BIT : 0) | ((newParm->stageFlags & PROGRAM_STAGE_FLAG_FRAGMENT) ? VK_SHADER_STAGE_FRAGMENT_BIT : 0);
					const uint32_t offset = (uint32_t)newParm->binding;
					const uint32_t size = (uint32_t)getPushConstantSize(newParm->type);
					VC(device->vkCmdPushConstants(cmdBuffer, commandLayout->pipelineLayout, stageFlags, offset, size, data));
				}
			}
			const Geometry *geometry = command.pipeline->geometry;
			if (state->pipeline == nullptr || geometry != state->pipeline->geometry || command.vertexBuffer != state->vertexBuffer || command.instanceBuffer != state->instanceBuffer)
			{
				const VkBuffer vertexBuffer = (command.vertexBuffer != nullptr) ? command.vertexBuffer->buffer : geometry->vertexBuffer.buffer;
				for (int i = 0; i < command.pipeline->firstInstanceBinding; i++)
				{
					VC(device->vkCmdBindVertexBuffers(cmdBuffer, i, 1, &vertexBuffer, &command.pipeline->vertexBindingOffsets[i]));
				}
				const VkBuffer instanceBuffer = (command.instanceBuffer != nullptr) ? command.instanceBuffer->buffer : geometry->instanceBuffer.buffer;
				for (int i = command.pipeline->firstInstanceBinding; i < command.pipeline->vertexBindingCount; i++)
				{
					VC(device->vkCmdBindVertexBuffers(cmdBuffer, i, 1, &instanceBuffer, &command.pipeline->vertexBindingOffsets[i]));
				}
				const VkIndexType indexType = (sizeof(TriangleIndex) == sizeof(unsigned int)) ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
				VC(device->vkCmdBindIndexBuffer(cmdBuffer, geometry->indexBuffer.buffer, 0, indexType));
			}
			VC(device->vkCmdDrawIndexed(cmdBuffer, geometry->indexCount, command.numInstances, 0, 0, 0));
			commandBuffer.currentGraphicsState = command;
			VC(device->vkCmdEndRenderPass(cmdBuffer));
			commandBuffer.currentRenderPass = nullptr;
			{
				const VkImageLayout newImageLayout = layoutForTextureUsage(TEXTURE_USAGE_SAMPLED);
				VkImageMemoryBarrier imageMemoryBarrier { };
				imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				imageMemoryBarrier.srcAccessMask = accessForTextureUsage(framebuffer.Framebuffer.colorTextures[commandBuffer.currentBuffer].usage);
				imageMemoryBarrier.dstAccessMask = accessForTextureUsage(TEXTURE_USAGE_SAMPLED);
				imageMemoryBarrier.oldLayout = framebuffer.Framebuffer.colorTextures[commandBuffer.currentBuffer].imageLayout;
				imageMemoryBarrier.newLayout = newImageLayout;
				imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageMemoryBarrier.image = framebuffer.Framebuffer.colorTextures[commandBuffer.currentBuffer].image;
				imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageMemoryBarrier.subresourceRange.levelCount = framebuffer.Framebuffer.colorTextures[commandBuffer.currentBuffer].mipCount;
				imageMemoryBarrier.subresourceRange.layerCount = framebuffer.Framebuffer.colorTextures[commandBuffer.currentBuffer].layerCount;
				const VkPipelineStageFlags src_stages = pipelineStagesForTextureUsage(framebuffer.Framebuffer.colorTextures[commandBuffer.currentBuffer].usage, true);
				const VkPipelineStageFlags dst_stages = pipelineStagesForTextureUsage(TEXTURE_USAGE_SAMPLED, false);
				const VkDependencyFlags flags = 0;
				VC(appState.Context.device->vkCmdPipelineBarrier(cmdBuffer, src_stages, dst_stages, flags, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
				framebuffer.Framebuffer.colorTextures[commandBuffer.currentBuffer].usage = TEXTURE_USAGE_SAMPLED;
				framebuffer.Framebuffer.colorTextures[commandBuffer.currentBuffer].imageLayout = newImageLayout;
			}
			commandBuffer.currentFramebuffer = nullptr;
			VK(device->vkEndCommandBuffer(commandBuffer.cmdBuffers[commandBuffer.currentBuffer]));
			const VkPipelineStageFlags stageFlags[1] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
			VkSubmitInfo submitInfo { };
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &commandBuffer.cmdBuffers[commandBuffer.currentBuffer];
			VK(device->vkQueueSubmit(commandBuffer.context->queue, 1, &submitInfo, fence->fence));
			fence->submitted = true;
		}
		ovrLayerProjection2 worldLayer = vrapi_DefaultLayerProjection2();
		worldLayer.HeadPose = tracking.HeadPose;
		for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++)
		{
			int eyeToSample = appState.Renderer.NumEyes == 1 ? 0 : eye;
			worldLayer.Textures[eye].ColorSwapChain = appState.Renderer.Framebuffer[eyeToSample].ColorTextureSwapChain;
			worldLayer.Textures[eye].SwapChainIndex = appState.Renderer.Framebuffer[eyeToSample].Framebuffer.currentBuffer;
			worldLayer.Textures[eye].TexCoordsFromTanAngles = ovrMatrix4f_TanAngleMatrixFromProjection(&tracking.Eye[eye].ProjectionMatrix);
		}
		auto screenLayer = vrapi_DefaultLayerCylinder2();
		const float fadeLevel = 1.0f;
		screenLayer.Header.ColorScale.x = screenLayer.Header.ColorScale.y = screenLayer.Header.ColorScale.z =
		screenLayer.Header.ColorScale.w = fadeLevel;
		screenLayer.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_SRC_ALPHA;
		screenLayer.Header.DstBlend = VRAPI_FRAME_LAYER_BLEND_ONE_MINUS_SRC_ALPHA;
		screenLayer.HeadPose = tracking.HeadPose;
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
			ovrMatrix4f modelViewMatrix = ovrMatrix4f_Multiply(&tracking.Eye[i].ViewMatrix, &cylinderTransform);
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
		const ovrLayerHeader2* layers[] = { &worldLayer.Header, &screenLayer.Header };
		ovrSubmitFrameDescription2 frameDesc = { };
		frameDesc.SwapInterval = appState.SwapInterval;
		frameDesc.FrameIndex = appState.FrameIndex;
		frameDesc.DisplayTime = appState.DisplayTime;
		frameDesc.LayerCount = 2;
		frameDesc.Layers = layers;
		vrapi_SubmitFrame2(appState.Ovr, &frameDesc);
	}
	for (auto i = 0; i < appState.Renderer.NumEyes; i++)
	{
		auto& framebuffer = appState.Renderer.Framebuffer[i];
		for (auto j = 0; j < framebuffer.TextureSwapChainLength; j++)
		{
			if (framebuffer.Framebuffer.framebuffers.size() > 0)
			{
				VC(appState.Context.device->vkDestroyFramebuffer(appState.Context.device->device, framebuffer.Framebuffer.framebuffers[j], nullptr));
			}
			if (framebuffer.Framebuffer.colorTextures.size() > 0)
			{
				VC(appState.Context.device->vkDestroySampler(appState.Context.device->device, framebuffer.Framebuffer.colorTextures[j].sampler, nullptr));
				if (framebuffer.Framebuffer.colorTextures[j].memory != VK_NULL_HANDLE)
				{
					VC(appState.Context.device->vkDestroyImageView(appState.Context.device->device, framebuffer.Framebuffer.colorTextures[j].view, nullptr));
					VC(appState.Context.device->vkDestroyImage(appState.Context.device->device, framebuffer.Framebuffer.colorTextures[j].image, nullptr));
					VC(appState.Context.device->vkFreeMemory(appState.Context.device->device, framebuffer.Framebuffer.colorTextures[j].memory, nullptr));
				}
			}
			if (framebuffer.Framebuffer.fragmentDensityTextures.size() > 0)
			{
				VC(appState.Context.device->vkDestroySampler(appState.Context.device->device, framebuffer.Framebuffer.fragmentDensityTextures[j].sampler, nullptr));
				if (framebuffer.Framebuffer.fragmentDensityTextures[j].memory != VK_NULL_HANDLE)
				{
					VC(appState.Context.device->vkDestroyImageView(appState.Context.device->device, framebuffer.Framebuffer.fragmentDensityTextures[j].view, nullptr));
					VC(appState.Context.device->vkDestroyImage(appState.Context.device->device, framebuffer.Framebuffer.fragmentDensityTextures[j].image, nullptr));
					VC(appState.Context.device->vkFreeMemory(appState.Context.device->device, framebuffer.Framebuffer.fragmentDensityTextures[j].memory, nullptr));
				}
			}
		}
		if (framebuffer.Framebuffer.depthBuffer.image != VK_NULL_HANDLE && framebuffer.Framebuffer.depthBuffer.internalFormat != VK_FORMAT_UNDEFINED)
		{
			VC(appState.Context.device->vkDestroyImageView(appState.Context.device->device, framebuffer.Framebuffer.depthBuffer.view, nullptr));
			VC(appState.Context.device->vkDestroyImage(appState.Context.device->device, framebuffer.Framebuffer.depthBuffer.image, nullptr));
			VC(appState.Context.device->vkFreeMemory(appState.Context.device->device, framebuffer.Framebuffer.depthBuffer.memory, nullptr));
		}
		if (framebuffer.Framebuffer.renderTexture.image != VK_NULL_HANDLE)
		{
			VC(appState.Context.device->vkDestroySampler(appState.Context.device->device, framebuffer.Framebuffer.renderTexture.sampler, nullptr));
			if (framebuffer.Framebuffer.renderTexture.memory != VK_NULL_HANDLE)
			{
				VC(appState.Context.device->vkDestroyImageView(appState.Context.device->device, framebuffer.Framebuffer.renderTexture.view, nullptr));
				VC(appState.Context.device->vkDestroyImage(appState.Context.device->device, framebuffer.Framebuffer.renderTexture.image, nullptr));
				VC(appState.Context.device->vkFreeMemory(appState.Context.device->device, framebuffer.Framebuffer.renderTexture.memory, nullptr));
			}
		}
		vrapi_DestroyTextureSwapChain(framebuffer.ColorTextureSwapChain);
		auto& commandBuffer = appState.Renderer.EyeCommandBuffer[i];
		for (auto j = 0; j < commandBuffer.numBuffers; j++)
		{
			VC(appState.Context.device->vkFreeCommandBuffers(appState.Context.device->device, appState.Context.commandPool, 1, &commandBuffer.cmdBuffers[j]));
			VC(appState.Context.device->vkDestroyFence(appState.Context.device->device, commandBuffer.fences[j].fence, nullptr));
			for (Buffer *b = commandBuffer.mappedBuffers[j], *next = nullptr; b != nullptr; b = next)
			{
				next = b->next;
				if (b->mapped != nullptr)
				{
					VC(appState.Context.device->vkUnmapMemory(appState.Context.device->device, b->memory));
				}
				if (b->owner)
				{
					VC(appState.Context.device->vkDestroyBuffer(appState.Context.device->device, b->buffer, nullptr));
					VC(appState.Context.device->vkFreeMemory(appState.Context.device->device, b->memory, nullptr));
				}
				free(b);
			}
			for (Buffer *b = commandBuffer.oldMappedBuffers[j], *next = nullptr; b != nullptr; b = next)
			{
				next = b->next;
				if (b->mapped != nullptr)
				{
					VC(appState.Context.device->vkUnmapMemory(appState.Context.device->device, b->memory));
				}
				if (b->owner)
				{
					VC(appState.Context.device->vkDestroyBuffer(appState.Context.device->device, b->buffer, nullptr));
					VC(appState.Context.device->vkFreeMemory(appState.Context.device->device, b->memory, nullptr));
				}
				free(b);
			}
			for (PipelineResources *r = commandBuffer.pipelineResources[j], *next = nullptr; r != nullptr; r = next)
			{
				next = r->next;
				VC(appState.Context.device->vkFreeDescriptorSets(appState.Context.device->device, r->descriptorPool, 1, &r->descriptorSet));
				VC(appState.Context.device->vkDestroyDescriptorPool(appState.Context.device->device, r->descriptorPool, nullptr));
				free(r);
			}
			commandBuffer.pipelineResources[j] = nullptr;
		}
	}
	VC(appState.Context.device->vkFreeCommandBuffers(appState.Context.device->device, appState.Context.commandPool, 1, &appState.CylinderCommandBuffer));
	VC(appState.Context.device->vkDestroyRenderPass(appState.Context.device->device, appState.Renderer.RenderPassSingleView.renderPass, nullptr));
	VK(appState.Context.device->vkQueueWaitIdle(appState.Context.queue));
	VC(appState.Context.device->vkDestroyPipeline(appState.Context.device->device, appState.Scene.Pipeline.pipeline, nullptr));
	if (appState.Scene.Cube.indexBuffer.mapped != nullptr)
	{
		VC(appState.Context.device->vkUnmapMemory(appState.Context.device->device, appState.Scene.Cube.indexBuffer.memory));
	}
	if (appState.Scene.Cube.indexBuffer.owner)
	{
		VC(appState.Context.device->vkDestroyBuffer(appState.Context.device->device, appState.Scene.Cube.indexBuffer.buffer, nullptr));
		VC(appState.Context.device->vkFreeMemory(appState.Context.device->device, appState.Scene.Cube.indexBuffer.memory, nullptr));
	}
	if (appState.Scene.Cube.vertexBuffer.mapped != nullptr)
	{
		VC(appState.Context.device->vkUnmapMemory(appState.Context.device->device, appState.Scene.Cube.vertexBuffer.memory));
	}
	if (appState.Scene.Cube.vertexBuffer.owner)
	{
		VC(appState.Context.device->vkDestroyBuffer(appState.Context.device->device, appState.Scene.Cube.vertexBuffer.buffer, nullptr));
		VC(appState.Context.device->vkFreeMemory(appState.Context.device->device, appState.Scene.Cube.vertexBuffer.memory, nullptr));
	}
	if (appState.Scene.Cube.instanceBuffer.size != 0)
	{
		if (appState.Scene.Cube.instanceBuffer.mapped != nullptr)
		{
			VC(appState.Context.device->vkUnmapMemory(appState.Context.device->device, appState.Scene.Cube.instanceBuffer.memory));
		}
		if (appState.Scene.Cube.instanceBuffer.owner)
		{
			VC(appState.Context.device->vkDestroyBuffer(appState.Context.device->device, appState.Scene.Cube.instanceBuffer.buffer, nullptr));
			VC(appState.Context.device->vkFreeMemory(appState.Context.device->device, appState.Scene.Cube.instanceBuffer.memory, nullptr));
		}
	}
	VC(appState.Context.device->vkDestroyPipelineLayout(appState.Context.device->device, appState.Scene.Program.parmLayout.pipelineLayout, nullptr));
	VC(appState.Context.device->vkDestroyDescriptorSetLayout(appState.Context.device->device, appState.Scene.Program.parmLayout.descriptorSetLayout, nullptr));
	VC(appState.Context.device->vkDestroyShaderModule(appState.Context.device->device, appState.Scene.Program.vertexShaderModule, nullptr));
	VC(appState.Context.device->vkDestroyShaderModule(appState.Context.device->device, appState.Scene.Program.fragmentShaderModule, nullptr));
	if (appState.Scene.SceneMatrices.mapped != nullptr)
	{
		VC(appState.Context.device->vkUnmapMemory(appState.Context.device->device, appState.Scene.SceneMatrices.memory));
	}
	if (appState.Scene.SceneMatrices.owner)
	{
		VC(appState.Context.device->vkDestroyBuffer(appState.Context.device->device, appState.Scene.SceneMatrices.buffer, nullptr));
		VC(appState.Context.device->vkFreeMemory(appState.Context.device->device, appState.Scene.SceneMatrices.memory, nullptr));
	}
	appState.Scene.CreatedScene = false;
	vrapi_DestroySystemVulkan();
	if (appState.Context.device != nullptr)
	{
		if (pthread_mutex_trylock(&appState.Context.device->queueFamilyMutex) == EBUSY)
		{
			pthread_mutex_lock(&appState.Context.device->queueFamilyMutex);
		}
		if ((appState.Context.device->queueFamilyUsedQueues[appState.Context.queueFamilyIndex] & (1 << appState.Context.queueIndex)) != 0)
		{
			appState.Context.device->queueFamilyUsedQueues[appState.Context.queueFamilyIndex] &= ~(1 << appState.Context.queueIndex);
			pthread_mutex_unlock(&appState.Context.device->queueFamilyMutex);
			if (appState.Context.setupCommandBuffer != nullptr)
			{
				VC(appState.Context.device->vkFreeCommandBuffers(appState.Context.device->device, appState.Context.commandPool, 1, &appState.Context.setupCommandBuffer));
			}
			VC(appState.Context.device->vkDestroyCommandPool(appState.Context.device->device, appState.Context.commandPool, nullptr));
			VC(appState.Context.device->vkDestroyPipelineCache(appState.Context.device->device, appState.Context.pipelineCache, nullptr));
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
