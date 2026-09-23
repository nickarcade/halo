/* ===========================================================================
 * tif_write.c -- vendored libtiff (Sam Leffler, rev 1.41 92/02/10),
 * scanline-oriented write support.
 *
 * kb.json's tif_write.obj bucket spans 0x6e740-0x74a30, but only the twelve
 * functions at 0x6f9f0-0x704b6 are upstream tif_write.c; the rest of the
 * bucket is tif_read/tif_strip/tif_swab/tif_thunder/tif_tile/tif_warning and
 * the s3tc/bitmap code that follows. Those twelve are transcribed from
 * upstream libtiff tif_write.c (the vendored-library rule). The binary
 * carries the `c:\halo\SOURCE\bitmaps\libtiff\tif_write.c` __FILE__ string
 * (0x2612f0, pushed with lines 390/392/484/509/511 exactly as upstream's
 * malloc/realloc sites), the per-function module names at 0x2ed00c-0x2ed078,
 * and every function's size and order match the PAL build 2342
 * reconstruction (halo-pal-2342 source/bitmaps/libtiff/tif_write.c). As in
 * PAL, the file-static helpers are emitted first (0x6f9f0-0x6fe0f), then
 * TIFFFlushData1 and the public entry points; 0x703f0 is TIFFWriteRawTile and
 * 0x70460 TIFFWriteTile (their 2276-dump names had landed on 0x71cc0/0x74a30,
 * which are s3tc/bitmap code).
 *
 * ABI: the file-statics carry custom register conventions from the original
 * link, recorded in kb.json as `@<reg>` and read off entry code and call
 * sites:
 *   TIFFSetupStrips    tif@esi                              (0x6f9f0)
 *   TIFFWriteCheck     tif@eax tiles@ecx module@edi         (0x6faf0)
 *   TIFFBufferSetup    tif@esi, module on the stack         (0x6fbd0)
 *   TIFFGrowStrips     tif@esi delta@edi, module on stack   (0x6fc60)
 *   TIFFAppendToStrip  tif@esi strip@edi cc@ebx, data stack (0x6fd30)
 * The public functions are plain cdecl.
 * ======================================================================== */

#define TIFF_BUFFERSETUP 0x4
#define TIFF_BEENWRITING 0x8
#define TIFF_NOBITREV 0x20
#define TIFF_ISTILED 0x80
#define TIFF_POSTENCODE 0x200 /* `or byte ptr [esi+0xb],2` */

#define PLANARCONFIG_SEPARATE 2
#define FIELD_IMAGEDIMENSIONS 0
#define FIELD_PLANARCONFIG 20
#define FIELD_STRIPBYTECOUNTS 26
#define FIELD_STRIPOFFSETS 27

#define O_RDONLY 0
#define L_SET 0
#define L_XTND 2

typedef int (*tiff_bool_method_t)(void *tif);
typedef int (*tiff_code_method_t)(void *tif, char *buf, int cc, int s);
typedef int (*tiff_seek_method_t)(void *tif, int n);

/* Upstream libtiff's TIFFDirectory without the optional-support tails. The
 * offsets relative to the TIFF base are the ones tif_open.c proves
 * (td_fieldsset at 0x14 ... td_stripbytecount at 0xc0); _TIFFgetfield is
 * passed `tif + 0x14` (0x67f88), i.e. the directory is a nested member. */
