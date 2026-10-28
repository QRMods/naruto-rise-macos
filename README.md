# Naruto: Rise of a Ninja para macOS (Apple Silicon)

Port nativo del juego de Xbox 360 a Macs con chip Apple. El código PowerPC del juego
se recompila estáticamente a ARM64 y los gráficos se dibujan con el renderizador
nativo del port de PC: intercepta las llamadas Direct3D del juego y las ejecuta en
Vulkan sobre Metal (MoltenVK). La emulación de la GPU Xenos queda como alternativa.

Necesitas tu propia copia del juego (imagen ISO de Xbox 360). Ni el juego ni el
código generado a partir de él se distribuyen con este proyecto.

## Requisitos

- Mac con Apple Silicon y macOS 26 o posterior.
- Xcode, CMake, Git y Python 3.
- La ISO del juego en la carpeta `Naruto - Rise of a Ninja/`, o indicada con `--iso`.

## Compilar y jugar

```bash
./Build.command
```

Equivale a `python3 tools/build.py`; acepta `--iso /ruta/al/juego.iso` y `--jobs N`.
Descarga las fuentes en las revisiones fijadas en `sources.json`, aplica los parches,
extrae el juego en `game/`, genera el código recompilado, traduce los shaders del
juego (XenosRecomp, DXC, SPIR-V) y crea `dist/Naruto Rise of a Ninja.app`.

```bash
./Play.command
```

Las partidas guardadas, la caché y los logs se quedan en `user/`.

## Controles (teclado y ratón)

| Mando | Teclas |
| --- | --- |
| Stick izquierdo | W A S D |
| Cámara | Ratón o teclado numérico 8 4 2 6 |
| A / B / X / Y | Espacio / E / J o clic izquierdo / K o clic derecho |
| LB / RB | Q / R |
| LT / RT | Control / Mayúsculas |
| Start / Back | Intro o Esc / Tab |
| Cruceta | Flechas o 1 2 3 4 |

Un mando compatible con SDL también funciona. Las teclas se cambian en
`user/narutorise.toml` (`keybind_*`).

## Configuración

`user/narutorise.toml` (se crea en la primera compilación a partir de `macos/narutorise.toml`):

- `renderer = "native"` (por defecto) o `"xenos"` (la GPU emulada, si algo se ve mal).
- `frame_rate = 30`: el mundo abierto a 30 como en consola; los combates van siempre a 60.
  De 60 a 240 se interpolan fotogramas, pero en un M4 el pueblo se queda en unos 40.
- `msaa_samples`: 0 es el 4× del juego; 2, 4 u 8.
- `fullscreen`, `vsync`, `brightness`, `internal_resolution_scale` (1.5 = 1080p).

Con `REX_NATIVE_FPS_TITLE=1` en el entorno, el título de la ventana muestra los FPS.

## Rendimiento (MacBook con M4)

| Escena | Renderizador nativo | Xenos (emulación) |
| --- | --- | --- |
| Konoha (mundo abierto) | 30 FPS estables | 12-15 FPS |
| Combate | 59 FPS | 16-20 FPS |

### Comprobar caídas de FPS sin jugar

```bash
python3 tools/scenarios.py --cold
```

Recorre escenas solo, sin que nadie juegue: Konoha, la partida guardada (con la entrada
grabada en `user/replays/user-route.nrinput`) y los escenarios del Modo Lucha uno a uno,
con la entrada generada por el propio script. Mide cada escena y marca como fallo la que
baje de su objetivo (30 FPS en el mundo, 60 en combate) o tenga un fotograma de más del
doble de lo previsto. Juega sobre una copia de `user/`, así que no toca tus partidas.
`--cold` empieza sin la caché de pipelines, como la primera vez que se juega;
`--only konoha fight-stage-3` repite escenas concretas. Para medir otra ruta, grábala con
`REX_NATIVE_INPUT_RECORD=archivo.nrinput` al jugar.

## De dónde sale cada parte

- [naruto-rise-recomp](https://github.com/jorjbarbosa/naruto-rise-recomp) (port de PC):
  la recompilación, la aplicación, el renderizador nativo y su cadena de shaders.
- [naruto-ps5](https://github.com/mshivam019/naruto-ps5): su parche del SDK (vigilancia
  de escrituras con páginas de 16 KB, envíos a Vulkan, ritmo de presentación,
  miniaturas de las partidas), el extractor de la ISO y el hilo de carga.
- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) y [Plume](https://github.com/renderbag/plume).

Lo propio de macOS está en `macos/`:

- `main.cpp`, `platform.cpp`: entrada de la app y búsqueda del cargador de Vulkan del paquete.
- `native/`: la capa Win32 del renderizador nativo sobre POSIX, Mach y SDL3, la caché de
  pipelines en disco y la copia protegida de la memoria del juego.
- `naruto-native-macos.patch`: superficie Metal, lectura de memoria, omisión de pipelines
  en Vulkan, y FPS y fotogramas lentos (`[native-hitch]`) en el log.
- `rexglue-*.patch`: cambios del SDK (ventana SDL, presentador Vulkan, carga de módulos).

## Shaders nuevos sin tirones

Cada combinación de shaders nueva se traduce a Metal y se compila al aparecer, lo que
en un Mac cuesta de decenas a cientos de milisegundos. En macOS:

- El objeto cuyo pipeline aún no está listo se omite durante ese instante, en vez de
  congelar el fotograma; se compila en otros hilos y aparece en cuanto termina.
  El backend Vulkan del port de PC nunca omitía: compilaba cada pipeline nuevo en el
  hilo que graba los fotogramas, y cada espera de fence del juego se quedaba detrás
  (tirones de 200 a 900 ms al aparecer contenido nuevo). En macOS omite como el de D3D12.
  `REX_NATIVE_PSO_ASYNC=prefetch` recupera la espera del port de PC.
- Los pipelines ya compilados se guardan en `user/cache/native-vulkan-pipelines.bin`
  y se reutilizan en las siguientes sesiones.

## Lectura de la memoria del juego

El renderizador lee de la memoria del juego los vértices, índices, constantes y fences
de cada draw, miles de veces por fotograma. En PC cada lectura es un `memcpy` bajo
`__try`; en macOS era una llamada al kernel (`mach_vm_read_overwrite`), unas 60 veces
más lenta, así que el coste crecía con lo que dibujaba cada escena. Ahora es un `memcpy`
con un manejador de señales delante del del SDK (`macos/native/guest_fault_guard.cpp`):
una dirección sin memoria hace fallar la copia, como en PC, y la vigilancia de escrituras
del SDK sigue recibiendo sus fallos.

## Pendiente

- El modo de 60 FPS interpolado no se sostiene en el pueblo en un M4 (unos 40).
