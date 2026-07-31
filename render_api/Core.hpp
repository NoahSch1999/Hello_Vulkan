#pragma once
#include "VulkanInclude.hpp"

namespace aZero::RenderAPI
{
	inline bool chkSucceed(VkResult result)
	{
		if (result != VK_SUCCESS) {
			std::cerr << "Vulkan call returned an error (" << result << ")\n";
			return false;
		}

		return true;
	}

	enum class EQueueType { Graphics, Compute, Copy };

	class Core
	{
	public:
		struct Desc
		{
			std::string ApplicationName;
			bool EnableValidationLayer = false;
		};

		Core() = default;
		Core(const Desc& desc) 
		{
			this->Init(desc);
		}

		void Init(const Desc& desc)
		{
			this->InitAppInstance(desc);
			this->InitPhysicalDevice();
			this->InitDeviceAndQueues();
			this->InitVma();
		}

		void FlushGPU() const { chkSucceed(vkDeviceWaitIdle(m_Device)); }

		std::string GetPhysicalDeviceName() const {
			VkPhysicalDeviceProperties2 deviceProperties{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
			vkGetPhysicalDeviceProperties2(m_PhysicalDevice, &deviceProperties);
			return deviceProperties.properties.deviceName;
		}

		uint32_t GetIndexForQueueType(EQueueType type) const {
			if (type == EQueueType::Graphics) {
				return m_GraphicsQueueIndex;
			}
			else if (type == EQueueType::Compute) {
				return m_ComputeQueueIndex;
			}
			else if (type == EQueueType::Copy) {
				return m_CopyQueueIndex;
			}
		}

		VkInstance GetInstance() const { return m_Instance; }
		VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
		VkDevice GetLogicalDevice() const { return m_Device; }
		VmaAllocator GetVmaAllocator() const { return m_VmaAllocator; }

	private:
		VkInstance m_Instance;
		VkPhysicalDevice m_PhysicalDevice;
		VkDevice m_Device;

		uint32_t m_GraphicsQueueIndex;
		uint32_t m_ComputeQueueIndex;
		uint32_t m_CopyQueueIndex;

		VmaAllocator m_VmaAllocator;


		const char* const ValidationLayerSL = "VK_LAYER_KHRONOS_validation";

		void InitAppInstance(const Desc& desc)
		{
			volkInitialize();

			// create app and instance
			VkApplicationInfo appInfo{
				.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
				.pApplicationName = desc.ApplicationName.c_str(),
				.apiVersion = VK_API_VERSION_1_3
			};

			uint32_t sdlExtensionCount = 0;
			char const* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
			std::vector<const char*> extensions;
			extensions.reserve(sdlExtensionCount + 1);
			for (uint32_t i = 0; i < sdlExtensionCount; i++)
			{
				extensions.push_back(sdlExtensions[i]);
			}

			VkInstanceCreateInfo instanceCI{
				.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
				.pApplicationInfo = &appInfo,
				.enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
				.ppEnabledExtensionNames = extensions.data()
			};

			std::vector<const char*> layers;
			if (this->CheckValidationLayerSupport())
			{
				layers.push_back("VK_LAYER_KHRONOS_validation");
			}

			instanceCI.enabledLayerCount = (uint32_t)layers.size();
			instanceCI.ppEnabledLayerNames = layers.data();

			chkSucceed(vkCreateInstance(&instanceCI, nullptr, &m_Instance));

			volkLoadInstance(m_Instance); // hooks up function pointers for the instance
		}

		void InitPhysicalDevice()
		{
			// lookup available devices
			uint32_t deviceCount{ 0 };
			chkSucceed(vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr)); // get count without initializing the list of devices since we dont know how many yet
			std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
			chkSucceed(vkEnumeratePhysicalDevices(m_Instance, &deviceCount, physicalDevices.data())); // do it again

			m_PhysicalDevice = physicalDevices[0];
		}

