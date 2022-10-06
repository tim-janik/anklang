// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "wave.hh"
#include "datautils.hh"
#include "utils.hh"
#include "platform.hh"
#include "randomhash.hh"
#include "internal.hh"
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <opus.h>
#include <ogg/ogg.h>
#include <FLAC/all.h>

namespace Ase {

// == WaveWriter ==
WaveWriter::~WaveWriter ()
{}

// == WAV ==
static std::vector<unsigned char>
wav_header (const uint8_t n_bits, const uint32_t n_channels, const uint32_t sample_freq, const uint32_t n_samples)
{
  union ByteStream {
    void *v; char *c; uint8_t *u8;
    uint16_t *u16;
    uint32_t *u32;
    char*
    puts (const char *str)
    {
      strcpy (c, str);
      c += strlen (str);
      return c;
    }
  };
  const uint32_t byte_per_sample = n_channels * n_bits / 8;
  const uint32_t byte_per_second = byte_per_sample * sample_freq;
  const uint32_t n_data_bytes = (n_samples * byte_per_sample + 1) / 2 * 2; // round odd
  std::vector<unsigned char> buffer;
  buffer.resize (1024);
  ByteStream b { &buffer[0] };
  b.puts ("RIFF");                      // main chunk
  const ByteStream lpos = b;
  b.u32++;                              // skip file length
  const ByteStream cpos = b;            // save chunk pos
  b.puts ("WAVE");                      // chunk type
  b.puts ("fmt ");                      // sub chunk
  const uint16_t fmt = n_bits == 32 ? 3 : 1;
  const bool extensible = n_channels > 2 || fmt == 3;
  const uint32_t fmtsz = extensible ? 18 : 16;
  *b.u32++ = htole32 (fmtsz);           // sub chunk length
  *b.u16++ = htole16 (fmt);             // format, 1=PCM, 3=FLOAT
  *b.u16++ = htole16 (n_channels);
  *b.u32++ = htole32 (sample_freq);
  *b.u32++ = htole32 (byte_per_second);
  *b.u16++ = htole16 (byte_per_sample); // block align
  *b.u16++ = htole16 (n_bits);
  if (extensible)
    *b.u16++ = htole16 (0);             // extension size
  if (fmt == 3)
    {
      b.puts ("fact");                  // sub chunk
      *b.u32++ = htole32 (4);           // sub chunk length
      *b.u32++ = htole32 (n_samples);   // frames
    }
  b.puts ("data");                      // data chunk
  *b.u32++ = htole32 (n_data_bytes);
  const uint32_t length = b.c - cpos.c + n_data_bytes;
  *lpos.u32 = htole32 (length);         // fix file length
  buffer.resize (b.u8 - &buffer[0]);
  return buffer;
}

static int
wav_write (int fd, uint8_t n_bits, uint32_t n_channels, uint32_t sample_freq, const float *samples, size_t n_frames)
{
  const size_t n_samples = n_channels * n_frames;
  if (n_bits == 8) // unsigned WAV
    {
      uint8_t buffer[16384], *const e = buffer + sizeof (buffer) / sizeof (buffer[0]), *b = buffer;
      for (size_t n = 0; n < n_samples; )
        {
          const uint8_t u8 = 127.5 + 127.5 * samples[n++];
          *b++ = u8;
          if (b + 1 >= e || n >= n_samples)
            {
              if (n >= n_samples && (n_channels & 1) && (n_samples & 1)) // && (byte_per_sample & 1)
                *b++ = 0; // final pad byte
              if (write (fd, buffer, (b - buffer) * sizeof (buffer[0])) < 0)
                return -errno;
              b = buffer;
            }
        }
    }
  if (n_bits == 24)
    {
      uint8_t buffer[16384], *const e = buffer + sizeof (buffer) / sizeof (buffer[0]), *b = buffer;
      for (size_t n = 0; n < n_samples; )
        {
          const int32_t i24 = samples[n++] * 8388607.5 - 0.5;
          *b++ = i24;
          *b++ = i24 >> 8;
          *b++ = i24 >> 16;
          if (b + 4 >= e || n >= n_samples)
            {
              if (n >= n_samples && (n_channels & 1) && (n_samples & 1)) // && (byte_per_sample & 1)
                *b++ = 0; // final pad byte
              if (write (fd, buffer, (b - buffer) * sizeof (buffer[0])) < 0)
                return -errno;
              b = buffer;
            }
        }
    }
  if (n_bits == 16)
    {
      uint16_t buffer[16384], *const e = buffer + sizeof (buffer) / sizeof (buffer[0]), *b = buffer;
      for (size_t n = 0; n < n_samples; )
        {
          const int16_t i16 = samples[n++] * 32767.5 - 0.5;
          *b++ = htole16 (i16);
          if (b >= e || n >= n_samples)
            {
              if (write (fd, buffer, (b - buffer) * sizeof (buffer[0])) < 0)
                return -errno;
              b = buffer;
            }
        }
    }
  if (n_bits == 32)
    {
      uint32_t buffer[16384], *const e = buffer + sizeof (buffer) / sizeof (buffer[0]), *b = buffer;
      for (size_t n = 0; n < n_samples; )
        {
          union { float f; uint32_t u32; } u { samples[n++] };
          *b++ = htole32 (u.u32);
          if (b >= e || n >= n_samples)
            {
              if (write (fd, buffer, (b - buffer) * sizeof (buffer[0])) < 0)
                return -errno;
              b = buffer;
            }
        }
    }
  return 0;
}

class WavWriterImpl : public WaveWriter {
  String   filename_;
  uint32_t n_channels_ = 0;
  uint32_t sample_freq_ = 0;
  uint8_t  n_bits_ = 0;
  int      fd_ = -1;
  size_t   n_samples_ = 0;
  std::function<void()> flush_atquit;
public:
  WavWriterImpl()
  {
    flush_atquit = [this] () { close(); };
    atquit_add (&flush_atquit);
  }
  ~WavWriterImpl()
  {
    atquit_del (&flush_atquit);
    close();
  }
  bool
  open (const String &filename, uint8_t n_bits, uint32_t n_channels, uint32_t sample_freq)
  {
    assert_return (fd_ == -1, false);
    assert_return (!filename.empty(), false);
    assert_return (n_bits == 8 || n_bits == 16 || n_bits == 24 || n_bits == 32, false);
    assert_return (n_channels > 0, false);
    assert_return (sample_freq > 0, false);
    // open, must be seekable
    fd_ = ::open (filename.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd_ < 0)
      return false;
    if (lseek (fd_, 0, SEEK_SET) < 0)
      {
        ::close (fd_);
        fd_ = -1;
        return false;
      }
    // setup fields
    filename_ = filename;
    n_bits_ = n_bits;
    n_channels_ = n_channels;
    sample_freq_ = sample_freq;
    n_samples_ = 0;
    // write header
    std::vector<unsigned char> header = wav_header (n_bits_, n_channels_, sample_freq_, 4294967168);
    if (::write (fd_, header.data(), header.size()) < 0)
      {
        ::close (fd_);
        fd_ = -1;
        return false;
      }
    return true;
  }
  String
  name () const override
  {
    return filename_;
  }
  ssize_t
  write (const float *frames, size_t n_frames) override
  {
    assert_return (n_bits_ == 32, false);
    assert_return (fd_ >= 0, false);
    return_unless (n_frames, false);

    n_samples_ += n_frames * n_channels_;

    return wav_write (fd_, n_bits_, n_channels_, sample_freq_, frames, n_frames) >= 0;
  }
  bool
  close () override
  {
    return_unless (fd_ >= 0, false);
    std::vector<unsigned char> header = wav_header (n_bits_, n_channels_, sample_freq_, n_samples_ / n_channels_);
    bool ok = true;
    if (lseek (fd_, 0, SEEK_SET) >= 0)
      ok &= ::write (fd_, header.data(), header.size()) >= 0;
    ok &=::close (fd_) >= 0;
    fd_ = -1;
    return ok;
  }
};

WaveWriterP
wave_writer_create_wav (int rate, int channels, const String &filename, int mode, uint8_t n_bits)
{
  auto wavw = std::make_shared<WavWriterImpl>();
  if (wavw->open (filename, n_bits, channels, rate) == false)
    return nullptr;
  return wavw;
}

// == OpusWriter ==
String
wave_writer_opus_version()
{
  return opus_get_version_string();
}

class OpusWriter : public WaveWriter {
  String name_;
  OpusEncoder *enc_ = nullptr;
  int fd_ = -1;
  float *bmark_ = nullptr;
  std::vector<float> buffer_;
  ogg_stream_state ostream_ = { 0, };
  uint32_t rate_ = 0;
  uint8_t n_channels_ = 0;
  bool eos_ = false;
  uint packetno_ = 0;
  int64_t granulepos_ = 0;
  std::function<void()> flush_atquit;
public:
  OpusWriter (const String &filename)
  {
    name_ = filename;
    flush_atquit = [this] () { finish_and_close (true); };
    atquit_add (&flush_atquit);
  }
  ~OpusWriter()
  {
    atquit_del (&flush_atquit);
    close();
    opus_encoder_destroy (enc_);
    enc_ = nullptr;
    ogg_stream_clear (&ostream_);
  }
  String
  name() const override
  {
    return name_;
  }
  bool
  create (int mode)
  {
    errno = EBUSY;
    assert_return (fd_ < 0, false);
    fd_ = open (name_.c_str(), O_CREAT | O_TRUNC | O_WRONLY, mode);
    return fd_ >= 0;
  }
  bool
  setup_encoder (int rate, int channels, int complexity, float bitrate)
  {
    assert_return (fd_ >= 0, false);
    errno = EINVAL;
    assert_return (!enc_, false);
    assert_return (channels == 1 || channels == 2, false);
    assert_return (rate > 24000, false); // lets opus operate at 48000
    rate_ = rate;
    int error = 0;
    n_channels_ = channels;
    enc_ = opus_encoder_create (rate_, n_channels_, OPUS_APPLICATION_AUDIO, &error);
    if (error == OPUS_OK)
      {
        bitrate = n_channels_ * bitrate * 1000;
        const int serialno = random_int64();
        if (OPUS_OK == opus_encoder_ctl (enc_, OPUS_SET_BITRATE (MIN (256000 * n_channels_, bitrate))) &&
            OPUS_OK == opus_encoder_ctl (enc_, OPUS_SET_VBR (1)) &&
            OPUS_OK == opus_encoder_ctl (enc_, OPUS_SET_VBR_CONSTRAINT (0)) &&
            OPUS_OK == opus_encoder_ctl (enc_, OPUS_SET_FORCE_CHANNELS (n_channels_)) &&
            OPUS_OK == opus_encoder_ctl (enc_, OPUS_SET_COMPLEXITY (complexity)) &&
            ogg_stream_init (&ostream_, serialno) >= 0)
          {
            const int std_fragment_size = 20 * 48000 / 1000; // 960
            int fragment_size = std_fragment_size * rate_ / 48000;
            buffer_.resize (fragment_size * n_channels_);
            bmark_ = buffer_.data();
            if (write_header() > 0)
              return true;
          }
      }
    finish_and_close (false);
    errno = EINVAL;
    return false;
  }
  ssize_t
  write_packet (ogg_packet *op, bool force_flush)
  {
    ogg_stream_packetin (&ostream_, op);
    ogg_page opage;
    auto stream_pageout = force_flush ? ogg_stream_flush : ogg_stream_pageout;
    while (stream_pageout (&ostream_, &opage))
      {
        ssize_t n;
        do
          n = ::write (fd_, opage.header, opage.header_len);
        while (n == -1 && errno == EINTR);
        if (n != opage.header_len) return -1;
        do
          n = ::write (fd_, opage.body, opage.body_len);
        while (n == -1 && errno == EINTR);
        if (n != opage.body_len) return -1;
      }
    return 1;
  }
  ssize_t
  write_header ()
  {
    int lookahead = 0;
    if (OPUS_OK != opus_encoder_ctl (enc_, OPUS_GET_LOOKAHEAD (&lookahead)))
      lookahead = 0;
    // header packet
    struct OpusHeader {
      // https://www.rfc-editor.org/rfc/rfc7845.html#section-5
      char     magic[8] = { 'O', 'p', 'u', 's', 'H', 'e', 'a', 'd' };
      uint8_t  version = 1;
      uint8_t  channels = 0;
      uint16_t pre_skip_le = 0;
      uint32_t rate_le = 0;
      int16_t  gain_le = 0;       // in dB, stored as 20*log10(gainfactor) * 256; sample *= pow (10, gain_le/256./20)
      uint8_t  chmapping = 0;
      // uint8_t mappings...
    } __attribute__ ((packed));
    OpusHeader oh = {
      .channels = n_channels_,
      .pre_skip_le = htole16 (lookahead),
      .rate_le = htole32 (rate_),
    };
    static_assert (sizeof (oh) == 19);
    ogg_packet op0 = {
      .packet = (uint8_t*) &oh,
      .bytes = sizeof (oh),
      .b_o_s = 0, .e_o_s = 0,
      .granulepos = 0,
      .packetno = packetno_++
    };
    if (write_packet (&op0, true) < 0)
      return -1;
    // comment packet
    uint32_t u32_le;
    std::string cmtheader;
    cmtheader += "OpusTags";                            // 8, magic
    const String opus_version = opus_get_version_string();
    u32_le = htole32 (opus_version.size());
    cmtheader += String ((char*) &u32_le, 4);           // 4, vendor string length
    cmtheader += opus_version;                          // vendor string
    StringS tags;
    tags.insert (tags.begin(), "ENCODER=Anklang-" + String (ase_version_short));
    // R128_TRACK_GAIN, R128_ALBUM_GAIN, BPM, ARTIST, TITLE, DATE, ALBUM
    u32_le = htole32 (tags.size());
    cmtheader += String ((char*) &u32_le, 4);           // 4, number of tags
    for (const auto &tag : tags)
      {
        u32_le = htole32 (tag.size());
        cmtheader += String ((char*) &u32_le, 4);       // 4, tag length
        cmtheader += tag;                               // tag string
      }
    ogg_packet op1 = {
      .packet = (uint8_t*) cmtheader.data(),
      .bytes = long (cmtheader.size()),
      .b_o_s = 0, .e_o_s = 0,
      .granulepos = 0,
      .packetno = packetno_++
    };
    if (write_packet (&op1, true) < 0)
      return -1;
    return 2;
  }
  ssize_t
  write_ogg (uint8_t *data, long l, bool force_flush)
  {
    ogg_packet op = {
      .packet = data,
      .bytes = l,
      .b_o_s = 0, .e_o_s = eos_,
      .granulepos = granulepos_,
      .packetno = packetno_++
    };
    if (write_packet (&op, force_flush) < 0)
      return -1;
    return 1;
  }
  ssize_t
  write (const float *frames, size_t n_frames) override
  {
    return write_opus (frames, n_frames, false);
  }
  ssize_t
  write_opus (const float *frames, size_t n_frames, bool force_flush)
  {
    errno = EINVAL;
    return_unless (fd_ >= 0, -1);
    return_unless (n_frames, 0);
    assert_return (enc_, -1);
    assert_return (frames, -1);
    const float *fend = frames + n_frames * n_channels_;
    const float *bmax = buffer_.data() + buffer_.size();
    while (frames < fend)
      {
        ssize_t l = MIN (bmax - bmark_, fend - frames);
        fast_copy (l, bmark_, frames);
        frames += l;
        bmark_ += l;
        if (bmark_ == bmax)
          {
            uint8_t pmem[16384];
            bmark_ = buffer_.data();
            const uint n_frames = buffer_.size() / n_channels_;
            granulepos_ += n_frames;
            l = opus_encode_float (enc_, bmark_, n_frames, pmem, sizeof (pmem));
            if (l < 0)
              {
                printerr ("%s: OpusWriter encoding error: %s\n", name_, opus_strerror (l));
                finish_and_close (false);
                errno = EIO;
                return -1;
              }
            if (l > 0 && write_ogg (pmem, l, force_flush) < 0)
              {
                const int serrno = errno;
                finish_and_close (false);
                errno = serrno;
                return -1;
              }
          }
      }
    return n_frames;
  }
  bool
  finish_and_close (bool flush)
  {
    return_unless (fd_ >= 0, false);
    bool err = false;
    if (flush && enc_)
      {
        const size_t n_filled = bmark_ - buffer_.data();
        // flush fragment and ogg stream
        const size_t n_floats = buffer_.size() - n_filled;
        float zeros[n_floats];
        floatfill (zeros, 0, n_floats);
        eos_ = true;
        err &= write_opus (zeros, n_floats / n_channels_, true) < 0;
      }
    // flush file descroiptor
    err &= ::close (fd_) < 0;
    fd_ = -1;
    if (err)
      printerr ("%s: OpusWriter close error: %s\n", name_, strerror (errno ? errno : EIO));
    return !err;
  }
  bool
  close() override
  {
    return finish_and_close (true);
  }
};

WaveWriterP
wave_writer_create_opus (int rate, int channels, const String &filename, int mode, int complexity, float bitrate)
{
  auto ow = std::make_shared<OpusWriter> (filename);
  if (ow->create (mode) == false)
    return nullptr;
  if (!ow->setup_encoder (rate, channels, complexity, bitrate))
    return nullptr;
  return ow;
}

// == FlacWriter ==
String
wave_writer_flac_version()
{
  return FLAC__VERSION_STRING;
}

class FlacWriter : public WaveWriter {
  String name_;
  FLAC__StreamEncoder *enc_ = nullptr;
  FLAC__StreamMetadata *metadata_[2] = { nullptr, nullptr };
  uint32_t rate_ = 0;
  uint8_t n_channels_ = 0;
  std::vector<int32_t> ibuffer_;
  std::function<void()> flush_atquit;
public:
  FlacWriter (const String &filename)
  {
    name_ = filename;
    ibuffer_.reserve (65536);
    flush_atquit = [this] () { if (enc_) close(); };
    atquit_add (&flush_atquit);
  }
  ~FlacWriter()
  {
    atquit_del (&flush_atquit);
    close();
  }
  void
  cleanup()
  {
    if (enc_)
      FLAC__stream_encoder_delete (enc_);
    enc_ = nullptr;
    if (metadata_[0])
      FLAC__metadata_object_delete (metadata_[0]);
    metadata_[0] = nullptr;
  }
  bool
  close() override
  {
    errno = EINVAL;
    assert_return (enc_, false);
    const bool ok = FLAC__stream_encoder_finish (enc_);
    const int saved_errno = errno;
    // const FLAC__StreamEncoderState state = FLAC__stream_encoder_get_state (enc_);
    cleanup();
    if (!ok)
      printerr ("%s: FlacWriter error: %s\n", name_, strerror (saved_errno ? saved_errno : EIO));
    return ok;
  }
  bool
  create (int mode, int rate, int channels, int compresion)
  {
    errno = EINVAL;
    assert_return (channels == 1 || channels == 2, false);
    assert_return (rate > 24000, false); // lets flac operate at 48000
    errno = EBUSY;
    assert_return (enc_ == nullptr, false);
    const int fd = open (name_.c_str(), O_CREAT | O_TRUNC | O_RDWR, mode);
    return_unless (fd >= 0, false);
    FILE *file = fdopen (fd, "w+b");
    if (!file)
      {
        ::close (fd);
        return false;
      }
    enc_ = FLAC__stream_encoder_new();
    if (!enc_)
      {
        fclose (file); // closes fd
        return false;
      }
    rate_ = rate;
    n_channels_ = channels;
    bool setup_ok = true;
    setup_ok &= FLAC__stream_encoder_set_channels (enc_, n_channels_);
    setup_ok &= FLAC__stream_encoder_set_bits_per_sample (enc_, 24);
    setup_ok &= FLAC__stream_encoder_set_sample_rate (enc_, rate_);
    setup_ok &= FLAC__stream_encoder_set_compression_level (enc_, compresion);
    StringS tags;
    tags.insert (tags.begin(), "ENCODER=Anklang-" + String (ase_version_short));
    metadata_[0] = FLAC__metadata_object_new (FLAC__METADATA_TYPE_VORBIS_COMMENT);
    for (const auto &tag : tags)
      {
        FLAC__StreamMetadata_VorbisComment_Entry entry = { uint32_t (tag.size()), (uint8_t*) tag.data() };
        setup_ok &= FLAC__metadata_object_vorbiscomment_append_comment (metadata_[0], entry, true);
      }
    setup_ok &= FLAC__stream_encoder_set_metadata (enc_, metadata_, 1);
    printerr ("%s:%d: ok=%d\n", __FILE__, __LINE__, setup_ok);
    setup_ok &= FLAC__STREAM_ENCODER_INIT_STATUS_OK == FLAC__stream_encoder_init_FILE (enc_, file, nullptr, nullptr);
    printerr ("%s:%d: ok=%d\n", __FILE__, __LINE__, setup_ok);
    if (setup_ok)
      return true;
    cleanup(); // deletes enc_ which closes file
    errno = EINVAL;
    return false;
  }
  ssize_t
  write (const float *frames, size_t n_frames) override
  {
    errno = EINVAL;
    return_unless (enc_ != nullptr, -1);
    return_unless (n_frames, 0);
    assert_return (frames, -1);
    const size_t n_samples = n_frames * n_channels_;
    if (ibuffer_.size() < n_samples)
      ibuffer_.resize (n_samples);
    for (size_t i = 0; i < n_samples; i++)
      {
        int32_t v = frames[i] * 8388607.5 - 0.5;
        ibuffer_[i] = CLAMP (v, -8388608, +8388607);
      }
    const bool ok = FLAC__stream_encoder_process_interleaved (enc_, ibuffer_.data(), n_frames);
    return ok;
  }
  String
  name() const override
  {
    return name_;
  }
};

WaveWriterP
wave_writer_create_flac (int rate, int channels, const String &filename, int mode, int compresion)
{
  auto ow = std::make_shared<FlacWriter> (filename);
  if (ow->create (mode, rate, channels, compresion) == false)
    return nullptr;
  return ow;
}

} // Ase
