/* Channel argument parsing.

   Copyright (C) 1996, 1997, 1998, 1999, 2001, 2002, 2007
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

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111, USA. */

#include "channel.h"

#include <argp.h>
#include <argz.h>
#include <assert.h>
#include <hurd.h>
#include <string.h>

/* We use this class variable instead of just the name to ensure that
   it gets linked.  */
#define DEFAULT_CHANNEL_CLASS channel_query_class

static const struct argp_option options[] = {
  {"channel-type",'T', "TYPE",   0, "Each CHANNEL names a channel of type TYPE"},
  {0}
};

static const char args_doc[] = "CHANNEL...";
/* XXX handle multiple channels.  */
static const char doc[] = "\vDoes not yet handle multiple CHANNELs.";

struct channel_parsed
{
  /* Names of parsed channels.  */
  char *names;
  size_t names_len;

  /* Prefix that should be applied to each member of NAMES.  */
  char *name_prefix;

  /* --channel-type specified.  This defaults to the `query' type.  */
  const struct channel_class *type;

  /* A vector of class pointers used to lookup class names.  Defaults to
     CHANNEL_STD_CLASSES.  */
  const struct channel_class *const *classes;

  /* DEFAULT_TYPE field passed to parser.  */
  const struct channel_class *default_type;
};

void
channel_parsed_free (struct channel_parsed *parsed)
{
  if (parsed->names_len > 0)
    free (parsed->names);

  if (parsed->name_prefix)
    free (parsed->name_prefix);

  free (parsed);
}

/* Add the arguments PARSED, and return the corresponding channel in
   CHANNEL.  */
error_t
channel_parsed_append_args (const struct channel_parsed *parsed,
			    char **args, size_t *args_len)
{
  char buf[40];
  error_t err = 0;

  /* XXX handle multiple specified channels.  */

  if (!err && parsed->type != parsed->default_type)
    {
      if (parsed->name_prefix)
	/* A name prefix of "PFX" is equivalent to appending ":PFX" to the
	   type name.  */
	{
	  size_t npfx_len = strlen (parsed->name_prefix);
	  char tname[strlen ("--channel-type=")
		     + strlen (parsed->type->name) + 1 + npfx_len + 1];
	  snprintf (tname, sizeof tname, "--channel-type=%s:%.*s",
		    parsed->type->name, (int) npfx_len, parsed->name_prefix);
	  err = argz_add (args, args_len, tname);
	}
      else
	/* A simple type name.  */
	{
	  snprintf (buf, sizeof buf, "--channel-type=%s", parsed->type->name);
	  err = argz_add (args, args_len, buf);
	}
    }

  if (! err)
    err = argz_append (args, args_len, parsed->names, parsed->names_len);

  return err;
}

error_t
channel_parsed_name (const struct channel_parsed *parsed, char **name)
{
  char *pfx = 0;

  /* XXX handle multiple specified channels.  */

  if (pfx)
    *name = malloc (strlen (pfx) + parsed->names_len + 1);
  else
    *name = malloc (parsed->names_len);

  if (! *name)
    return ENOMEM;

  if (pfx)
    {
      char *end = stpcpy (*name, pfx);
      bcopy (parsed->names, end, parsed->names_len);
      argz_stringify (end, parsed->names_len, ',');
      strcpy (end + parsed->names_len, ")");
    }
  else
    {
      bcopy (parsed->names, *name, parsed->names_len);
      argz_stringify (*name, parsed->names_len, ',');
    }

  return 0;
}

/* Open PARSED, and return the corresponding channel in CHANNEL.  */
error_t
channel_create_parsed_hub (const struct channel_parsed *parsed, int flags,
			   struct channel_hub **hub)
{
  size_t pfx_len = parsed->name_prefix ? strlen (parsed->name_prefix) : 0;
  size_t num = argz_count (parsed->names, parsed->names_len);

  error_t create (char *name, struct channel_hub **hub)
  {
    const struct channel_class *type = parsed->type;
    if (type->create_hub)
      {
	if (parsed->name_prefix)
	  /* If there's a name prefix, we prefix any names we open with that
	     and a colon.  */
	  {
	    char pfxed_name[pfx_len + 1 + strlen (name) + 1];
	    stpcpy (stpcpy (stpcpy (pfxed_name, parsed->name_prefix),
			    ":"),
		    name);
	    return (*type->create_hub) (pfxed_name, flags, parsed->classes,
					hub);
	  }
	else
	  return (*type->create_hub) (name, flags, parsed->classes, hub);
      }
    else
      return EOPNOTSUPP;
  }

