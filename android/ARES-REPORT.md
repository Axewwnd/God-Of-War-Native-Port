# PROJECT ARES — registro de investigación, 2026-10-29 UTC

**Resultado actual: APK ARM64 real compilada y verificada localmente. No se ha ejecutado GitHub Actions ni
se ha demostrado que el juego arranque/renderice en Android.**

- Archivo: `ProjectARES-GodOfWar-android-arm64.apk`, 54.038.192 bytes.
- SHA-256: `2a0afa5a3b4a9224d57efb362103a3c65488ea19a3690cac8cb947a1791b13c8`.
- `assembleRelease`: **BUILD SUCCESSFUL**, salida 0, en el cuarto intento con fuentes reales.
- Biblioteca empaquetada ELF64/AArch64, NativeActivity definida, tabla enlazada y **6.415/6.415 funciones
  registradas definidas dentro de esa misma biblioteca**, sin funciones ausentes.
- `apksigner verify`: válido, firma v2 con certificado Android Debug. `zipalign` y manifiesto/ABI comprobados.
- La prueba negativa rechaza un runtime AArch64 que sustituye la biblioteca del juego. Fixture temporal eliminado.
- GitHub sigue bloqueado por HTTP 401; el workflow está preparado y pasa `actionlint`, pero no hay run remoto nuevo.
- La APK necesita los datos del disco aparte. El backend GS Android no está implementado ni se ha probado hardware.
  No se declara jugable ni completado el port.

Las evidencias finales están en `ares-evidence/gradle-real-game-04.txt`, `apk-verification.json`,
`apk-signature.txt`, `apk-runtime-rejection-test.txt` y `generated-code-integrity.json`.

## Continuación con el enlace proporcionado por el usuario (16:59 UTC)

Se extrajo por rangos HTTP el ejecutable `SCUS_973.99` del enlace de ISO proporcionado.
Tamaño: 1.911.580 bytes; SHA-256: `ec9397d252f14d4412daffaad6be1f1931769e56222b5dbb9b39a947fc8ad71c`.
`SYSTEM.CNF` identifica `SCUS_973.99`, `VMODE = NTSC`. No se descargaron los PAK ni se considera
completa la extracción del disco. El manifiesto de rangos y los hashes permanecen en la evidencia local.

La herramienta de preparación ejecutó el recompilador real con la configuración USA:
6.414 funciones descubiertas, 6.415 generadas, 6.338 recompiladas, 76 stubs SDK,
0 instrucciones sin manejar, 0 fallos de decodificación, 0 errores y 2.222 advertencias de saltos indirectos.
Los stubs SDK proceden de la configuración original; no sustituyen las funciones del juego.
Las fuentes completas se instalaron localmente y pasan la comprobación de cobertura del registro.
La tabla regenerada coincide exactamente con la tabla del repositorio tras normalizar finales de línea.
No se incorporan las fuentes derivadas del juego al parche de infraestructura.

Se ejecutaron cuatro ensamblados Gradle con las fuentes reales:
El primer intento compiló 200/201 bloques y falló en el override/cabecera manuscritos:
`std::from_chars(double)` no está implementado por la libc++ del NDK, y `advanceQuietRepeated`
pertenece a un scheduler más reciente que esta rama. Se añadió `ares_android_compat.h` y una adaptación
reproducible/idempotente de esos dos archivos manuscritos. No se modifica ninguna implementación EE generada.
El parser usa una locale C privada con consumo completo, validación decimal, ERANGE y finitud; tiene pruebas
para valores válidos, espacios, hexadecimal, NaN/Inf, basura final y desbordamiento/subdesbordamiento.
El ajuste del countdown solo usa el salto masivo cuando el scheduler lo ofrece; en la base no salta iteraciones,
por lo que los bucles reales conservan todos sus checkpoints, avance de timers y avance IOP.
Las pruebas del adaptador pasan en host y compilan para AArch64/API28; el primer test necesitó añadir
`<initializer_list>` a su propio archivo. Se inició un segundo ensamblado incremental sin invalidar los 200 bloques.
El segundo intento terminó de compilar el bloque 201 y alcanzó el enlace, que falló por 15 símbolos VU de PCSX2.
En `VUops.cpp` y `VUflags.cpp` las funciones declaradas extern en headers tenían definiciones inline confinadas
a archivos `.cpp`; Clang no emitía los cuerpos requeridos por las otras unidades. Se retiró `__fi`/`__ri` de esas
15 definiciones públicas, manteniendo sus cuerpos y los helpers estáticos internos.
Un tercer intento intermedio se inició antes de terminar de editar el segundo archivo: recompiló VUops, pero
conservó el objeto antiguo de VUflags y falló por los nueve símbolos restantes. El cuarto intento, con ambos
archivos completos, recompiló VUflags, enlazó la biblioteca del juego y generó/firmó la APK: `BUILD SUCCESSFUL in 16s`.
El primer build completo tardó 19m10s; el segundo incremental 3m43s y el tercero 12s. Se conservan los cuatro logs.
La comparación byte a byte verifica que los 6.415 archivos de implementación generados siguen intactos y que
la tabla coincide tras normalizar saltos de línea. No se ocultaron símbolos sin resolver ni se usaron stubs nuevos.
Finalmente se volvió a ejecutar la preparación automatizada con las correcciones integradas: los 6.421 archivos
de entrada del juego (implementaciones, registro, overrides y cabeceras) coinciden con los usados en el build,
normalizando solo finales de línea. No se requiere una edición manual de fuentes para repetir ese paso.
Los hallazgos iniciales de fuentes ausentes que siguen debajo describen el estado anterior al enlace.
El acceso autenticado a GitHub continúa devolviendo HTTP 401.

