/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>
  Creation date: 2015-07-30

  See LICENSES/BLOSC.txt for details about copyright and rights to use.
**********************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#if defined(USING_CMAKE)
  #include "config.h"
#endif /*  USING_CMAKE */
#include "blosc.h"

#if defined(_WIN32) && !defined(__MINGW32__)
  #include <windows.h>
  #include <malloc.h>

  /* stdint.h only available in VS2010 (VC++ 16.0) and newer */
  #if defined(_MSC_VER) && _MSC_VER < 1600
    #include "win32/stdint-windows.h"
  #else
    #include <stdint.h>
  #endif

#else
  #include <stdint.h>
  #include <unistd.h>
  #include <inttypes.h>
#endif  /* _WIN32 */

/* If C11 is supported, use it's built-in aligned allocation. */
#if __STDC_VERSION__ >= 201112L
  #include <stdalign.h>
#endif


/* Create a new super-chunk */
schunk_header* blosc2_new_schunk(schunk_params params) {
  schunk_header* sc_header = calloc(1, sizeof(schunk_header));

  sc_header->version = 0x0;  	/* pre-first version */
  sc_header->filters = params.filters;
  sc_header->compressor = params.compressor;
  sc_header->clevel = params.clevel;
  printf("Abans de posar el data...\n");
  sc_header->data = malloc(0);
  printf("Despres de posar el data...\n");
  /* The rest of the structure will remain zeroed */

  return sc_header;
}


/* Append an existing chunk to a super-chunk. */
int blosc2_append_chunk(schunk_header* sc_header, void* chunk) {
  int64_t nchunks = sc_header->nchunks;
  int64_t** i64_data;
  /* The chunksize starts in byte 12 */
  int32_t nbytes = *(int32_t*)(chunk + 4);
  int32_t chunksize = *(int32_t*)(chunk + 12);
  printf("chunksize: %d, %d, %lu\n", chunksize, nbytes, sizeof(void*));

  /* Get rid of possibly unused space in chunk */
  /* chunk = realloc(chunk, chunksize); */
  /* printf("chunk reallocated...\n"); */

  /* Make space for appending a new chunk and do it */
  sc_header->data = realloc(sc_header->data, (nchunks + 1) * sizeof(void*));
  printf("data reallocated... %p\n", sc_header->data);
  printf("chunk address: %p, %lld\n", chunk, nchunks);
  i64_data = sc_header->data;
  i64_data[nchunks] = chunk;
  sc_header->nchunks = nchunks + 1;

  return nchunks + 1;
}


/* Append a data buffer to a super-chunk. */
int blosc2_append_buffer(schunk_header* sc_header, size_t typesize,
			 size_t nbytes, void* src) {
  int chunksize;
  void* chunk = malloc(nbytes);

  /* Compress the src buffer using super-chunk defaults */
  chunksize = blosc_compress(sc_header->clevel, sc_header->filters,
			     typesize, nbytes, src, chunk, nbytes);
  if (chunksize < 0) {
    free(chunk);
    return chunksize;
  }

  return blosc2_append_chunk(sc_header, chunk);
}


/* Decompress and return a chunk that is part of a super-chunk. */
int blosc2_decompress_chunk(schunk_header* sc_header, int nchunk,
			    void **dest) {
  int64_t nchunks = sc_header->nchunks;
  int64_t** data_pointers;
  void* src;
  int chunksize;
  int32_t destsize;

  printf("nchunk: %d, %lld\n", nchunk, nchunks);
  if (nchunk >= nchunks) {
    return -10;
  }

  /* Grab the address of the chunk */
  data_pointers = sc_header->data;
  src = data_pointers[nchunk];
  printf("Hola2! %p\n", src);
  /* Create a buffer for destination */
  destsize = *(int32_t*)(src + 4);
  printf("destsize: %d\n", destsize);
  *dest = malloc(destsize);

  /* And decompress it */
  chunksize = blosc_decompress(src, *dest, destsize);
  if (chunksize < 0) {
    return chunksize;
  }
  if (chunksize != destsize) {
    return -11;
  }

  return chunksize;
}


/* Free all memory from a super-chunk. */
int blosc2_destroy_schunk(schunk_header* sc_header) {
  int i;
  int64_t** data_pointers;

  if (sc_header->metadata != NULL)
    free(sc_header->metadata);
  if (sc_header->userdata != NULL)
    free(sc_header->userdata);
  if (sc_header->data != NULL) {
    for (i = 0; i < sc_header->nchunks; i++) {
      data_pointers = sc_header->data;
      printf("freeing chunk %d\n", i);
      if (data_pointers[i] != NULL)
	free(data_pointers[i]);
    }
    printf("freeing header_data");
    free(sc_header->data);
  }
  printf("freeing header");
  free(sc_header);
  return 0;
}