		void InitDeviceAndQueues()
		{
			// find graphics queue and queue support
			uint32_t queueFamilyCount{ 0 };
			vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);
			std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, queueFamilies.data());

			for (size_t i = 0; i < queueFamilies.size(); i++) {
				if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
					m_GraphicsQueueIndex = i;
					continue;
				}
				if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
					m_ComputeQueueIndex = i;
					continue;
				}
				if (queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
					m_CopyQueueIndex = i;
					continue;
				}
			}
			//chk(SDL_Vulkan_GetPresentationSupport(instance, physicalDevices[deviceIndex], queueFamily));

			// configure features
			VkPhysicalDeviceVulkan12Features enabledVk12Features{
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
				.shaderFloat16 = VK_TRUE, // enables float16 in shaders
				.descriptorIndexing = true,
				.shaderSampledImageArrayNonUniformIndexing = true,
				.descriptorBindingVariableDescriptorCount = true,
				.runtimeDescriptorArray = true,
				.timelineSemaphore = VK_TRUE,
				.bufferDeviceAddress = true,
			};
			VkPhysicalDeviceVulkan13Features enabledVk13Features{
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
				.pNext = &enabledVk12Features,
				.synchronization2 = true,
				.dynamicRendering = true,
			};
			VkPhysicalDeviceFeatures enabledVk10Features{
				.samplerAnisotropy = VK_TRUE,
				.shaderInt64 = VK_TRUE,
				.shaderInt16 = VK_TRUE, // enables int16 in shaders
			};

			// create logical device
			const std::vector<const char*> deviceExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME };

			const float qfpriorities{ 1.f };
			VkDeviceQueueCreateInfo graphicsQueueCI{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex = m_GraphicsQueueIndex,
				.queueCount = 1,
				.pQueuePriorities = &qfpriorities
			};

			VkDeviceQueueCreateInfo computeQueueCI{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex = m_ComputeQueueIndex,
				.queueCount = 1,
				.pQueuePriorities = &qfpriorities
			};

			VkDeviceQueueCreateInfo copyQueueCI{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex = m_CopyQueueIndex,
				.queueCount = 1,
				.pQueuePriorities = &qfpriorities
			};

			std::vector<VkDeviceQueueCreateInfo> queueCIs{ graphicsQueueCI, computeQueueCI, copyQueueCI };

			VkDeviceCreateInfo deviceCI{
				.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
				.pNext = &enabledVk13Features,
				.queueCreateInfoCount = static_cast<uint32_t>(queueCIs.size()),
				.pQueueCreateInfos = queueCIs.data(),
				.enabledExtensionCount = (uint32_t)deviceExtensions.size(),
				.ppEnabledExtensionNames = deviceExtensions.data(),
				.pEnabledFeatures = &enabledVk10Features
			};

			chkSucceed(vkCreateDevice(m_PhysicalDevice, &deviceCI, nullptr, &m_Device));
		}

		void InitVma()
		{
			// setup vma
			VmaVulkanFunctions vkFunctions{
				.vkGetInstanceProcAddr = vkGetInstanceProcAddr,
				.vkGetDeviceProcAddr = vkGetDeviceProcAddr,
				.vkCreateImage = vkCreateImage
			};

			VmaAllocatorCreateInfo allocatorCI{
				.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
				.physicalDevice = m_PhysicalDevice,
				.device = m_Device,
				.pVulkanFunctions = &vkFunctions,
				.instance = m_Instance
			};

			chkSucceed(vmaCreateAllocator(&allocatorCI, &m_VmaAllocator));
		}

		bool CheckValidationLayerSupport()
		{
			uint32_t layerCount = 0;
			vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

			std::vector<VkLayerProperties> availableLayers(layerCount);
			vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
			
			for (auto layer : availableLayers)
			{
				if (strcmp(ValidationLayerSL, layer.layerName) == 0)
				{
					return true;
				}
			}

			return false;
		}
	};
}