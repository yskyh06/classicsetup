#include <assert.h>
#include <string.h>

#include "classicsetup/license_agreement.h"
#include "classicsetup/tui.h"

int main(void)
{
    char wrapped[256];
    int width;
    int height;

    assert(classicsetup_tui_compact_list_height(0, 8) == 1);
    assert(classicsetup_tui_compact_list_height(3, 8) == 3);
    assert(classicsetup_tui_compact_list_height(12, 8) == 8);
    assert(classicsetup_tui_compact_list_height(3, 0) == 1);

    assert(classicsetup_tui_wrap_text(
               "one two three", 7, wrapped, sizeof(wrapped)) == 2);
    assert(strcmp(wrapped, "one two\nthree") == 0);
    assert(classicsetup_tui_wrap_text(
               "alpha beta gamma", 5, wrapped, sizeof(wrapped)) == 3);
    assert(strcmp(wrapped, "alpha\nbeta\ngamma") == 0);
    assert(classicsetup_tui_wrap_text(
               "abcdefgh", 3, wrapped, sizeof(wrapped)) == 3);
    assert(strcmp(wrapped, "abc\ndef\ngh") == 0);
    assert(classicsetup_tui_wrap_text(
               "first\nsecond", 20, wrapped, sizeof(wrapped)) == 2);
    assert(strcmp(wrapped, "first\nsecond") == 0);
    assert(classicsetup_tui_wrap_text(
               "Files on the selected disk may be permanently deleted "
               "during installation.\n\nBack up important data.",
               74,
               wrapped,
               sizeof(wrapped)) >= 3);
    assert(strstr(wrapped, "during installation.") != NULL);
    assert(strstr(wrapped, "\n\nBack up important data.") != NULL);

    assert(classicsetup_tui_select_layout_profile(79, 25) ==
           CLASSICSETUP_TUI_LAYOUT_COMPACT);
    assert(classicsetup_tui_select_layout_profile(80, 25) ==
           CLASSICSETUP_TUI_LAYOUT_CLASSIC);
    assert(classicsetup_tui_select_layout_profile(100, 30) ==
           CLASSICSETUP_TUI_LAYOUT_MEDIUM);
    assert(classicsetup_tui_select_layout_profile(120, 35) ==
           CLASSICSETUP_TUI_LAYOUT_LARGE);
    classicsetup_tui_canvas_dimensions_for_terminal(200, 80, &width, &height);
    assert(width == 120);
    assert(height == 40);
    classicsetup_tui_canvas_dimensions_for_terminal(60, 20, &width, &height);
    assert(width == 60);
    assert(height == 20);

    assert(classicsetup_license_agreement_result_for_key('\n') ==
           CLASSICSETUP_LICENSE_AGREEMENT_WAIT);
    assert(classicsetup_license_agreement_result_for_key('a') ==
           CLASSICSETUP_LICENSE_AGREEMENT_ACCEPT);
    assert(classicsetup_license_agreement_result_for_key('A') ==
           CLASSICSETUP_LICENSE_AGREEMENT_ACCEPT);
    assert(classicsetup_license_agreement_result_for_key('q') ==
           CLASSICSETUP_LICENSE_AGREEMENT_QUIT);
    assert(classicsetup_license_agreement_result_for_key('Q') ==
           CLASSICSETUP_LICENSE_AGREEMENT_QUIT);
    assert(classicsetup_license_agreement_result_for_key('b') ==
           CLASSICSETUP_LICENSE_AGREEMENT_BACK);
    return 0;
}
