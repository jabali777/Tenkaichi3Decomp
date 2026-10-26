#!/bin/sh
# Menu liviano: conserva los lanzadores directos para scripts y diagnosticos.
root=$(CDPATH= cd "$(dirname "$0")" && pwd -P) || exit 1

if [ ! -t 0 ] || [ ! -t 1 ]; then
    if [ "${BT3_MENU_EN_TERMINAL:-0}" = 1 ]; then
        printf '%s\n' 'Could not open the menu in an interactive terminal.' >&2
        exit 1
    fi
    if ! command -v xfce4-terminal >/dev/null 2>&1; then
        printf 'Open a terminal and run: "%s/play.sh"\n' "$root" >&2
        exit 1
    fi
    export BT3_MENU_EN_TERMINAL=1
    exec xfce4-terminal --disable-server --title='Tenkaichi 3 - Start' \
        --execute "$root/play.sh" "$@"
fi

meter=1
while :; do
    if [ "$meter" = 1 ]; then
        meter_label=enabled
    else
        meter_label=hidden
    fi
    printf '\n%s\n' 'TENKAICHI 3 — START MENU'
    printf '%s\n' \
        'All profiles use a 640×480 window. Avoid maximizing it.' \
        '' \
        '1) Maximum smoothness (recommended): 256×224, more pixelated image.' \
        '2) Sharper image: 512×448, simplified shading and no shadows.' \
        '3) With shadows: 512×448, simplified shading and shadows.' \
        '4) Original shading: 512×448, retains other performance settings.'
    printf '5) Show/hide FPS (currently: %s).\n' "$meter_label"
    printf '%s\n' \
        '6) Diagnose slowdowns: fast profile, save copies, and visible FPS.' \
        '0) Exit.'
    printf '\nChoose an option and press Enter: '
    IFS= read -r choice || exit 0
    case "$choice" in
        1) lowres=1; shading=2 ;;
        2) lowres=0; shading=2 ;;
        3) lowres=0; shading=1 ;;
        4) lowres=0; shading=0 ;;
        5) meter=$((1 - meter)); continue ;;
        6) lowres=1; shading=2 ;;
        0) exit 0 ;;
        *) printf '\n%s\n' 'Invalid option. Choose a number from 0 to 6.'; continue ;;
    esac

    # El menu elige estos valores aunque la terminal tenga otro perfil heredado.
    export BT3_OPTIMIZACION_GL=1
    export BT3_BAJA_RESOLUCION="$lowres"
    export BT3_SOMBREADO_SIMPLE="$shading"
    export BT3_METER="$meter"
    if [ "$choice" = 6 ]; then
        exec "$root/play-diagnostics.sh" "$@"
    fi
    exec "$root/play-fast.sh" "$@"
done