typedef struct tiff_directory_s {
  unsigned long td_fieldsset[2]; /* 0x14 */
  unsigned long td_imagewidth; /* 0x1c */
  unsigned long td_imagelength; /* 0x20 */
  unsigned long td_imagedepth; /* 0x24 */
  unsigned long td_tilewidth; /* 0x28 */
  unsigned long td_tilelength; /* 0x2c */
  unsigned long td_tiledepth; /* 0x30 */
  unsigned short td_subfiletype; /* 0x34 */
  unsigned short td_bitspersample; /* 0x36 -- `mov cl,[ebx+0x36]` 0x67a09 */
  unsigned short td_sampleformat; /* 0x38 */
  unsigned short td_compression; /* 0x3a */
  unsigned short td_photometric; /* 0x3c */
  unsigned short td_threshholding; /* 0x3e */
  unsigned short td_fillorder; /* 0x40 */
  unsigned short td_orientation; /* 0x42 */
  unsigned short td_samplesperpixel; /* 0x44 -- movzx 0x67f7f */
  unsigned short td_predictor; /* 0x46 */
  unsigned long td_rowsperstrip; /* 0x48 */
  unsigned long td_minsamplevalue; /* 0x4c */
  unsigned long td_maxsamplevalue; /* 0x50 */
  float td_xresolution; /* 0x54 */
  float td_yresolution; /* 0x58 */
  unsigned short td_resolutionunit; /* 0x5c */
  unsigned short td_planarconfig; /* 0x5e */
  float td_xposition; /* 0x60 */
  float td_yposition; /* 0x64 */
  unsigned long td_group3options; /* 0x68 */
  unsigned long td_group4options; /* 0x6c */
  unsigned short td_pagenumber[2]; /* 0x70 */
  unsigned short td_matteing; /* 0x74 */
  unsigned short td_cleanfaxdata; /* 0x76 */
  unsigned short td_badfaxrun; /* 0x78 */
  unsigned long td_badfaxlines; /* 0x7c */
  unsigned short *td_colormap[3]; /* 0x80 */
  unsigned short td_halftonehints[2]; /* 0x8c */
  char *td_documentname; /* 0x90 */
  char *td_artist; /* 0x94 */
  char *td_datetime; /* 0x98 */
  char *td_hostcomputer; /* 0x9c */
  char *td_imagedescription; /* 0xa0 */
  char *td_make; /* 0xa4 */
  char *td_model; /* 0xa8 */
  char *td_software; /* 0xac */
  char *td_pagename; /* 0xb0 */
  unsigned long td_stripsperimage; /* 0xb4 */
  unsigned long td_nstrips; /* 0xb8 */
  unsigned long *td_stripoffset; /* 0xbc */
  unsigned long *td_stripbytecount; /* 0xc0 */
} tiff_directory_t;

typedef struct tiff_header_s {
  unsigned short tiff_magic; /* 0xc4 */
  unsigned short tiff_version; /* 0xc6 */
  unsigned long tiff_diroff; /* 0xc8 */
} tiff_header_t;

/* Upstream libtiff's `struct tiff` (tiffioP.h). tif_fd/tif_mode are 16-bit
 * (`movsx eax,word ptr [esi+4]` throughout), tif_flags is the 16-bit word at
 * 0x0a (byte tests at 0x0a/0x0b), and the rest of the layout is the one
 * tif_open.c proves field by field. */
typedef struct tiff_s {
  char *tif_name; /* 0x00 */
  short tif_fd; /* 0x04 */
  short tif_mode; /* 0x06 */
  char tif_fillorder; /* 0x08 */
  char tif_options; /* 0x09 */
  short tif_flags; /* 0x0a */
  long tif_diroff; /* 0x0c */
  long tif_nextdiroff; /* 0x10 */
  tiff_directory_t tif_dir; /* 0x14 */
  tiff_header_t tif_header; /* 0xc4 */
  const int *tif_typeshift; /* 0xcc -- `mov ecx,[esi+0xcc]` 0x67748 */
  const long *tif_typemask; /* 0xd0 -- `mov eax,[esi+0xd0]` 0x67732 */
  long tif_row; /* 0xd4 */
  int tif_curdir; /* 0xd8 */
  int tif_curstrip; /* 0xdc */
  long tif_curoff; /* 0xe0 */
  long tif_col; /* 0xe4 */
  int tif_curtile; /* 0xe8 */
  long tif_tilesize; /* 0xec */
  tiff_bool_method_t tif_predecode; /* 0xf0 */
  tiff_bool_method_t tif_preencode; /* 0xf4 */
  tiff_bool_method_t tif_postencode; /* 0xf8 */
  tiff_code_method_t tif_decoderow; /* 0xfc */
  tiff_code_method_t tif_encoderow; /* 0x100 */
  tiff_code_method_t tif_decodestrip; /* 0x104 */
  tiff_code_method_t tif_encodestrip; /* 0x108 */
  tiff_code_method_t tif_decodetile; /* 0x10c */
  tiff_code_method_t tif_encodetile; /* 0x110 */
  tiff_bool_method_t tif_close; /* 0x114 */
  tiff_seek_method_t tif_seek; /* 0x118 */
  tiff_bool_method_t tif_cleanup; /* 0x11c */
  char *tif_data; /* 0x120 */
  int tif_scanlinesize; /* 0x124 */
  int tif_scanlineskew; /* 0x128 */
  char *tif_rawdata; /* 0x12c */
  long tif_rawdatasize; /* 0x130 */
  char *tif_rawcp; /* 0x134 */
  long tif_rawcc; /* 0x138 */
} tiff_t;

