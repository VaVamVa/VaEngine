#pragma once

#include "targetver.h"

#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <memory>


#include "Interfaces/IRenderDevice.h"
#include "Interfaces/ISwapChain.h"
#include "Interfaces/ICommandQueue.h"
#include "Interfaces/ICommandList.h"
#include "Interfaces/IFence.h"

#include "RHI/RHILoader.h"


#include "Private/Utility/Macros.h"