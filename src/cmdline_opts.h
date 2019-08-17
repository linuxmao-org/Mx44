#ifndef CMDLINE_OPTS_H
#define CMDLINE_OPTS_H

#include <getopt.h>

#define VOICEPOOL  48

#define MINVOICES 1
#define MAXVOICES 256

typedef struct {
    char* load_from;    /* patch to load on startup                 */
    char* save_to;      /* patch to save on shutdown                */
    int  voices;        /* number of voices                         */
    char console;       /* > 0 == don't run gui                     */
    char autoconnect;   /* > 0 == autoconnect outputs to playback   */
    char cmderror;      /* > 0 == error in options                  */
} opts;

opts * get_cmdline_opts(int argc, char **argv);
void cleanup_opts(opts * options);

#endif
