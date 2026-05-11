// Engine.cpp : 정적 라이브러리를 위한 함수를 정의합니다.
//

#include "pch.h"
#include "framework.h"
#include "Utilities/DebuggingHelper.h"

// VA_DEBUG 매크로는 Utilities/DebuggingHelper.h 에서 전역적으로 제어됩니다.
// 현재 설정값: 
#if VA_DEBUG
    #pragma message("VaEngine: VA_DEBUG is ON")
#else
    #pragma message("VaEngine: VA_DEBUG is OFF")
#endif

// TODO: 라이브러리 함수의 예제입니다.
void fnEngine()
{
}
