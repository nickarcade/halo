#define FILE_REF_MAGIC 0x66696C6F

/* File-reference flag families. See docs/halocea/README.md for the corpus and
 * .claude/skills/naming-confidence for the name_source tiers.
 *
 * All four BOUNDS were read out of this build before the corpus was consulted,
 * and all four agree with it exactly:
 *
 *   reference_info flags  files.c:0x1fe          rejects & 0xfffe  => 1
 *   name flags            files.c:0xba           rejects & 0xfff0  => 4
 *   find_files flags      files_windows.c:0x224  rejects & ~3      => 2
 *   permission flags      files_windows.c:0x134  rejects & ~7      => 3
 *
 * Unusually for this lane, most MEMBER names are T1 here too: 2276 stamps the
 * identifiers themselves into assert strings, and those asserts also pin the
 * bit values.
 *
 *   _has_filename_bit           files.c:0x8a and files_windows.c:0x225 assert
 *                               !TEST_FLAG(info->flags, _has_filename_bit)
 *                               against `& 1`                       => bit 0
 *   _name_directory_bit         0xbc asserts flags !=
 * (FLAG(_name_directory_bit) _name_extension_bit         |
 * FLAG(_name_extension_bit)) against `== 9`, so the pair is bits 0 and 3; 0xbd
 * asserts _name_directory_bit vs _name_parent_directory_bit against `(flags&1)
 * && (flags&2)`, fixing directory=0, so extension=3. _name_parent_directory_bit
 * from the same 0xbd pair                => bit 1 _name_filename_bit T1 BY
 * EXHAUSTION: bits 0, 1 and 3 are each named verbatim above and
 * NUMBER_OF_NAME_FLAGS is 4 (proven by the 0xfff0 reject), so bit 2 is forced.
 * Both halves are needed for this to be T1 rather than a guess.
 *   _permission_read_bit        0x135 asserts flags &
 * (FLAG(_permission_read_bit) _permission_write_bit       |
 * FLAG(_permission_write_bit)) against `& 3`; 0x136 asserts
 * _permission_write_bit against
 *                               `& 2`, so write=1 and read=0.
 *   _permission_append_bit      named at 0x136 against `& 4`           => bit 2
 *                               Corroborated by file_open (files_windows.c):
 *                               bit 0 maps to GENERIC_READ,
 *                               bit 1 to GENERIC_WRITE, bit 2 to a seek-to-end.
 *
 * find_files members carry no 2276 string and are name_source: halocea, T2 —
 * DB-verified there (types_enum_values _3BB901B9596B139CD611B9F64EADD532), and
 * consistent with the existing FIND_FILES_*_BIT masks (now in files_windows.c),
 * which this commit deliberately leaves alone (they are masks under a *_BIT
 * name; correcting that is a rename, not a constants change, and belongs in
 * its own commit).
 */
enum reference_info_flags {
  _has_filename_bit = 0,
  NUMBER_OF_REFERENCE_INFO_FLAGS = 1
};

enum name_flags {
  _name_directory_bit = 0,
  _name_parent_directory_bit = 1,
  _name_filename_bit = 2,
  _name_extension_bit = 3,
  NUMBER_OF_NAME_FLAGS = 4
};

/* Bits at or above each bound; what the VALID_FLAGS guards reject. Spelled to
 * keep the original immediate: the reference-info and name sites load a 16-bit
 * word. The find-files and permission masks (32-bit) live in files_windows.c. */
#define REFERENCE_INFO_FLAGS_INVALID_MASK 0xfffe
#define NAME_FLAGS_INVALID_MASK 0xfff0

/**
 * file_reference_create - initialize a file reference in place.
 *
 * Asserts the destination is non-NULL and that location is in
 * [NONE, NUMBER_OF_FILE_REFERENCE_LOCATIONS) == [-1, 2), then zeroes
 * the whole 0x10C-byte structure, stores the location and finally the
 * FILE_REFERENCE_SIGNATURE magic. Store order (location before magic)
 * follows the binary (MOV [ESI+6],DI at 0x199486 then MOV [ESI],imm32
 * at 0x19948b).
 *
 * Returns the initialized reference (MOV EAX,ESI at 0x199491).
 */
