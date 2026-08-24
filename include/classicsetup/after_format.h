#ifndef CLASSICSETUP_AFTER_FORMAT_H
#define CLASSICSETUP_AFTER_FORMAT_H

enum classicsetup_after_format_result {
    CLASSICSETUP_AFTER_FORMAT_FINISH,
    CLASSICSETUP_AFTER_FORMAT_BACK,
    CLASSICSETUP_AFTER_FORMAT_QUIT
};

enum classicsetup_after_format_result classicsetup_show_after_format(
    int partitions_applied);

#endif
