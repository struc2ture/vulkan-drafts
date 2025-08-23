#include <cstdio>
#include <vector>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

#define fatal(FMT, ...) do { \
    fprintf(stderr, "[FATAL: %s:%d:%s]: " FMT "\n", \
        __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    __builtin_debugtrap(); \
    exit(EXIT_FAILURE); \
} while (0)

#define assert(cond) do { \
    if (!(cond)) { \
        __builtin_debugtrap(); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

struct Vertex
{
    float x, y;
    float r, g, b;
};

VkShaderModule create_shader_module(VkDevice device, const char *path)
{
    FILE *file = fopen(path, "rb");
    assert(file);
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    char* buffer = (char *)malloc(size);
    assert(buffer);
    fread(buffer, 1, size, file);
    fclose(file);

    VkShaderModuleCreateInfo shader_module_create_info = {};
    shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_module_create_info.codeSize = (size_t)size;
    shader_module_create_info.pCode = (uint32_t *)buffer;

    VkShaderModule module;
    VkResult result = vkCreateShaderModule(device, &shader_module_create_info, NULL, &module);
    if (result != VK_SUCCESS) fatal("Failed to create shader module");

    free(buffer);
    return module;
}

uint32_t find_memory_type(VkPhysicalDevice physical_device, uint32_t type_filter, VkMemoryPropertyFlags props)
{
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++)
    {
        if ((type_filter & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & props) == props)
        {
            return i;
        }
    }
    fatal("Failed to find suitable memory type");
    return 0;
}

