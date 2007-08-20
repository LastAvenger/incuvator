/* A translator for doing I/O to channels.

   Copyright (C) 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2007
     Free Software Foundation, Inc.

   Written by Miles Bader <miles@gnu.org>
   Reworked for channelio by Carl Fredrik Hammar <hammy.lite@gmail.com>

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   675 Mass Ave, Cambridge, MA 02139, USA. */

#include <stdio.h>
#include <error.h>
#include <assert.h>
#include <fcntl.h>
#include <argp.h>
#include <argz.h>

#include <mach.h>
#include <device/device.h>

#include <hurd.h>
#include <hurd/ports.h>
#include <hurd/trivfs.h>
#include <hurd/channel.h>
#include <version.h>

#include "node.h"

static struct argp_option options[] =
{
  {"readonly",   'r',    0, 0, "Disallow writing"},
  {"writable",   'w',    0, 0, "Disallow reading"},
  {"no-file-io", 'F',    0, 0, "Never perform io via plain file io RPCs"},
  {"no-fileio",    0,    0, OPTION_ALIAS | OPTION_HIDDEN},
  {"enforced",   'e',    0, 0,
   "Never reveal underlying devices, even to root"},
  {"rdev",       'n', "ID", 0,
   "The stat rdev number for this node; may be either a"
   " single integer, or of the form MAJOR,MINOR"},
  {0}
};
static const char doc[] =
  "Translator for character devices and other channels";

const char *argp_program_version = STANDARD_HURD_VERSION (channelio);

/* Desired channel parameters specified by the user.  */
struct channelio_argp_params
{
  /* Filled in by channel_argp parser.  */
  struct channel_argp_params channel_params;

  /* We fill in its flag members and rdev number.  */
  struct node *node;
};

