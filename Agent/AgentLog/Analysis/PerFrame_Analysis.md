# Per Frame Analysis (프레임 단위 데이터 처리 분석)

이 프로젝트에서 프레임마다 갱신되는 전역 데이터(카메라 행렬, 시간 등)는 `PerFrame` 시스템을 통해 관리됩니다.

## 1. 주요 구조체 및 상수 버퍼 (Constant Buffer)
`_Shaders\00_Global.fx`에 정의된 `CB_PerFrame`은 렌더링에 필요한 필수 행렬들을 포함합니다.

```hlsl
cbuffer CB_PerFrame
{
    Matrix View;            // 뷰 행렬
    Matrix ViewInverse;     // 뷰 역행렬 (카메라 위치 계산용)
    Matrix Projection;      // 투영 행렬
    Matrix VP;              // View * Projection 행렬
    
    float4 Culling[4];      // 컬링용 평면
    float4 Cliping;         // 클리핑 설정

    float Time;             // 경과 시간
    float Padding[3];
};
```

## 2. CPU 측 관리 (`Framework\Renders\PerFrame.cpp`)
- **행렬 계산**: 매 프레임 `D3DXMatrixInverse`를 사용하여 `View` 행렬의 역행렬을 계산합니다.
- **업데이트**: 계산된 행렬과 현재 시간(`Time::Delta()`) 등을 버퍼에 기록하고 GPU로 전송합니다.
- **VP 행렬**: 쉐이더 내에서의 연산을 줄이기 위해 `View * Projection`을 미리 계산하여 제공합니다.

## 3. 쉐이더에서의 활용
- **카메라 위치 추출**: `ViewInverse._41_42_43`을 통해 월드 좌표계에서의 카메라 위치를 즉시 파악합니다.
- **좌표 변환**: `mul(position, VP)`를 통해 로컬/월드 정점을 화면 좌표계로 변환하는 데 핵심적인 역할을 합니다.
- **시간 기반 효과**: `Time` 변수를 활용하여 애니메이션, 파티클, 혹은 `Wiggle` 같은 포스트 이펙트의 오프셋을 계산합니다.