int main()
{
    int width = 1000;
    int height = 900;

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(width, height, "Vulkan", NULL, NULL);

    // Vulkan Instance
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.apiVersion = VK_API_VERSION_1_3;

    uint32_t glfw_ext_count = 0;
    const char **glfw_ext = glfwGetRequiredInstanceExtensions(&glfw_ext_count);

    std::vector<const char *> extensions;
    for (uint32_t i = 0; i < glfw_ext_count; i++)
    {
        extensions.push_back(glfw_ext[i]);
    }
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    std::vector<const char *> validation_layers = {"VK_LAYER_KHRONOS_validation"};

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = (uint32_t)extensions.size();
    create_info.ppEnabledExtensionNames = extensions.data();
    create_info.enabledLayerCount = (uint32_t)validation_layers.size();
    create_info.ppEnabledLayerNames = validation_layers.data();
    create_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

    VkInstance vk_instance;
    VkResult result = vkCreateInstance(&create_info, NULL, &vk_instance);
    if (result != VK_SUCCESS) fatal("Failed to create instance");

    // Surface
    VkSurfaceKHR vk_surface;
    result = glfwCreateWindowSurface(vk_instance, window, NULL, &vk_surface);
    if (result != VK_SUCCESS) fatal("Failed to create surface");

    // Physical device
    uint32_t count;
    vkEnumeratePhysicalDevices(vk_instance, &count, nullptr);
    std::vector<VkPhysicalDevice> physical_devices(count);
    vkEnumeratePhysicalDevices(vk_instance, &count, physical_devices.data());
    VkPhysicalDevice vk_physical_device = physical_devices[0];

    // Find graphics queue
    uint32_t vk_graphics_queue_family_index = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vk_physical_device, &count, NULL);
    std::vector<VkQueueFamilyProperties> queue_families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(vk_physical_device, &count, queue_families.data());
    for (size_t i = 0; i < queue_families.size(); i++)
    {
        VkBool32 present_support;
        vkGetPhysicalDeviceSurfaceSupportKHR(vk_physical_device, i, vk_surface, &present_support);
        if ((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present_support)
        {
            vk_graphics_queue_family_index = i;
        }
    }

    // Logical device
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info = {};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = vk_graphics_queue_family_index;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &priority;

    // VK_KHR_portability_subset must be enabled because physical device VkPhysicalDevice 0x600001667be0 supports it.
    std::vector<const char *> device_extensions = {"VK_KHR_portability_subset", "VK_KHR_swapchain"};
    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pQueueCreateInfos = &queue_create_info;
    device_create_info.enabledExtensionCount = (uint32_t)device_extensions.size();
    device_create_info.ppEnabledExtensionNames = device_extensions.data();

    VkDevice vk_device;
    result = vkCreateDevice(vk_physical_device, &device_create_info, nullptr, &vk_device);
    if (result != VK_SUCCESS) fatal("Failed to create logical device");

    // Get queue handle of the graphics queue family
    VkQueue vk_graphics_queue;
    vkGetDeviceQueue(vk_device,vk_graphics_queue_family_index, 0, &vk_graphics_queue);

    // Swapchain
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_physical_device, vk_surface, &capabilities);

    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physical_device, vk_surface, &format_count, NULL);
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physical_device, vk_surface, &format_count, formats.data());

    VkSurfaceFormatKHR vk_surface_format = formats[0];
    assert(vk_surface_format.format == VK_FORMAT_B8G8R8A8_UNORM && vk_surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
    VkExtent2D vk_swapchain_extent = capabilities.currentExtent;
    uint32_t vk_image_count = 2;

    VkSwapchainCreateInfoKHR swapchain_create_info = {};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = vk_surface;
    swapchain_create_info.minImageCount = vk_image_count;
    swapchain_create_info.imageFormat = vk_surface_format.format;
    swapchain_create_info.imageColorSpace = vk_surface_format.colorSpace;
    swapchain_create_info.imageExtent = vk_swapchain_extent;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.preTransform = capabilities.currentTransform;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR; // vsync
    swapchain_create_info.clipped = VK_TRUE;

    VkSwapchainKHR vk_swapchain;
    result = vkCreateSwapchainKHR(vk_device, &swapchain_create_info, NULL, &vk_swapchain);
    if (result != VK_SUCCESS) fatal("Failed to create swapchain");

    // Get swapchain images
    uint32_t actual_image_count;
    vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &actual_image_count, NULL);
    std::vector<VkImage> vk_swapchain_images(actual_image_count);
    vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &actual_image_count, vk_swapchain_images.data());
    assert(vk_image_count == actual_image_count);

    // Image views
    std::vector<VkImageView> vk_image_views(vk_image_count);
    for (uint32_t i = 0; i < vk_image_count; i++)
    {
        VkImageViewCreateInfo view_create_info = {};
        view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_create_info.image = vk_swapchain_images[i];
        view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_create_info.format = vk_surface_format.format;
        view_create_info.components = {};
        view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_create_info.subresourceRange = {};
        view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_create_info.subresourceRange.baseMipLevel = 0;
        view_create_info.subresourceRange.levelCount = 1;
        view_create_info.subresourceRange.baseArrayLayer = 0;
        view_create_info.subresourceRange.layerCount = 1;

        result = vkCreateImageView(vk_device, &view_create_info, NULL, &vk_image_views[i]);
        if (result != VK_SUCCESS) fatal("Failed to create image view");
    }

    // Render pass
    VkAttachmentDescription color_attachment_description = {};
    color_attachment_description.format = vk_surface_format.format;
    color_attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment_description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_reference = {};
    color_attachment_reference.attachment = 0;
    color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass_description = {};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.colorAttachmentCount = 1;
    subpass_description.pColorAttachments = &color_attachment_reference;

    VkRenderPassCreateInfo render_pass_create_info = {};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.attachmentCount = 1;
    render_pass_create_info.pAttachments = &color_attachment_description;
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass_description;

    VkRenderPass vk_render_pass;
    result = vkCreateRenderPass(vk_device, &render_pass_create_info, NULL, &vk_render_pass);
    if (result != VK_SUCCESS) fatal("Failed to create render pass");

    // Framebuffers
    std::vector<VkFramebuffer> vk_framebuffers(vk_image_count);
    for (uint32_t i = 0; i < vk_image_count; i++)
    {
        VkImageView attachments[] = { vk_image_views[i] };

        VkFramebufferCreateInfo framebuffer_create_info = {};
        framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_create_info.renderPass = vk_render_pass;
        framebuffer_create_info.attachmentCount = 1;
        framebuffer_create_info.pAttachments = attachments;
        framebuffer_create_info.width = vk_swapchain_extent.width;
        framebuffer_create_info.height = vk_swapchain_extent.height;
        framebuffer_create_info.layers = 1;

        result = vkCreateFramebuffer(vk_device, &framebuffer_create_info, NULL, &vk_framebuffers[i]);
        if (result != VK_SUCCESS) fatal("Failed to create framebuffer");
    }

    ////////////////////////////////////////////

    // Graphics pipeline
    VkShaderModule vk_vert_shader_module = create_shader_module(vk_device, "bin/shaders/tri.vert.spv");
    VkShaderModule vk_frag_shader_module = create_shader_module(vk_device, "bin/shaders/tri.frag.spv");

    VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_infos[2] = {};
    pipeline_shader_stage_create_infos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_shader_stage_create_infos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    pipeline_shader_stage_create_infos[0].module = vk_vert_shader_module;
    pipeline_shader_stage_create_infos[0].pName = "main";
    pipeline_shader_stage_create_infos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_shader_stage_create_infos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    pipeline_shader_stage_create_infos[1].module = vk_frag_shader_module;
    pipeline_shader_stage_create_infos[1].pName = "main";

    VkVertexInputBindingDescription vertex_input_binding_description = {};
    vertex_input_binding_description.binding = 0;
    vertex_input_binding_description.stride = sizeof(Vertex);
    vertex_input_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> vertex_input_attribute_descriptions(2);
    vertex_input_attribute_descriptions[0].location = 0;
    vertex_input_attribute_descriptions[0].binding = 0;
    vertex_input_attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    vertex_input_attribute_descriptions[0].offset = offsetof(Vertex, x);
    vertex_input_attribute_descriptions[1].location = 1;
    vertex_input_attribute_descriptions[1].binding = 0;
    vertex_input_attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_input_attribute_descriptions[1].offset = offsetof(Vertex, r);

    VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_state_create_info = {};
    pipeline_vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    pipeline_vertex_input_state_create_info.vertexBindingDescriptionCount = 1;
    pipeline_vertex_input_state_create_info.pVertexBindingDescriptions = &vertex_input_binding_description;
    pipeline_vertex_input_state_create_info.vertexAttributeDescriptionCount = vertex_input_attribute_descriptions.size();
    pipeline_vertex_input_state_create_info.pVertexAttributeDescriptions = vertex_input_attribute_descriptions.data();

    VkPipelineInputAssemblyStateCreateInfo pipeline_input_assembly_create_info = {};
    pipeline_input_assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipeline_input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport = {0, 0, (float)width, (float)height, 0.0f, 1.0f};
    VkRect2D scissor = {{0, 0}, {(uint32_t)width, (uint32_t)height}};
    VkPipelineViewportStateCreateInfo pipeline_viewport_state_create_info = {};
    pipeline_viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    pipeline_viewport_state_create_info.viewportCount = 1;
    pipeline_viewport_state_create_info.pViewports = &viewport;
    pipeline_viewport_state_create_info.scissorCount = 1;
    pipeline_viewport_state_create_info.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo pipeline_rasterization_state_create_info = {};
    pipeline_rasterization_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    pipeline_rasterization_state_create_info.polygonMode = VK_POLYGON_MODE_FILL;
    pipeline_rasterization_state_create_info.lineWidth = 1.0f;
    pipeline_rasterization_state_create_info.cullMode = VK_CULL_MODE_NONE;
    pipeline_rasterization_state_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo pipeline_multisample_state_create_info = {};
    pipeline_multisample_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    pipeline_multisample_state_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState pipeline_color_blend_attachment_state = {};
    pipeline_color_blend_attachment_state.colorWriteMask = (VK_COLOR_COMPONENT_R_BIT |
                                                            VK_COLOR_COMPONENT_G_BIT |
                                                            VK_COLOR_COMPONENT_B_BIT |
                                                            VK_COLOR_COMPONENT_A_BIT);
    pipeline_color_blend_attachment_state.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info = {};
    pipeline_color_blend_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    pipeline_color_blend_state_create_info.attachmentCount = 1;
    pipeline_color_blend_state_create_info.pAttachments = &pipeline_color_blend_attachment_state;

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    VkPipelineLayout vk_pipeline_layout;
    result = vkCreatePipelineLayout(vk_device, &pipeline_layout_create_info, nullptr, &vk_pipeline_layout);
    if (result != VK_SUCCESS) fatal("Failed to create pipeline layout");
    
    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {};
    graphics_pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    graphics_pipeline_create_info.stageCount = 2;
    graphics_pipeline_create_info.pStages = pipeline_shader_stage_create_infos;
    graphics_pipeline_create_info.pVertexInputState = &pipeline_vertex_input_state_create_info;
    graphics_pipeline_create_info.pInputAssemblyState = &pipeline_input_assembly_create_info;
    graphics_pipeline_create_info.pViewportState = &pipeline_viewport_state_create_info;
    graphics_pipeline_create_info.pRasterizationState = &pipeline_rasterization_state_create_info;
    graphics_pipeline_create_info.pMultisampleState = &pipeline_multisample_state_create_info;
    graphics_pipeline_create_info.pColorBlendState = &pipeline_color_blend_state_create_info;
    graphics_pipeline_create_info.layout = vk_pipeline_layout;
    graphics_pipeline_create_info.renderPass = vk_render_pass;
    graphics_pipeline_create_info.subpass = 0;
    
    VkPipeline vk_pipeline;
    result = vkCreateGraphicsPipelines(vk_device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &vk_pipeline);
    if (result != VK_SUCCESS) fatal("Failed to create graphics pipeline");

    vkDestroyShaderModule(vk_device, vk_vert_shader_module, nullptr);
    vkDestroyShaderModule(vk_device, vk_frag_shader_module, nullptr);

    // Vertex buffer
    const Vertex verts[] = {
        {  0.0f, -0.5f, 1.0f, 0.0f, 0.0f },
        {  0.5f,  0.5f, 0.0f, 1.0f, 0.0f },
        { -0.5f,  0.5f, 0.0f, 0.0f, 1.0f },
    };

    VkDeviceSize buffer_size = sizeof(verts);

    VkBufferCreateInfo buffer_create_info = {};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size = buffer_size;
    buffer_create_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer vk_vertex_buffer;
    result = vkCreateBuffer(vk_device, &buffer_create_info, nullptr, &vk_vertex_buffer);
    if (result != VK_SUCCESS) fatal("Failed to create vertex buffer");

    // Allocate memory for vertex buffer
    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(vk_device, vk_vertex_buffer, &memory_requirements);

    VkMemoryAllocateInfo memory_allocate_info = {};
    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.allocationSize = memory_requirements.size;
    memory_allocate_info.memoryTypeIndex = find_memory_type(
        vk_physical_device,
        memory_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );;

    VkDeviceMemory vk_vertex_buffer_memory;
    result = vkAllocateMemory(vk_device, &memory_allocate_info, NULL, &vk_vertex_buffer_memory);
    if (result != VK_SUCCESS) fatal("Failed to allocate memory for vertex buffer");

    result = vkBindBufferMemory(vk_device, vk_vertex_buffer, vk_vertex_buffer_memory, 0);
    if (result != VK_SUCCESS) fatal("Failed to bind memory to vertex buffer");

    // Upload data to the vertex buffer
    void *data;
    vkMapMemory(vk_device, vk_vertex_buffer_memory, 0, buffer_size, 0, &data);
    memcpy(data, verts, (size_t)buffer_size);
    vkUnmapMemory(vk_device, vk_vertex_buffer_memory);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }

    vkDestroyBuffer(vk_device, vk_vertex_buffer, NULL);
    vkFreeMemory(vk_device, vk_vertex_buffer_memory, NULL);
    vkDestroyPipeline(vk_device, vk_pipeline, nullptr);
    vkDestroyPipelineLayout(vk_device, vk_pipeline_layout, nullptr);
    for (auto framebuffer: vk_framebuffers)
    {
        vkDestroyFramebuffer(vk_device, framebuffer, nullptr);
    }
    vkDestroyRenderPass(vk_device, vk_render_pass, nullptr);
    for (auto image_view: vk_image_views)
    {
        vkDestroyImageView(vk_device, image_view, nullptr);
    }
    vkDestroySwapchainKHR(vk_device, vk_swapchain, nullptr);
    vkDestroyDevice(vk_device, nullptr);
    vkDestroySurfaceKHR(vk_instance, vk_surface, nullptr);
    vkDestroyInstance(vk_instance, nullptr);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
