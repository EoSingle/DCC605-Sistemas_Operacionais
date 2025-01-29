#include "types.h"
#include "user.h"

int
main(int argc, char *argv[])
{
    int initial_pages = num_pages();

    // Aloca memória adicional
    sbrk(2 * 4096);

    int new_pages = num_pages();
    printf(1, "Páginas antes: %d, Páginas depois: %d\n", initial_pages, new_pages);
    
    exit();
}
