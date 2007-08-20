/* Dynamic loading of channel class modules.

   Copyright (C) 2002, 2003, 2007
     Free Software Foundation, Inc.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111, USA. */

#include "channel.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <error.h>		/* XXX */

static error_t
open_class (int need_create_hub,
	    const char *name, const char *clname_end,
	    const struct channel_class **classp)
{
  char *modname, *clsym;
  void *mod;

  /* Construct the name of the shared object for this module.  */
  if (asprintf (&modname,
		"libchannel_%.*s%s", (int) (clname_end - name), name,
		CHANNEL_SONAME_SUFFIX) < 0)
    return ENOMEM;

  /* Now try to load the module.

     Note we never dlclose the module, and add a ref every time we open it
     anew.  We can't dlclose it until no channels of this class exist, so
     we'd need a creation/deletion hook for that.  */

  errno = 0;
  mod = dlopen (modname, RTLD_LAZY);
  if (mod == NULL)
    {
      const char *errstring = dlerror (); /* Must always call or it leaks! */
      if (errno != ENOENT)
	/* XXX not good, but how else to report the error? */
	error (0, 0, "cannot load %s: %s", modname, errstring);
    }
  free (modname);
  if (mod == NULL)
    return errno ?: ENOENT;

  if (asprintf (&clsym, "channel_%.*s_class",
		(int) (clname_end - name), name) < 0)
    {
      dlclose (mod);
      return ENOMEM;
    }

  *classp = dlsym (mod, clsym);
  free (clsym);
  if (*classp == NULL)
    {
      error (0, 0, "invalid channel module %.*s: %s",
	     (int) (clname_end - name), name, dlerror ());
      dlclose (mod);
      return EGRATUITOUS;
    }

  if (need_create_hub && ! (*classp)->create_hub)
    {
      /* This class cannot be opened as needed.  */
      dlclose (mod);
      return EOPNOTSUPP;
    }

  return 0;
}

/* Load the module that defines the class NAME (upto NAME_END) and return
   it in CLASS.  Return ENOENT if module isn't available, or any other
   error code from dlopen and friends if module couldn't be loaded.  */
error_t
channel_module_find_class (const char *name, const char *clname_end,
			   const struct channel_class **classp)
{
  return open_class (0, name, clname_end, classp);
}

/* Create and return in HUB, the hub specified by NAME, which should
   consist of a hub type name followed by a `:' and any type-specific
   name.  Its class is loaded dynamically and CLASSES is only passed to
   the class' create_hub method (which usually use it for creating
   sub-hubs.)  */
error_t
channel_create_module_hub (const char *name, int flags,
			   const struct channel_class *const *classes,
			   struct channel_hub **hub)
{
  const struct channel_class *cl;
  const char *clname_end = strchrnul (name, ':');
  error_t err;

  err = open_class (1, name, clname_end, &cl);
  if (err)
    return err;

  if (*clname_end)
    /* Skip the ':' separating the class-name from the device name.  */
    clname_end++;

  if (! *clname_end)
    /* The class-specific portion of the name is empty, so make it *really*
       empty.  */
    clname_end = 0;

  return (*cl->create_hub) (clname_end, flags, classes, hub);
}

const struct channel_class channel_module_class =
  { name: "module", create_hub: channel_create_module_hub };
CHANNEL_STD_CLASS (module);
