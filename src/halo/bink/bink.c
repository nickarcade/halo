/* RAD Bink 1.0 for Xbox (bink.obj).  Public entry points called by
 * bink_playback.c.  Structure layouts and the matching bodies of the
 * 2342 reconstruction are in PAL-2342 libs/binkxbox/binkread.c; each body here
 * is re-derived from the 2276 disassembly and differs from PAL where 2276 does.
 *
 * MSVC 7.1 /Oi intrinsics memset() to the inline `rep stosd` clears the
 * original uses (cseries.c provides the clang-build body); declared extern as
 * in hud_weapon.c / game.c. */
extern void *__cdecl memset(void *, int, unsigned int);

/* mult64anddiv (RAD.H): an inline __asm helper, 32x32->64 MUL then 64/32 DIV.
 * Every site inlines it as `mov eax,m1 / mov ecx,m2 / mul ecx / mov ecx,d /
 * div ecx` (e.g. 0x22f578-0x22f586).  A C `(u64)a*b/d` would need __aulldiv,
 * which this build does not link. */
#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(disable : 4035) /* result returned in EAX */
static __inline unsigned long mult64anddiv(unsigned long m1, unsigned long m2,
                                           unsigned long d)
{
  __asm {
    mov eax, m1
    mov ecx, m2
    mul ecx
    mov ecx, d
    div ecx
  }
}
#pragma warning(default : 4035)
#else
static __inline unsigned long mult64anddiv(unsigned long m1, unsigned long m2,
                                           unsigned long d)
{
  unsigned long quotient;
  unsigned long high;

  __asm__("mull %3\n\tdivl %4"
          : "=a"(quotient), "=&d"(high)
          : "0"(m1), "rm"(m2), "rm"(d)
          : "cc");
  return quotient;
}
#endif

/* BinkOpen's sample-rate rescale is plain C 64-bit math, compiled to
 * _allmul (0x1dd620) and _aulldiv (0x1dd770) at 0x230d58/0x230d70.  The
 * clang build does not link __aulldiv, so it divides by shift-subtract. */
#if defined(_MSC_VER) && !defined(__clang__)
static __inline unsigned long udiv64(uint64_t numerator, uint64_t divisor)
{
  return (unsigned long)(numerator / divisor);
}
#else
static unsigned long udiv64(uint64_t numerator, uint64_t divisor)
{
  uint64_t quotient;
  uint64_t remainder;
  int bit;

  quotient = 0;
  remainder = 0;
  for (bit = 63; bit >= 0; bit--) {
    remainder = (remainder << 1) | ((numerator >> bit) & 1);
    if (remainder >= divisor) {
      remainder -= divisor;
      quotient |= (uint64_t)1 << bit;
    }
  }
  return (unsigned long)quotient;
}
#endif

/* BinkSetSoundSystem in the RAD API; kb name kept. */
long __stdcall BinkSoundUseDirectSound(void *dsound_proc, void *dsound_handle)
{
  bink_sound_system_open_proc open;
  bink_sound_open_proc result;

  open = (bink_sound_system_open_proc)dsound_proc;
  if (open == NULL) {
    return 0;
  }
  if (sound_system_open != NULL) {
    if (sound_system_open == open) {
      goto call_open;
    }
    if (sound_open_count != 0) {
      return 0;
    }
  }
  sound_system_open = open;
call_open:
  result = sound_system_open((unsigned long)dsound_handle);
  if (result != NULL) {
    sound_open = result;
  }
  return sound_open != NULL;
}

/* BinkSetIOSize in the RAD API: BinkOpen consumes forced_io_size once when
 * opened with flag 0x1000000 (0x230a5c). kb name kept. */
void __stdcall BinkSetMemory(unsigned long size)
{
  forced_io_size = size;
}

/* Surface types (flags & 0xf) follow the conv_strs order in PAL binkxbox.c:
 * 3=32, 5=32A, 7=4444, 8=5551, 9..12=16-bit; 4/6 copy nothing.  Returns 1
 * when the blit was skipped, or when a skip was due but 0x80000 (NOSKIP)
 * forced the copy anyway (0x22e6af). */
