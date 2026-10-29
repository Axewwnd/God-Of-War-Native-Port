# Investigación Vulkan para SPES

Objetivo: Redmi Note 11 SPES, Android 14, Adreno 610, Vulkan 1.1 comunicado por el usuario.
Estado: no hay un backend Vulkan conectado al juego. No hay prueba de renderizado ni jugabilidad.

## Aurora

Se inspeccionó encounter/aurora en 505b5ebc91d585948f41ff5b6ec14694af078891.
Su renderer es una implementación de GX (GameCube/Wii) sobre Dawn/WebGPU. ARES entrega estados y primitivas
GS de PS2 a `GSRasterBackend`, no llamadas GX. Enlazar Aurora no traduce esa interfaz.
La selección Compatibility de Dawn observada en esa revisión tampoco garantiza soporte en la Adreno 610.
El usuario aclaró que no necesita específicamente Aurora: necesita Vulkan compatible con su teléfono.
No se añadió una dependencia GX sin consumidor ni un backend que finja haber dibujado.

## Candidato móvil de Play!

Se inspeccionó jpd002/Play- en 1f27232772e484bfb79364b751b84f41f4c87481, licencia BSD de dos cláusulas.
`Source/gs/GSH_Vulkan/GSH_VulkanPlatformDefs.h` selecciona MOBILE en Android.
`CGSH_Vulkan::CreateInstance` pide Vulkan 1.0 para esa variante y 1.2 para desktop.
La variante móvil no exige fragment-shader-interlock, shaderInt16 ni shaderInt8 de la variante desktop.
Sí solicita `fragmentStoresAndAtomics` y `VK_KHR_swapchain`; deben verificarse en el dispositivo.
`Source/ui_android/GSH_VulkanAndroid.cpp` ya implementa superficie ANativeWindow.
Esto lo convierte en candidato para evaluación, no en una garantía de compatibilidad o rendimiento.

Prueba realizada localmente: 14 unidades de traducción del renderer y superficie Android compilaron con
NDK r28c, API 28, ABI arm64-v8a, y se archivaron en `libares_play_vulkan_compile_check.a`.
La prueba solo compila objetos: no resuelve todos sus símbolos, no enlaza un ejecutable/APK y no conecta ARES.
Los logs están en `ares-evidence/play-vulkan-*.log`.

Receta reproducible (tras clonar Play! y fijar la revisión indicada):

```sh
git -C /ruta/Play- submodule update --init deps/Dependencies deps/Framework deps/Nuanceur
git -C /ruta/Play-/deps/Dependencies submodule update --init ghc_filesystem
cmake -S tools/ares/play-vulkan-compile-check -B /ruta/build-play-vulkan -G Ninja \
  -DPLAY_ROOT=/ruta/Play- \
  -DCMAKE_TOOLCHAIN_FILE=/ruta/ndk/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28 -DCMAKE_BUILD_TYPE=Release
cmake --build /ruta/build-play-vulkan --parallel 2
```

## Integración pendiente

- Aislar y enlazar CGSHandler y sus dependencias Framework/Nuanceur/configuración; conservar licencias.
- Diseñar el puente: registros/imágenes GS originales hacia CGSHandler o traducción completa desde
  GSRasterBackend. No mezclar dos dueños de VRAM ni ignorar lecturas local-to-host.
- Conservar PMODE/DISPLAY/DISPFB, transferencias, CLUT y sincronización correctas; no basta traducir triángulos.
- Dar a Vulkan la propiedad de la superficie; el arranque actual usa raylib/EGL. Coordinar hilo y ciclo de vida,
  pérdida/recreación de ventana y presentación para evitar competir por ANativeWindow.
- Consultar capacidades reales y probar transferencias/lecturas antes del arranque del juego.
- Compilar una APK nueva con el puente real y verificar imagen, audio y controles en hardware.

## Recompilación local solicitada

Se extrajo nuevamente SCUS_973.99 del enlace Archive.org del usuario. SHA256:
`ec9397d252f14d4412daffaad6be1f1931769e56222b5dbb9b39a947fc8ad71c`, idéntico a la entrada de generación.
Gradle assembleRelease --rerun-tasks terminó en 20s reutilizando objetos nativos que no cambiaron.
La biblioteca nativa es idéntica a la publicada antes; esta recompilación no cambia el comportamiento gráfico.
La nueva APK mide 54038192 bytes y su SHA256 es
`2b1645ddf074a0530ffb57b37002de3af89c58565c592d302ee2f9bd075737f5`.
Verificación: AArch64, entrada NativeActivity y 6415/6415 funciones reales definidas, firma válida.
La APK sigue usando el backend GL existente. El archivo estático de la prueba Vulkan no está enlazado en ella.
