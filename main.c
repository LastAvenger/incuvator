/* netio - creates socket ports via the filesystem
   Copyright (C) 2001, 02 Moritz Schulte <moritz@duesseldorf.ccc.de>
 
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or * (at your option) any later version.
 
   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
 
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
   USA */

#include <hurd.h>
#include <hurd/netfs.h>
#include <hurd/paths.h>
#include <argp.h>
#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#include "netio.h"
#include "lib.h"
#include "node.h"
#include "protocol.h"

#include "version.h"

const char *argp_program_version = STANDARD_HURD_VERSION (netio);
const char *argp_program_bug_address =
"Moritz Schulte <moritz@duesseldorf.ccc.de>";
const char *doc = "Hurd netio translator v" NETIO_VERSION;
char *netfs_server_name = "netio";
char *netfs_server_version = HURD_VERSION;

/* The underlying node.  */
mach_port_t ul_node;

/* The socket server, PF_INET for us.  */
pf_t socket_server;

/* Has to be defined for libnetfs...  */
int netfs_maxsymlinks = 0;

/* Used for updating node information.  */
volatile struct mapped_time_value *netio_maptime;

/* argp options.  */
static const struct argp_option netio_options[] =
{
  { 0 }
};

/* argp option parser.  */
static error_t
parse_opts (int key, char *arg, struct argp_state *sate)
{
  return 0;
}

/* Main.  */
int
main (int argc, char **argv)
{
  struct argp netio_argp = { netio_options, parse_opts,
			     NULL, doc, NULL };
  mach_port_t bootstrap_port;
  error_t err;

  argp_parse (&netio_argp, argc, argv, 0, 0, 0);
  task_get_bootstrap_port (mach_task_self (), &bootstrap_port);
  netfs_init ();
  ul_node = netfs_startup (bootstrap_port, 0);

  err = node_make_root_node (&netfs_root_node);
  if (err)
    error (EXIT_FAILURE, err, "cannot create root node");

  {
    /* Here we adjust the root node permissions.  */
    struct stat ul_node_stat;
    err = io_stat (ul_node, &ul_node_stat);
    if (err)
      error (EXIT_FAILURE, err, "cannot stat underlying node");
    netfs_root_node->nn_stat.st_fsid = getpid ();
    netfs_root_node->nn_stat = ul_node_stat;
    netfs_root_node->nn_stat.st_mode = S_IFDIR | (ul_node_stat.st_mode
						  & ~S_IFMT & ~S_ITRANS);

    /* If the underlying node isn't a directory, enhance the stat
       information, if needed.  */
    if (! S_ISDIR (ul_node_stat.st_mode))
      {
	if (ul_node_stat.st_mode & S_IRUSR)
	  netfs_root_node->nn_stat.st_mode |= S_IXUSR;
	if (ul_node_stat.st_mode & S_IRGRP)
	  netfs_root_node->nn_stat.st_mode |= S_IXGRP;
	if (ul_node_stat.st_mode & S_IROTH)
	  netfs_root_node->nn_stat.st_mode |= S_IXOTH;
      }
  }

  err = maptime_map (0, 0, &netio_maptime);
  if (err)
    error (EXIT_FAILURE, err, "cannot map time");

  fshelp_touch (&netfs_root_node->nn_stat,
		TOUCH_ATIME|TOUCH_MTIME|TOUCH_CTIME,
		netio_maptime);

  err = open_socket_server (PF_INET, &socket_server);
  if (err)
    error (EXIT_FAILURE, err, "open_socket_server");

  err = protocol_register_protocols ();
  if (err)
    error (EXIT_FAILURE, err, "protocol_register_protocols");

  for (;;)
    netfs_server_loop ();

  /* Never reached.  */
  exit (0);
}
