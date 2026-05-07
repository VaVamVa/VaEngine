#include "System/ITime.h"

#include <chrono>

class Time : public ITime
{
public:
	Time()
	{
		auto now = std::chrono::high_resolution_clock::now();
		startPoint  = now;
		lastPoint   = now;
		fpsLastPoint = now;
	}

	void Update() override
	{
		if (stopped) return;

		auto now = std::chrono::high_resolution_clock::now();
		delta    = std::chrono::duration<float>(now - lastPoint).count();
		running  = std::chrono::duration<float>(now - startPoint).count();
		lastPoint = now;

		++frameCount;
		float fpsElapsed = std::chrono::duration<float>(now - fpsLastPoint).count();
		if (fpsElapsed >= 0.5f)
		{
			fps          = static_cast<float>(frameCount) / fpsElapsed;
			frameCount   = 0;
			fpsLastPoint = now;
		}
	}

	void Start() override
	{
		if (!stopped) return;
		lastPoint = std::chrono::high_resolution_clock::now();
		stopped   = false;
	}

	void Stop() override
	{
		if (stopped) return;
		auto now = std::chrono::high_resolution_clock::now();
		running += std::chrono::duration<float>(now - lastPoint).count();
		stopped  = true;
	}

	float Delta()     const override { return delta;   }
	float FPS()       const override { return fps;     }
	float Running()   const override { return running; }
	bool  IsStopped() const override { return stopped; }

private:
	using Clock     = std::chrono::high_resolution_clock;
	using TimePoint = Clock::time_point;

	TimePoint startPoint;
	TimePoint lastPoint;
	TimePoint fpsLastPoint;

	float   delta      = 0.0f;
	float   running    = 0.0f;
	float   fps        = 0.0f;
	uint32_t frameCount = 0;
	bool    stopped    = true;
};

std::unique_ptr<ITime> ITime::Create()
{
	return std::make_unique<Time>();
}
