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

#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "libdvdcss.h"
#include "dvd_udf.h"
#include "dvd_reader.h"

#define CSS_LIB "libdvdcss.so.2"

#if defined(__OpenBSD__) && !defined(__ELF__) || defined(__OS2__)
#define U_S "_"
#else
#define U_S
#endif

static struct dvdcss_s * (*DVDcss_open)  (const char *);
static int           (*DVDcss_close) (struct dvdcss_s *);
static int           (*DVDcss_seek)  (struct dvdcss_s *, int, int);
static int           (*DVDcss_title) (struct dvdcss_s *, int);
static int           (*DVDcss_read)  (struct dvdcss_s *, void *, int, int);
static char *        (*DVDcss_error) (struct dvdcss_s *);

struct vobs_s {
	uint32_t start;
	uint32_t len;
};

struct segment_s {
	uint64_t start;
	uint64_t length;
	int encrypted;
};

//const char *filename = "t1.img";
const char *filename = "/dev/sr0";

/**
 * Set the level of caching on udf
 * level = 0 (no caching)
 * level = 1 (caching filesystem info)
 */
int DVDUDFCacheLevel(struct dvd_reader_s *device, int level) {

	if(level > 0) {
		level = 1;
	} else if(level < 0) {
		return device->udfcache_level;
	}

	device->udfcache_level = level;

	return level;
}

void *GetUDFCacheHandle(struct dvd_reader_s *device) {
	return device->udfcache;
}

void SetUDFCacheHandle(struct dvd_reader_s *device, void *cache) {
	device->udfcache = cache;
}

static int findVOBs( struct dvd_reader_s *dvd, struct vobs_s *vobs, int *vobs_length ) {
	char filename[ MAX_UDF_FILE_NAME_LEN ];
	int title;
	uint32_t start, len;
	int m, l;
	int length = 0;
	

	sprintf( filename, "/VIDEO_TS/VIDEO_TS.VOB" );
	start = UDFFindFile( dvd, filename, &len );
	if (start != 0 && len != 0) {
		printf("Found file %s, start=0x%x, len=0x%x\n",
			filename, start, len / DVD_VIDEO_LB_LEN);
		vobs[length].start = start;
		vobs[length].len = len / DVD_VIDEO_LB_LEN;
		length++;
	}
	
	for( title = 1; title < 100; title++ ) {
		int found = 0;
		for (m = 0; m < 10; m++) {
      			sprintf( filename, "/VIDEO_TS/VTS_%02d_%d.VOB", title, m );
			start = UDFFindFile( dvd, filename, &len );
			if (start != 0 && len != 0) {
				printf("Found file %s, start=0x%x, len=0x%x\n",
					filename, start, len / DVD_VIDEO_LB_LEN);
				found = 0;
				for(l = 0; l < length; l++) {
					if ((vobs[l].start == start) &&
						(vobs[l].len == len)) {
						found = 1;
						break;
					}
				}
				if (!found) {
					vobs[length].start = start;
					vobs[length].len = len / DVD_VIDEO_LB_LEN;
					length++;
				}
			}
		}
	}
	*vobs_length = length;	
	return 0;
}
	


