/*
 * This code is based on dvdudf by:
 *   Christian Wolff <scarabaeus@convergence.de>.
 *
 * Modifications by:
 *   Billy Biggs <vektor@dumbterm.net>.
 *   Björn Englund <d4bjorn@dtek.chalmers.se>.
 *   James Courtier-Dutton <James@superbug.co.uk>
 *
 * dvdudf: parse and read the UDF volume information of a DVD Video
 * Copyright (C) 2012 James Courtier-Dutton
 *
 * This file has been modified from libdvdread.
 *
 * dvd_archive is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * dvd_archive is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with dvd_archive; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef DVD_UDF_H
#define DVD_UDF_H

#include <inttypes.h>

#include "dvd_reader.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Looks for a file on the UDF disc/imagefile and returns the block number
 * where it begins, or 0 if it is not found.  The filename should be an
 * absolute pathname on the UDF filesystem, starting with '/'.  For example,
 * '/VIDEO_TS/VTS_01_1.IFO'.  On success, filesize will be set to the size of
 * the file in bytes.
 */
uint32_t UDFFindFile(struct dvd_reader_s *device, char *filename, uint32_t *size );

void FreeUDFCache(void *cache);
int UDFGetVolumeIdentifier(struct dvd_reader_s *device,
                           char *volid, unsigned int volid_size);
int UDFGetVolumeSetIdentifier(struct dvd_reader_s *device,
                              uint8_t *volsetid, unsigned int volsetid_size);
void *GetUDFCacheHandle(struct dvd_reader_s *device);
void SetUDFCacheHandle(struct dvd_reader_s *device, void *cache);
int UDFGetPartition(struct dvd_reader_s *device, int *start, int *length);


#ifdef __cplusplus
};
#endif
#endif /* DVD_UDF_H */
