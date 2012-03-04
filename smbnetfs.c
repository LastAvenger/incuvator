/*
  Copyright (C) 1997, 2002, 2004, 2007, 2009 Free Software Foundation, Inc.
  Copyright (C) 2004, 2007, 2009 Giuseppe Scrivano.
  Written by Giuseppe Scrivano <gscrivano@gnu.org>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2, or (at
  your option) any later version.
  
  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "smb.h"
#include <stddef.h>
#include <sys/mman.h>
#include <hurd/fsys.h>
#include <dirent.h>
#include <sys/time.h>
#include <maptime.h>
#include <cthreads.h>

/* Returned directory entries are aligned to blocks this many bytes long.
   Must be a power of two.  */
#define DIRENT_ALIGN 4
#define DIRENT_NAME_OFFS offsetof (struct dirent, d_name)
/* Length is structure before the name + the name + '\0', all
   padded to a four-byte alignment.  */
#define DIRENT_LEN(name_len)    ((DIRENT_NAME_OFFS + (name_len) + 1 \
                               + (DIRENT_ALIGN - 1)) & ~(DIRENT_ALIGN - 1))

struct smb_credentials credentials =
  {
    .workgroup = "WORKGROUP",
    .password = ""
  };

static volatile struct mapped_time_value *maptime;
static struct mutex smb_mutex;

char *netfs_server_name = "smbfs";
char *netfs_server_version = "0.1";
int netfs_maxsymlinks = 0;

struct netnode
{
  struct node *node;
  char *filename;
  struct netnode *parent;
  struct netnode *next;
};

static struct netnode  *nodes;

/* Free the memory used by the nodes.  */
static void
clear_nodes ()
{
  if (!nodes)
    return;
  struct netnode *pt = nodes;
  struct netnode *pt2 = 0;

  for (;;)
    {
      pt2 = pt;
      if (pt2)
        {
          pt = pt->next;
          free (pt2->node);
          free (pt2->filename);
          free (pt2);
        }
      else
        break;
    }
  nodes = 0;
}

static void
append_node_to_list (struct netnode *n)
{
  n->next = nodes;
  nodes = n;
}

/* Create a new node and initialize it with default values.  */
static int
create_node (struct node **node)
{
  struct netnode *n = malloc (sizeof (struct netnode));

  if(!n)
  	return ENOMEM;

  *node = n->node = netfs_make_node (n);
  if (!(*node))
    {
      free (n);
      return ENOMEM;
    }  
  append_node_to_list (n);
  return 0;
}

static struct netnode *
search_node (char *filename, struct node *dir)
{
  struct netnode *pt = nodes;

  while (pt)
    {
      if ((pt->parent)  && (dir == pt->parent->node)
          && !strcmp (pt->filename, filename))
            return pt;

      pt = pt->next;
    }

  return 0;
}

static void
remove_node (struct node *np)
{
  struct netnode *pt;
  struct netnode *prevpt;

  for ( pt=nodes, prevpt=0 ; pt ; pt=pt->next )
    {
      if (pt->node == np)
        {
          free (pt->node);
          free (pt->filename);
          free (pt);

          if (prevpt)
            prevpt->next = pt->next;
          else
            nodes = pt->next;

          break;
        }
      else
        prevpt = pt;
    }
}

static void
create_root_node ()
{
  struct node *node;
  int err = create_node (&node);
  if (err)
    return;  
  netfs_root_node = node;
  node->nn->parent = 0;
  node->nn->filename = malloc (strlen (credentials.share) + 1);
  if (node->nn->filename)
    strcpy (node->nn->filename, credentials.share);
  
  netfs_validate_stat (node, 0);
  
  }


static int
add_node (char *filename, struct node *top ,struct netnode** nn)
{
  int err;
  struct netnode *n;
  struct node *newnode;
  io_statbuf_t  st;

  n = search_node (filename, top);
  if (n)
    {
      *nn = n;
      return 0;
    }
    
  err = create_node (&newnode);
  if (err)
    return err;
  n = newnode->nn;
  n->node = newnode;

  if (top)
    n->parent = top->nn;
  else
    n->parent = 0;

  n->filename = malloc ( strlen(top->nn->filename) + strlen (filename) + 1);
  if (!n->filename)
    return 0;

  sprintf (n->filename, "%s/%s", top->nn->filename, filename);

  mutex_lock (&smb_mutex);  
  err = smbc_stat (n->filename, &st);
  mutex_unlock (&smb_mutex);  
    
  if (err)
    return errno;
  
  /* Consider only directories and regular files.  */
  if (((st.st_mode &  S_IFDIR) == 0)  && ((st.st_mode &  S_IFREG) == 0))
    err=-1;
  if(err)
    { 
      remove_node (newnode);
      return errno;
    }
    
  *nn = n;
  return 0;
}

