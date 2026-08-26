#include "classicsetup/keymap.h"

int classicsetup_key_is_quit(int key)
{
    return key == 'q' || key == 'Q';
}

int classicsetup_key_is_apply(int key)
{
    return key == 'a' || key == 'A';
}
