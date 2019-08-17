/*
 * This software is released under GPL. If you don't know what that is
 * immediately point your webbrowser to  http://www.gnu.org and read
 * all about it.
 *
 * The short version: You can modify the sources for your own purposes
 * but you must include the new sources if you distribute a derived work.
 *
 * (C) 2009 James Morris <james@jwm-art.net>
 *
 *
 *
 *    (
 *     )
 *   c[]
 *
 *
 */ 
#include "cmdline_opts.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define HAS_ARG 1

static struct option longopts[] = {
    {   "load-from",    HAS_ARG,    0,  'l' },
    {   "save-to",      HAS_ARG,    0,  's' },
    {   "voices",       HAS_ARG,    0,  'v' },
    {   "console",      0,          0,  'c' },
    {   "auto-connect", 0,          0,  'a' },
    {   0,              0,          0,  0   }
};


static void showusage(char* argvzero)
{
    printf("usage:  %s [options]\n", argvzero);
    printf("options:\n");
    printf("    -l, --load-from     patch file to load\n");
    printf("    -s, --save-to       patch file to save\n\n");
    printf("note: if you specify --load-from you must also\n"
           "specify --save-to.\n\n");
    printf("    -v, --voices        number of voices "
           "(default: %d)\n\n",VOICEPOOL);
    printf("    -c, --console       force mx44 to run without\n"
           "                        a gui.\n");
    printf("    -a, --auto-connect  automatically connect outputs\n"
           "                        to system playback ports.\n");
    printf("\nMx44.2 (c) 2006-2009 Jens M Andreasen and others\n\n");
    printf("distributed under the terms of the GNU GPLv3\n");
}

opts * get_cmdline_opts(int argc, char **argv)
{
    int optcount = 0;
    while (longopts[optcount].name)
        ++optcount;

    char shortopts[optcount * 2 + 1];
    char* sop = shortopts;
    struct option * lop = longopts;

    for (; lop < &longopts[optcount]; ++lop)
    {
        *sop++ = lop->val;
        if (lop->has_arg)
            *sop++ = ':';
    }

    opts * options = malloc(sizeof(opts));

    if (!options)
    {
        fprintf(stderr, "memory allocation error "
                        "parsing commandline options\n");
        return 0;
    }
    /* initialize the options to zero! */
    memset(options, 0, sizeof(opts));
    options->voices = VOICEPOOL;
    if (!getenv("DISPLAY"))
    {
        fprintf(stderr,"no DISPLAY found, running in console mode.\n");
        options->console = 1;
    }

    for (;;)
    {
        int c = getopt_long(argc, argv, shortopts, longopts, 0);

        if (c == -1)
            break;

        switch(c)
        {
        case 'l':
            if (!(options->load_from = strdup(optarg)))
            {
                fprintf(stderr, "out of memory processing %s=%s\n"
                                "aborting command line processing!",
                                longopts[optind].name, optarg);
                free(options);
                return 0;
            }
            break;

        case 's':
            if (!(options->save_to = strdup(optarg)))
            {
                fprintf(stderr, "out of memory processing %s=%s\n"
                                "aborting command line processing!",
                                longopts[optind].name, optarg);
                free(options);
                return 0;
            }
            break;

        case 'v':
            options->voices = atoi(optarg);
            if (options->voices < MINVOICES
             || options->voices > MAXVOICES)
            {
                fprintf(stderr, "ignoring %s=%s\n",
                                longopts[optind].name, optarg);
                options->voices = VOICEPOOL;
            }
            break;

        case 'c':
            options->console = 1;
            printf("running in console mode\n");
            break;

        case 'a':
            options->autoconnect = 1;
            printf("outputs set to auto-connect\n");
            break;

        default:
            options->cmderror = 1;
            showusage(argv[0]);
            return options;
        }
    }

    if (options->load_from)
        if (!options->save_to)
        {
            options->cmderror = 1;
            showusage(argv[0]);
        }

    return options;
}

void cleanup_opts(opts * options)
{
    if (!options)
        return;
    if (options->load_from)
        free(options->load_from);
    if (options->save_to)
        free(options->save_to);
    free(options);
}

