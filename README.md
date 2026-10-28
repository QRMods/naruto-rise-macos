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

`tools/bench.py` reproduce una entrada grabada hasta Konoha y mide los FPS
(`--native` para el renderizador nativo).

## De dónde sale cada parte

- [naruto-rise-recomp](https://github.com/jorjbarbosa/naruto-rise-recomp) (port de PC):
  la recompilación, la aplicación, el renderizador nativo y su cadena de shaders.
- [naruto-ps5](https://github.com/mshivam019/naruto-ps5): su parche del SDK (vigilancia
  de escrituras con páginas de 16 KB, envíos a Vulkan, ritmo de presentación,
  miniaturas de las partidas), el extractor de la ISO y el hilo de carga.
- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) y [Plume](https://github.com/renderbag/plume).

Lo propio de macOS está en `macos/`:

- `main.cpp`, `platform.cpp`: entrada de la app y búsqueda del cargador de Vulkan del paquete.
- `native/`: la capa Win32 del renderizador nativo sobre POSIX, Mach y SDL3.
- `naruto-native-macos.patch`: superficie Metal, lectura segura de memoria y FPS en el log.
- `rexglue-*.patch`: cambios del SDK (ventana SDL, presentador Vulkan, carga de módulos).

## Pendiente

- El guardado y la carga no se han verificado de principio a fin: al empezar la historia
  el juego no permite guardar todavía. El juego sí crea correctamente sus ranuras de
  guardado en `user/`.
- La primera vez que aparece una combinación de shaders nueva puede haber un tirón breve.
