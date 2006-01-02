#include "device.h"
#include "partition.h"

int main() 
{
    int disknr = find_iPod();
    if (disknr >= 0)
        printf ("Found iPod at physical disk %d.\n", disknr);
    else
        printf ("iPod not found.\n");
    return 0;
}
