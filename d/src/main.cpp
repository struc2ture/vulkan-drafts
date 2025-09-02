/* WHAT CREATE_BASICALLY_EVERYTHING DOES:
 * 1. Query physical device-surface capabilities and formats
 * 2. Create swapchain
 * 3. Get swapchain images
 * 4. Create image views for swapchain images
 * 5. Create framebuffers with image view attachments
 * 6. Create uniform buffer for MVP
 * 7. Texture:
 *     a. Upload texture to staging buffer
 *     b. Copy texture from staging buffer into an image using a one-time command buffer
 *     c. Create texture image view
 *     d. Create texture sampler
 * 9. Descriptor set:
 *     a. layout (binding for uniform buffer and for texture sampler)
 *     b. Descriptor pool
 *     c. Allocate descriptor sets
 *     d. Update desctiptor sets to point bindings into uniform buffer and texture image
 * 10. Graphics pipeline:
 *     a. Create shader modules
 *     b. Specify pipeline shader stages
 *     c. Specify vertex input state (input bindings (i.e. to buffers) and input attributes) and input assembly state (e.g. topology - triangle list)
 *     d. Specify viewport state -- viewport and scissor
 *     e. Specify rasterization state -- polygon mode (fill, line, point), line width, cull mode, front face
 *     f. Specify multisample state -- rasterization samples, e.g. 1 sample count
 *     g. Specify color blend state -- attachments -- color write mask and enable/disable blend
 *     h. Create pipeline layout, reference desriptor set layout created previously
 *     i. Create graphics pipeline
 * 11. Can destroy shade modules
 * 12. Create image available and render finished semaphores
 */

/* OTHER INIT DONE IN MAIN:
 * 1. Create instance:
 *     a. Specify extensions: GLFW-required + other required
 *     b. Specify validation layer
 * 2. Create surface (glfw helper)
 * 3. Enumerate and choose physical device
 * 4. Find graphics queue with present support for physical device
 * 5. Create logical device:
 *     a. Device queue for the graphics queue index found above
 *     b. Specify device extensions: swapchain extension
 * 6. Create vertex buffer and upload triangle vertices
 * 7. Create index buffer and upload
 * 8. Create the main command pool and command buffer
 * 9. Call create_basically_everything
 */

#include <cstdio>
#include <vector>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "lin_math.hpp"

#define fatal(FMT, ...) do { \
    fprintf(stderr, "[FATAL: %s:%d:%s]: " FMT "\n", \
        __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    __builtin_debugtrap(); \
    exit(EXIT_FAILURE); \
} while (0)

#define trace(FMT, ...) do { \
    printf("[TRACE: %s:%d:%s]: " FMT "\n", \
    __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
} while (0)

