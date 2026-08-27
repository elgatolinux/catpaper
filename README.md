# catpaper

Selector de wallpapers independiente de Quickshell, escrito en **C++ (Qt 6 QML)**.
Corre en cualquier WM (Wayland/X11) y usa la paleta de pywal para su tema.
En Wayland + niri se abre por defecto como **overlay wlr-layer-shell**
(lib `layer-shell-qt`), superpuesto encima de todo y transparente.

## Requisitos

- Qt6 (base, declarative, multimedia, network, widgets)
- CMake + ninja y un compilador C++20
- [pywal] — tema + regeneración de paleta al aplicar
- Un setter de wallpapers: autodetecta `awww` → `swww` → `swaybg` → `feh`/`nitrogen`
- Opcional: `layer-shell-qt` (overlay nativo), `mpvpaper` (videos), `ffmpeg` (thumbs de videos)

## Build

```sh
cmake -B build -GNinja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./catpaper                          # o
cmake --install build --prefix ~/.local   # solo el binario (QML va embebido)
```

Los archivos QML se compilan dentro del binario (`.qrc`), así que `build/catpaper`
es 100 % auto-contenido: podés copiarlo a `~/.local/bin` y funciona sin el
directorio `qml/` (para debugging, `CATPAPER_QML_DIR` → sobreescribe con QML de
disco).

Config: `~/.config/wallpaper_picker/config.conf` (ver `config.conf.example`).

## Uso

Teclado: `←/→` o scroll para navegar · `Enter` aplicar · `Tab` ciclar filtros ·
`Escape` salir del filtro de búsqueda y cerrar. Click en una tarjeta aplica directo.

El botón de carpeta (junto a los filtros) abre un diálogo para elegir el
directorio de wallpapers; se guarda en la config y regenera thumbs/colores.

Al aplicar una imagen:

1. El setter detectado la aplica (multi-monitor solo con awww/swww).
2. `wal -i <img> -q` regenera tu paleta (waybar, gtk, mako...) + `reload_cmd` opcional.
3. La UI se re-anima al nuevo tema automáticamente (escucha
   `~/.cache/wal/colors.json`, soporta formatos flat y pywal16-anidado).

## Búsqueda online (DuckDuckGo)

En el filtro búsqueda: baja las thumbs al momento (QNetworkAccessManager) y la
imagen a resolución completa al aplicar.

## Notas

- Wallpapers con prefijo `000_` se tratan como video (`mpvpaper`).
- `wpaper_cmd` y `wal_cmd` en config permiten forzar comandos propios (`%s` = ruta).
- La versión Python anterior está en `python-legacy/`.