int __stdcall BinkCopyToBuffer(BINK *bink, void *dest, int pitch, int height,
                               int x, int y, unsigned int flags)
{
  int skipped;
  int masked;
  unsigned long surface;
  unsigned long now;
  unsigned long target;
  unsigned long behind;
  const unsigned char *mask;

  skipped = 0;
  if (bink == NULL) {
    return 0;
  }
  if (dest == NULL) {
    return 0;
  }
  flags |= bink->OpenFlags & 0x70030000;
  surface = flags & 0xf;
  bink->io.Working = 1;
  FUN_00234110(surface);
  if (pitch < 0) {
    dest = (char *)dest - (height - y - 1) * pitch;
    y = 0;
  }
  if (bink->trackindex != -1 && bink->FrameRate != 0 && bink->Paused == 0) {
    inittimer(bink);
    now = FUN_002328c0();
    target = mult64anddiv(
      mult64anddiv(bink->playedframes * 1000 - bink->startframe * 1000,
                   bink->FrameRateDiv, bink->FrameRate),
      bink->sound.VideoScale, 0x10000);
    behind = now - bink->big_sound_skip_adj - bink->starttime;
    if (behind < target) {
      bink->skippedlastblit = 0;
    } else if (behind - target <= bink->resynctime) {
      bink->field_3d0 = 0;
      bink->skippedlastblit = 0;
    } else if (++bink->field_3d0 > 3) {
      bink->field_3d0 = 0;
    } else if ((flags & 0x80000) == 0 && (bink->OpenFlags & 0x80000) == 0) {
      bink->skippedlastblit = 1;
      bink->skippedblits++;
      bink->io.Working = 0;
      return 1;
    } else {
      skipped = 1;
    }
  }
  if (bink->preloadptr == NULL && bink->io.DoingARead == 0 &&
      (bink->io.CurBufUsed + 1) * 100 / (bink->io.CurBufSize + 1) < 75) {
    if (FUN_00232430(io_handler, &bink->io.callback) != 0) {
      bink->io.Idle(&bink->io);
      FUN_002324f0(io_handler, &bink->io.callback);
    } else if ((bink->OpenFlags & 0x8000000) == 0) {
      FUN_00232600();
    }
  }
  bink->lastblitflags = flags;
  /* 0x22e760: inline strlen of dirty_mask (0-terminated at field_e4 by
   * BinkOpen); a shorter length means some block is clean. */
  masked = 0;
  if ((int)flags >= 0) {
    mask = bink->dirty_mask;
    do {
    } while (*mask++ != 0);
    if ((unsigned long)(mask - (bink->dirty_mask + 1)) < bink->field_e4) {
      masked = 1;
    }
  }
  if (!masked) {
    switch (surface) {
    case 5:
      if (bink->field_c4[0] != NULL) {
        FUN_00246030(dest, x, y, pitch, bink->field_bc[bink->field_b8], 0, 0,
                     bink->field_214, bink->field_218, bink->dirty_width,
                     bink->dirty_height, bink->field_c4[bink->field_b8], flags);
        break;
      }
      /* fall through */
    case 3:
      FUN_00244b40(dest, x, y, pitch, bink->field_bc[bink->field_b8], 0, 0,
                   bink->field_214, bink->field_218, bink->dirty_width,
                   bink->dirty_height, flags);
      break;
    case 8:
      if (bink->field_c4[0] != NULL) {
        FUN_00249ab0(dest, x, y, pitch, bink->field_bc[bink->field_b8], 0, 0,
                     bink->field_214, bink->field_218, bink->dirty_width,
                     bink->dirty_height, bink->field_c4[bink->field_b8], flags);
        break;
      }
      /* fall through (see the masked switch) */
    case 7:
      if (bink->field_c4[0] != NULL) {
        FUN_00248980(dest, x, y, pitch, bink->field_bc[bink->field_b8], 0, 0,
                     bink->field_214, bink->field_218, bink->dirty_width,
                     bink->dirty_height, bink->field_c4[bink->field_b8], flags);
        break;
      }
      /* fall through */
    case 9:
    case 10:
    case 11:
    case 12:
      FUN_002473f0(dest, x, y, pitch, bink->field_bc[bink->field_b8], 0, 0,
                   bink->field_214, bink->field_218, bink->dirty_width,
                   bink->dirty_height, flags);
      break;
    }
  } else {
    switch (surface) {
    case 5:
      if (bink->field_c4[0] != NULL) {
        FUN_00246080(dest, x, y, pitch, bink->dirty_mask, bink->dirty_pitch,
                     bink->field_bc[bink->field_b8], bink->field_214,
                     bink->field_218, bink->dirty_width, bink->dirty_height,
                     bink->field_c4[bink->field_b8], flags);
        break;
      }
      /* fall through */
    case 3:
      FUN_00244b90(dest, x, y, pitch, bink->dirty_mask, bink->dirty_pitch,
                   bink->field_bc[bink->field_b8], bink->field_214,
                   bink->field_218, bink->dirty_width, bink->dirty_height,
                   flags);
      break;
    case 8:
      if (bink->field_c4[0] != NULL) {
        FUN_00249b00(dest, x, y, pitch, bink->dirty_mask, bink->dirty_pitch,
                     bink->field_bc[bink->field_b8], bink->field_214,
                     bink->field_218, bink->dirty_width, bink->dirty_height,
                     bink->field_c4[bink->field_b8], flags);
        break;
      }
      /* fall through: the binary jumps straight to the 16-bit body
       * (0x22e9e4 -> 0x22ea93); case 7 re-tests the same pointer */
    case 7:
      if (bink->field_c4[0] != NULL) {
        FUN_002489d0(dest, x, y, pitch, bink->dirty_mask, bink->dirty_pitch,
                     bink->field_bc[bink->field_b8], bink->field_214,
                     bink->field_218, bink->dirty_width, bink->dirty_height,
                     bink->field_c4[bink->field_b8], flags);
        break;
      }
      /* fall through */
    case 9:
    case 10:
    case 11:
    case 12:
      FUN_00247440(dest, x, y, pitch, bink->dirty_mask, bink->dirty_pitch,
                   bink->field_bc[bink->field_b8], bink->field_214,
                   bink->field_218, bink->dirty_width, bink->dirty_height,
                   flags);
      break;
    }
  }
  bink->io.Working = 0;
  return skipped;
}

