/**********************************************************
 * cvs_files.h
 *
 * Copyright 2004, Stefan Siegl <stesie@brokenpipe.de>, Germany
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Publice License,
 * version 2 or any later. The license is contained in the COPYING
 * file that comes with the cvsfs distribution.
 *
 * download arbitrary revisions from cvs host and cache them locally
 */

#ifndef CVS_FILES_H
#define CVS_FILES_H

#include <stdio.h>

/* Download the revision (as specified by rev) of the specified file.  */
error_t cvs_files_cache(struct netnode *file, struct revision *rev);

/* ask cvs server whether there is a particular revision (as specified by rev)
 * available. return 0 if yes, ENOENT if not. EIO on communication error.
 */
error_t cvs_files_hit(struct netnode *file, struct revision *rev);

#endif /* CVS_FILES_H */
