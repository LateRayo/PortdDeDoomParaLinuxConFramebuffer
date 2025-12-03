
#include <stdio.h>
#include <stdlib.h>

#include "doomgeneric.h"

//DG_GetKey
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>


//DG_GetTicksMs
#include <time.h>
#include <stdint.h>

#include <fcntl.h>      // open(), O_RDWR, etc.
#include <unistd.h>     // close(), read(), write(), lseek(), etc.
#include <sys/ioctl.h>  // ioctl() para configurar dispositivos
#include <linux/fb.h>   // estructuras como fb_var_screeninfo y fb_fix_screeninfo
#include <sys/mman.h>   // mmap() y munmap() para mapear memoria

#include "doomgeneric.h"

static int fb_fd = -1;
static struct fb_var_screeninfo vinfo; 
//smem_start: dirección de memoria física del framebuffer.
//line_length: cantidad de bytes por fila de píxeles.
//smem_len: tamaño total del framebuffer.
//xoffset, yoffset: desplazamiento de la ventana virtual.
static struct fb_fix_screeninfo finfo;
//xres, yres: resolución visible (ancho y alto).
//bits_per_pixel: profundidad de color.
//red, green, blue: máscaras de color.
static uint8_t* framebuffer = NULL;

//DG_GetKey
static struct termios oldt, newt;
static int initialized = 0;
//oldt :guarda la configuración original del terminal para poder restaurarla después (idealmente).
//newt :será la configuración que cambiaremos para poner el terminal en modo raw.
//initia :lized evita repetir la configuración varias veces.

// prepara el buffer para ser usado
void DG_Init() 
{
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        perror("Error: no se pudo abrir /dev/fb0");
        exit(1);
    }
    //llena el buffer finfo
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) == -1) 
    {
        perror("Error: ioctl FSCREENINFO");
        exit(1);
    }
    //llena el buffer vinfo
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) == -1) 
    {
        perror("Error: ioctl VSCREENINFO");
        exit(1);
    }

    size_t screensize = vinfo.yres_virtual * finfo.line_length; //tamaño en bytes del framebuffer

    framebuffer = (uint8_t*) mmap(NULL, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    //Nos permite acceder a la memoria de video directamente desde el código, como si fuera un array.
    //Es mucho más rápido que usar read() o write().
    //Una vez mapeado, podés escribir en fbp[...] y eso va directo a la pantalla.
    if (framebuffer == MAP_FAILED) 
    {
        perror("Error: mmap framebuffer");
        exit(1);
    }

    // Si querés, podés imprimir info útil acá (debug):
    printf("Framebuffer: %dx%d, %dbpp, line_length: %d\n",
        vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, finfo.line_length);
}


void DG_DrawFrame()
{
    if (!framebuffer) return;

    for (unsigned int y = 0; y < vinfo.yres && y < DOOMGENERIC_RESY; y++) 
    {
        for (unsigned int x = 0; x < vinfo.xres && x < DOOMGENERIC_RESX; x++) 
        {
            long location = (x + vinfo.xoffset) * (vinfo.bits_per_pixel / 8) + (y + vinfo.yoffset) * finfo.line_length;

            pixel_t pixel = DG_ScreenBuffer[y * DOOMGENERIC_RESX + x]; // entrego la informacion de un pixel

            if (vinfo.bits_per_pixel == 32) 
            {
                *((uint32_t*)(framebuffer + location)) = pixel;
            } 
            else if (vinfo.bits_per_pixel == 16) 
            {
                // Convert RGBA8888 to RGB565
                uint8_t r = (pixel >> 16) & 0xFF;
                uint8_t g = (pixel >> 8) & 0xFF;
                uint8_t b = pixel & 0xFF;
                uint16_t rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                *((uint16_t*)(framebuffer + location)) = rgb565;
            }
        }
    }
}

//implementacion tipica para linux
void DG_SleepMs(uint32_t ms)
{
    usleep(ms * 1000); 
}



uint32_t DG_GetTicksMs() 
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    
    // Convertimos segundos + nanosegundos a milisegundos
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}



void disableRawMode() 
{
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

// para cierres abruptos
void handleSignal(int sig) {
    disableRawMode();
    printf("\nPrograma terminado por señal %d\n", sig);
    exit(1);
}

void enableRawMode() 
{
    if (initialized) return;  // Solo configurar una vez

    // Guardar configuración actual del terminal
    if (tcgetattr(STDIN_FILENO, &oldt) == -1) 
    {
        perror("tcgetattr");
        exit(1);
    }
    newt = oldt;

    // Quitar modo canónico y eco
    newt.c_lflag &= ~(ICANON | ECHO);

    // Aplicar configuración nueva
    if (tcsetattr(STDIN_FILENO, TCSANOW, &newt) == -1) {

        perror("tcsetattr");
        exit(1);
    }

    // Configurar stdin para lectura no bloqueante
    if (fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) == -1) {
        perror("fcntl");
        exit(1);
    }

    // Registro para restaurar al salir normalmente
    atexit(disableRawMode);  

    // Registrar manejadores para señales comunes de terminación
    signal(SIGINT, handleSignal);  // Ctrl+C
    signal(SIGTERM, handleSignal); // kill
    signal(SIGQUIT, handleSignal); // Ctrl + '\\' para salir


    initialized = 1;
}

int DG_GetKey(int* pressed, unsigned char* key) 
{
    enableRawMode();  // Asegurar que el terminal está en modo raw

    char ch;
    ssize_t n = read(STDIN_FILENO, &ch, 1);  // Leer 1 byte de stdin

    if (n == 1) {  // Si se leyó algo
        *pressed = 1;  // Indicamos que hay tecla presionada
        *key = ch;     // Guardamos la tecla
        return 1;      // Retornamos que hay una tecla nueva
    } else {
        *pressed = 0;
        return 0;
    }

    return 0;  // No hay tecla nueva
}

void DG_SetWindowTitle(const char * title)
{
    printf("[DEBUG] Window title: %s\n", title);
}



int main(int argc, char **argv)
{
    doomgeneric_Create(argc, argv);

    for (int i = 0; ; i++)
    {

        doomgeneric_Tick();
    }
    
    return 0;
}