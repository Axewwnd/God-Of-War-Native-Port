"""Adapt only the pinned Framework loader to a process-lifetime handle selected by ARES."""
import hashlib
import pathlib
import sys
source, output = map(pathlib.Path, sys.argv[1:])
raw = source.read_bytes()
if hashlib.sha256(raw).hexdigest() != 'a3ebf1ffceadf8a6a674396ade98d18f860dbf4665c38f4d965dafba23d66113':
    raise SystemExit('Unvalidated Framework loader source')
text = raw.decode()
start = text.index('\tassert(m_vulkanDl == nullptr);')
end = text.index('\n#endif\n\n\tvkCreateInstance', start)
text = text[:start] + '''\tassert(m_vulkanDl == nullptr);
    m_vulkanDl = ares_play_vulkan_library();
    if(!m_vulkanDl || !dlsym(m_vulkanDl, "vkGetInstanceProcAddr"))
        throw std::runtime_error("ARES must provide its validated Vulkan loader before GS initialization");
''' + text[end:]
text = 'extern "C" void* ares_play_vulkan_library();\n' + text
needle = '\tvkEnumerateInstanceExtensionProperties = reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(GetLibraryProcAddr("vkEnumerateInstanceExtensionProperties"));'
if text.count(needle) != 1:
    raise SystemExit('Unexpected loader entry points')
text = text.replace(needle, needle + '''
    if(!vkCreateInstance || !vkGetInstanceProcAddr || !vkEnumerateInstanceLayerProperties || !vkEnumerateInstanceExtensionProperties)
        throw std::runtime_error("ARES Vulkan loader lacks required global entry points");''')
output.write_text(text)
