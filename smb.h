/*

   Copyright (C) 2004 Free Software Foundation, Inc.
   Written by Giuseppe Scrivano <gscrivano@quipo.it>
   
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. 
  */
#define _GNU_SOURCE 1
#define _FILE_OFFSET_BITS 64
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <maptime.h>
#include <errno.h>
#include <error.h>
#include <argp.h>
#include <hurd/netfs.h>
#include <libsmbclient.h>

struct smb_credentials
{
  char *server;
  char *share;
  char *workgroup;
  char *username;
  char *password;
};
extern struct smb_credentials credentials;

extern int init_smb ();
extern void stop_netsmb ();
