/* Basic read/write test.

   Copyright (C) 2007 Free Software Foundation, Inc.

   Written by Carl Fredrik Hammar <hammy.lite@gmail.com>

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

#include <error.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>

#include "channel.h"

#define MIN(n, m) ((n) < (m) ? (n) : (m))

struct buf
{
  size_t len;
  char data[];
};

static error_t
buf_realloc (struct buf **buf, size_t len)
{
  struct buf *new_buf = realloc (*buf, len + sizeof (struct buf));
  if (!new_buf)
    return ENOMEM;

  new_buf->len = len;
  *buf = new_buf;
  return 0;
}

/* An implementation of a trivial fifo channel, where first written is
   the first to be read, no matter who read it.  Not really a channel
   at all and only useful for testing. */

static error_t
triv_fifo_read (struct channel *channel,
		mach_msg_type_number_t amount,
		void **buf, mach_msg_type_number_t *len)
{
  struct buf *fifo_buf = channel->hook;
  int n = MIN (amount, fifo_buf->len);
  size_t new_len = fifo_buf->len - n;
  int alloced = 0;
  error_t err;
  
  if (*len < n)
    {
      *buf = mmap (0, n, PROT_READ|PROT_WRITE, MAP_ANON, 0, 0);
      if (*buf == MAP_FAILED)
	return errno;
      alloced = 1;
    }

  memcpy (*buf, fifo_buf->data, n);
  *len = n;

  memmove (fifo_buf->data, fifo_buf->data + n, new_len);
  err = buf_realloc (&fifo_buf, new_len);
  if (err)
    {
      if (alloced)
	munmap (*buf, n);
      return err;
    }

  return 0;
}

static error_t
triv_fifo_write (struct channel *channel,
		 const void *buf, mach_msg_type_number_t len,
		 mach_msg_type_number_t *amount)
{
  struct buf *fifo_buf = channel->hook;
  size_t old_len = fifo_buf->len;
  size_t new_len = old_len + len;
  error_t err;

  err = buf_realloc (&fifo_buf, new_len);
  if (err)
    return err;

  memcpy (fifo_buf->data + old_len, buf, len);
  *amount = len;

  return 0;
}

static void
triv_fifo_cleanup (struct channel *fifo)
{
  free (fifo->hook);
}

const struct channel_class triv_fifo_class =
{
  "", triv_fifo_read, triv_fifo_write, 0, 0, triv_fifo_cleanup
};

error_t
triv_fifo_create (int flags, struct channel **channel)
{
  error_t err;
  struct buf *buf;

  err = channel_create (&triv_fifo_class, flags | CHANNEL_INNOCUOUS,
			channel);
  if (err)
    return err;

  buf = malloc (sizeof (struct buf));
  if (!buf)
    {
      channel_free (*channel);
      return ENOMEM;
    }

  buf->len = 0;

  (*channel)->hook = buf;
  return 0;
}

static error_t
do_reads (struct channel *fifo, void *buf, size_t len, size_t step)
{
  void *p, *end = buf + len;
  size_t n;
  error_t err;

  for (p = buf, n = 1; p < end && n > 0; p += n)
    {
      void *q = p;
      n = MIN (end - p, step);

      err = channel_read (fifo, n, &q, &n);
      if (err)
	return err;

      if (q != p)
	memcpy (p, q, n);
    }

  return 0;
}

static error_t
do_writes (struct channel *fifo, void *buf, size_t len, size_t step)
{
  void *p, *end = buf + len;
  size_t n;
  error_t err;

  for (p = buf, n = 0; p < end; p += n)
    {
      n = MIN (end - p, step);
      err = channel_write (fifo, p, n, &n);
      if (err)
	return err;
    }

  return 0;
}

int
main (int argc, char **argv)
{
  struct channel *fifo;
  void *data, *buf;
  size_t len = 128 * 1024;
  int i;
  error_t err;

  data = mmap (0, len, PROT_READ|PROT_WRITE, MAP_ANON, 0, 0);
  if (data == MAP_FAILED)
    error (1, errno, "main");

  err = triv_fifo_create (0, &fifo);
  if (err)
    error (1, err, "triv_fifo_create");

  /* Fill test data.  */
  for (i = 0; i < len; i++)
    ((char*) data)[i] = i;

  /* Small writes.  */
  err = do_writes (fifo, data, 4 * 1024, 32);
  if (err)
    error (1, err, "triv_fifo_write (small)");
  
  /* Large writes.  */
  err = do_writes (fifo, data + 4 * 1024, len - 4 * 1024, 4 * 1024);
  if (err)
    error (1, err, "triv_fifo_write (large)");

  buf = mmap (0, len, PROT_READ|PROT_WRITE, MAP_ANON, 0, 0);
  if (buf == MAP_FAILED)
    error (1, errno, "main");

  /* Small reads.  */
  err = do_reads (fifo, buf, 4 * 1024, 32);
  if (err)
    error (1, err, "triv_fifo_read (small)");
  
  /* Large reads.  */
  err = do_reads (fifo, buf + 4 * 1024, len - 4 * 1024, 4 * 1024);
  if (err)
    error (1, err, "triv_fifo_read (large)");

  if (memcmp (data, buf, len) != 0)
    error (1, 0, "written data inconsistent with read data");
    
  return 0;
}
