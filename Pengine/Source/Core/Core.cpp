#include "Core.h"

using namespace Pengine;

GlobalDataAccessor& GlobalDataAccessor::GetInstance()
{
	static GlobalDataAccessor globalDataAccessor;
	return globalDataAccessor;
}

std::unordered_map<UUID, std::filesystem::path, uuid_hash>& GlobalDataAccessor::GetFilepathByUuid() { return filepathByUuid; }
std::unordered_map<std::filesystem::path, UUID, path_hash>& GlobalDataAccessor::GetUuidByFilepath() { return uuidByFilepath; }

int GlobalDataAccessor::GetDrawCallCount() const { return drawCallCount; }
size_t GlobalDataAccessor::GetTriangleCount() const { return triangleCount; }
size_t GlobalDataAccessor::GetCurrentFrame() const { return currentFrame; }
int64_t GlobalDataAccessor::GetVramAllocated() const { return vramAllocated; }

uint32_t& GlobalDataAccessor::GetFrameInFlightCount() { return Vk::frameInFlightCount; }
uint32_t& GlobalDataAccessor::GetFrameInFlightIndex() { return Vk::frameInFlightIndex; }

uint32_t& GlobalDataAccessor::GetSwapChainImageCount() { return Vk::swapChainImageCount; }
uint32_t& GlobalDataAccessor::GetSwapChainImageIndex() { return Vk::swapChainImageIndex; }

std::mutex& Pengine::GlobalDataAccessor::GetUUIDMutex() { return uuidMutex; }

std::shared_ptr<class Device> GlobalDataAccessor::GetDevice() const { return device; }