// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_WAVE_HH__
#define __ASE_WAVE_HH__

#include <ase/defs.hh>

namespace Ase {

class WaveWriter {
public:
  virtual        ~WaveWriter ();
  virtual String  name       () const = 0;
  virtual ssize_t write      (const float *frames, size_t n_frames) = 0;
  virtual bool    close      () = 0;
};
using WaveWriterP = std::shared_ptr<WaveWriter>;

WaveWriterP wave_writer_create_wav (int rate, int channels, const String &filename, int mode = 0664, uint8_t n_bits = 32);

WaveWriterP wave_writer_create_opus (int rate, int channels, const String &filename, int mode = 0664, int complexity = 10, float bitrate = 128);
String      wave_writer_opus_version ();

WaveWriterP wave_writer_create_flac (int rate, int channels, const String &filename, int mode = 0664, int compresion = 9);
String      wave_writer_flac_version ();

} // Ase

#endif // __ASE_WAVE_HH__