/* Return a zeroed stat buffer for CRED.  */
static struct stat
empty_stat (struct iouser *cred)
{
  struct stat st;

  st.st_fstype = FSTYPE_MISC;
  st.st_fsid = getpid ();
  st.st_ino = 0;
  st.st_dev = st.st_rdev = 0;
  st.st_size = 0;
  st.st_blksize = 0;
  st.st_blocks = 0;
  st.st_mode = 0;
  st.st_nlink = 0;
  st.st_atime = st.st_mtime = st.st_ctime = 0;
  st.st_uid = cred->uids->num > 0 ? cred->uids->ids[0] : -1;
  st.st_gid = cred->gids->num > 0 ? cred->gids->ids[0] : -1;

  return st;
}


error_t
netfs_validate_stat (struct node * np, struct iouser *cred)
{
  mutex_lock (&smb_mutex);    
  int err = smbc_stat (np->nn->filename, &np->nn_stat);
  mutex_unlock (&smb_mutex);
  if (err)
    return errno;
    
  return 0;
}

error_t
netfs_attempt_chown (struct iouser * cred, struct node * np, uid_t uid,
		     uid_t gid)
{
  return EOPNOTSUPP;
}

error_t
netfs_attempt_chauthor (struct iouser * cred, struct node * np, uid_t author)
{
  return EOPNOTSUPP;
}

error_t
netfs_attempt_chmod (struct iouser * cred, struct node * np, mode_t mode)
{
  int err;
  mutex_lock (&smb_mutex);    
  err=smbc_chmod (np->nn->filename,mode);
  mutex_unlock (&smb_mutex);  

  if (err)
    return errno;
  else 
    return 0;
}

error_t
netfs_attempt_mksymlink (struct iouser * cred, struct node * np, char *name)
{
  return EOPNOTSUPP;
}

error_t
netfs_attempt_mkdev (struct iouser * cred, struct node * np, mode_t type,dev_t indexes)
{
  return EOPNOTSUPP;
}

error_t
netfs_set_translator (struct iouser * cred, struct node * np, char *argz,size_t argzlen)
{
  return 0;
}

error_t
netfs_get_translator (struct node * node, char **argz, size_t * argz_len)
{
  return 0;
}

error_t
netfs_attempt_chflags (struct iouser * cred, struct node * np, int flags)
{
  return EOPNOTSUPP;
}

error_t
netfs_attempt_utimes (struct iouser * cred, struct node * np,
		      struct timespec * atime, struct timespec * mtime)
{
  int err;
  struct timeval tv;
  tv.tv_usec = 0;/* Not used by samba.  */
  if (mtime)
    tv.tv_sec = mtime->tv_sec;
  else
    maptime_read (maptime, &tv);

  mutex_lock (&smb_mutex);    
  err = smbc_utimes (np->nn->filename, &tv);
  mutex_unlock (&smb_mutex);

  if(err)
    return errno;
  else 
    return 0;
}

error_t
netfs_attempt_set_size (struct iouser * cred, struct node * np, loff_t size)
{
  int ret = 0;
  int fd;
  int current_filesize;
  
  mutex_lock (&smb_mutex);  
  fd = smbc_open (np->nn->filename, O_WRONLY | O_CREAT, O_RDWR);
  mutex_unlock (&smb_mutex);  

  if (fd < 0)
    return errno;

  mutex_lock (&smb_mutex);  

  current_filesize = smbc_lseek (fd, 0, SEEK_END);

  mutex_unlock (&smb_mutex);    

  if (current_filesize < 0)
    {
      mutex_lock (&smb_mutex);
      smbc_close (fd);
      mutex_unlock (&smb_mutex);
      return errno;    
    }

  if (current_filesize < size)
    {
      /* FIXME. trunc here.  */  
      mutex_lock (&smb_mutex);
      smbc_close (fd);
      fd = smbc_open (np->nn->filename, O_WRONLY | O_TRUNC, O_RDWR);
      mutex_unlock (&smb_mutex);
      current_filesize = 0;
    }
  mutex_lock (&smb_mutex);
  ret=smbc_lseek (fd, size, SEEK_SET);
  mutex_unlock (&smb_mutex);

  if (ret < 0)
    {
      mutex_lock (&smb_mutex);
      smbc_close (fd);
      mutex_unlock (&smb_mutex);
      return errno;
    }

  mutex_lock (&smb_mutex);
  smbc_close (fd);
  mutex_unlock (&smb_mutex);
  return 0;

}

