// Drum Suite — SFZ / SF2 / multisample loader tests
// SPDX-License-Identifier: GPL-3.0-or-later
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "dsp/instrument.hpp"
#include "dsp/sample_player.hpp"

namespace {

void writeU16(std::ofstream& f, uint16_t v) {
  const char b[2] = {static_cast<char>(v & 0xff),
                     static_cast<char>((v >> 8) & 0xff)};
  f.write(b, 2);
}
void writeU32(std::ofstream& f, uint32_t v) {
  const char b[4] = {static_cast<char>(v & 0xff),
                     static_cast<char>((v >> 8) & 0xff),
                     static_cast<char>((v >> 16) & 0xff),
                     static_cast<char>((v >> 24) & 0xff)};
  f.write(b, 4);
}

void writeWav(const std::filesystem::path& path, float amplitude,
              int frames = 200) {
  std::ofstream f(path, std::ios::binary);
  const uint32_t dataSize = static_cast<uint32_t>(frames * 2);
  f.write("RIFF", 4);
  writeU32(f, 36 + dataSize);
  f.write("WAVE", 4);
  f.write("fmt ", 4);
  writeU32(f, 16);
  writeU16(f, 1);
  writeU16(f, 1);
  writeU32(f, 48000);
  writeU32(f, 48000 * 2);
  writeU16(f, 2);
  writeU16(f, 16);
  f.write("data", 4);
  writeU32(f, dataSize);
  for (int i = 0; i < frames; ++i) {
    const float s = amplitude * std::sin(2.0 * 3.14159265 * 220.0 * i / 48000.0);
    const int16_t v = static_cast<int16_t>(std::lround(s * 32767.0f));
    f.write(reinterpret_cast<const char*>(&v), 2);
  }
}

void appendChunk(std::vector<uint8_t>& out, const char* id,
                 const std::vector<uint8_t>& body) {
  out.insert(out.end(), id, id + 4);
  const uint32_t n = static_cast<uint32_t>(body.size());
  for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(n >> (8 * i)));
  out.insert(out.end(), body.begin(), body.end());
  if (n & 1u) out.push_back(0);
}

void appendName(std::vector<uint8_t>& out, const char* name) {
  char buf[20] = {};
  std::snprintf(buf, sizeof(buf), "%s", name);
  out.insert(out.end(), buf, buf + 20);
}