/* Parse a single option.  */
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  struct channelio_argp_params *params = state->input;

  switch (key)
    {
    case 'r': params->node->flags |= CHANNEL_READONLY; break;
    case 'w': params->node->flags |= CHANNEL_WRITEONLY; break;

    case 'e': params->node->flags |= CHANNEL_ENFORCED; break;
    case 'F': params->node->flags |= CHANNEL_NO_FILEIO; break;

    case 'n':
      {
	char *start = arg, *end;
	dev_t rdev;

	rdev = strtoul (start, &end, 0);
	if (*end == ',')
	  /* MAJOR,MINOR form.  */
	  {
	    start = end + 1;
	    rdev = makedev (rdev, strtoul (start, &end, 0));
	  }

	if (end == start || *end != '\0')
	  {
	    argp_error (state, "%s: Invalid argument to --rdev", arg);
	    return EINVAL;
	  }

	params->node->rdev = rdev;
      }
      break;

    case ARGP_KEY_INIT:
      /* Now channel_argp's parser will get to initialize its state.
	 The default_type member is our input parameter to it.  */
      memset (&params->channel_params, 0, sizeof params->channel_params);
      /* XXX set default type.  */
      params->channel_params.channel_optional = 1;
      state->child_inputs[0] = &params->channel_params;
      break;

    case ARGP_KEY_SUCCESS:
      params->node->channel_name = params->channel_params.result;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

static const struct argp_child argp_kids[] = { { &channel_argp }, {0} };
static const struct argp argp = { options, parse_opt, 0, doc, argp_kids };

/* Handles channel control RPCs.  */

/* Find and return the channel associated with PORT or return null if
   there is no such channel.  Overrides libchannel's implementation.  */
struct channel *
channel_begin_using_channel (mach_port_t port)
{
  struct trivfs_protid *cred = trivfs_begin_using_protid (port);
  struct open *open;

  if (! cred)
    return 0;

  open = cred->po->hook;
  mutex_lock (&open->lock);
  return open->channel;
}

/* Reverses any side-effects of channel_begin_using_channel.  Overrides
   libchannel's implementation.  */
void
channel_end_using_channel (struct channel *channel)
{
  if (channel)
    {
      struct open *open = channel->user_hook;
      mutex_unlock (&open->lock);
    }
}

int
channelio_demuxer (mach_msg_header_t *in, mach_msg_header_t *out)
{
  return trivfs_demuxer (in, out) || channel_control_demuxer (in, out);
}

struct trivfs_control *channelio_fsys;

int
main (int argc, char *argv[])
{
  error_t err;
  mach_port_t bootstrap;
  struct channelio_argp_params params;
  struct node node;

  node_init (&node);
  params.node = &node;

  argp_parse (&argp, argc, argv, 0, 0, &params);

  task_get_bootstrap_port (mach_task_self (), &bootstrap);
  if (bootstrap == MACH_PORT_NULL)
    error (2, 0, "Must be started as a translator");

  /* Reply to our parent.  */
  err = trivfs_startup (bootstrap, 0, 0, 0, 0, 0, &channelio_fsys);
  if (err)
    error (3, err, "trivfs_startup");

  channelio_fsys->hook = &node;

  /* Launch. */
  ports_manage_port_operations_multithread (channelio_fsys->pi.bucket,
					    channelio_demuxer,
					    30*1000, 5*60*1000, 0);

  return 0;
}

error_t
trivfs_append_args (struct trivfs_control *trivfs_control,
		    char **argz, size_t *argz_len)
{
  struct node *node = trivfs_control->hook;
  error_t err = 0;

  if (node->rdev != (dev_t) 0)
    {
      char buf[40];
      snprintf (buf, sizeof buf, "--rdev=%d,%d",
		major (node->rdev), minor (node->rdev));
      err = argz_add (argz, argz_len, buf);
    }

  if (!err && (node->flags & CHANNEL_ENFORCED))
    err = argz_add (argz, argz_len, "--enforced");

  if (!err && (node->flags & CHANNEL_NO_FILEIO))
    err = argz_add (argz, argz_len, "--no-file-io");

  if (!err && (node->flags & CHANNEL_READONLY))
    err = argz_add (argz, argz_len, "--readonly");

  if (!err && (node->flags & CHANNEL_WRITEONLY))
    err = argz_add (argz, argz_len, "--writeonly");

  if (! err)
    err = channel_parsed_append_args (node->channel_name, argz, argz_len);

  return err;
}

/* Called whenever a new lookup is done of our node.  The only reason we
   set this hook is to duplicate the check done normally done against
   trivfs_allow_open in trivfs_S_fsys_getroot, but looking at the
   per-device state.  This gets checked again in check_open_hook, but this
   hook runs before a little but more overhead gets incurred.  In the
   success case, we just return EAGAIN to have trivfs_S_fsys_getroot
   continue with its generic processing.  */
static error_t
getroot_hook (struct trivfs_control *cntl,
	      mach_port_t reply_port,
	      mach_msg_type_name_t reply_port_type,
	      mach_port_t dotdot,
	      uid_t *uids, u_int nuids, uid_t *gids, u_int ngids,
	      int flags,
	      retry_type *do_retry, char *retry_name,
	      mach_port_t *node, mach_msg_type_name_t *node_type)
{
  return node_check_perms (cntl->hook, flags) ? EAGAIN : EPERM;
}

/* Called whenever someone tries to open our node (even for a stat).  We
   delay creating the hub until this point, as we can usefully return
   errors from here.  */
static error_t
check_open_hook (struct trivfs_control *trivfs_control,
		 struct iouser *user,
		 int flags)
{
  struct node *node = trivfs_control->hook;
  struct channel_hub *hub;
  error_t err;

  if (node->hub)
    return 0;

  if (node->channel_name == 0)
    /* This means we had no channel arguments.
       We are to operate on our underlying node. */
    err = channel_fetch_hub (channelio_fsys->underlying, flags, 0, &hub);
  else
    /* Create hub based on the previously parsed channel arguments.  */
    err = channel_create_parsed_hub (node->channel_name, flags, &hub);

  if (!err && !node_check_perms (node, flags))
    err = EPERM;

  if (err)
    return err;

  mutex_lock (&node->lock);
  node->hub = hub;
  mutex_unlock (&node->lock);

  return 0;
}

static error_t
open_hook (struct trivfs_peropen *po)
{
  struct open *open = po->hook;
  error_t err;

  err = node_open (po->cntl->hook, modes_to_flags (po->openmodes), &open);
  po->hook = open;

  return err;
}

static void
close_hook (struct trivfs_peropen *po)
{
  node_close (po->cntl->hook, po->hook);
}

/* Trivfs hooks.  */

int trivfs_fstype = FSTYPE_DEV;
int trivfs_fsid = 0;

int trivfs_support_read = 1;
int trivfs_support_write = 1;
int trivfs_support_exec = 0;

int trivfs_allow_open = O_READ | O_WRITE;

void
trivfs_modify_stat (struct trivfs_protid *cred, struct stat *st)
{
  struct node *node = cred->po->cntl->hook;
  int flags = node_flags (node);

  st->st_mode &= ~S_IFMT;
  st->st_size = 0;
  st->st_blksize = 1;
  st->st_mode |= S_IFCHR;
  st->st_rdev = node->rdev;

  if (flags & CHANNEL_READONLY)
    st->st_mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);

  if (flags & CHANNEL_WRITEONLY)
    st->st_mode &= ~(S_IRUSR | S_IRGRP | S_IROTH);
}

