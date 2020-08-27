#include "vk.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "util.h"

const char* vk_instance_extensions[] = {
	"VK_KHR_surface",
	"VK_KHR_display",
};
const char* vk_device_extensions[] = {
	"VK_KHR_swapchain"
};
#ifdef DEBUG
const char* vk_validation_layers[] = {
	"VK_LAYER_KHRONOS_validation"
};
#endif


bool physical_device_suitable(VkPhysicalDevice device) {
	return true;
}

bool load_shader(const char* path, uint8_t** shader_data, size_t* shader_len) {
	FILE* shader_file = fopen(path, "rb");
	if (!shader_file)
		return false;
	
	fseek(shader_file, 0, SEEK_END);
	*shader_len = ftell(shader_file);
	fseek(shader_file, 0, SEEK_SET);
	if (*shader_len < 0)
		return false;

	*shader_data = malloc(*shader_len);
	size_t index = 0;
	for (int byte; (byte = fgetc(shader_file)) != EOF && index < *shader_len; index++)
		(*shader_data)[index] = (uint8_t)byte;
	
	if (ferror(shader_file) || index < *shader_len)
		return false;

	fclose(shader_file);
	return true;
}

Vulkan vk_setup(void) {
	Vulkan vk;
	vk.physical_device = VK_NULL_HANDLE;
	vk.swapchain_image_len = 0;
	vk.present_mode = VK_PRESENT_MODE_FIFO_KHR;

	VkApplicationInfo vk_appinfo = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Wayvk",
		.applicationVersion = VK_MAKE_VERSION(0, 0, 1),
		.pEngineName = "No Engine",
		.engineVersion = VK_MAKE_VERSION(0, 0, 1),
		.apiVersion = VK_API_VERSION_1_0
	};
	VkInstanceCreateInfo vk_instance_info = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &vk_appinfo,
		.enabledExtensionCount = sizeof(vk_instance_extensions) / sizeof(*vk_instance_extensions),
		.ppEnabledExtensionNames = vk_instance_extensions,
		#ifdef DEBUG
			.enabledLayerCount = 1,
			.ppEnabledLayerNames = vk_validation_layers
		#else
			.enabledLayerCount = 0,
		#endif
	};

	if (vkCreateInstance(&vk_instance_info, NULL, &vk.instance) != VK_SUCCESS)
		panic("Error creating instance");

	uint32_t device_len = 0;
	vkEnumeratePhysicalDevices(vk.instance, &device_len, NULL);
	if (device_len <= 0)
		panic("No Vulkan-compatible physical devices could be found");
	VkPhysicalDevice* devices = malloc(sizeof(VkPhysicalDevice) * device_len);
	vkEnumeratePhysicalDevices(vk.instance, &device_len, devices);
	for (int index = 0; index < device_len; index++) {
		if (physical_device_suitable(devices[index])) {
			vk.physical_device = devices[index];
			break;
		}
	}
	free(devices);

	if (vk.physical_device == VK_NULL_HANDLE)
		panic("No suitable physical devices could be found");

	uint32_t queue_family_len = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(vk.physical_device, &queue_family_len, NULL);
	VkQueueFamilyProperties* queue_family_properties = malloc(sizeof(VkQueueFamilyProperties) * queue_family_len);
	vkGetPhysicalDeviceQueueFamilyProperties(vk.physical_device, &queue_family_len, queue_family_properties);
	for (int index = 0; index < queue_family_len; index++) {
		if (queue_family_properties[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			vk.queue_family = index;
			break;
		}
	}
	free(queue_family_properties);

	float vk_queue_priorities = { 1.0f };
	VkDeviceQueueCreateInfo vk_queue_info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = vk.queue_family,
		.queueCount = 1,
		.pQueuePriorities = &vk_queue_priorities
	};
	VkPhysicalDeviceFeatures vk_device_features = { VK_FALSE };

	VkDeviceCreateInfo vk_device_info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &vk_queue_info,
		.pEnabledFeatures = &vk_device_features,
		.enabledExtensionCount = sizeof(vk_device_extensions) / sizeof(*vk_device_extensions),
		.ppEnabledExtensionNames = vk_device_extensions
	};
	if (vkCreateDevice(vk.physical_device, &vk_device_info, NULL, &vk.device) != VK_SUCCESS)
		panic("Unable to create device");
	vkGetDeviceQueue(vk.device, vk.queue_family, 0, &vk.queue);

	// Get Display info
	uint32_t display_len = 0;
	vkGetPhysicalDeviceDisplayPropertiesKHR(vk.physical_device, &display_len, NULL);
	if (display_len == 0)
		panic("Unable to get a direct display");
	VkDisplayPropertiesKHR* displays = malloc(sizeof(VkDisplayPropertiesKHR) * display_len);
	vkGetPhysicalDeviceDisplayPropertiesKHR(vk.physical_device, &display_len, displays);
	for (int index = 0; index < display_len; index++) {
		vk.display = displays[index].display;
		vk.display_properties = displays[index];
		break;
	}
	free(displays);

	// Get Display Plane Info
	bool display_plane_found = false;
	uint32_t display_properties_len = 0;
	vkGetPhysicalDeviceDisplayPlanePropertiesKHR(vk.physical_device, &display_properties_len, NULL);
	if (display_properties_len == 0)
		panic("No valid raw Vulkan display found");
	VkDisplayPlanePropertiesKHR* display_properties = malloc(sizeof(VkDisplayPlanePropertiesKHR) * display_properties_len);
	vkGetPhysicalDeviceDisplayPlanePropertiesKHR(vk.physical_device, &display_properties_len, display_properties);
	for (int index = 0; index < display_properties_len; index++) {
		if (display_properties[index].currentDisplay == NULL || display_properties[index].currentDisplay == vk.display) {
			vk.display = display_properties[index].currentDisplay;
			vk.display_plane = index;
			vk.display_stack = display_properties[index].currentStackIndex;
			display_plane_found = true;
			break;
		}
	}
	free(display_properties);
	if (!display_plane_found)
		panic("Unable to find a suitable display plane");

	// Get Raw Display Mode Info
	uint32_t display_mode_len = 0;
	vkGetDisplayModePropertiesKHR(vk.physical_device, vk.display, &display_mode_len, NULL);
	if (display_mode_len == 0)
		panic("No valid raw Vulkan display mode found");
	VkDisplayModePropertiesKHR* display_modes = malloc(sizeof(VkDisplayModePropertiesKHR) * display_mode_len);
	vkGetDisplayModePropertiesKHR(vk.physical_device, vk.display, &display_mode_len, display_modes);
	for (int index = 0; index < display_mode_len; index++) {
		vk.display_mode = display_modes[index].displayMode;
		vk.display_mode_params = display_modes[index].parameters;
		break;
	}
	free(display_modes);

	// Create Display Surface
	VkDisplaySurfaceCreateInfoKHR vk_surface_info = {
		.sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR,
		.displayMode = vk.display_mode,
		.planeIndex = vk.display_plane,
		.planeStackIndex = vk.display_stack,
		.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR,
		.imageExtent = vk.display_mode_params.visibleRegion
	};

	if (vkCreateDisplayPlaneSurfaceKHR(vk.instance, &vk_surface_info, NULL, &vk.surface) != VK_SUCCESS)
		panic("Unable to create surface");

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk.physical_device, vk.surface, &vk.surface_capabilities);
	
	// Get supported surface formats
	bool found_format = false;
	uint32_t format_len = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(vk.physical_device, vk.surface, &format_len, NULL);
	if (format_len == 0)
		panic("No supported surface formats");
	VkSurfaceFormatKHR* formats = malloc(sizeof(VkSurfaceFormatKHR) * format_len);
	vkGetPhysicalDeviceSurfaceFormatsKHR(vk.physical_device, vk.surface, &format_len, formats);
	for (int index = 0; index < format_len; index++) {
			if (formats[index].format == VK_FORMAT_B8G8R8A8_SRGB && formats[index].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				vk.surface_format = formats[index];
				found_format = true;
				break;
			}
	}
	free(formats);
	if (!found_format)
		panic("Could not find an acceptable surface format");

	uint32_t present_mode_len = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(vk.physical_device, vk.surface, &present_mode_len, NULL);
	if (present_mode_len == 0)
		panic("No supported present mode");
	VkPresentModeKHR* present_modes = malloc(sizeof(VkPresentModeKHR) * present_mode_len);
	vkGetPhysicalDeviceSurfacePresentModesKHR(vk.physical_device, vk.surface, &present_mode_len, present_modes);
	for (int index = 0; index < present_mode_len; index++) {
		if (present_modes[index] == VK_PRESENT_MODE_MAILBOX_KHR) {
			vk.present_mode = present_modes[index];
			break;
		}
	}
	free(present_modes);

	if (vk.surface_capabilities.currentExtent.width != UINT32_MAX)
		vk.swapchain_extent = vk.surface_capabilities.currentExtent;
	else
		vk.swapchain_extent = vk.display_mode_params.visibleRegion;

	VkSwapchainCreateInfoKHR vk_swapchain_info = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = vk.surface,
		.minImageCount = vk.surface_capabilities.minImageCount + vk.surface_capabilities.maxImageCount > vk.surface_capabilities.minImageCount ? 1 : 0,
		.imageFormat = vk.surface_format.format,
		.imageColorSpace = vk.surface_format.colorSpace,
		.imageExtent = vk.swapchain_extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.preTransform = vk.surface_capabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = vk.present_mode,
		.clipped = VK_TRUE,
		.oldSwapchain = VK_NULL_HANDLE
	};

	if (vkCreateSwapchainKHR(vk.device, &vk_swapchain_info, NULL, &vk.swapchain) != VK_SUCCESS)
		panic("Unable to create swapchain\nIs the display already in use by Xorg or a Wayland compositor?");

	// Get the swapchain images
	vkGetSwapchainImagesKHR(vk.device, vk.swapchain, &vk.swapchain_image_len, NULL);
	vk.swapchain_images = malloc(sizeof(Image) * vk.swapchain_image_len);
	VkImage* swapchain_image_buffer = malloc(sizeof(VkImage) * vk.swapchain_image_len);
	vkGetSwapchainImagesKHR(vk.device, vk.swapchain, &vk.swapchain_image_len, swapchain_image_buffer);
	VkImageViewCreateInfo vk_image_view_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = vk.surface_format.format,
		.components = { VK_COMPONENT_SWIZZLE_IDENTITY },
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};
	for (int index = 0; index < vk.swapchain_image_len; index++) {
		vk_image_view_info.image = vk.swapchain_images[index].image = swapchain_image_buffer[index];
		if (vkCreateImageView(vk.device, &vk_image_view_info, NULL, &vk.swapchain_images[index].view) != VK_SUCCESS)
			panic("Unable to create swapchain image view");
	}
	free(swapchain_image_buffer);

	// Create the graphics pipeline
	
	VkPipelineVertexInputStateCreateInfo vk_vertex_input_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 0,
		.vertexAttributeDescriptionCount = 0
	};
	VkPipelineInputAssemblyStateCreateInfo vk_input_assembly_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE
	};
	VkViewport vk_viewport = {
		.x = 0.0f,
		.y = 0.0f,
		.width = (float) vk.swapchain_extent.width,
		.height = (float) vk.swapchain_extent.height,
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};
	VkRect2D vk_scissor = {
		.offset = { 0 },
		.extent = vk.swapchain_extent
	};
	VkPipelineViewportStateCreateInfo vk_viewport_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = &vk_viewport,
		.scissorCount = 1,
		.pScissors = &vk_scissor
	};
	VkPipelineRasterizationStateCreateInfo vk_raster_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.lineWidth = 1.0f,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
	};
	VkPipelineMultisampleStateCreateInfo vk_multisample_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.sampleShadingEnable = VK_FALSE,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
	};
	VkPipelineColorBlendAttachmentState vk_framebuffer_blend_state = {
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		.blendEnable = VK_FALSE,
	};
	VkPipelineColorBlendStateCreateInfo vk_framebuffer_blend_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.attachmentCount = 1,
		.pAttachments = &vk_framebuffer_blend_state
	};
	VkPipelineLayoutCreateInfo vk_layout_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
	};
	if (vkCreatePipelineLayout(vk.device, &vk_layout_info, NULL, &vk.pipeline_layout) != VK_SUCCESS)
		panic("Unable to create pipeline layout");

	VkAttachmentDescription vk_framebuffer_attachment = {
		.format = vk.surface_format.format,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	};
	VkAttachmentReference vk_framebuffer_attachment_reference = {
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};
	VkSubpassDescription vk_present_pass = {
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &vk_framebuffer_attachment_reference
	};
	VkSubpassDependency vk_present_pass_dependency = {
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = 0,
		.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
	};

	VkRenderPassCreateInfo vk_renderpass_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &vk_framebuffer_attachment,
		.subpassCount = 1,
		.pSubpasses = &vk_present_pass,
		.dependencyCount = 1,
		.pDependencies = &vk_present_pass_dependency
	};

	if (vkCreateRenderPass(vk.device, &vk_renderpass_info, NULL, &vk.renderpass) != VK_SUCCESS)
		panic("Unable to create renderpass");

	// Load shaders
	size_t vert_shader_len;
	uint8_t* vert_shader;
	size_t frag_shader_len;
	uint8_t* frag_shader;

	if (!load_shader("shader/basic.vert.spv", &vert_shader, &vert_shader_len))
		panic("Failed to load vertex shader");
	if (!load_shader("shader/basic.frag.spv", &frag_shader, &frag_shader_len))
		panic("Failed to load fragment shader");

	VkShaderModuleCreateInfo vk_vert_shader_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = vert_shader_len,
		.pCode = (uint32_t*)vert_shader
	};
	if (vkCreateShaderModule(vk.device, &vk_vert_shader_info, NULL, &vk.vert_shader) != VK_SUCCESS)
		panic("Unable to create vertex shader module");
	VkShaderModuleCreateInfo vk_frag_shader_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = frag_shader_len,
		.pCode = (uint32_t*)frag_shader
	};
	if (vkCreateShaderModule(vk.device, &vk_frag_shader_info, NULL, &vk.frag_shader) != VK_SUCCESS)
		panic("Unable to create fragment shader module");
	
	VkPipelineShaderStageCreateInfo vk_vert_stage_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.module = vk.vert_shader,
		.pName = "main"
	};
	VkPipelineShaderStageCreateInfo vk_frag_stage_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = vk.frag_shader,
		.pName = "main"
	};
	VkPipelineShaderStageCreateInfo vk_shader_stages[] = {vk_vert_stage_info, vk_frag_stage_info};

	VkGraphicsPipelineCreateInfo vk_pipeline_info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = 2,
		.pStages = vk_shader_stages,
		.pVertexInputState = &vk_vertex_input_info,
		.pInputAssemblyState = &vk_input_assembly_info,
		.pViewportState = &vk_viewport_info,
		.pRasterizationState = &vk_raster_info,
		.pMultisampleState = &vk_multisample_info,
		.pDepthStencilState = NULL,
		.pColorBlendState = &vk_framebuffer_blend_info,
		.pDynamicState = NULL,
		.layout = vk.pipeline_layout,
		.renderPass = vk.renderpass,
		.subpass = 0,
		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = -1,
	};
	if (vkCreateGraphicsPipelines(vk.device, VK_NULL_HANDLE, 1, &vk_pipeline_info, NULL, &vk.pipeline) != VK_SUCCESS)
		panic("Unable to create graphics pipeline");

	// Create framebuffers
	vk.framebuffers = malloc(sizeof(VkFramebuffer) * vk.swapchain_image_len);
	VkFramebufferCreateInfo vk_framebuffer_info = {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = vk.renderpass,
		.attachmentCount = 1,
		.width = vk.swapchain_extent.width,
		.height = vk.swapchain_extent.height,
		.layers = 1
	};
	for (int index = 0; index < vk.swapchain_image_len; index++) {
		vk_framebuffer_info.pAttachments = &vk.swapchain_images[index].view;
		if (vkCreateFramebuffer(vk.device, &vk_framebuffer_info, NULL, &vk.framebuffers[index]) != VK_SUCCESS)
			panic("Unable to create framebuffer");
	}

	// Create command buffers
	VkCommandPoolCreateInfo vk_command_pool_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.queueFamilyIndex = vk.queue_family,
	};
	if (vkCreateCommandPool(vk.device, &vk_command_pool_info, NULL, &vk.command_pool) != VK_SUCCESS)
		panic("Unable to create command pool");

	vk.command_buffers = malloc(sizeof(VkCommandBuffer) * vk.swapchain_image_len);
	VkCommandBufferAllocateInfo vk_command_buffer_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = vk.command_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = vk.swapchain_image_len
	};
	if (vkAllocateCommandBuffers(vk.device, &vk_command_buffer_info, vk.command_buffers) != VK_SUCCESS)
		panic("Unable to allocate command buffers");

	// Create semaphores
	VkSemaphoreCreateInfo vk_semaphore_info = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
	};
	if (
		vkCreateSemaphore(vk.device, &vk_semaphore_info, NULL, &vk.render_semaphore) != VK_SUCCESS ||
		vkCreateSemaphore(vk.device, &vk_semaphore_info, NULL, &vk.present_semaphore) != VK_SUCCESS
	) panic("Unable to create semaphores");

	return vk;
}