/* Returns 1 on a read error, else 0; no caller reads it. */
long __stdcall BinkDoFrame(BINK *bink)
{
  unsigned char *comp;
  const unsigned char *input;
  void *output;
  unsigned long track_size;
  unsigned long left;
  unsigned long space;
  unsigned long amt;
  unsigned long tail;
  unsigned long now;
  unsigned long end;
  long i;

  if (bink == NULL || bink->lastdecompframe == bink->FrameNum) {
    return 0;
  }
  bink->io.Working = 1;
  bink->field_238 = FUN_002328c0();
  memmove(bink->rtframetimes + 1, bink->rtframetimes, bink->runtimemoveamt);
  memmove(bink->rtvdecomptimes + 1, bink->rtvdecomptimes, bink->runtimemoveamt);
  memmove(bink->rtadecomptimes + 1, bink->rtadecomptimes, bink->runtimemoveamt);
  memmove(bink->rtblittimes + 1, bink->rtblittimes, bink->runtimemoveamt);
  memmove(bink->rtreadtimes + 1, bink->rtreadtimes, bink->runtimemoveamt);
  memmove(bink->rtidlereadtimes + 1, bink->rtidlereadtimes,
          bink->runtimemoveamt);
  memmove(bink->rtthreadreadtimes + 1, bink->rtthreadreadtimes,
          bink->runtimemoveamt);
  bink->rtframetimes[0] = bink->field_238;
  bink->rtvdecomptimes[0] = bink->timevdecomp;
  bink->rtadecomptimes[0] = bink->timeadecomp;
  bink->rtblittimes[0] = bink->timeblit;
  bink->rtreadtimes[0] = bink->io.ForegroundTime;
  bink->rtidlereadtimes[0] = bink->io.IdleTime;
  bink->rtthreadreadtimes[0] = bink->io.ThreadTime;
  if (bink->firstframetime == 0) {
    bink->rtframetimes[1] =
      bink->field_238 -
      mult64anddiv(1000, bink->fileframeratediv, bink->fileframerate);
    bink->io.ThreadTime = 0;
    bink->firstframetime = bink->field_238;
    bink->io.IdleTime = 0;
  } else if (bink->trackindex != -1) {
    FUN_0022e0c0(bink);
  }
  comp = bink->compframe;
  bink->longestframetime = bink->field_238;
  if (bink->io.ReadError != 0) {
    bink->ReadError = 1;
  }
  if (bink->ReadError != 0) {
    return 1;
  }
  for (i = 0; i < bink->NumTracks; i++) {
    track_size = *(unsigned long *)comp;
    comp += 4;
    if (i == bink->trackindex && track_size != 0) {
      FUN_00232460(sound_handler, &bink->sound_callback);
      left = *(unsigned long *)comp;
      input = comp + 4;
      while (left != 0) {
        space = bink->sndbufsize - bink->sndamt;
        if (space == 0) {
          break;
        }
        FUN_0023ec50(bink->sndcomp, &output, &amt, input, &input);
        if (amt > left) {
          amt = left;
        }
        left -= amt;
        if (amt > space) {
          amt = space;
        }
        bink->sndamt += amt;
        tail = (unsigned long)(bink->sndend - bink->sndwritepos);
        if (tail < amt) {
          if (tail != 0) {
            memcpy(bink->sndwritepos, output, tail);
            amt -= tail;
            output = (unsigned char *)output + tail;
          }
          memcpy(bink->sndbuf, output, amt);
          bink->sndwritepos = bink->sndbuf + amt;
        } else {
          memcpy(bink->sndwritepos, output, amt);
          bink->sndwritepos += amt;
        }
      }
      FUN_002324f0(sound_handler, &bink->sound_callback);
    }
    comp += track_size;
  }
  now = FUN_002328c0();
  if (bink->VideoOn != 0) {
    if (bink->skippedlastblit == 0) {
      memset(bink->dirty_mask, 0,
             ((unsigned long)bink->dirty_height >> 4) *
               ((unsigned long)bink->dirty_width >> 4));
    }
    if (bink->preloadptr == NULL && bink->io.DoingARead == 0 &&
        (bink->io.CurBufUsed + 1) * 100 / (bink->io.CurBufSize + 1) < 75) {
      if (FUN_00232430(io_handler, &bink->io.callback) != 0) {
        bink->io.Idle(&bink->io);
        FUN_002324f0(io_handler, &bink->io.callback);
      } else if ((bink->OpenFlags & 0x8000000) == 0) {
        FUN_00232600();
      }
    }
    FUN_00238070(
      bink->field_bc[bink->field_b8 ^ 1], bink->field_bc[bink->field_b8],
      bink->field_c4[bink->field_b8 ^ 1], bink->field_c4[bink->field_b8],
      bink->dirty_mask, bink->field_214, bink->field_218, bink->dirty_width,
      bink->dirty_height, comp, bink->frameoffsets[bink->FrameNum - 1] & 1,
      bink->field_3ac, bink->OpenFlags, bink->BinkType);
    bink->field_b8 ^= 1;
  }
  if (bink->trackindex != -1) {
    FUN_0022e0c0(bink);
  }
  bink->NumRects = -1;
  bink->playedframes++;
  if (bink->starttime == 0) {
    bink->starttime = FUN_002328c0();
    bink->startframe = bink->playedframes - 1;
  }
  end = FUN_002328c0();
  bink->startblittime = end;
  bink->timeadecomp += now - bink->field_238;
  bink->lastdecompframe = bink->FrameNum;
  bink->LastFrameNum = bink->FrameNum;
  bink->timevdecomp += end - now;
  bink->io.Working = 0;
  return 0;
}