error_t
trivfs_goaway (struct trivfs_control *fsys, int flags)
{
  struct node *const node = fsys->hook;
  error_t err;
  int force = (flags & FSYS_GOAWAY_FORCE);
  int sync = ! (flags & FSYS_GOAWAY_NOSYNC);
  struct port_class *root_port_class = fsys->protid_class;

  mutex_lock (&node->lock);

  /* Wait until all pending rpcs are done.  */
  err = ports_inhibit_class_rpcs (root_port_class);
  if (err == EINTR || (err && !force))
    {
      mutex_unlock (&node->lock);
      return err;
    }

  if (!force && ports_count_class (root_port_class) > 0)
    /* Still users, so don't exit.  */
    {
      /* Allow normal operations to proceed.  */
      ports_enable_class (root_port_class);
      ports_resume_class_rpcs (root_port_class);
      mutex_unlock (&node->lock);

      /* Complain that there are still users.  */
      return EBUSY;
    }

  if (sync)
    {
      node_sync (node);

      /* Closing hub could cause pending data to be written back.  */
      if (node->hub != NULL)
	channel_free_hub (node->hub);
    }

  exit (0);
}

/* If this variable is set, it is called by trivfs_S_fsys_getroot before any
   other processing takes place; if the return value is EAGAIN, normal trivfs
   getroot processing continues, otherwise the rpc returns with that return
   value.  */
error_t (*trivfs_getroot_hook) (struct trivfs_control *cntl,
				mach_port_t reply_port,
				mach_msg_type_name_t reply_port_type,
				mach_port_t dotdot,
				uid_t *uids, u_int nuids, uid_t *gids, u_int ngids,
				int flags,
				retry_type *do_retry, char *retry_name,
				mach_port_t *node, mach_msg_type_name_t *node_type)
     = getroot_hook;

/* If this variable is set, it is called every time an open happens.
   USER and FLAGS are from the open; CNTL identifies the
   node being opened.  This call need not check permissions on the underlying
   node.  If the open call should block, then return EWOULDBLOCK.  Other
   errors are immediately reflected to the user.  If O_NONBLOCK
   is not set in FLAGS and EWOULDBLOCK is returned, then call
   trivfs_complete_open when all pending open requests for this
   file can complete. */
error_t (*trivfs_check_open_hook)(struct trivfs_control *trivfs_control,
				  struct iouser *user,
				  int flags)
     = check_open_hook;

/* If this variable is set, it is called every time a new peropen
   structure is created and initialized. */
error_t (*trivfs_peropen_create_hook)(struct trivfs_peropen *) = open_hook;

/* If this variable is set, it is called every time a peropen structure
   is about to be destroyed. */
void (*trivfs_peropen_destroy_hook) (struct trivfs_peropen *) = close_hook;

/* Sync this filesystem.  */
error_t
trivfs_S_fsys_syncfs (struct trivfs_control *cntl,
		      mach_port_t reply, mach_msg_type_name_t replytype,
		      int wait, int dochildren)
{
  struct node *node = cntl->hook;
  if (node)
    return node_sync (node);
  else
    return 0;
}
