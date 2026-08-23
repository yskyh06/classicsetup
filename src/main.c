#include <stdlib.h>

#include "classicsetup/welcome.h"

int main(void)
{
    enum classicsetup_welcome_result result = classicsetup_show_welcome();

    switch (result) {
    case CLASSICSETUP_WELCOME_CONTINUE:
    case CLASSICSETUP_WELCOME_QUIT:
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}