#undef assert
#define assert(cond) do { \
    if (!(cond)) { \
        __builtin_debugtrap(); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

#define bp() __builtin_debugtrap()

struct Vertex
{
    float x, y;
    float u, v;
    float r, g, b;
};

// UNUSED
struct VulkanState
{
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    uint32_t graphics_queue_family_index;
    VkDevice device;
    VkQueue graphics_queue;
    VkSurfaceFormatKHR surface_format;
    VkExtent2D swapchain_extent;
    uint32_t image_count;
    VkSwapchainKHR swapchain;
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;
    VkRenderPass render_pass;
    std::vector<VkFramebuffer> framebuffers;
    VkShaderModule vert_shader_module;
    VkShaderModule frag_shader_module;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_memory;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    VkSemaphore image_available_semaphore;
    VkSemaphore render_finished_semaphore;
};

// UNUSED
static VulkanState g_VulkanState;

struct VulkanBasicallyEverything
{
    VkSwapchainKHR swapchain;
    VkExtent2D swapchain_extent;
    std::vector<VkImageView> image_views;
    std::vector<VkFramebuffer> framebuffers;
    VkRenderPass render_pass;

    VkBuffer uniform_buffer;
    VkDeviceMemory uniform_buffer_memory;

    VkImage texture_image;
    VkDeviceMemory texture_image_memory;
    VkImageView texture_image_view;
    VkSampler texture_sampler;

    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;

    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

    VkSemaphore image_available_semaphore;
    VkSemaphore render_finished_semaphore;
};

static VulkanBasicallyEverything g_TempVulkan;

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

void create_basically_everything(GLFWwindow *window, VkPhysicalDevice vk_physical_device, VkSurfaceKHR vk_surface, VkDevice vk_device, VkQueue vk_graphics_queue, VkCommandPool vk_command_pool)
{
    VkResult result;

    VkSurfaceCapabilitiesKHR capabilities;
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_physical_device, vk_surface, &capabilities);
    if (result != VK_SUCCESS) fatal("Failed to get physical device-surface capabilities");

    uint32_t format_count;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physical_device, vk_surface, &format_count, NULL);
    if (result != VK_SUCCESS) fatal("Failed to get physical device-surface formats");
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physical_device, vk_surface, &format_count, formats.data());
    if (result != VK_SUCCESS) fatal("Failed to get physical device-surface formats 2");

    VkSurfaceFormatKHR vk_surface_format = formats[0];
    assert(vk_surface_format.format == VK_FORMAT_B8G8R8A8_UNORM && vk_surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
    g_TempVulkan.swapchain_extent = capabilities.currentExtent;
    uint32_t vk_image_count = 2;

    VkSwapchainCreateInfoKHR swapchain_create_info = {};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = vk_surface;
    swapchain_create_info.minImageCount = vk_image_count;
    swapchain_create_info.imageFormat = vk_surface_format.format;
    swapchain_create_info.imageColorSpace = vk_surface_format.colorSpace;
    swapchain_create_info.imageExtent = g_TempVulkan.swapchain_extent;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.preTransform = capabilities.currentTransform;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR; // vsync
    swapchain_create_info.clipped = VK_TRUE;

    result = vkCreateSwapchainKHR(vk_device, &swapchain_create_info, NULL, &g_TempVulkan.swapchain);
    if (result != VK_SUCCESS) fatal("Failed to create swapchain");

    // Get swapchain images
    uint32_t actual_image_count;
    result = vkGetSwapchainImagesKHR(vk_device, g_TempVulkan.swapchain, &actual_image_count, NULL);
    if (result != VK_SUCCESS) fatal("Failed to get swapchain images");
    std::vector<VkImage> vk_swapchain_images(actual_image_count);
    result = vkGetSwapchainImagesKHR(vk_device, g_TempVulkan.swapchain, &actual_image_count, vk_swapchain_images.data());
    if (result != VK_SUCCESS) fatal("Failed to get swapchain images 2");
    assert(vk_image_count == actual_image_count);

    // Image views
    g_TempVulkan.image_views.resize(vk_image_count);
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

        result = vkCreateImageView(vk_device, &view_create_info, NULL, &g_TempVulkan.image_views[i]);
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

    result = vkCreateRenderPass(vk_device, &render_pass_create_info, NULL, &g_TempVulkan.render_pass);
    if (result != VK_SUCCESS) fatal("Failed to create render pass");

    // Framebuffers
    g_TempVulkan.framebuffers.resize(vk_image_count);
    for (uint32_t i = 0; i < vk_image_count; i++)
    {
        VkImageView attachments[] = { g_TempVulkan.image_views[i] };

        VkFramebufferCreateInfo framebuffer_create_info = {};
        framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_create_info.renderPass = g_TempVulkan.render_pass;
        framebuffer_create_info.attachmentCount = 1;
        framebuffer_create_info.pAttachments = attachments;
        framebuffer_create_info.width = g_TempVulkan.swapchain_extent.width;
        framebuffer_create_info.height = g_TempVulkan.swapchain_extent.height;
        framebuffer_create_info.layers = 1;

        result = vkCreateFramebuffer(vk_device, &framebuffer_create_info, NULL, &g_TempVulkan.framebuffers[i]);
        if (result != VK_SUCCESS) fatal("Failed to create framebuffer");
    }

    // Create uniform buffer for orthographic projection
    int w, h;
    glfwGetWindowSize(window, &w, &h);
    m4 ortho_proj = m4_proj_ortho(0.0f, w, 0.0f, h, -1.0f, 1.0f);
    VkDeviceSize vk_uniform_buffer_size = sizeof(ortho_proj);

    VkBufferCreateInfo uniform_buffer_create_info = {};
    uniform_buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    uniform_buffer_create_info.size = vk_uniform_buffer_size;
    uniform_buffer_create_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    uniform_buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    result = vkCreateBuffer(vk_device, &uniform_buffer_create_info, nullptr, &g_TempVulkan.uniform_buffer);
    if (result != VK_SUCCESS) fatal("Failed to create uniform buffer");

    // Allocate memory for uniform buffer
    VkMemoryRequirements uniform_buffer_memory_requirements;
    (void)vkGetBufferMemoryRequirements(vk_device, g_TempVulkan.uniform_buffer, &uniform_buffer_memory_requirements);

    VkMemoryAllocateInfo uniform_buffer_memory_allocate_info = {};
    uniform_buffer_memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    uniform_buffer_memory_allocate_info.allocationSize = uniform_buffer_memory_requirements.size;
    uniform_buffer_memory_allocate_info.memoryTypeIndex = find_memory_type(
        vk_physical_device,
        uniform_buffer_memory_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    result = vkAllocateMemory(vk_device, &uniform_buffer_memory_allocate_info, NULL, &g_TempVulkan.uniform_buffer_memory);
    if (result != VK_SUCCESS) fatal("Failed to allocate memory for uniform buffer");

    result = vkBindBufferMemory(vk_device, g_TempVulkan.uniform_buffer, g_TempVulkan.uniform_buffer_memory, 0);
    if (result != VK_SUCCESS) fatal("Failed to bind memory to uniform buffer");

    // Upload data to the uniform buffer
    void *uniform_data_ptr;
    result = vkMapMemory(vk_device, g_TempVulkan.uniform_buffer_memory, 0, vk_uniform_buffer_size, 0, &uniform_data_ptr);
    if (result != VK_SUCCESS) fatal("Failed to map uniform buffer memory");
    memcpy(uniform_data_ptr, &ortho_proj, (size_t)vk_uniform_buffer_size);
    (void)vkUnmapMemory(vk_device, g_TempVulkan.uniform_buffer_memory);

    // Texture
    int tex_w, tex_h, tex_ch;
    stbi_set_flip_vertically_on_load(true);
    stbi_uc *pixels = stbi_load("res/DUCKS.png", &tex_w, &tex_h, &tex_ch, STBI_rgb_alpha);
    VkDeviceSize image_size = tex_w * tex_h * 4; // 4 bytes per pixel

    // Texture staging buffer
    VkBufferCreateInfo texture_staging_buffer_create_info = {};
    texture_staging_buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    texture_staging_buffer_create_info.size = image_size;
    texture_staging_buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    texture_staging_buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer vk_texture_staging_buffer;
    result = vkCreateBuffer(vk_device, &texture_staging_buffer_create_info, nullptr, &vk_texture_staging_buffer);
    if (result != VK_SUCCESS) fatal("Failed to create texture staging buffer");

    // Allocate memory for texture staging buffer
    VkMemoryRequirements texture_staging_buffer_memory_requirements;
    (void)vkGetBufferMemoryRequirements(vk_device, vk_texture_staging_buffer, &texture_staging_buffer_memory_requirements);

    VkMemoryAllocateInfo texture_staging_buffer_memory_allocate_info = {};
    texture_staging_buffer_memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    texture_staging_buffer_memory_allocate_info.allocationSize = texture_staging_buffer_memory_requirements.size;
    texture_staging_buffer_memory_allocate_info.memoryTypeIndex = find_memory_type(
        vk_physical_device,
        texture_staging_buffer_memory_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    VkDeviceMemory vk_texture_staging_buffer_memory;
    result = vkAllocateMemory(vk_device, &texture_staging_buffer_memory_allocate_info, NULL, &vk_texture_staging_buffer_memory);
    if (result != VK_SUCCESS) fatal("Failed to allocate memory for texture staging buffer");

    result = vkBindBufferMemory(vk_device, vk_texture_staging_buffer, vk_texture_staging_buffer_memory, 0);
    if (result != VK_SUCCESS) fatal("Failed to bind memory to texture staging buffer");

    // Upload image to the texture staging buffer
    void *texture_staging_buffer_data_ptr;
    result = vkMapMemory(vk_device, vk_texture_staging_buffer_memory, 0, image_size, 0, &texture_staging_buffer_data_ptr);
    if (result != VK_SUCCESS) fatal("Failed to map texture staging buffer memory");
    memcpy(texture_staging_buffer_data_ptr, pixels, (size_t)image_size);
    (void)vkUnmapMemory(vk_device, vk_texture_staging_buffer_memory);

    // Can free the texture in RAM now
    free(pixels);

    // Create image for the texture
    VkImageCreateInfo texture_image_create_info = {};
    texture_image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    texture_image_create_info.imageType = VK_IMAGE_TYPE_2D;
    texture_image_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    texture_image_create_info.extent = { (uint32_t)tex_w, (uint32_t)tex_h, 1 };
    texture_image_create_info.mipLevels = 1;
    texture_image_create_info.arrayLayers = 1;
    texture_image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    texture_image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    texture_image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    texture_image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    texture_image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    result = vkCreateImage(vk_device, &texture_image_create_info, NULL, &g_TempVulkan.texture_image);
    if (result != VK_SUCCESS) fatal("Failed to create texture image");

    // Allocate memory for the texture image
    VkMemoryRequirements texture_image_memory_requirements;
    (void)vkGetImageMemoryRequirements(vk_device, g_TempVulkan.texture_image, &texture_image_memory_requirements);

    VkMemoryAllocateInfo texture_image_memory_allocate_info = {};
    texture_image_memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    texture_image_memory_allocate_info.allocationSize = texture_image_memory_requirements.size;
    texture_image_memory_allocate_info.memoryTypeIndex = find_memory_type(
        vk_physical_device,
        texture_image_memory_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    result = vkAllocateMemory(vk_device, &texture_image_memory_allocate_info, NULL, &g_TempVulkan.texture_image_memory);
    if (result != VK_SUCCESS) fatal("Failed to allocate memory for texture image");

    result = vkBindImageMemory(vk_device, g_TempVulkan.texture_image, g_TempVulkan.texture_image_memory, 0);
    if (result != VK_SUCCESS) fatal("Failed to bind memory to texture image");

    // Command buffer
    VkCommandBufferAllocateInfo texture_command_buffer_allocate_info{};
    texture_command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    texture_command_buffer_allocate_info.commandPool = vk_command_pool;
    texture_command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    texture_command_buffer_allocate_info.commandBufferCount = 1;

    VkCommandBuffer vk_texture_command_buffer;
    result = vkAllocateCommandBuffers(vk_device, &texture_command_buffer_allocate_info, &vk_texture_command_buffer);
    if (result != VK_SUCCESS) fatal("Failed to allocate command buffer for texture");

    // Record commands for copying texture from staging buffer to device-local image memory
    VkCommandBufferBeginInfo texture_command_buffer_begin_info = {};
    texture_command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    texture_command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = vkBeginCommandBuffer(vk_texture_command_buffer, &texture_command_buffer_begin_info);
    if (result != VK_SUCCESS) fatal("Failed to begin texture command buffer");

    // Command buffer: image memory layout transition: VK_IMAGE_LAYOUT_UNDEFINED -> VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    VkImageMemoryBarrier texture_image_memory_barrier1 = {};
    texture_image_memory_barrier1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    texture_image_memory_barrier1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    texture_image_memory_barrier1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    texture_image_memory_barrier1.srcAccessMask = 0;
    texture_image_memory_barrier1.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    texture_image_memory_barrier1.image = g_TempVulkan.texture_image;
    texture_image_memory_barrier1.subresourceRange = {};
    texture_image_memory_barrier1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    texture_image_memory_barrier1.subresourceRange.baseMipLevel = 0;
    texture_image_memory_barrier1.subresourceRange.levelCount = 1;
    texture_image_memory_barrier1.subresourceRange.baseArrayLayer = 0;
    texture_image_memory_barrier1.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(
        vk_texture_command_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &texture_image_memory_barrier1
    );

    // Command buffer: copy from staging buffer to image buffer
    VkBufferImageCopy buffer_image_copy = {};
    buffer_image_copy.bufferOffset = 0;
    buffer_image_copy.bufferRowLength = 0;
    buffer_image_copy.bufferImageHeight = 0;
    buffer_image_copy.imageSubresource = {};
    buffer_image_copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    buffer_image_copy.imageSubresource.mipLevel = 0;
    buffer_image_copy.imageSubresource.baseArrayLayer = 0;
    buffer_image_copy.imageSubresource.layerCount = 1;
    buffer_image_copy.imageOffset = {0, 0, 0};
    buffer_image_copy.imageExtent = {(uint32_t)tex_w, (uint32_t)tex_h, 1};

    vkCmdCopyBufferToImage(
        vk_texture_command_buffer,
        vk_texture_staging_buffer,
        g_TempVulkan.texture_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &buffer_image_copy
    );

    // Command buffer: image memory layout transition: VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL -> VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    VkImageMemoryBarrier texture_image_memory_barrier2 = texture_image_memory_barrier1;
    texture_image_memory_barrier2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    texture_image_memory_barrier2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    texture_image_memory_barrier2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    texture_image_memory_barrier2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        vk_texture_command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &texture_image_memory_barrier2
    );

    result = vkEndCommandBuffer(vk_texture_command_buffer);
    if (result != VK_SUCCESS) fatal("Failed to end texture command buffer");

    // Submit texture command buffer to graphics queue
    VkSubmitInfo texture_command_buffer_submit_info = {};
    texture_command_buffer_submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    texture_command_buffer_submit_info.commandBufferCount = 1;
    texture_command_buffer_submit_info.pCommandBuffers = &vk_texture_command_buffer;

    result = vkQueueSubmit(vk_graphics_queue, 1, &texture_command_buffer_submit_info, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) fatal("Failed to submit texture command buffer to queue");

    // Just CPU-block until the queue is idle to ensure image is done copying
    result = vkQueueWaitIdle(vk_graphics_queue);
    if (result != VK_SUCCESS) fatal("Failed to wait idle for graphics queue");

    // Can destroy the staging buffer and memory
    (void)vkFreeMemory(vk_device, vk_texture_staging_buffer_memory, NULL);
    (void)vkDestroyBuffer(vk_device, vk_texture_staging_buffer, NULL);

    // Create texture image view
    VkImageViewCreateInfo texture_image_view_create_info = {};
    texture_image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    texture_image_view_create_info.image = g_TempVulkan.texture_image;
    texture_image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    texture_image_view_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    texture_image_view_create_info.subresourceRange = {};
    texture_image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    texture_image_view_create_info.subresourceRange.baseMipLevel = 0;
    texture_image_view_create_info.subresourceRange.levelCount = 1;
    texture_image_view_create_info.subresourceRange.baseArrayLayer = 0;
    texture_image_view_create_info.subresourceRange.layerCount = 1;

    result = vkCreateImageView(vk_device, &texture_image_view_create_info, NULL, &g_TempVulkan.texture_image_view);
    if (result != VK_SUCCESS) fatal("Failed to create texture image view");

    VkSamplerCreateInfo texture_sampler_create_info = {};
    texture_sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    texture_sampler_create_info.magFilter = VK_FILTER_LINEAR;
    texture_sampler_create_info.minFilter = VK_FILTER_LINEAR;
    texture_sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    texture_sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    texture_sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    texture_sampler_create_info.anisotropyEnable = VK_FALSE;
    texture_sampler_create_info.maxAnisotropy = 1.0f;
    texture_sampler_create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    texture_sampler_create_info.unnormalizedCoordinates = VK_FALSE;
    texture_sampler_create_info.compareEnable = VK_FALSE;
    texture_sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    result = vkCreateSampler(vk_device, &texture_sampler_create_info, NULL, &g_TempVulkan.texture_sampler);
    if (result != VK_SUCCESS) fatal("Failed to create texture sampler");

    // Descriptor set layout

    // Binding for uniform buffer
    VkDescriptorSetLayoutBinding uniform_buffer_descriptor_set_layout_binding = {};
    uniform_buffer_descriptor_set_layout_binding.binding = 0;
    uniform_buffer_descriptor_set_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniform_buffer_descriptor_set_layout_binding.descriptorCount = 1;
    uniform_buffer_descriptor_set_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uniform_buffer_descriptor_set_layout_binding.pImmutableSamplers = NULL;

    // Binding for texture sampler
    VkDescriptorSetLayoutBinding texture_sampler_descriptor_set_layout_binding = {};
    texture_sampler_descriptor_set_layout_binding.binding = 1;
    texture_sampler_descriptor_set_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    texture_sampler_descriptor_set_layout_binding.descriptorCount = 1;
    texture_sampler_descriptor_set_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    texture_sampler_descriptor_set_layout_binding.pImmutableSamplers = NULL;

    VkDescriptorSetLayoutBinding descriptor_set_layout_bindings[] = {uniform_buffer_descriptor_set_layout_binding, texture_sampler_descriptor_set_layout_binding};
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {};
    descriptor_set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_create_info.bindingCount = 2;
    descriptor_set_layout_create_info.pBindings = descriptor_set_layout_bindings;

    result = vkCreateDescriptorSetLayout(vk_device, &descriptor_set_layout_create_info, NULL, &g_TempVulkan.descriptor_set_layout);
    if (result != VK_SUCCESS) fatal("Failed to create descriptor set layout");

    // Descriptor pool
    VkDescriptorPoolSize descriptor_pool_sizes[2] = {};
    descriptor_pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptor_pool_sizes[0].descriptorCount = 1;
    descriptor_pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptor_pool_sizes[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo decriptor_pool_create_info = {};
    decriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    decriptor_pool_create_info.poolSizeCount = 2;
    decriptor_pool_create_info.pPoolSizes = descriptor_pool_sizes;
    decriptor_pool_create_info.maxSets = 1;

    result = vkCreateDescriptorPool(vk_device, &decriptor_pool_create_info, NULL, &g_TempVulkan.descriptor_pool);
    if (result != VK_SUCCESS) fatal("Failed to create descriptor pool");

    // Allocate descriptor sets
    VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {};
    descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptor_set_allocate_info.descriptorPool = g_TempVulkan.descriptor_pool;
    descriptor_set_allocate_info.descriptorSetCount = 1;
    descriptor_set_allocate_info.pSetLayouts = &g_TempVulkan.descriptor_set_layout;

    result = vkAllocateDescriptorSets(vk_device, &descriptor_set_allocate_info, &g_TempVulkan.descriptor_set);
    if (result != VK_SUCCESS) fatal("Failed to allocate descriptor set");

    // Update descriptor sets to point binding 0 to the uniform buffer
    VkDescriptorBufferInfo uniform_buffer_descriptor_buffer_info = {};
    uniform_buffer_descriptor_buffer_info.buffer = g_TempVulkan.uniform_buffer;
    uniform_buffer_descriptor_buffer_info.offset = 0;
    uniform_buffer_descriptor_buffer_info.range = vk_uniform_buffer_size;

    VkWriteDescriptorSet uniform_buffer_write_descriptor_set = {};
    uniform_buffer_write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    uniform_buffer_write_descriptor_set.dstSet = g_TempVulkan.descriptor_set;
    uniform_buffer_write_descriptor_set.dstBinding = 0;
    uniform_buffer_write_descriptor_set.dstArrayElement = 0;
    uniform_buffer_write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniform_buffer_write_descriptor_set.descriptorCount = 1;
    uniform_buffer_write_descriptor_set.pBufferInfo = &uniform_buffer_descriptor_buffer_info;

    vkUpdateDescriptorSets(vk_device, 1, &uniform_buffer_write_descriptor_set, 0, NULL);

    // Update descriptor sets to point binding 1 to the texture sampler
    VkDescriptorImageInfo texture_sampler_descriptor_image_info = {};
    texture_sampler_descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    texture_sampler_descriptor_image_info.imageView = g_TempVulkan.texture_image_view;
    texture_sampler_descriptor_image_info.sampler = g_TempVulkan.texture_sampler;

    VkWriteDescriptorSet texture_sampler_write_descriptor_set = {};
    texture_sampler_write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    texture_sampler_write_descriptor_set.dstSet = g_TempVulkan.descriptor_set;
    texture_sampler_write_descriptor_set.dstBinding = 1;
    texture_sampler_write_descriptor_set.dstArrayElement = 0;
    texture_sampler_write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    texture_sampler_write_descriptor_set.descriptorCount = 1;
    texture_sampler_write_descriptor_set.pImageInfo = &texture_sampler_descriptor_image_info;

    vkUpdateDescriptorSets(vk_device, 1, &texture_sampler_write_descriptor_set, 0, NULL);

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

    std::vector<VkVertexInputAttributeDescription> vertex_input_attribute_descriptions(3);
    vertex_input_attribute_descriptions[0].location = 0;
    vertex_input_attribute_descriptions[0].binding = 0;
    vertex_input_attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    vertex_input_attribute_descriptions[0].offset = offsetof(Vertex, x);
    vertex_input_attribute_descriptions[1].location = 1;
    vertex_input_attribute_descriptions[1].binding = 0;
    vertex_input_attribute_descriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    vertex_input_attribute_descriptions[1].offset = offsetof(Vertex, u);
    vertex_input_attribute_descriptions[2].location = 2;
    vertex_input_attribute_descriptions[2].binding = 0;
    vertex_input_attribute_descriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_input_attribute_descriptions[2].offset = offsetof(Vertex, r);

    VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_state_create_info = {};
    pipeline_vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    pipeline_vertex_input_state_create_info.vertexBindingDescriptionCount = 1;
    pipeline_vertex_input_state_create_info.pVertexBindingDescriptions = &vertex_input_binding_description;
    pipeline_vertex_input_state_create_info.vertexAttributeDescriptionCount = vertex_input_attribute_descriptions.size();
    pipeline_vertex_input_state_create_info.pVertexAttributeDescriptions = vertex_input_attribute_descriptions.data();

    VkPipelineInputAssemblyStateCreateInfo pipeline_input_assembly_create_info = {};
    pipeline_input_assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipeline_input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    int fb_width, fb_height;
    glfwGetFramebufferSize(window, &fb_width, &fb_height);
    VkViewport viewport = {0, 0, (float)fb_width, (float)fb_height, 0.0f, 1.0f};
    VkRect2D scissor = {{0, 0}, {(uint32_t)fb_width, (uint32_t)fb_height}};
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
    pipeline_layout_create_info.setLayoutCount = 1;
    // Reference the descriptor set layout
    pipeline_layout_create_info.pSetLayouts = &g_TempVulkan.descriptor_set_layout;
    result = vkCreatePipelineLayout(vk_device, &pipeline_layout_create_info, nullptr, &g_TempVulkan.pipeline_layout);
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
    graphics_pipeline_create_info.layout = g_TempVulkan.pipeline_layout;
    graphics_pipeline_create_info.renderPass = g_TempVulkan.render_pass;
    graphics_pipeline_create_info.subpass = 0;
    
    result = vkCreateGraphicsPipelines(vk_device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &g_TempVulkan.pipeline);
    if (result != VK_SUCCESS) fatal("Failed to create graphics pipeline");

    (void)vkDestroyShaderModule(vk_device, vk_vert_shader_module, nullptr);
    (void)vkDestroyShaderModule(vk_device, vk_frag_shader_module, nullptr);

    // Create image available and render finished semaphores
    VkSemaphoreCreateInfo semaphore_create_info = {};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    result = vkCreateSemaphore(vk_device, &semaphore_create_info, NULL, &g_TempVulkan.image_available_semaphore);
    if (result != VK_SUCCESS) fatal("Failed to create image available semaphore");
    result = vkCreateSemaphore(vk_device, &semaphore_create_info, NULL, &g_TempVulkan.render_finished_semaphore);
    if (result != VK_SUCCESS) fatal("Failed to create render finished semaphore");
}

void destroy_basically_everything(VkDevice vk_device)
{
    (void)vkDestroySampler(vk_device, g_TempVulkan.texture_sampler, NULL);

    (void)vkDestroyImage(vk_device, g_TempVulkan.texture_image, NULL);
    (void)vkFreeMemory(vk_device, g_TempVulkan.texture_image_memory, NULL);
    
    (void)vkDestroyImageView(vk_device, g_TempVulkan.texture_image_view, NULL);

    (void)vkDestroyDescriptorPool(vk_device, g_TempVulkan.descriptor_pool, NULL);
    (void)vkDestroyDescriptorSetLayout(vk_device, g_TempVulkan.descriptor_set_layout, NULL);

    (void)vkFreeMemory(vk_device, g_TempVulkan.uniform_buffer_memory, NULL);
    (void)vkDestroyBuffer(vk_device, g_TempVulkan.uniform_buffer, NULL);

    (void)vkDestroyPipeline(vk_device, g_TempVulkan.pipeline, nullptr);
    (void)vkDestroyPipelineLayout(vk_device, g_TempVulkan.pipeline_layout, nullptr);
    for (auto framebuffer: g_TempVulkan.framebuffers)
    {
        (void)vkDestroyFramebuffer(vk_device, framebuffer, nullptr);
    }
    (void)vkDestroyRenderPass(vk_device, g_TempVulkan.render_pass, nullptr);
    for (auto image_view: g_TempVulkan.image_views)
    {
        (void)vkDestroyImageView(vk_device, image_view, nullptr);
    }
    (void)vkDestroySwapchainKHR(vk_device, g_TempVulkan.swapchain, nullptr);

    (void)vkDestroySemaphore(vk_device, g_TempVulkan.image_available_semaphore, NULL);
    (void)vkDestroySemaphore(vk_device, g_TempVulkan.render_finished_semaphore, NULL);
}

int main()
{
    int width = 1000;
    int height = 900;

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
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
    result = vkEnumeratePhysicalDevices(vk_instance, &count, nullptr);
    if (result != VK_SUCCESS) fatal("Failed to enumerate physical devices");
    std::vector<VkPhysicalDevice> physical_devices(count);
    result = vkEnumeratePhysicalDevices(vk_instance, &count, physical_devices.data());
    if (result != VK_SUCCESS) fatal("Failed to enumerate physical devices 2");
    VkPhysicalDevice vk_physical_device = physical_devices[0];

    // Find graphics queue
    uint32_t vk_graphics_queue_family_index = 0;
    (void)vkGetPhysicalDeviceQueueFamilyProperties(vk_physical_device, &count, NULL);
    std::vector<VkQueueFamilyProperties> queue_families(count);
    (void)vkGetPhysicalDeviceQueueFamilyProperties(vk_physical_device, &count, queue_families.data());
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
    (void)vkGetDeviceQueue(vk_device,vk_graphics_queue_family_index, 0, &vk_graphics_queue);

    // Vertex buffer
    int w_int, h_int;
    glfwGetWindowSize(window, &w_int, &h_int);
    float w = w_int;
    float h = h_int;
    float pad = 100.0f;
    float q_min_x = pad;
    float q_max_x = w - pad;
    float q_min_y = pad;
    float q_max_y = h - pad;
    
    const Vertex verts[] = {
        { q_min_x, q_max_y, 0.0f, 0.0f, 0.7f, 0.6f, 0.5f },
        { q_max_x, q_max_y, 1.0f, 0.0f, 0.7f, 0.6f, 0.5f },
        { q_max_x, q_min_y, 1.0f, 1.0f, 0.7f, 0.6f, 0.5f },
        { q_min_x, q_min_y, 0.0f, 1.0f, 0.7f, 0.6f, 0.5f },
    };

    VkDeviceSize vertex_buffer_size = sizeof(verts);

    VkBufferCreateInfo vertex_buffer_create_info = {};
    vertex_buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertex_buffer_create_info.size = vertex_buffer_size;
    vertex_buffer_create_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vertex_buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer vk_vertex_buffer;
    result = vkCreateBuffer(vk_device, &vertex_buffer_create_info, nullptr, &vk_vertex_buffer);
    if (result != VK_SUCCESS) fatal("Failed to create vertex buffer");

    // Allocate memory for vertex buffer
    VkMemoryRequirements vertex_buffer_memory_requirements;
    (void)vkGetBufferMemoryRequirements(vk_device, vk_vertex_buffer, &vertex_buffer_memory_requirements);

    VkMemoryAllocateInfo vertex_buffer_memory_allocate_info = {};
    vertex_buffer_memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    vertex_buffer_memory_allocate_info.allocationSize = vertex_buffer_memory_requirements.size;
    vertex_buffer_memory_allocate_info.memoryTypeIndex = find_memory_type(
        vk_physical_device,
        vertex_buffer_memory_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );;

    VkDeviceMemory vk_vertex_buffer_memory;
    result = vkAllocateMemory(vk_device, &vertex_buffer_memory_allocate_info, NULL, &vk_vertex_buffer_memory);
    if (result != VK_SUCCESS) fatal("Failed to allocate memory for vertex buffer");

    result = vkBindBufferMemory(vk_device, vk_vertex_buffer, vk_vertex_buffer_memory, 0);
    if (result != VK_SUCCESS) fatal("Failed to bind memory to vertex buffer");

    // Upload data to the vertex buffer
    void *vertex_buffer_data_ptr;
    result = vkMapMemory(vk_device, vk_vertex_buffer_memory, 0, vertex_buffer_size, 0, &vertex_buffer_data_ptr);
    if (result != VK_SUCCESS) fatal("Failed to map vertex buffer memory");
    memcpy(vertex_buffer_data_ptr, verts, (size_t)vertex_buffer_size);
    (void)vkUnmapMemory(vk_device, vk_vertex_buffer_memory);

    // Index buffer
    uint32_t indices[] = {0, 1, 2, 0, 2, 3};
    int index_count = 6;

    VkDeviceSize index_buffer_size = sizeof(indices);

    VkBufferCreateInfo index_buffer_create_info = {};
    index_buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    index_buffer_create_info.size = index_buffer_size;
    index_buffer_create_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    index_buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer vk_index_buffer;
    result = vkCreateBuffer(vk_device, &index_buffer_create_info, nullptr, &vk_index_buffer);
    if (result != VK_SUCCESS) fatal("Failed to create index buffer");

    // Allocate memory for index buffer
    VkMemoryRequirements index_buffer_memory_requirements;
    (void)vkGetBufferMemoryRequirements(vk_device, vk_index_buffer, &index_buffer_memory_requirements);

    VkMemoryAllocateInfo index_buffer_memory_allocate_info = {};
    index_buffer_memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    index_buffer_memory_allocate_info.allocationSize = index_buffer_memory_requirements.size;
    index_buffer_memory_allocate_info.memoryTypeIndex = find_memory_type(
        vk_physical_device,
        index_buffer_memory_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );;

    VkDeviceMemory vk_index_buffer_memory;
    result = vkAllocateMemory(vk_device, &index_buffer_memory_allocate_info, NULL, &vk_index_buffer_memory);
    if (result != VK_SUCCESS) fatal("Failed to allocate memory for index buffer");

    result = vkBindBufferMemory(vk_device, vk_index_buffer, vk_index_buffer_memory, 0);
    if (result != VK_SUCCESS) fatal("Failed to bind memory to index buffer");

    // Upload data to the index buffer
    void *index_buffer_data_ptr;
    result = vkMapMemory(vk_device, vk_index_buffer_memory, 0, index_buffer_size, 0, &index_buffer_data_ptr);
    if (result != VK_SUCCESS) fatal("Failed to map index buffer memory");
    memcpy(index_buffer_data_ptr, indices, (size_t)index_buffer_size);
    (void)vkUnmapMemory(vk_device, vk_index_buffer_memory);

    // Command pool
    VkCommandPoolCreateInfo command_pool_create_info{};
    command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_create_info.queueFamilyIndex = vk_graphics_queue_family_index;
    command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // allows resetting individual buffers

    VkCommandPool vk_command_pool;
    result = vkCreateCommandPool(vk_device, &command_pool_create_info, nullptr, &vk_command_pool);
    if (result != VK_SUCCESS) fatal("Failed to create command pool");

    // Command buffer
    VkCommandBufferAllocateInfo command_buffer_allocate_info{};
    command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_allocate_info.commandPool = vk_command_pool;
    command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_allocate_info.commandBufferCount = 1;

    VkCommandBuffer vk_command_buffer;
    result = vkAllocateCommandBuffers(vk_device, &command_buffer_allocate_info, &vk_command_buffer);
    if (result != VK_SUCCESS) fatal("Failed to allocate command buffers");

    create_basically_everything(window, vk_physical_device, vk_surface, vk_device, vk_graphics_queue, vk_command_pool);

    bool recreate_everything = false;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        if (recreate_everything)
        {
            vkDeviceWaitIdle(vk_device);
            destroy_basically_everything(vk_device);
            create_basically_everything(window, vk_physical_device, vk_surface, vk_device, vk_graphics_queue, vk_command_pool);
            trace("Recreated everything. Swapchain extent: %ux%u", g_TempVulkan.swapchain_extent.width, g_TempVulkan.swapchain_extent.height);
            recreate_everything = false;
        }

        // Re-upload quad verts
        int w_int, h_int;
        glfwGetWindowSize(window, &w_int, &h_int);
        float w = w_int;
        float h = h_int;
        float pad = 100.0f;
        float q_min_x = pad;
        float q_max_x = w - pad;
        float q_min_y = pad;
        float q_max_y = h - pad;

        const Vertex verts[] = {
            { q_min_x, q_max_y, 0.0f, 0.0f, 0.7f, 0.6f, 0.5f },
            { q_max_x, q_max_y, 1.0f, 0.0f, 0.7f, 0.6f, 0.5f },
            { q_max_x, q_min_y, 1.0f, 1.0f, 0.7f, 0.6f, 0.5f },
            { q_min_x, q_min_y, 0.0f, 1.0f, 0.7f, 0.6f, 0.5f },
        };

        void *vertex_buffer_data_ptr;
        result = vkMapMemory(vk_device, vk_vertex_buffer_memory, 0, vertex_buffer_size, 0, &vertex_buffer_data_ptr);
        if (result != VK_SUCCESS) fatal("Failed to map vertex buffer memory");
        memcpy(vertex_buffer_data_ptr, verts, (size_t)vertex_buffer_size);
        (void)vkUnmapMemory(vk_device, vk_vertex_buffer_memory);

        // Acquire next image
        uint32_t next_image_index;
        result = vkAcquireNextImageKHR(vk_device, g_TempVulkan.swapchain, UINT64_MAX, g_TempVulkan.image_available_semaphore, VK_NULL_HANDLE, &next_image_index);
        if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            recreate_everything = true;
            continue;
        }
        else if (result != VK_SUCCESS) fatal("Failed to acquire next image");

        // Reset and re-record command buffer
        vkResetCommandBuffer(vk_command_buffer, 0);
        if (result != VK_SUCCESS) fatal("Failed to reset command buffer");
        VkCommandBufferBeginInfo command_buffer_begin_info = {};
        command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        result = vkBeginCommandBuffer(vk_command_buffer, &command_buffer_begin_info);
        if (result != VK_SUCCESS) fatal("Failed to begin command buffer");

        // Doing rendering to a framebuffer -- > need render pass
        VkClearValue clear_value = {};
        clear_value.color = { { 1.0f, 0.0f, 0.0f, 1.0f } };
        VkRect2D render_area = {};
        render_area.offset = (VkOffset2D){0, 0};
        render_area.extent = g_TempVulkan.swapchain_extent;
        VkRenderPassBeginInfo render_pass_begin_info = {};
        render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_begin_info.renderPass = g_TempVulkan.render_pass;
        render_pass_begin_info.framebuffer = g_TempVulkan.framebuffers[next_image_index]; // render pass to the right frame buffer index
        render_pass_begin_info.renderArea = render_area;
        render_pass_begin_info.clearValueCount = 1;
        render_pass_begin_info.pClearValues = &clear_value;
        (void)vkCmdBeginRenderPass(vk_command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        // Bind descriptor set for uniform buffer
        vkCmdBindDescriptorSets(
            vk_command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            g_TempVulkan.pipeline_layout,
            0, // firstSet
            1, &g_TempVulkan.descriptor_set,
            0, NULL
        );
        // Bind pipeline that is used for drawing triangle
        (void)vkCmdBindPipeline(vk_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g_TempVulkan.pipeline);
        VkDeviceSize offsets[] = { 0 };
        // Bind vertex buffer that contains triangle vertices
        (void)vkCmdBindVertexBuffers(vk_command_buffer, 0, 1, &vk_vertex_buffer, offsets);
        (void)vkCmdBindIndexBuffer(vk_command_buffer, vk_index_buffer, 0, VK_INDEX_TYPE_UINT32);
        // Draw call for 6 vertices
        // (void)vkCmdDraw(vk_command_buffer, 6, 1, 0, 0);
        (void)vkCmdDrawIndexed(vk_command_buffer, index_count, 1, 0, 0, 0);

        (void)vkCmdEndRenderPass(vk_command_buffer);
        result = vkEndCommandBuffer(vk_command_buffer);
        if (result != VK_SUCCESS) fatal("Failed to end command buffer");

        // Submit command buffer
        VkPipelineStageFlags wait_destination_stage_mask[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }; // wait on the semaphore before executing the color attachment-writing phase
        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &g_TempVulkan.image_available_semaphore;
        submit_info.pWaitDstStageMask = wait_destination_stage_mask;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &vk_command_buffer;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &g_TempVulkan.render_finished_semaphore;

        result = vkQueueSubmit(vk_graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
        if (result != VK_SUCCESS) fatal("Failed to submit command buffer to queue");

        // Present -- use the same queue as the graphics queue, as it has present support in this case
        VkPresentInfoKHR present_info = {};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &g_TempVulkan.render_finished_semaphore;
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &g_TempVulkan.swapchain;
        present_info.pImageIndices = &next_image_index;
        result = vkQueuePresentKHR(vk_graphics_queue, &present_info);
        if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            recreate_everything = true;
            continue;
        }
        else if (result != VK_SUCCESS) fatal("Error when presenting");

        // Wait until present queue, in this case same as graphics queue, is done -- the image has been presented
        result = vkQueueWaitIdle(vk_graphics_queue);
        if (result != VK_SUCCESS) fatal("Failed to wait idle for graphics queue");
    }

    (void)vkDestroyCommandPool(vk_device, vk_command_pool, NULL);

    (void)vkFreeMemory(vk_device, vk_index_buffer_memory, NULL);
    (void)vkDestroyBuffer(vk_device, vk_index_buffer, NULL);

    (void)vkFreeMemory(vk_device, vk_vertex_buffer_memory, NULL);
    (void)vkDestroyBuffer(vk_device, vk_vertex_buffer, NULL);

    destroy_basically_everything(vk_device);

    (void)vkDestroyDevice(vk_device, nullptr);
    (void)vkDestroySurfaceKHR(vk_instance, vk_surface, nullptr);
    (void)vkDestroyInstance(vk_instance, nullptr);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
