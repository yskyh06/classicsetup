#include <assert.h>

#include "classicsetup/tui.h"

int main(void)
{
    assert(classicsetup_tui_compact_list_height(0, 8) == 1);
    assert(classicsetup_tui_compact_list_height(3, 8) == 3);
    assert(classicsetup_tui_compact_list_height(12, 8) == 8);
    assert(classicsetup_tui_compact_list_height(3, 0) == 1);
    return 0;
}
