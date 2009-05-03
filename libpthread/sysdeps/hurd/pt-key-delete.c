/* pthread_key_delete.  Hurd version.
   Copyright (C) 2002 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include <pthread.h>

#include <pt-internal.h>

int
pthread_key_delete (pthread_key_t key)
{
  error_t err = 0;

  __pthread_key_lock_ready ();

  __pthread_mutex_lock (&__pthread_key_lock);

  if (key < 0 || key >= __pthread_key_count
      || __pthread_key_destructors[key] == PTHREAD_KEY_INVALID)
    err = EINVAL;
  else
    {
      __pthread_key_destructors[key] = PTHREAD_KEY_INVALID;
      __pthread_key_invalid_count ++;
    }

  __pthread_mutex_unlock (&__pthread_key_lock);

  return err;
}
