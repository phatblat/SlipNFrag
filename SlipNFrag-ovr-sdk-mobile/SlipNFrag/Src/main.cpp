#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <android/log.h>
#include <android/window.h>
#include <android/native_window_jni.h>
#include <android_native_app_glue.h>
#include "VrApi.h"
#include "VrApi_Vulkan.h"
#include "VrApi_Helpers.h"
#include "VrApi_SystemUtils.h"
#include "VrApi_Input.h"
#include "sys_ovr.h"

#define VK_USE_PLATFORM_ANDROID_KHR

#include <vulkan/vulkan.h>
#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <vector>

#define DEBUG 1
#define _DEBUG 1

#define OFFSETOF_MEMBER(type, member) (size_t) & ((type*)0)->member
#define SIZEOF_MEMBER(type, member) sizeof(((type*)0)->member)
#define VK(func) VkCheckErrors(func, #func);
#define VC(func) func;
#define EXPLICIT_RESOLVE 0
#define MAX_PROGRAM_PARMS 16
#define MAX_VERTEX_ATTRIBUTES 16
#define ICD_SPV_MAGIC 0x07230203
#define PROGRAM(name) name##SPIRV
#define ARRAY_SIZE(a) (sizeof((a)) / sizeof((a)[0]))
#define MAX_SAVED_PUSH_CONSTANT_BYTES 512
#define MAX_VERTEX_BUFFER_UNUSED_COUNT 16
#define MAX_PIPELINE_RESOURCES_UNUSED_COUNT 16
#define VULKAN_LIBRARY "libvulkan.so"

enum ovrVkBufferType
{
	OVR_BUFFER_TYPE_VERTEX,
	OVR_BUFFER_TYPE_INDEX,
	OVR_BUFFER_TYPE_UNIFORM
};

enum ovrDefaultVertexAttributeFlags
{
	OVR_VERTEX_ATTRIBUTE_FLAG_POSITION = 1 << 0, // vec3 vertexPosition
	OVR_VERTEX_ATTRIBUTE_FLAG_NORMAL = 1 << 1, // vec3 vertexNormal
	OVR_VERTEX_ATTRIBUTE_FLAG_TANGENT = 1 << 2, // vec3 vertexTangent
	OVR_VERTEX_ATTRIBUTE_FLAG_BINORMAL = 1 << 3, // vec3 vertexBinormal
	OVR_VERTEX_ATTRIBUTE_FLAG_COLOR = 1 << 4, // vec4 vertexColor
	OVR_VERTEX_ATTRIBUTE_FLAG_UV0 = 1 << 5, // vec2 vertexUv0
	OVR_VERTEX_ATTRIBUTE_FLAG_UV1 = 1 << 6, // vec2 vertexUv1
	OVR_VERTEX_ATTRIBUTE_FLAG_UV2 = 1 << 7, // vec2 vertexUv2
	OVR_VERTEX_ATTRIBUTE_FLAG_JOINT_INDICES = 1 << 8, // vec4 jointIndices
	OVR_VERTEX_ATTRIBUTE_FLAG_JOINT_WEIGHTS = 1 << 9, // vec4 jointWeights
	OVR_VERTEX_ATTRIBUTE_FLAG_TRANSFORM = 1
			<< 10 // mat4 vertexTransform (NOTE this mat4 takes up 4 attribute locations)
};

enum ovrVkProgramParmType
{
	OVR_PROGRAM_PARM_TYPE_TEXTURE_SAMPLED, // texture plus sampler bound together		(GLSL:
	// sampler*, isampler*, usampler*)
			OVR_PROGRAM_PARM_TYPE_TEXTURE_STORAGE, // not sampled, direct read-write storage	(GLSL:
	// image*, iimage*, uimage*)
			OVR_PROGRAM_PARM_TYPE_BUFFER_UNIFORM, // read-only uniform buffer					(GLSL:
	// uniform)
			OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_INT, // int										(GLSL: int)
	OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_INT_VECTOR2, // int[2] (GLSL: ivec2)
	OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_INT_VECTOR3, // int[3] (GLSL: ivec3)
	OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_INT_VECTOR4, // int[4] (GLSL: ivec4)
	OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT, // float									(GLSL:
	// float)
			OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_VECTOR2, // float[2] (GLSL: vec2)
	OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_VECTOR3, // float[3] (GLSL: vec3)
	OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_VECTOR4, // float[4] (GLSL: vec4)
	OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX2X2, // float[2][2]
	// (GLSL: mat2x2 or mat2)
			OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX2X3, // float[2][3] (GLSL: mat2x3)
	OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX2X4, // float[2][4] (GLSL: mat2x4)
	OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX3X2, // float[3][2] (GLSL: mat3x2)
	OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX3X3, // float[3][3]
	// (GLSL: mat3x3 or mat3)
			OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX3X4, // float[3][4] (GLSL: mat3x4)
	OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX4X2, // float[4][2] (GLSL: mat4x2)
	OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX4X3, // float[4][3] (GLSL: mat4x3)
	OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX4X4, // float[4][4]
	// (GLSL: mat4x4 or mat4)
			OVR_PROGRAM_PARM_TYPE_MAX
};

enum ovrVkProgramParmAccess
{
	OVR_PROGRAM_PARM_ACCESS_READ_ONLY,
	OVR_PROGRAM_PARM_ACCESS_WRITE_ONLY,
	OVR_PROGRAM_PARM_ACCESS_READ_WRITE
};