error_t
netfs_attempt_statfs (struct iouser * cred, struct node * np,
		      fsys_statfsbuf_t * st)
{
  return EOPNOTSUPP;
}

error_t
netfs_attempt_sync (struct iouser * cred, struct node * np, int wait)
{
  return 0;
}

error_t
netfs_attempt_syncfs (struct iouser * cred, int wait)
{
  return 0;
}

error_t
netfs_attempt_lookup (struct iouser * user, struct node * dir, char *name,
		      struct node ** np)
{
  struct netnode *n;
  error_t err = 0;

  if (*name == '\0' || strcmp (name, ".") == 0)	/*Current directory */
    {
      netfs_nref (dir);/*Add a reference to current directory */
      *np = dir;
      mutex_unlock (&dir->lock);
      return 0;
    }
  else if (strcmp (name, "..") == 0)	/*Parent directory */
    {
      if (dir->nn->parent)
        {
          *np = dir->nn->parent->node;
          if (*np)
            {
              netfs_nref (*np);
              err = 0;
            }
          else
            err = ENOENT;
        }
      else
        {
          err = ENOENT;
          *np = 0;
        }

      mutex_unlock (&dir->lock);
      return err;    
    }

  mutex_unlock (&dir->lock);
  
  err = add_node (name, dir,&n);

  if(err)
    return err;

  *np = n->node;
  netfs_nref (*np);

  if (*np)
    err = 0;

  return err;
}

error_t
netfs_attempt_unlink (struct iouser * user, struct node * dir, char *name)
{
  char *filename;

  if (dir->nn->filename)
    filename = malloc (strlen (dir->nn->filename) + strlen (name) + 2);
  else
    filename = malloc (strlen (credentials.share) + strlen (name) + 1);

  if (!filename)
    return ENOMEM;

  if (dir->nn->filename)
    sprintf (filename, "%s/%s", dir->nn->filename,  name);
  else
    sprintf (filename, "%s/%s", credentials.share, name);

  mutex_lock (&smb_mutex);
  error_t err = smbc_unlink (filename);
  mutex_unlock (&smb_mutex);  
  
  free (filename);

  if (err)
    return errno;
  else
    return 0;
}

error_t
netfs_attempt_rename (struct iouser * user, struct node * fromdir,
		      char *fromname, struct node * todir, char *toname,
		      int excl)
{
  char *filename;		/* Origin file name.  */
  char *filename2;		/* Destination file name.  */

  if (fromdir->nn->filename)
    filename = malloc (strlen (fromdir->nn->filename) + strlen (fromname) + 2);
  else
    filename = malloc (strlen (credentials.share) + strlen (fromname) + 1);

  if (!filename)
    return ENOMEM;

  if (todir->nn->filename)
    filename2 =malloc (strlen (todir->nn->filename) + strlen (toname) + 2);
  else
    filename2 = malloc (strlen (credentials.share) + strlen (toname) + 1);

  if (!filename2)
    {
      free (filename);
      return ENOMEM;
    }

  if (fromdir->nn->filename)
    sprintf (filename, "%s/%s", fromdir->nn->filename,fromname);
  else
    sprintf (filename, "%s/%s", credentials.share, fromname);

  if (todir->nn->filename)
    sprintf (filename, "%s/%s", todir->nn->filename, toname);
  else
    sprintf (filename, "%s/%s", credentials.share, toname);

  mutex_lock (&smb_mutex);
  error_t err = smbc_rename (filename, filename2);
  mutex_unlock (&smb_mutex);  
  
  free (filename);
  free (filename2);
  return err ? errno : 0;
}

