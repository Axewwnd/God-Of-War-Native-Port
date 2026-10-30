#include "nuanceur/Builder.h"
#include "nuanceur/generators/SpirvShaderGenerator.h"
#include "MemStream.h"
#include <stdexcept>
#include <string>
#include <iostream>
int main() {
    for (unsigned mode = 0; mode < 3; ++mode) {
        Nuanceur::CShaderBuilder b;
        if (mode == 0) b.CreateUniformArrayUchar("unsupported8");
        if (mode == 1) b.CreateUniformArrayUshort("unsupported16");
        if (mode == 2) Nuanceur::BeginInvocationInterlock(b);
        Framework::CMemStream stream;
        bool rejected = false;
        try { Nuanceur::CSpirvShaderGenerator::Generate(stream, b, Nuanceur::CSpirvShaderGenerator::SHADER_TYPE_FRAGMENT); }
        catch (const std::runtime_error& e) {
            rejected = std::string(e.what()).find("mobile path rejects") != std::string::npos;
        }
        if (!rejected) throw std::runtime_error("Unsupported mobile feature escaped guard");
    }
    std::cout << "PASS: int8, int16 and fragment interlock rejected by mobile-only adaptation\n";
}