std::vector<uint8_t> makeSf2() {
  // sdta: one 200-frame 16-bit sample.
  std::vector<uint8_t> smpl;
  for (int i = 0; i < 200; ++i) {
    const float s = 0.5f * std::sin(2.0 * 3.14159265 * 220.0 * i / 48000.0);
    const int16_t v = static_cast<int16_t>(std::lround(s * 32767.0f));
    smpl.push_back(static_cast<uint8_t>(v & 0xff));
    smpl.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
  }
  std::vector<uint8_t> sdta;
  appendChunk(sdta, "smpl", smpl);

  std::vector<uint8_t> phdr;
  appendName(phdr, "Test Kit");
  for (int i = 0; i < 6; ++i) phdr.push_back(0);  // preset, bank, bag
  for (int i = 0; i < 12; ++i) phdr.push_back(0);

  std::vector<uint8_t> pbag;
  pbag.push_back(0);
  pbag.push_back(0);  // gen 0
  pbag.push_back(1);
  pbag.push_back(0);  // gen count 1

  std::vector<uint8_t> pgen;
  pgen.push_back(41);  // instrument
  pgen.push_back(0);
  pgen.push_back(0);
  pgen.push_back(0);  // amount 0

  std::vector<uint8_t> inst;
  appendName(inst, "Inst");
  inst.push_back(0);
  inst.push_back(0);  // bag 0

  std::vector<uint8_t> ibag;
  ibag.push_back(0);
  ibag.push_back(0);  // gen 0
  ibag.push_back(4);
  ibag.push_back(0);  // gen count 4

  std::vector<uint8_t> igen;
  auto gen = [&](uint16_t oper, int16_t amount) {
    igen.push_back(static_cast<uint8_t>(oper & 0xff));
    igen.push_back(static_cast<uint8_t>(oper >> 8));
    igen.push_back(static_cast<uint8_t>(amount & 0xff));
    igen.push_back(static_cast<uint8_t>((amount >> 8) & 0xff));
  };
  gen(43, static_cast<int16_t>(36 | (36 << 8)));   // key range 36
  gen(44, static_cast<int16_t>(0 | (127 << 8)));   // velocity range
  gen(58, 36);                                     // overriding root key
  gen(53, 0);                                      // sample id
  gen(0, 0);                                       // terminator

  std::vector<uint8_t> shdr;
  appendName(shdr, "Sample");
  for (uint32_t v : {0u, 200u, 0u, 200u, 48000u}) {
    for (int i = 0; i < 4; ++i) shdr.push_back(static_cast<uint8_t>(v >> (8 * i)));
  }
  shdr.push_back(60);  // original pitch
  shdr.push_back(0);   // correction
  shdr.push_back(0);
  shdr.push_back(0);   // link
  shdr.push_back(1);
  shdr.push_back(0);   // type mono

  std::vector<uint8_t> pdta;
  appendChunk(pdta, "phdr", phdr);
  appendChunk(pdta, "pbag", pbag);
  appendChunk(pdta, "pgen", pgen);
  appendChunk(pdta, "inst", inst);
  appendChunk(pdta, "ibag", ibag);
  appendChunk(pdta, "igen", igen);
  appendChunk(pdta, "shdr", shdr);

  std::vector<uint8_t> body;
  std::vector<uint8_t> sdtaList;
  sdtaList.insert(sdtaList.end(), {'s', 'd', 't', 'a'});
  sdtaList.insert(sdtaList.end(), sdta.begin(), sdta.end());
  appendChunk(body, "LIST", sdtaList);
  std::vector<uint8_t> pdtaList;
  pdtaList.insert(pdtaList.end(), {'p', 'd', 't', 'a'});
  pdtaList.insert(pdtaList.end(), pdta.begin(), pdta.end());
  appendChunk(body, "LIST", pdtaList);

  std::vector<uint8_t> out;
  out.insert(out.end(), {'R', 'I', 'F', 'F'});
  const uint32_t total = static_cast<uint32_t>(body.size() + 4);
  for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(total >> (8 * i)));
  out.insert(out.end(), {'s', 'f', 'b', 'k'});
  out.insert(out.end(), body.begin(), body.end());
  return out;
}

} // namespace

