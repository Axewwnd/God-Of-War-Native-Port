# PROJECT ARES: Android ARM64

La iteración con Turnip y pantalla de diagnóstico se describe en [ARES-TURNIP-PORT.md](ARES-TURNIP-PORT.md).
Incluye el controlador y la prueba de carga; el puente del renderer Vulkan GS todavía no está implementado.

Estado: **APK Android ARM64 compilada localmente con el código real del juego y verificada**.
El build incluye las 6.415 funciones del registro. La firma de pruebas, la arquitectura y la cobertura del código
empaquetado se comprobaron; el arranque/renderizado no se ha validado y el backend gráfico Android sigue pendiente.
No existe un run nuevo en GitHub Actions: la API autenticada devuelve HTTP 401.
No se encontraron `AGENTS.md`, `CODEX-TASK.md` ni un paquete PROJECT ARES v2 en las ramas `main` y `runtime` examinadas.
Estos cambios parten de `runtime`, no de un supuesto paquete v2.

## Fuentes necesarias para reconstruir

La revisión original `ba16c8d` incluye `ps2xRuntime/src/runner/register_functions.cpp`, pero omite los siguientes
archivos. Ya se generaron/instalaron localmente desde el enlace proporcionado; no se añaden al parche público:

- `ps2xRuntime/include/ps2_recompiled_functions.h`.
- `ps2xRuntime/include/ps2_recompiled_stubs.h`.
- Todas las implementaciones EE generadas para esa tabla: 6.415 nombres `sub_*`/`entry_*` sin definición.
  Obtener la lista completa con `python3 tools/ares/check_game_sources.py --report missing.json`.
  Son archivos `<nombre>_0x<dirección>.cpp`, o `ps2_recompiled_functions.cpp` si se eligió salida combinada;
  el generador puede acortar nombres de archivo. La lista del informe identifica símbolos, no adivina rutas.
- El override del juego de `main:port/overrides-usa/gow_overrides.cpp`, más `gow_pad2_packet.h`
  (repositorio de referencia KIexster, `src/`) y `gow_gs_replay.h` (`main:port/switch/`).
  Colocar los `.cpp` en `ps2xRuntime/src/runner/` y sus cabeceras en `ps2xRuntime/include/`.

Si se entrega una nueva salida del recompilador, reemplazar también la tabla de registro y ambas cabeceras;
no mezclar tablas, configuraciones ni regiones. La tabla aislada no contiene las implementaciones del juego.

Para regenerarlas desde la copia original se necesita el ejecutable **NTSC-U `SCUS_973.99`**.
La configuración USA que la documentación de `main` menciona en `port/usa/` está ignorada y no está publicada ahí.
Su fuente identificada es <https://github.com/KIexster/god-of-war-recomp>:
`config/recomp.template.toml` y `config/funcmap.csv`. Sustituir `@ELF@`, `@MAP@`, `@OUT@` por rutas absolutas
y ejecutar el `ps2_recomp` construido desde esta base. Registrar los commits y SHA-256 del ELF/config/mapa.
Las configuraciones publicadas `port/gow.toml` y `port/gow-kl.toml` usan **PAL `SCES_531.33`**;
no sirven como configuración USA aunque el nombre `kl` pudiera sugerirlo.

Para jugar se necesita además la extracción completa de las dos capas del disco propio:
`SCUS_973.99`, `SYSTEM.CNF`, `IOPRP300.IMG`, `GODOFWAR.TOC`, `PART1.PAK`, `PART2.PAK`,
todos los `.IRX`, `.disc_layout` y `.disc_meta`. Ejecutar `main:tools/iso_extract.py <ISO> <salida> --layout`.
Los nombres y tamaños definitivos de los módulos se obtienen del manifiesto de esa extracción;
no se han comprobado aquí porque no hay disco. No usar los offsets PAL del documento `docs/disc.md` para USA.

Opcionales para las rutas AOT adicionales: `ps2xIOP/src/native/iopn_*.cpp` incluido `iopn_registry.cpp`,
y las capturas VU1 más sus generadores. Sin ellos el runtime usa intérpretes; no equivale al port completamente nativo.
La generación VU1 de este CMake está además deshabilitada en Android. No se ha resuelto esa integración.

## Compilación y Actions

Herramientas fijadas: Gradle 8.9, AGP 8.6.1, JDK 17 en CI, SDK/build-tools 34, NDK 28.2.13676358, CMake 3.22.1.
ABI única: `arm64-v8a`, API mínima 28. La APK release usa la clave debug y es solo para pruebas.