void vk_cleanup(Vulkan* vk) {
	vkDeviceWaitIdle(vk->device);
	vkDestroySemaphore(vk->device, vk->render_semaphore, NULL);
	vkDestroySemaphore(vk->device, vk->present_semaphore, NULL);
	vkDestroyCommandPool(vk->device, vk->command_pool, NULL);
	free(vk->command_buffers);

	for (int index = 0; index < vk->swapchain_image_len; index++)
		vkDestroyFramebuffer(vk->device, vk->framebuffers[index], NULL);
	vkDestroyPipeline(vk->device, vk->pipeline, NULL);
	vkDestroyPipelineLayout(vk->device, vk->pipeline_layout, NULL);
	vkDestroyShaderModule(vk->device, vk->vert_shader, NULL);
	vkDestroyShaderModule(vk->device, vk->frag_shader, NULL);
	vkDestroyRenderPass(vk->device, vk->renderpass, NULL);
	for (int index = 0; index < vk->swapchain_image_len; index++)
		vkDestroyImageView(vk->device, vk->swapchain_images[index].view, NULL);
	free(vk->swapchain_images);
	vkDestroySwapchainKHR(vk->device, vk->swapchain, NULL);
	vkDestroySurfaceKHR(vk->instance, vk->surface, NULL);
	vkDestroyDevice(vk->device, NULL);
	vkDestroyInstance(vk->instance, NULL);
}

