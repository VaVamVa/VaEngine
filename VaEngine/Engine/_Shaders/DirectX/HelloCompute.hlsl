// Step 2 — RawBufferDemo 재현 (가장 단순한 compute 검증).
// Output[i] = Input[i] * Input[i]
//
// 입력  : Input  [64] = { 0, 1, 2, ..., 63 }
// 기대값: Output [64] = { 0, 1, 4, 9, 16, ..., 3969 }
//
// 64 thread × 1 group = 64 element. Dispatch(1, 1, 1).

StructuredBuffer<uint>   Input  : register(t0);
RWStructuredBuffer<uint> Output : register(u0);

[numthreads(64, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    uint idx = dtid.x;
    uint v   = Input[idx];
    Output[idx] = v * v;
}