long __stdcall BinkWait(BINK *bink)
{
  unsigned long start;
  unsigned long now;
  unsigned long target;
  long elapsed;

  if (bink == NULL || (bink->playedframes == 0 && bink->Paused == 0) ||
      bink->ReadError != 0) {
    return 0;
  }
  start = bink->starttime;
  if (start == 0) {
    inittimer(bink);
    start = bink->starttime;
  }
  if (bink->trackindex != -1) {
    FUN_0022e0c0(bink);
  }
  now = FUN_002328c0();
  if (bink->startblittime != 0) {
    bink->timeblit += now - bink->startblittime;
    bink->startblittime = 0;
  }
  endframe(bink, now);
  if (bink->Paused != 0 || (bink->trackindex != -1 && bink->SoundOn == 0)) {
    goto idle_io;
  }
  if (bink->FrameRate != 0) {
    target = mult64anddiv(
      mult64anddiv(bink->playedframes * 1000 - bink->startframe * 1000,
                   bink->FrameRateDiv, bink->FrameRate),
      bink->sound.VideoScale, 0x10000);
    elapsed = (long)(now - bink->big_sound_skip_adj - start);
    if (elapsed < (long)target) {
      goto idle_io;
    }
    elapsed -= target;
    if (elapsed > (long)bink->resynctime) {
      if (bink->trackindex == -1) {
        bink->starttime = now;
        bink->startframe = bink->playedframes - 1;
      } else {
        bink->big_sound_skip_adj = elapsed;
        bink->big_sound_skip_reduce =
          mult64anddiv(elapsed, bink->FrameRateDiv, bink->FrameRate);
      }
    }
    if (bink->big_sound_skip_adj < bink->big_sound_skip_reduce) {
      bink->big_sound_skip_adj = 0;
      return 0;
    }
    bink->big_sound_skip_adj -= bink->big_sound_skip_reduce;
  }
  return 0;

idle_io:
  if (bink->preloadptr == NULL &&
      FUN_00232430(io_handler, &bink->io.callback) != 0) {
    bink->io.Idle(&bink->io);
    FUN_002324f0(io_handler, &bink->io.callback);
  }
  return 1;
}

void __stdcall BinkGetSummary(BINK *bink, void *summary_out)
{
  BINKSUMMARY *summary;
  unsigned long now;
  unsigned long total_frame_bytes;

  summary = (BINKSUMMARY *)summary_out;
  if (bink != NULL && summary != NULL) {
    now = FUN_002328c0();
    if (bink->startblittime != 0) {
      bink->timeblit += now - bink->startblittime;
      bink->startblittime = 0;
    }
    endframe(bink, now);
    memset(summary, 0, sizeof(*summary));
    summary->FrameRate = bink->FrameRate;
    summary->FrameRateDiv = bink->FrameRateDiv;
    summary->SkippedBlits = bink->skippedblits;
    summary->SoundSkips = bink->soundskips;
    summary->FileFrameRate = bink->fileframerate;
    summary->FileFrameRateDiv = bink->fileframeratediv;
    summary->TotalFrames = bink->Frames;
    summary->TotalPlayedFrames = bink->playedframes;
    summary->TotalTime = FUN_002328c0() - bink->firstframetime;
    summary->TotalOpenTime = bink->timeopen;
    summary->TotalAudioDecompTime = bink->timeadecomp;
    summary->TotalVideoDecompTime = bink->timevdecomp;
    summary->TotalBlitTime = bink->timeblit;
    summary->HighestMemAmount += bink->totalmem;
    summary->TotalIOMemory = bink->iosize;
    summary->TotalReadSpeed =
      mult64anddiv(bink->io.BytesRead, 1000, bink->io.TotalTime + 1);
    summary->TotalReadTime = bink->io.ForegroundTime;
    summary->TotalIdleReadTime = bink->io.IdleTime;
    summary->TotalBackReadTime = bink->io.ThreadTime;
    total_frame_bytes = bink->Size - (bink->frameoffsets[0] & ~1UL);
    summary->AverageDataRate =
      mult64anddiv(total_frame_bytes, bink->fileframerate,
                   bink->Frames * bink->fileframeratediv);
    summary->AverageFrameSize = total_frame_bytes / bink->Frames;
    summary->Highest1SecRate = bink->Highest1SecRate;
    summary->Highest1SecFrame = bink->Highest1SecFrame + 1;
    summary->Width = bink->Width;
    summary->Height = bink->Height;
    summary->SlowestFrameTime = bink->slowestframetime;
    summary->Slowest2FrameTime = bink->slowest2frametime;
    summary->SlowestFrameNum = bink->slowestframe;
    summary->Slowest2FrameNum = bink->slowest2frame;
    /* Overwrites the iosize stored above with the backend's BufSize. */
    summary->TotalIOMemory = bink->io.BufSize;
    summary->HighestIOUsed = bink->io.BufHighUsed;
  }
}

/* BinkGetRealtime in the RAD API; kb name kept.  Requires a valid handle,
 * output and runtime history arrays (no NULL checks in 2276). */
