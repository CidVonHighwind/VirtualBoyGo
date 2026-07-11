// Flat desktop window - no OpenXR, no headset required at all. Draws the
// same AppMenu/UiRenderer content the composition-layer quad shows on the
// headset builds, presented into a normal window swapchain instead of an
// OpenXR session, driven by arrow keys/Enter/Escape instead of controller
// input. This is the fast local-iteration debug build the emulator/menu
// rendering will eventually show up in without needing to put the headset on.
#include "VulkanRenderer.h"
#include "AssetLoader.h"
#include "Emulator.h"
#include "ui/AppMenu.h"
#include "ui/ButtonMapping.h"
#include "ui/UiRenderer.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// Only need stbi_info_from_memory here, to size the window to the game
// image before a Vulkan device/surface exist (STB_IMAGE_IMPLEMENTATION is
// defined once in VulkanRenderer.cpp, same vbgo_app link unit).
#include "third_party/stb_image.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

    void CheckVk(VkResult result, const char *what)
    {
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error(std::string("Vulkan call failed: ") + what + " (" + std::to_string(result) + ")");
        }
    }

    void PollKeyboardButtonState(GLFWwindow *window, uint32_t buttonStates[3])
    {
        using namespace ButtonMapper;
        buttonStates[DeviceGamepad] = 0;
        buttonStates[DeviceLeftTouch] = 0;
        buttonStates[DeviceRightTouch] = 0;

        uint32_t &bits = buttonStates[DeviceRightTouch];
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            bits |= ButtonMapping[EmuButton_Up];
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            bits |= ButtonMapping[EmuButton_Down];
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            bits |= ButtonMapping[EmuButton_Left];
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            bits |= ButtonMapping[EmuButton_Right];
        if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS)
            bits |= ButtonMapping[EmuButton_A];
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            bits |= ButtonMapping[EmuButton_B];
    }

} // namespace

