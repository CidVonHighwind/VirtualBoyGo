// Build-time GLSL -> SPIR-V compiler using glslang's C++ API directly,
// so the project never needs a system-installed Vulkan SDK / glslc.
#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <SPIRV/GlslangToSpv.h>

#include <cctype>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

EShLanguage StageFromPath(const std::string& path) {
    if (path.size() >= 5 && path.compare(path.size() - 5, 5, ".vert") == 0) return EShLangVertex;
    if (path.size() >= 5 && path.compare(path.size() - 5, 5, ".frag") == 0) return EShLangFragment;
    std::cerr << "Unknown shader stage for: " << path << "\n";
    std::exit(1);
}

}  // namespace

std::string SymbolNameFromPath(const std::string& path) {
    std::string base = path;
    const size_t slash = base.find_last_of("/\\");
    if (slash != std::string::npos) base = base.substr(slash + 1);
    for (char& c : base) {
        if (!isalnum(static_cast<unsigned char>(c))) c = '_';
    }
    return "g_" + base;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: ShaderCompiler <in.vert|in.frag> <out.h>\n";
        return 1;
    }
    const std::string inPath = argv[1];
    const std::string outPath = argv[2];

    std::ifstream in(inPath, std::ios::binary);
    if (!in) {
        std::cerr << "ShaderCompiler: cannot open " << inPath << "\n";
        return 1;
    }
    const std::string src((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    const EShLanguage stage = StageFromPath(inPath);

    glslang::InitializeProcess();

    glslang::TShader shader(stage);
    const char* srcs[] = {src.c_str()};
    shader.setStrings(srcs, 1);
    shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

    if (!shader.parse(GetDefaultResources(), 100, false, EShMsgDefault)) {
        std::cerr << "ShaderCompiler: parse failed for " << inPath << "\n"
                   << shader.getInfoLog() << "\n"
                   << shader.getInfoDebugLog() << "\n";
        return 1;
    }

    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(EShMsgDefault)) {
        std::cerr << "ShaderCompiler: link failed for " << inPath << "\n" << program.getInfoLog() << "\n";
        return 1;
    }

    std::vector<uint32_t> spirv;
    glslang::GlslangToSpv(*program.getIntermediate(stage), spirv);

    // Emit as an embeddable C header (array), so both PC and Android can
    // link the shader in directly instead of needing filesystem/asset access.
    const std::string symbol = SymbolNameFromPath(inPath);
    std::ofstream out(outPath);
    out << "#pragma once\n#include <cstdint>\n";
    out << "static const uint32_t " << symbol << "[] = {\n";
    for (size_t i = 0; i < spirv.size(); ++i) {
        out << spirv[i] << ",";
        if ((i + 1) % 12 == 0) out << "\n";
    }
    out << "\n};\n";
    out << "static const uint32_t " << symbol << "_size = " << (spirv.size() * sizeof(uint32_t)) << ";\n";

    glslang::FinalizeProcess();
    return 0;
}
