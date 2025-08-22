#include <cstdio>
#include <vector>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

#include "vk_enum_str.cpp"

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
    printf("Available physical device-surface formats:\n");
    for (uint32_t i = 0; i < format_count; i++)
    {
        auto format = formats[i];
        printf("formats[%u]: %s, %s\n", i, get_VkFormat_str(format.format), get_VkColorSpaceKHR_str(format.colorSpace));
    }

    VkSurfaceFormatKHR vk_surface_format = formats[0];
    printf("Using format 0: %s, %s\n", get_VkFormat_str(vk_surface_format.format), get_VkColorSpaceKHR_str(vk_surface_format.colorSpace));
    assert(vk_surface_format.format == VK_FORMAT_B8G8R8A8_UNORM && vk_surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);

    VkExtent2D vk_swapchain_extent = capabilities.currentExtent;
    printf("Swapchain extent: %d, %d\n", vk_swapchain_extent.width, vk_swapchain_extent.height);

    printf("Swapchain transform: %s\n", get_VkColorSpaceKHR_str(capabilities.currentTransform));

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

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }

    vkDestroySwapchainKHR(vk_device, vk_swapchain, NULL);
    vkDestroyDevice(vk_device, NULL);
    vkDestroySurfaceKHR(vk_instance, vk_surface, NULL);
    vkDestroyInstance(vk_instance, NULL);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