void vk_draw(Vulkan* vk) {
	bool running = true;
	while (running) {
	uint32_t image_index;
	vkAcquireNextImageKHR(vk->device, vk->swapchain, UINT64_MAX, vk->render_semaphore, VK_NULL_HANDLE, &image_index);

	VkCommandBufferBeginInfo vk_command_begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
	};
	if (vkBeginCommandBuffer(vk->command_buffers[image_index], &vk_command_begin_info) != VK_SUCCESS)
		panic("Unable to start command buffer");

	VkClearValue vk_clear_values[] = {
		{ 0.0f, 1.0f, 0.0f, 1.0f }
	};
	VkRenderPassBeginInfo vk_renderpass_begin_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = vk->renderpass,
		.framebuffer = vk->framebuffers[image_index],
		.renderArea = {
			.offset = { 0, 0 },
			.extent = vk->swapchain_extent
		},
		.clearValueCount = 1,
		.pClearValues = vk_clear_values,
	};
	vkCmdBeginRenderPass(vk->command_buffers[image_index], &vk_renderpass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(vk->command_buffers[image_index], VK_PIPELINE_BIND_POINT_GRAPHICS, vk->pipeline);
	vkCmdEndRenderPass(vk->command_buffers[image_index]);


	if (vkEndCommandBuffer(vk->command_buffers[image_index]) != VK_SUCCESS)
		panic("Unable to complete command buffer");

	VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSubmitInfo vk_submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &vk->render_semaphore,
		.pWaitDstStageMask = wait_stages,
		.commandBufferCount = 1,
		.pCommandBuffers = &vk->command_buffers[image_index],
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &vk->present_semaphore
	};

	if (vkQueueSubmit(vk->queue, 1, &vk_submit_info, VK_NULL_HANDLE) != VK_SUCCESS)
		panic("Unable to submit render queue");
	// Present
	VkPresentInfoKHR vk_present_info = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &vk->present_semaphore,
		.swapchainCount = 1,
		.pSwapchains = &vk->swapchain,
		.pImageIndices = &image_index
	};
	if (vkQueuePresentKHR(vk->queue, &vk_present_info) != VK_SUCCESS)
		panic("Unable to present the swapchain");

	}
}