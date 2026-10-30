# Turnip y ruta al port completo de ARES para Android

## Resultado y alcance

El paquete del usuario se ha incorporado como asset verificado y se ha implementado un cargador rootless
con libadrenotools. La pantalla de inicio permite probar Turnip, compararlo con el controlador del sistema
y guardar un informe JSON desde Android. La selección real se confirma por la identidad del driver y por
la creación de un dispositivo lógico; no se deduce del nombre del archivo.

**El renderizador Vulkan de PS2 todavía no está conectado a ARES. No se ha demostrado arranque ni jugabilidad
en Android.** La APK de esta iteración es una prueba del controlador que conserva el código real del juego;
la interfaz lo indica expresamente. No se presenta el diagnóstico como una partida funcionando.

## Paquete suministrado

- ZIP: `Turnip_v26.0.0_R7_Sysmem.zip`, 3481064 bytes.
- SHA256 ZIP: `8c9fe5e357ddfe4970840cf91e2244b81e4dfbedf627d4f855db9f522ef7dd65`.
- Coincide con el digest del asset publicado por K11MCH1/KIMCHI en
  https://github.com/K11MCH1/AdrenoToolsDrivers/releases/tag/v26.0.0-rc07
- Biblioteca: `vulkan.ad07xx.so`, 17161105 bytes, ELF64 little-endian AArch64, ET_DYN.
- SHA256 biblioteca: `ab879bc6605b81bc790eb3519608a998a681d0c47b2d957ea8219f2e6eaa6082`.
- Metadata: Mesa 26.0.0 R7 Sysmem, minApi 27; anuncia Vulkan 1.4.335.
  La versión utilizable se consulta al loader y al dispositivo; no se fuerza Vulkan 1.4.
- Exporta el módulo HAL `HMI`, no `vkGetInstanceProcAddr`. Un dlopen directo del driver no sustituye al loader.
- Depende de bibliotecas Android, entre ellas libcutils, libhardware, libnativewindow y libsync.
  No es ejecutable ni comprobable como driver de GPU en este contenedor Linux.

Mesa incluye `GPUId(610)` en el archivo de dispositivos de la base publicada (`68fb3a601c63`). Es evidencia
para evaluarlo en el SPES, no una prueba del binario con su kernel/ROM/controlador KGSL.
Fuentes: https://docs.mesa3d.org/drivers/freedreno.html y
https://gitlab.freedesktop.org/mesa/mesa/-/blob/68fb3a601c63/src/freedreno/common/freedreno_devices.py

## Integración del cargador

Se fija libadrenotools en `8fae8ce254dfc1344527e05301e43f37dea2df80` y su linkernsbypass en
`aa3975893d83ef1bc84c321ec60c65fbf1287887`.
Referencia de API: https://github.com/bylaws/libadrenotools/blob/8fae8ce254dfc1344527e05301e43f37dea2df80/include/adrenotools/driver.h

1. `prepare_turnip.py` verifica ZIP, metadata, arquitectura y hash antes de producir assets. No extrae rutas arbitrarias.
2. Gradle rechaza un driver ausente o con hash diferente y usa `useLegacyPackaging true`.
3. Se empaquetan los hooks ARM64 `libmain_hook.so` y `libhook_impl.so`; las licencias acompañan al paquete.
4. El driver se copia a `ANativeActivity.internalDataPath`, no a almacenamiento compartido.
5. `nativeLibraryDir` se obtiene con JNI de ApplicationInfo; no se inventa una ruta `/data/app/...`.
6. adrenotools abre una copia aislada del loader. Se conserva su handle durante la vida del proceso.
7. El probe pide una instancia Vulkan 1.1, enumera GPU, extensiones, identidad, límites relevantes y R32_UINT;
   intenta crear un dispositivo con cola gráfica/compute y `fragmentStoresAndAtomics`.
8. Un handle no basta para declarar éxito: si el dispositivo no se identifica como Mesa Turnip, se rechaza la
   atribución a Turnip. El fallback del sistema queda explícito y conserva el error anterior.