error_t
netfs_attempt_mkdir (struct iouser * user, struct node * dir, char *name,
		     mode_t mode)
{
  char *filename;
  error_t err;

  if (dir->nn->filename)
    filename = malloc (strlen (dir->nn->filename) + strlen (name) + 2);
  else
    filename = malloc (strlen (credentials.share) + strlen (name) + 1);
  if (!filename)
    return ENOMEM;

  if (dir->nn->filename)
    sprintf (filename, "%s/%s", dir->nn->filename,name);
  else
    sprintf (filename, "%s/%s", credentials.share, name);

  mutex_lock (&smb_mutex);
  err = smbc_mkdir (filename, mode);
  mutex_unlock (&smb_mutex);  
  
  free (filename);
  return err ? errno : 0;
}

error_t
netfs_attempt_rmdir (struct iouser * user, struct node * dir, char *name)
{
  char *filename;
  error_t err;

  if (dir->nn->filename)
    filename = malloc (strlen (dir->nn->filename) +strlen (name) + 2);
  else
    filename = malloc (strlen (credentials.share) + strlen (name) + 1);
  if (!filename)
    return ENOMEM;

  if (dir->nn->filename)
    sprintf (filename, "%s/%s", dir->nn->filename,    name);
  else
    sprintf (filename, "%s/%s", credentials.share, name);

  mutex_lock (&smb_mutex);
  err = smbc_rmdir (filename);
  mutex_unlock (&smb_mutex);  

  free(filename);
  return err ? errno : 0;
}

error_t
netfs_attempt_link (struct iouser * user, struct node * dir,
		    struct node * file, char *name, int excl)
{
  return EOPNOTSUPP;
}

error_t
netfs_attempt_mkfile (struct iouser * user, struct node * dir, mode_t mode,
		      struct node ** np)
{
  mutex_unlock (&dir->lock);
  return EOPNOTSUPP;
}

error_t
netfs_attempt_create_file (struct iouser * user, struct node * dir,
			   char *name, mode_t mode, struct node ** np)
{
  error_t err = 0;
  char *filename;
  struct netnode *nn;
  int fd;
  *np = 0;

  if (dir->nn->filename)
    filename = malloc ( strlen (dir->nn->filename) +strlen (name) + 2);
  else
    filename = malloc (strlen (credentials.share) + strlen (name) + 1);
  if (!filename)
    return ENOMEM;

  if (dir->nn->filename)
    sprintf (filename, "%s/%s", dir->nn->filename,name);
  else
    sprintf (filename, "%s/%s", credentials.share, name);

  mutex_lock (&smb_mutex);
  fd = smbc_open (filename,O_WRONLY | O_CREAT , O_RDWR);
  if (fd < 0)
    {
      mutex_unlock (&smb_mutex);  
      mutex_unlock (&dir->lock);
      return errno;
    }
  smbc_close (fd);
  mutex_unlock (&smb_mutex);    

  err = add_node (name, dir, &nn);

  if (err)
    {
      mutex_unlock (&dir->lock);
      return err;
    }

  *np = nn->node;

  mutex_unlock (&dir->lock);
  return 0;
}

error_t
netfs_attempt_readlink (struct iouser * user, struct node * np, char *buf)
{
  return EOPNOTSUPP;
}

error_t
netfs_check_open_permissions (struct iouser * user, struct node * np,
			      int flags, int newnode)
{
  error_t err;
  io_statbuf_t  nn_stat;
  
  mutex_lock (&smb_mutex);
  err = smbc_stat (np->nn->filename, &nn_stat);
  mutex_unlock (&smb_mutex);
   
  if (err)
    return errno;

  if (flags & O_READ)
    err = !(S_IREAD & nn_stat.st_mode);
  if (flags & O_WRITE)
    err |= !(S_IWRITE & nn_stat.st_mode);
  if (flags & O_EXEC)
    err |= !(S_IEXEC & nn_stat.st_mode);

  return err?EPERM:0;
}