/* Loop over all titles and call dvdcss_title to crack the keys. */
static int initAllCSSKeys( struct dvd_reader_s *dvd ) {
	struct timeval all_s, all_e;
	struct timeval t_s, t_e;
	char filename[ MAX_UDF_FILE_NAME_LEN ];
	uint32_t start, len;
	int title;

	char *nokeys_str = getenv("DVDREAD_NOKEYS");
	if(nokeys_str != NULL) {
		return 0;
	}

	fprintf( stderr, "\n" );
	fprintf( stderr, "dvd_archive: Attempting to retrieve all CSS keys\n" );
	fprintf( stderr, "dvd_archive: This can take a _long_ time, "
		"please be patient\n\n" );
	gettimeofday(&all_s, NULL);

	for( title = 0; title < 100; title++ ) {
		gettimeofday( &t_s, NULL );
		if( title == 0 ) {
			sprintf( filename, "/VIDEO_TS/VIDEO_TS.VOB" );
		} else {
			sprintf( filename, "/VIDEO_TS/VTS_%02d_%d.VOB", title, 0 );
		}
		printf("filename=%s\n", filename);
		start = UDFFindFile( dvd, filename, &len );
		if( start != 0 && len != 0 ) {
			/* Perform CSS key cracking for this title. */
			fprintf( stderr, "dvd_archive: Get key for %s at 0x%08x, len=0x%08x\n",
				filename, start, len );
			if( DVDcss_title( dvd->dev, (int)start ) < 0 ) {
				fprintf( stderr, "dvd_archive: Error cracking CSS key for %s (0x%08x)\n", filename, start);
			}
			gettimeofday( &t_e, NULL );
			fprintf( stderr, "dvd_archive: Elapsed time %ld\n",
				(long int) t_e.tv_sec - t_s.tv_sec );
		}

		if( title == 0 ) continue;

		gettimeofday( &t_s, NULL );
		sprintf( filename, "/VIDEO_TS/VTS_%02d_%d.VOB", title, 1 );
		start = UDFFindFile( dvd, filename, &len );
		if( start == 0 || len == 0 ) break;

		/* Perform CSS key cracking for this title. */
		fprintf( stderr, "dvd_archive: Get key for %s at 0x%08x, len=0x%08x\n",
			filename, start, len );
		if( DVDcss_title( dvd->dev, (int)start ) < 0 ) {
			fprintf( stderr, "dvd_archive: Error cracking CSS key for %s (0x%08x)!!\n", filename, start);
		}
		gettimeofday( &t_e, NULL );
		fprintf( stderr, "dvd_archive: Elapsed time %ld\n",
			(long int) t_e.tv_sec - t_s.tv_sec );
	}
	title--;

	fprintf( stderr, "dvd_archive: Found %d VTS's\n", title );
	gettimeofday(&all_e, NULL);
	fprintf( stderr, "dvd_archive: Elapsed time %ld\n",
		(long int) all_e.tv_sec - all_s.tv_sec );

	return 0;
}


/* Internal, but used from dvd_udf.c */
int UDFReadBlocksRaw( struct dvd_reader_s *device, uint32_t lb_number,
	size_t block_count, unsigned char *data,
	int encrypted ) {
	int ret;

	if( !device->dev ) {
		fprintf( stderr, "libdvdread: Fatal error in block read.\n" );
		return 0;
	}

	ret = DVDcss_seek( device->dev, (int) lb_number, encrypted );
	if( ret != (int) lb_number ) {
		fprintf( stderr, "libdvdread: Can't seek to block %u\n", lb_number );
		return 0;
	}

	ret = DVDcss_read( device->dev, (char *) data,
		(int) block_count, encrypted );
	return ret;
}

/* Used to simulate errors on the DVD */
/* FIXME: TODO */


struct error_s {
	int sector;
	int error;
};

struct error_s errors[10] = {
	{0x200, 0},
	{0x300, -1}
};

int dvd_position = 0;
int dvd_seek(struct dvdcss_s *dvdcss, int i_blocks, int i_flags ) {
	int tmp;
	tmp = DVDcss_seek(dvdcss, i_blocks, i_flags);
	dvd_position = i_blocks;
	return tmp;
}

int dvd_read(struct dvdcss_s *dvdcss, void *p_buffer, int i_blocks, int i_flags ) {
	int tmp;
	int n;
	int iret;
	iret = DVDcss_read(dvdcss, p_buffer, i_blocks, i_flags);
	printf("read tmp=0x%x, dvd_position=0x%x, i_blocks=0x%x\n",
		iret, dvd_position, i_blocks);
	tmp = iret;
	if (tmp < 0) {
		tmp = 0;
	}
#if 0 /* Error simulator */
	for (n = 0; n < 10; n++) {
		int err = errors[n].sector;
		//printf("err=0x%x\n", err);
		if ((err) &&
			(err >= dvd_position) &&
			(err < (dvd_position + i_blocks))) {
			if (errors[n].error < 0) {
				iret = errors[n].error;
				tmp = 0;
			} else {
				tmp = err - dvd_position;
				iret = tmp;
			}
			printf("ERROR FORCED\n");
			break;
		} 
	}
#endif
	dvd_position += tmp;
	return iret;
}


