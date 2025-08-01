# Entendiendo DoomGeneric y otras cosas

## Que significan los prefijos en los fichero:

| Prefijo | Significado                                                             | Ejemplo(s)                             | Comentario                                                                 |
| ------- | ----------------------------------------------------------------------- | -------------------------------------- | -------------------------------------------------------------------------- |
| `i_`    | *"Implementation" o "Interface"* de bajo nivel (plataforma dependiente) | `i_sound.c`, `i_video.c`, `i_system.c` | Encapsulan la comunicación con el sistema operativo o el hardware          |
| `s_`    | *"Sound"*                                                               | `s_sound.c`                            | Lógica de manejo de sonido a nivel general, independiente de plataforma    |
| `m_`    | *"Miscellaneous"* o *"Memory"*                                          | `m_misc.c`, `m_fixed.c`, `m_alloc.c`   | Utilidades generales o funciones matemáticas                               |
| `p_`    | *"Player"* o *"Physics"*                                                | `p_user.c`, `p_map.c`, `p_tick.c`      | Lógica del jugador o del mundo                                             |
| `g_`    | *"Game"*                                                                | `g_game.c`                             | Control general del flujo de juego (iniciar partida, cargar niveles, etc.) |
| `f_`    | *"Finale"*                                                              | `f_finale.c`                           | Manejo de las escenas finales o intermedias                                |
| `d_`    | *"Data"* o *"Definitions"*                                              | `d_main.c`, `d_items.c`                | Datos compartidos o inicialización global                                  |
| `r_`    | *"Renderer"*                                                            | `r_draw.c`, `r_main.c`, `r_bsp.c`      | Todo lo que tiene que ver con gráficos y renderizado                       |
| `v_`    | *"Video"*                                                               | `v_video.c`                            | A veces lo usan en lugar de `i_video.c`                                    |
| `hu_`   | *"Heads Up display"*                                                    | `hu_stuff.c`                           | HUD, textos en pantalla                                                    |
| `st_`   | *"Status bar"*                                                          | `st_stuff.c`                           | Lógica de la barra de estado (vida, balas, etc.)                           |
| `am_`   | *"Automap"*                                                             | `am_map.c`                             | Código del automapa                                                        |
| `wi_`   | *"WInscreen"* o *"WIpe"*                                                | `wi_stuff.c`                           | Pantalla de puntuación entre niveles                                       |
| `z_`    | *"Zone memory management"*                                              | `z_zone.c`                             | Sistema de gestión de memoria dinámica                                     |

## Archivos principales `doomgeneric.c`y`doomgeneric.h`

 Estos son el núcleo del "port genérico". Acá es donde se abstraen las funciones específicas del sistema operativo (dibujado, entrada, tiempo, etc.).  

Hay que trabajar principalmente sobre estos si querés hacer un port para otra plataforma (como Kindle).

### Estructura del doomgeneric.c

Estos nombres **no son parte del Doom original**, sino que son funciones que DoomGeneric espera que vos implementes para cada plataforma.

```c
//Implement below functions for your platform
void DG_Init();
void DG_DrawFrame();
void DG_SleepMs(uint32_t ms);
uint32_t DG_GetTicksMs();
int DG_GetKey(int* pressed, unsigned char* key);
void DG_SetWindowTitle(const char * title);
```

### Impleplementacion para linux con framebuffer.

#### ¿Qué es el *framebuffer*?

**Es un bloque de memoria que representa lo que se va a mostrar en la pantalla.**

Cada píxel que ves en tu monitor está representado por un conjunto de bits en ese framebuffer.

En sistemas como Linux, suele estar mapeado en el dispositivo `/dev/fb0`. Este archivo representa la memoria de video directamente. Cuando escribís en él, cambiás los píxeles que se ven en pantalla.

Cuando usás el framebuffer directamente en Linux (sin X11, ni Wayland, ni nada gráfico), estás escribiendo **directamente sobre la pantalla** (como si fuera una terminal de texto en modo gráfico). No hay ventanas, ni mouse, ni decoraciones, ni títulos. Todo el sistema gráfico lo manejás vos, como si fueras el único programa dibujando.

Para encontrar un píxel en la posición (x, y), usás la fórmula:

```c
offset = y * pitch + x * bytes_por_pixel
```

Donde:

- `pitch` = número de bytes por fila (a veces mayor que `ancho * bytes_por_pixel` por padding)

- `bytes_por_pixel` = 1, 2, 3 o 4 dependiendo del modo de color