file_ref_t *file_reference_create(file_ref_t *info, int16_t location)
{
  if (info == NULL) {
    display_assert("info", "c:\\halo\\SOURCE\\tag_files\\files.c", 0x5b, true);
    system_exit(-1);
  }
  if (location < -1 || location >= 2) {
    display_assert("location>=NONE && "
                   "location<NUMBER_OF_FILE_REFERENCE_LOCATIONS",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x5c, true);
    system_exit(-1);
  }
  csmemset(info, 0, 0x10c);
  info->unk_6 = location;
  info->magic = FILE_REF_MAGIC;
  return info;
}

/**
 * find_files - enumerate files matching a directory reference.
 *
 * Begins a file search using the given flags and directory reference,
 * then iterates up to max_count entries, storing each result in the
 * results array (stride = sizeof(file_ref_t) = 0x10C).
 * Returns the number of files found.
 */
int16_t find_files(int flags, file_ref_t *dir, int16_t max_count,
                   file_ref_t *results)
{
  int16_t count = 0;

  if (max_count < 1) {
    display_assert("maximum_count>0", "c:\\halo\\SOURCE\\tag_files\\files.c",
                   0x101, true);
    system_exit(-1);
  }
  if (results == NULL) {
    display_assert("references", "c:\\halo\\SOURCE\\tag_files\\files.c", 0x102,
                   true);
    system_exit(-1);
  }

  find_files_begin(flags, dir);

  if (max_count > 0) {
    do {
      if (!find_files_next(&results[count], 0))
        return count;
      count++;
    } while (count < max_count);
  }

  return count;
}

/**
 * file_read_into_buffer - read an entire file into a newly allocated buffer.
 *
 * Opens the file for reading, queries its size via file_get_eof, allocates
 * a buffer with debug_malloc, reads the full contents into it, then closes
 * the file. On success, writes the file size to *size_out and returns
 * the allocated buffer. On any failure (open, alloc, or read), returns NULL.
 * If the read fails after allocation, the buffer is freed before returning.
 */
void *file_read_into_buffer(file_ref_t *file_ref, int *size_out)
{
  void *buffer;

  buffer = NULL;
  if (file_open(file_ref, 1)) {
    *size_out = file_get_eof(file_ref);
    buffer =
      debug_malloc(*size_out, 0, "c:\\halo\\SOURCE\\tag_files\\files.c", 0x118);
    if (buffer != NULL) {
      if (!file_read(file_ref, *size_out, buffer)) {
        debug_free(buffer, "c:\\halo\\SOURCE\\tag_files\\files.c", 0x11e);
        buffer = NULL;
      }
    }
    file_close(file_ref);
  }
  return buffer;
}

/**
 * file_printf (0x1995c0) - format a string and append it to an open file.
 *
 * Formats into a 1024-byte stack buffer with vsprintf, writes csstrlen(buffer)
 * bytes to the file, then truncates the file at the new position (the
 * file_get_position result is the second argument to file_set_eof). Does
 * nothing when the format string is NULL ([EBP+0xc] TEST/JZ at 0x1995cc).
 *
 * [EBP+8] = info (kept in ESI), [EBP+0xc] = format, [EBP+0x10] = first vararg,
 * so the arglist is (char *)&format + 4 (LEA ECX,[EBP+0x10] at 0x1995d1).
 * Return values of file_write and file_set_eof are discarded.
 */
void file_printf(file_ref_t *info, const char *format, ...)
{
  char buffer[1024];
  char *arglist;

  if (format != NULL) {
    arglist = (char *)&format + 4;
    vsprintf(buffer, format, arglist);
    file_write(info, csstrlen(buffer), buffer);
    file_set_eof(info, file_get_position(info));
  }
}

/**
 * file_reference_verify - validate a file_ref_t pointer.
 *
 * Checks that the pointer is non-NULL, the magic signature matches
 * FILE_REFERENCE_SIGNATURE (0x66696C6F), flags are valid (only bit 0
 * allowed in the reference info flags), and the location field is in
 * the range [-1, 1] (NONE through NUMBER_OF_FILE_REFERENCE_LOCATIONS).
 *
 * Returns the validated pointer.
 */
