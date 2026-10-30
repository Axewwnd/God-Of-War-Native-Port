"""Extract pinned, unmodified GS shader builders; replace only the Vulkan upload with a file sink."""
import pathlib
import subprocess
import sys
root, output = map(pathlib.Path, sys.argv[1:])
for path, revision in [(root, '1f27232772e484bfb79364b751b84f41f4c87481'),
                       (root / 'deps/Nuanceur', 'd96578b5a18edc67a268ed4811f0a035c56de7a0')]:
    actual = subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != revision:
        raise SystemExit(f'Unvalidated revision: {path}: {actual}')
src = (root / 'Source/gs/GSH_Vulkan/GSH_VulkanDrawMobile.cpp').read_text()
header = src[:src.index('CDrawMobile::CDrawMobile')]
header += '\n#include <fstream>\n#include <string>\nusing PIPELINE_CAPS = CDraw::PIPELINE_CAPS;\n'
header += '''static std::string outputPath;
static void Save(const Framework::CMemStream& stream) {
    std::ofstream out(outputPath, std::ios::binary);
    out.write(reinterpret_cast<const char*>(stream.GetBuffer()), stream.GetSize());
    if (!out) throw std::runtime_error("Could not save shader");
}
'''
functions = ['CreateLoadStoreVertexShader', 'CreateDrawFragmentShader', 'CreateLoadFragmentShader', 'CreateStoreFragmentShader']
for name in functions:
    start = src.index('Framework::Vulkan::CShaderModule CDrawMobile::' + name)
    brace = src.index('{', start)
    # These pinned functions have no braces inside string literals or comments.
    depth, end = 1, brace + 1
    while depth:
        depth += (src[end] == '{') - (src[end] == '}')
        end += 1
    body = src[start:end]
    body = body.replace('Framework::Vulkan::CShaderModule CDrawMobile::', 'static void ', 1)
    old = 'return Framework::Vulkan::CShaderModule(m_context->device, shaderStream);'
    if body.count(old) != 1:
        raise SystemExit('Shader upload changed upstream')
    header += body.replace(old, 'Save(shaderStream);') + '\n'
header += '''int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const std::string dir = argv[1];
    outputPath = dir + "/load-store.vert.spv";
    CreateLoadStoreVertexShader();
    unsigned index = 0;
    for (unsigned fb : {0u, 1u, 2u}) {
        for (unsigned texture : {0u, 1u, 2u, 19u, 20u}) {
            for (unsigned effects : {0u, 1u, 2u}) {
                auto caps = make_convertible<PIPELINE_CAPS>(0);
                caps.framebufferFormat = fb;
                caps.depthbufferFormat = 0x30 + fb;
                caps.textureFormat = texture;
                caps.clutFormat = 0;
                caps.hasTexture = effects != 0;
                caps.hasFog = effects == 2;
                caps.hasAlphaBlending = effects == 2;
                caps.hasDstAlphaTest = effects == 2;
                caps.writeDepth = 1;
                caps.depthTestFunction = 1;
                caps.alphaTestFunction = 1;
                caps.colClamp = 1;
                const std::string prefix = dir + "/" + std::to_string(index++);
                outputPath = prefix + "-draw.frag.spv";
                CreateDrawFragmentShader(caps);
                outputPath = prefix + "-load.frag.spv";
                CreateLoadFragmentShader(caps);
                outputPath = prefix + "-store.frag.spv";
                CreateStoreFragmentShader(caps);
            }
        }
    }
}
'''
output.write_text(header)