Cuando *mapeás* un archivo o un dispositivo a memoria con `mmap()`, lo que hacés es **crear una región en la memoria de tu programa que representa directamente el contenido de ese archivo o dispositivo**.

Así, podés leer y escribir en esa región de memoria como si fuera un array, y esos cambios **afectan directamente el archivo o dispositivo subyacente**.

mapear el `framebuffer` es muchísimo más rápido y práctico que usar `read()` y `write()` constantemente.  Además, te da acceso aleatorio: podés ir directo al byte que querés sin tener que leer todo antes.

```c
int fb = open("/dev/fb0", O_RDWR); 
//mapeo
uint8_t* framebuffer = mmap(NULL, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);`
```

```c
framebuffer[0] = 0x00; // modifica el primer byte del framebuffer`
```

### ¿Qué tiene que hacer `DG_Init()`?

**Paso a paso conceptual:**

1. **Abrir el framebuffer `/dev/fb0`**.

2. **Obtener información sobre la pantalla** usando `ioctl()` (resolución, formato de píxeles, etc.).

3. **Mapear el framebuffer a memoria** con `mmap()` para poder escribir píxeles directamente.

4. (Opcional) Verificar que el formato sea compatible con `pixel_t` (ej: 32 bits RGB o similar).

5. Dejar todo listo para que `DG_DrawFrame()` simplemente copie el buffer.

### ¿Qué tiene que hacer `DG_GetTicksMs()`?

Debe devolver el **tiempo actual en milisegundos** desde que empezó el programa, o desde algún punto de referencia.

Esto lo usa Doom para calcular cuánto tiempo pasó entre frames, controlar animaciones, hacer sincronización, etc.

### ¿Que hace `DG_DrawFrame()`?

Se encarga de copiar el contenido del framebuffer lógico (`dg_screenbuffer`) directamente al framebuffer real (`/dev/fb0` mapeado con `mmap`).

## ¿Que hace `DG_SetWindowTitle()`?

No hace nada en framebuffer puro. Podés dejarlo vacío.

### ¿Que hace `DG_GetKey()`?

Es el encargado de obetner la entrada y guardarla para usarla en `void I_GetEvent(void)`. `DG_GetKey(int* pressed, unsigned char* key)` devuelve un `1` si hay techa, guarda en `key` la tecla en `ascii` y si hay tecla(devuelta) en `pressed`

#### Modo en que se consigio obener la telcanal

Se obtiene a partir de la entrada por terminal, pero hay que hacer configutraciones para que no haya que paretar `entrer` luego de calda tecla.

 **Modo canónico (normal)**

- La terminal **espera hasta que pulses ENTER** antes de enviar los caracteres al programa.

- Eso significa que si escribís "hola", el programa no verá nada hasta que aprietes ENTER.

- Además, la terminal procesa algunos caracteres especiales (como CTRL+C para interrumpir).

**Modo no canónico (raw)**

- La terminal envía **cada carácter inmediatamente cuando se presiona**.

- Así, si apretás 'h', el programa recibe esa 'h' sin esperar al ENTER.

- Además, en raw podés desactivar la visualización automática de las teclas (eco).

- Permite detectar cada pulsación de tecla al instante.

**Porque se usa modo Raw**

Un juego necesita reaccionar **en tiempo real** a las teclas, no esperar que el jugador escriba y presione ENTER. En modo canónico, sería imposible jugar porque el programa se quedaría esperando que pulses ENTER cada vez.

**Cómo funcionan las señales (ej. Ctrl+C) si el terminal está en modo raw?**

- Aunque el modo raw **deshabilita el procesamiento normal de la terminal** (como esperar ENTER o hacer eco), el kernel de Linux sigue reconociendo ciertas combinaciones de teclas especiales como **señales**.

- Por ejemplo, cuando apretás **Ctrl+C**, el terminal genera la señal **SIGINT** para el proceso, **incluso en modo raw**.

- Eso significa que el programa recibe la señal y puede manejarla (o terminar).

### ¿Que hace `DG_SleepMs()`?

`DG_SleepMs` detiene la ejecución del programa durante una cantidad de milisegundos especificada. Es útil para controlar la velocidad del bucle principal del juego y evitar que el programa consuma innecesariamente toda la CPU. Internamente, suele utilizar funciones del sistema operativo como `usleep()` en Linux, que permiten suspender la ejecución por intervalos breves de tiempo.
