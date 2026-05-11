#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <cstring>

#ifdef USE_DIRECTX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3dcompiler.h>

// D3D_COMPILE_STANDARD_FILE_INCLUDE는 #include를 항상 원본 소스 파일 기준으로 해석하므로
// 중첩 include(헤더가 다른 헤더를 include)가 올바르게 동작하지 않는다.
// 이 핸들러는 현재 열린 파일의 디렉토리를 스택으로 관리하여 상대 경로를 정확히 해석한다.
class FileInclude : public ID3DInclude
{
public:
    explicit FileInclude(std::filesystem::path baseDir)
        : dirStack_({ std::move(baseDir) }) {}

    HRESULT __stdcall Open(D3D_INCLUDE_TYPE, LPCSTR pFileName,
                           const void*, const void** ppData, UINT* pBytes) override
    {
        auto path = (dirStack_.back() / pFileName).lexically_normal();

        std::ifstream file(path, std::ios::binary);
        if (!file)
            return E_FAIL;

        std::vector<char> buf((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());

        auto* data = new char[buf.size()];
        std::memcpy(data, buf.data(), buf.size());
        *ppData = data;
        *pBytes = static_cast<UINT>(buf.size());

        dirStack_.push_back(path.parent_path());
        return S_OK;
    }

    HRESULT __stdcall Close(const void* pData) override
    {
        delete[] static_cast<const char*>(pData);
        dirStack_.pop_back();
        return S_OK;
    }

private:
    std::vector<std::filesystem::path> dirStack_;
};

#endif // USE_DIRECTX

// --- CLI helpers ---

static std::string ParseArg(int argc, char* argv[], std::string_view key)
{
    for (int i = 1; i + 1 < argc; ++i)
        if (argv[i] == key) return argv[i + 1];
    return {};
}

static bool HasFlag(int argc, char* argv[], std::string_view key)
{
    for (int i = 1; i < argc; ++i)
        if (argv[i] == key) return true;
    return false;
}

// --- Backend implementations ---

#ifdef USE_DIRECTX
static int CompileHLSL(const std::string& inPath, const std::string& stage,
                       const std::string& entry, const std::string& outPath, bool isDebug)
{
    const std::string profile = stage + "_5_0";

    UINT flags = D3DCOMPILE_PACK_MATRIX_ROW_MAJOR | D3DCOMPILE_WARNINGS_ARE_ERRORS;
    if (isDebug)
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    else
        flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;

    const std::wstring wInPath = std::filesystem::path(inPath).wstring();

    FileInclude includeHandler(std::filesystem::path(inPath).parent_path());

    ID3DBlob* code   = nullptr;
    ID3DBlob* errors = nullptr;
    HRESULT hr = D3DCompileFromFile(
        wInPath.c_str(), nullptr, &includeHandler,
        entry.c_str(), profile.c_str(), flags, 0, &code, &errors
    );

    if (errors && errors->GetBufferSize() > 0)
        std::cerr << static_cast<const char*>(errors->GetBufferPointer()) << '\n';
    if (errors) errors->Release();

    if (FAILED(hr))
    {
        std::cerr << "Compile failed: " << inPath << '\n';
        if (code) code->Release();
        return 1;
    }

    std::filesystem::create_directories(std::filesystem::path(outPath).parent_path());
    std::ofstream out(outPath, std::ios::binary);
    if (!out)
    {
        std::cerr << "Cannot write: " << outPath << '\n';
        code->Release();
        return 1;
    }
    out.write(static_cast<const char*>(code->GetBufferPointer()),
              static_cast<std::streamsize>(code->GetBufferSize()));
    code->Release();

    std::cout << "OK  " << outPath << '\n';
    return 0;
}
#endif // USE_DIRECTX

#ifdef USE_VULKAN
static int CompileGLSL(const std::string& /*inPath*/, const std::string& /*stage*/,
                       const std::string& /*outPath*/, bool /*isDebug*/)
{
    // TODO: shaderc (FetchContent) 연동 후 구현
    // shaderc_compiler_t compiler = shaderc_compiler_initialize();
    // shaderc_compilation_result_t result = shaderc_compile_into_spv(...)
    std::cerr << "Vulkan GLSL compiler not yet implemented\n";
    return 1;
}
#endif // USE_VULKAN

// --- Entry point ---

int main(int argc, char* argv[])
{
    const std::string target  = ParseArg(argc, argv, "--target");
    const std::string inPath  = ParseArg(argc, argv, "--in");
    const std::string stage   = ParseArg(argc, argv, "--stage");
    const std::string entry   = ParseArg(argc, argv, "--entry");
    const std::string outPath = ParseArg(argc, argv, "--out");
    const bool        isDebug = HasFlag(argc, argv, "--debug");

    if (inPath.empty() || stage.empty() || outPath.empty())
    {
        std::cerr << "Usage: VaShaderCompiler"
                     " --target <dx|vk>"
                     " --in <file>"
                     " --stage <vs|ps|cs|vert|frag|comp>"
                     " [--entry <entry>]"
                     " --out <file>"
                     " [--debug]\n";
        return 1;
    }

#ifdef USE_DIRECTX
    if (target == "dx")
    {
        if (entry.empty()) { std::cerr << "--entry is required for HLSL\n"; return 1; }
        return CompileHLSL(inPath, stage, entry, outPath, isDebug);
    }
#endif

#ifdef USE_VULKAN
    if (target == "vk")
        return CompileGLSL(inPath, stage, outPath, isDebug);
#endif

    std::cerr << "No backend compiled for --target " << (target.empty() ? "(missing)" : target) << '\n';
    return 1;
}