enum ovrVkProgramStageFlags
{
	OVR_PROGRAM_STAGE_FLAG_VERTEX = 1 << 0,
	OVR_PROGRAM_STAGE_FLAG_FRAGMENT = 1 << 1,
	OVR_PROGRAM_STAGE_MAX = 2
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

enum SurfaceColorFormat
{
	SURFACE_COLOR_FORMAT_R5G6B5,
	SURFACE_COLOR_FORMAT_B5G6R5,
	SURFACE_COLOR_FORMAT_R8G8B8A8,
	SURFACE_COLOR_FORMAT_B8G8R8A8,
	SURFACE_COLOR_FORMAT_MAX
};

enum SurfaceDepthFormat
{
	SURFACE_DEPTH_FORMAT_NONE,
	SURFACE_DEPTH_FORMAT_D16,
	SURFACE_DEPTH_FORMAT_D24,
	SURFACE_DEPTH_FORMAT_MAX
};

enum ovrVkCommandBufferType
{
	OVR_COMMAND_BUFFER_TYPE_PRIMARY,
	OVR_COMMAND_BUFFER_TYPE_SECONDARY,
	OVR_COMMAND_BUFFER_TYPE_SECONDARY_CONTINUE_RENDER_PASS
};

enum ovrVkBufferUnmapType
{
	OVR_BUFFER_UNMAP_TYPE_USE_ALLOCATED, // use the newly allocated (host visible) buffer
	OVR_BUFFER_UNMAP_TYPE_COPY_BACK // copy back to the original buffer
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

struct ovrVkVertexAttribute
{
	int attributeFlag; // OVR_VERTEX_ATTRIBUTE_FLAG_
	size_t attributeOffset; // Offset in bytes to the pointer in ovrVkVertexAttributeArrays
	size_t attributeSize; // Size in bytes of a single attribute
	VkFormat attributeFormat; // Format of the attribute
	int locationCount; // Number of attribute locations
	const char *name; // Name in vertex program
};

struct Buffer
{
	Buffer *next;
	int unusedCount;
	ovrVkBufferType type;
	size_t size;
	VkMemoryPropertyFlags flags;
	VkBuffer buffer;
	VkDeviceMemory memory;
	void *mapped;
	bool owner;
};

struct ovrVkGeometry
{
	const ovrVkVertexAttribute *layout;
	int vertexAttribsFlags;
	int instanceAttribsFlags;
	int vertexCount;
	int instanceCount;
	int indexCount;
	Buffer vertexBuffer;
	Buffer instanceBuffer;
	Buffer indexBuffer;
};

typedef unsigned short ovrTriangleIndex;

struct ovrVkVertexAttributeArrays
{
	const Buffer *buffer;
	const ovrVkVertexAttribute *layout;
	void *data;
	size_t dataSize;
	int vertexCount;
	int attribsFlags;
};

struct ovrDefaultVertexAttributeArrays
{
	ovrVkVertexAttributeArrays base;
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

struct ovrVkTriangleIndexArray
{
	const Buffer *buffer;
	ovrTriangleIndex *indexArray;
	int indexCount;
};

struct ovrVkProgramParm
{
	int stageFlags; // vertex, fragment
	ovrVkProgramParmType type; // texture, buffer or push constant
	ovrVkProgramParmAccess access; // read and/or write
	int index; // index into ovrVkProgramParmState::parms
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

struct ovrVkRenderPass
{
	VkSubpassContents type;
	SurfaceColorFormat colorFormat;
	SurfaceDepthFormat depthFormat;
	bool useFragmentDensity;
	VkSampleCountFlagBits sampleCount;
	VkFormat internalColorFormat;
	VkFormat internalDepthFormat;
	VkFormat internalFragmentDensityFormat;
	VkRenderPass renderPass;
	ovrVector4f clearColor;
};

struct Framebuffer
{
	std::vector<Texture> colorTextures;
	std::vector<Texture> fragmentDensityTextures;
	Texture renderTexture;
	DepthBuffer depthBuffer;
	std::vector<VkFramebuffer> framebuffers;
	ovrVkRenderPass *renderPass;
	int width;
	int height;
	int numLayers;
	int numBuffers;
	int currentBuffer;
	int currentLayer;
};

struct ovrVkProgramParmLayout
{
	int numParms;
	const ovrVkProgramParm *parms;
	VkDescriptorSetLayout descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	int offsetForIndex[MAX_PROGRAM_PARMS]; // push constant offsets into ovrVkProgramParmState::data
	// based on ovrVkProgramParm::index
	const ovrVkProgramParm *bindings[MAX_PROGRAM_PARMS]; // descriptor bindings
	const ovrVkProgramParm *pushConstants[MAX_PROGRAM_PARMS]; // push constants
	int numBindings;
	int numPushConstants;
	unsigned int hash;
};

struct ovrVkGraphicsProgram
{
	VkShaderModule vertexShaderModule;
	VkShaderModule fragmentShaderModule;
	VkPipelineShaderStageCreateInfo pipelineStages[2];
	ovrVkProgramParmLayout parmLayout;
	int vertexAttribsFlags;
};

struct ovrVkRasterOperations
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

struct ovrVkGraphicsPipeline
{
	ovrVkRasterOperations rop;
	const ovrVkGraphicsProgram *program;
	const ovrVkGeometry *geometry;
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

struct ovrVkGraphicsPipelineParms
{
	ovrVkRasterOperations rop;
	const ovrVkRenderPass *renderPass;
	const ovrVkGraphicsProgram *program;
	const ovrVkGeometry *geometry;
};

struct Fence
{
	VkFence fence;
	bool submitted;
};

struct ovrVkProgramParmState
{
	const void *parms[MAX_PROGRAM_PARMS];
	unsigned char data[MAX_SAVED_PUSH_CONSTANT_BYTES];
};

struct PipelineResources
{
	PipelineResources *next;
	int unusedCount; // Number of frames these resources have not been used.
	const ovrVkProgramParmLayout *parmLayout;
	ovrVkProgramParmState parms;
	VkDescriptorPool descriptorPool;
	VkDescriptorSet descriptorSet;
};

struct ovrVkGraphicsCommand
{
	const ovrVkGraphicsPipeline *pipeline;
	const Buffer *
			vertexBuffer; // vertex buffer returned by ovrVkCommandBuffer_MapVertexAttributes
	const Buffer *
			instanceBuffer; // instance buffer returned by ovrVkCommandBuffer_MapInstanceAttributes
	ovrVkProgramParmState parmState;
	int numInstances;
};

struct ovrVkCommandBuffer
{
	ovrVkCommandBufferType type;
	int numBuffers;
	int currentBuffer;
	std::vector<VkCommandBuffer> cmdBuffers;
	Context *context;
	std::vector<Fence> fences;
	std::vector<Buffer*> mappedBuffers;
	std::vector<Buffer*> oldMappedBuffers;
	std::vector<PipelineResources*> pipelineResources;
	ovrVkGraphicsCommand currentGraphicsState;
	Framebuffer *currentFramebuffer;
	ovrVkRenderPass *currentRenderPass;
};

struct ovrScreenRect
{
	int x;
	int y;
	int width;
	int height;
};

static const ovrVkVertexAttribute DefaultVertexAttributeLayout[] = {
		{OVR_VERTEX_ATTRIBUTE_FLAG_POSITION,
				OFFSETOF_MEMBER(ovrDefaultVertexAttributeArrays, position),
				SIZEOF_MEMBER(ovrDefaultVertexAttributeArrays, position[0]),
				  VK_FORMAT_R32G32B32_SFLOAT,
								1,
								   "vertexPosition"},
		{OVR_VERTEX_ATTRIBUTE_FLAG_NORMAL,
				OFFSETOF_MEMBER(ovrDefaultVertexAttributeArrays, normal),
				SIZEOF_MEMBER(ovrDefaultVertexAttributeArrays, normal[0]),
				  VK_FORMAT_R32G32B32_SFLOAT,
								1,
								   "vertexNormal"},
		{OVR_VERTEX_ATTRIBUTE_FLAG_TANGENT,
				OFFSETOF_MEMBER(ovrDefaultVertexAttributeArrays, tangent),
				SIZEOF_MEMBER(ovrDefaultVertexAttributeArrays, tangent[0]),
				  VK_FORMAT_R32G32B32_SFLOAT,
								1,
								   "vertexTangent"},
		{OVR_VERTEX_ATTRIBUTE_FLAG_BINORMAL,
				OFFSETOF_MEMBER(ovrDefaultVertexAttributeArrays, binormal),
				SIZEOF_MEMBER(ovrDefaultVertexAttributeArrays, binormal[0]),
				  VK_FORMAT_R32G32B32_SFLOAT,
								1,
								   "vertexBinormal"},
		{OVR_VERTEX_ATTRIBUTE_FLAG_COLOR,
				OFFSETOF_MEMBER(ovrDefaultVertexAttributeArrays, color),
				SIZEOF_MEMBER(ovrDefaultVertexAttributeArrays, color[0]),
				  VK_FORMAT_R32G32B32A32_SFLOAT,
								1,
								   "vertexColor"},
		{OVR_VERTEX_ATTRIBUTE_FLAG_UV0,
				OFFSETOF_MEMBER(ovrDefaultVertexAttributeArrays, uv0),
				SIZEOF_MEMBER(ovrDefaultVertexAttributeArrays, uv0[0]),
				  VK_FORMAT_R32G32_SFLOAT,
								1,
								   "vertexUv0"},
		{OVR_VERTEX_ATTRIBUTE_FLAG_UV1,
				OFFSETOF_MEMBER(ovrDefaultVertexAttributeArrays, uv1),
				SIZEOF_MEMBER(ovrDefaultVertexAttributeArrays, uv1[0]),
				  VK_FORMAT_R32G32_SFLOAT,
								1,
								   "vertexUv1"},
		{OVR_VERTEX_ATTRIBUTE_FLAG_UV2,
				OFFSETOF_MEMBER(ovrDefaultVertexAttributeArrays, uv2),
				SIZEOF_MEMBER(ovrDefaultVertexAttributeArrays, uv2[0]),
				  VK_FORMAT_R32G32_SFLOAT,
								1,
								   "vertexUv2"},
		{OVR_VERTEX_ATTRIBUTE_FLAG_JOINT_INDICES,
				OFFSETOF_MEMBER(ovrDefaultVertexAttributeArrays, jointIndices),
				SIZEOF_MEMBER(ovrDefaultVertexAttributeArrays, jointIndices[0]),
				  VK_FORMAT_R32G32B32A32_SFLOAT,
								1,
								   "vertexJointIndices"},
		{OVR_VERTEX_ATTRIBUTE_FLAG_JOINT_WEIGHTS,
				OFFSETOF_MEMBER(ovrDefaultVertexAttributeArrays, jointWeights),
				SIZEOF_MEMBER(ovrDefaultVertexAttributeArrays, jointWeights[0]),
				  VK_FORMAT_R32G32B32A32_SFLOAT,
								1,
								   "vertexJointWeights"},
		{OVR_VERTEX_ATTRIBUTE_FLAG_TRANSFORM,
				OFFSETOF_MEMBER(ovrDefaultVertexAttributeArrays, transform),
				SIZEOF_MEMBER(ovrDefaultVertexAttributeArrays, transform[0]),
				  VK_FORMAT_R32G32B32A32_SFLOAT,
								4,
								   "vertexTransform"},
		{0, 0, 0, (VkFormat) 0, 0, ""}};

static const VkQueueFlags requiredQueueFlags = VK_QUEUE_GRAPHICS_BIT;

static const int queueCount = 1;

size_t ovrGpuVertexAttributeArrays_GetDataSize(
		const ovrVkVertexAttribute *layout,
		const int vertexCount,
		const int attribsFlags)
{
	size_t totalSize = 0;
	for (int i = 0; layout[i].attributeFlag != 0; i++)
	{
		const ovrVkVertexAttribute *v = &layout[i];
		if ((v->attributeFlag & attribsFlags) != 0)
		{
			totalSize += v->attributeSize;
		}
	}
	return vertexCount * totalSize;
}

void ovrVkVertexAttributeArrays_Map(
		ovrVkVertexAttributeArrays *attribs,
		void *data,
		const size_t dataSize,
		const int vertexCount,
		const int attribsFlags)
{
	unsigned char *dataBytePtr = (unsigned char *) data;
	size_t offset = 0;

	for (int i = 0; attribs->layout[i].attributeFlag != 0; i++)
	{
		const ovrVkVertexAttribute *v = &attribs->layout[i];
		void **attribPtr = (void **) (((char *) attribs) + v->attributeOffset);
		if ((v->attributeFlag & attribsFlags) != 0)
		{
			*attribPtr = (dataBytePtr + offset);
			offset += vertexCount * v->attributeSize;
		}
		else
		{
			*attribPtr = NULL;
		}
	}

	assert(offset == dataSize);
}

void ovrVkVertexAttributeArrays_Alloc(
		ovrVkVertexAttributeArrays *attribs,
		const ovrVkVertexAttribute *layout,
		const int vertexCount,
		const int attribsFlags)
{
	const size_t dataSize =
			ovrGpuVertexAttributeArrays_GetDataSize(layout, vertexCount, attribsFlags);
	void *data = malloc(dataSize);
	attribs->buffer = NULL;
	attribs->layout = layout;
	attribs->data = data;
	attribs->dataSize = dataSize;
	attribs->vertexCount = vertexCount;
	attribs->attribsFlags = attribsFlags;
	ovrVkVertexAttributeArrays_Map(attribs, data, dataSize, vertexCount, attribsFlags);
}

void ovrVkTriangleIndexArray_Alloc(
		ovrVkTriangleIndexArray *indices,
		const int indexCount,
		const ovrTriangleIndex *data)
{
	indices->indexCount = indexCount;
	indices->indexArray = (unsigned short *) malloc(indexCount * sizeof(unsigned short));
	if (data != NULL)
	{
		memcpy(indices->indexArray, data, indexCount * sizeof(unsigned short));
	}
	indices->buffer = NULL;
}

void ovrGpuBuffer_CreateReference(
		Context *context,
		Buffer *buffer,
		const Buffer *other)
{
	buffer->next = NULL;
	buffer->unusedCount = 0;
	buffer->type = other->type;
	buffer->size = other->size;
	buffer->flags = other->flags;
	buffer->buffer = other->buffer;
	buffer->memory = other->memory;
	buffer->mapped = NULL;
	buffer->owner = false;
}

static VkBufferUsageFlags ovrGpuBuffer_GetBufferUsage(const ovrVkBufferType type)
{
	return (
			(type == OVR_BUFFER_TYPE_VERTEX)
			? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
			: ((type == OVR_BUFFER_TYPE_INDEX)
			   ? VK_BUFFER_USAGE_INDEX_BUFFER_BIT
			   : ((type == OVR_BUFFER_TYPE_UNIFORM) ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : 0)));
}

static const char *VkErrorString(VkResult result)
{
	switch (result)
	{
		case VK_SUCCESS:
			return "VK_SUCCESS";
		case VK_NOT_READY:
			return "VK_NOT_READY";
		case VK_TIMEOUT:
			return "VK_TIMEOUT";
		case VK_EVENT_SET:
			return "VK_EVENT_SET";
		case VK_EVENT_RESET:
			return "VK_EVENT_RESET";
		case VK_INCOMPLETE:
			return "VK_INCOMPLETE";
		case VK_ERROR_OUT_OF_HOST_MEMORY:
			return "VK_ERROR_OUT_OF_HOST_MEMORY";
		case VK_ERROR_OUT_OF_DEVICE_MEMORY:
			return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
		case VK_ERROR_INITIALIZATION_FAILED:
			return "VK_ERROR_INITIALIZATION_FAILED";
		case VK_ERROR_DEVICE_LOST:
			return "VK_ERROR_DEVICE_LOST";
		case VK_ERROR_MEMORY_MAP_FAILED:
			return "VK_ERROR_MEMORY_MAP_FAILED";
		case VK_ERROR_LAYER_NOT_PRESENT:
			return "VK_ERROR_LAYER_NOT_PRESENT";
		case VK_ERROR_EXTENSION_NOT_PRESENT:
			return "VK_ERROR_EXTENSION_NOT_PRESENT";
		case VK_ERROR_FEATURE_NOT_PRESENT:
			return "VK_ERROR_FEATURE_NOT_PRESENT";
		case VK_ERROR_INCOMPATIBLE_DRIVER:
			return "VK_ERROR_INCOMPATIBLE_DRIVER";
		case VK_ERROR_TOO_MANY_OBJECTS:
			return "VK_ERROR_TOO_MANY_OBJECTS";
		case VK_ERROR_FORMAT_NOT_SUPPORTED:
			return "VK_ERROR_FORMAT_NOT_SUPPORTED";
		case VK_ERROR_SURFACE_LOST_KHR:
			return "VK_ERROR_SURFACE_LOST_KHR";
		case VK_SUBOPTIMAL_KHR:
			return "VK_SUBOPTIMAL_KHR";
		case VK_ERROR_OUT_OF_DATE_KHR:
			return "VK_ERROR_OUT_OF_DATE_KHR";
		case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
			return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
		case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
			return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
		case VK_ERROR_VALIDATION_FAILED_EXT:
			return "VK_ERROR_VALIDATION_FAILED_EXT";
		default:
		{
			if (result == VK_ERROR_INVALID_SHADER_NV)
			{
				return "VK_ERROR_INVALID_SHADER_NV";
			}
			return "unknown";
		}
	}
}

static void VkCheckErrors(VkResult result, const char *function)
{
	if (result != VK_SUCCESS)
	{
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Vulkan error: %s: %s\n", function, VkErrorString(result));
	}
}

static uint32_t VkGetMemoryTypeIndex(
		Device *device,
		const uint32_t typeBits,
		const VkMemoryPropertyFlags requiredProperties)
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

bool ovrVkBuffer_Create(
		Context *context,
		Buffer *buffer,
		const ovrVkBufferType type,
		const size_t dataSize,
		const void *data,
		const bool hostVisible)
{
	memset(buffer, 0, sizeof(Buffer));

	assert(
			dataSize <=
			context->device->physicalDeviceProperties.properties.limits.maxStorageBufferRange);

	buffer->type = type;
	buffer->size = dataSize;
	buffer->owner = true;

	VkBufferCreateInfo bufferCreateInfo;
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.pNext = NULL;
	bufferCreateInfo.flags = 0;
	bufferCreateInfo.size = dataSize;
	bufferCreateInfo.usage = ovrGpuBuffer_GetBufferUsage(type) |
							 (hostVisible ? VK_BUFFER_USAGE_TRANSFER_SRC_BIT : VK_BUFFER_USAGE_TRANSFER_DST_BIT);
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
	memoryAllocateInfo.memoryTypeIndex =
			VkGetMemoryTypeIndex(context->device, memoryRequirements.memoryTypeBits, buffer->flags);

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
			stagingMemoryAllocateInfo.memoryTypeIndex = VkGetMemoryTypeIndex(
					context->device,
					stagingMemoryRequirements.memoryTypeBits,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

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

void ovrVkGeometry_Create(
		Context *context,
		ovrVkGeometry *geometry,
		const ovrVkVertexAttributeArrays *attribs,
		const ovrVkTriangleIndexArray *indices)
{
	memset(geometry, 0, sizeof(ovrVkGeometry));

	geometry->layout = attribs->layout;
	geometry->vertexAttribsFlags = attribs->attribsFlags;
	geometry->vertexCount = attribs->vertexCount;
	geometry->indexCount = indices->indexCount;

	if (attribs->buffer != NULL)
	{
		ovrGpuBuffer_CreateReference(context, &geometry->vertexBuffer, attribs->buffer);
	}
	else
	{
		ovrVkBuffer_Create(
				context,
				&geometry->vertexBuffer,
				OVR_BUFFER_TYPE_VERTEX,
				attribs->dataSize,
				attribs->data,
				false);
	}
	if (indices->buffer != NULL)
	{
		ovrGpuBuffer_CreateReference(context, &geometry->indexBuffer, indices->buffer);
	}
	else
	{
		ovrVkBuffer_Create(
				context,
				&geometry->indexBuffer,
				OVR_BUFFER_TYPE_INDEX,
				indices->indexCount * sizeof(indices->indexArray[0]),
				indices->indexArray,
				false);
	}
}

void ovrVkVertexAttributeArrays_Free(ovrVkVertexAttributeArrays *attribs)
{
	free(attribs->data);
	memset(attribs, 0, sizeof(ovrVkVertexAttributeArrays));
}

void ovrVkTriangleIndexArray_Free(ovrVkTriangleIndexArray *indices)
{
	free(indices->indexArray);
	memset(indices, 0, sizeof(unsigned short));
}

void ovrVkBuffer_Destroy(Device *device, Buffer *buffer)
{
	if (buffer->mapped != NULL)
	{
		VC(device->vkUnmapMemory(device->device, buffer->memory));
	}
	if (buffer->owner)
	{
		VC(device->vkDestroyBuffer(device->device, buffer->buffer, nullptr));
		VC(device->vkFreeMemory(device->device, buffer->memory, nullptr));
	}
}

void ovrVkGeometry_Destroy(Device *device, ovrVkGeometry *geometry)
{
	ovrVkBuffer_Destroy(device, &geometry->indexBuffer);
	ovrVkBuffer_Destroy(device, &geometry->vertexBuffer);
	if (geometry->instanceBuffer.size != 0)
	{
		ovrVkBuffer_Destroy(device, &geometry->instanceBuffer);
	}

	memset(geometry, 0, sizeof(ovrVkGeometry));
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

static int IntegerLog2(int i)
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

static VkImageLayout LayoutForTextureUsage(const TextureUsage usage)
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

static VkAccessFlags accessForTextureUsage(const TextureUsage usage)
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

void ovrVkTexture_ChangeUsage(
		Context *context,
		VkCommandBuffer cmdBuffer,
		Texture *texture,
		const TextureUsage usage)
{
	assert((texture->usageFlags & usage) != 0);

	const VkImageLayout newImageLayout = LayoutForTextureUsage(usage);

	VkImageMemoryBarrier imageMemoryBarrier;
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.pNext = NULL;
	imageMemoryBarrier.srcAccessMask = accessForTextureUsage(texture->usage);
	imageMemoryBarrier.dstAccessMask = accessForTextureUsage(usage);
	imageMemoryBarrier.oldLayout = texture->imageLayout;
	imageMemoryBarrier.newLayout = newImageLayout;
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.image = texture->image;
	imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	imageMemoryBarrier.subresourceRange.levelCount = texture->mipCount;
	imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	imageMemoryBarrier.subresourceRange.layerCount = texture->layerCount;

	const VkPipelineStageFlags src_stages = pipelineStagesForTextureUsage(texture->usage, true);
	const VkPipelineStageFlags dst_stages = pipelineStagesForTextureUsage(usage, false);
	const VkDependencyFlags flags = 0;

	VC(context->device->vkCmdPipelineBarrier(
			cmdBuffer, src_stages, dst_stages, flags, 0, NULL, 0, NULL, 1, &imageMemoryBarrier));

	texture->usage = usage;
	texture->imageLayout = newImageLayout;
}

void ovrVkTexture_Destroy(Context *context, Texture *texture)
{
	VC(context->device->vkDestroySampler(context->device->device, texture->sampler, nullptr));
	// A texture created from a swapchain does not own the view, image or memory.
	if (texture->memory != VK_NULL_HANDLE)
	{
		VC(context->device->vkDestroyImageView(
				context->device->device, texture->view, nullptr));
		VC(context->device->vkDestroyImage(context->device->device, texture->image, nullptr));
		VC(context->device->vkFreeMemory(context->device->device, texture->memory, nullptr));
	}
	memset(texture, 0, sizeof(Texture));
}

void ovrVkDepthBuffer_Destroy(Context *context, DepthBuffer *depthBuffer)
{
	if (depthBuffer->internalFormat == VK_FORMAT_UNDEFINED)
	{
		return;
	}

	VC(context->device->vkDestroyImageView(
			context->device->device, depthBuffer->view, nullptr));
	VC(context->device->vkDestroyImage(context->device->device, depthBuffer->image, nullptr));
	VC(context->device->vkFreeMemory(context->device->device, depthBuffer->memory, nullptr));
}

void ovrVkGeometry_AddInstanceAttributes(
		Context *context,
		ovrVkGeometry *geometry,
		const int numInstances,
		const int instanceAttribsFlags)
{
	assert(geometry->layout != NULL);
	assert((geometry->vertexAttribsFlags & instanceAttribsFlags) == 0);

	geometry->instanceCount = numInstances;
	geometry->instanceAttribsFlags = instanceAttribsFlags;

	const size_t dataSize = ovrGpuVertexAttributeArrays_GetDataSize(
			geometry->layout, numInstances, geometry->instanceAttribsFlags);

	ovrVkBuffer_Create(
			context, &geometry->instanceBuffer, OVR_BUFFER_TYPE_VERTEX, dataSize, NULL, false);
}

void ovrGpuDevice_CreateShader(
		Device *device,
		VkShaderModule *shaderModule,
		const VkShaderStageFlagBits stage,
		const void *code,
		size_t codeSize)
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

static bool ovrGpuProgramParm_IsOpaqueBinding(const ovrVkProgramParmType type)
{
	return (
			(type == OVR_PROGRAM_PARM_TYPE_TEXTURE_SAMPLED)
			? true
			: ((type == OVR_PROGRAM_PARM_TYPE_TEXTURE_STORAGE)
			   ? true
			   : ((type == OVR_PROGRAM_PARM_TYPE_BUFFER_UNIFORM) ? true : false)));
}

static VkDescriptorType ovrGpuProgramParm_GetDescriptorType(const ovrVkProgramParmType type)
{
	return (
			(type == OVR_PROGRAM_PARM_TYPE_TEXTURE_SAMPLED)
			? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
			: ((type == OVR_PROGRAM_PARM_TYPE_TEXTURE_STORAGE)
			   ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
			   : ((type == OVR_PROGRAM_PARM_TYPE_BUFFER_UNIFORM)
				  ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
				  : VK_DESCRIPTOR_TYPE_MAX_ENUM)));
}

int ovrVkProgramParm_GetPushConstantSize(ovrVkProgramParmType type)
{
	static const int parmSize[OVR_PROGRAM_PARM_TYPE_MAX] = {(unsigned int) 0,
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
	assert((sizeof(parmSize) / sizeof(parmSize[0])) == OVR_PROGRAM_PARM_TYPE_MAX);
	return parmSize[type];
}

static VkShaderStageFlags ovrGpuProgramParm_GetShaderStageFlags(const int stageFlags)
{
	return ((stageFlags & OVR_PROGRAM_STAGE_FLAG_VERTEX) ? VK_SHADER_STAGE_VERTEX_BIT : 0) |
		   ((stageFlags & OVR_PROGRAM_STAGE_FLAG_FRAGMENT) ? VK_SHADER_STAGE_FRAGMENT_BIT : 0);
}

void ovrVkProgramParmLayout_Create(
		Context *context,
		ovrVkProgramParmLayout *layout,
		const ovrVkProgramParm *parms,
		const int numParms)
{
	memset(layout, 0, sizeof(ovrVkProgramParmLayout));

	layout->numParms = numParms;
	layout->parms = parms;

	int numSampledTextureBindings[OVR_PROGRAM_STAGE_MAX] = {0};
	int numStorageTextureBindings[OVR_PROGRAM_STAGE_MAX] = {0};
	int numUniformBufferBindings[OVR_PROGRAM_STAGE_MAX] = {0};
	int numStorageBufferBindings[OVR_PROGRAM_STAGE_MAX] = {0};

	int offset = 0;
	memset(layout->offsetForIndex, -1, sizeof(layout->offsetForIndex));

	for (int i = 0; i < numParms; i++)
	{
		if (parms[i].type == OVR_PROGRAM_PARM_TYPE_TEXTURE_SAMPLED ||
			parms[i].type == OVR_PROGRAM_PARM_TYPE_TEXTURE_STORAGE ||
			parms[i].type == OVR_PROGRAM_PARM_TYPE_BUFFER_UNIFORM)
		{
			for (int stageIndex = 0; stageIndex < OVR_PROGRAM_STAGE_MAX; stageIndex++)
			{
				if ((parms[i].stageFlags & (1 << stageIndex)) != 0)
				{
					numSampledTextureBindings[stageIndex] +=
							(parms[i].type == OVR_PROGRAM_PARM_TYPE_TEXTURE_SAMPLED);
					numStorageTextureBindings[stageIndex] +=
							(parms[i].type == OVR_PROGRAM_PARM_TYPE_TEXTURE_STORAGE);
					numUniformBufferBindings[stageIndex] +=
							(parms[i].type == OVR_PROGRAM_PARM_TYPE_BUFFER_UNIFORM);
				}
			}

			assert(parms[i].binding >= 0 && parms[i].binding < MAX_PROGRAM_PARMS);

			// Make sure each binding location is only used once.
			assert(layout->bindings[parms[i].binding] == NULL);

			layout->bindings[parms[i].binding] = &parms[i];
			if ((int) parms[i].binding >= layout->numBindings)
			{
				layout->numBindings = (int) parms[i].binding + 1;
			}
		}
		else
		{
			assert(layout->numPushConstants < MAX_PROGRAM_PARMS);
			layout->pushConstants[layout->numPushConstants++] = &parms[i];

			layout->offsetForIndex[parms[i].index] = offset;
			offset += ovrVkProgramParm_GetPushConstantSize(parms[i].type);
		}
	}

	// Make sure the descriptor bindings are packed.
	for (int binding = 0; binding < layout->numBindings; binding++)
	{
		assert(layout->bindings[binding] != NULL);
	}

	// Verify the push constant layout.
	for (int push0 = 0; push0 < layout->numPushConstants; push0++)
	{
		// The push constants for a pipeline cannot use more than 'maxPushConstantsSize' bytes.
		assert(
				layout->pushConstants[push0]->binding +
				ovrVkProgramParm_GetPushConstantSize(layout->pushConstants[push0]->type) <=
				(int) context->device->physicalDeviceProperties.properties.limits.maxPushConstantsSize);
		// Make sure no push constants overlap.
		for (int push1 = push0 + 1; push1 < layout->numPushConstants; push1++)
		{
			assert(
					layout->pushConstants[push0]->binding >= layout->pushConstants[push1]->binding +
															 ovrVkProgramParm_GetPushConstantSize(layout->pushConstants[push1]->type) ||
					layout->pushConstants[push0]->binding +
					ovrVkProgramParm_GetPushConstantSize(layout->pushConstants[push0]->type) <=
					layout->pushConstants[push1]->binding);
		}
	}

	// Check the descriptor limits.
	int numTotalSampledTextureBindings = 0;
	int numTotalStorageTextureBindings = 0;
	int numTotalUniformBufferBindings = 0;
	int numTotalStorageBufferBindings = 0;
	for (int stage = 0; stage < OVR_PROGRAM_STAGE_MAX; stage++)
	{
		assert(
				numSampledTextureBindings[stage] <=
				(int) context->device->physicalDeviceProperties.properties.limits
						.maxPerStageDescriptorSampledImages);
		assert(
				numStorageTextureBindings[stage] <=
				(int) context->device->physicalDeviceProperties.properties.limits
						.maxPerStageDescriptorStorageImages);
		assert(
				numUniformBufferBindings[stage] <=
				(int) context->device->physicalDeviceProperties.properties.limits
						.maxPerStageDescriptorUniformBuffers);
		assert(
				numStorageBufferBindings[stage] <=
				(int) context->device->physicalDeviceProperties.properties.limits
						.maxPerStageDescriptorStorageBuffers);

		numTotalSampledTextureBindings += numSampledTextureBindings[stage];
		numTotalStorageTextureBindings += numStorageTextureBindings[stage];
		numTotalUniformBufferBindings += numUniformBufferBindings[stage];
		numTotalStorageBufferBindings += numStorageBufferBindings[stage];
	}

	assert(
			numTotalSampledTextureBindings <= (int) context->device->physicalDeviceProperties.properties
					.limits.maxDescriptorSetSampledImages);
	assert(
			numTotalStorageTextureBindings <= (int) context->device->physicalDeviceProperties.properties
					.limits.maxDescriptorSetStorageImages);
	assert(
			numTotalUniformBufferBindings <= (int) context->device->physicalDeviceProperties.properties
					.limits.maxDescriptorSetUniformBuffers);
	assert(
			numTotalStorageBufferBindings <= (int) context->device->physicalDeviceProperties.properties
					.limits.maxDescriptorSetStorageBuffers);

	//
	// Create descriptor set layout and pipeline layout
	//

	{
		VkDescriptorSetLayoutBinding descriptorSetBindings[MAX_PROGRAM_PARMS];
		VkPushConstantRange pushConstantRanges[MAX_PROGRAM_PARMS];

		int numDescriptorSetBindings = 0;
		int numPushConstantRanges = 0;
		for (int i = 0; i < numParms; i++)
		{
			if (ovrGpuProgramParm_IsOpaqueBinding(parms[i].type))
			{
				descriptorSetBindings[numDescriptorSetBindings].binding = parms[i].binding;
				descriptorSetBindings[numDescriptorSetBindings].descriptorType =
						ovrGpuProgramParm_GetDescriptorType(parms[i].type);
				descriptorSetBindings[numDescriptorSetBindings].descriptorCount = 1;
				descriptorSetBindings[numDescriptorSetBindings].stageFlags =
						ovrGpuProgramParm_GetShaderStageFlags(parms[i].stageFlags);
				descriptorSetBindings[numDescriptorSetBindings].pImmutableSamplers = NULL;
				numDescriptorSetBindings++;
			}
			else // push constant
			{
				pushConstantRanges[numPushConstantRanges].stageFlags =
						ovrGpuProgramParm_GetShaderStageFlags(parms[i].stageFlags);
				pushConstantRanges[numPushConstantRanges].offset = parms[i].binding;
				pushConstantRanges[numPushConstantRanges].size =
						ovrVkProgramParm_GetPushConstantSize(parms[i].type);
				numPushConstantRanges++;
			}
		}

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
		descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutCreateInfo.pNext = NULL;
		descriptorSetLayoutCreateInfo.flags = 0;
		descriptorSetLayoutCreateInfo.bindingCount = numDescriptorSetBindings;
		descriptorSetLayoutCreateInfo.pBindings =
				(numDescriptorSetBindings != 0) ? descriptorSetBindings : NULL;

		VK(context->device->vkCreateDescriptorSetLayout(
				context->device->device,
				&descriptorSetLayoutCreateInfo,
				nullptr,
				&layout->descriptorSetLayout));

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.pNext = NULL;
		pipelineLayoutCreateInfo.flags = 0;
		pipelineLayoutCreateInfo.setLayoutCount = 1;
		pipelineLayoutCreateInfo.pSetLayouts = &layout->descriptorSetLayout;
		pipelineLayoutCreateInfo.pushConstantRangeCount = numPushConstantRanges;
		pipelineLayoutCreateInfo.pPushConstantRanges =
				(numPushConstantRanges != 0) ? pushConstantRanges : NULL;

		VK(context->device->vkCreatePipelineLayout(
				context->device->device,
				&pipelineLayoutCreateInfo,
				nullptr,
				&layout->pipelineLayout));
	}

	// Calculate a hash of the layout.
	unsigned int hash = 5381;
	for (int i = 0; i < numParms * (int) sizeof(parms[0]); i++)
	{
		hash = ((hash << 5) - hash) + ((const char *) parms)[i];
	}
	layout->hash = hash;
}

bool ovrVkGraphicsProgram_Create(
		Context *context,
		ovrVkGraphicsProgram *program,
		const void *vertexSourceData,
		const size_t vertexSourceSize,
		const void *fragmentSourceData,
		const size_t fragmentSourceSize,
		const ovrVkProgramParm *parms,
		const int numParms,
		const ovrVkVertexAttribute *vertexLayout,
		const int vertexAttribsFlags)
{
	program->vertexAttribsFlags = vertexAttribsFlags;

	ovrGpuDevice_CreateShader(
			context->device,
			&program->vertexShaderModule,
			VK_SHADER_STAGE_VERTEX_BIT,
			vertexSourceData,
			vertexSourceSize);
	ovrGpuDevice_CreateShader(
			context->device,
			&program->fragmentShaderModule,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			fragmentSourceData,
			fragmentSourceSize);

	program->pipelineStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	program->pipelineStages[0].pNext = NULL;
	program->pipelineStages[0].flags = 0;
	program->pipelineStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	program->pipelineStages[0].module = program->vertexShaderModule;
	program->pipelineStages[0].pName = "main";
	program->pipelineStages[0].pSpecializationInfo = NULL;

	program->pipelineStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	program->pipelineStages[1].pNext = NULL;
	program->pipelineStages[1].flags = 0;
	program->pipelineStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	program->pipelineStages[1].module = program->fragmentShaderModule;
	program->pipelineStages[1].pName = "main";
	program->pipelineStages[1].pSpecializationInfo = NULL;

	ovrVkProgramParmLayout_Create(context, &program->parmLayout, parms, numParms);

	return true;
}

Buffer *ovrVkCommandBuffer_MapBuffer(ovrVkCommandBuffer *commandBuffer, Buffer *buffer, void **data)
{
	assert(commandBuffer->currentRenderPass == NULL);

	Device *device = commandBuffer->context->device;

	Buffer *newBuffer = NULL;
	for (Buffer **b = &commandBuffer->oldMappedBuffers[commandBuffer->currentBuffer];
		 *b != NULL;
		 b = &(*b)->next)
	{
		if ((*b)->size == buffer->size && (*b)->type == buffer->type)
		{
			newBuffer = *b;
			*b = (*b)->next;
			break;
		}
	}
	if (newBuffer == NULL)
	{
		newBuffer = (Buffer *) malloc(sizeof(Buffer));
		ovrVkBuffer_Create(
				commandBuffer->context, newBuffer, buffer->type, buffer->size, NULL, true);
	}

	newBuffer->unusedCount = 0;
	newBuffer->next = commandBuffer->mappedBuffers[commandBuffer->currentBuffer];
	commandBuffer->mappedBuffers[commandBuffer->currentBuffer] = newBuffer;

	assert(newBuffer->mapped == NULL);
	VK(device->vkMapMemory(
			commandBuffer->context->device->device,
			newBuffer->memory,
			0,
			newBuffer->size,
			0,
			&newBuffer->mapped));

	*data = newBuffer->mapped;

	return newBuffer;
}

static VkAccessFlags ovrGpuBuffer_GetBufferAccess(const ovrVkBufferType type)
{
	return (
			(type == OVR_BUFFER_TYPE_INDEX)
			? VK_ACCESS_INDEX_READ_BIT
			: ((type == OVR_BUFFER_TYPE_VERTEX)
			   ? VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
			   : ((type == OVR_BUFFER_TYPE_UNIFORM) ? VK_ACCESS_UNIFORM_READ_BIT : 0)));
}

static VkPipelineStageFlags PipelineStagesForBufferUsage(const ovrVkBufferType type)
{
	return (
			(type == OVR_BUFFER_TYPE_INDEX)
			? VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT
			: ((type == OVR_BUFFER_TYPE_VERTEX)
			   ? VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
			   : ((type == OVR_BUFFER_TYPE_UNIFORM) ? VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT : 0)));
}

void ovrVkCommandBuffer_UnmapBuffer(
		ovrVkCommandBuffer *commandBuffer,
		Buffer *buffer,
		Buffer *mappedBuffer,
		const ovrVkBufferUnmapType type)
{
	// Can only copy or issue memory barrier outside a render pass.
	assert(commandBuffer->currentRenderPass == NULL);

	Device *device = commandBuffer->context->device;

	VC(device->vkUnmapMemory(commandBuffer->context->device->device, mappedBuffer->memory));
	mappedBuffer->mapped = NULL;

	// Optionally copy the mapped buffer back to the original buffer. While the copy is not for
	// free, there may be a performance benefit from using the original buffer if it lives in device
	// local memory.
	if (type == OVR_BUFFER_UNMAP_TYPE_COPY_BACK)
	{
		assert(buffer->size == mappedBuffer->size);

		{
			// Add a memory barrier for the mapped buffer from host write to DMA read.
			VkBufferMemoryBarrier bufferMemoryBarrier;
			bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			bufferMemoryBarrier.pNext = NULL;
			bufferMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
			bufferMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			bufferMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferMemoryBarrier.buffer = mappedBuffer->buffer;
			bufferMemoryBarrier.offset = 0;
			bufferMemoryBarrier.size = mappedBuffer->size;

			const VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_HOST_BIT;
			const VkPipelineStageFlags dst_stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
			const VkDependencyFlags flags = 0;

			VC(device->vkCmdPipelineBarrier(
					commandBuffer->cmdBuffers[commandBuffer->currentBuffer],
					src_stages,
					dst_stages,
					flags,
					0,
					NULL,
					1,
					&bufferMemoryBarrier,
					0,
					NULL));
		}

		{
			// Copy back to the original buffer.
			VkBufferCopy bufferCopy;
			bufferCopy.srcOffset = 0;
			bufferCopy.dstOffset = 0;
			bufferCopy.size = buffer->size;

			VC(device->vkCmdCopyBuffer(
					commandBuffer->cmdBuffers[commandBuffer->currentBuffer],
					mappedBuffer->buffer,
					buffer->buffer,
					1,
					&bufferCopy));
		}

		{
			// Add a memory barrier for the original buffer from DMA write to the buffer access.
			VkBufferMemoryBarrier bufferMemoryBarrier;
			bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			bufferMemoryBarrier.pNext = NULL;
			bufferMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			bufferMemoryBarrier.dstAccessMask = ovrGpuBuffer_GetBufferAccess(buffer->type);
			bufferMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferMemoryBarrier.buffer = buffer->buffer;
			bufferMemoryBarrier.offset = 0;
			bufferMemoryBarrier.size = buffer->size;

			const VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
			const VkPipelineStageFlags dst_stages = PipelineStagesForBufferUsage(buffer->type);
			const VkDependencyFlags flags = 0;

			VC(device->vkCmdPipelineBarrier(
					commandBuffer->cmdBuffers[commandBuffer->currentBuffer],
					src_stages,
					dst_stages,
					flags,
					0,
					NULL,
					1,
					&bufferMemoryBarrier,
					0,
					NULL));
		}
	}
	else
	{
		{
			// Add a memory barrier for the mapped buffer from host write to buffer access.
			VkBufferMemoryBarrier bufferMemoryBarrier;
			bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			bufferMemoryBarrier.pNext = NULL;
			bufferMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
			bufferMemoryBarrier.dstAccessMask = ovrGpuBuffer_GetBufferAccess(mappedBuffer->type);
			bufferMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferMemoryBarrier.buffer = mappedBuffer->buffer;
			bufferMemoryBarrier.offset = 0;
			bufferMemoryBarrier.size = mappedBuffer->size;

			const VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			const VkPipelineStageFlags dst_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			const VkDependencyFlags flags = 0;

			VC(device->vkCmdPipelineBarrier(
					commandBuffer->cmdBuffers[commandBuffer->currentBuffer],
					src_stages,
					dst_stages,
					flags,
					0,
					NULL,
					1,
					&bufferMemoryBarrier,
					0,
					NULL));
		}
	}
}

void ovrVkCommandBuffer_UnmapInstanceAttributes(
		ovrVkCommandBuffer *commandBuffer,
		ovrVkGeometry *geometry,
		Buffer *mappedInstanceBuffer,
		const ovrVkBufferUnmapType type)
{
	ovrVkCommandBuffer_UnmapBuffer(
			commandBuffer, &geometry->instanceBuffer, mappedInstanceBuffer, type);
}

void ovrVkProgramParmState_SetParm(
		ovrVkProgramParmState *parmState,
		const ovrVkProgramParmLayout *parmLayout,
		const int index,
		const ovrVkProgramParmType parmType,
		const void *pointer)
{
	assert(index >= 0 && index < MAX_PROGRAM_PARMS);
	if (pointer != NULL)
	{
		bool found = false;
		for (int i = 0; i < parmLayout->numParms; i++)
		{
			if (parmLayout->parms[i].index == index)
			{
				assert(parmLayout->parms[i].type == parmType);
				found = true;
				break;
			}
		}
		// Currently parms can be set even if they are not used by the program.
		// assert( found );
	}

	parmState->parms[index] = pointer;

	const int pushConstantSize = ovrVkProgramParm_GetPushConstantSize(parmType);
	if (pushConstantSize > 0)
	{
		assert(parmLayout->offsetForIndex[index] >= 0);
		assert(
				parmLayout->offsetForIndex[index] + pushConstantSize <= MAX_SAVED_PUSH_CONSTANT_BYTES);
		memcpy(&parmState->data[parmLayout->offsetForIndex[index]], pointer, pushConstantSize);
	}
}

void ovrVkGraphicsCommand_Init(ovrVkGraphicsCommand *command)
{
	command->pipeline = NULL;
	command->vertexBuffer = NULL;
	command->instanceBuffer = NULL;
	memset((void *) &command->parmState, 0, sizeof(command->parmState));
	command->numInstances = 1;
}

void ovrVkGraphicsCommand_SetPipeline(
		ovrVkGraphicsCommand *command,
		const ovrVkGraphicsPipeline *pipeline)
{
	command->pipeline = pipeline;
}

void ovrVkGraphicsCommand_SetParmBufferUniform(
		ovrVkGraphicsCommand *command,
		const int index,
		const Buffer *buffer)
{
	ovrVkProgramParmState_SetParm(
			&command->parmState,
			&command->pipeline->program->parmLayout,
			index,
			OVR_PROGRAM_PARM_TYPE_BUFFER_UNIFORM,
			buffer);
}

void ovrVkGraphicsCommand_SetNumInstances(ovrVkGraphicsCommand *command, const int numInstances)
{
	command->numInstances = numInstances;
}

bool ovrVkProgramParmState_DescriptorsMatch(
		const ovrVkProgramParmLayout *layout1,
		const ovrVkProgramParmState *parmState1,
		const ovrVkProgramParmLayout *layout2,
		const ovrVkProgramParmState *parmState2)
{
	if (layout1 == NULL || layout2 == NULL)
	{
		return false;
	}
	if (layout1->hash != layout2->hash)
	{
		return false;
	}
	for (int i = 0; i < layout1->numBindings; i++)
	{
		if (parmState1->parms[layout1->bindings[i]->index] !=
			parmState2->parms[layout2->bindings[i]->index])
		{
			return false;
		}
	}
	return true;
}

void ovrVkPipelineResources_Create(
		Context *context,
		PipelineResources *resources,
		const ovrVkProgramParmLayout *parmLayout,
		const ovrVkProgramParmState *parms)
{
	memset(resources, 0, sizeof(PipelineResources));

	resources->parmLayout = parmLayout;
	memcpy((void *) &resources->parms, parms, sizeof(ovrVkProgramParmState));

	//
	// Create descriptor pool.
	//

	{
		VkDescriptorPoolSize typeCounts[MAX_PROGRAM_PARMS];

		int count = 0;
		for (int i = 0; i < parmLayout->numBindings; i++)
		{
			VkDescriptorType type =
					ovrGpuProgramParm_GetDescriptorType(parmLayout->bindings[i]->type);
			for (int j = 0; j < count; j++)
			{
				if (typeCounts[j].type == type)
				{
					typeCounts[j].descriptorCount++;
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

		VkDescriptorPoolCreateInfo destriptorPoolCreateInfo;
		destriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		destriptorPoolCreateInfo.pNext = NULL;
		destriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		destriptorPoolCreateInfo.maxSets = 1;
		destriptorPoolCreateInfo.poolSizeCount = count;
		destriptorPoolCreateInfo.pPoolSizes = (count != 0) ? typeCounts : NULL;

		VK(context->device->vkCreateDescriptorPool(
				context->device->device,
				&destriptorPoolCreateInfo,
				nullptr,
				&resources->descriptorPool));
	}

	//
	// Allocated and update a descriptor set.
	//

	{
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo;
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.pNext = NULL;
		descriptorSetAllocateInfo.descriptorPool = resources->descriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &parmLayout->descriptorSetLayout;

		VK(context->device->vkAllocateDescriptorSets(
				context->device->device, &descriptorSetAllocateInfo, &resources->descriptorSet));

		VkWriteDescriptorSet writes[MAX_PROGRAM_PARMS];
		memset(writes, 0, sizeof(writes));

		VkDescriptorImageInfo imageInfo[MAX_PROGRAM_PARMS] = {{0}};
		VkDescriptorBufferInfo bufferInfo[MAX_PROGRAM_PARMS] = {{0}};

		int numWrites = 0;
		for (int i = 0; i < parmLayout->numBindings; i++)
		{
			const ovrVkProgramParm *binding = parmLayout->bindings[i];

			writes[numWrites].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[numWrites].pNext = NULL;
			writes[numWrites].dstSet = resources->descriptorSet;
			writes[numWrites].dstBinding = binding->binding;
			writes[numWrites].dstArrayElement = 0;
			writes[numWrites].descriptorCount = 1;
			writes[numWrites].descriptorType =
					ovrGpuProgramParm_GetDescriptorType(parmLayout->bindings[i]->type);
			writes[numWrites].pImageInfo = &imageInfo[numWrites];
			writes[numWrites].pBufferInfo = &bufferInfo[numWrites];
			writes[numWrites].pTexelBufferView = NULL;

			if (binding->type == OVR_PROGRAM_PARM_TYPE_TEXTURE_SAMPLED)
			{
				const Texture *texture = (const Texture *) parms->parms[binding->index];
				assert(texture->usage == TEXTURE_USAGE_SAMPLED);
				assert(texture->imageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

				imageInfo[numWrites].sampler = texture->sampler;
				imageInfo[numWrites].imageView = texture->view;
				imageInfo[numWrites].imageLayout = texture->imageLayout;
			}
			else if (binding->type == OVR_PROGRAM_PARM_TYPE_TEXTURE_STORAGE)
			{
				const Texture *texture = (const Texture *) parms->parms[binding->index];
				assert(texture->usage == TEXTURE_USAGE_STORAGE);
				assert(texture->imageLayout == VK_IMAGE_LAYOUT_GENERAL);

				imageInfo[numWrites].sampler = VK_NULL_HANDLE;
				imageInfo[numWrites].imageView = texture->view;
				imageInfo[numWrites].imageLayout = texture->imageLayout;
			}
			else if (binding->type == OVR_PROGRAM_PARM_TYPE_BUFFER_UNIFORM)
			{
				const Buffer *buffer = (const Buffer *) parms->parms[binding->index];
				assert(buffer->type == OVR_BUFFER_TYPE_UNIFORM);

				bufferInfo[numWrites].buffer = buffer->buffer;
				bufferInfo[numWrites].offset = 0;
				bufferInfo[numWrites].range = buffer->size;
			}

			numWrites++;
		}

		if (numWrites > 0)
		{
			VC(context->device->vkUpdateDescriptorSets(
					context->device->device, numWrites, writes, 0, NULL));
		}
	}
}

const void *ovrVkProgramParmState_NewPushConstantData(
		const ovrVkProgramParmLayout *newLayout,
		const int newPushConstantIndex,
		const ovrVkProgramParmState *newParmState,
		const ovrVkProgramParmLayout *oldLayout,
		const int oldPushConstantIndex,
		const ovrVkProgramParmState *oldParmState,
		const bool force)
{
	const ovrVkProgramParm *newParm = newLayout->pushConstants[newPushConstantIndex];
	const unsigned char *newData = &newParmState->data[newLayout->offsetForIndex[newParm->index]];
	if (force || oldLayout == NULL || oldPushConstantIndex >= oldLayout->numPushConstants)
	{
		return newData;
	}
	const ovrVkProgramParm *oldParm = oldLayout->pushConstants[oldPushConstantIndex];
	const unsigned char *oldData = &oldParmState->data[oldLayout->offsetForIndex[oldParm->index]];
	if (newParm->type != oldParm->type || newParm->binding != oldParm->binding)
	{
		return newData;
	}
	const int pushConstantSize = ovrVkProgramParm_GetPushConstantSize(newParm->type);
	if (memcmp(newData, oldData, pushConstantSize) != 0)
	{
		return newData;
	}
	return NULL;
}

void ovrVkCommandBuffer_UpdateProgramParms(
		ovrVkCommandBuffer *commandBuffer,
		const ovrVkProgramParmLayout *newLayout,
		const ovrVkProgramParmLayout *oldLayout,
		const ovrVkProgramParmState *newParmState,
		const ovrVkProgramParmState *oldParmState,
		VkPipelineBindPoint bindPoint)
{
	VkCommandBuffer cmdBuffer = commandBuffer->cmdBuffers[commandBuffer->currentBuffer];
	Device *device = commandBuffer->context->device;

	const bool descriptorsMatch =
			ovrVkProgramParmState_DescriptorsMatch(newLayout, newParmState, oldLayout, oldParmState);
	if (!descriptorsMatch)
	{
		// Try to find existing resources that match.
		PipelineResources *resources = NULL;
		for (PipelineResources *r =
				commandBuffer->pipelineResources[commandBuffer->currentBuffer];
			 r != NULL;
			 r = r->next)
		{
			if (ovrVkProgramParmState_DescriptorsMatch(
					newLayout, newParmState, r->parmLayout, &r->parms))
			{
				r->unusedCount = 0;
				resources = r;
				break;
			}
		}

		// Create new resources if none were found.
		if (resources == NULL)
		{
			resources = (PipelineResources *) malloc(sizeof(PipelineResources));
			ovrVkPipelineResources_Create(
					commandBuffer->context, resources, newLayout, newParmState);
			resources->next = commandBuffer->pipelineResources[commandBuffer->currentBuffer];
			commandBuffer->pipelineResources[commandBuffer->currentBuffer] = resources;
		}

		VC(device->vkCmdBindDescriptorSets(
				cmdBuffer,
				bindPoint,
				newLayout->pipelineLayout,
				0,
				1,
				&resources->descriptorSet,
				0,
				NULL));
	}

	for (int i = 0; i < newLayout->numPushConstants; i++)
	{
		const void *data = ovrVkProgramParmState_NewPushConstantData(
				newLayout, i, newParmState, oldLayout, i, oldParmState, false);
		if (data != NULL)
		{
			const ovrVkProgramParm *newParm = newLayout->pushConstants[i];
			const VkShaderStageFlags stageFlags =
					ovrGpuProgramParm_GetShaderStageFlags(newParm->stageFlags);
			const uint32_t offset = (uint32_t) newParm->binding;
			const uint32_t size = (uint32_t) ovrVkProgramParm_GetPushConstantSize(newParm->type);
			VC(device->vkCmdPushConstants(
					cmdBuffer, newLayout->pipelineLayout, stageFlags, offset, size, data));
		}
	}
}

void ovrVkCommandBuffer_SubmitGraphicsCommand(
		ovrVkCommandBuffer *commandBuffer,
		const ovrVkGraphicsCommand *command)
{
	assert(commandBuffer->currentRenderPass != NULL);

	Device *device = commandBuffer->context->device;

	VkCommandBuffer cmdBuffer = commandBuffer->cmdBuffers[commandBuffer->currentBuffer];
	const ovrVkGraphicsCommand *state = &commandBuffer->currentGraphicsState;

	// If the pipeline has changed.
	if (command->pipeline != state->pipeline)
	{
		VC(device->vkCmdBindPipeline(
				cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, command->pipeline->pipeline));
	}

	const ovrVkProgramParmLayout *commandLayout = &command->pipeline->program->parmLayout;
	const ovrVkProgramParmLayout *stateLayout =
			(state->pipeline != NULL) ? &state->pipeline->program->parmLayout : NULL;

	ovrVkCommandBuffer_UpdateProgramParms(
			commandBuffer,
			commandLayout,
			stateLayout,
			&command->parmState,
			&state->parmState,
			VK_PIPELINE_BIND_POINT_GRAPHICS);

	const ovrVkGeometry *geometry = command->pipeline->geometry;

	// If the geometry has changed.
	if (state->pipeline == NULL || geometry != state->pipeline->geometry ||
		command->vertexBuffer != state->vertexBuffer ||
		command->instanceBuffer != state->instanceBuffer)
	{
		const VkBuffer vertexBuffer = (command->vertexBuffer != NULL)
									  ? command->vertexBuffer->buffer
									  : geometry->vertexBuffer.buffer;
		for (int i = 0; i < command->pipeline->firstInstanceBinding; i++)
		{
			VC(device->vkCmdBindVertexBuffers(
					cmdBuffer, i, 1, &vertexBuffer, &command->pipeline->vertexBindingOffsets[i]));
		}

		const VkBuffer instanceBuffer = (command->instanceBuffer != NULL)
										? command->instanceBuffer->buffer
										: geometry->instanceBuffer.buffer;
		for (int i = command->pipeline->firstInstanceBinding;
			 i < command->pipeline->vertexBindingCount;
			 i++)
		{
			VC(device->vkCmdBindVertexBuffers(
					cmdBuffer, i, 1, &instanceBuffer, &command->pipeline->vertexBindingOffsets[i]));
		}

		const VkIndexType indexType = (sizeof(ovrTriangleIndex) == sizeof(unsigned int))
									  ? VK_INDEX_TYPE_UINT32
									  : VK_INDEX_TYPE_UINT16;
		VC(device->vkCmdBindIndexBuffer(cmdBuffer, geometry->indexBuffer.buffer, 0, indexType));
	}

	VC(device->vkCmdDrawIndexed(cmdBuffer, geometry->indexCount, command->numInstances, 0, 0, 0));

	commandBuffer->currentGraphicsState = *command;
}

void ovrVkFence_Destroy(Context *context, Fence *fence)
{
	VC(context->device->vkDestroyFence(context->device->device, fence->fence, nullptr));
	fence->fence = VK_NULL_HANDLE;
	fence->submitted = false;
}

void ovrVkPipelineResources_Destroy(Context *context, PipelineResources *resources)
{
	VC(context->device->vkFreeDescriptorSets(
			context->device->device, resources->descriptorPool, 1, &resources->descriptorSet));
	VC(context->device->vkDestroyDescriptorPool(
			context->device->device, resources->descriptorPool, nullptr));

	memset(resources, 0, sizeof(PipelineResources));
}

void ovrVkCommandBuffer_Destroy(Context *context, ovrVkCommandBuffer *commandBuffer)
{
	assert(context == commandBuffer->context);

	for (int i = 0; i < commandBuffer->numBuffers; i++)
	{
		VC(context->device->vkFreeCommandBuffers(
				context->device->device, context->commandPool, 1, &commandBuffer->cmdBuffers[i]));

		ovrVkFence_Destroy(context, &commandBuffer->fences[i]);

		for (Buffer *b = commandBuffer->mappedBuffers[i], *next = NULL; b != NULL; b = next)
		{
			next = b->next;
			ovrVkBuffer_Destroy(context->device, b);
			free(b);
		}
		commandBuffer->mappedBuffers[i] = NULL;

		for (Buffer *b = commandBuffer->oldMappedBuffers[i], *next = NULL; b != NULL;
			 b = next)
		{
			next = b->next;
			ovrVkBuffer_Destroy(context->device, b);
			free(b);
		}
		commandBuffer->oldMappedBuffers[i] = NULL;

		for (PipelineResources *r = commandBuffer->pipelineResources[i], *next = NULL;
			 r != NULL;
			 r = next)
		{
			next = r->next;
			ovrVkPipelineResources_Destroy(context, r);
			free(r);
		}
		commandBuffer->pipelineResources[i] = NULL;
	}
}

void ovrVkRenderPass_Destroy(Context *context, ovrVkRenderPass *renderPass)
{
	VC(context->device->vkDestroyRenderPass(
			context->device->device, renderPass->renderPass, nullptr));
}

ovrScreenRect ovrVkFramebuffer_GetRect(const Framebuffer *framebuffer)
{
	ovrScreenRect rect;
	rect.x = 0;
	rect.y = 0;
	rect.width = framebuffer->width;
	rect.height = framebuffer->height;
	return rect;
}

void ovrVkCommandBuffer_ManageBuffers(ovrVkCommandBuffer *commandBuffer)
{
	//
	// Manage buffers.
	//

	{
		// Free any old buffers that were not reused for a number of frames.
		for (Buffer **b = &commandBuffer->oldMappedBuffers[commandBuffer->currentBuffer];
			 *b != NULL;)
		{
			if ((*b)->unusedCount++ >= MAX_VERTEX_BUFFER_UNUSED_COUNT)
			{
				Buffer *next = (*b)->next;
				ovrVkBuffer_Destroy(commandBuffer->context->device, *b);
				free(*b);
				*b = next;
			}
			else
			{
				b = &(*b)->next;
			}
		}

		// Move the last used buffers to the list with old buffers.
		for (Buffer *b = commandBuffer->mappedBuffers[commandBuffer->currentBuffer],
					 *next = NULL;
			 b != NULL;
			 b = next)
		{
			next = b->next;
			b->next = commandBuffer->oldMappedBuffers[commandBuffer->currentBuffer];
			commandBuffer->oldMappedBuffers[commandBuffer->currentBuffer] = b;
		}
		commandBuffer->mappedBuffers[commandBuffer->currentBuffer] = NULL;
	}

	//
	// Manage pipeline resources.
	//

	{
		// Free any pipeline resources that were not reused for a number of frames.
		for (PipelineResources **r =
				&commandBuffer->pipelineResources[commandBuffer->currentBuffer];
			 *r != NULL;)
		{
			if ((*r)->unusedCount++ >= MAX_PIPELINE_RESOURCES_UNUSED_COUNT)
			{
				PipelineResources *next = (*r)->next;
				ovrVkPipelineResources_Destroy(commandBuffer->context, *r);
				free(*r);
				*r = next;
			}
			else
			{
				r = &(*r)->next;
			}
		}
	}
}

void ovrVkCommandBuffer_BeginPrimary(ovrVkCommandBuffer *commandBuffer)
{
	assert(commandBuffer->type == OVR_COMMAND_BUFFER_TYPE_PRIMARY);
	assert(commandBuffer->currentFramebuffer == NULL);
	assert(commandBuffer->currentRenderPass == NULL);

	Device *device = commandBuffer->context->device;

	commandBuffer->currentBuffer = (commandBuffer->currentBuffer + 1) % commandBuffer->numBuffers;

	Fence *fence = &commandBuffer->fences[commandBuffer->currentBuffer];
	if (fence->submitted)
	{
		VK(device->vkWaitForFences(
				device->device, 1, &fence->fence, VK_TRUE, 1ULL * 1000 * 1000 * 1000));
		VK(device->vkResetFences(device->device, 1, &fence->fence));
		fence->submitted = false;
	}

	ovrVkCommandBuffer_ManageBuffers(commandBuffer);

	ovrVkGraphicsCommand_Init(&commandBuffer->currentGraphicsState);

	VK(device->vkResetCommandBuffer(commandBuffer->cmdBuffers[commandBuffer->currentBuffer], 0));

	VkCommandBufferBeginInfo commandBufferBeginInfo;
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.pNext = NULL;
	commandBufferBeginInfo.flags = 0;
	commandBufferBeginInfo.pInheritanceInfo = NULL;

	VK(device->vkBeginCommandBuffer(
			commandBuffer->cmdBuffers[commandBuffer->currentBuffer], &commandBufferBeginInfo));

	// Make sure any CPU writes are flushed.
	{
		VkMemoryBarrier memoryBarrier;
		memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		memoryBarrier.pNext = NULL;
		memoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
		memoryBarrier.dstAccessMask = 0;

		const VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_HOST_BIT;
		const VkPipelineStageFlags dst_stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
		const VkDependencyFlags flags = 0;

		VC(device->vkCmdPipelineBarrier(
				commandBuffer->cmdBuffers[commandBuffer->currentBuffer],
				src_stages,
				dst_stages,
				flags,
				1,
				&memoryBarrier,
				0,
				NULL,
				0,
				NULL));
	}
}

void ovrVkCommandBuffer_ChangeTextureUsage(
		ovrVkCommandBuffer *commandBuffer,
		Texture *texture,
		const TextureUsage usage)
{
	ovrVkTexture_ChangeUsage(
			commandBuffer->context,
			commandBuffer->cmdBuffers[commandBuffer->currentBuffer],
			texture,
			usage);
}

void ovrVkCommandBuffer_BeginFramebuffer(
		ovrVkCommandBuffer *commandBuffer,
		Framebuffer *framebuffer,
		const int arrayLayer,
		const TextureUsage usage)
{
	assert(commandBuffer->type == OVR_COMMAND_BUFFER_TYPE_PRIMARY);
	assert(commandBuffer->currentFramebuffer == NULL);
	assert(commandBuffer->currentRenderPass == NULL);
	assert(arrayLayer >= 0 && arrayLayer < framebuffer->numLayers);

	// Only advance when rendering to the first layer.
	if (arrayLayer == 0)
	{
		framebuffer->currentBuffer = (framebuffer->currentBuffer + 1) % framebuffer->numBuffers;
	}
	framebuffer->currentLayer = arrayLayer;

	assert(
			framebuffer->depthBuffer.internalFormat == VK_FORMAT_UNDEFINED ||
			framebuffer->depthBuffer.imageLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	ovrVkCommandBuffer_ChangeTextureUsage(
			commandBuffer, &framebuffer->colorTextures[framebuffer->currentBuffer], usage);

	commandBuffer->currentFramebuffer = framebuffer;
}

void ovrVkCommandBuffer_BeginRenderPass(
		ovrVkCommandBuffer *commandBuffer,
		ovrVkRenderPass *renderPass,
		Framebuffer *framebuffer,
		const ovrScreenRect *rect)
{
	assert(commandBuffer->type == OVR_COMMAND_BUFFER_TYPE_PRIMARY);
	assert(commandBuffer->currentRenderPass == NULL);
	assert(commandBuffer->currentFramebuffer == framebuffer);

	Device *device = commandBuffer->context->device;

	VkCommandBuffer cmdBuffer = commandBuffer->cmdBuffers[commandBuffer->currentBuffer];

	uint32_t clearValueCount = 0;
	VkClearValue clearValues[3];
	memset(clearValues, 0, sizeof(clearValues));

	clearValues[clearValueCount].color.float32[0] = renderPass->clearColor.x;
	clearValues[clearValueCount].color.float32[1] = renderPass->clearColor.y;
	clearValues[clearValueCount].color.float32[2] = renderPass->clearColor.z;
	clearValues[clearValueCount].color.float32[3] = renderPass->clearColor.w;
	clearValueCount++;

	if (renderPass->sampleCount > VK_SAMPLE_COUNT_1_BIT)
	{
		clearValues[clearValueCount].color.float32[0] = renderPass->clearColor.x;
		clearValues[clearValueCount].color.float32[1] = renderPass->clearColor.y;
		clearValues[clearValueCount].color.float32[2] = renderPass->clearColor.z;
		clearValues[clearValueCount].color.float32[3] = renderPass->clearColor.w;
		clearValueCount++;
	}

	if (renderPass->internalDepthFormat != VK_FORMAT_UNDEFINED)
	{
		clearValues[clearValueCount].depthStencil.depth = 1.0f;
		clearValues[clearValueCount].depthStencil.stencil = 0;
		clearValueCount++;
	}

	VkRenderPassBeginInfo renderPassBeginInfo;
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.pNext = NULL;
	renderPassBeginInfo.renderPass = renderPass->renderPass;
	renderPassBeginInfo.framebuffer =
			framebuffer->framebuffers[framebuffer->currentBuffer + framebuffer->currentLayer];
	renderPassBeginInfo.renderArea.offset.x = rect->x;
	renderPassBeginInfo.renderArea.offset.y = rect->y;
	renderPassBeginInfo.renderArea.extent.width = rect->width;
	renderPassBeginInfo.renderArea.extent.height = rect->height;
	renderPassBeginInfo.clearValueCount = clearValueCount;
	renderPassBeginInfo.pClearValues = clearValues;

	VkSubpassContents contents = renderPass->type;

	VC(device->vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, contents));

	commandBuffer->currentRenderPass = renderPass;
}

void ovrVkCommandBuffer_SetViewport(ovrVkCommandBuffer *commandBuffer, const ovrScreenRect *rect)
{
	Device *device = commandBuffer->context->device;

	VkViewport viewport;
	viewport.x = (float) rect->x;
	viewport.y = (float) rect->y;
	viewport.width = (float) rect->width;
	viewport.height = (float) rect->height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkCommandBuffer cmdBuffer = commandBuffer->cmdBuffers[commandBuffer->currentBuffer];
	VC(device->vkCmdSetViewport(cmdBuffer, 0, 1, &viewport));
}

void ovrVkCommandBuffer_SetScissor(ovrVkCommandBuffer *commandBuffer, const ovrScreenRect *rect)
{
	Device *device = commandBuffer->context->device;

	VkRect2D scissor;
	scissor.offset.x = rect->x;
	scissor.offset.y = rect->y;
	scissor.extent.width = rect->width;
	scissor.extent.height = rect->height;

	VkCommandBuffer cmdBuffer = commandBuffer->cmdBuffers[commandBuffer->currentBuffer];
	VC(device->vkCmdSetScissor(cmdBuffer, 0, 1, &scissor));
}

void ovrVkCommandBuffer_EndRenderPass(
		ovrVkCommandBuffer *commandBuffer,
		ovrVkRenderPass *renderPass)
{
	assert(commandBuffer->type == OVR_COMMAND_BUFFER_TYPE_PRIMARY);
	assert(commandBuffer->currentRenderPass == renderPass);

	Device *device = commandBuffer->context->device;

	VkCommandBuffer cmdBuffer = commandBuffer->cmdBuffers[commandBuffer->currentBuffer];

	VC(device->vkCmdEndRenderPass(cmdBuffer));

	commandBuffer->currentRenderPass = NULL;
}

void ovrVkCommandBuffer_EndFramebuffer(
		ovrVkCommandBuffer *commandBuffer,
		Framebuffer *framebuffer,
		const int arrayLayer,
		const TextureUsage usage)
{
	assert(commandBuffer->type == OVR_COMMAND_BUFFER_TYPE_PRIMARY);
	assert(commandBuffer->currentFramebuffer == framebuffer);
	assert(commandBuffer->currentRenderPass == NULL);
	assert(arrayLayer >= 0 && arrayLayer < framebuffer->numLayers);

#if EXPLICIT_RESOLVE != 0
	if (framebuffer->renderTexture.image != VK_NULL_HANDLE) {
		ovrVkCommandBuffer_ChangeTextureUsage(
			commandBuffer, &framebuffer->renderTexture, TEXTURE_USAGE_TRANSFER_SRC);
		ovrVkCommandBuffer_ChangeTextureUsage(
			commandBuffer,
			&framebuffer->colorTextures[framebuffer->currentBuffer],
			TEXTURE_USAGE_TRANSFER_DST);

		VkImageResolve region;
		region.srcOffset.x = 0;
		region.srcOffset.y = 0;
		region.srcOffset.z = 0;
		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.mipLevel = 0;
		region.srcSubresource.baseArrayLayer = arrayLayer;
		region.srcSubresource.layerCount = 1;
		region.dstOffset.x = 0;
		region.dstOffset.y = 0;
		region.dstOffset.z = 0;
		region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.dstSubresource.mipLevel = 0;
		region.dstSubresource.baseArrayLayer = arrayLayer;
		region.dstSubresource.layerCount = 1;
		region.extent.width = framebuffer->renderTexture.width;
		region.extent.height = framebuffer->renderTexture.height;
		region.extent.depth = framebuffer->renderTexture.depth;

		commandBuffer->context->device->vkCmdResolveImage(
			commandBuffer->cmdBuffers[commandBuffer->currentBuffer],
			framebuffer->renderTexture.image,
			framebuffer->renderTexture.imageLayout,
			framebuffer->colorTextures[framebuffer->currentBuffer].image,
			framebuffer->colorTextures[framebuffer->currentBuffer].imageLayout,
			1,
			&region);

		ovrVkCommandBuffer_ChangeTextureUsage(
			commandBuffer, &framebuffer->renderTexture, TEXTURE_USAGE_COLOR_ATTACHMENT);
	}
#endif

	ovrVkCommandBuffer_ChangeTextureUsage(
			commandBuffer, &framebuffer->colorTextures[framebuffer->currentBuffer], usage);

	commandBuffer->currentFramebuffer = NULL;
}

void ovrVkCommandBuffer_EndPrimary(ovrVkCommandBuffer *commandBuffer)
{
	assert(commandBuffer->type == OVR_COMMAND_BUFFER_TYPE_PRIMARY);
	assert(commandBuffer->currentFramebuffer == NULL);
	assert(commandBuffer->currentRenderPass == NULL);

	Device *device = commandBuffer->context->device;
	VK(device->vkEndCommandBuffer(commandBuffer->cmdBuffers[commandBuffer->currentBuffer]));
}

void ovrVkFence_Submit(Context *context, Fence *fence)
{
	fence->submitted = true;
}

Fence *ovrVkCommandBuffer_SubmitPrimary(ovrVkCommandBuffer *commandBuffer)
{
	assert(commandBuffer->type == OVR_COMMAND_BUFFER_TYPE_PRIMARY);
	assert(commandBuffer->currentFramebuffer == NULL);
	assert(commandBuffer->currentRenderPass == NULL);

	Device *device = commandBuffer->context->device;

	const VkPipelineStageFlags stageFlags[1] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

	VkSubmitInfo submitInfo;
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = NULL;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = NULL;
	submitInfo.pWaitDstStageMask = NULL;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer->cmdBuffers[commandBuffer->currentBuffer];
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = NULL;

	Fence *fence = &commandBuffer->fences[commandBuffer->currentBuffer];
	VK(device->vkQueueSubmit(commandBuffer->context->queue, 1, &submitInfo, fence->fence));
	ovrVkFence_Submit(commandBuffer->context, fence);

	return fence;
}

static void ParseExtensionString(
		char *extensionNames,
		uint32_t *numExtensions,
		const char *extensionArrayPtr[],
		const uint32_t arrayCount)
{
	uint32_t extensionCount = 0;
	char *nextExtensionName = extensionNames;

	while (*nextExtensionName && (extensionCount < arrayCount))
	{
		extensionArrayPtr[extensionCount++] = nextExtensionName;

		// Skip to a space or null
		while (*(++nextExtensionName))
		{
			if (*nextExtensionName == ' ')
			{
				// Null-terminate and break out of the loop
				*nextExtensionName++ = '\0';
				break;
			}
		}
	}

	*numExtensions = extensionCount;
}

static const int CPU_LEVEL = 2;
static const int GPU_LEVEL = 3;
static VkSampleCountFlagBits SAMPLE_COUNT = VK_SAMPLE_COUNT_4_BIT;

/*
    TODO:
    - enable layer for gles -> spir-v
*/

/*
================================================================================

System Clock Time

================================================================================
*/

static double GetTimeInSeconds()
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (now.tv_sec * 1e9 + now.tv_nsec) * 0.000000001;
}

/*
================================================================================

ovrGeometry

================================================================================
*/

static void ovrGeometry_CreateCube(Context *context, ovrVkGeometry *geometry)
{
	// The cube is centered about the origin and spans the [-1,1] XYZ range.
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

	static const ovrTriangleIndex cubeIndices[36] = {
			0, 2, 1, 2, 0, 3, // top
			4, 6, 5, 6, 4, 7, // bottom
			2, 6, 7, 7, 1, 2, // right
			0, 4, 5, 5, 3, 0, // left
			3, 5, 6, 6, 2, 3, // front
			0, 1, 7, 7, 4, 0 // back
	};

	ovrDefaultVertexAttributeArrays cubeAttributeArrays;
	ovrVkVertexAttributeArrays_Alloc(
			&cubeAttributeArrays.base,
			DefaultVertexAttributeLayout,
			8,
			OVR_VERTEX_ATTRIBUTE_FLAG_POSITION | OVR_VERTEX_ATTRIBUTE_FLAG_COLOR);

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

	ovrVkTriangleIndexArray cubeIndexArray;
	ovrVkTriangleIndexArray_Alloc(&cubeIndexArray, 36, cubeIndices);

	ovrVkGeometry_Create(context, geometry, &cubeAttributeArrays.base, &cubeIndexArray);

	ovrVkVertexAttributeArrays_Free(&cubeAttributeArrays.base);
	ovrVkTriangleIndexArray_Free(&cubeIndexArray);
}

static void ovrGeometry_Destroy(Context *context, ovrVkGeometry *geometry)
{
	ovrVkGeometry_Destroy(context->device, geometry);
}

/*
================================================================================

ovrProgram

================================================================================
*/

enum
{
	PROGRAM_UNIFORM_SCENE_MATRICES,
};

static ovrVkProgramParm colorOnlyProgramParms[] = {
		{OVR_PROGRAM_STAGE_FLAG_VERTEX,
				OVR_PROGRAM_PARM_TYPE_BUFFER_UNIFORM,
				OVR_PROGRAM_PARM_ACCESS_READ_ONLY,
				PROGRAM_UNIFORM_SCENE_MATRICES,
				"SceneMatrices",
				0},
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
		"#extension GL_OVR_multiview2 : enable\n"
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

/*
================================================================================

ovrFramebuffer

================================================================================
*/

struct ColorSwapChain
{
	int SwapChainLength;
	ovrTextureSwapChain *SwapChain;
	std::vector<VkImage> ColorTextures;
	std::vector<VkImage> FragmentDensityTextures;
	std::vector<VkExtent2D> FragmentDensityTextureSizes;
};

/*
================================================================================

ovrFramebuffer

================================================================================
*/

typedef struct
{
	int Width;
	int Height;
	VkSampleCountFlagBits SampleCount;
	int TextureSwapChainLength;
	int TextureSwapChainIndex;
	ovrTextureSwapChain *ColorTextureSwapChain;
	Framebuffer Framebuffer;
} ovrFrameBuffer;

static void ovrFramebuffer_Clear(ovrFrameBuffer *frameBuffer)
{
	frameBuffer->Width = 0;
	frameBuffer->Height = 0;
	frameBuffer->SampleCount = VK_SAMPLE_COUNT_1_BIT;
	frameBuffer->TextureSwapChainLength = 0;
	frameBuffer->TextureSwapChainIndex = 0;
	frameBuffer->ColorTextureSwapChain = NULL;

	memset(&frameBuffer->Framebuffer, 0, sizeof(Framebuffer));
}

static void ovrFramebuffer_Destroy(Context *context, ovrFrameBuffer *frameBuffer)
{
	for (int i = 0; i < frameBuffer->TextureSwapChainLength; i++)
	{
		if (frameBuffer->Framebuffer.framebuffers.size() > 0)
		{
			VC(context->device->vkDestroyFramebuffer(
					context->device->device, frameBuffer->Framebuffer.framebuffers[i], nullptr));
		}
		if (frameBuffer->Framebuffer.colorTextures.size() > 0)
		{
			ovrVkTexture_Destroy(context, &frameBuffer->Framebuffer.colorTextures[i]);
		}
		if (frameBuffer->Framebuffer.fragmentDensityTextures.size() > 0)
		{
			ovrVkTexture_Destroy(context, &frameBuffer->Framebuffer.fragmentDensityTextures[i]);
		}
	}

	if (frameBuffer->Framebuffer.depthBuffer.image != VK_NULL_HANDLE)
	{
		ovrVkDepthBuffer_Destroy(context, &frameBuffer->Framebuffer.depthBuffer);
	}
	if (frameBuffer->Framebuffer.renderTexture.image != VK_NULL_HANDLE)
	{
		ovrVkTexture_Destroy(context, &frameBuffer->Framebuffer.renderTexture);
	}

	vrapi_DestroyTextureSwapChain(frameBuffer->ColorTextureSwapChain);

	ovrFramebuffer_Clear(frameBuffer);
}

/*
================================================================================

ovrScene

================================================================================
*/

#define NUM_INSTANCES 1500
#define NUM_ROTATIONS 16

typedef struct
{
	bool CreatedScene;
	unsigned int Random;
	ovrVkGraphicsProgram Program;
	ovrVkGeometry Cube;
	ovrVkGraphicsPipeline Pipelines;
	Buffer SceneMatrices;
	int NumViews;

	ovrVector3f Rotations[NUM_ROTATIONS];
	ovrVector3f CubePositions[NUM_INSTANCES];
	int CubeRotations[NUM_INSTANCES];
} ovrScene;

// Returns a random float in the range [0, 1].
static float ovrScene_RandomFloat(ovrScene *scene)
{
	scene->Random = 1664525L * scene->Random + 1013904223L;
	unsigned int rf = 0x3F800000 | (scene->Random & 0x007FFFFF);
	return (*(float *) &rf) - 1.0f;
}

void InitVertexAttributes(
		const bool instance,
		const ovrVkVertexAttribute *vertexLayout,
		const int numAttribs,
		const int storedAttribsFlags,
		const int usedAttribsFlags,
		VkVertexInputAttributeDescription *attributes,
		int *attributeCount,
		VkVertexInputBindingDescription *bindings,
		int *bindingCount,
		VkDeviceSize *bindingOffsets)
{
	size_t offset = 0;
	for (int i = 0; vertexLayout[i].attributeFlag != 0; i++)
	{
		const ovrVkVertexAttribute *v = &vertexLayout[i];
		if ((v->attributeFlag & storedAttribsFlags) != 0)
		{
			if ((v->attributeFlag & usedAttribsFlags) != 0)
			{
				for (int location = 0; location < v->locationCount; location++)
				{
					attributes[*attributeCount + location].location = *attributeCount + location;
					attributes[*attributeCount + location].binding = *bindingCount;
					attributes[*attributeCount + location].format = (VkFormat) v->attributeFormat;
					attributes[*attributeCount + location].offset = (uint32_t) (
							location * v->attributeSize /
							v->locationCount); // limited offset used for packed vertex data
				}

				bindings[*bindingCount].binding = *bindingCount;
				bindings[*bindingCount].stride = (uint32_t) v->attributeSize;
				bindings[*bindingCount].inputRate =
						instance ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;

				bindingOffsets[*bindingCount] =
						(VkDeviceSize) offset; // memory offset within vertex buffer

				*attributeCount += v->locationCount;
				*bindingCount += 1;
			}
			offset += numAttribs * v->attributeSize;
		}
	}
}

bool ovrVkGraphicsPipeline_Create(
		Context *context,
		ovrVkGraphicsPipeline *pipeline,
		const ovrVkGraphicsPipelineParms *parms)
{
	// Make sure the geometry provides all the attributes needed by the program.
	assert(
			((parms->geometry->vertexAttribsFlags | parms->geometry->instanceAttribsFlags) &
			 parms->program->vertexAttribsFlags) == parms->program->vertexAttribsFlags);

	pipeline->rop = parms->rop;
	pipeline->program = parms->program;
	pipeline->geometry = parms->geometry;
	pipeline->vertexAttributeCount = 0;
	pipeline->vertexBindingCount = 0;

	InitVertexAttributes(
			false,
			parms->geometry->layout,
			parms->geometry->vertexCount,
			parms->geometry->vertexAttribsFlags,
			parms->program->vertexAttribsFlags,
			pipeline->vertexAttributes,
			&pipeline->vertexAttributeCount,
			pipeline->vertexBindings,
			&pipeline->vertexBindingCount,
			pipeline->vertexBindingOffsets);

	pipeline->firstInstanceBinding = pipeline->vertexBindingCount;

	InitVertexAttributes(
			true,
			parms->geometry->layout,
			parms->geometry->instanceCount,
			parms->geometry->instanceAttribsFlags,
			parms->program->vertexAttribsFlags,
			pipeline->vertexAttributes,
			&pipeline->vertexAttributeCount,
			pipeline->vertexBindings,
			&pipeline->vertexBindingCount,
			pipeline->vertexBindingOffsets);

	pipeline->vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	pipeline->vertexInputState.pNext = NULL;
	pipeline->vertexInputState.flags = 0;
	pipeline->vertexInputState.vertexBindingDescriptionCount = pipeline->vertexBindingCount;
	pipeline->vertexInputState.pVertexBindingDescriptions = pipeline->vertexBindings;
	pipeline->vertexInputState.vertexAttributeDescriptionCount = pipeline->vertexAttributeCount;
	pipeline->vertexInputState.pVertexAttributeDescriptions = pipeline->vertexAttributes;

	pipeline->inputAssemblyState.sType =
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	pipeline->inputAssemblyState.pNext = NULL;
	pipeline->inputAssemblyState.flags = 0;
	pipeline->inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	pipeline->inputAssemblyState.primitiveRestartEnable = VK_FALSE;

	VkPipelineTessellationStateCreateInfo tessellationStateCreateInfo;
	tessellationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
	tessellationStateCreateInfo.pNext = NULL;
	tessellationStateCreateInfo.flags = 0;
	tessellationStateCreateInfo.patchControlPoints = 0;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo;
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.pNext = NULL;
	viewportStateCreateInfo.flags = 0;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = NULL;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = NULL;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo;
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.pNext = NULL;
	rasterizationStateCreateInfo.flags = 0;
	// NOTE: If the depth clamping feature is not enabled, depthClampEnable must be VK_FALSE.
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.cullMode = (VkCullModeFlags) parms->rop.cullMode;
	rasterizationStateCreateInfo.frontFace = (VkFrontFace) parms->rop.frontFace;
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
	rasterizationStateCreateInfo.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo;
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.pNext = NULL;
	multisampleStateCreateInfo.flags = 0;
	multisampleStateCreateInfo.rasterizationSamples =
			(VkSampleCountFlagBits) parms->renderPass->sampleCount;
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.minSampleShading = 1.0f;
	multisampleStateCreateInfo.pSampleMask = NULL;
	multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo;
	depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCreateInfo.pNext = NULL;
	depthStencilStateCreateInfo.flags = 0;
	depthStencilStateCreateInfo.depthTestEnable = parms->rop.depthTestEnable ? VK_TRUE : VK_FALSE;
	depthStencilStateCreateInfo.depthWriteEnable = parms->rop.depthWriteEnable ? VK_TRUE : VK_FALSE;
	depthStencilStateCreateInfo.depthCompareOp = (VkCompareOp) parms->rop.depthCompare;
	depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
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

	VkPipelineColorBlendAttachmentState colorBlendAttachementState[1];
	colorBlendAttachementState[0].blendEnable = parms->rop.blendEnable ? VK_TRUE : VK_FALSE;
	colorBlendAttachementState[0].srcColorBlendFactor = (VkBlendFactor) parms->rop.blendSrcColor;
	colorBlendAttachementState[0].dstColorBlendFactor = (VkBlendFactor) parms->rop.blendDstColor;
	colorBlendAttachementState[0].colorBlendOp = (VkBlendOp) parms->rop.blendOpColor;
	colorBlendAttachementState[0].srcAlphaBlendFactor = (VkBlendFactor) parms->rop.blendSrcAlpha;
	colorBlendAttachementState[0].dstAlphaBlendFactor = (VkBlendFactor) parms->rop.blendDstAlpha;
	colorBlendAttachementState[0].alphaBlendOp = (VkBlendOp) parms->rop.blendOpAlpha;
	colorBlendAttachementState[0].colorWriteMask =
			(parms->rop.redWriteEnable ? VK_COLOR_COMPONENT_R_BIT : 0) |
			(parms->rop.blueWriteEnable ? VK_COLOR_COMPONENT_G_BIT : 0) |
			(parms->rop.greenWriteEnable ? VK_COLOR_COMPONENT_B_BIT : 0) |
			(parms->rop.alphaWriteEnable ? VK_COLOR_COMPONENT_A_BIT : 0);

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo;
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.pNext = NULL;
	colorBlendStateCreateInfo.flags = 0;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_CLEAR;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = colorBlendAttachementState;
	colorBlendStateCreateInfo.blendConstants[0] = parms->rop.blendColor.x;
	colorBlendStateCreateInfo.blendConstants[1] = parms->rop.blendColor.y;
	colorBlendStateCreateInfo.blendConstants[2] = parms->rop.blendColor.z;
	colorBlendStateCreateInfo.blendConstants[3] = parms->rop.blendColor.w;

	VkDynamicState dynamicStateEnables[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
	};

	VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo;
	pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	pipelineDynamicStateCreateInfo.pNext = NULL;
	pipelineDynamicStateCreateInfo.flags = 0;
	pipelineDynamicStateCreateInfo.dynamicStateCount = ARRAY_SIZE(dynamicStateEnables);
	pipelineDynamicStateCreateInfo.pDynamicStates = dynamicStateEnables;

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo;
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.pNext = NULL;
	graphicsPipelineCreateInfo.flags = 0;
	graphicsPipelineCreateInfo.stageCount = 2;
	graphicsPipelineCreateInfo.pStages = parms->program->pipelineStages;
	graphicsPipelineCreateInfo.pVertexInputState = &pipeline->vertexInputState;
	graphicsPipelineCreateInfo.pInputAssemblyState = &pipeline->inputAssemblyState;
	graphicsPipelineCreateInfo.pTessellationState = &tessellationStateCreateInfo;
	graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState =
			(parms->renderPass->internalDepthFormat != VK_FORMAT_UNDEFINED)
			? &depthStencilStateCreateInfo
			: NULL;
	graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = &pipelineDynamicStateCreateInfo;
	graphicsPipelineCreateInfo.layout = parms->program->parmLayout.pipelineLayout;
	graphicsPipelineCreateInfo.renderPass = parms->renderPass->renderPass;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;

	VK(context->device->vkCreateGraphicsPipelines(
			context->device->device,
			context->pipelineCache,
			1,
			&graphicsPipelineCreateInfo,
			nullptr,
			&pipeline->pipeline));

	return true;
}

void ovrVkGraphicsPipelineParms_Init(ovrVkGraphicsPipelineParms *parms)
{
	parms->rop.blendEnable = false;
	parms->rop.redWriteEnable = true;
	parms->rop.blueWriteEnable = true;
	parms->rop.greenWriteEnable = true;
	parms->rop.alphaWriteEnable = false;
	parms->rop.depthTestEnable = true;
	parms->rop.depthWriteEnable = true;
	parms->rop.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	parms->rop.cullMode = VK_CULL_MODE_BACK_BIT;
	parms->rop.depthCompare = VK_COMPARE_OP_LESS_OR_EQUAL;
	parms->rop.blendColor.x = 0.0f;
	parms->rop.blendColor.y = 0.0f;
	parms->rop.blendColor.z = 0.0f;
	parms->rop.blendColor.w = 0.0f;
	parms->rop.blendOpColor = VK_BLEND_OP_ADD;
	parms->rop.blendSrcColor = VK_BLEND_FACTOR_ONE;
	parms->rop.blendDstColor = VK_BLEND_FACTOR_ZERO;
	parms->rop.blendOpAlpha = VK_BLEND_OP_ADD;
	parms->rop.blendSrcAlpha = VK_BLEND_FACTOR_ONE;
	parms->rop.blendDstAlpha = VK_BLEND_FACTOR_ZERO;
	parms->renderPass = NULL;
	parms->program = NULL;
	parms->geometry = NULL;
}

static void ovrScene_Create(Context *context, ovrScene *scene, ovrVkRenderPass *renderPass)
{
	const bool isMultiview = context->device->supportsMultiview;
	scene->NumViews = (isMultiview) ? 2 : 1;

	ovrGeometry_CreateCube(context, &scene->Cube);
	// Create the instance transform attribute buffer.
	ovrVkGeometry_AddInstanceAttributes(
			context, &scene->Cube, NUM_INSTANCES, OVR_VERTEX_ATTRIBUTE_FLAG_TRANSFORM);

	ovrVkGraphicsProgram_Create(
			context,
			&scene->Program,
			isMultiview ? PROGRAM(colorOnlyMultiviewVertexProgram) : PROGRAM(colorOnlyVertexProgram),
			isMultiview ? sizeof(PROGRAM(colorOnlyMultiviewVertexProgram))
						: sizeof(PROGRAM(colorOnlyVertexProgram)),
			PROGRAM(colorOnlyFragmentProgram),
			sizeof(PROGRAM(colorOnlyFragmentProgram)),
			colorOnlyProgramParms,
			ARRAY_SIZE(colorOnlyProgramParms),
			scene->Cube.layout,
			OVR_VERTEX_ATTRIBUTE_FLAG_POSITION | OVR_VERTEX_ATTRIBUTE_FLAG_COLOR |
			OVR_VERTEX_ATTRIBUTE_FLAG_TRANSFORM);

	// Set up the graphics pipeline.
	ovrVkGraphicsPipelineParms pipelineParms;
	ovrVkGraphicsPipelineParms_Init(&pipelineParms);
	pipelineParms.renderPass = renderPass;
	pipelineParms.program = &scene->Program;
	pipelineParms.geometry = &scene->Cube;

	// ROP state.
	pipelineParms.rop.blendEnable = false;
	pipelineParms.rop.redWriteEnable = true;
	pipelineParms.rop.blueWriteEnable = true;
	pipelineParms.rop.greenWriteEnable = true;
	pipelineParms.rop.alphaWriteEnable = false;
	pipelineParms.rop.depthTestEnable = true;
	pipelineParms.rop.depthWriteEnable = true;
	pipelineParms.rop.frontFace = VK_FRONT_FACE_CLOCKWISE;
	pipelineParms.rop.cullMode = VK_CULL_MODE_BACK_BIT;
	pipelineParms.rop.depthCompare = VK_COMPARE_OP_LESS_OR_EQUAL;
	pipelineParms.rop.blendColor.x = 0.0f;
	pipelineParms.rop.blendColor.y = 0.0f;
	pipelineParms.rop.blendColor.z = 0.0f;
	pipelineParms.rop.blendColor.w = 0.0f;
	pipelineParms.rop.blendOpColor = VK_BLEND_OP_ADD;
	pipelineParms.rop.blendSrcColor = VK_BLEND_FACTOR_ONE;
	pipelineParms.rop.blendDstColor = VK_BLEND_FACTOR_ZERO;
	pipelineParms.rop.blendOpAlpha = VK_BLEND_OP_ADD;
	pipelineParms.rop.blendSrcAlpha = VK_BLEND_FACTOR_ONE;
	pipelineParms.rop.blendDstAlpha = VK_BLEND_FACTOR_ZERO;

	ovrVkGraphicsPipeline_Create(context, &scene->Pipelines, &pipelineParms);

	// Setup the scene matrices.
	ovrVkBuffer_Create(
			context,
			&scene->SceneMatrices,
			OVR_BUFFER_TYPE_UNIFORM,
			2 * scene->NumViews * sizeof(ovrMatrix4f),
			NULL,
			false);

	// Setup random rotations.
	for (int i = 0; i < NUM_ROTATIONS; i++)
	{
		scene->Rotations[i].x = ovrScene_RandomFloat(scene);
		scene->Rotations[i].y = ovrScene_RandomFloat(scene);
		scene->Rotations[i].z = ovrScene_RandomFloat(scene);
	}

	// Setup random cube positions and rotations.
	for (int i = 0; i < NUM_INSTANCES; i++)
	{
		// Using volatile keeps the compiler from optimizing away multiple calls to
		// ovrScene_RandomFloat().
		volatile float rx, ry, rz;
		for (;;)
		{
			rx = (ovrScene_RandomFloat(scene) - 0.5f) * (50.0f + sqrt(NUM_INSTANCES));
			ry = (ovrScene_RandomFloat(scene) - 0.5f) * (50.0f + sqrt(NUM_INSTANCES));
			rz = (ovrScene_RandomFloat(scene) - 0.5f) * (50.0f + sqrt(NUM_INSTANCES));
			// If too close to 0,0,0
			if (fabsf(rx) < 4.0f && fabsf(ry) < 4.0f && fabsf(rz) < 4.0f)
			{
				continue;
			}
			// Test for overlap with any of the existing cubes.
			bool overlap = false;
			for (int j = 0; j < i; j++)
			{
				if (fabsf(rx - scene->CubePositions[j].x) < 4.0f &&
					fabsf(ry - scene->CubePositions[j].y) < 4.0f &&
					fabsf(rz - scene->CubePositions[j].z) < 4.0f)
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

		// Insert into list sorted based on distance.
		int insert = 0;
		const float distSqr = rx * rx + ry * ry + rz * rz;
		for (int j = i; j > 0; j--)
		{
			const ovrVector3f *otherPos = &scene->CubePositions[j - 1];
			const float otherDistSqr =
					otherPos->x * otherPos->x + otherPos->y * otherPos->y + otherPos->z * otherPos->z;
			if (distSqr > otherDistSqr)
			{
				insert = j;
				break;
			}
			scene->CubePositions[j] = scene->CubePositions[j - 1];
			scene->CubeRotations[j] = scene->CubeRotations[j - 1];
		}

		scene->CubePositions[insert].x = rx;
		scene->CubePositions[insert].y = ry;
		scene->CubePositions[insert].z = rz;

		scene->CubeRotations[insert] = (int) (ovrScene_RandomFloat(scene) * (NUM_ROTATIONS - 0.1f));
	}

	scene->CreatedScene = true;
}

void ovrVkContext_WaitIdle(Context *context)
{
	VK(context->device->vkQueueWaitIdle(context->queue));
}

void ovrVkGraphicsPipeline_Destroy(Context *context, ovrVkGraphicsPipeline *pipeline)
{
	VC(context->device->vkDestroyPipeline(
			context->device->device, pipeline->pipeline, nullptr));

	memset(pipeline, 0, sizeof(ovrVkGraphicsPipeline));
}

void ovrVkProgramParmLayout_Destroy(Context *context, ovrVkProgramParmLayout *layout)
{
	VC(context->device->vkDestroyPipelineLayout(
			context->device->device, layout->pipelineLayout, nullptr));
	VC(context->device->vkDestroyDescriptorSetLayout(
			context->device->device, layout->descriptorSetLayout, nullptr));
}

void ovrVkGraphicsProgram_Destroy(Context *context, ovrVkGraphicsProgram *program)
{
	ovrVkProgramParmLayout_Destroy(context, &program->parmLayout);

	VC(context->device->vkDestroyShaderModule(
			context->device->device, program->vertexShaderModule, nullptr));
	VC(context->device->vkDestroyShaderModule(
			context->device->device, program->fragmentShaderModule, nullptr));
}

static void ovrScene_Destroy(Context *context, ovrScene *scene)
{
	ovrVkContext_WaitIdle(context);

	ovrVkGraphicsPipeline_Destroy(context, &scene->Pipelines);

	ovrGeometry_Destroy(context, &scene->Cube);

	ovrVkGraphicsProgram_Destroy(context, &scene->Program);

	ovrVkBuffer_Destroy(context->device, &scene->SceneMatrices);

	scene->CreatedScene = false;
}

Buffer *ovrVkCommandBuffer_MapInstanceAttributes(
		ovrVkCommandBuffer *commandBuffer,
		ovrVkGeometry *geometry,
		ovrVkVertexAttributeArrays *attribs)
{
	void *data = NULL;
	Buffer *buffer =
			ovrVkCommandBuffer_MapBuffer(commandBuffer, &geometry->instanceBuffer, &data);

	attribs->layout = geometry->layout;
	ovrVkVertexAttributeArrays_Map(
			attribs, data, buffer->size, geometry->instanceCount, geometry->instanceAttribsFlags);

	return buffer;
}

static void ovrScene_UpdateBuffers(
		ovrVkCommandBuffer *commandBuffer,
		ovrScene *scene,
		const ovrVector3f *currRotation,
		const ovrMatrix4f *viewMatrix,
		const ovrMatrix4f *projectionMatrix)
{
	// Update the scene matrices uniform buffer.
	ovrMatrix4f *sceneMatrices = NULL;
	Buffer *sceneMatricesBuffer =
			ovrVkCommandBuffer_MapBuffer(commandBuffer, &scene->SceneMatrices, (void **) &sceneMatrices);
	memcpy(sceneMatrices + 0 * scene->NumViews, viewMatrix, scene->NumViews * sizeof(ovrMatrix4f));
	memcpy(
			sceneMatrices + 1 * scene->NumViews,
			projectionMatrix,
			scene->NumViews * sizeof(ovrMatrix4f));
	ovrVkCommandBuffer_UnmapBuffer(
			commandBuffer, &scene->SceneMatrices, sceneMatricesBuffer, OVR_BUFFER_UNMAP_TYPE_COPY_BACK);

	ovrMatrix4f rotationMatrices[NUM_ROTATIONS];
	for (int i = 0; i < NUM_ROTATIONS; i++)
	{
		rotationMatrices[i] = ovrMatrix4f_CreateRotation(
				scene->Rotations[i].x * currRotation->x,
				scene->Rotations[i].y * currRotation->y,
				scene->Rotations[i].z * currRotation->z);
	}

	// Update the instanced transform buffer.
	ovrDefaultVertexAttributeArrays attribs;
	Buffer *instanceBuffer =
			ovrVkCommandBuffer_MapInstanceAttributes(commandBuffer, &scene->Cube, &attribs.base);

	ovrMatrix4f *transforms = &attribs.transform[0];
	for (int i = 0; i < NUM_INSTANCES; i++)
	{
		const int index = scene->CubeRotations[i];

		// Write in order in case the mapped buffer lives on write-combined memory.
		transforms[i].M[0][0] = rotationMatrices[index].M[0][0];
		transforms[i].M[0][1] = rotationMatrices[index].M[0][1];
		transforms[i].M[0][2] = rotationMatrices[index].M[0][2];
		transforms[i].M[0][3] = rotationMatrices[index].M[0][3];

		transforms[i].M[1][0] = rotationMatrices[index].M[1][0];
		transforms[i].M[1][1] = rotationMatrices[index].M[1][1];
		transforms[i].M[1][2] = rotationMatrices[index].M[1][2];
		transforms[i].M[1][3] = rotationMatrices[index].M[1][3];

		transforms[i].M[2][0] = rotationMatrices[index].M[2][0];
		transforms[i].M[2][1] = rotationMatrices[index].M[2][1];
		transforms[i].M[2][2] = rotationMatrices[index].M[2][2];
		transforms[i].M[2][3] = rotationMatrices[index].M[2][3];

		transforms[i].M[3][0] = scene->CubePositions[i].x;
		transforms[i].M[3][1] = scene->CubePositions[i].y;
		transforms[i].M[3][2] = scene->CubePositions[i].z;
		transforms[i].M[3][3] = 1.0f;
	}

	ovrVkCommandBuffer_UnmapInstanceAttributes(
			commandBuffer, &scene->Cube, instanceBuffer, OVR_BUFFER_UNMAP_TYPE_COPY_BACK);
}

static void ovrScene_Render(ovrVkCommandBuffer *commandBuffer, ovrScene *scene)
{
	ovrVkGraphicsCommand command;
	ovrVkGraphicsCommand_Init(&command);
	ovrVkGraphicsCommand_SetPipeline(&command, &scene->Pipelines);
	ovrVkGraphicsCommand_SetParmBufferUniform(
			&command, PROGRAM_UNIFORM_SCENE_MATRICES, &scene->SceneMatrices);
	ovrVkGraphicsCommand_SetNumInstances(&command, NUM_INSTANCES);

	ovrVkCommandBuffer_SubmitGraphicsCommand(commandBuffer, &command);
}

/*
================================================================================

ovrSimulation

================================================================================
*/

struct ovrSimulation
{
	ovrVector3f CurrentRotation;
};

/*
================================================================================

ovrRenderer

================================================================================
*/

typedef struct
{
	ovrVkRenderPass RenderPassSingleView;
	ovrVkCommandBuffer EyeCommandBuffer[VRAPI_FRAME_LAYER_EYE_MAX];
	ovrFrameBuffer Framebuffer[VRAPI_FRAME_LAYER_EYE_MAX];

	ovrMatrix4f ViewMatrix[VRAPI_FRAME_LAYER_EYE_MAX];
	ovrMatrix4f ProjectionMatrix[VRAPI_FRAME_LAYER_EYE_MAX];
	int NumEyes;
} ovrRenderer;

static void ovrRenderer_Destroy(ovrRenderer *renderer, Context *context)
{
	for (int eye = 0; eye < renderer->NumEyes; eye++)
	{
		ovrFramebuffer_Destroy(context, &renderer->Framebuffer[eye]);

		ovrVkCommandBuffer_Destroy(context, &renderer->EyeCommandBuffer[eye]);
	}

	for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++)
	{
		renderer->ViewMatrix[eye] = ovrMatrix4f_CreateIdentity();
		renderer->ProjectionMatrix[eye] = ovrMatrix4f_CreateIdentity();
	}

	ovrVkRenderPass_Destroy(context, &renderer->RenderPassSingleView);
}

static ovrLayerProjection2 ovrRenderer_RenderFrame(
		ovrRenderer *renderer,
		long long frameIndex,
		ovrScene *scene,
		const ovrSimulation *simulation,
		const ovrTracking2 *tracking)
{
	ovrMatrix4f eyeViewMatrixTransposed[2];
	eyeViewMatrixTransposed[0] = ovrMatrix4f_Transpose(&tracking->Eye[0].ViewMatrix);
	eyeViewMatrixTransposed[1] = ovrMatrix4f_Transpose(&tracking->Eye[1].ViewMatrix);

	renderer->ViewMatrix[0] = eyeViewMatrixTransposed[0];
	renderer->ViewMatrix[1] = eyeViewMatrixTransposed[1];

	ovrMatrix4f projectionMatrixTransposed[2];
	projectionMatrixTransposed[0] = ovrMatrix4f_Transpose(&tracking->Eye[0].ProjectionMatrix);
	projectionMatrixTransposed[1] = ovrMatrix4f_Transpose(&tracking->Eye[1].ProjectionMatrix);

	renderer->ProjectionMatrix[0] = projectionMatrixTransposed[0];
	renderer->ProjectionMatrix[1] = projectionMatrixTransposed[1];

	// Render the scene.

	for (int eye = 0; eye < renderer->NumEyes; eye++)
	{
		const ovrScreenRect screenRect =
				ovrVkFramebuffer_GetRect(&renderer->Framebuffer[eye].Framebuffer);

		ovrVkCommandBuffer_BeginPrimary(&renderer->EyeCommandBuffer[eye]);
		ovrVkCommandBuffer_BeginFramebuffer(
				&renderer->EyeCommandBuffer[eye],
				&renderer->Framebuffer[eye].Framebuffer,
				0 /*eye*/,
				TEXTURE_USAGE_COLOR_ATTACHMENT);

		// Update the instance transform attributes.
		// NOTE: needs to be called before ovrVkCommandBuffer_BeginRenderPass when current render
		// pass is not set.
		ovrScene_UpdateBuffers(
				&renderer->EyeCommandBuffer[eye],
				scene,
				&simulation->CurrentRotation,
				&renderer->ViewMatrix[eye],
				&renderer->ProjectionMatrix[eye]);

		ovrVkCommandBuffer_BeginRenderPass(
				&renderer->EyeCommandBuffer[eye],
				&renderer->RenderPassSingleView,
				&renderer->Framebuffer[eye].Framebuffer,
				&screenRect);

		ovrVkCommandBuffer_SetViewport(&renderer->EyeCommandBuffer[eye], &screenRect);
		ovrVkCommandBuffer_SetScissor(&renderer->EyeCommandBuffer[eye], &screenRect);

		ovrScene_Render(&renderer->EyeCommandBuffer[eye], scene);

		ovrVkCommandBuffer_EndRenderPass(
				&renderer->EyeCommandBuffer[eye], &renderer->RenderPassSingleView);

		ovrVkCommandBuffer_EndFramebuffer(
				&renderer->EyeCommandBuffer[eye],
				&renderer->Framebuffer[eye].Framebuffer,
				0 /*eye*/,
				TEXTURE_USAGE_SAMPLED);
		ovrVkCommandBuffer_EndPrimary(&renderer->EyeCommandBuffer[eye]);

		ovrVkCommandBuffer_SubmitPrimary(&renderer->EyeCommandBuffer[eye]);
	}

	ovrLayerProjection2 layer = vrapi_DefaultLayerProjection2();
	layer.HeadPose = tracking->HeadPose;
	for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++)
	{
		int eyeToSample = renderer->NumEyes == 1 ? 0 : eye;
		layer.Textures[eye].ColorSwapChain =
				renderer->Framebuffer[eyeToSample].ColorTextureSwapChain;
		layer.Textures[eye].SwapChainIndex =
				renderer->Framebuffer[eyeToSample].Framebuffer.currentBuffer;
		layer.Textures[eye].TexCoordsFromTanAngles =
				ovrMatrix4f_TanAngleMatrixFromProjection(&tracking->Eye[eye].ProjectionMatrix);
	}

	return layer;
}

struct AppState
{
	ovrJava Java;
	ANativeWindow *NativeWindow;
	bool Resumed;
	Device Device;
	Context Context;
	ovrMobile *Ovr;
	ovrScene Scene;
	ovrSimulation Simulation;
	long long FrameIndex;
	double DisplayTime;
	int SwapInterval;
	int CpuLevel;
	int GpuLevel;
	int MainThreadTid;
	int RenderThreadTid;
	bool BackButtonDownLastFrame;
	ovrRenderer Renderer;
};

/*
================================================================================

Native Activity

================================================================================
*/

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

static void appHandleCmd(struct android_app *app, int32_t cmd)
{
	auto appState = (AppState *) app->userData;
	switch (cmd)
	{
		case APP_CMD_START:
			__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "onStart()");
			__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "    APP_CMD_START");
			break;
		case APP_CMD_RESUME:
			__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "onResume()");
			__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "    APP_CMD_RESUME");
			appState->Resumed = true;
			break;
		case APP_CMD_PAUSE:
			__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "onPause()");
			__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "    APP_CMD_PAUSE");
			appState->Resumed = false;
			break;
		case APP_CMD_STOP:
			__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "onStop()");
			__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "    APP_CMD_STOP");
			break;
		case APP_CMD_DESTROY:
			__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "onDestroy()");
			__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "    APP_CMD_DESTROY");
			appState->NativeWindow = NULL;
			break;
		case APP_CMD_INIT_WINDOW:
			__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "surfaceCreated()");
			__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "    APP_CMD_INIT_WINDOW");
			appState->NativeWindow = app->window;
			break;
		case APP_CMD_TERM_WINDOW:
			__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "surfaceDestroyed()");
			__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "    APP_CMD_TERM_WINDOW");
			appState->NativeWindow = NULL;
			break;
	}
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
		auto queueFamilyProperties = new VkQueueFamilyProperties[queueFamilyCount];
		VC(instance.vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[physicalDeviceIndex], &queueFamilyCount, queueFamilyProperties));
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
			delete[] queueFamilyProperties;
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
		appState.Device.queueFamilyProperties = queueFamilyProperties;
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
	appState.Renderer.RenderPassSingleView.colorFormat = SURFACE_COLOR_FORMAT_R8G8B8A8;
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
	attachments[attachmentCount].storeOp = (EXPLICIT_RESOLVE != 0) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[attachmentCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[attachmentCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[attachmentCount].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachments[attachmentCount].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachmentCount++;
	if (EXPLICIT_RESOLVE == 0)
	{
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
	}
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
	subpassDescription.pResolveAttachments = (EXPLICIT_RESOLVE == 0) ? &resolveAttachmentReference : nullptr;
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
		appState.Renderer.Framebuffer[i].Width = eyeTextureWidth;
		appState.Renderer.Framebuffer[i].Height = eyeTextureHeight;
		appState.Renderer.Framebuffer[i].SampleCount = appState.Renderer.RenderPassSingleView.sampleCount;
		appState.Renderer.Framebuffer[i].ColorTextureSwapChain = colorSwapChains[i].SwapChain;
		appState.Renderer.Framebuffer[i].TextureSwapChainLength = colorSwapChains[i].SwapChainLength;
		appState.Renderer.Framebuffer[i].Framebuffer.colorTextures.resize(appState.Renderer.Framebuffer[i].TextureSwapChainLength);
		if (colorSwapChains[i].FragmentDensityTextures.size() > 0)
		{
			appState.Renderer.Framebuffer[i].Framebuffer.fragmentDensityTextures.resize(appState.Renderer.Framebuffer[i].TextureSwapChainLength);
		}
		appState.Renderer.Framebuffer[i].Framebuffer.framebuffers.resize(appState.Renderer.Framebuffer[i].TextureSwapChainLength);
		appState.Renderer.Framebuffer[i].Framebuffer.renderPass = &appState.Renderer.RenderPassSingleView;
		appState.Renderer.Framebuffer[i].Framebuffer.width = eyeTextureWidth;
		appState.Renderer.Framebuffer[i].Framebuffer.height = eyeTextureHeight;
		appState.Renderer.Framebuffer[i].Framebuffer.numLayers = isMultiview ? 2 : 1;
		appState.Renderer.Framebuffer[i].Framebuffer.numBuffers = appState.Renderer.Framebuffer[i].TextureSwapChainLength;
		appState.Renderer.Framebuffer[i].Framebuffer.currentBuffer = 0;
		appState.Renderer.Framebuffer[i].Framebuffer.currentLayer = 0;
		for (auto j = 0; j < appState.Renderer.Framebuffer[i].TextureSwapChainLength; j++)
		{
			auto texture = &appState.Renderer.Framebuffer[i].Framebuffer.colorTextures[j];
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
			if (appState.Renderer.Framebuffer[i].Framebuffer.fragmentDensityTextures.size() > 0)
			{
				auto textureFragmentDensity = &appState.Renderer.Framebuffer[i].Framebuffer.fragmentDensityTextures[j];
				textureFragmentDensity->width = colorSwapChains[i].FragmentDensityTextureSizes[j].width;
				textureFragmentDensity->height = colorSwapChains[i].FragmentDensityTextureSizes[j].height;
				textureFragmentDensity->depth = 1;
				textureFragmentDensity->layerCount = isMultiview ? 2 : 1;
				textureFragmentDensity->mipCount = 1;
				textureFragmentDensity->sampleCount = VK_SAMPLE_COUNT_1_BIT;
				textureFragmentDensity->usage = TEXTURE_USAGE_FRAG_DENSITY;
				textureFragmentDensity->wrapMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
				textureFragmentDensity->filter = VK_SAMPLER_MIPMAP_MODE_LINEAR;
				textureFragmentDensity->maxAnisotropy = 1.0f;
				textureFragmentDensity->format = VK_FORMAT_R8G8_UNORM;
				textureFragmentDensity->imageLayout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
				textureFragmentDensity->image = colorSwapChains[i].FragmentDensityTextures[j];
				textureFragmentDensity->memory = VK_NULL_HANDLE;
				textureFragmentDensity->sampler = VK_NULL_HANDLE;
				textureFragmentDensity->view = VK_NULL_HANDLE;
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
					imageMemoryBarrier.subresourceRange.layerCount = textureFragmentDensity->layerCount;
					const VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
					const VkPipelineStageFlags dst_stages = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
					const VkDependencyFlags flags = 0;
					VC(appState.Context.device->vkCmdPipelineBarrier(appState.Context.setupCommandBuffer, src_stages, dst_stages, flags, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier));
					flushSetupCommandBuffer(&appState.Context);
				}
				VkImageViewCreateInfo imageViewCreateInfoFragmentDensity { };
				imageViewCreateInfoFragmentDensity.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				imageViewCreateInfoFragmentDensity.image = textureFragmentDensity->image;
				imageViewCreateInfoFragmentDensity.viewType = isMultiview ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
				imageViewCreateInfoFragmentDensity.format = VK_FORMAT_R8G8_UNORM;
				imageViewCreateInfoFragmentDensity.components.r = VK_COMPONENT_SWIZZLE_R;
				imageViewCreateInfoFragmentDensity.components.g = VK_COMPONENT_SWIZZLE_G;
				imageViewCreateInfoFragmentDensity.components.b = VK_COMPONENT_SWIZZLE_B;
				imageViewCreateInfoFragmentDensity.components.a = VK_COMPONENT_SWIZZLE_A;
				imageViewCreateInfoFragmentDensity.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageViewCreateInfoFragmentDensity.subresourceRange.levelCount = 1;
				imageViewCreateInfoFragmentDensity.subresourceRange.layerCount = textureFragmentDensity->layerCount;
				VK(appState.Context.device->vkCreateImageView(appState.Context.device->device, &imageViewCreateInfoFragmentDensity, nullptr, &textureFragmentDensity->view));
				updateTextureSampler(&appState.Context, textureFragmentDensity);
			}
		}
		if (appState.Renderer.RenderPassSingleView.sampleCount > VK_SAMPLE_COUNT_1_BIT)
		{
			const int depth = 0;
			const int faceCount = 1;
			const int width = appState.Renderer.Framebuffer[i].Width;
			const int height = appState.Renderer.Framebuffer[i].Height;
			const int maxDimension = width > height ? (width > depth ? width : depth) : (height > depth ? height : depth);
			const int maxMipLevels = (1 + IntegerLog2(maxDimension));
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
			auto texture = &appState.Renderer.Framebuffer[i].Framebuffer.renderTexture;
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
			memoryAllocateInfo.memoryTypeIndex = VkGetMemoryTypeIndex(appState.Context.device, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
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
				VC(appState.Context.device->vkCmdPipelineBarrier(appState.Context.setupCommandBuffer, src_stages, dst_stages, flags, 0, NULL, 0, NULL, 1, &imageMemoryBarrier));
				texture->usage = TEXTURE_USAGE_COLOR_ATTACHMENT;
				texture->imageLayout = newImageLayout;
				flushSetupCommandBuffer(&appState.Context);
			}
		}
		if (appState.Renderer.RenderPassSingleView.internalDepthFormat != VK_FORMAT_UNDEFINED)
		{
			const auto depthBuffer = &appState.Renderer.Framebuffer[i].Framebuffer.depthBuffer;
			const auto depthFormat = appState.Renderer.RenderPassSingleView.depthFormat;
			const auto width = appState.Renderer.Framebuffer[i].Width;
			const auto height = appState.Renderer.Framebuffer[i].Height;
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
				memoryAllocateInfo.memoryTypeIndex = VkGetMemoryTypeIndex(appState.Context.device, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
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
		for (auto j = 0; j < appState.Renderer.Framebuffer[i].TextureSwapChainLength; j++)
		{
			uint32_t attachmentCount = 0;
			VkImageView attachments[4];
			if (appState.Renderer.RenderPassSingleView.sampleCount > VK_SAMPLE_COUNT_1_BIT)
			{
				attachments[attachmentCount++] = appState.Renderer.Framebuffer[i].Framebuffer.renderTexture.view;
			}
			if (appState.Renderer.RenderPassSingleView.sampleCount <= VK_SAMPLE_COUNT_1_BIT || EXPLICIT_RESOLVE == 0)
			{
				attachments[attachmentCount++] = appState.Renderer.Framebuffer[i].Framebuffer.colorTextures[j].view;
			}
			if (appState.Renderer.RenderPassSingleView.internalDepthFormat != VK_FORMAT_UNDEFINED)
			{
				attachments[attachmentCount++] = appState.Renderer.Framebuffer[i].Framebuffer.depthBuffer.view;
			}
			if (appState.Renderer.Framebuffer[i].Framebuffer.fragmentDensityTextures.size() > 0 && appState.Renderer.RenderPassSingleView.useFragmentDensity)
			{
				attachments[attachmentCount++] = appState.Renderer.Framebuffer[i].Framebuffer.fragmentDensityTextures[j].view;
			}
			VkFramebufferCreateInfo framebufferCreateInfo { };
			framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferCreateInfo.renderPass = appState.Renderer.RenderPassSingleView.renderPass;
			framebufferCreateInfo.attachmentCount = attachmentCount;
			framebufferCreateInfo.pAttachments = attachments;
			framebufferCreateInfo.width = appState.Renderer.Framebuffer[i].Framebuffer.width;
			framebufferCreateInfo.height = appState.Renderer.Framebuffer[i].Framebuffer.height;
			framebufferCreateInfo.layers = 1;
			VK(appState.Context.device->vkCreateFramebuffer(appState.Context.device->device, &framebufferCreateInfo, nullptr, &appState.Renderer.Framebuffer[i].Framebuffer.framebuffers[j]));
		}
		const auto numBuffers = appState.Renderer.Framebuffer[i].Framebuffer.numBuffers;
		appState.Renderer.EyeCommandBuffer[i].type = OVR_COMMAND_BUFFER_TYPE_PRIMARY;
		appState.Renderer.EyeCommandBuffer[i].numBuffers = numBuffers;
		appState.Renderer.EyeCommandBuffer[i].currentBuffer = 0;
		appState.Renderer.EyeCommandBuffer[i].context = &appState.Context;
		appState.Renderer.EyeCommandBuffer[i].cmdBuffers.resize(numBuffers);
		appState.Renderer.EyeCommandBuffer[i].fences.resize(numBuffers);
		appState.Renderer.EyeCommandBuffer[i].mappedBuffers.resize(numBuffers);
		appState.Renderer.EyeCommandBuffer[i].oldMappedBuffers.resize(numBuffers);
		appState.Renderer.EyeCommandBuffer[i].pipelineResources.resize(numBuffers);
		for (auto j = 0; j < numBuffers; j++)
		{
			VkCommandBufferAllocateInfo commandBufferAllocateInfo { };
			commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			commandBufferAllocateInfo.commandPool = appState.Context.commandPool;
			commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			commandBufferAllocateInfo.commandBufferCount = 1;
			VK(appState.Context.device->vkAllocateCommandBuffers(appState.Context.device->device, &commandBufferAllocateInfo, &appState.Renderer.EyeCommandBuffer[i].cmdBuffers[j]));
			VkFenceCreateInfo fenceCreateInfo { };
			fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			VK(appState.Context.device->vkCreateFence(appState.Context.device->device, &fenceCreateInfo, nullptr, &appState.Renderer.EyeCommandBuffer[i].fences[j].fence));
			appState.Renderer.EyeCommandBuffer[i].fences[j].submitted = false;
		}
	}
	for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++)
	{
		appState.Renderer.ViewMatrix[eye] = ovrMatrix4f_CreateIdentity();
		appState.Renderer.ProjectionMatrix[eye] = ovrMatrix4f_CreateIdentity();
	}
	app->userData = &appState;
	app->onAppCmd = appHandleCmd;
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
					__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "        vrapi_EnterVrMode()");
					appState.Ovr = vrapi_EnterVrMode((ovrModeParms*)&parms);
					if (appState.Ovr == nullptr)
					{
						__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "android_main(): Invalid ANativeWindow.");
						appState.NativeWindow = nullptr;
					}
					if (appState.Ovr != nullptr)
					{
						vrapi_SetClockLevels(appState.Ovr, appState.CpuLevel, appState.GpuLevel);
						__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "		vrapi_SetClockLevels( %d, %d )", appState.CpuLevel, appState.GpuLevel);
						vrapi_SetPerfThread(appState.Ovr, VRAPI_PERF_THREAD_TYPE_MAIN, appState.MainThreadTid);
						__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "		vrapi_SetPerfThread( MAIN, %d )", appState.MainThreadTid);
						vrapi_SetPerfThread(appState.Ovr, VRAPI_PERF_THREAD_TYPE_RENDERER, appState.RenderThreadTid);
						__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "		vrapi_SetPerfThread( RENDERER, %d )", appState.RenderThreadTid);
					}
				}
			}
			else if (appState.Ovr != nullptr)
			{
				__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "        vrapi_LeaveVrMode()");
				vrapi_LeaveVrMode(appState.Ovr);
				appState.Ovr = nullptr;
			}
		}
		ovrEventDataBuffer eventDataBuffer { };
		for (;;)
		{
			ovrEventHeader *eventHeader = (ovrEventHeader *)(&eventDataBuffer);
			ovrResult res = vrapi_PollEvent(eventHeader);
			if (res != ovrSuccess)
			{
				break;
			}
			switch (eventHeader->EventType)
			{
				case VRAPI_EVENT_DATA_LOST:
					__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "vrapi_PollEvent: Received VRAPI_EVENT_DATA_LOST");
					break;
				case VRAPI_EVENT_VISIBILITY_GAINED:
					__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "vrapi_PollEvent: Received VRAPI_EVENT_VISIBILITY_GAINED");
					break;
				case VRAPI_EVENT_VISIBILITY_LOST:
					__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "vrapi_PollEvent: Received VRAPI_EVENT_VISIBILITY_LOST");
					break;
				case VRAPI_EVENT_FOCUS_GAINED:
					__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "vrapi_PollEvent: Received VRAPI_EVENT_FOCUS_GAINED");
					break;
				case VRAPI_EVENT_FOCUS_LOST:
					__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "vrapi_PollEvent: Received VRAPI_EVENT_FOCUS_LOST");
					break;
				default:
					__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "vrapi_PollEvent: Unknown event");
					break;
			}
		}
		bool backButtonDownThisFrame = false;
		for (int i = 0;; i++)
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
					backButtonDownThisFrame |= trackedRemoteState.Buttons & ovrButton_Back;
					backButtonDownThisFrame |= trackedRemoteState.Buttons & ovrButton_B;
					backButtonDownThisFrame |= trackedRemoteState.Buttons & ovrButton_Y;
				}
			}
		}
		bool backButtonDownLastFrame = appState.BackButtonDownLastFrame;
		appState.BackButtonDownLastFrame = backButtonDownThisFrame;
		if (backButtonDownLastFrame && !backButtonDownThisFrame)
		{
			__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "back button short press");
			__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "        vrapi_ShowSystemUI( confirmQuit )");
			vrapi_ShowSystemUI(&appState.Java, VRAPI_SYS_UI_CONFIRM_QUIT_MENU);
		}
		if (appState.Ovr == NULL)
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

			// Create the scene.
			ovrScene_Create(
					&appState.Context, &appState.Scene, &appState.Renderer.RenderPassSingleView);
		}
		appState.FrameIndex++;
		const double predictedDisplayTime = vrapi_GetPredictedDisplayTime(appState.Ovr, appState.FrameIndex);
		const ovrTracking2 tracking = vrapi_GetPredictedTracking2(appState.Ovr, predictedDisplayTime);
		appState.DisplayTime = predictedDisplayTime;
		appState.Simulation.CurrentRotation.x = (float)predictedDisplayTime;
		appState.Simulation.CurrentRotation.y = (float)predictedDisplayTime;
		appState.Simulation.CurrentRotation.z = (float)predictedDisplayTime;
		const ovrLayerProjection2 worldLayer = ovrRenderer_RenderFrame(&appState.Renderer, appState.FrameIndex, &appState.Scene, &appState.Simulation, &tracking);
		const ovrLayerHeader2* layers[] = { &worldLayer.Header };
		ovrSubmitFrameDescription2 frameDesc = { };
		frameDesc.SwapInterval = appState.SwapInterval;
		frameDesc.FrameIndex = appState.FrameIndex;
		frameDesc.DisplayTime = appState.DisplayTime;
		frameDesc.LayerCount = 1;
		frameDesc.Layers = layers;
		vrapi_SubmitFrame2(appState.Ovr, &frameDesc);
	}

	ovrRenderer_Destroy(&appState.Renderer, &appState.Context);

	ovrScene_Destroy(&appState.Context, &appState.Scene);

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
	delete[] appState.Device.queueFamilyProperties;
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
