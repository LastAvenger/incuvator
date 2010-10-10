/* Gopher filesystem

   Copyright (C) 2002 James A. Morrison <ja2morri@uwaterloo.ca>
   Copyright (C) 2000 Igor Khavkine <igor@twu.net>
   This file is part of the Gopherfs translator.

   Gopherfs is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   Gopherfs is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA. */

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <maptime.h>
#include <errno.h>
#include <error.h>
#include <argp.h>
#include <argz.h>

#include <hurd/netfs.h>

#include "gopherfs.h"
#include "fs.h"

char *netfs_server_name = GOPHER_SERVER_NAME;
char *netfs_server_version = GOPHER_SERVER_VERSION;
struct gopherfs *gopherfs;	/* filesystem global pointer */
volatile struct mapped_time_value *gopherfs_maptime;

int
main (int argc, char **argv)
{
  error_t err;
  mach_port_t bootstrap, underlying_node;
  struct stat underlying_stat;
  struct gopherfs_params *gopherfs_params = malloc (sizeof (struct gopherfs_params));
  memset (gopherfs_params, 0, sizeof (struct gopherfs_params));
  if (!gopherfs_params)
    error (1, errno, "Couldn't create params structure");

  // Parse arguments and fill GOPHERFS_PARAMS.
  gopherfs_parse_args (argc, argv, gopherfs_params);
  debug ("pid %d", getpid ());

  task_get_bootstrap_port (mach_task_self (), &bootstrap);
  if (bootstrap == MACH_PORT_NULL)
    error (1, 0, "Must be started as a translator");

  netfs_init ();

  err = maptime_map (0, 0, &gopherfs_maptime);
  if (err)
    error (1, err, "Error mapping time.");

  // Create main gopherfs object.
  err = gopherfs_create (gopherfs_params);
  if (err)
    error (1, err, "Couldn't create gopherfs structure");

  netfs_root_node = gopherfs->root;

  underlying_node = netfs_startup (bootstrap, 0);

  err = io_stat (underlying_node, &underlying_stat);
  if (err)
    error (1, err, "cannot stat underlying node");

  /* Initialize stat information of the root node.  */
  netfs_root_node->nn_stat = underlying_stat;
  netfs_root_node->nn_stat.st_mode =
    S_IFDIR | (underlying_stat.st_mode & ~S_IFMT & ~S_ITRANS);

  /* If the underlying node isn't a directory, propagate read permission to                                   
     execute permission since we need that for lookups.  */
  if (! S_ISDIR (underlying_stat.st_mode))
    {
      if (underlying_stat.st_mode & S_IRUSR)
        netfs_root_node->nn_stat.st_mode |= S_IXUSR;
      if (underlying_stat.st_mode & S_IRGRP)
        netfs_root_node->nn_stat.st_mode |= S_IXGRP;
      if (underlying_stat.st_mode & S_IROTH)
        netfs_root_node->nn_stat.st_mode |= S_IXOTH;
    }

  debug("entering the main loop");

  for (;;)
    netfs_server_loop ();

  return 0;
}


/*
 * vim:ts=2:sw=2:autoindent:cindent:
 */