#define TIFF_WRITE_FILE "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_write.c"

#define BITn(n) (((unsigned)1L) << ((n) & 0x1f))
#define BITFIELDn(tif, n) ((tif)->tif_dir.td_fieldsset[(n) / 32])
#define TIFFFieldSet(tif, field) (BITFIELDn(tif, field) & BITn(field))
#define TIFFSetFieldBit(tif, field) (BITFIELDn(tif, field) |= BITn(field))
#define isTiled(tif) (((tif)->tif_flags & TIFF_ISTILED) != 0)
#define howmany(x, y) \
  ((((unsigned int)(x)) + (((unsigned int)(y)) - 1)) / ((unsigned int)(y)))
#define SeekOK(fd, off) (__lseek(fd, (long)off, L_SET) == (long)off)
#define WriteOK(fd, buf, size) (__write(fd, (char *)buf, size) == size)

/* FUN_00068a30 is TIFFError, FUN_0006a210 TIFFFlushData and FUN_0006f910
 * TIFFTileSize. */

/* 0x6f9f0 -- size and allocate the strip (or tile) offset/bytecount
 * arrays for the first write. */
int TIFFSetupStrips(void *tif_)
{
#define isUnspecified(td, v) (td->v == 0xffffffff || (td)->td_imagelength == 0)
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;

  if (!isTiled(tif))
    td->td_stripsperimage = isUnspecified(td, td_rowsperstrip) ?
                              1 :
                              howmany(td->td_imagelength, td->td_rowsperstrip);
  else
    td->td_stripsperimage =
      isUnspecified(td, td_tilelength) ? 1 : TIFFNumberOfTiles(tif);
  td->td_nstrips = td->td_stripsperimage;
  if (td->td_planarconfig == PLANARCONFIG_SEPARATE)
    td->td_nstrips *= td->td_samplesperpixel;
  td->td_stripoffset = (unsigned long *)debug_malloc(
    td->td_nstrips * sizeof(unsigned long), 0, TIFF_WRITE_FILE, 390);
  td->td_stripbytecount = (unsigned long *)debug_malloc(
    td->td_nstrips * sizeof(unsigned long), 0, TIFF_WRITE_FILE, 392);
  if (td->td_stripoffset == NULL || td->td_stripbytecount == NULL)
    return (0);
  /*
   * Place data at the end-of-file
   * (by setting offsets to zero).
   */
  csmemset((char *)td->td_stripoffset, 0,
           td->td_nstrips * sizeof(unsigned long));
  csmemset((char *)td->td_stripbytecount, 0,
           td->td_nstrips * sizeof(unsigned long));
  TIFFSetFieldBit(tif, FIELD_STRIPOFFSETS);
  TIFFSetFieldBit(tif, FIELD_STRIPBYTECOUNTS);
  return (1);
#undef isUnspecified
}

/* 0x6faf0 -- verify the file is writable and that the directory information
 * is setup properly; on the first write also "freeze" the directory. */
