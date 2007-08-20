/* Managing hub children.

   Copyright (C) 1995, 1996, 1997, 2001, 2002, 2007
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "channel.h"

/* Set the HUB's list of children to a copy of CHILDREN and NUM_CHILDREN,
   or if allocation fails return ENOMEM.  */
error_t
channel_set_children (struct channel_hub *hub,
		      struct channel_hub *const *children,
		      size_t num_children)
{
  size_t size = num_children * sizeof (struct channel_hub *);
  struct channel_hub **copy = malloc (size);

  if (!copy)
    return ENOMEM;

  if (hub->children)
    free (hub->children);

  memcpy (copy, children, size);
  hub->children = copy;
  hub->num_children = num_children;

  return 0;
}

/* Set FLAGS in all the children of HUB, and if successful, set them in
   HUB also.  Flags are set using channel_set_hub_flags.  Propagate any
   error on failure.  */
error_t
channel_set_child_flags (struct channel_hub *hub, int flags)
{
  int i;
  error_t err = 0;
  int old_child_flags[hub->num_children];

  for (i = 0; i < hub->num_children && !err; i++)
    {
      old_child_flags[i] = hub->children[i]->flags;
      err = channel_set_hub_flags (hub->children[i], flags);
    }

  if (err)
    while (i-- > 0)
      channel_clear_hub_flags (hub->children[i],
			       flags & ~old_child_flags[i]);
  else
    hub->flags |= flags;

  return err;
}

/* Clear FLAGS in all the children of HUB, and if successful, clear them
   in HUB also.  Flags are cleared as if using channel_clear_hub_flags.
   Propagate any error on failure.  */
error_t
channel_clear_child_flags (struct channel_hub *hub, int flags)
{
  int i;
  error_t err = 0;
  int old_child_flags[hub->num_children];

  for (i = 0; i < hub->num_children && !err; i++)
    {
      old_child_flags[i] = hub->children[i]->flags;
      err = channel_clear_hub_flags (hub->children[i], flags);
    }

  if (err)
    while (i-- > 0)
      channel_set_hub_flags (hub->children[i], flags & ~old_child_flags[i]);
  else
    hub->flags &= ~flags;

  return err;
}

/* Parse multiple hub names in NAME, createing and returning each in HUBS
   and NUM_HUBS.  The syntax of name is a single non-alpha-numeric
   character followed by each child hub's name, seperated by the same
   separator.  Each child name is in TYPE:NAME notation as parsed by
   channel_create_typed_hub.  If all children has the same TYPE: prefix,
   then it may be factored out and put before the child list instead.  */
error_t
channel_create_hub_children (const char *name, int flags,
			     const struct channel_class *const *classes,
			     struct channel_hub ***hubs, size_t *num_hubs)
{
  char *pfx = 0;      /* Prefix applied to each part name.  */
  size_t pfx_len = 0; /* Space PFX + separator takes up.  */
  char sep = *name;   /* Character separating individual names.  */

  if (sep && isalnum (sep))
    /* If the first character is a `name' character, it's likely to be either
       a type prefix (e.g, TYPE:@NAME1@NAME2@), so we distribute the type
       prefix among the elements (@TYPE:NAME1@TYPE:NAME2@).  */
    {
      const char *pfx_end = name;

      while (isalnum (*pfx_end))
	pfx_end++;

      if (*pfx_end++ != ':')
	return EINVAL;

      /* Make a copy of the prefix.  */
      pfx = strndupa (name, pfx_end - name);
      pfx_len = pfx_end - name;

      sep = *pfx_end;
    }

  if (sep)
    /* Parse a list of hub specs separated by SEP.  */
    {
      int k;
      const char *p, *end;
      error_t err = 0;
      size_t count = 0;

      /* First, see how many there are.  */
      for (p = name; p && p[1]; p = strchr (p + 1, sep))
	count++;

      /* Make a vector to hold them.  */
      *hubs = malloc (count * sizeof (struct channel_hub *));
      *num_hubs = count;
      if (! *hubs)
	return ENOMEM;

      bzero (*hubs, count * sizeof (struct channel_hub *));

      /* Open each child hub.  */
      for (p = name, k = 0; !err && p && p[1]; p = end, k++)
	{
	  size_t kname_len;

	  end = strchr (p + 1, sep);
	  kname_len = (end ? end - p - 1 : strlen (p + 1));

	  {
	    /* Allocate temporary child name on the stack.  */
	    char kname[pfx_len + kname_len + 1];

	    if (pfx)
	      /* Add type prefix to child name.  */
	      memcpy (kname, pfx, pfx_len);

	    memcpy (kname + pfx_len, p + 1, kname_len);
	    kname[pfx_len + kname_len] = '\0';

	    err = channel_create_typed_hub (kname, flags, classes,
					    &(*hubs)[k]);
	  }
	}

      if (err)
	/* Failure opening some child, deallocate what we've done so far.  */
	{
	  while (--k >= 0)
	    channel_free_hub ((*hubs)[k]);
	  free (*hubs);
	}

      return err;
    }
  else
    /* Empty list.  */
    {
      *hubs = 0;
      *num_hubs = 0;
      return 0;
    }
}

/* Generate a name for the children of HUB into NAME.  It done by
   combining the name of each child in a way that the name can be parsed
   by channel_create_hub_children.  This is done heuristically, and it may
   fail and return EGRATUITOUS.  If a child does not have a name, return
   EINVAL.  If memory is exausted, return ENOMEM.  */
error_t
channel_children_name (const struct channel_hub *hub, char **name)
{
  static char try_seps[] = "@+=,._%|;^!~'&";
  struct channel_hub **kids = hub->children;
  size_t num_kids = hub->num_children;

  if (num_kids == 0)
    {
      *name = strdup ("");
      return *name ? 0 : ENOMEM;
    }
  else
    {
      int k;
      char *s;			/* Current separator in search for one.  */
      int fail;			/* If we couldn't use *S as as sep. */
      size_t total_len = 0;	/* Length of name we will return.  */

      /* Detect children without names, and calculate the total length of the
	 name we will return (which is the sum of the lengths of the child
	 names plus room for the types and separator characters.  */
      for (k = 0; k < num_kids; k++)
	if (!kids[k] || !kids[k]->name)
	  return EINVAL;
	else
	  total_len +=
	    /* separator + type name + type separator + child name */
	    1 + strlen (kids[k]->class->name) + 1 + strlen (kids[k]->name);

      /* Look for a separator character from those in TRY_SEPS that doesn't
	 occur in any of the the child names.  */
      for (s = try_seps, fail = 1; *s && fail; s++)
	for (k = 0, fail = 0; k < num_kids && !fail; k++)
	  if (strchr (kids[k]->name, *s))
	    fail = 1;

      if (*s)
	/* We found a usable separator!  */
	{
	  char *p = malloc (total_len + 1);

	  if (! p)
	    return ENOMEM;
	  *name = p;

	  for (k = 0; k < num_kids; k++)
	    p +=
	      sprintf (p, "%c%s:%s", *s, kids[k]->class->name, kids[k]->name);

	  return 0;
	}
      else
	return EGRATUITOUS;
    }
}