9. El informe diferencia carga/consulta del driver, presentación y renderizado GS; estos dos últimos quedan sin verificar.

El futuro backend debe obtener todos sus entry points de `aresAndroidVulkanLibrary()`. Si abre por su cuenta
`libvulkan.so`, puede volver al controlador del sistema y hacer que la prueba de Turnip sea irrelevante para el juego.

## Prueba sin PC en el Redmi Note 11

Abrir ARES, pulsar **Probar Turnip incluido**, esperar a volver a la pantalla y pulsar **Guardar informe**.
Repetir con el sistema si Turnip falla, guardando cada informe con un nombre distinto.
El JSON exportado incluye modelo/Android, resultado de los intentos y el registro de arranque. No recopila
identificadores de cuenta ni números de serie. No exige root ni reemplaza el driver global del teléfono.

Cada prueba vive en el proceso `:runtime`; la pantalla permanece en el proceso principal. Se elimina el informe
anterior y se deja un informe parcial antes de llamar al driver, para que un cierre no parezca un éxito antiguo.
La salida de una prueba termina el proceso nativo, evitando reutilizar namespaces de distintos controladores
u otro hilo de redirección de stdout en la siguiente prueba.

## Investigación del renderer PS2: qué se ha demostrado

Aurora de encounter implementa GX de GameCube/Wii sobre Dawn. No implementa GS de PS2 ni la interfaz
GSRasterBackend de ARES. Enlazarlo sin puente no dibuja este juego.

Se seleccionó para evaluación el GS Vulkan móvil de Play!, revisión
`1f27232772e484bfb79364b751b84f41f4c87481`:
https://github.com/jpd002/Play-/tree/1f27232772e484bfb79364b751b84f41f4c87481/Source/gs/GSH_Vulkan

- Android selecciona MOBILE y una instancia Vulkan 1.0; la variante desktop pide 1.2 y características distintas.
- La variante móvil evita exigir fragment shader interlock y los int8/int16 de la variante desktop.
- Usa attachments R32_UINT, subpasses, almacenamiento GS y shaders generados mediante Nuanceur/SPIR-V.
- Primero compilaron sus 14 unidades principales para Android ARM64.
- Después se **enlazó una biblioteca compartida AArch64 con `--no-undefined`**, el renderer real, CGSHandler,
  Framework, Nuanceur, mailbox y auxiliares de estado/INTC. No incorpora una CPU EE/IOP de Play!.
- El primer enlace ampliado falló en compilación por `Jitter.h`: FrameDump incluye tipos de MIPS. Se añadieron
  el submódulo CodeGen fijado y su include; no se enlazó un ejecutor de CPU ni se sustituyó nada por stubs.
- Hay 75 símbolos definidos de CGSH_Vulkan en la biblioteca de comprobación. El hash y ELF están en la evidencia.
- Este experimento es de compilación y enlace: **no ejecutó el renderer en GPU y no está dentro de la APK**.

Receta reproducible: `tools/ares/play-vulkan-link-check/`. Inicializar Dependencies, Framework, Nuanceur y CodeGen,
y ghc_filesystem/zstd dentro de Dependencies. Configurar para Android ARM64 con NDK r28c y `PLAY_ROOT`.

## Puente pendiente y decisión de arquitectura

Conservar el EE recompilado y los sistemas de ARES. Reutilizar únicamente el GS; arrancar Play! completo cambiaría
el proyecto por un emulador. La tabla indica trabajo de adaptación, no funcionalidad ya implementada.

