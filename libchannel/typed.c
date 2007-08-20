/* Support for creating channel hubs by type.

   Copyright (C) 1997, 1998, 2001, 2002, 2003, 2004, 2007
     Free Software Foundation, Inc.

   Written by Miles Bader <miles@gnu.org>
   Reworked for libchannel by Carl Fredrik Hammar <hammy.lite@gmail.com>

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

#include <string.h>
#include <dlfcn.h>
#include <link.h>


/* Find and return a class by name in CLASSES if not null, otherwise in
   the `channel_std_class' section and already loaded modules.  NAME_END
   points to the character after the class name in NAME; if null, then
   NAME is the null-terminated class name.  */
const struct channel_class *
channel_find_class (const char *name, const char *clname_end,
		    const struct channel_class *const *classes)
{
  const struct channel_class *const *cl;

  if (! clname_end)
    clname_end = strchr (name, '\0');

  if (classes != 0)
    {
      /* The caller gave a class list, so that's is all we'll use.  */
      for (cl = classes; *cl != 0; ++cl)
	if (strlen ((*cl)->name) == (clname_end - name)
	    && !memcmp (name, (*cl)->name, (clname_end - name)))
	  break;
      return *cl;
    }

  /* Check the statically-linked set of classes found in the
     `channel_std_classes' section.  For static linking, this is the
     section in the program executable itself and it has been populated by
     the set of -lchannel_TYPE pseudo-libraries included in the link.  For
     dynamic linking with just -lchannel, these symbols will be found in
     libchannel.so and have the set statically included when the shared
     object was built.  If a dynamically-linked program has its own
     `channel_std_classes' section, e.g. by -lchannel_TYPE objects
     included in the link, this will be just that section and
     libchannel.so itself is covered below.  */
  for (cl = __start_channel_std_classes; cl < __stop_channel_std_classes; ++cl)
    if (strlen ((*cl)->name) == (clname_end - name)
	&& strncmp (name, (*cl)->name, (clname_end - name)) == 0)
      return *cl;

  /* Now we will iterate through all of the dynamic objects loaded and
     examine each one's `channel_std_classes' section.  */
# pragma weak _r_debug
# pragma weak dlsym
# pragma weak dlopen
# pragma weak dlclose
# pragma weak dlerror
  if (dlsym)
    {
      struct link_map *map;
      for (map = _r_debug.r_map; map != 0; map = map->l_next)
	{
	  const struct channel_class *const *start, *const *stop;

	  /* We cannot just use MAP directly because it may not have been
	     opened by dlopen such that its data structures are fully set
	     up for dlsym.  */
	  void *module = dlopen (map->l_name, RTLD_NOLOAD);
	  if (module == 0)
	    {
	      (void) dlerror (); /* Required to avoid a leak! */
	      continue;
	    }

	  start = dlsym (map, "__start_channel_std_classes");
	  if (start == 0)
	    (void) dlerror ();	/* Required to avoid a leak! */
	  else if (start != __start_channel_std_classes) /*  */
	    {
	      stop = dlsym (map, "__stop_channel_std_classes");
	      if (stop == 0)
		(void) dlerror (); /* Required to avoid a leak! */
	      else
		for (cl = start; cl < stop; ++cl)
		  if (strlen ((*cl)->name) == (clname_end - name)
		      && strncmp (name, (*cl)->name, (clname_end - name)) == 0)
		    {
		      dlclose (module);
		      return *cl;
		    }
	    }
	  dlclose (module);
	}
    }

  return 0;
}


/* Create the channel hub indicated by NAME, which should consist of a
   channel type name followed by a ':' and any type-specific name,
   returning the new hub in HUB.  If NAME doesn't contain a `:', then it
   will be interpreted as either a class name, if such a class occurs in
   CLASSES, or a filename, which is opened by calling
   channel_create_query_hub on NAME; a `:' at the end or the beginning of
   NAME unambiguously causes the remainder to be treated as a class-name
   or a filename, respectively.  CLASSES is used to select classes
   specified by the type name; if it is 0, CHANNEL_STD_CLASSES is
   used.  */
error_t
channel_create_typed_hub (const char *name, int flags,
			  const struct channel_class *const *classes,
			  struct channel_hub **hub)
{
  const struct channel_class *cl;
  const char *clname_end = strchrnul (name, ':');

  if (clname_end == name && *clname_end)
    /* Query NAME.  */
    return channel_create_query_hub (name + 1, flags, classes, hub);

  /* Try to find an existing class by the given name.  */
  cl = channel_find_class (name, clname_end, classes);
  if (cl != 0)
    {
      if (! cl->create_hub)
	/* CL cannot be opened.  */
	return EOPNOTSUPP;

      if (*clname_end)
	/* Skip the ':' separating the class-name from the device name.  */
	clname_end++;

      if (! *clname_end)
	/* The class-specific portion of the name is empty, so make it *really*
	   empty.  */
	clname_end = 0;

      return (*cl->create_hub) (clname_end, flags, classes, hub);
    }

  /* Try to open a channel hub by loading a module containing the class,
     if we have the module-loading support linked in.  We don't just use
     channel_module_find_class, because channel_create_module_hub will
     unload the new module if the open doesn't succeed and we have no
     other way to unload it.  We always leave modules loaded once a
     channel from the module has been successfully opened and so can leave
     unbounded numbers of old modules loaded after closing all the hubs
     using them.  But at least we can avoid having modules loaded for hubs
     we never even opened.  */
# pragma weak channel_create_module_hub
  if (channel_create_module_hub)
    {
      error_t err = channel_create_module_hub (name, flags, classes, hub);
      if (err != ENOENT)
	return err;
    }

  /* No class with the given name found.  */
  if (*clname_end)
    /* NAME really should be a class name, which doesn't exist.  */
    return EINVAL;
  else
    /* Try opening NAME by querying it instead.  */
    return channel_create_query_hub (name, flags, classes, hub);
}

const struct channel_class
channel_typed_class = { name: "typed", create_hub: channel_create_typed_hub };
CHANNEL_STD_CLASS (typed);