## Primera etapa: estado anterior al enlace de ISO

**Resultado histórico de esa primera etapa: bloqueado para compilar el juego completo. Entonces no había APK
del juego ni un run nuevo de GitHub Actions.**
Se compilaron el recompilador host y la biblioteca del runtime Android ARM64. Estos resultados NO se contabilizan
como una compilación de God of War. No se generaron implementaciones falsas ni se reemplazó el registro por uno vacío.

## Base y alcance

- Repositorio solicitado: `https://github.com/Waffle565/God-Of-War-Native-Port.git`, rama `runtime`, base `ba16c8d`.
- `main`, inspeccionado como worktree aparte: `514f891`. Contiene configuración, herramientas y overrides.
- Referencia indicada por esa documentación: `KIexster/god-of-war-recomp`, `2d3c91dc173727058073c267bbfb27b3dab703a1`.
  Solo se consultó; no se modificó su código.
- No se encontraron `AGENTS.md`, `CODEX-TASK.md` ni PROJECT ARES v2 en las dos ramas solicitadas.
  El usuario indicó que desconoce dónde están. No se han inventado esas instrucciones ni declarado aplicado v2.
- La API pública permitió inspeccionar los runs existentes. El run runtime
  <https://github.com/Waffle565/God-Of-War-Native-Port/actions/runs/39164390263>
  muestra fallos de compilación Linux GCC/Clang y Windows MSVC; no tiene job Android.
  La descarga pública de sus logs devolvió HTTP 403, `Must have admin rights to Repository`.
  No se atribuye a esos runs ninguno de los errores locales sin haber leído sus logs.
- La API autenticada mediante `gh api` devuelve **HTTP 401 Bad credentials**. No fue posible subir cambios,
  crear/disparar el workflow ni descargar sus artefactos. `gh auth status` también muestra credencial inválida;
  no se debe inferir autorización solo de su código de salida observado.

## Errores, correcciones y comprobaciones

