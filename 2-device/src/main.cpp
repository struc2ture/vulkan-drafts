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
    if (result != VK_SUCCESS)
    {
        fatal("Failed to create instance");
    }

    // Surface
    VkSurfaceKHR vk_surface;
    result = glfwCreateWindowSurface(vk_instance, window, NULL, &vk_surface);
    if (result != VK_SUCCESS)
    {
        fatal("Failed to create surface");
    }

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
    if (result != VK_SUCCESS)
    {
        fatal("Failed to create logical device");
    }

    // Get queue handle of the graphics queue family
    VkQueue vk_graphics_queue;
    vkGetDeviceQueue(vk_device,vk_graphics_queue_family_index, 0, &vk_graphics_queue);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }

    vkDestroyDevice(vk_device, NULL);
    vkDestroySurfaceKHR(vk_instance, vk_surface, NULL);
    vkDestroyInstance(vk_instance, NULL);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