void __stdcall BinkGetFrameBuffersInfo(BINK *bink, void *realtime_out,
                                       unsigned long frames)
{
  BINKREALTIME *realtime;
  unsigned long now;

  realtime = (BINKREALTIME *)realtime_out;
  now = FUN_002328c0();
  if (bink->startblittime != 0) {
    bink->timeblit += now - bink->startblittime;
    bink->startblittime = 0;
  }
  endframe(bink, now);
  if (frames == 0 || frames >= bink->runtimeframes) {
    frames = bink->runtimeframes - 1;
  }
  if (frames > bink->FrameNum) {
    frames = bink->FrameNum - 1;
    if (frames == 0) {
      frames = 1;
    }
  }
  realtime->FrameNum = bink->LastFrameNum;
  realtime->FrameRate = bink->FrameRate;
  /* BUG (original): reads FrameRate (+0x14) again, not FrameRateDiv. */
  realtime->FrameRateDiv = bink->FrameRate;
  realtime->ReadBufferSize = bink->io.CurBufSize;
  realtime->ReadBufferUsed = bink->io.CurBufUsed;
  realtime->FramesDataRate =
    mult64anddiv(bink->frameoffsets[bink->FrameNum] -
                   bink->frameoffsets[bink->FrameNum - frames],
                 bink->fileframerate, bink->fileframeratediv * frames);
  realtime->Frames = frames;
  realtime->FramesTime = bink->rtframetimes[0] - bink->rtframetimes[frames];
  if (realtime->FramesTime == 0) {
    realtime->FramesTime = 1;
  }
  realtime->FramesVideoDecompTime =
    bink->rtvdecomptimes[0] - bink->rtvdecomptimes[frames];
  realtime->FramesAudioDecompTime =
    bink->rtadecomptimes[0] - bink->rtadecomptimes[frames];
  realtime->FramesBlitTime = bink->rtblittimes[0] - bink->rtblittimes[frames];
  realtime->FramesReadTime = bink->rtreadtimes[0] - bink->rtreadtimes[frames];
  realtime->FramesIdleReadTime =
    bink->rtidlereadtimes[0] - bink->rtidlereadtimes[frames];
  realtime->FramesThreadReadTime =
    bink->rtthreadreadtimes[0] - bink->rtthreadreadtimes[frames];
}

/* Header dwords: 0 magic, 1 Size, 2 Frames, 3 largest frame (field_e8),
 * 4 InternalFrames, 5 Width, 6 Height, 7/8 file frame rate/div, 9 BinkType,
 * 10 NumTracks.  Everything is staged in a stack BINK, sub-allocations are
 * queued with pushmalloc (FUN_0022de60) and the BINK itself is the single
 * popmalloc (FUN_0022dec0) block they are carved from. */