El workflow `.github/workflows/ares-android.yml` se ejecuta manualmente con `source_run_id`:
un run del mismo repositorio con un artifact llamado `ares-game-sources`, que contenga
`include/*.h` y `runner/*.cpp` completos. Es un requisito de entrada, no un artifact que ya exista.
Alternativamente, dejar `source_run_id` vacío y proporcionar `iso_url`, una URL HTTPS de ISO que permita
rangos de bytes. En ese caso el workflow extrae `SCUS_973.99`, compila `ps2_recomp` para el host y genera
las fuentes reales. La configuración y los overrides proceden de commits fijados de `main` y de la referencia USA.
Instala esas fuentes, comprueba su presencia/cobertura, guarda hashes, compila y valida ELF AArch64,
NativeActivity, tabla de funciones y firma de la APK. Sube registros incluso cuando falla.
La comprobación estática no certifica la identidad ni la semántica del código: el enlace y las pruebas del juego
siguen siendo necesarios. Una APK compilada tampoco demuestra que renderiza o que es jugable.

Localmente, después de instalar las fuentes, ejecutar desde `android/`:

```sh
./gradlew assembleRelease --stacktrace --info
```

CMake rechaza por defecto la ausencia de fuentes del juego antes de descargar dependencias.
`-DPS2X_REQUIRE_GAME_SOURCES=OFF` existe exclusivamente para diagnosticar objetivos de bibliotecas del runtime;
no lo utiliza el workflow del juego y su resultado no se presenta como un port compilado.

## Bloqueo gráfico conocido

`ps2xRuntime/src/lib/gs/gpu/gs_gl.cpp` implementa contexto/funciones GPU con WGL en Windows.
En las demás plataformas, `GSGL::Context::Create` devuelve
`GPU GS backend is only implemented for Windows`. `GSThreadedBackend::MakeDefault` selecciona ese backend.
Hace falta implementar el contexto EGL y adaptar/verificar las funciones y shaders del GS para Android.
Raylib usa OpenGL ES2 en la configuración observada; no proporciona por sí sola el backend GS del juego.
No se ha sustituido ese backend por una pantalla vacía ni por funciones de éxito ficticias.

Los registros y resultados concretos de esta sesión se documentan en `ARES-REPORT.md`.

## Preparación desde el ejecutable propio

`tools/ares/prepare_game_sources.py` automatiza la plantilla USA, invoca el recompilador real,
copia la salida y los overrides a un bundle, y registra hashes y commits. Rechaza entradas ausentes,
un ELF que no sea MIPS ELF32 little-endian y una salida ya ocupada. No certifica la revisión por su nombre.

```sh
python3 tools/ares/prepare_game_sources.py \
  --elf /ruta/disc-usa/SCUS_973.99 \
  --recompiler /ruta/build-host/ps2xRecomp/ps2_recomp \
  --main-checkout /ruta/checkout-main \
  --reference-checkout /ruta/god-of-war-recomp \
  --output /ruta/ares-generated
```

La herramienta se comprobó primero con entradas ausentes y después con el ELF extraído del enlace del usuario:
generó 6.415 funciones con 0 errores. `extract_boot_elf.py` también se verificó obteniendo un ELF byte a byte
idéntico en dos extracciones independientes. El registro de compilación posterior está en `ARES-REPORT.md`.

`adapt_game_sources.py` adapta únicamente los overrides manuscritos a la libc++ Android y a la versión del
scheduler. No cambia las funciones EE generadas. `verify_apk.py` comprueba que todas las funciones del registro
están definidas en la biblioteca que realmente está dentro de la APK. `test_apk_verifier.py` demuestra que sustituir
esa biblioteca por un runtime ARM64 sin el juego se rechaza.

## Uso de la APK de pruebas

La APK contiene el código nativo recompilado; los datos de las dos capas del disco se proporcionan aparte.
El ELF y todos los archivos extraídos, incluidos `.disc_layout` y `.disc_meta`, deben ir en
`/storage/emulated/0/Android/data/com.ps2x.runner/files/`. Requiere Android 9/API28 o posterior y ARM64.
No se ha realizado una instalación ni una prueba de juego en hardware. El bloqueo GS descrito arriba sigue vigente:
esta entrega demuestra compilación y enlace del juego, no una versión jugable.