int TIFFWriteCheck(void *tif_, int tiles, char *module)
{
  tiff_t *tif = (tiff_t *)tif_;

  if (tif->tif_mode == O_RDONLY) {
    FUN_00068a30(module, "%s: File not open for writing", tif->tif_name);
    return (0);
  }
  if (tiles ^ isTiled(tif)) {
    FUN_00068a30(tif->tif_name, tiles ?
                                  "Can not write tiles to a stripped image" :
                                  "Can not write scanlines to a tiled image");
    return (0);
  }
  /*
   * On the first write verify all the required information
   * has been setup and initialize any data structures that
   * had to wait until directory information was set.
   * Note that a lot of our work is assumed to remain valid
   * because we disallow any of the important parameters
   * from changing after we start writing (i.e. once
   * TIFF_BEENWRITING is set, TIFFSetField will only allow
   * the image's length to be changed).
   */
  if ((tif->tif_flags & TIFF_BEENWRITING) == 0) {
    if (!TIFFFieldSet(tif, FIELD_IMAGEDIMENSIONS)) {
      FUN_00068a30(module, "%s: Must set \"ImageWidth\" before writing data",
                   tif->tif_name);
      return (0);
    }
    if (!TIFFFieldSet(tif, FIELD_PLANARCONFIG)) {
      FUN_00068a30(module,
                   "%s: Must set \"PlanarConfiguration\" before writing data",
                   tif->tif_name);
      return (0);
    }
    if (tif->tif_dir.td_stripoffset == NULL && !TIFFSetupStrips(tif)) {
      tif->tif_dir.td_nstrips = 0;
      FUN_00068a30(module, "%s: No space for %s arrays", tif->tif_name,
                   isTiled(tif) ? "tile" : "strip");
      return (0);
    }
    tif->tif_flags |= TIFF_BEENWRITING;
  }
  return (1);
}

/* 0x6fbd0 -- setup the raw data buffer used for encoding. */
int TIFFBufferSetup(void *tif_, char *module)
{
  tiff_t *tif = (tiff_t *)tif_;
  int size;

  if (isTiled(tif))
    tif->tif_tilesize = size = FUN_0006f910(tif);
  else
    tif->tif_scanlinesize = size = TIFFScanlineSize((int)tif);
  /*
   * Make raw data buffer at least 8K
   */
  if (size < 8 * 1024)
    size = 8 * 1024;
  tif->tif_rawdata = (char *)debug_malloc(size, 0, TIFF_WRITE_FILE, 484);
  if (tif->tif_rawdata == NULL) {
    FUN_00068a30(module, "%s: No space for output buffer", tif->tif_name);
    return (0);
  }
  tif->tif_rawdatasize = size;
  tif->tif_rawcc = 0;
  tif->tif_rawcp = tif->tif_rawdata;
  return (1);
}

/* 0x6fc60 -- grow the strip data structures by delta strips. */
int TIFFGrowStrips(void *tif_, int delta, char *module)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;

  td->td_stripoffset = (unsigned long *)debug_realloc(
    td->td_stripoffset, (td->td_nstrips + delta) * sizeof(unsigned long),
    TIFF_WRITE_FILE, 509);
  td->td_stripbytecount = (unsigned long *)debug_realloc(
    td->td_stripbytecount, (td->td_nstrips + delta) * sizeof(unsigned long),
    TIFF_WRITE_FILE, 511);
  if (td->td_stripoffset == NULL || td->td_stripbytecount == NULL) {
    td->td_nstrips = 0;
    FUN_00068a30(module, "%s: No space to expand strip arrays", tif->tif_name);
    return (0);
  }
  csmemset((char *)td->td_stripoffset + td->td_nstrips, 0,
           delta * sizeof(unsigned long));
  csmemset((char *)td->td_stripbytecount + td->td_nstrips, 0,
           delta * sizeof(unsigned long));
  td->td_nstrips += delta;
  return (1);
}

/* 0x6fd30 -- append the data to the specified strip. We don't check that
 * there's space in the file (i.e. that strips do not overlap). */
int TIFFAppendToStrip(void *tif_, unsigned int strip, unsigned char *data,
                      unsigned int cc)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;

  if (td->td_stripoffset[strip] == 0 || tif->tif_curoff == 0) {
    /*
     * No current offset, set the current strip.
     */
    if (td->td_stripoffset[strip] != 0) {
      if (!SeekOK(tif->tif_fd, td->td_stripoffset[strip])) {
        FUN_00068a30("TIFFAppendToStrip", "%s: Seek error at scanline %d",
                     tif->tif_name, tif->tif_row);
        return (0);
      }
    } else
      td->td_stripoffset[strip] = __lseek(tif->tif_fd, 0L, L_XTND);
    tif->tif_curoff = td->td_stripoffset[strip];
  }
  if (!WriteOK(tif->tif_fd, data, cc)) {
    FUN_00068a30("TIFFAppendToStrip", "%s: Write error at scanline %d",
                 tif->tif_name, tif->tif_row);
    return (0);
  }
  tif->tif_curoff += cc;
  td->td_stripbytecount[strip] += cc;
  return (1);
}

