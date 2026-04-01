#ifndef AXAR_ARCHIVE_H
#define AXAR_ARCHIVE_H



#define AXAR_MAGIC "!<arch>\n"
#define AXAR_MAGIC_LEN 8

struct ar_header {
    char ar_name[16];
    char ar_date[12];
    char ar_uid[6];
    char ar_gid[6];
    char ar_mode[8];
    char ar_size[10];
    char ar_fmag[2];
};

#endif