error_t
netfs_attempt_read (struct iouser * cred, struct node * np, loff_t offset,
		    size_t * len, void *data)
{
  int fd;
  int ret = 0;

  mutex_lock (&smb_mutex);
  fd = smbc_open (np->nn->filename, O_RDONLY, O_RDWR);
  mutex_unlock (&smb_mutex);

  if (fd < 0)
    {
      *len = 0;
      return errno;
    }

  mutex_lock (&smb_mutex);
  ret = smbc_lseek (fd, offset, SEEK_SET);
  mutex_unlock (&smb_mutex);
  
  if ((ret < 0) || (ret != offset))
    {
      *len = 0;
      mutex_lock (&smb_mutex);
      smbc_close (fd);
      mutex_unlock (&smb_mutex);
      return errno;
    }

  mutex_lock (&smb_mutex);
  ret = smbc_read (fd, data, *len);
  mutex_unlock (&smb_mutex);

  if (ret < 0)
    {
      *len = 0;
      mutex_lock (&smb_mutex);
      smbc_close (fd);
      mutex_unlock (&smb_mutex);
      return errno;
    }

  *len = ret;
  mutex_lock (&smb_mutex);
  smbc_close (fd);
  mutex_unlock (&smb_mutex);
  return 0;
}

error_t
netfs_attempt_write (struct iouser * cred, struct node * np, loff_t offset,
		     size_t * len, void *data)
{
  int ret = 0;
  int fd;
  
  mutex_lock (&smb_mutex);
  fd = smbc_open (np->nn->filename, O_WRONLY, O_RDWR);
  mutex_unlock (&smb_mutex);

  if (fd < 0)
    {
      *len = 0;
      return errno;
    }
  mutex_lock (&smb_mutex);
  ret = smbc_lseek (fd, offset, SEEK_SET) < 0;
  mutex_unlock (&smb_mutex);
  
  if ((ret < 0) || (ret != offset))
    {
      *len = 0;
      mutex_lock (&smb_mutex);
      smbc_close (fd);
      mutex_unlock (&smb_mutex);
      return errno;
    }
  mutex_lock (&smb_mutex);
  ret = smbc_write (fd, data, *len);
  mutex_unlock (&smb_mutex);
  
  if (ret < 0)
    {
      *len = 0;
      mutex_lock (&smb_mutex);
      smbc_close (fd);
      mutex_unlock (&smb_mutex);      
      return errno;
    }

  *len = ret;
  mutex_lock (&smb_mutex);
  smbc_close (fd);
  mutex_unlock (&smb_mutex);      

  return 0;
}

error_t
netfs_report_access (struct iouser * cred, struct node * np, int *types)
{
  *types = 0;

  if (fshelp_access (&np->nn_stat, S_IREAD, cred) == 0)
    *types |= O_READ;
  if (fshelp_access (&np->nn_stat, S_IWRITE, cred) == 0)
    *types |= O_WRITE;
  if (fshelp_access (&np->nn_stat, S_IEXEC, cred) == 0)
    *types |= O_EXEC;

  return 0;
}

struct iouser *
netfs_make_user (uid_t * uids, int nuids, uid_t * gids, int ngids)
{
  return 0;
}

void
netfs_node_norefs (struct node *np)
{
  remove_node (np);
}