BINK *__stdcall BinkOpen(const char *filename, unsigned int flags)
{
  BINK b;
  unsigned long header[11];
  BINK *bink;
  bink_io_open_proc open;
  unsigned long from_memory;
  unsigned long copy_mode;
  unsigned long all_keyframes;
  unsigned long simulate;
  unsigned long plane_size;
  unsigned long preload_size;
  unsigned long track_type;
  unsigned long frequency;
  unsigned long runtime_frames;
  const char *source;
  char *error;
  long i;

  memset(&b, 0, sizeof(b));
  open = BinkFileOpen;
  b.timeopen = FUN_002328c0();
  bink_error[0] = 0;
  from_memory = flags & 0x4000000;
  if (from_memory != 0) {
    for (i = 0; i < 11; i++) {
      header[i] = ((const unsigned long *)filename)[i];
    }
  } else {
    if ((flags & 0x2000000) != 0 && forced_io_open != NULL) {
      open = forced_io_open;
    }
    forced_io_open = NULL;
    if (!open(&b.io, filename, flags)) {
      if (bink_error[0] == 0) {
        FUN_0022df50("Error opening file.");
      }
      return NULL;
    }
    b.io.ReadHeader(&b.io, 0, header, 0x2c);
  }
  if (header[0] != 0x664b4942 && header[0] != 0x674b4942 &&
      header[0] != 0x684b4942 && header[0] != 0x694b4942) {
    FUN_0022df50("Not a Bink file.");
    goto close_io;
  }
  if (header[2] == 0) {
    /* inline copy, not a FUN_0022df50 call */
    source = "The file doesn't contain any compressed frames yet.";
    error = bink_error;
    do {
      *error = *source;
      error++;
    } while (*source++ != 0);
    goto close_io;
  }
  b.field_d4 = (((header[5] + 1) >> 1) + 7) & ~7UL;
  b.field_d8 = (((header[6] + 1) >> 1) + 7) & ~7UL;
  b.dirty_width = b.field_d4 * 2;
  b.dirty_height = b.field_d8 * 2;
  b.dirty_pitch = (unsigned long)b.dirty_width >> 4;
  b.OpenFlags = (flags | (header[9] & 0x20000)) & 0x8fffffff;
  b.Width = header[5];
  b.Height = header[6];
  b.field_214 = header[5];
  b.field_218 = header[6];
  b.BinkType = header[9];
  if ((header[9] & 0x100000) == 0) {
    b.OpenFlags &= ~0x100000UL;
  }
  copy_mode = flags & 0x70000000;
  if (copy_mode != 0x70000000) {
    if (copy_mode != 0) {
      b.OpenFlags |= copy_mode;
    } else {
      b.OpenFlags |= header[9] & 0x70000000;
    }
    switch (b.OpenFlags & 0x70000000) {
    case 0x30000000:
      b.Width = header[5] * 2;
      break;
    case 0x40000000:
    case 0x50000000:
      b.Width = header[5] * 2;
      /* fall through */
    case 0x10000000:
    case 0x20000000:
      b.Height = header[6] * 2;
      break;
    }
  }
  if (header[0] == 0x664b4942 || header[0] == 0x674b4942) {
    b.OpenFlags |= 0x18000;
  } else if (header[0] == 0x684b4942) {
    b.OpenFlags |= 0x8000;
  }
  b.Frames = header[2];
  b.InternalFrames = header[4];
  if ((flags & 0x1000) != 0 && forced_frame_rate != (unsigned long)-1) {
    b.FrameRate = forced_frame_rate;
    b.FrameRateDiv = forced_frame_rate_div;
    forced_frame_rate = (unsigned long)-1;
  } else {
    b.FrameRate = header[7];
    b.FrameRateDiv = header[8];
  }
  b.Size = header[1];
  b.NumTracks = header[10];
  b.field_e8 = header[3];
  b.fileframerate = header[7];
  b.fileframeratediv = header[8];
  b.field_e4 = ((unsigned long)b.dirty_pitch * b.dirty_height) >> 4;
  runtime_frames = ((header[8] >> 1) + header[7]) / header[8];
  b.runtimeframes = runtime_frames;
  if (runtime_frames == 0) {
    runtime_frames = 1;
    b.runtimeframes = 1;
  }
  b.runtimemoveamt = runtime_frames * 4 - 4;
  FUN_0022de60((void **)&b.dirty_mask, b.field_e4 + 0x10);
  FUN_0022de60((void **)&b.rtframetimes, runtime_frames * 4);
  FUN_0022de60((void **)&b.rtadecomptimes, runtime_frames * 4);
  FUN_0022de60((void **)&b.rtvdecomptimes, runtime_frames * 4);
  FUN_0022de60((void **)&b.rtblittimes, runtime_frames * 4);
  FUN_0022de60((void **)&b.rtreadtimes, runtime_frames * 4);
  FUN_0022de60((void **)&b.rtidlereadtimes, runtime_frames * 4);
  FUN_0022de60((void **)&b.rtthreadreadtimes, runtime_frames * 4);
  FUN_00236210(b.field_3ac, b.dirty_width);
  FUN_0022de60((void **)&b.field_3ac[0], b.field_3ac[0]);
  FUN_0022de60((void **)&b.field_3ac[1], b.field_3ac[1]);
  FUN_0022de60((void **)&b.field_3ac[2], b.field_3ac[2]);
  FUN_0022de60((void **)&b.field_3ac[3], b.field_3ac[3]);
  FUN_0022de60((void **)&b.field_3ac[4], b.field_3ac[4]);
  FUN_0022de60((void **)&b.field_3ac[5], b.field_3ac[5]);
  FUN_0022de60((void **)&b.field_3ac[6], b.field_3ac[6]);
  FUN_0022de60((void **)&b.field_3ac[7], b.field_3ac[7]);
  FUN_0022de60((void **)&b.field_3ac[8], b.field_3ac[8]);
  if (from_memory == 0) {
    FUN_0022de60((void **)&b.frameoffsets, b.InternalFrames * 4 + 4);
    FUN_0022de60((void **)&b.tracksizes, b.NumTracks * 4);
    FUN_0022de60((void **)&b.tracktypes, b.NumTracks * 4);
    FUN_0022de60((void **)&b.trackIDs, b.NumTracks * 4);
  }
  b.totalmem += pending_allocation_bytes + 0x408;
  bink = (BINK *)FUN_0022dec0(0x408);
  if (bink == NULL) {
    goto out_of_memory;
  }
  *bink = b;
  bink->io.bink = bink;
  *bink->rtadecomptimes = 0;
  *bink->rtvdecomptimes = 0;
  *bink->rtblittimes = 0;
  *bink->rtreadtimes = 0;
  *bink->rtidlereadtimes = 0;
  *bink->rtthreadreadtimes = 0;
  if (from_memory != 0) {
    bink->tracksizes = (unsigned long *)(filename + 0x2c);
    bink->tracktypes = (unsigned long *)(filename + bink->NumTracks * 4 + 0x2c);
    bink->trackIDs = (unsigned long *)(filename + bink->NumTracks * 8 + 0x2c);
    bink->frameoffsets =
      (unsigned long *)(filename + bink->NumTracks * 12 + 0x2c);
  } else {
    bink->io.ReadHeader(&bink->io, -1, bink->tracksizes, bink->NumTracks * 4);
    bink->io.ReadHeader(&bink->io, -1, bink->tracktypes, bink->NumTracks * 4);
    bink->io.ReadHeader(&bink->io, -1, bink->trackIDs, bink->NumTracks * 4);
    bink->io.ReadHeader(&bink->io, -1, bink->frameoffsets,
                        bink->InternalFrames * 4 + 4);
  }
  bink->Highest1SecRate =
    high1secrate(bink->Frames, bink->runtimeframes, &bink->Highest1SecFrame,
                 &all_keyframes, bink->frameoffsets);
  if ((bink->OpenFlags & 0x100000) != 0) {
    FUN_0022de60(&bink->field_c4[0], bink->dirty_width * bink->dirty_height);
    if (all_keyframes == 0) {
      FUN_0022de60(&bink->field_c4[1], bink->dirty_width * bink->dirty_height);
    }
  }
  if (all_keyframes == 0) {
    FUN_0022de60(&bink->field_bc[1], bink->dirty_width * bink->dirty_height +
                                       bink->field_d4 * bink->field_d8 * 2);
  }
  plane_size = bink->dirty_width * bink->dirty_height +
               bink->field_d4 * bink->field_d8 * 2;
  bink->totalmem += plane_size + pending_allocation_bytes;
  bink->field_bc[0] = FUN_0022dec0(plane_size);
  if (bink->field_bc[0] == NULL) {
    goto free_bink;
  }
  if (all_keyframes != 0) {
    bink->field_bc[1] = bink->field_bc[0];
    bink->field_c4[1] = bink->field_c4[0];
  }
  if ((flags & 0x1000000) != 0 && forced_io_size != (unsigned long)-1) {
    bink->iosize = forced_io_size;
    forced_io_size = (unsigned long)-1;
  } else {
    bink->iosize = bink->Highest1SecRate;
  }
  if ((flags & 0x400000) != 0 && forced_simulate_rate != (unsigned long)-1) {
    simulate = forced_simulate_rate;
    forced_simulate_rate = (unsigned long)-1;
  } else {
    simulate = 0;
  }
  if (from_memory != 0) {
    bink->preloadptr = (void *)(filename + (bink->frameoffsets[0] & ~1UL));
  } else {
    bink->iosize = bink->io.GetBufferSize(&bink->io, bink->iosize);
    if (bink->iosize >= bink->Size * 9 / 10) {
      flags |= 0x2000;
      bink->OpenFlags |= 0x2000;
    }
    if ((flags & 0x2000) != 0) {
      preload_size = bink->Size - (bink->frameoffsets[0] & ~1UL) + 8;
      bink->preloadptr = bpopmalloc(preload_size, bink);
      if (bink->preloadptr == NULL) {
        /* frees the stack copy's plane pointer, which is still 0 */
        FUN_00231520(b.field_bc[0]);
        goto free_bink;
      }
      bink->io.SetInfo(&bink->io, NULL, 0, bink->Size + 8, simulate);
      bink->io.ReadFrame(&bink->io, 0, bink->frameoffsets[0] & ~1UL,
                         bink->preloadptr, preload_size);
      bink->io.Close(&bink->io);
      bink->io.ForegroundTime = 0;
    } else {
      FUN_0022de60((void **)&bink->compframe, bink->field_e8);
      bink->iobuffer = bpopmalloc(bink->iosize, bink);
      if (bink->iobuffer == NULL) {
        bink->iosize = 0;
      }
      bink->io.SetInfo(&bink->io, bink->iobuffer, bink->iosize, bink->Size + 8,
                       simulate);
    }
  }
  bink->dirty_mask[bink->field_e4] = 0;
  bink->FrameNum = (unsigned long)-1;
  if (bink->FrameRate != 0) {
    bink->resynctime = mult64anddiv(2000, bink->FrameRateDiv, bink->FrameRate);
  } else {
    bink->resynctime = 2000;
  }
  bink->VideoOn = 1;
  GotoFrame(bink, 1);
  bink->timeopen = FUN_002328c0() - bink->timeopen;
  if (bink->NumTracks == 0) {
    bink->trackindex = -1;
  } else if ((bink->OpenFlags & 0x4000) == 0) {
    bink->trackindex = 0;
  } else if (forced_sound_track == (unsigned long)-1) {
    bink->trackindex = -1;
  } else {
    for (i = 0; i < bink->NumTracks; i++) {
      if (bink->trackIDs[i] == forced_sound_track) {
        break;
      }
    }
    if (i >= bink->NumTracks) {
      i = -1;
    }
    bink->trackindex = i;
  }
  forced_sound_track = (unsigned long)-1;
  if (bink->trackindex != -1) {
    if ((long)bink->tracktypes[bink->trackindex] < 0) {
      if (sound_open == NULL) {
        BinkSoundUseDirectSound((void *)BinkOpenDirectSound, NULL);
      }
      if (sound_open != NULL) {
        track_type = bink->tracktypes[bink->trackindex];
        frequency = track_type & 0xffff;
        if (b.FrameRate != 0 && b.FrameRateDiv != 0) {
          frequency =
            udiv64((uint64_t)frequency * b.fileframeratediv * b.FrameRate,
                   (uint64_t)b.fileframerate * b.FrameRateDiv);
        }
        if (sound_open(&bink->sound, frequency,
                       ((track_type >> 30) & 1) * 8 + 8,
                       ((track_type >> 29) & 1) + 1, bink->OpenFlags, bink)) {
          sound_open_count++;
          bink->sndconvert8 =
            ((bink->tracktypes[bink->trackindex] >> 27) & 8) == 0;
          bink->SoundOn = 1;
          bink->sndbufsize =
            (bink->tracksizes[bink->trackindex] + 0xff) & ~0xffUL;
          bink->sndbuf = FUN_002314b0(bink->sndbufsize);
          bink->sndwritepos = bink->sndbuf;
          bink->sndreadpos = bink->sndbuf;
          bink->sndend = bink->sndbuf + bink->sndbufsize;
          bink->sndprime =
            mult64anddiv(
              (((bink->tracktypes[bink->trackindex] >> 29) & 1) + 1) *
                frequency * 2,
              750 - bink->sound.Latency, 1000) &
            ~3UL;
          if (bink->sndprime > bink->sndbufsize) {
            bink->sndprime = bink->sndbufsize;
          }
          bink->sndcomp = FUN_0023cd10(
            bink->tracktypes[bink->trackindex] & 0xffff,
            ((bink->tracktypes[bink->trackindex] >> 29) & 1) + 1, 1);
          bink->sndamt = 0;
          bink->sndendframe = bink->Frames - bink->fileframerate * 3 /
                                               (bink->fileframeratediv * 4);
        }
      }
    }
    if (bink->SoundOn == 0) {
      bink->trackindex = -1;
    }
  }
  if (bink->sound.VideoScale == 0) {
    bink->sound.VideoScale = 0x10000;
  }
  if (bink->trackindex != -1) {
    if (sound_handler == NULL) {
      sound_handler = FUN_00232130(0x14);
      FUN_00232390(sound_handler);
    }
    FUN_002321c0(sound_handler, &bink->sound_callback, FUN_0022e4d0,
                 FUN_0022e4f0);
  }
  if ((bink->OpenFlags & 0x8000000) == 0 && bink->preloadptr == NULL) {
    if (io_handler == NULL) {
      io_handler = FUN_00232130(5);
      FUN_00232390(io_handler);
    }
    FUN_002321c0(io_handler, &bink->io.callback, FUN_0022e3f0, FUN_0022e440);
  }
  bink->io.resume_callback = FUN_0022e4a0;
  bink->io.suspend_callback = FUN_0022e460;
  bink->io.try_suspend_callback = FUN_0022e480;
  bink->io.idle_on_callback = FUN_0022e4c0;
  if (bink->preloadptr == NULL && (flags & 0x200000) == 0) {
    while (bink->io.Idle(&bink->io)) {
    }
  }
  return bink;

free_bink:
  FUN_00231520(bink);
out_of_memory:
  source = "Out of memory.";
  error = bink_error;
  do {
    *error = *source;
    error++;
  } while (*source++ != 0);
close_io:
  if ((flags & 0x4000000) == 0) {
    b.io.Close(&b.io);
  }
  return NULL;
}