| Contrato de ARES | Entrada candidata de Play! | Trabajo que no puede omitirse |
|---|---|---|
| Initialize/Reset | Initialize/Reset de CGSHandler | Propiedad de VRAM y configuración privada; estado inicial CPU/GPU coherente |
| Submit | WriteRegister/ProcessWriteBuffer | Codificar contexto, PRIM, RGBAQ, ST/UV, XYZ, fog y z; expandir strips/fans que ARES ya ensambló |
| LoadClut/TextureFlush | TEX0/TEXCLUT/TEXFLUSH | Respetar CLD/CBP0/CBP1; no recargar CLUT por accidente al repetir estado de cada draw |
| BeginTransfer/UploadImage | BITBLTBUF/TRXPOS/TRXREG/TRXDIR, FeedImageData | Unidades de puntero, PSM, direcciones y tamaños exactos; transferencia completa y parcial |
| Local-to-host/ReadVram | ReadImageData y sincronización de memoria | Leer resultados reales de GPU antes de devolver bytes al juego |
| WriteVram/SnapshotVram | Memoria cacheada más subida/lectura GPU | Un único dueño del estado; sin perder escrituras directas de overrides nativos |
| Flush/Sync/Finish | Mailbox, command buffer y fences Vulkan | Esperar al GPU cuando corresponda; Finish de CGSHandler no equivale por sí solo a vkQueueWaitIdle |
| Present | PMODE/SMODE2/DISPFB/DISPLAY y salida Vulkan | Componer los dos circuitos, recortar/corregir formato y conservar el origen preferido de ARES |
| ClearFramebuffer | Dibujo/transferencia real respetando máscaras | No sustituirlo por éxito ficticio o un memset que ignore formato/profundidad |

La primera ruta de presentación a evaluar es render Vulkan **offscreen** y lectura de imagen hacia la presentación
RAM que ARES ya soporta. Evita hacer competir EGL/raylib y Vulkan por el mismo ANativeWindow. Tiene coste de
lectura/subida y no es una solución de rendimiento demostrada. Después de validar imagen, evaluar superficie
Vulkan directa, separando ventana/render de los servicios de entrada/audio que hoy proporciona raylib.

Dos hallazgos impiden conectar el ejemplo offscreen sin más:

- `CGSH_Vulkan::GetScreenshot()` devuelve un bitmap vacío en esta revisión.
- `GetFramebuffer()` sí sincroniza y lee memoria, pero devuelve un framebuffer de altura 1024, no una imagen
  final de pantalla. Hace falta aplicar los registros de display, formatos y composición de ARES.

Además, Framework::Vulkan::CLoader abre su propio loader de sistema. Hay que darle el handle elegido por ARES
antes de crear el primer dispositivo; mezclar entry points de distintas instancias/loaders es incorrecto.
Coordinar los hilos GS: ARES y CGSHandler tienen colas propias. Determinar una única frontera y propagar fallos;
no asumir que una excepción en el worker llega al hilo principal. Las interrupciones siguen siendo de ARES.

## Datos y servicios del juego

El enlace Archive.org permitió extraer el ELF para generar código. **No se descargaron los dos PAK completos.**
La APK no contiene texturas, niveles, sonido o vídeos del disco. El manifiesto observado contiene:

- `SYSTEM.CNF`: 57 bytes.
- `SCUS_973.99`: 1911580 bytes.
- `IOPRP300.IMG`: 278305 bytes.
- `SIO2MAN.IRX`: 6641 bytes.
- `DBCMAN.IRX`: 15653 bytes.
- `SIO2D.IRX`: 11289 bytes.
- `MC2_S1.IRX`: 20165 bytes.
- `DS1O_S1.IRX`: 9437 bytes.
- `MC2_D.IRX`: 20221 bytes.
- `DS2U_D.IRX`: 11725 bytes.
- `LIBSD.IRX`: 28573 bytes.
- `989NOMID.IRX`: 178884 bytes.
- `SMPD_IOP.IRX`: 47948 bytes.
- `GODOFWAR.TOC`: 40068 bytes.
- `PART1.PAK`: 4257794048 bytes.
- `PART2.PAK`: 4261273600 bytes.

Total de archivos: 8521648194 bytes. También se necesitan `.disc_layout` y `.disc_meta` para reconstruir lecturas
por sector y la segunda capa (base 2080544). El script original `tools/iso_extract.py --layout`
del checkout main fijado produce ambos. No basta copiar SCUS_973.99.