/* 0x6fe10 -- internal version of TIFFFlushData that can be called by
 * ``encodestrip routines'' w/o concern for infinite recursion. */
int TIFFFlushData1(void *tif_)
{
  tiff_t *tif = (tiff_t *)tif_;

  if (tif->tif_rawcc > 0) {
    if (tif->tif_dir.td_fillorder != tif->tif_fillorder &&
        (tif->tif_flags & TIFF_NOBITREV) == 0)
      TIFFReverseBits((unsigned char *)tif->tif_rawdata, tif->tif_rawcc);
    if (!TIFFAppendToStrip(tif,
                           isTiled(tif) ? tif->tif_curtile : tif->tif_curstrip,
                           (unsigned char *)tif->tif_rawdata, tif->tif_rawcc))
      return (0);
    tif->tif_rawcc = 0;
    tif->tif_rawcp = tif->tif_rawdata;
  }
  return (1);
}

/* 0x6fea0 -- write one scanline, growing the image (PlanarConfig=1 only)
 * and the strip arrays as needed. */
int TIFFWriteScanline(int file, void *buffer, unsigned int row,
                      unsigned int sample)
{
  tiff_t *tif = (tiff_t *)file;
  tiff_directory_t *td;
  int strip, status, imagegrew = 0;

  if (!TIFFWriteCheck(tif, 0, "TIFFWriteScanline"))
    return (-1);
  /*
   * Handle delayed allocation of data buffer.  This
   * permits it to be sized more intelligently (using
   * directory information).
   */
  if ((tif->tif_flags & TIFF_BUFFERSETUP) == 0) {
    if (!TIFFBufferSetup(tif, "TIFFWriteScanline"))
      return (-1);
    tif->tif_flags |= TIFF_BUFFERSETUP;
  }
  td = &tif->tif_dir;
  /*
   * Extend image length if needed
   * (but only for PlanarConfig=1).
   */
  if (row >= td->td_imagelength) { /* extend image */
    if (td->td_planarconfig == PLANARCONFIG_SEPARATE) {
      FUN_00068a30(tif->tif_name,
                   "Can not change \"ImageLength\" when using separate planes");
      return (-1);
    }
    td->td_imagelength = row + 1;
    imagegrew = 1;
  }
  /*
   * Calculate strip and check for crossings.
   */
  if (td->td_planarconfig == PLANARCONFIG_SEPARATE) {
    if (sample >= td->td_samplesperpixel) {
      FUN_00068a30(tif->tif_name, "%d: Sample out of range, max %d", sample,
                   td->td_samplesperpixel);
      return (-1);
    }
    strip = sample * td->td_stripsperimage + row / td->td_rowsperstrip;
  } else
    strip = row / td->td_rowsperstrip;
  if (strip != tif->tif_curstrip) {
    /*
     * Changing strips -- flush any data present.
     */
    if (tif->tif_rawcc > 0 && !FUN_0006a210(tif))
      return (-1);
    tif->tif_curstrip = strip;
    /*
     * Watch out for a growing image.  The value of
     * strips/image will initially be 1 (since it
     * can't be deduced until the imagelength is known).
     */
    if (strip >= td->td_stripsperimage && imagegrew)
      td->td_stripsperimage = howmany(td->td_imagelength, td->td_rowsperstrip);
    tif->tif_row = (strip % td->td_stripsperimage) * td->td_rowsperstrip;
    if (tif->tif_preencode && !(*tif->tif_preencode)(tif))
      return (-1);
    tif->tif_flags |= TIFF_POSTENCODE;
  }
  /*
   * Check strip array to make sure there's space.
   * We don't support dynamically growing files that
   * have data organized in separate bitplanes because
   * it's too painful.  In that case we require that
   * the imagelength be set properly before the first
   * write (so that the strips array will be fully
   * allocated above).
   */
  if (strip >= td->td_nstrips && !TIFFGrowStrips(tif, 1, "TIFFWriteScanline"))
    return (-1);
  /*
   * Ensure the write is either sequential or at the
   * beginning of a strip (or that we can randomly
   * access the data -- i.e. no encoding).
   */
  if (row != tif->tif_row) {
    if (tif->tif_seek) {
      if (row < tif->tif_row) {
        /*
         * Moving backwards within the same strip:
         * backup to the start and then decode
         * forward (below).
         */
        tif->tif_row = (strip % td->td_stripsperimage) * td->td_rowsperstrip;
        tif->tif_rawcp = tif->tif_rawdata;
      }
      /*
       * Seek forward to the desired row.
       */
      if (!(*tif->tif_seek)(tif, row - tif->tif_row))
        return (-1);
      tif->tif_row = row;
    } else {
      FUN_00068a30(tif->tif_name,
                   "Compression algorithm does not support random access");
      return (-1);
    }
  }
  status = (*tif->tif_encoderow)(tif, buffer, tif->tif_scanlinesize, sample);
  tif->tif_row++;
  return (status);
}

