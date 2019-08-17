/*
 * This software is released under GPL. If you don't know what that is
 * immediately point your webbrowser to  http://www.gnu.org and read
 * all about it.
 *
 * The short version: You can modify the sources for your own purposes
 * but you must include the new sources if you distribute a derived work.
 *
 * (C) 2002, 2003, 2004, 2005, 2006 Jens M Andreasen <ja@linux.nu>
 *     2009 James Morris <james@jwm-art.net>
 *
 *
 *
 *    (
 *     )
 *   c[]
 *
 *
 */ 
#include "stateio.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static char* userpatch_name = 0;

static int patch_fd = -1;

static char*
get_patchname(const char* path, const char* patch)
{
    if (!patch)
    {
        fprintf(stderr,"patch file name error\n");
        return 0;
    }

    int pathlen = 0;
    char glue = 0;

    if (path)
    {
        pathlen = strlen(path);
        if (pathlen > 0)
            if (path[pathlen - 1] != '/' && *patch !='/')
                glue = 1;
    }

    int   patchlen = strlen(patch);
    char* patchname = malloc(pathlen + glue + patchlen + 1);

    if (!patchname)
    {
        fprintf(stderr, "memory error for patch name and path\n");
        return 0;
    }

    char *ptr = patchname;
    strcpy(patchname, path);
    ptr += pathlen;

    if (glue)
        *ptr++ = '/';

    strcpy(ptr, patch);
    return patchname;
}


void load_state(Mx44state * mx44,
                const char* load_from,
                const char* save_to)
{
    char* pname = 0;
    const char* home = getenv("HOME");

    const char* patches[][2] = {
        {   0,          0,              },
        {   0,          0,              },
        {   home,       ".mx44patch"    },
        {   home,       ".mx44patch"    },
        {   "",         "mx44patch"     },
        {   "",         "mx41patch"     },
        {   DATADIR,    "mx44patch"     },
        {   0,          0               }
    };

    int p = 2; /* skips first two  patch entries  to make *
                * .mx44patch default save to, followed by *
                * .mx44patch default load from, unless... */

    int saveto = p;

    if (load_from)
    {
        --p;
        patches[p][0] = "";
        patches[p][1] = load_from;
    }

    if (save_to)
    {
        saveto = --p;
        patches[p][0] = "";
        patches[p][1] = save_to;
    }

    int fd = -1;

    while (patches[p][0])
    {
        pname = get_patchname(patches[p][0], patches[p][1]);
        if (pname)
        {
            if (saveto == p)
            {
                userpatch_name = strdup(pname);
                ++p;
                continue;
            }

            if ((fd = open(pname, O_RDONLY)) > -1)
                break;

            fprintf(stderr, "error opening: %s : %s\n",
                        pname, strerror(errno));
            free(pname);
        }
        ++p;
    }

    int count;

    if (fd > -1)
    {
      count = read(fd, mx44->patch,
		   sizeof(mx44->patch)
		   + sizeof(mx44->tmpPatch)
		   + sizeof(mx44->patchNo)
		   + sizeof(mx44->monomode)
		   + sizeof(mx44->shruti));
      count += read(fd, &mx44->temperament, sizeof(mx44->temperament));

      if (count > 0)
	printf("patch data loaded from: %s\n", pname);
      else
	fprintf(stderr, "Data error reading: %s: %s\n",
		strerror(errno), pname);
      
      close(fd);
      free(pname);
    }

    if (!userpatch_name)
    {
      fprintf(stderr, "user patch name could not be formed.\n"
	      "patches will not be saved :-/\n");
      return;
    }
    
    patch_fd = open(userpatch_name, O_RDWR | O_CREAT, 0666);
    
    if (patch_fd > -1)
      printf("patches will be saved to %s\n", userpatch_name);
    else
      fprintf(stderr, "error opening patch for writing: %s : %s.\n"
	      "patches will not be saved :-(\n",
	      userpatch_name, strerror(errno));
}


void save_state(Mx44state * mx44)
{
  puts("\n");
  
  if (!userpatch_name || patch_fd < 0)
    return;
  
  int count;
  lseek(patch_fd, 0, SEEK_SET);
  count =  write(patch_fd,mx44->patch,       sizeof(mx44->patch));
  count += write(patch_fd,mx44->tmpPatch,    sizeof(mx44->tmpPatch));
  count += write(patch_fd,mx44->patchNo,     sizeof(mx44->patchNo));
  count += write(patch_fd,mx44->monomode,    sizeof(mx44->monomode));
  count += write(patch_fd,mx44->shruti,      sizeof(mx44->shruti));
  count += write(patch_fd,&mx44->temperament,sizeof(mx44->temperament));
/*
  int total = sizeof(mx44->patch)
	    + sizeof(mx44->tmpPatch)
	    + sizeof(mx44->patchNo)
	    + sizeof(mx44->monomode)
	    + sizeof(mx44->shruti)
	    + sizeof(mx44->temperament);

  printf("writing: total:%d count:%d\n",total,count);
*/
  if(count == 
       sizeof(mx44->patch)
     + sizeof(mx44->tmpPatch)
     + sizeof(mx44->patchNo)
     + sizeof(mx44->monomode)
     + sizeof(mx44->shruti)
     + sizeof(mx44->temperament))
    {
      printf("patch data saved to %s\n", userpatch_name);
    }
  else
    fprintf(stderr, "error saving patch data :-|\n");
  
  fprintf(stderr,"Thankyou for using Mx44!\n");
  free(userpatch_name);
  close(patch_fd);
}
