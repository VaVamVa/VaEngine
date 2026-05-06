#pragma once

#include "targetver.h"

#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <memory>


#include "RHI/IRenderDevice.h"
#include "RHI/ISwapChain.h"
#include "RHI/ICommandQueue.h"
#include "RHI/ICommandList.h"
#include "RHI/IFence.h"

#include "RHI/RHILoader.h"


#include "Private/Utility/Macros.h"