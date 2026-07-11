// Flat desktop window - no OpenXR, no headset required at all. Reuses the
// same VulkanRenderer content-drawing code (RenderTexturedQuad) that the
// composition-layer quad uses on the headset builds, just presented into a
// normal window swapchain instead of an OpenXR session. This is the fast
// local-iteration debug build the emulator/menu rendering will eventually
// show up in without needing to put the headset on.
#include "VulkanRenderer.h"
#include "AssetLoader.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void CheckVk(VkResult result, const char* what) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string("Vulkan call failed: ") + what + " (" + std::to_string(result) + ")");
    }
}

}  // namespace

int main() {
    if (!glfwInit()) {
        std::fprintf(stderr, "VirtualBoyGo 2D: glfwInit failed\n");
        return 1;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(960, 640, "VirtualBoyGo (2D debug)", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "VirtualBoyGo 2D: glfwCreateWindow failed\n");
        glfwTerminate();
        return 1;
    }

    VulkanRenderer renderer;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFence acquireFence = VK_NULL_HANDLE;
    int exitCode = 0;

    try {
        uint32_t glfwExtCount = 0;
        const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
        const std::vector<const char*> instanceExtensions(glfwExts, glfwExts + glfwExtCount);

        VkInstance instance = renderer.CreateInstanceStandalone(instanceExtensions);
        CheckVk(glfwCreateWindowSurface(instance, window, nullptr, &surface), "glfwCreateWindowSurface");
        renderer.CreateDeviceForSurface(surface);

        // Prefer an sRGB surface format - matches the color-space handling
        // used for the test texture (see VulkanRenderer::LoadTestImage).
        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(renderer.GetPhysicalDevice(), surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(renderer.GetPhysicalDevice(), surface, &formatCount, formats.data());
        VkSurfaceFormatKHR chosen = formats.empty() ? VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_SRGB} : formats[0];
        for (const auto& f : formats) {
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB || f.format == VK_FORMAT_R8G8B8A8_SRGB) {
                chosen = f;
                break;
            }
        }

        int fbWidth = 0, fbHeight = 0;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        const VkExtent2D extent{static_cast<uint32_t>(fbWidth), static_cast<uint32_t>(fbHeight)};

        VkSwapchainCreateInfoKHR swapchainInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        swapchainInfo.surface = surface;
        swapchainInfo.minImageCount = 2;
        swapchainInfo.imageFormat = chosen.format;
        swapchainInfo.imageColorSpace = chosen.colorSpace;
        swapchainInfo.imageExtent = extent;
        swapchainInfo.imageArrayLayers = 1;
        swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        swapchainInfo.clipped = VK_TRUE;
        CheckVk(vkCreateSwapchainKHR(renderer.GetDevice(), &swapchainInfo, nullptr, &swapchain), "vkCreateSwapchainKHR");

        uint32_t imageCount = 0;
        vkGetSwapchainImagesKHR(renderer.GetDevice(), swapchain, &imageCount, nullptr);
        std::vector<VkImage> swapchainImages(imageCount);
        vkGetSwapchainImagesKHR(renderer.GetDevice(), swapchain, &imageCount, swapchainImages.data());

        const std::vector<uint8_t> imageBytes = LoadAssetBytes("test_image.jpg");
        if (!imageBytes.empty()) {
            uint32_t imgW = 0, imgH = 0;
            renderer.LoadTestImage(imageBytes, imgW, imgH);
        } else {
            std::fprintf(stderr, "VirtualBoyGo 2D: test_image.jpg not found next to the exe\n");
        }

        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        CheckVk(vkCreateFence(renderer.GetDevice(), &fenceInfo, nullptr, &acquireFence), "vkCreateFence");

        std::printf("VirtualBoyGo 2D debug window running (%ux%u)\n", extent.width, extent.height);

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            vkResetFences(renderer.GetDevice(), 1, &acquireFence);
            uint32_t imageIndex = 0;
            const VkResult acquireResult = vkAcquireNextImageKHR(renderer.GetDevice(), swapchain, UINT64_MAX,
                                                                 VK_NULL_HANDLE, acquireFence, &imageIndex);
            if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
                // e.g. VK_ERROR_OUT_OF_DATE_KHR - window resize isn't handled
                // in this first version (fixed-size, non-resizable window).
                continue;
            }
            vkWaitForFences(renderer.GetDevice(), 1, &acquireFence, VK_TRUE, UINT64_MAX);

            // RenderTexturedQuad blocks internally (vkQueueWaitIdle) until
            // rendering is complete, so presenting right after is safe
            // without a rendering-finished semaphore.
            renderer.RenderTexturedQuad(swapchainImages[imageIndex], chosen.format, extent.width, extent.height);

            VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &swapchain;
            presentInfo.pImageIndices = &imageIndex;
            vkQueuePresentKHR(renderer.GetQueue(), &presentInfo);
        }

        vkDeviceWaitIdle(renderer.GetDevice());
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "VirtualBoyGo 2D: %s\n", ex.what());
        exitCode = 1;
    }

    if (acquireFence != VK_NULL_HANDLE) vkDestroyFence(renderer.GetDevice(), acquireFence, nullptr);
    if (swapchain != VK_NULL_HANDLE) vkDestroySwapchainKHR(renderer.GetDevice(), swapchain, nullptr);
    // Surface must be destroyed before the instance - renderer.Shutdown()
    // destroys the instance, so this has to happen first.
    if (surface != VK_NULL_HANDLE) vkDestroySurfaceKHR(renderer.GetInstance(), surface, nullptr);
    renderer.Shutdown();

    glfwDestroyWindow(window);
    glfwTerminate();
    return exitCode;
}
