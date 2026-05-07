#pragma once

#include <memory>

class ITime
{
public:
	static std::unique_ptr<ITime> Create();
	virtual ~ITime() = default;

	virtual void Update() = 0;
	virtual void Start()  = 0;
	virtual void Stop()   = 0;

	virtual float Delta()     const = 0;  // 프레임 간 시간(초)
	virtual float FPS()       const = 0;
	virtual float Running()   const = 0;  // 총 경과 시간(초)
	virtual bool  IsStopped() const = 0;
};
