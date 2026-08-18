# DX12 오프라인 셰이더 컴파일 헬퍼 — CMakeLists.txt가 include()하여 사용한다.
# 신규 .hlsl 컴파일 대상을 추가하려면 이 파일이 아니라 ShaderList.cmake에
# add_dx_shader() 호출을 한 줄 추가하면 된다.
#
# 호출 전 CMakeLists.txt에서 아래 변수가 먼저 정의되어 있어야 한다:
#   _dx_shader_src           — .hlsl 소스 디렉토리 (_Shaders/DirectX)
#   _dx_shader_bin           — .cso 출력 디렉토리
#   _dx_shader_common_files  — 공용 헤더(.hlsli) 목록 (NO_COMMON 미지정 시 DEPENDS에 포함)
#   _ALL_SHADER_OUTPUTS      — "" 로 초기화된 누적 리스트 (add_dx_shader가 PARENT_SCOPE로 채움)

# add_dx_shader(<이름> STAGES <VS|PS|CS...> [NO_COMMON])
#   <이름>     : ${_dx_shader_src}/<이름>.hlsl (확장자 제외)
#   STAGES     : VS(→VSMain)/PS(→PSMain)/CS(→CSMain) 중 필요한 스테이지만 나열
#                VS PS 둘 다 지정하면 그래픽스 셰이더, CS 단독이면 컴퓨트 셰이더
#   NO_COMMON  : 이 셰이더가 _dx_shader_common_files(공용 .hlsli)를 include하지 않을 때 지정 —
#                지정하지 않으면 공용 헤더 변경 시에도 재컴파일되도록 DEPENDS에 자동 포함
function(add_dx_shader NAME)
    cmake_parse_arguments(ARG "NO_COMMON" "" "STAGES" ${ARGN})

    set(_src "${_dx_shader_src}/${NAME}.hlsl")
    set(_outputs "")
    set(_commands "")

    foreach(_stage ${ARG_STAGES})
        if(_stage STREQUAL "VS")
            set(_entry "VSMain")
        elseif(_stage STREQUAL "PS")
            set(_entry "PSMain")
        elseif(_stage STREQUAL "CS")
            set(_entry "CSMain")
        else()
            message(FATAL_ERROR "add_dx_shader(${NAME}): 알 수 없는 STAGE '${_stage}' (VS/PS/CS만 지원)")
        endif()

        string(TOLOWER ${_stage} _stage_lower)
        set(_out "${_dx_shader_bin}/${NAME}_${_stage}.cso")
        list(APPEND _outputs "${_out}")
        list(APPEND _commands
            COMMAND $<TARGET_FILE:VaShaderCompiler>
                    --target dx --in "${_src}" --stage ${_stage_lower} --entry ${_entry} --out "${_out}"
        )
    endforeach()

    set(_depends VaShaderCompiler "${_src}")
    if(NOT ARG_NO_COMMON)
        list(APPEND _depends ${_dx_shader_common_files})
    endif()

    add_custom_command(
        OUTPUT ${_outputs}
        ${_commands}
        DEPENDS ${_depends}
        COMMENT "셰이더 컴파일: ${NAME}"
        VERBATIM
    )

    set(_ALL_SHADER_OUTPUTS "${_ALL_SHADER_OUTPUTS};${_outputs}" PARENT_SCOPE)
endfunction()