int main() {
  namespace fs = std::filesystem;
  const fs::path root = "/tmp/opencode/drum-instrument-test";
  fs::remove_all(root);
  fs::create_directories(root);

  writeWav(root / "quiet.wav", 0.10f);
  writeWav(root / "loud.wav", 0.60f);
  {
    std::ofstream sfz(root / "test.sfz");
    sfz << "<control> default_path=.\n";
    sfz << "<group> lovel=0 hivel=63 key=36 pitch_keycenter=36\n";
    sfz << "<region> sample=quiet.wav\n";
    sfz << "<group> lovel=64 hivel=127 key=36 pitch_keycenter=36\n";
    sfz << "<region> sample=loud.wav\n";
  }
  {
    std::ofstream sf2(root / "test.sf2", std::ios::binary);
    const auto bytes = makeSf2();
    sf2.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
  }

  auto sfz = drum::loadInstrument((root / "test.sfz").string());
  assert(sfz && sfz->zones.size() == 2);
  assert(sfz->zones[0].lovel == 0 && sfz->zones[0].hivel == 63);
  assert(sfz->zones[1].lovel == 64 && sfz->zones[1].hivel == 127);
  assert(sfz->zones[0].lokey == 36 && sfz->zones[1].hikey == 36);

  auto sf2 = drum::loadInstrument((root / "test.sf2").string());
  assert(sf2 && !sf2->zones.empty());
  assert(sf2->zones[0].sample && sf2->zones[0].sample->data.size() == 200);

  auto folder = drum::loadInstrument(root.string());
  assert(folder && folder->zones.size() >= 2);

  // Velocity selects the quiet/loud zone.
  drum::SamplePlayer player;
  player.prepare(48000.0f);
  player.setInstrument(sfz);
  drum::Controls c;
  c.sampleOn = true;
  c.sampleLevel = 100.0f;
  c.sampleDirect = 100.0f;
  c.sampleTrigger = 3;
  c.sampleNote = 36;
  c.sampleMidiNote = 36;
  c.sampleChoke = false;
  c.sampleFadeMs = 0.1f;
  c.sampleMidiVelocity = 20;
  player.setControls(c);
  std::vector<float> l(256, 0.0f), r(256, 0.0f);
  std::vector<uint8_t> gate(256, 0);
  drum::Meters m;
  c.sampleMidiTrigger = 1;
  player.setControls(c);
  player.process(l.data(), r.data(), gate.data(), 256, m);
  double quiet = 0.0, loud = 0.0;
  for (float v : l) quiet += std::fabs(v);

  player.reset();
  player.setInstrument(sfz);
  c.sampleMidiTrigger = 1;
  c.sampleMidiVelocity = 120;
  player.setControls(c);
  std::fill(l.begin(), l.end(), 0.0f);
  std::fill(r.begin(), r.end(), 0.0f);
  player.process(l.data(), r.data(), gate.data(), 256, m);
  for (float v : l) loud += std::fabs(v);
  assert(loud > quiet * 2.0);

  {
    // A held gate must trigger once, not every sample.
    auto buffer = std::make_shared<drum::SampleBuffer>();
    buffer->data.assign(8000, 0.1f);
    buffer->sampleRate = 48000;
    drum::SamplePlayer p;
    p.prepare(48000.0f);
    p.setSample(buffer);
    drum::Controls c;
    c.sampleOn = true;
    c.sampleLevel = 100.0f;
    c.sampleDirect = 0.0f;
    c.sampleChoke = false;
    c.sampleTrigger = 0;
    c.sampleNote = 36;
    p.setControls(c);
    std::vector<float> l(512, 0.0f), r(512, 0.0f);
    std::vector<uint8_t> gate(512, 1);
    drum::Meters m;
    p.process(l.data(), r.data(), gate.data(), 512, m);
    assert(m.sampleVoices <= 1);
  }

  {
    // Round robins still cycle when the selected note is outside the SFZ key
    // range (single-drum instruments often use a fixed key).
    auto a = std::make_shared<drum::SampleBuffer>();
    a->data.assign(200, 0.5f);
    a->sampleRate = 48000;
    auto b = std::make_shared<drum::SampleBuffer>();
    b->data.assign(200, -0.25f);
    b->sampleRate = 48000;
    auto inst = std::make_shared<drum::Instrument>();
    drum::Zone za;
    za.sample = a;
    za.lokey = za.hikey = 40;
    za.seqLength = 2;
    za.seqPosition = 1;
    inst->zones.push_back(za);
    drum::Zone zb = za;
    zb.sample = b;
    zb.seqPosition = 2;
    inst->zones.push_back(zb);
    drum::SamplePlayer p;
    p.prepare(48000.0f);
    p.setInstrument(inst);
    drum::Controls c;
    c.sampleOn = true;
    c.sampleLevel = 100.0f;
    c.sampleDirect = 0.0f;
    c.sampleChoke = false;
    c.sampleTrigger = 3;
    c.sampleNote = 36;
    c.sampleMidiNote = 36;
    c.sampleMidiVelocity = 100;
    c.sampleFadeMs = 0.1f;
    std::vector<float> l(300), r(300);
    std::vector<uint8_t> gate(300, 0);
    drum::Meters m;
    c.sampleMidiTrigger = 1;
    p.setControls(c);
    p.process(l.data(), r.data(), gate.data(), 300, m);
    const float first = l[1];
    c.sampleMidiTrigger = 1;
    p.setControls(c);
    std::fill(l.begin(), l.end(), 0.0f);
    p.process(l.data(), r.data(), gate.data(), 300, m);
    assert(first > 0.05f && l[1] < -0.01f);

    // Phase invert flips the sample layer.
    p.setInstrument(inst);
    c.sampleMidiTrigger = 1;
    c.samplePhase = true;
    p.setControls(c);
    std::fill(l.begin(), l.end(), 0.0f);
    p.process(l.data(), r.data(), gate.data(), 300, m);
    assert(l[1] < -0.01f);
  }

  std::printf("drum_instrument: ok\n");
  return 0;
}
