# Experimental mobile-only Vulkan 1.0 path. Do not enable for desktop GS.
# Nuanceur's uint32 Uniform + BufferBlock representation already fits SPIR-V 1.0.
# Reject features outside the tested mobile subset instead of silently emitting them.
function(ares_enable_play_spirv10)
    if(NOT TARGET Nuanceur)
        message(FATAL_ERROR "Create the pinned Nuanceur target before applying compatibility changes")
    endif()
    set(generator "${PLAY_ROOT}/deps/Nuanceur/src/generators/SpirvShaderGenerator.cpp")
    file(SHA256 "${generator}" digest)
    if(NOT digest STREQUAL "70ca48599cd87d137b9b52b01129a97c0394a6cf67a4b05f989bb6222116dbb3")
        message(FATAL_ERROR "Nuanceur generator changed: compatibility adjustment needs review")
    endif()
    file(READ "${generator}" source)
    string(REPLACE "Write32(0x00010300); //SPIR-V 1.3 (compatible with Vulkan 1.1)"
        "Write32(0x00010000); // ARES mobile subset: validated for Vulkan 1.0" source "${source}")
    string(REPLACE "m_has8BitInt |= m_has16BitInt;"
        "m_has8BitInt |= m_has16BitInt;\n\tif(m_has8BitInt || m_has16BitInt || hasInvocationInterlock)\n\t\tthrow std::runtime_error(\"ARES SPIR-V 1.0 mobile path rejects int8/int16/interlock\");" source "${source}")
    set(patched "${CMAKE_CURRENT_BINARY_DIR}/AresSpirvShaderGenerator10.cpp")
    file(WRITE "${patched}" "#include <stdexcept>\n${source}")
    get_target_property(sources Nuanceur SOURCES)
    list(FILTER sources EXCLUDE REGEX "(^|/)SpirvShaderGenerator\\.cpp$")
    list(APPEND sources "${patched}")
    set_property(TARGET Nuanceur PROPERTY SOURCES "${sources}")
endfunction()