/* 0x700c0 -- encode the supplied data and write it to the specified strip.
 * There must be space for the data; we don't check if strips overlap! */
int TIFFWriteEncodedStrip(void *tif_, unsigned int strip, unsigned char *data,
                          unsigned int cc)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td = &tif->tif_dir;

  if (!TIFFWriteCheck(tif, 0, "TIFFWriteEncodedStrip"))
    return (-1);
  if (strip >= td->td_nstrips) {
    FUN_00068a30("TIFFWriteEncodedStrip", "%s: Strip %d out of range, max %d",
                 tif->tif_name, strip, td->td_nstrips);
    return (-1);
  }
  /*
   * Handle delayed allocation of data buffer.  This
   * permits it to be sized according to the directory
   * info.
   */
  if ((tif->tif_flags & TIFF_BUFFERSETUP) == 0) {
    if (!TIFFBufferSetup(tif, "TIFFWriteEncodedStrip"))
      return (-1);
    tif->tif_flags |= TIFF_BUFFERSETUP;
  }
  tif->tif_curstrip = strip;
  tif->tif_flags &= ~TIFF_POSTENCODE;
  if (tif->tif_preencode && !(*tif->tif_preencode)(tif))
    return (-1);
  if (!(*tif->tif_encodestrip)(tif, (char *)data, cc,
                               strip / td->td_stripsperimage))
    return (0);
  if (tif->tif_postencode && !(*tif->tif_postencode)(tif))
    return (-1);
  if (td->td_fillorder != tif->tif_fillorder &&
      (tif->tif_flags & TIFF_NOBITREV) == 0)
    TIFFReverseBits((unsigned char *)tif->tif_rawdata, tif->tif_rawcc);
  if (tif->tif_rawcc > 0 &&
      !TIFFAppendToStrip(tif, strip, (unsigned char *)tif->tif_rawdata,
                         tif->tif_rawcc))
    return (-1);
  tif->tif_rawcc = 0;
  tif->tif_rawcp = tif->tif_rawdata;
  return (cc);
}

/* 0x701f0 -- write the supplied data to the specified strip. */
int TIFFWriteRawStrip(void *tif_, unsigned int strip, unsigned char *data,
                      unsigned int cc)
{
  tiff_t *tif = (tiff_t *)tif_;

  if (!TIFFWriteCheck(tif, 0, "TIFFWriteRawStrip"))
    return (-1);
  if (strip >= tif->tif_dir.td_nstrips) {
    FUN_00068a30("TIFFWriteRawStrip", "%s: Strip %d out of range, max %d",
                 tif->tif_name, strip, tif->tif_dir.td_nstrips);
    return (-1);
  }
  return (TIFFAppendToStrip(tif, strip, data, cc) ? cc : -1);
}

/* 0x70260 -- encode the supplied data and write it to the specified tile,
 * clamping the write to the tile size. */
