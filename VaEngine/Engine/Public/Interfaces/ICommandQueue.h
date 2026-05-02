#pragma once

#include <vector>
#include <cstdint>

class IFence;
class ICommandList;

class ICommandQueue
{
public:
	virtual ~ICommandQueue() = default;

	virtual void ExecuteCommandLists(const std::vector<ICommandList*>& commandLists) = 0;

	virtual void Signal(IFence* fence, uint64_t value) = 0;
	virtual void Wait(IFence* fence, uint64_t value) = 0;
};