void __stdcall BinkNextFrame(BINK *bink)
{
  unsigned long now;

  if (bink != NULL) {
    bink->io.Working = 1;
    bink->io.Working = 0;
    if (bink->sound.SoundDroppedOut != 0) {
      bink->sound.SoundDroppedOut = 0;
      if (bink->FrameNum > 1 && (long)bink->FrameNum <= bink->sndendframe) {
        FUN_00232490(sound_handler, &bink->sound_callback, io_handler,
                     &bink->io.callback);
        FUN_002301a0(bink, 0);
        while (((bink->io.CurBufUsed + 1) * 100) / (bink->io.CurBufSize + 1) <
               90) {
          if (bink->io.Idle(&bink->io) == 0) {
            break;
          }
        }
        bink->soundskips++;
        dosilence(bink);
        bink->starttime = 0;
        bink->big_sound_skip_adj = 0;
        FUN_002301a0(bink, 1);
        FUN_002324f0(sound_handler, &bink->sound_callback);
        FUN_002324f0(io_handler, &bink->io.callback);
      }
    }
    bink->io.Working = 1;
    if (bink->trackindex != -1) {
      FUN_0022e0c0(bink);
    }
    now = FUN_002328c0();
    if (bink->startblittime != 0) {
      bink->timeblit += now - bink->startblittime;
      bink->startblittime = 0;
    }
    if (bink->FrameNum >= bink->Frames) {
      GotoFrame(bink, 1);
    } else {
      GotoFrame(bink, bink->FrameNum + 1);
    }
    bink->io.Working = 0;
  }
}

