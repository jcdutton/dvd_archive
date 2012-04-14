/*
 * This code is written by:
 *   James Courtier-Dutton <James@superbug.co.uk>
 *   Copyright 2012
 *
 * dvd_archive: Make a archive .IMG of a DVD, similar to the linux command dd.
 *
 * This file is part of dvd_archive.
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

#ifndef DVD_READER_H
#define DVD_READER_H


//struct dvd_input_s {
//  /* libdvdcss handle */
//  struct dvdcss_s *dvdcss;
//
//  /* dummy file input */
//  int fd;
//};

//typedef struct dvd_input_s *dvd_input_t;

struct dvd_reader_s {
  /* Basic information. */
  int isImageFile;

  /* Hack for keeping track of the css status.
   * 0: no css, 1: perhaps (need init of keys), 2: have done init */
  int css_state;
  int css_title; /* Last title that we have called dvdinpute_title for. */

  /* Information required for an image file. */
  struct dvdcss_s *dev;

  /* Information required for a directory path drive. */
  char *path_root;

  /* Filesystem cache */
  int udfcache_level; /* 0 - turned off, 1 - on */
  void *udfcache;
};

//typedef struct dvd_reader_s dvd_reader_t;

/**
 * Maximum length of filenames allowed in UDF.
 */
#define MAX_UDF_FILE_NAME_LEN 2048
#define DVD_VIDEO_LB_LEN 2048

int UDFReadBlocksRaw(struct dvd_reader_s *device, uint32_t lb_number,
                     size_t block_count, unsigned char *data, int encrypted);


/**
 * Sets the level of caching that is done when reading from a device
 *
 * @param dvd A read handle to get the disc ID from
 * @param level The level of caching wanted.
 *             -1 - returns the current setting.
 *              0 - UDF Cache turned off.
 *              1 - (default level) Pointers to IFO files and some data from
 *                  PrimaryVolumeDescriptor are cached.
 *
 * @return The level of caching.
 */
int DVDUDFCacheLevel(struct dvd_reader_s *, int);



#endif //DVD_READER_H