int main() {
	void *DVDcss_library = NULL;
	char **DVDcss_version = NULL;
	struct dvdcss_s *dev = NULL;
	struct dvd_reader_s *device;
	int tmp;
	char *volume_id;
	int partition_start, partition_length;
	struct segment_s *segments;
	int n;
	uint64_t m64;
	int segments_len;
	char *filename1;
	char *filename2;
	uint8_t *buffer;
	int output_file_handle;
	struct vobs_s *vobs;
	int vobs_length;
	uint32_t sector;
	int limp_factor = 0;

	device = calloc(1, sizeof(struct dvd_reader_s));
	if (!device) {
		printf("Out of memory\n");
		return 0;
	}
	volume_id = calloc(1, 32 + 1);
	if (!volume_id) {
		printf("Out of memory\n");
		return 0;
	}
	vobs = calloc(200, sizeof(struct vobs_s));
	if (!vobs) {
		printf("Out of memory\n");
		return 0;
	}
	segments = calloc(100, sizeof(struct segment_s));
	if (!segments) {
		printf("Out of memory\n");
		return 0;
	}
	filename1 = calloc(1, MAX_UDF_FILE_NAME_LEN);
	if (!filename1) {
		printf("Out of memory\n");
		return 0;
	}
	filename2 = calloc(1, MAX_UDF_FILE_NAME_LEN);
	if (!filename2) {
		printf("Out of memory\n");
		return 0;
	}
	buffer = calloc(0x200, DVD_VIDEO_LB_LEN); // 256 blocks
	if (!buffer) {
		printf("Out of memory\n");
		return 0;
	}
	DVDcss_library = dlopen(CSS_LIB, RTLD_LAZY);

	if(DVDcss_library != NULL) {
		DVDcss_open = (struct dvdcss_s * (*)(const char*))
			dlsym(DVDcss_library, U_S "dvdcss_open");
		DVDcss_close = (int (*)(struct dvdcss_s *))
			dlsym(DVDcss_library, U_S "dvdcss_close");
		DVDcss_title = (int (*)(struct dvdcss_s *, int))
			dlsym(DVDcss_library, U_S "dvdcss_title");
		DVDcss_seek = (int (*)(struct dvdcss_s *, int, int))
			dlsym(DVDcss_library, U_S "dvdcss_seek");
		DVDcss_read = (int (*)(struct dvdcss_s *, void*, int, int))
			dlsym(DVDcss_library, U_S "dvdcss_read");
		DVDcss_error = (char* (*)(struct dvdcss_s *))
			dlsym(DVDcss_library, U_S "dvdcss_error");

		DVDcss_version = (char **)dlsym(DVDcss_library, U_S "dvdcss_interface_2");

		if(dlsym(DVDcss_library, U_S "dvdcss_crack")) {
			fprintf(stderr,
				"dvd_archive: Old (pre-0.0.2) version of libdvdcss found.\n"
				"dvd_archive: You should get the latest version from "
				"http://www.videolan.org/\n" );
			dlclose(DVDcss_library);
			DVDcss_library = NULL;
		} else if(!DVDcss_open  || !DVDcss_close || !DVDcss_title || !DVDcss_seek
			|| !DVDcss_read || !DVDcss_error || !DVDcss_version) {
			fprintf(stderr,  "libdvdread: Missing symbols in %s, "
				"this shouldn't happen !\n", CSS_LIB);
			dlclose(DVDcss_library);
			DVDcss_library = NULL;
		}
	} else {
		printf("Failed to find libdvdcss2\nTry install-css.sh\n");
		return 1;
	}
	printf("dvdcss_library = %p\n", DVDcss_library);	
	dev = DVDcss_open(filename);
	if (!dev) {
		printf("Failed to open %s\nIs there a DVD in the DVD-ROM drive?\n", filename);
		return 1;
	}
	printf("dev = %p\n", dev);
	device->dev = dev;
	tmp = UDFGetVolumeIdentifier(device, volume_id, 32);
	printf("Volume ID = %s, length=%d\n", volume_id, tmp);

	tmp = sprintf(filename1,"%s.IMG",volume_id);
	printf("filename1 = %s, length=%d\n", filename1, tmp);
	
	tmp = initAllCSSKeys(device);

	tmp = findVOBs(device, vobs, &vobs_length);
	printf("FindVOBs len = %d\n", vobs_length);

	partition_start = 0;
	partition_length = 0;
	tmp = UDFGetPartition(device, &partition_start, &partition_length);
	printf("Partition Start=0x%x, Length=0x%x\n",
		partition_start,
		partition_length);

	sector = 0;
	segments_len = 0;
	for (n = 0; n < vobs_length; n++) {
		if (sector < vobs[n].start) {
			segments[segments_len].start = sector;
			segments[segments_len].length = vobs[n].start - sector;
			segments[segments_len].encrypted = 0;
			segments_len++;
			sector = vobs[n].start;
		}
		if (sector == vobs[n].start) {
			segments[segments_len].start = vobs[n].start;
			segments[segments_len].length = vobs[n].len;
			segments[segments_len].encrypted = DVDCSS_READ_DECRYPT;
//			segments[segments_len].encrypted = 0;
			segments_len++;
			sector = vobs[n].start + vobs[n].len;
		}
	}
	segments[segments_len].start = sector;
	segments[segments_len].length = partition_start + partition_length - sector;
	segments_len++;
	
	for (n=0; n < segments_len; n++) {
		printf("Segment %d: start = 0x%"PRIx64", length = 0x%"PRIx64", encrypted=%d\n",
			n, segments[n].start, segments[n].length, segments[n].encrypted);
	}
	output_file_handle = open(filename1, O_CREAT|O_WRONLY, S_IRWXU);
	if (!output_file_handle) {
		printf("Open failed with %d\n", output_file_handle);
		return 1;
	}
	for (n=0; n < segments_len; n++) { //segments_len
		int step;
		int64_t write_step;
		off64_t offset;
		uint64_t tmp64;
		if (segments[n].encrypted) {
			tmp = dvd_seek(device->dev, segments[n].start, DVDCSS_SEEK_KEY);
		} else {
			tmp = dvd_seek(device->dev, segments[n].start, 0);
		}
		printf("Seek:%d:0x%"PRIx64":0x%x\n", n, segments[n].start, tmp);
		m64 = 0;
		step = 0;
		limp_factor = 0;
		while (m64 < segments[n].length) {
			step = segments[n].length - m64;

			if (step > 0x200) {
				step = 0x200;
			}
			if (limp_factor > 0) {
				step = 1;
				limp_factor--;
			}
			offset = segments[n].start * DVD_VIDEO_LB_LEN;
			offset += (m64 * DVD_VIDEO_LB_LEN);
			//offset += DVD_VIDEO_LB_LEN; // tmp fix for reading from file

			/* clear it each time round for error case reasons */
			memset(buffer, 0, step * DVD_VIDEO_LB_LEN);
			tmp = dvd_seek(device->dev, segments[n].start + m64, 0);
			if (tmp < 0) {  // Error occured
				/* FIXME: Handle error. It might fail if the destination sector is bad */
				printf("Input-seek Error:%d:0x%x:0x%"PRIx64"\n", n, tmp, offset );
				return 1;
			}
			
			tmp = dvd_read(device->dev, buffer, step, segments[n].encrypted);
			write_step = step;
			if (tmp <= 0) {  // Error occured
			/* FIXME: Handle error */
			/* For now just skip it */
			/* Perhaps:
			 *          start reading single sectors until a success again
			 */
				memset(buffer, 0, step * DVD_VIDEO_LB_LEN);
				printf("Read Error:%d:0x%x:0x%x\n", n, tmp, step );
				write_step = 1;
				step = 1;
				limp_factor = 10;
				printf("Limping %d\n", limp_factor);
			} else if (tmp != step) {
				printf("Read Error:%d:0x%x:0x%x\n", n, tmp, step );
				/* FIXME: TODO: HANDLE ERRORS */
				write_step = tmp;
				step = tmp;
			}

			tmp64 = lseek64(output_file_handle, offset, SEEK_SET);
			printf("output-seek=0x%"PRIx64", tmp=0x%"PRIx64", start = 0x%"PRIx64"\n", offset, tmp64, segments[n].start);
			tmp = write(output_file_handle, buffer, write_step * DVD_VIDEO_LB_LEN);
			if (tmp != (write_step * DVD_VIDEO_LB_LEN)) {
				printf("Write Error:%d:0x%x\n", n, tmp);
				/* FIXME: TODO: HANDLE ERRORS */
				return 1;
			}
			m64 += step;
		}
	}
	/* Seek to end of file before close, to prevent truncation */
	tmp = lseek64(output_file_handle, 0, SEEK_END);
	tmp = close(output_file_handle);

	return 0;
}