  /* XXX handle multiple specified channels.  */

  if (num >= 1)
    return create (parsed->names, hub);

  else
    return create (0, hub);
}

static const struct channel_class *
find_class (const char *name, const struct channel_class *const *classes)
{
  const struct channel_class *const *cl;
  for (cl = classes ?: __start_channel_std_classes;
       classes ? *cl != 0 : cl < __stop_channel_std_classes;
       cl++)
    if ((*cl)->name && strcmp (name, (*cl)->name) == 0)
      return *cl;

  /* XXX search for classes in modules.  */

  return 0;
}

/* Print a parsing error message and (if exiting is turned off) return the
   error code ERR.  Requires a variable called STATE to be in scope.  */
#define PERR(err, fmt, args...) \
  do { argp_error (state, fmt , ##args); return err; } while (0)

/* Parse a --channel-type/-T option.  */
static error_t
parse_type (char *arg, struct argp_state *state,
	    struct channel_parsed *parsed)
{
  char *name_prefix = 0;
  char *type_name = arg;
  const struct channel_class *type;
  char *class_sep = strchr (arg, ':');

  if (class_sep)
    /* A `:'-separated class name "T1:T2" is equivalent to prepending "T2:"
       to the device name passed to T1, and is useful for the case where T1
       takes typed names of the form "T:NAME".  A trailing `:', like "T1:" is
       equivalent to prefixing `:' to the device name, which causes NAME to
       be opened with channel_open, as a file.  */
    {
      type_name = strndupa (arg, class_sep - arg);
      name_prefix = class_sep + 1;
    }

  type = find_class (type_name, parsed->classes);
  if (!type || !type->open)
    PERR (EINVAL, "%s: Invalid argument to --channel-type", arg);
  else if (type != parsed->type && parsed->type != parsed->default_type)
    PERR (EINVAL, "--channel-type specified multiple times");

  parsed->type = type;
  parsed->name_prefix = name_prefix;

  return 0;
}

static error_t
parse_opt (int opt, char *arg, struct argp_state *state)
{
  struct channel_parsed *parsed = state->hook;
  error_t err = 0;

  switch (opt)
    {
    case 'T':
      return parse_type (arg, state, parsed);

    case ARGP_KEY_ARG:
      /* A channel name!  */

      if (parsed->type->validate_name)
	err = (*parsed->type->validate_name) (arg, parsed->classes);

      if (! err)
	err = argz_add (&parsed->names, &parsed->names_len, arg);

      if (err)
	argp_failure (state, 1, err, "%s", arg);

      return err;

    case ARGP_KEY_INIT:
      /* Initialize our parsing state.  */
      {
	struct channel_argp_params *params = state->input;
	if (! params)
	  return EINVAL; /* Need at least a way to return a result.  */

	parsed = state->hook = malloc (sizeof (struct channel_parsed));
	if (! parsed)
	  return ENOMEM;

	bzero (parsed, sizeof (struct channel_parsed));
	parsed->classes = params->classes;
	parsed->default_type =
	  find_class (params->default_type ?: DEFAULT_CHANNEL_CLASS.name,
		      parsed->classes);
	if (! parsed->default_type)
	  {
	    free (parsed);
	    return EINVAL;
	  }
	parsed->type = parsed->default_type;
      }
      break;

    case ARGP_KEY_ERROR:
      /* Parsing error occured, free everything.  */
      channel_parsed_free (parsed);
      break;

    case ARGP_KEY_SUCCESS:
      /* Successfully finished parsing, return a result.  */
      if (parsed->names == 0
	  && (!parsed->type->validate_name
	      || (*parsed->type->validate_name) (0, parsed->classes) != 0))
	{
	  struct channel_argp_params *params = state->input;
	  channel_parsed_free (parsed);
	  if (!params->channel_optional)
	    PERR (EINVAL, "No channel specified");
	  parsed = 0;
	}
      ((struct channel_argp_params *)state->input)->result = parsed;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }

  return 0;
}

struct argp
channel_argp = { options, parse_opt, args_doc, doc };
