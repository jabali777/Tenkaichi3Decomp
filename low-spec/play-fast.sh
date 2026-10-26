#!/bin/sh
# Perfil minimo para Pentium B950 + Intel HD Graphics 2000 (SNB GT1).
#
# Que hace distinto de abrirlo normal:
#   - usa OpenGL sobre la GPU Intel identificada por el controlador
#   - reduce a 256x224 la imagen interna mediante el adaptador local
#   - apaga todos los efectos gráficos para priorizar rendimiento
#   - permite saltar dibujos cuando el equipo no llega a tiempo, para mantener la velocidad del juego
#   - usa una ventana pequena y desactiva la interpolacion de cuadros
#   - desactiva vsync y el hilo de Mesa; este ultimo no mejoro la prueba de combate
#   - envia parametros de dibujo en buffers nuevos para evitar esperas del controlador
#
# Se puede tocar todo: los numeros estan comentados abajo.
root=$(CDPATH= cd "$(dirname "$0")" && pwd -P) || exit 1
if [ ! -x "$root/Tenkaichi3Decomp/Tenkaichi3Decomp" ]; then
    printf '%s\n' 'Missing port executable: Tenkaichi3Decomp/Tenkaichi3Decomp.' \
        'Install the Linux x86-64 port as described in README.md.' >&2
    exit 1
fi
cd "$root/Tenkaichi3Decomp" || exit 1

# GPU: evita heredar una seleccion del rasterizador de referencia por software.
export BT3_GS=gpu
export BT3_GPU_API=gl

# Ventana menor que la predeterminada de 1194x896; F11 permite ampliarla.
export BT3_WINDOW=640x480
export BT3_FULLSCREEN=0

# El combate sigue simulándose a 30 Hz; interpolar dibujos permite presentar a 60.
# Es experimental en esta versión del port. BT3_INTERP=0 lo desactiva manualmente.
export BT3_INTERP="${BT3_INTERP:-1}"
export BT3_GS_THREAD=0

# Base del port: 1 = PS2. El adaptador opcional de abajo la reduce a la mitad.
export BT3_SCALE=1

# La prueba grabada en este Pentium empeoro con un trabajador de texturas extra.
export BT3_TEX_THREADS=0

# Priorizar fluidez en ataques: 256x224, con imagen mas pixelada.
# BT3_BAJA_RESOLUCION=0 permite volver a 512x448 conservando las otras mejoras.
export BT3_BAJA_RESOLUCION="${BT3_BAJA_RESOLUCION:-1}"

# Efectos apagados, bits: 1 outline, 2 see-through tint, 4 depth tint,
# 8 glare+glow, 16 distance blur, 32 movement blur, 64 weather.
# 127 apaga los siete efectos.
export BT3_FX_OFF=127
export BT3_GLOW=0
export BT3_GLARE=0

# Perfil de pocos recursos: 2 simplifica sombreado y sombras bajo personajes.
# 1 conserva las sombras; 0 conserva tambien el sombreado original.
export BT3_SOMBREADO_SIMPLE="${BT3_SOMBREADO_SIMPLE:-2}"

# Si un dibujo tarda demasiado, el juego lo omite para no ralentizar la partida.
export BT3_FRAME_SKIP=1

# Mostrar FPS reales y velocidad del juego; BT3_METER=0 permite ocultarlo.
export BT3_METER="${BT3_METER:-1}"

# Esta carpeta no tiene pack de texturas; evita el suavizado de sprites 2D.
export BT3_2D_SMOOTH=0

# Mesa/OpenGL: sin espera de vsync. El hilo extra no dio una mejora apreciable.
export vblank_mode=0
export mesa_glthread=false

# Descomentar para ver en la terminal cuanto tarda cada frame (util para medir):
# export BT3_GS_VERBOSE=1

# Adaptador local de OpenGL. Se puede omitir con BT3_OPTIMIZACION_GL=0.
if [ "${BT3_OPTIMIZACION_GL:-1}" != 0 ]; then
    # LD_PRELOAD separa rutas por espacios y dos puntos, incluso entre comillas.
    case "$root" in
        *[[:space:]:]*)
            printf '%s\n' 'Optimized mode requires a path without spaces or colons.' >&2
            exit 1 ;;
    esac
    adapter="$root/optimization/libbt3_opengl_fast.so"
    lowres="$root/optimization/libbt3_resolution.so"
    if [ ! -r "$adapter" ] || { [ "$BT3_BAJA_RESOLUCION" != 0 ] && [ ! -r "$lowres" ]; }; then
        printf '%s\n' 'Missing adapters. Run: sh optimization/build.sh' >&2
        exit 1
    fi
    if [ -r "$adapter" ]; then
        case ":${LD_PRELOAD:-}:" in
            *":$adapter:"*) ;;
            *) export LD_PRELOAD="$adapter${LD_PRELOAD:+:$LD_PRELOAD}" ;;
        esac
        if [ "$BT3_BAJA_RESOLUCION" != 0 ]; then
            if [ -r "$lowres" ]; then
                case ":${LD_PRELOAD:-}:" in
                    *":$lowres:"*) ;;
                    *) export LD_PRELOAD="$lowres${LD_PRELOAD:+:$LD_PRELOAD}" ;;
                esac
            fi
        fi
    fi
fi

exec ./Tenkaichi3Decomp "$@"