error_t
netfs_get_dirents (struct iouser *cred, struct node *dir, int entry,
                   int nentries, char **data,
                   mach_msg_type_number_t * datacnt, vm_size_t bufsize,
                   int *amt)
{
  io_statbuf_t  st; 
  struct smbc_dirent * dirent;  
  int size = 0,  dd;
  int nreturningentries = 0;
  int err = 0;
  int add_dir_entry_size = 0;
  char *p = 0;

  if (!dir)
    return ENOTDIR;

  mutex_lock (&smb_mutex);
  dd = smbc_opendir (dir->nn->filename);
  mutex_unlock (&smb_mutex);
  
  if (dd < 0)
    return ENOTDIR;  
  
  mutex_lock (&smb_mutex);
  err = smbc_lseekdir (dd, entry);
  mutex_unlock (&smb_mutex);
  
  if(err)
    {
      if(errno == EINVAL)
        {
          *datacnt = 0;
          *amt = 0;
          mutex_lock (&smb_mutex);
          smbc_closedir (dd);
          mutex_unlock (&smb_mutex);
          return 0;
        }
      return errno;
    }
   
  int
  addSize (char *filename)
    {
      if (nentries == -1 || nreturningentries < nentries)
        {
          size_t new_size = size + DIRENT_LEN (strlen (filename));
          if (bufsize > 0 && new_size > bufsize)
            return 1;
          size = new_size;
          nreturningentries++;
          return 0;
        }
      else
        return 1;
    }

  for(;;)
    {
      mutex_lock (&smb_mutex);      
      dirent = smbc_readdir (dd);
      mutex_unlock (&smb_mutex);

      if(!dirent)
        break;

      /* Add only files and directories.  */
      if ((dirent->smbc_type == SMBC_DIR) || (dirent->smbc_type == SMBC_FILE) ) 
        if (addSize(dirent->name))/* bufsize or nentries reached.  */
          break;
      
    }

  if(size > *datacnt) /* if the supplied buffer isn't large enough.  */
    *data = mmap (0, size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS, 0, 0);

  if (!(*data) ||  (*data == (void *) -1))
    {
      mutex_lock (&smb_mutex);
      smbc_closedir(dd);
      mutex_unlock (&smb_mutex);
      return ENOMEM;    
    }

  mutex_lock (&smb_mutex);
  err=smbc_lseekdir (dd, entry);
  mutex_unlock (&smb_mutex);

  if (err)
    {
      if (errno == EINVAL)
        {
          *datacnt = 0;
          *amt = 0;
          mutex_lock (&smb_mutex);
          smbc_closedir (dd);
          mutex_unlock (&smb_mutex);
          return 0;
        }

      return errno;
    }
      
  add_dir_entry_size = size;
  p = *data;
  int count = 0;
  
  int
  add_dir_entry (const char *name, ino_t fileno, int type)
    {
      if(count < nreturningentries)
        {
          struct dirent hdr;
          size_t name_len = strlen (name);
          size_t sz = DIRENT_LEN (name_len);
  
          if (sz > add_dir_entry_size)
            return 1;
          
          add_dir_entry_size -= sz;
 
          hdr.d_fileno = fileno;
          hdr.d_reclen = sz;
          hdr.d_type = type;
          hdr.d_namlen = name_len;
 
          memcpy (p, &hdr, DIRENT_NAME_OFFS);
          strcpy (p + DIRENT_NAME_OFFS, name);
          p += sz;
          ++count;
          return 0;
      }
    else
      return 1;
    }
 
    for(;;)
      {
        mutex_lock (&smb_mutex);
        dirent = smbc_readdir (dd);
        mutex_unlock (&smb_mutex);
        if (!dirent)
          break;
        int type = 0;
        if (dirent->smbc_type == SMBC_DIR)
          type = DT_DIR;
        else if (dirent->smbc_type == SMBC_FILE)
          type = DT_REG;
        else
          continue;

        char *stat_file_name;
        stat_file_name = malloc (strlen (dir->nn->filename)
                                 + strlen (dirent->name) + 2);
        if(!stat_file_name)
          {
            mutex_lock (&smb_mutex);
            smbc_closedir(dd);
            mutex_unlock (&smb_mutex);
            return ENOMEM;
          }                      
    
        if (!strcmp (dirent->name, "."))
          {
            sprintf (stat_file_name, "%s", dir->nn->filename);
            mutex_lock (&smb_mutex);
            err=smbc_stat (stat_file_name, &st);
            mutex_unlock (&smb_mutex);
          }
        else if (!strcmp (dirent->name,".."))
          {
            if (dir->nn->parent)
              sprintf (stat_file_name, "%s/%s", dir->nn->filename, dirent->name);
            else
              st.st_ino = 0;
          }
        else
          {
            sprintf (stat_file_name,"%s/%s", dir->nn->filename, dirent->name);
            mutex_lock (&smb_mutex);
            err = smbc_stat (stat_file_name, &st);
            mutex_unlock (&smb_mutex);

	    if (err)
	      {
		/* STAT_FILE_NAME is not accessible but ought to be listed.  */
		st = empty_stat (cred);
		err = 0;
	      }
          }

        free (stat_file_name);

        if (err)
          {
            mutex_lock (&smb_mutex);
            smbc_closedir(dd);
            mutex_unlock (&smb_mutex);
            return errno;
          }

        err = add_dir_entry (dirent->name, st.st_ino, type);
        if (err)
          break;
      } 

  *datacnt = size;
  *amt = nreturningentries;
  mutex_lock (&smb_mutex);
  smbc_closedir (dd);
  mutex_unlock (&smb_mutex);
  return 0;
}

void
smbfs_init ()
{
  int err;
  nodes = 0;
  err = maptime_map (0, 0, &maptime);

  if(err)
    return;  
   
  create_root_node ();
}

void
smbfs_terminate ()
{
  mutex_init (&smb_mutex);
  clear_nodes ();
}
