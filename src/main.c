/*
 * This software is released under GPL. If you don't know what that is
 * immediately point your webbrowser to  http://www.gnu.org and read
 * all about it.
 *
 * The short version: You can modify the sources for your own purposes
 * but you must include the new source  if you distribute a derived work.
 *
 * (C) 2002, 2003, 2004, 2005, 2006 Jens M Andreasen <ja@linux.nu>
 *     2009 James Morris <james@jwm-art.net> 
 *
 *
 *     (
 *      )
 *    c[]
 *
 */ 

#include "mx44.h"
#include "interface2.h"
#include "stateio.h"
#include "cmdline_opts.h"

#include <sys/mman.h> /* for mlock */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

Mx44state * mx44;
opts * options;

void call_save_state(void)
{
    save_state(mx44);
    cleanup_opts(options);
}

int main(int argc, char **argv)
{
  options = get_cmdline_opts(argc, argv);

  if (!options)
    return -1;

  if (options->cmderror)
    {
      cleanup_opts(options);
      return -1;
    }

  /*
   * mmx_ok() seems to be broken with gcc 3.x ?
   * 
   if(mmx_ok()) puts("MMX OK!");
   else puts("MMX not available."), exit(0);
   *
   */

  mx44 = mx44_new(options->voices); /* starts jack & alsa threads */

  if(!(mx44->samplerate))
  {
    cleanup_opts(options);
    exit(1);
  }

  #if _POSIX_MEMLOCK > 0
  mlockall(MCL_CURRENT | MCL_FUTURE);
  #endif

  load_state(mx44, options->load_from, options->save_to);

  atexit(call_save_state);

  mx44_reset(mx44, 440.0, mx44->samplerate);

  if(options->console)
    while(1)
        sleep(1);
  else
    {
      mx44->isGraphic = 1; 
      main_interface(argc, argv);
    }

  exit(0);
}