void __stdcall BinkClose(BINK *bink)
{
  struct radcb_handler *io;

  if (bink != NULL) {
    FUN_0022f3e0(bink, 1);
    if ((bink->OpenFlags & 0x8000000) != 0 || bink->preloadptr != NULL) {
      io = NULL;
    } else {
      io = io_handler;
    }
    switch (FUN_002326e0(io, &bink->io.callback,
                         bink->trackindex != -1 ? sound_handler : NULL,
                         &bink->sound_callback, 1)) {
    case 1:
      io_handler = NULL;
      break;
    case 2:
      sound_handler = NULL;
      break;
    case 3:
      sound_handler = NULL;
      io_handler = NULL;
      break;
    }
    if (bink->trackindex != -1) {
      bink->sound.Close(&bink->sound);
      FUN_0023ce50(bink->sndcomp);
    }
    if (bink->preloadptr != NULL) {
      if ((bink->OpenFlags & 0x4000000) == 0) {
        FUN_00231520(bink->preloadptr);
      }
    } else {
      bink->io.Close(&bink->io);
      FUN_00231520(bink->iobuffer);
    }
    if (bink->sndbuf != NULL) {
      FUN_00231520(bink->sndbuf);
    }
    if (bink->field_bc[0] != NULL) {
      FUN_00231520(bink->field_bc[0]);
    }
    memset(bink, 0, sizeof(*bink));
    FUN_00231520(bink);
  }
}

/* RADSetMemory in the RAD API; kb name kept. */
void __stdcall FUN_00231490(void *alloc_callback, void *free_callback)
{
  rad_allocate_callback = alloc_callback;
  rad_release_callback = free_callback;
}