int main()
{
    // Peek the game image's dimensions up front (pure file IO, no Vulkan
    // device needed yet) so its native resolution can size the window
    // before glfwCreateWindow - the window must exist before the Vulkan
    // instance/surface/device can be created, and Emulator::Initialize
    // (which actually uploads the texture) needs that device. Emulator
    // re-reads/re-decodes the same file itself once the device exists.
    const std::vector<uint8_t> gameImageBytes = LoadAssetBytes("game_image.png");
    int gameImageNativeWidth = 0, gameImageNativeHeight = 0;
    if (!gameImageBytes.empty())
    {
        int comp = 0;
        stbi_info_from_memory(gameImageBytes.data(), static_cast<int>(gameImageBytes.size()), &gameImageNativeWidth,
                              &gameImageNativeHeight, &comp);
    }
    else
    {
        std::fprintf(stderr, "VirtualBoyGo 2D: game_image.png not found next to the exe\n");
    }
    // Window is sized to exactly fit the upscaled game screen; the menu is
    // a smaller fixed-size (AppMenu::kMenuWidth/kMenuHeight) panel composited
    // (rounded corners and all) at a centered offset within it, not the
    // window's full size.
    const int windowWidth = gameImageNativeWidth > 0 ? gameImageNativeWidth * Emulator::kScale : kMenuWidth;
    const int windowHeight = gameImageNativeHeight > 0 ? gameImageNativeHeight * Emulator::kScale : kMenuHeight;

    if (!glfwInit())
    {
        std::fprintf(stderr, "VirtualBoyGo 2D: glfwInit failed\n");
        return 1;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow *window = glfwCreateWindow(windowWidth, windowHeight, "VirtualBoyGo (2D debug)", nullptr, nullptr);
    if (!window)
    {
        std::fprintf(stderr, "VirtualBoyGo 2D: glfwCreateWindow failed\n");
        glfwTerminate();
        return 1;
    }

    VulkanRenderer renderer;
    UiRenderer uiRenderer;
    Emulator emulator;
    AppMenu appMenu;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFence acquireFence = VK_NULL_HANDLE;
    int exitCode = 0;

    try
    {
        uint32_t glfwExtCount = 0;
        const char **glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
        const std::vector<const char *> instanceExtensions(glfwExts, glfwExts + glfwExtCount);

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
        for (const auto &f : formats)
        {
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB || f.format == VK_FORMAT_R8G8B8A8_SRGB)
            {
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

        uiRenderer.Initialize(renderer.GetDevice(), renderer.GetPhysicalDevice(), renderer.GetQueue(),
                              renderer.GetQueueFamilyIndex(), renderer.GetCommandPool(), renderer.GetCommandBuffer());
        emulator.Initialize(uiRenderer);
        appMenu.Initialize(uiRenderer, chosen.format);

        // Menu panel is centered within the (larger) window.
        const float menuX = (static_cast<float>(windowWidth) - kMenuWidth) / 2.0f;
        const float menuY = (static_cast<float>(windowHeight) - kMenuHeight) / 2.0f;

        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        CheckVk(vkCreateFence(renderer.GetDevice(), &fenceInfo, nullptr, &acquireFence), "vkCreateFence");

        std::printf("VirtualBoyGo 2D debug window running (%ux%u)\n", extent.width, extent.height);

        uint32_t buttonStates[3]{};
        uint32_t lastButtonStates[3]{};
        auto lastFrameTime = std::chrono::steady_clock::now();

        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();

            const auto now = std::chrono::steady_clock::now();
            const float deltaSeconds = std::chrono::duration<float>(now - lastFrameTime).count();
            lastFrameTime = now;

            std::memcpy(lastButtonStates, buttonStates, sizeof(buttonStates));
            PollKeyboardButtonState(window, buttonStates);
            appMenu.Update(buttonStates, lastButtonStates, deltaSeconds);
            appMenu.RenderToBuffer(uiRenderer);

            vkResetFences(renderer.GetDevice(), 1, &acquireFence);
            uint32_t imageIndex = 0;
            const VkResult acquireResult = vkAcquireNextImageKHR(renderer.GetDevice(), swapchain, UINT64_MAX,
                                                                 VK_NULL_HANDLE, acquireFence, &imageIndex);
            if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
            {
                // e.g. VK_ERROR_OUT_OF_DATE_KHR - window resize isn't handled
                // in this first version (fixed-size, non-resizable window).
                continue;
            }
            vkWaitForFences(renderer.GetDevice(), 1, &acquireFence, VK_TRUE, UINT64_MAX);

            // UiRenderer::EndFrame blocks internally (vkQueueWaitIdle) until
            // rendering is complete, so presenting right after is safe
            // without a rendering-finished semaphore.
            uiRenderer.BeginFrame(swapchainImages[imageIndex], chosen.format, extent.width, extent.height,
                                  appMenu.GetBackgroundColor());
            if (emulator.HasScreen())
            {
                emulator.DrawScreen(uiRenderer, 0, 0, static_cast<float>(extent.width), static_cast<float>(extent.height));
            }
            appMenu.Draw(uiRenderer, menuX, menuY);
            uiRenderer.EndFrame();

            VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &swapchain;
            presentInfo.pImageIndices = &imageIndex;
            vkQueuePresentKHR(renderer.GetQueue(), &presentInfo);
        }

        vkDeviceWaitIdle(renderer.GetDevice());
    }
    catch (const std::exception &ex)
    {
        std::fprintf(stderr, "VirtualBoyGo 2D: %s\n", ex.what());
        exitCode = 1;
    }

    if (acquireFence != VK_NULL_HANDLE)
        vkDestroyFence(renderer.GetDevice(), acquireFence, nullptr);
    if (swapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(renderer.GetDevice(), swapchain, nullptr);
    // Surface must be destroyed before the instance - renderer.Shutdown()
    // destroys the instance, so this has to happen first. uiRenderer also
    // owns Vulkan resources backed by renderer's device, so it must go first.
    if (surface != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(renderer.GetInstance(), surface, nullptr);
    uiRenderer.Shutdown();
    renderer.Shutdown();

    glfwDestroyWindow(window);
    glfwTerminate();
    return exitCode;
}
