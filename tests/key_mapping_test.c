#include <assert.h>
#include <ncurses.h>

#include "classicsetup/keymap.h"

int main(void)
{
    assert(classicsetup_key_is_quit('q'));
    assert(classicsetup_key_is_quit('Q'));
    assert(!classicsetup_key_is_quit('a'));
    assert(!classicsetup_key_is_quit('\n'));
    assert(!classicsetup_key_is_quit(KEY_F0 + 3));

    assert(classicsetup_key_is_apply('a'));
    assert(classicsetup_key_is_apply('A'));
    assert(!classicsetup_key_is_apply('q'));
    assert(!classicsetup_key_is_apply('\n'));
    assert(!classicsetup_key_is_apply(KEY_F0 + 10));

    return 0;
}
