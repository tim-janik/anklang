// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "compress.hh"
#include "storage.hh"
#include "utils.hh"
#include "platform.hh"
#include "internal.hh"

#if !__has_include(<zstd.h>)
#error "Missing <zstd.h> from libzstd-dev, please set CXXFLAGS and LDFLAGS"
#endif
#include <zstd.h>

namespace Ase {

bool
is_aiff (const String &input)
{
  return input.substr (0, 4) == "FORM" && input.substr (8, 4) == "AIFF";
}

bool
is_wav (const String &input)
{
  return input.substr (0, 4) == "RIFF" && input.substr (8, 4) == "WAVE";
}

bool
is_midi (const String &input)
{
  return input.substr (0, 4) == "MThd";
}

bool
is_pdf (const String &input)
{
  return input.substr (0, 5) == "%PDF-";
}

static constexpr const size_t MB = 1024 * 1024;
static struct { size_t level, size; } const zstd_adaptive_level[] = {
  { 18,  1 * MB },      // slow, use only for small sizes
  { 14,  3 * MB },
  { 11, 11 * MB },
  {  8, 20 * MB },
  {  5, 42 * MB },
  {  4, ~size_t (0) },  // acceptable fast compression
}; // each level + size combination should take roughly the same time

static int
guess_zstd_level (size_t input_size)
{
  uint zal = 0;
  while (zstd_adaptive_level[zal].size > input_size)
    zal++;
  return zstd_adaptive_level[zal].level;
}

String
zstd_compress (const String &input)
{
  const size_t maxosize = ZSTD_compressBound (input.size());
  String data;
  data.resize (maxosize);
  const size_t osize = ZSTD_compress (&data[0], data.size(), &input[0], input.size(), guess_zstd_level (input.size()));
  if (ZSTD_isError (osize))
    {
      warning ("zstd compression failed (input=%zu): %s", input.size(), ZSTD_getErrorName (osize));
      return "";
    }
  data.resize (osize);
  data.reserve (data.size());
  return data;
}

String
zstd_uncompress (const String &input)
{
  const size_t maxosize = ZSTD_getFrameContentSize (&input[0], input.size());
  if (maxosize > 8 * 1024 * 1024 * 1024ull)
    {
      String err;
      switch (maxosize)
        {
        case ZSTD_CONTENTSIZE_ERROR:    err = "invalid zstd header"; break;
        case ZSTD_CONTENTSIZE_UNKNOWN:  err = "unknown zstd data size"; break;
        default:                        err = string_format ("uncompress limit exceeded (%u)", maxosize); break;
        }
      warning ("zstd decompression failed (input=%zu): %s", input.size(), err);
      return "";
    }
  String data;
  data.resize (maxosize);
  const size_t osize = ZSTD_decompress (&data[0], data.size(), &input[0], input.size());
  if (ZSTD_isError (osize))
    {
      warning ("zstd decompression failed (input=%zu): %s", input.size(), ZSTD_getErrorName (osize));
      return "";
    }
  data.resize (osize);
  data.reserve (data.size());
  assert_return (osize == maxosize, data); // paranoid, esnured by zstd
  return data;
}

bool
is_zstd (const String &input)
{
  return (input.size() >= 4 &&
          input[0] == char (0x28) &&
          input[1] == char (0xb5) &&
          input[2] == char (0x2f) &&
          input[3] == char (0xfd));
}

bool
is_lz4 (const String &input)
{
  return (input.size() >= 4 &&
          input[0] == char (0x04) &&
          input[1] == char (0x22) &&
          input[2] == char (0x4d) &&
          input[3] == char (0x18));
}

bool
is_zip (const String &input)
{
  return (input.size() >= 4 &&
          input[0] == 'P' && input[1] == 'K' &&
          ((input[2] == 0x03 && input[3] == 0x04) ||
           (input[2] == 0x05 && input[3] == 0x06) ||
           (input[2] == 0x07 && input[3] == 0x08)));
}

bool
is_arj (const String &input)
{
  return input.size() >= 2 && input[0] == char (0x60) && input[1] == char (0xea);
}

bool
is_isz (const String &input)
{
  return input.substr (0, 4) == "IsZ!";
}

bool
is_ogg (const String &input)
{
  return input.substr (0, 4) == "OggS";
}

bool
is_avi (const String &input)
{
  return input.substr (0, 4) == "RIFF" && input.substr (8, 4) == "AVI ";
}

bool
is_gz (const String &input)
{
  return input.size() >= 2 && input[0] == char (0x1f) && input[1] == char (0x8B);
}

bool
is_xz (const String &input)
{
  return (input.size() >= 6 &&
          input[0] == char (0xfd) &&
          input[1] == char (0x37) &&
          input[2] == char (0x7a) &&
          input[3] == char (0x58) &&
          input[4] == char (0x5a) &&
          input[5] == char (0x00));
}

bool
is_png (const String &input)
{
  return (input.size() >= 8 &&
          input[0] == char (0x89) &&
          input.substr (1, 3) == "PNG" &&
          input[4] == char (0x0d) &&
          input[5] == char (0x0a) &&
          input[6] == char (0x1a) &&
          input[7] == char (0x0a));
}

bool
is_jpg (const String &input)
{
  return (input.size() >= 4 &&
          input[0] == char (0xff) &&
          input[1] == char (0xd8) &&
          input[2] == char (0xff) &&
          (input[3] == char (0xdb) || input[3] == char (0xe0) ||
           input[3] == char (0xee) || input[3] == char (0xe1)));
}

bool
is_compressed (const String &input)
{
  return (is_zstd (input) ||
          is_lz4 (input) ||
          is_zip (input) ||
          is_arj (input) ||
          is_isz (input) ||
          is_ogg (input) ||
          is_avi (input) ||
          is_gz (input) ||
          is_xz (input) ||
          is_png (input) ||
          is_jpg (input));

}

class StreamReaderZStd : public StreamReader {
  StreamReaderP istream_;
  std::vector<uint8_t> ibuffer_;
  ZSTD_inBuffer zinput_ = { nullptr, 0, 0 };
  ZSTD_DCtx *dctx_ = nullptr;
  String name_;
public:
  StreamReaderZStd (const StreamReaderP &istream)
  {
    istream_ = istream;
    name_ = istream_->name();
    ibuffer_.resize (ZSTD_DStreamInSize());
    dctx_ = ZSTD_createDCtx();
    assert_return (dctx_ != nullptr);
  }
  ~StreamReaderZStd()
  {
    close();
  }
  String
  name() const override
  {
    return name_;
  }
  ssize_t
  read (void *buffer, size_t len) override
  {
    return_unless (buffer && len > 0, 0);
    size_t ret = 0;
    // (pos<size) is true until all decompressed data is flushed
    if (zinput_.pos < zinput_.size) {
      ZSTD_outBuffer zoutput = { buffer, len, 0 };
      ret = ZSTD_decompressStream (dctx_, &zoutput, &zinput_);
      if (ZSTD_isError (ret))
        goto zerror;
      if (zoutput.pos)
        return zoutput.pos;
    }
    // provide more input
    while (zinput_.pos == zinput_.size && istream_) {
      const ssize_t l = istream_->read (&ibuffer_[0], ibuffer_.size());
      if (l > 0) {
        zinput_ = { &ibuffer_[0], size_t (l), 0 };
      } else // l <= 0
        goto done;
      ZSTD_outBuffer zoutput = { buffer, len, 0 };
      ret = ZSTD_decompressStream (dctx_, &zoutput, &zinput_);
      if (ZSTD_isError (ret))
        goto zerror;
      if (zoutput.pos)
        return zoutput.pos;
    }
  zerror:
    printerr ("%s: ZSTD_decompressStream: %s\n", program_alias(), ZSTD_getErrorName (ret));
  done:
    istream_ = nullptr;
    return 0;
  }
  bool
  close() override
  {
    return_unless (istream_, false);
    const bool closeok = istream_->close();
    istream_ = nullptr;
    ibuffer_.clear();
    ibuffer_.reserve (0);
    if (dctx_)
      ZSTD_freeDCtx (dctx_);
    dctx_ = nullptr;
    return closeok;
  }
};

StreamReaderP
stream_reader_zstd (StreamReaderP &istream)
{
  return_unless (istream, nullptr);
  return std::make_shared<StreamReaderZStd> (istream);
}

static constexpr bool PRINT_ADAPTIVE = false;

class StreamWriterZStd : public StreamWriter {
  StreamWriterP ostream_;
  std::vector<uint8_t> obuffer_;
  String name_;
  ZSTD_CCtx *cctx_ = nullptr;
  size_t itotal_ = 0;
  uint8_t zal_ = 0;
  bool last_block_ = false;
public:
  ~StreamWriterZStd()
  {
    close();
    obuffer_.clear();
    obuffer_.reserve (0);
    if (cctx_)
      ZSTD_freeCCtx (cctx_);
    cctx_ = nullptr;
  }
  StreamWriterZStd (const StreamWriterP &ostream, int level)
  {
    obuffer_.resize (ZSTD_CStreamOutSize());
    cctx_ = ZSTD_createCCtx();
    assert_return (cctx_ != nullptr);
    size_t pret;
    pret = ZSTD_CCtx_setParameter (cctx_, ZSTD_c_checksumFlag, 1);
    assert_return (!ZSTD_isError (pret)); // ZSTD_getErrorName (pret)
    if (level == 0) {
      while (zstd_adaptive_level[zal_].size < std::numeric_limits<decltype (zstd_adaptive_level[0].size)>::max())
        zal_++;
      pret = ZSTD_CCtx_setParameter (cctx_, ZSTD_c_compressionLevel, level);
      if (PRINT_ADAPTIVE) printerr ("ZSTD_compressStream2: fixed level=%u: %s\n", level, ZSTD_getErrorName (pret));
    } else {
      pret = ZSTD_CCtx_setParameter (cctx_, ZSTD_c_compressionLevel, zstd_adaptive_level[zal_].level);
      if (PRINT_ADAPTIVE) printerr ("ZSTD_compressStream2: size=%u level=%u: %s\n", itotal_, zstd_adaptive_level[zal_].level, ZSTD_getErrorName (pret));
    }
    assert_return (!ZSTD_isError (pret)); // ZSTD_getErrorName (pret)
    ostream_ = ostream;
    name_ = ostream_->name();
  }
  String
  name() const override
  {
    return name_;
  }
  ssize_t
  write (const void *buffer, size_t len) override
  {
    errno = EIO;
    return_unless (ostream_, -1);
    const ZSTD_EndDirective mode = last_block_ ? ZSTD_e_end : ZSTD_e_continue;
    ZSTD_inBuffer zinput = { buffer, len, 0 };
    size_t ret = 0;
    bool block_finished = false;
    while (!block_finished) {
      ZSTD_outBuffer zoutput = { &obuffer_[0], obuffer_.size(), 0 };

      const size_t current_total = itotal_ + zinput.pos;
      if (!last_block_ && current_total > zstd_adaptive_level[zal_].size)
        {
          zal_++;
          const size_t pret = ZSTD_CCtx_setParameter (cctx_, ZSTD_c_compressionLevel, zstd_adaptive_level[zal_].level);
          if (PRINT_ADAPTIVE) printerr ("ZSTD_compressStream2: size=%u level=%u: %s\n", current_total, zstd_adaptive_level[zal_].level, ZSTD_getErrorName (pret));
          zinput.size = zinput.pos;
          ret = ZSTD_compressStream2 (cctx_, &zoutput, &zinput, ZSTD_e_end);
          zinput.size = len;
        }
      else
        ret = ZSTD_compressStream2 (cctx_, &zoutput, &zinput, mode);

      if (ZSTD_isError (ret))
        goto zerror;
      const char *p = (const char*) &obuffer_[0], *const e = p + zoutput.pos;
      while (p < e) {
        const ssize_t n = ostream_->write (p, e - p);
        if (n < 0)
          goto error;
        p += n;
      }
      block_finished = last_block_ ? ret == 0 : zinput.pos == zinput.size;
    }
    itotal_ += zinput.pos;
    return zinput.pos;
  zerror:
    printerr ("%s: ZSTD_compressStream2: %s\n", program_alias(), ZSTD_getErrorName (ret));
    errno = EIO;
    return -1;
  error:
    printerr ("%s: ZSTD_compressStream2: %s\n", program_alias(), strerror (errno ? errno : EIO));
    errno = errno ? errno : EIO;
    return -1;
  }
  bool
  close() override
  {
    bool closedok = true;
    errno = EIO;
    if (ostream_)
      {
        last_block_ = true;
        size_t l;
        do
          l = write (nullptr, 0);
        while (l > 0);
        closedok &= l >= 0;
        closedok &= ostream_->close();
        ostream_ = nullptr;
      }
    return closedok;
  }
};

StreamWriterP
stream_writer_zstd (const StreamWriterP &ostream, int level)
{
  return_unless (ostream, nullptr);
  return std::make_shared<StreamWriterZStd> (ostream, level);
}

} // Ase