| Prueba / evidencia en `ares-evidence/` | Resultado y acción |
|---|---|
| `preflight.txt` | Entorno inicial sin repositorio ni CMake. Se clonó runtime recursivamente, se instaló CMake 3.31.6/Ninja, NDK r28c, SDK 34/build-tools 34, CMake SDK 3.22.1 y Gradle 8.9. Java local 21; workflow fija Java 17. |
| `android-configure-01.txt` | CMake cruzado ARM64/API28 configura con NDK r28c. Descarga raylib completa; se añade shallow fetch para futuras ejecuciones. |
| `android-runtime-01.txt` | Falla en `gs_threaded_backend.cpp`: `xmmintrin.h` solo admite x86/x64. Se usan inclusiones `sse2neon.h` bajo `USE_SSE2NEON` en ese archivo, `ps2_runtime.cpp`, `ps2_vu1_native.h` y `ps2_vu1_compiled.inl`. |
| `android-runtime-02.txt` | Falla `static_assert(kPairs == std::size(ns.slots))`: referencia a parámetro no constante en C++20/Clang. Se usa el tamaño de tipo vía `sizeof(ns.slots) / sizeof(ns.slots[0])`. Se conserva la comprobación. |
| `android-runtime-03.txt` | Falla `ee_ref/common/BitUtils.h`: no encuentra `common/Pcsx2Defs.h`. Se cambia a `Pcsx2Defs.h`, adyacente al header. |
| `android-runtime-04.txt`, `android-runtime-final.txt` | Compilación de `ps2_runtime` finaliza con código 0. Se generan `libps2_runtime.a`, `libps2_iop.a`, libmpeg2 y raylib. Persisten warnings de enumeraciones y `u8path` obsoleto. |
| `runtime-object-elf.txt` | Objeto del runtime: ELF64, relocatable, máquina AArch64. No es una biblioteca compartida de juego ni una APK. |
| `android-runner-01.txt` | Compilación del runner, con el control de fuentes desactivado SOLO para diagnóstico: falla porque falta `ps2_recompiled_functions.h`. No se sustituye por un header vacío. |
| `game-sources.json`, `game-sources.txt` | Faltan dos cabeceras y 6.415 símbolos de funciones EE referenciados por el registro. El JSON contiene la lista completa. |
| `android-source-guard.txt` | Configuración Android normal rechaza las fuentes ausentes antes de descargar dependencias. |
| `gradle-tasks-01.txt` | Sin configurar el proxy de Java, Gradle no resuelve el plugin `com.android.application:8.6.1`. |
| `gradle-tasks-02.txt` | Con el proxy HTTP/HTTPS del entorno configurado como propiedades Java: `BUILD SUCCESSFUL`, tareas Android disponibles. No se cambió AGP para ocultar un problema de red. |
| `gradle-wrapper.txt`, `gradle-wrapper-02.txt`, `gradle-wrapper-03.txt` | Generación inicial en directorio sin settings falla; segundo intento falla validando URL sin proxy. Se crea settings temporal y genera offline; la distribución descargada se verifica con SHA-256 oficial. Se añaden wrapper real y checksum. El intento posterior usa ese wrapper y descarga/valida Gradle. |
| `gradle-assemble-01.txt` | `./gradlew assembleRelease --stacktrace --info`, SDK 34, CMake SDK 3.22.1 y NDK r28c: alcanza `configureCMakeRelWithDebInfo[arm64-v8a]` y falla por la cabecera real ausente. No produce APK. |
| `host-configure.txt`, `host-build.txt` | Compilación nativa Linux de `ps2_recomp`, sin runtime/studio/tests/analyzer, finaliza con código 0. La herramienta ejecuta su mensaje de uso; no se ejecutó sobre un ELF del juego. |
| `source-guard-tests-final.txt` | 3 pruebas pasan: checkout sin fuentes, solo tabla/cabeceras, y conjunto parcial de funciones deben rechazarse. Los fixtures son temporales; nunca se copian al runtime ni a una APK. |
| `prepare-missing-elf.txt` | La herramienta de preparación rechaza un ELF ausente y no produce fuentes. Su generación completa sigue sin probar por falta del original. |
| `actionlint-final.txt`, `diff-check-final.txt` | `actionlint 1.7.7` y `git diff --check` terminan con código 0. No prueban ejecución remota. |

Otros ajustes: PIC para bibliotecas estáticas enlazadas en NativeActivity; ABI única ARM64; ruta de arranque
`SCUS_973.99`; se elimina la selección automática de crypto/CRC basada solo en que el compilador acepte flags
(eso no prueba que todos los dispositivos los admitan). No se modifica la lógica del juego para superar el build.

## Workflow preparado y límites

`.github/workflows/ares-android.yml` admite ejecución manual, exige un artifact de fuentes completas,
valida cobertura del registro, conserva hashes, fija toolchain, compila y comprueba ELF AArch64,
símbolo NativeActivity, tabla recompiled, firma y SHA-256 de la APK. Los logs se suben incluso ante fallos.
El artifact de entrada aún no existe en esta sesión. La validación del YAML pasa; el workflow no se ha ejecutado.

Además de las fuentes, persiste un bloqueo de ejecución: `GSGL::Context::Create` solo tiene implementación WGL
en Windows y falla explícitamente en Android. El backend GS necesita adaptación EGL/GLES y verificación de shaders
y capacidades; el raylib ES2 que configura CMake no resuelve esa capa. La ruta VU1 AOT también está excluida
en Android. No se declara preparación completa ni jugabilidad.

## Entrega y continuación

Consultar [ARES.md](ARES.md) para las rutas exactas de fuentes, configuración USA y datos del disco necesarios.
`tools/ares/prepare_game_sources.py` está preparado para generar un bundle local desde el ELF original con
el recompilador construido. Primero se necesitan `SCUS_973.99` y una conexión válida con permisos GitHub.
Después: generar/sincronizar fuentes, compilar/enlazar el juego, resolver los errores que ese código revele,
implementar/probar GS Android, ejecutar Actions y conservar URL/run SHA/APK SHA-256/logs; finalmente probar
arranque, renderizado, controles y carga en un dispositivo ARM64 con la extracción completa del disco.
Nada de esa verificación del juego se ha sustituido por el éxito parcial del runtime.

Los SHA-256 de los binarios parciales y los códigos de comprobación están en `ares-evidence/build-sha256.txt`
y `ares-evidence/final-checks.json`. Los binarios permanecen locales fuera del repositorio.
