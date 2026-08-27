#include "classicsetup/gui.h"

#include <stdio.h>
#include <string.h>

#include "classicsetup/disk.h"

int classicsetup_gui_session_init(
    struct classicsetup_gui_session *session)
{
    struct classicsetup_disk_info disks[CLASSICSETUP_GUI_MAX_DISKS];
    size_t disk_count = 0;
    size_t index;

    if (session == NULL) {
        return -1;
    }
    classicsetup_gui_session_reset(session);
    session->firmware = classicsetup_detect_firmware();
    session->scan_failed = classicsetup_scan_disks(
                               disks,
                               CLASSICSETUP_GUI_MAX_DISKS,
                               &disk_count) != 0;
    if (session->scan_failed) {
        return 0;
    }
    for (index = 0; index < disk_count; ++index) {
        if (classicsetup_assess_disk(
                &disks[index],
                &session->assessments[index]) != 0) {
            memset(&session->assessments[index], 0,
                   sizeof(session->assessments[index]));
            session->assessments[index].disk = disks[index];
            session->assessments[index].disk_class =
                CLASSICSETUP_DISK_UNKNOWN;
            session->assessments[index].selectable = 0;
            (void)snprintf(
                session->assessments[index].presentation,
                sizeof(session->assessments[index].presentation),
                "%s",
                classicsetup_disk_class_presentation(
                    CLASSICSETUP_DISK_UNKNOWN));
        }
    }
    session->assessment_count = disk_count;
    return 0;
}