int TIFFWriteEncodedTile(void *tif_, unsigned int tile, unsigned char *data,
                         unsigned int cc)
{
  tiff_t *tif = (tiff_t *)tif_;
  tiff_directory_t *td;

  if (!TIFFWriteCheck(tif, 1, "TIFFWriteEncodedTile"))
    return (-1);
  td = &tif->tif_dir;
  if (tile >= td->td_nstrips) {
    FUN_00068a30("TIFFWriteEncodedTile", "%s: Tile %d out of range, max %d",
                 tif->tif_name, tile, td->td_nstrips);
    return (-1);
  }
  /*
   * Handle delayed allocation of data buffer.  This
   * permits it to be sized more intelligently (using
   * directory information).
   */
  if ((tif->tif_flags & TIFF_BUFFERSETUP) == 0) {
    if (!TIFFBufferSetup(tif, "TIFFWriteEncodedTile"))
      return (-1);
    tif->tif_flags |= TIFF_BUFFERSETUP;
  }
  tif->tif_curtile = tile;
  /*
   * Compute tiles per row & per column to compute
   * current row and column
   */
  tif->tif_row =
    (tile % howmany(td->td_imagelength, td->td_tilelength)) * td->td_tilelength;
  tif->tif_col =
    (tile % howmany(td->td_imagewidth, td->td_tilewidth)) * td->td_tilewidth;

  tif->tif_flags &= ~TIFF_POSTENCODE;
  if (tif->tif_preencode && !(*tif->tif_preencode)(tif))
    return (-1);
  /*
   * Clamp write amount to the tile size.  This is mostly
   * done so that callers can pass in some large number
   * (e.g. -1) and have the tile size used instead.
   */
  if (cc > tif->tif_tilesize)
    cc = tif->tif_tilesize;
  if (!(*tif->tif_encodetile)(tif, (char *)data, cc,
                              tile / td->td_stripsperimage))
    return (0);
  if (tif->tif_postencode && !(*tif->tif_postencode)(tif))
    return (-1);
  if (td->td_fillorder != tif->tif_fillorder &&
      (tif->tif_flags & TIFF_NOBITREV) == 0)
    TIFFReverseBits((unsigned char *)tif->tif_rawdata, tif->tif_rawcc);
  if (tif->tif_rawcc > 0 &&
      !TIFFAppendToStrip(tif, tile, (unsigned char *)tif->tif_rawdata,
                         tif->tif_rawcc))
    return (-1);
  tif->tif_rawcc = 0;
  tif->tif_rawcp = tif->tif_rawdata;
  return (cc);
}

/* 0x703f0 -- write the supplied data to the specified tile. */
int TIFFWriteRawTile(void *tif_, unsigned int tile, unsigned char *data,
                     unsigned int cc)
{
  tiff_t *tif = (tiff_t *)tif_;

  if (!TIFFWriteCheck(tif, 1, "TIFFWriteRawTile"))
    return (-1);
  if (tile >= tif->tif_dir.td_nstrips) {
    FUN_00068a30("TIFFWriteRawTile", "%s: Tile %d out of range, max %d",
                 tif->tif_name, tile, tif->tif_dir.td_nstrips);
    return (-1);
  }
  return (TIFFAppendToStrip(tif, tile, data, cc) ? cc : -1);
}

/* 0x70460 -- write and compress a tile of data selected by (x,y,z,s).
 * Upstream passes (x, y, z, s) to a TIFFComputeTile whose K&R definition
 * names its parameters (x, y, s, z); the binary pushes the caller's order
 * unchanged (0x70497-0x7049e), so the call is transcribed as written.
 *
 * NB: A tile size of -1 is used instead of tif_tilesize knowing that
 *     TIFFWriteEncodedTile will clamp this to the tile size. This is done
 *     because the tile size may not be defined until after the output
 *     buffer is setup in TIFFBufferSetup. */
int TIFFWriteTile(void *tif, unsigned char *buf, unsigned long x,
                  unsigned long y, unsigned long z, unsigned int s)
{
  if (!TIFFCheckTile(tif, x, y, z, s))
    return (-1);
  return (TIFFWriteEncodedTile(tif, TIFFComputeTile(tif, x, y, z, s), buf,
                               (unsigned int)-1));
}
