#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <vulkan/vulkan.h>

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include <vector>
#include <optional>
#include <set>
#include <algorithm>





class WindowApplication {
public:
	const uint32_t WIDTH	= 800;
	const uint32_t HEIGHT	= 600;
	
	const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	const std::vector<const char*> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

#ifdef NODEBUG
	const bool enabledValidationLayers = false;
#else
	const bool enabledValidationLayers = true;
#endif


	void run() {
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

private:
	GLFWwindow*			window			= NULL;
	VkInstance			instance		= NULL;
	VkPhysicalDevice	physicalDevice	= NULL;

	VkDevice			device			= NULL;
	
	VkQueue				graphicsQueue	= NULL;
	
	VkSurfaceKHR		surface			= NULL;
	VkQueue				presentQueue	= NULL;

	VkSwapchainKHR			swapChain	= NULL;
	std::vector<VkImage>	swapChainImages;
	VkFormat				swapChainImageFormat;
	VkExtent2D				swapChainExtent;




	// 選択したレイヤーのサポートを確認
	bool checkValidationLayerSupport() {
		// 使用できるレイヤー情報を取得

		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, NULL);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		// 選択されたレイヤーのサポートを確認

		for (const char* layerName : validationLayers) {
			bool layerFound = false;

			for (auto &layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				return false;
			}
		}
		return true;
	}

	// アプリインタンスを作成
	void createInstance() {
		// アプリ情報

		VkApplicationInfo appInfo;

		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Base Vulkan Application";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_MAKE_VERSION(1, 0, 0);

		// インスタンスの作成パラメーターを設定する

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		// Vulkan に GLFW でスキャンされた拡張機能を導入

		uint32_t glfwExtensionCount = 0;
		const char** glfwExtension;

		glfwExtension = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		createInfo.enabledExtensionCount = glfwExtensionCount;
		createInfo.ppEnabledExtensionNames = glfwExtension;

		// 検証レイヤー

		if (enabledValidationLayers) {
			createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else {
			createInfo.enabledLayerCount = 0;
		}

		// インスタンスの作成を実行する

		VkResult result = vkCreateInstance(&createInfo, NULL, &instance);

		// (ERROR) インスタンスの作成が失敗されました
		if (result != VK_SUCCESS) {
			printf("Vulkan: failed to create instance\n");
			raise(SIGFPE);
		}
	}


	
	bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

		for (const auto& extension : availableExtensions) {
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

	bool isDeviceSuitable(VkPhysicalDevice device) {
		QueueFamilyIndices indices = findQueueFamilies(device);

		bool extensionsSupported = checkDeviceExtensionSupport(device);

		return indices.isComplete() && extensionsSupported;
	}

	// デバイスの理論性能を測定する
	int rateDeviceSuitability(VkPhysicalDevice device) {
		VkPhysicalDeviceProperties properties;
		VkPhysicalDeviceFeatures features;
		vkGetPhysicalDeviceProperties(device, &properties);
		vkGetPhysicalDeviceFeatures(device, &features);

		int score = 0;

		// Application can't function without geometry shaders
		if (!features.geometryShader || !isDeviceSuitable(device)) {
			return 0;
		}

		// Discrete GPUs have a significant performance advantage
		if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			score += 1000;
		}

		// Maximum possible size of textures affects graphics quality
		score += properties.limits.maxImageDimension3D;

		return score;
	}

	// プログラムに使用するグラフィックスカードを選択する
	void pickPhysicalDevice() {
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);

		// (ERROR) サポートしている GPU が見つからない
		if (deviceCount == 0) {
			printf("failed to find any GPUs with Vulkan support!\n");
			raise(SIGFPE);
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		// GPU の理論性能で選択する

		int biggestScore = 0;

		for (auto &device : devices) {
			int score = rateDeviceSuitability(device);

			if (score > biggestScore) {
				physicalDevice = device;
				biggestScore = score;
			}
		}

		// (ERROR) 相応しい GPU が見つからない
		if (biggestScore == 0) {
			printf("Vulkan: failed to find a suitable GPU!\n");
			raise(SIGFPE);
		}
	}


	struct QueueFamilyIndices {
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;

		bool isComplete() {
			return graphicsFamily.has_value() && presentFamily.has_value();
		}
	};
	
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
		QueueFamilyIndices indices;

		// コマンドサポートを確認

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		int i = 0;
		for (const auto& queueFamily : queueFamilies) {
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				indices.graphicsFamily = i;
			}

			// プレゼンテーションサポートを確認

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

			if (presentSupport) {
				indices.presentFamily = i;
			}

			// 完成
			if (indices.isComplete()) {
				break;
			}
			
			i++;
		}

		return indices;
	}


	void createLogicalDevice() {
		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

		//VkDeviceQueueCreateInfo queueCreateInfo{};
		//queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		//queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
		//queueCreateInfo.queueCount = 1;

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

		float queuePriority = 1.0f;
		
		for (uint32_t queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		

		VkPhysicalDeviceFeatures deviceFeatures{};

		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
		
		createInfo.pEnabledFeatures = &deviceFeatures;

		createInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();

		if (enabledValidationLayers) {
			createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else {
			createInfo.enabledLayerCount = 0;
		}

		VkResult result = vkCreateDevice(physicalDevice, &createInfo, NULL, &device);

		if (result != VK_SUCCESS) {
			printf("Vulkan: failed to create logical device!\n");
			raise(SIGFPE);
		}

		vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
		vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
	}

	void createSurface() {
		VkResult result = glfwCreateWindowSurface(instance, window, NULL, &surface);

		// (ERROR) Surface 作成失敗
		if (result != VK_SUCCESS) {
			printf("Vulkan: failed to create window surface!\n");
			raise(SIGFPE);
		}
	}


	struct SwapChainSupportDetails {
		VkSurfaceCapabilitiesKHR		capabilities;
		std::vector<VkSurfaceFormatKHR>	formats;
		std::vector<VkPresentModeKHR>	presentModes;
	};

	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
		SwapChainSupportDetails details;

		return details;
	}

	void createSwapChain() {
		SwapChainSupportDetails details = querySwapChainSupport();

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);

		if (formatCount != 0) {
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats.data());
		}

	}







	void initWindow() {
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE,  GLFW_FALSE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "C++ Application", NULL, NULL);
	}

	void initVulkan() {
		createInstance();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
	}

	void mainLoop() {
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
		}
	}

	void cleanup() {
		vkDestroySwapchainKHR(device, swapChain, NULL);
		vkDestroyDevice(device, NULL);

		vkDestroySurfaceKHR(instance, surface, NULL);
		vkDestroyInstance(instance, NULL);

		glfwDestroyWindow(window);
		glfwTerminate();
	}
};




void sighandler(int signum) {
	printf("エラー %d、プログラムを終了する\n", signum);
	exit(1);
}



int main(int argc, char** argv) {
	signal(SIGFPE, sighandler);

	WindowApplication app;
	app.run();

	return 0;
}