# Compute Shader Analysis (컴퓨트 쉐이더 활용 분석)

이 프로젝트는 GPGPU 기술을 활용하여 CPU의 부하를 GPU로 분산시키는 여러 방식을 포함하고 있습니다.

## 1. 애니메이션 본 연산 가속 (`18_GetMultiBone.fx`)
- **목적**: 수많은 인스턴스 모델의 본(Bone) 트랜스폼 계산을 GPU에서 수행합니다.
- **동작 방식**: 
    - `InputWorlds`, `InputBones`를 `StructuredBuffer`로 입력받습니다.
    - 현재 프레임과 다음 프레임의 애니메이션 데이터를 `TransformsMap`(Texture2DArray)에서 읽어 보간(Lerp)합니다.
    - 계산된 최종 행렬을 `RWTexture2D` 형태의 `Output` 버퍼에 저장합니다.
- **최적화**: `MAX_MODEL_TRANSFORMS` 크기의 스레드 그룹을 사용하여 병렬 처리를 극대화합니다.

## 2. 원시 버퍼 처리 (`14_RawBuffer.fx`)
- **특징**: `ByteAddressBuffer`와 `RWByteAddressBuffer`를 사용한 저수준 데이터 접근을 보여줍니다.
- **활용**: 정해진 구조 없이 바이트 단위로 데이터를 읽고 쓰며, 기본적인 산술 연산(덧셈, 곱셈 등) 결과를 GPU 메모리에 직접 기록합니다.

## 3. 이미지 및 텍스처 처리 (`16_TextureBuffer.fx`)
- **특징**: `RWTexture2D`를 사용하여 텍스처 데이터를 직접 수정합니다.
- **활용**: 이미지의 색상 반전, 그레이스케일 변환 등을 픽셀 쉐이더가 아닌 컴퓨트 쉐이더의 `Dispatch`를 통해 병렬 처리합니다.

## 4. 프레임워크 지원 (`Framework\Renders\Buffers\`)
- `RawBuffer`, `StructuredBuffer`, `TextureBuffer` 등 다양한 GPU 리소스 타입을 C++ 클래스로 래핑하여 제공합니다.
- `Shader::Dispatch`를 통해 간단하게 컴퓨트 쉐이더를 실행할 수 있는 인터페이스를 구축하고 있습니다.
