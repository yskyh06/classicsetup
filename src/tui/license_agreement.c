#include "classicsetup/license_agreement.h"

#include <ncurses.h>

#include "classicsetup/keymap.h"
#include "classicsetup/tui.h"

enum classicsetup_license_agreement_result
classicsetup_license_agreement_result_for_key(int key)
{
    if (key == 'a' || key == 'A') {
        return CLASSICSETUP_LICENSE_AGREEMENT_ACCEPT;
    }
    if (key == 'b' || key == 'B') {
        return CLASSICSETUP_LICENSE_AGREEMENT_BACK;
    }
    if (classicsetup_key_is_quit(key)) {
        return CLASSICSETUP_LICENSE_AGREEMENT_QUIT;
    }
    return CLASSICSETUP_LICENSE_AGREEMENT_WAIT;
}

enum classicsetup_license_agreement_result
classicsetup_show_license_agreement(void)
{
    static const char agreement[] =
        "ClassicSetup can change partition tables, filesystems, and boot "
        "configuration. Files on the selected disk may be permanently "
        "deleted during installation.\n\n"
        "Back up all important data before continuing. ClassicSetup is not "
        "guaranteed to work correctly with every hardware configuration or "
        "disk layout. You are responsible for confirming the target disk and "
        "maintaining a current backup of important data.\n\n"
        "By continuing, you confirm that you understand these risks.";

    for (;;) {
        enum classicsetup_license_agreement_result result;
        int key;

        classicsetup_tui_begin_screen("ClassicSetup - License / Risk Agreement");
        classicsetup_tui_draw_wrapped_text(
            3,
            3,
            classicsetup_tui_canvas_width() - 6,
            agreement);
        classicsetup_tui_draw_warning(
            classicsetup_tui_canvas_height() - 4,
            "Press A only if you agree and want to continue.");
        classicsetup_tui_draw_footer("A=I Agree    B=Back    Q=Quit");
        refresh();

        key = getch();
        result = classicsetup_license_agreement_result_for_key(key);
        if (result != CLASSICSETUP_LICENSE_AGREEMENT_WAIT) {
            return result;
        }
    }
}
