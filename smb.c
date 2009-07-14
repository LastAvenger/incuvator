/*
  Copyright (C) 2004, 2007, 2009 Free Software Foundation, Inc.
  Copyright (C) 2004, 2007, 2009 Giuseppe Scrivano.
  Written by Giuseppe Scrivano <gscrivano@gnu.org>
  
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 3, or (at
  your option) any later version.
  
  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "smb.h"

void
auth_data_fn (const char *server, const char *share, char *workgroup,
              int wgmaxlen, char *username, int unmaxlen, char *password,
              int pwmaxlen)
{
  if (strcmp (server, credentials.server))
    return;
  strncpy (workgroup, credentials.workgroup, wgmaxlen);
  strncpy (username, credentials.username, unmaxlen);
  strncpy (password, credentials.password, pwmaxlen);
}

int
init_smb ()
{
  int ret = smbc_init (auth_data_fn, 10);
  return ret;
}
