#pragma once
#include "VulkanInclude.hpp"

struct DeleteCopy
{
	DeleteCopy() = default;
	~DeleteCopy() = default;
	DeleteCopy(const DeleteCopy&) = delete;
	DeleteCopy& operator=(const DeleteCopy&) = delete;
};

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

	class Core : public DeleteCopy
	{
	public:
		struct Desc
		{
			std::string ApplicationName;
			std::vector<const char*> Extensions;
			bool EnableValidationLayer = false;
		};

		Core() = default;
		~Core()
		{
			this->FlushGPU();
			vmaDestroyAllocator(m_VmaAllocator);
			vkDestroyDevice(m_Device, nullptr);
			vkDestroyInstance(m_Instance, nullptr);
		}

		Core(Core&& other) noexcept
		{
			*this = std::move(other);
		}

		Core& operator=(Core&& other) noexcept
		{
			std::swap(m_Instance, other.m_Instance);
			std::swap(m_Device, other.m_Device);
			std::swap(m_PhysicalDevice, other.m_PhysicalDevice);
			std::swap(m_VmaAllocator, other.m_VmaAllocator);
			std::swap(m_GraphicsQueueIndex, other.m_GraphicsQueueIndex);
			std::swap(m_ComputeQueueIndex, other.m_ComputeQueueIndex);
			std::swap(m_CopyQueueIndex, other.m_CopyQueueIndex);
			std::swap(m_VariadicDescriptorCount, other.m_VariadicDescriptorCount);
			std::swap(m_IsMeshShadingSupported, other.m_IsMeshShadingSupported);
			std::swap(m_IsSupportingDebugNames, other.m_IsSupportingDebugNames);
			return *this;
		}

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
			return std::numeric_limits<uint32_t>::max();
		}

		VkInstance GetInstance() const { return m_Instance; }
		VkDevice GetLogicalDevice() const { return m_Device; }
		VmaAllocator GetVmaAllocator() const { return m_VmaAllocator; }
		VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
		uint32_t GetVariadicDescriptorCount() const { return m_VariadicDescriptorCount; }
		bool IsMeshShadingSupported() const { return m_IsMeshShadingSupported; }
		bool IsDebugNamesSupported() const { return m_IsSupportingDebugNames; }

	private:
		VkInstance m_Instance;
		VkDevice m_Device;
		VmaAllocator m_VmaAllocator;
		VkPhysicalDevice m_PhysicalDevice;

		uint32_t m_GraphicsQueueIndex;
		uint32_t m_ComputeQueueIndex;
		uint32_t m_CopyQueueIndex;

		uint32_t m_VariadicDescriptorCount = 1024u;
		bool m_IsMeshShadingSupported = false;
		bool m_IsSupportingDebugNames = false;

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

			VkInstanceCreateInfo instanceCI{
				.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
				.pApplicationInfo = &appInfo,
				.enabledExtensionCount = static_cast<uint32_t>(desc.Extensions.size()),
				.ppEnabledExtensionNames = desc.Extensions.data()
			};

			std::vector<const char*> layers;
			if (this->CheckValidationLayerSupport() && desc.EnableValidationLayer)
			{
				layers.push_back("VK_LAYER_KHRONOS_validation");
			}

			instanceCI.enabledLayerCount = (uint32_t)layers.size();
			instanceCI.ppEnabledLayerNames = layers.data();

			chkSucceed(vkCreateInstance(&instanceCI, nullptr, &m_Instance));

			volkLoadInstance(m_Instance); // hooks up function pointers for the instance

			for (uint32_t i = 0; i < desc.Extensions.size(); i++) 
			{
				if (std::string(desc.Extensions[i]) == std::string(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
				{
					m_IsSupportingDebugNames = true;
					break;
				}
			}
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
			const std::vector<const char*> deviceExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME, VK_EXT_MESH_SHADER_EXTENSION_NAME };

			//VkPhysicalDeviceMeshShaderFeaturesEXT meshFeatures{
			//		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
			//};

			//VkPhysicalDeviceFeatures2 meshFeaturesPD{
			//	.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
			//	.pNext = &meshFeatures
			//};

			//vkGetPhysicalDeviceFeatures2(
			//	m_PhysicalDevice,
			//	&meshFeaturesPD);

			//m_IsMeshShadingSupported = meshFeatures.meshShader && meshFeatures.taskShader;

			//if (m_IsMeshShadingSupported)
			//{
			//	meshFeatures.meshShader = VK_TRUE;
			//	meshFeatures.taskShader = VK_TRUE;
			//	meshFeatures.multiviewMeshShader = VK_FALSE;
			//	meshFeatures.primitiveFragmentShadingRateMeshShader = VK_FALSE;
			//}
			//else
			//{
			//	meshFeaturesPD.pNext = nullptr;
			//}

			//meshFeaturesPD.features.samplerAnisotropy = VK_TRUE;
			//meshFeaturesPD.features.shaderInt64 = VK_TRUE;
			//meshFeaturesPD.features.shaderInt16 = VK_TRUE;

			//VkPhysicalDeviceVulkan13Features vk13Features{
			//	.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
			//	.pNext = &meshFeaturesPD,
			//	.synchronization2 = true,
			//	.dynamicRendering = true,
			//};

			//VkPhysicalDeviceVulkan12Features vk12Features{
			//	.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
			//	.pNext = &vk13Features,
			//	.shaderFloat16 = VK_TRUE, // enables float16 in shaders
			//	.descriptorIndexing = true,
			//	.shaderSampledImageArrayNonUniformIndexing = true,
			//	.descriptorBindingVariableDescriptorCount = true,
			//	.runtimeDescriptorArray = true,
			//	.timelineSemaphore = VK_TRUE,
			//	.bufferDeviceAddress = true,
			//};

			//VkPhysicalDeviceVulkan11Features vk11Features{
			//	.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
			//	.pNext = &vk12Features,
			//	.variablePointersStorageBuffer = VK_TRUE,
			//};

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
				.queueCreateInfoCount = static_cast<uint32_t>(queueCIs.size()),
				.pQueueCreateInfos = queueCIs.data(),
				.enabledExtensionCount = (uint32_t)deviceExtensions.size(),
				.ppEnabledExtensionNames = deviceExtensions.data(),
				.pEnabledFeatures = nullptr
			};

			VkPhysicalDeviceFeatures2 supportedFeatures{};
			VkPhysicalDeviceVulkan11Features supported11{}; VkPhysicalDeviceVulkan12Features supported12{}; VkPhysicalDeviceVulkan13Features supported13{};
			VkPhysicalDeviceMeshShaderFeaturesEXT supportedMesh{};
			this->HandleFeatures(deviceCI, supportedFeatures, supported11, supported12, supported13, supportedMesh);

			chkSucceed(vkCreateDevice(m_PhysicalDevice, &deviceCI, nullptr, &m_Device));
		}

		void HandleFeatures(
			VkDeviceCreateInfo& createInfo,
			VkPhysicalDeviceFeatures2& supportedFeatures,
			VkPhysicalDeviceVulkan11Features& supported11,
			VkPhysicalDeviceVulkan12Features& supported12,
			VkPhysicalDeviceVulkan13Features& supported13,
			VkPhysicalDeviceMeshShaderFeaturesEXT& supportedMesh
		)
		{
			supportedFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			supported11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
			supported12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
			supported13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
			supportedMesh.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;

			supportedFeatures.pNext = &supported11;
			supported11.pNext = &supported12;
			supported12.pNext = &supported13;
			supported13.pNext = &supportedMesh;

			vkGetPhysicalDeviceFeatures2(
				m_PhysicalDevice,
				&supportedFeatures
			);

			m_IsMeshShadingSupported = supportedMesh.meshShader && supportedMesh.taskShader;

			if (m_IsMeshShadingSupported)
			{
				supportedMesh.meshShader = VK_TRUE;
				supportedMesh.taskShader = VK_TRUE;
				supportedMesh.multiviewMeshShader = VK_FALSE;
				supportedMesh.primitiveFragmentShadingRateMeshShader = VK_FALSE;
			}
			else
			{
				supportedMesh.pNext = nullptr;
			}

			if (supportedFeatures.features.samplerAnisotropy == VK_FALSE) {

			}
			if (supportedFeatures.features.shaderInt64 == VK_FALSE) {

			}
			if (supportedFeatures.features.shaderInt16 == VK_FALSE) {

			}

			// vk13
			if (supported13.dynamicRendering == VK_FALSE) {

			}
			if (supported13.synchronization2 == VK_FALSE) {

			}

			// vk12
			if (supported12.shaderFloat16 == VK_FALSE) {

			}
			if (supported12.descriptorIndexing == VK_FALSE) {

			}
			if (supported12.shaderSampledImageArrayNonUniformIndexing == VK_FALSE) {

			}
			if (supported12.descriptorBindingVariableDescriptorCount == VK_FALSE) {

			}
			if (supported12.runtimeDescriptorArray == VK_FALSE) {

			}
			if (supported12.timelineSemaphore == VK_FALSE) {

			}
			if (supported12.bufferDeviceAddress == VK_FALSE) {

			}

			// vk11
			if (supported11.variablePointersStorageBuffer == VK_FALSE) {

			}

			createInfo.pNext = &supportedFeatures;
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
			
			for (const VkLayerProperties& layer : availableLayers)
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