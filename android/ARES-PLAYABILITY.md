# Trabajo pendiente para ejecutar God of War en Android

Estado: APK con código real enlazado; arranque, imagen y jugabilidad sin verificar en hardware.
La ausencia del backend gráfico es comprobable en código, pero no identifica por sí sola la causa del cierre observado por el usuario.

## Orden de implementación y criterios de prueba

1. **Diagnóstico del arranque.** Capturar logcat de una apertura con la APK actual y registrar modelo, Android y GPU. Separar errores de carga de biblioteca, ventana, ELF y GS. No inferir el punto del fallo de una pantalla negra.
2. **Datos del disco.** Preparar e importar la extracción completa de las dos capas, no solo `SCUS_973.99`. Validar archivos y metadatos `.disc_layout`/`.disc_meta` antes de arrancar. El extractor de CI obtiene únicamente el ejecutable para generar código: no prepara una instalación jugable.
3. **Contextos y presentación Android.** Implementar EGL en `ps2xRuntime/src/lib/gs/gpu/gs_gl.cpp`, incluyendo contexto de render, contextos de trabajadores, sincronización, transferencia de imagen al contexto de raylib y recreación al perder la superficie. La configuración de raylib observada utiliza ES2; debe coordinarse con la versión ES elegida para GS.
4. **Render GS compatible.** El contexto Windows pide OpenGL 4.6 y `gs_hw_backend.cpp` contiene shaders `#version 430 core`. Auditar funciones/extensiones y portar shaders y recursos a OpenGL ES, determinando requisitos mínimos reales. Cambiar solo la creación del contexto no basta. Verificar texturas, profundidad, mezclado y sincronización con capturas del juego.
5. **Entrada, audio y ciclo de vida.** Conectar un mando para la primera prueba, después controles táctiles. Verificar sonido, pausa/reanudación y recreación de superficie. `ps2_android_runtime.cpp` solo contiene un TODO para controles táctiles.
6. **Prueba de aceptación.** Instalar en ARM64, cargar datos, alcanzar menú, iniciar partida y jugar una sección con imagen, sonido y controles. Conservar logcat, modelo/GPU/Android, revisión de fuentes, SHA de APK y evidencia visual. Solo entonces declarar el hito demostrado.

## Captura de un cierre con ADB

Con el teléfono conectado y depuración USB autorizada:

```sh
adb devices
adb logcat -c
adb shell am force-stop com.ps2x.runner
adb shell am start -n com.ps2x.runner/android.app.NativeActivity
# Esperar a que se produzca el problema y ejecutar:
adb logcat -d -v threadtime > ares-arranque.log
```

El runtime escribe stdout/stderr con la etiqueta `ps2x`. Conservar también mensajes del cargador, AndroidRuntime y señales nativas.
No hay acceso directo a un teléfono del usuario desde este entorno.

## GitHub Actions

El run 40948077241 generó y validó las fuentes reales, pero falló antes de compilar Android:
`setup-android@v3` intentó instalar el paquete retirado `tools` (`Failed to find package 'tools'`).
Se configura `packages: platform-tools`; SDK/NDK/CMake siguen fijados en el paso siguiente.
Prueba local: actionlint. La ejecución remota posterior determina si esta corrección supera el fallo.
La APK publicada inicialmente se compiló localmente; no atribuirla a ese run fallido.

## Dispositivo objetivo comunicado por el usuario

Redmi Note 11 SPES, HyperOS 2 / Android 14, Adreno 610; Vulkan 1.1 según el usuario.
Estas capacidades todavía no se han consultado desde la aplicación. No se ha medido rendimiento.
La ruta inicial a evaluar es EGL/OpenGL ES por la arquitectura actual del renderer; Vulkan requeriría otro backend.

Auditoría inicial del cargador GL: exige `glGetBufferSubData`, `glClearBufferData`, `glBufferStorage`,
`glQueryCounter`, `glGetQueryObjectui64v`, `glClearDepth` y `glTextureBarrier`, entre otras funciones.
Varias no pertenecen al núcleo ES o tienen variantes/extensiones. No exigir funciones de diagnóstico o
presentación Windows innecesarias. En particular, el renderer llama `TextureBarrier` al menos en cinco puntos:
si no existe una extensión equivalente, estudiar copias/alternancia de render targets preservando la semántica,
no sustituir esas barreras por no-ops. Para profundidad, adaptar `ClearDepth` a la API ES adecuada.
Definir los requisitos finales después de auditar shaders, uso real de recursos y extensiones del teléfono.