Para un producto utilizable en Android 14 hace falta importación mediante Storage Access Framework, con
selección de ISO/directorio, progreso, cancelación, espacio libre y validación final. No depender de copiar
archivos manualmente a Android/data desde un gestor al que Android puede negar acceso. Mantener las partidas
separadas de los datos reimportables; PS2Runtime::configureIoPathsFromElf configura el directorio actual de mc0.

Después de imagen y lectura de disco, verificar audio a 48 kHz y callback Android, entrada de mando (raylib tiene
soporte Android), controles táctiles (todavía TODO), carga/guardado, pausa/reanudación, recreación de superficie,
FMV y sincronización EE/VU/IOP. Los resultados de Windows del proyecto original no certifican ARM64/Android.

## Hitos que permiten declarar resultados concretos

1. **Driver:** el SPES exporta un informe con Turnip confirmado y dispositivo lógico creado. Pendiente de teléfono.
2. **GS aislado:** subir/leer patrones en PSMCT32/24/16, PSMT8/4 y CLUT; comprobar puntos, triángulos, sprites,
   Z, alpha, máscaras y transferencias local-to-local/host. Comparar bytes/capturas con la implementación conocida.
3. **Presentación:** framebuffer del GS visible y correcto; lifecycle probado sin perder datos ni competir por la ventana.
4. **Arranque:** datos completos, menú y partida real; conservar logs con primera causa de cualquier fallo.
5. **Jugabilidad:** imagen, audio, controles y partida guardada/cargada en una sección del juego en el SPES.
6. **Rendimiento:** medir a resolución interna del juego, con perfil y temperatura; optimizar después de verificar corrección.

No se fija una tasa de FPS, una fecha de finalización ni compatibilidad completa a partir de Vulkan 1.1 o del ZIP.

## Pruebas de esta iteración

- Importación exacta del ZIP; rechazo de corrupción, rutas inesperadas, metadata incompatible, ABI errónea y truncación.
- El probe portátil rechazó una biblioteca sin Vulkan y creó instancia/dispositivo real con SwiftShader (Vulkan 1.3)
  en Linux; lo identificó como distinto de Turnip. Esta prueba no valida la carga Android del ZIP.
- Compilación sintáctica de ambos módulos para Android ARM64 API 28.
- Enlace aislado del GS móvil con todas las referencias de proyecto resueltas; fallo y corrección documentados.
- Compilación Gradle, firma, alineación y comprobación del APK se registran en la evidencia de la entrega final.
- `verify_apk.py` exige todas las funciones del juego; `verify_turnip_apk.py` comprueba asset, hooks ARM64,
  launcher Java, proceso runtime y extracción de bibliotecas. Ninguna de esas pruebas certifica jugabilidad.

## Registro adicional de ajustes y herramientas

La primera compilación de integración se interrumpió al detectar que propagar los includes de adrenotools
al runner cambiaba la línea de compilación de todo el juego. El cargador se aisló en una biblioteca estática
privada. El PCH ya había cambiado, por lo que la siguiente compilación reconstruyó las unidades afectadas;
no se forzaron timestamps ni se presentaron objetos sin validar como una nueva compilación.

El sistema tenía JRE 21 pero no javac. Se obtuvo Temurin JDK 17.0.21+9 desde los assets oficiales de
adoptium/temurin17-binaries y se verificó el SHA256 del archivo oficial. La API de adoptium.net respondió
403, por lo que se usó la publicación oficial en GitHub. El launcher pasó compilación javac independiente;
la compilación Gradle final usa ese JDK completo.

El guard de Gradle se probó con el driver exacto (éxito) y con un asset alterado (fallo esperado por hash).
La restauración del asset se hizo siempre, incluso si fallaba la prueba. El manifiesto y el contenido final
se verifican después de compilar: una biblioteca añadida a CMake no prueba que Gradle la empaquetó.

El diagnóstico procesa los eventos de NativeActivity mientras un worker consulta Vulkan. Sin ese bucle,
onStart/onResume pueden quedarse esperando al hilo nativo y causar un ANR durante la inicialización del driver.
Ese bucle se usa solo para la prueba aislada: no consume eventos de ventana antes de que raylib los necesite
para crear su contexto cuando se implemente la ruta del juego.
