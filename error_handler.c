#include <stdio.h>
#include <stdlib.h>
#include "error_handler.h"

void handle_error(char *msg)
{
    printf("%s", msg);
    exit(1);
}