file_ref_t *file_reference_verify(file_ref_t *info)
{
  if (info == NULL) {
    display_assert("info", "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1fc, true);
    system_exit(-1);
  }
  if (info->magic != FILE_REF_MAGIC) {
    display_assert("info->signature==FILE_REFERENCE_SIGNATURE",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1fd, true);
    system_exit(-1);
  }
  if ((*(uint16_t *)&info->unk_4[0] & REFERENCE_INFO_FLAGS_INVALID_MASK) != 0) {
    display_assert("VALID_FLAGS(info->flags, NUMBER_OF_REFERENCE_INFO_FLAGS)",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1fe, true);
    system_exit(-1);
  }
  if (info->unk_6 < -1 || info->unk_6 >= 2) {
    display_assert("info->location>=NONE && "
                   "info->location<NUMBER_OF_FILE_REFERENCE_LOCATIONS",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1ff, true);
    system_exit(-1);
  }
  return info;
}

/**
 * file_reference_copy - copy a validated file reference.
 *
 * Verifies the source reference (return value discarded; the assert side
 * effects are the point), then copies 0x108 bytes from source to
 * destination. Note the copy size is 0x108, not sizeof(file_ref_t)
 * (0x10C) — the binary uses the literal, so the trailing 4 bytes of the
 * destination are left untouched.
 *
 * Returns the destination pointer (MOV EAX,ESI at 0x1996ef, where ESI
 * was reloaded from [EBP+8]).
 */
file_ref_t *file_reference_copy(file_ref_t *destination, file_ref_t *source)
{
  file_reference_verify(source);
  csmemcpy(destination, source, 0x108);
  return destination;
}

/**
 * file_reference_add_directory - append a directory component to a file
 * reference's path.
 *
 * Verifies the file reference, asserts that the directory string is
 * non-NULL and that the _has_filename_bit flag is not set (cannot add a
 * directory component after a filename has been set), then appends the
 * directory to the internal path buffer using path_add_directory.
 *
 * Returns the file reference pointer.
 */
file_ref_t *file_reference_add_directory(file_ref_t *info,
                                         const char *directory)
{
  file_ref_t *ref;

  ref = file_reference_verify(info);
  if (directory == NULL) {
    display_assert("directory", "c:\\halo\\SOURCE\\tag_files\\files.c", 0x89,
                   true);
    system_exit(-1);
  }
  if (ref->unk_4[0] & (1 << _has_filename_bit)) {
    display_assert("!TEST_FLAG(info->flags, _has_filename_bit)",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x8a, true);
    system_exit(-1);
  }
  path_add_directory(ref->unk_8, directory);
  return info;
}

/**
 * file_reference_set_name - set the filename component of a file reference.
 *
 * Verifies the file reference and asserts that the name string is
 * non-NULL. If the _has_filename_bit flag is already set, strips the
 * existing filename from the path (via path_remove_filename) before
 * appending the new name with path_add_directory. Sets the
 * _has_filename_bit flag afterward.
 *
 * Returns the file reference pointer.
 */
file_ref_t *file_reference_set_name(file_ref_t *info, const char *name)
{
  file_ref_t *ref;

  ref = file_reference_verify(info);
  if (name == NULL) {
    display_assert("name", "c:\\halo\\SOURCE\\tag_files\\files.c", 0x97, true);
    system_exit(-1);
  }
  if (ref->unk_4[0] & 1) {
    path_remove_filename(ref->unk_8);
  }
  path_add_directory(ref->unk_8, name);
  ref->unk_4[0] |= 1;
  return info;
}

/* 0x1997f0 — return the location field (unk_6) of a validated file reference.
 * The location is a small integer: -1=relative, 0=E:, 1=D:, etc. */
int16_t file_reference_get_location(file_ref_t *info)
{
  file_ref_t *ref;
  ref = file_reference_verify(info);
  return ref->unk_6;
}

/**
 * file_reference_get_name - extract a formatted name string from a file
 * reference.
 *
 * Validates the file reference, builds the full path, splits it into
 * components (directory, parent directory, filename, extension), and
 * reassembles the requested parts based on the flags bitmask:
 *   bit 0: directory
 *   bit 1: parent directory
 *   bit 2: filename (stem)
 *   bit 3: extension
 *
 * Returns name_out.
 */
char *file_reference_get_name(file_ref_t *info, int flags, char *name_out)
{
  file_ref_t *ref;
  char path[256];
  char *dir_part;
  char *parent_part;
  char *file_part;
  char *ext_part;
  int has_dir_flag;

  ref = file_reference_verify(info);

  csmemset(path, 0, sizeof(path));

  if (name_out == NULL) {
    display_assert("name", "c:\\halo\\SOURCE\\tag_files\\files.c", 0xb9, true);
    system_exit(-1);
  }
  if ((*(uint16_t *)&ref->unk_4[0] & NAME_FLAGS_INVALID_MASK) != 0) {
    display_assert("VALID_FLAGS(info->flags, NUMBER_OF_NAME_FLAGS)",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0xba, true);
    system_exit(-1);
  }
  if (flags == 0) {
    display_assert("flags", "c:\\halo\\SOURCE\\tag_files\\files.c", 0xbb, true);
    system_exit(-1);
  } else if (flags ==
             ((1 << _name_directory_bit) | (1 << _name_extension_bit))) {
    display_assert(
      "flags!=(FLAG(_name_directory_bit)|FLAG(_name_extension_bit))",
      "c:\\halo\\SOURCE\\tag_files\\files.c", 0xbc, true);
    system_exit(-1);
  }
  if ((flags & (1 << _name_directory_bit)) &&
      (flags & (1 << _name_parent_directory_bit))) {
    display_assert(
      "!TEST_FLAG(flags, _name_directory_bit) || !TEST_FLAG(flags, "
      "_name_parent_directory_bit)",
      "c:\\halo\\SOURCE\\tag_files\\files.c", 0xbd, true);
    system_exit(-1);
  }

  has_dir_flag = flags & (1 << _name_directory_bit);

  path_from_file_reference(ref->unk_6, ref->unk_8, path);
  path_split(path, &dir_part, &parent_part, &file_part, &ext_part,
             (uint8_t)ref->unk_4[0] & 1);

  *name_out = '\0';

  if (has_dir_flag) {
    path_add_directory(name_out, dir_part);
  }
  if (flags & (1 << _name_parent_directory_bit)) {
    path_add_directory(name_out, parent_part);
  }
  if (flags & (1 << _name_filename_bit)) {
    path_add_directory(name_out, file_part);
  }
  if (flags & (1 << _name_extension_bit)) {
    path_add_extension(name_out, ext_part);
  }

  return name_out;
}

/* 0x1999a0 — return true if two file references point to the same path.
 * Compares the location field (unk_6) and path string (unk_8) of both
 * validated references. */
bool file_references_equal(file_ref_t *info1, file_ref_t *info2)
{
  file_ref_t *ref1;
  file_ref_t *ref2;
  bool equal;

  ref1 = file_reference_verify(info1);
  ref2 = file_reference_verify(info2);
  equal = false;
  if (ref1->unk_6 == ref2->unk_6 &&
      csstrcmp(ref1->unk_8, ref2->unk_8) == 0) {
    equal = true;
  }
  return equal;
}

/**
 * file_reference_create_from_path - initialize a file reference from a path.
 *
 * Zeroes the file_ref_t, sets the magic and location fields, then either
 * adds the directory as a path component (a3=true) or sets it as the
 * base name (a3=false).
 */
file_ref_t *file_reference_create_from_path(file_ref_t *info,
                                            const char *directory, bool a3)
{
  if (info == NULL) {
    display_assert("info", "c:\\halo\\SOURCE\\tag_files\\files.c", 0x5B, true);
    system_exit(-1);
  }

  csmemset(info, 0, sizeof(*info));
  info->magic = FILE_REF_MAGIC;
  info->unk_6 = -1;

  if (a3) {
    file_reference_add_directory(info, directory);
  } else {
    file_reference_set_name(info, directory);
  }

  return info;
}

/**
 * directory_create_or_delete_contents - ensure a directory exists and is empty.
 *
 * Builds a file reference for the directory (inlined create-from-path with
 * a3=true). If the directory already exists, enumerates its contents and
 * deletes every entry; otherwise creates it.
 */
void directory_create_or_delete_contents(const char *directory_name)
{
  file_ref_t entry;
  file_ref_t info;

  csmemset(&info, 0, sizeof(info));
  info.magic = FILE_REF_MAGIC;
  info.unk_6 = -1;
  file_reference_add_directory(&info, directory_name);

  if (file_exists(&info)) {
    find_files_begin(0, &info);
    if (find_files_next(&entry, 0)) {
      do {
        file_delete(&entry);
      } while (find_files_next(&entry, 0));
    }
  } else {
    file_create(&info);
  }
}

/* Data-store record layout, from the assert strings at 0x199bd4/0x199c04:
 *   DATASTORE_MAX_DATA_SIZE and DATASTORE_MAX_FIELD_NAME_SIZE are both 255
 *   (the compares are against 0xff). The record stride 0x1fe and the total
 *   file size 0x18e70 are read from the binary; the field count 200 (0xc8) is
 *   the loop bound at 0x199ced and satisfies 200 * 0x1fe == 0x18e70.
 * Each record is [field_name: 255 bytes][data: 255 bytes]; the data starts at
 * +0xff, which is the offset used by the csmemcpy at 0x199d0d. */
#define DATASTORE_MAX_FIELD_NAME_SIZE 255
#define DATASTORE_MAX_DATA_SIZE 255
#define DATASTORE_FIELD_COUNT 200
#define DATASTORE_RECORD_SIZE \
  (DATASTORE_MAX_FIELD_NAME_SIZE + DATASTORE_MAX_DATA_SIZE)
#define DATASTORE_FILE_SIZE (DATASTORE_FIELD_COUNT * DATASTORE_RECORD_SIZE)

/**
 * datastore_read_field (0x199b20) - read one named field out of a data-store
 * file into the caller's buffer.
 *
 * Reads the whole file, rejects it unless it is exactly DATASTORE_FILE_SIZE
 * bytes, then scans up to DATASTORE_FIELD_COUNT fixed-size records for one
 * whose name matches field_name, stopping early at the first record with an
 * empty name. On a hit it copies `length` bytes of that record's data area to
 * `buffer` and reports true.
 *
 * Parameter names are from the assert strings (file_name, field_name,
 * length); the function name itself has no string evidence and is derived
 * from the DATASTORE_* assert macros.
 *
 * Divergence note: the original keeps its result flag in the (dead) high byte
 * of the field_name parameter slot at [EBP+0xf] and only ever stores to it on
 * the hit path at 0x199d15 — the not-found and bad-size exits return whatever
 * that byte held, i.e. the high byte of the field_name pointer, which is 0 for
 * every reachable string address in this build. We initialize the flag to
 * false, which reproduces the observed behavior without relying on that
 * aliasing. There are no callers in this build (no xrefs to 0x199b20).
 */
bool datastore_read_field(const char *file_name, const char *field_name,
                          int length, void *buffer)
{
  file_ref_t info;
  char *data;
  char *cursor;
  int index;
  int size;
  bool found;

  size = 0;
  found = false;

  if (file_name == NULL) {
    display_assert("NULL != file_name", "c:\\halo\\SOURCE\\tag_files\\files.c",
                   0x171, true);
    system_exit(-1);
  }
  if (field_name == NULL) {
    display_assert("NULL != field_name", "c:\\halo\\SOURCE\\tag_files\\files.c",
                   0x172, true);
    system_exit(-1);
  }
  if (file_name[0] == '\0') {
    display_assert("'\\0' != file_name[0]",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x173, true);
    system_exit(-1);
  }
  if (field_name[0] == '\0') {
    display_assert("'\\0' != field_name[0]",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x174, true);
    system_exit(-1);
  }
  if (length >= DATASTORE_MAX_DATA_SIZE) {
    display_assert("length < DATASTORE_MAX_DATA_SIZE",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x175, true);
    system_exit(-1);
  }
  if ((unsigned int)csstrlen(field_name) >= DATASTORE_MAX_FIELD_NAME_SIZE) {
    display_assert("strlen(field_name) < DATASTORE_MAX_FIELD_NAME_SIZE",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x176, true);
    system_exit(-1);
  }

  csmemset(&info, 0, sizeof(info));
  info.magic = FILE_REF_MAGIC;
  info.unk_6 = -1;
  file_reference_set_name(&info, file_name);

  if (file_exists(&info)) {
    data = (char *)file_read_into_buffer(&info, &size);
    if (data == NULL) {
      file_delete(&info);
    }
    if (size != DATASTORE_FILE_SIZE) {
      debug_free(data, "c:\\halo\\SOURCE\\tag_files\\files.c", 0x185);
      file_delete(&info);
      return found;
    }
    if (data != NULL) {
      for (index = 0, cursor = data; index < DATASTORE_FIELD_COUNT;
           index++, cursor += DATASTORE_RECORD_SIZE) {
        if (csstrcmp(cursor, field_name) == 0) {
          csmemcpy(buffer,
                   data + index * DATASTORE_RECORD_SIZE +
                     DATASTORE_MAX_FIELD_NAME_SIZE,
                   length);
          found = true;
          break;
        }
        if (*cursor == '\0') {
          break;
        }
      }
      debug_free(data, "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1a0);
    }
  }

  return found;
}

/**
 * datastore_read (0x199d40) - store one named field into a data-store file.
 *
 * Despite the kb.json name (CEA PDB line-containment, marked probable), the
 * binary body writes: it loads or creates the DATASTORE_FILE_SIZE record file,
 * finds the first record whose name matches field_name or whose name is empty,
 * copies field_name and `length` bytes of `buffer` into that record, then
 * re-creates/opens the file, writes the whole buffer back and closes it. The
 * name is therefore UNVERIFIED for this address; the behavior below is taken
 * from the disassembly at 0x199d40-0x19a00f only.
 *
 * Parameter names come from the assert strings (file_name, field_name,
 * length); `buffer` is the [EBP+0x14] slot passed as the csmemcpy source at
 * 0x199f68. The final `success` assert at 0x1ef halts when no free or matching
 * record was found. There are no callers in this build (no xrefs to 0x199d40).
 */
bool datastore_read(const char *file_name, const char *field_name, int length,
                    void *buffer)
{
  file_ref_t info;
  char *data;
  char *cursor;
  int index;
  int size;
  bool success;

  success = false;
  size = 0;

  if (file_name == NULL) {
    display_assert("NULL != file_name", "c:\\halo\\SOURCE\\tag_files\\files.c",
                   0x1AE, true);
    system_exit(-1);
  }
  if (field_name == NULL) {
    display_assert("NULL != field_name", "c:\\halo\\SOURCE\\tag_files\\files.c",
                   0x1AF, true);
    system_exit(-1);
  }
  if (file_name[0] == '\0') {
    display_assert("'\\0' != file_name[0]",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1B0, true);
    system_exit(-1);
  }
  if (field_name[0] == '\0') {
    display_assert("'\\0' != field_name[0]",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1B1, true);
    system_exit(-1);
  }
  if (length >= DATASTORE_MAX_DATA_SIZE) {
    display_assert("length < DATASTORE_MAX_DATA_SIZE",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1B2, true);
    system_exit(-1);
  }
  if ((unsigned int)csstrlen(field_name) >= DATASTORE_MAX_FIELD_NAME_SIZE) {
    display_assert("strlen(field_name) < DATASTORE_MAX_FIELD_NAME_SIZE",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1B3, true);
    system_exit(-1);
  }

  csmemset(&info, 0, sizeof(info));
  info.magic = FILE_REF_MAGIC;
  info.unk_6 = -1;
  file_reference_set_name(&info, file_name);

  data = NULL;
  if (file_exists(&info)) {
    data = (char *)file_read_into_buffer(&info, &size);
    if (data == NULL) {
      file_delete(&info);
    }
    if (size != DATASTORE_FILE_SIZE) {
      debug_free(data, "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1C2);
      file_delete(&info);
      data = NULL;
    }
  }
  if (data == NULL) {
    data = (char *)debug_malloc(DATASTORE_FILE_SIZE, false,
                                "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1CD);
    if (data == NULL) {
      goto failure; /* 0x199f00: shares the final "success" assert arm */
    }
    csmemset(data, 0, DATASTORE_FILE_SIZE);
  }

  index = 0;
  cursor = data;
  do {
    if (*cursor == '\0' || csstrcmp(cursor, field_name) == 0) {
      csstrcpy(data + index * DATASTORE_RECORD_SIZE, field_name);
      csmemcpy(data + index * DATASTORE_RECORD_SIZE +
                 DATASTORE_MAX_FIELD_NAME_SIZE,
               buffer, length);
      success = true;
      break;
    }
    index++;
    cursor += DATASTORE_RECORD_SIZE;
  } while (index < DATASTORE_FIELD_COUNT);

  if (!file_exists(&info)) {
    file_create(&info);
  }
  if (file_open(&info, 2)) {
    file_write(&info, DATASTORE_FILE_SIZE, data);
    file_close(&info);
  }
  debug_free(data, "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1EC);

  if (success) {
    return success;
  }

failure:
  display_assert("success", "c:\\halo\\SOURCE\\tag_files\\files.c", 0x1EF,
                 true);
  system_exit(-1);
  return false;
}
