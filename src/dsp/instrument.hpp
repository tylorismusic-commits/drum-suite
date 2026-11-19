// Drum Suite — multisample instrument loaders (SFZ, SF2, WAV folders)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "controls.hpp"
#include "flac.hpp"
#include "wav.hpp"

namespace drum {

inline std::string lowerAscii(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return s;
}

inline std::string trimSfz(const std::string& in) {
  const size_t a = in.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return {};
  const size_t b = in.find_last_not_of(" \t\r\n");
  return in.substr(a, b - a + 1);
}

inline int noteNumber(const std::string& raw) {
  const std::string s = lowerAscii(trimSfz(raw));
  if (s.empty()) return 60;
  bool numeric = true;
  for (size_t i = 0; i < s.size(); ++i) {
    if (i == 0 && (s[i] == '-' || s[i] == '+')) continue;
    if (!std::isdigit(static_cast<unsigned char>(s[i]))) {
      numeric = false;
      break;
    }
  }
  if (numeric) return std::clamp(std::atoi(s.c_str()), 0, 127);
  int semitone = -1;
  size_t i = 0;
  switch (s[0]) {
    case 'c': semitone = 0; break;
    case 'd': semitone = 2; break;
    case 'e': semitone = 4; break;
    case 'f': semitone = 5; break;
    case 'g': semitone = 7; break;
    case 'a': semitone = 9; break;
    case 'b': semitone = 11; break;
    default: return 60;
  }
  ++i;
  if (i < s.size() && (s[i] == '#' || s[i] == 's')) {
    ++semitone;
    ++i;
  } else if (i < s.size() && s[i] == 'b') {
    --semitone;
    ++i;
  }
  while (i < s.size() && !std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
  int octave = 4;
  if (i < s.size()) octave = std::atoi(s.c_str() + i);
  return std::clamp((octave + 1) * 12 + semitone, 0, 127);
}

inline std::shared_ptr<const SampleBuffer> loadWavBuffer(
    const std::string& path) noexcept {
  WavData wav;
  const std::string ext = lowerAscii(
      std::filesystem::path(path).extension().string());
  const bool ok = ext == ".flac" ? readFlacData(path, wav, 0)
                                 : readWavData(path, wav, 0);
  if (!ok || wav.left.empty()) return nullptr;
  auto buffer = std::make_shared<SampleBuffer>();
  buffer->data = std::move(wav.left);
  buffer->dataR = std::move(wav.right);
  buffer->sampleRate = wav.sampleRate;
  buffer->loopStart = wav.loopStart;
  buffer->loopEnd = wav.loopEnd;
  buffer->name = std::filesystem::path(path).filename().string();
  buffer->path = path;
  return buffer;
}

inline std::string resolveSamplePath(const std::string& base,
                                     const std::string& raw,
                                     const std::string& defaultPath) {
  std::string s = trimSfz(raw);
  if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') ||
                        (s.front() == '\'' && s.back() == '\''))) {
    s = s.substr(1, s.size() - 2);
  }
  std::filesystem::path p(s);
  if (p.is_absolute()) return p.lexically_normal().string();
  if (!defaultPath.empty()) {
    std::filesystem::path d(defaultPath);
    if (d.is_absolute()) {
      const auto candidate = d / p;
      if (std::filesystem::exists(candidate)) return candidate.lexically_normal().string();
    } else {
      const auto candidate = std::filesystem::path(base) / d / p;
      if (std::filesystem::exists(candidate)) return candidate.lexically_normal().string();
    }
  }
  return (std::filesystem::path(base) / p).lexically_normal().string();
}

inline bool parseSfzFile(const std::string& path, Instrument& instrument,
                         int depth = 0) {
  if (depth > 8) return false;
  std::ifstream in(path);
  if (!in) return false;
  const std::string base = std::filesystem::path(path).parent_path().string();
  std::map<std::string, std::string> globals;
  std::map<std::string, std::string> group;
  std::map<std::string, std::string> region;
  std::map<std::string, std::shared_ptr<const SampleBuffer>> cache;
  std::string defaultPath;
  bool haveRegion = false;
  bool inGroup = false;

  auto merged = [&]() {
    std::map<std::string, std::string> m = globals;
    for (const auto& kv : group) m[kv.first] = kv.second;
    for (const auto& kv : region) m[kv.first] = kv.second;
    return m;
  };
  auto value = [&](const std::map<std::string, std::string>& m,
                   const std::string& key, const std::string& fallback) {
    const auto it = m.find(key);
    return it == m.end() ? fallback : it->second;
  };
  auto number = [&](const std::map<std::string, std::string>& m,
                    const std::string& key, double fallback) {
    const auto it = m.find(key);
    if (it == m.end()) return fallback;
    return std::atof(it->second.c_str());
  };

  auto finishRegion = [&]() {
    if (!haveRegion) return;
    const auto m = merged();
    const std::string raw = value(m, "sample", "");
    if (raw.empty()) return;
    const std::string samplePath = resolveSamplePath(base, raw, defaultPath);
    auto it = cache.find(samplePath);
    if (it == cache.end()) {
      it = cache.emplace(samplePath, loadWavBuffer(samplePath)).first;
    }
    if (!it->second || it->second->data.empty()) return;
    Zone z;
    z.sample = it->second;
    const int key = noteNumber(value(m, "key", "60"));
    z.lokey = std::clamp(static_cast<int>(number(m, "lokey", key)), 0, 127);
    z.hikey = std::clamp(static_cast<int>(number(m, "hikey", key)), 0, 127);
    if (m.find("lokey") == m.end() && m.find("hikey") == m.end()) {
      if (m.find("key") == m.end()) {
        z.lokey = 0;
        z.hikey = 127;
      } else {
        z.lokey = key;
        z.hikey = key;
      }
    }
    z.lovel = std::clamp(static_cast<int>(number(m, "lovel", 0)), 0, 127);
    z.hivel = std::clamp(static_cast<int>(number(m, "hivel", 127)), 0, 127);
    if (m.find("lovel") == m.end() && m.find("hivel") == m.end()) {
      z.lovel = 0;
      z.hivel = 127;
    }
    if (z.lokey > z.hikey) std::swap(z.lokey, z.hikey);
    if (z.lovel > z.hivel) std::swap(z.lovel, z.hivel);
    const std::string center = value(m, "pitch_keycenter", "");
    z.rootKey = center.empty() ? -1 : noteNumber(center);
    z.tuneCents = static_cast<float>(number(m, "tune", 0.0));
    z.gainDb = static_cast<float>(number(m, "volume", 0.0));
    const double frames = static_cast<double>(z.sample->data.size());
    if (frames > 1.0) {
      const double offset = number(m, "offset", 0.0);
      const double end = number(m, "end", frames);
      z.start = static_cast<float>(std::clamp(offset / frames, 0.0, 1.0));
      z.end = static_cast<float>(std::clamp(end / frames, 0.01, 1.0));
      if (z.end <= z.start) z.end = 1.0f;
    }
    z.pan = std::clamp(static_cast<float>(number(m, "pan", 0.0) / 100.0),
                       -1.0f, 1.0f);
    z.seqLength = std::max(0, static_cast<int>(number(m, "seq_length", 0)));
    z.seqPosition =
        std::max(1, static_cast<int>(number(m, "seq_position", 1)));
    z.loRand = std::clamp(static_cast<int>(number(m, "lorand", 0)), 0, 127);
    z.hiRand = std::clamp(static_cast<int>(number(m, "hirand", 127)), 0, 127);
    z.group = static_cast<int>(number(m, "group", 0));
    z.offBy = static_cast<int>(number(m, "off_by", 0));
    const std::string mode = lowerAscii(value(m, "loop_mode", "one_shot"));
    z.loop = (mode == "loop_continuous" || mode == "loop_sustain");
    if (m.find("loop_start") != m.end()) {
      z.sample = std::make_shared<SampleBuffer>(*it->second);
      auto copy = std::const_pointer_cast<SampleBuffer>(z.sample);
      copy->loopStart = static_cast<int>(number(m, "loop_start", 0));
      copy->loopEnd = static_cast<int>(
          number(m, "loop_end", static_cast<double>(copy->data.size())));
    }
    if (z.loop && z.sample->loopStart >= 0 && z.sample->loopEnd > z.sample->loopStart) {
      instrument.zones.push_back(std::move(z));
    } else if (!z.loop) {
      instrument.zones.push_back(std::move(z));
    }
  };

  std::string line;
  while (std::getline(in, line)) {
    const size_t comment = line.find("//");
    if (comment != std::string::npos) line.resize(comment);
    line = trimSfz(line);
    if (line.empty()) continue;
    if (line[0] == '<') {
      const size_t close = line.find('>');
      if (close == std::string::npos) continue;
      const std::string section = lowerAscii(line.substr(1, close - 1));
      if (section == "region") {
        finishRegion();
        region.clear();
        haveRegion = true;
        inGroup = false;
      } else if (section == "group") {
        finishRegion();
        haveRegion = false;
        region.clear();
        group = globals;
        inGroup = true;
      } else if (section == "global") {
        finishRegion();
        haveRegion = false;
        region.clear();
        group.clear();
        inGroup = false;
      } else if (section == "control") {
        finishRegion();
        haveRegion = false;
        region.clear();
        inGroup = false;
      }
      const std::string rest = trimSfz(line.substr(close + 1));
      if (rest.empty()) continue;
      line = rest;
    }
    if (line.rfind("#include", 0) == 0) {
      std::string inc = trimSfz(line.substr(8));
      if (inc.size() >= 2 && inc.front() == '"' && inc.back() == '"') {
        inc = inc.substr(1, inc.size() - 2);
      }
      const std::string incPath =
          resolveSamplePath(base, inc, defaultPath);
      if (depth < 8) {
        parseSfzFile(incPath, instrument, depth + 1);
      }
      continue;
    }
    size_t i = 0;
    while (i < line.size()) {
      while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
      const size_t eq = line.find('=', i);
      if (eq == std::string::npos) break;
      std::string key = lowerAscii(trimSfz(line.substr(i, eq - i)));
      if (key.empty() || key.find(' ') != std::string::npos) break;
      size_t vstart = eq + 1;
      while (vstart < line.size() &&
             std::isspace(static_cast<unsigned char>(line[vstart]))) {
        ++vstart;
      }
      size_t vend = line.size();
      if (vstart < line.size() &&
          (line[vstart] == '"' || line[vstart] == '\'')) {
        const char quote = line[vstart++];
        const size_t closeq = line.find(quote, vstart);
        vend = closeq == std::string::npos ? line.size() : closeq;
      } else {
        for (size_t j = vstart; j < line.size(); ++j) {
          if (!std::isspace(static_cast<unsigned char>(line[j]))) continue;
          size_t k = j;
          while (k < line.size() &&
                 std::isspace(static_cast<unsigned char>(line[k]))) {
            ++k;
          }
          const size_t nextEq = line.find('=', k);
          if (nextEq == std::string::npos) continue;
          const std::string token = trimSfz(line.substr(k, nextEq - k));
          if (token.empty() || token.find(' ') != std::string::npos) continue;
          bool valid = true;
          for (char c : token) {
            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) {
              valid = false;
              break;
            }
          }
          if (valid) {
            vend = j;
            break;
          }
        }
      }
      const std::string val = trimSfz(line.substr(vstart, vend - vstart));
      i = vend < line.size() ? vend + 1 : line.size();
      if (key == "default_path") {
        defaultPath = val;
      } else if (haveRegion) {
        region[key] = val;
      } else if (inGroup) {
        group[key] = val;
      } else {
        globals[key] = val;
      }
    }
  }
  finishRegion();
  instrument.type = "sfz";
  instrument.path = path;
  if (instrument.name.empty()) {
    instrument.name = std::filesystem::path(path).stem().string();
  }
  return !instrument.zones.empty();
}

// --- SF2 -------------------------------------------------------------------

inline uint16_t rd16(const std::vector<uint8_t>& d, size_t off) {
  if (off + 2 > d.size()) return 0;
  return static_cast<uint16_t>(d[off] | (d[off + 1] << 8));
}
inline uint32_t rd32(const std::vector<uint8_t>& d, size_t off) {
  if (off + 4 > d.size()) return 0;
  return static_cast<uint32_t>(d[off]) |
         (static_cast<uint32_t>(d[off + 1]) << 8) |
         (static_cast<uint32_t>(d[off + 2]) << 16) |
         (static_cast<uint32_t>(d[off + 3]) << 24);
}
inline std::string chunkName(const std::vector<uint8_t>& d, size_t off,
                             size_t len) {
  if (off + len > d.size()) return {};
  return std::string(reinterpret_cast<const char*>(d.data() + off), len);
}

struct Sf2Chunk {
  size_t offset = 0;
  size_t size = 0;
};

inline void collectSf2List(const std::vector<uint8_t>& d, size_t start,
                           size_t size, std::map<std::string, Sf2Chunk>& out) {
  size_t pos = start;
  const size_t end = std::min(d.size(), start + size);
  while (pos + 8 <= end) {
    const std::string id = chunkName(d, pos, 4);
    const uint32_t n = rd32(d, pos + 4);
    if (pos + 8 + n > end) break;
    if (id == "LIST") {
      const std::string type = chunkName(d, pos + 8, 4);
      collectSf2List(d, pos + 12, n - 4, out);
      (void)type;
    } else {
      out[id] = {pos + 8, n};
    }
    pos += 8 + n + (n & 1u);
  }
}

struct Sf2Gen {
  uint16_t oper = 0;
  int16_t amount = 0;
};

inline std::vector<Sf2Gen> readSf2Gens(const std::vector<uint8_t>& d,
                                       const Sf2Chunk& c, size_t bag,
                                       size_t nextBag) {
  std::vector<Sf2Gen> out;
  const size_t count = c.size / 4;
  for (size_t i = bag; i < nextBag && i < count; ++i) {
    Sf2Gen g;
    g.oper = rd16(d, c.offset + i * 4);
    g.amount = static_cast<int16_t>(rd16(d, c.offset + i * 4 + 2));
    if (g.oper == 0) break;
    out.push_back(g);
  }
  return out;
}

inline int sf2RangeLo(int16_t v) { return static_cast<uint8_t>(v & 0xff); }
inline int sf2RangeHi(int16_t v) {
  return static_cast<uint8_t>((static_cast<uint16_t>(v) >> 8) & 0xff);
}

inline double sf2Timecents(int16_t v) {
  return std::pow(2.0, static_cast<double>(v) / 1200.0);
}

inline bool parseSf2File(const std::string& path, Instrument& instrument) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in) return false;
  const std::streamsize size = in.tellg();
  if (size <= 12 || size > 1024ll * 1024 * 1024) return false;
  in.seekg(0);
  std::vector<uint8_t> d(static_cast<size_t>(size));
  if (!in.read(reinterpret_cast<char*>(d.data()), size)) return false;
  if (chunkName(d, 0, 4) != "RIFF" || chunkName(d, 8, 4) != "sfbk") {
    return false;
  }

  std::map<std::string, Sf2Chunk> chunks;
  size_t pos = 12;
  while (pos + 8 <= d.size()) {
    const std::string id = chunkName(d, pos, 4);
    const uint32_t n = rd32(d, pos + 4);
    if (pos + 8 + n > d.size()) break;
    if (id == "LIST") collectSf2List(d, pos + 12, n > 4 ? n - 4 : 0, chunks);
    pos += 8 + n + (n & 1u);
  }
  if (!chunks.count("phdr") || !chunks.count("pbag") ||
      !chunks.count("pgen") || !chunks.count("inst") ||
      !chunks.count("ibag") || !chunks.count("igen") ||
      !chunks.count("shdr") || !chunks.count("smpl")) {
    return false;
  }

  const Sf2Chunk& shdr = chunks["shdr"];
  const Sf2Chunk& smpl = chunks["smpl"];
  const size_t sampleCount = shdr.size / 46;
  struct SampleInfo {
    std::string name;
    uint32_t start = 0, end = 0, loopStart = 0, loopEnd = 0;
    uint32_t rate = 44100;
    int originalPitch = 60;
    int correction = 0;
    int link = 0;
    int type = 1;
  };
  std::vector<SampleInfo> samples(sampleCount);
  for (size_t i = 0; i < sampleCount; ++i) {
    const size_t o = shdr.offset + i * 46;
    samples[i].name = trimSfz(chunkName(d, o, 20));
    samples[i].start = rd32(d, o + 20);
    samples[i].end = rd32(d, o + 24);
    samples[i].loopStart = rd32(d, o + 28);
    samples[i].loopEnd = rd32(d, o + 32);
    samples[i].rate = rd32(d, o + 36);
    samples[i].originalPitch = d[o + 40];
    samples[i].correction = static_cast<int8_t>(d[o + 41]);
    samples[i].link = rd16(d, o + 42);
    samples[i].type = rd16(d, o + 44);
  }

  auto sampleAt = [&](int index) -> std::shared_ptr<const SampleBuffer> {
    if (index < 0 || index >= static_cast<int>(samples.size())) return nullptr;
    const SampleInfo& s = samples[index];
    if (s.end <= s.start || s.end > smpl.size / 2) return nullptr;
    auto buffer = std::make_shared<SampleBuffer>();
    buffer->sampleRate = s.rate ? static_cast<int>(s.rate) : 44100;
    buffer->rootKey = s.originalPitch;
    buffer->tuneCents = static_cast<float>(s.correction);
    buffer->name = s.name;
    buffer->path = path;
    const size_t frames = s.end - s.start;
    buffer->data.resize(frames);
    if (s.loopEnd > s.loopStart + 1 && s.loopEnd <= frames) {
      buffer->loopStart = static_cast<int>(s.loopStart);
      buffer->loopEnd = static_cast<int>(s.loopEnd);
    }
    for (size_t i = 0; i < frames; ++i) {
      const int16_t v = static_cast<int16_t>(rd16(d, smpl.offset + (s.start + i) * 2));
      buffer->data[i] = static_cast<float>(v) / 32768.0f;
    }
    // Link the right channel when the sample header declares a left sample.
    if (s.link > 0 && s.link < static_cast<int>(samples.size()) &&
        (s.type & 4) && (samples[s.link].type & 2)) {
      const SampleInfo& r = samples[s.link];
      if (r.end > r.start) {
        buffer->dataR.resize(frames, 0.0f);
        const size_t n = std::min(frames, static_cast<size_t>(r.end - r.start));
        for (size_t i = 0; i < n; ++i) {
          const int16_t v =
              static_cast<int16_t>(rd16(d, smpl.offset + (r.start + i) * 2));
          buffer->dataR[i] = static_cast<float>(v) / 32768.0f;
        }
      }
    }
    return buffer;
  };

  const Sf2Chunk& phdr = chunks["phdr"];
  const Sf2Chunk& pbag = chunks["pbag"];
  const Sf2Chunk& pgen = chunks["pgen"];
  const Sf2Chunk& inst = chunks["inst"];
  const Sf2Chunk& ibag = chunks["ibag"];
  const Sf2Chunk& igen = chunks["igen"];
  const size_t presetCount = phdr.size / 38;
  if (presetCount == 0) return false;
  const std::string presetName = trimSfz(chunkName(d, phdr.offset, 20));
  const size_t pBagIndex = rd16(d, phdr.offset + 24);
  const size_t pBagNext = presetCount > 1 ? rd16(d, phdr.offset + 38 + 24)
                                           : pbag.size / 4;
  std::map<int, std::shared_ptr<const SampleBuffer>> sampleCache;
  std::vector<bool> rightLinked(samples.size(), false);
  for (size_t i = 0; i < samples.size(); ++i) {
    if ((samples[i].type & 2) && samples[i].link >= 0 &&
        samples[i].link < static_cast<int>(samples.size()) &&
        (samples[samples[i].link].type & 4)) {
      rightLinked[i] = true;
    }
  }

  for (size_t pb = pBagIndex; pb < pBagNext; ++pb) {
    const size_t genStart = rd16(d, pbag.offset + pb * 4);
    const size_t genEnd =
        pb + 1 < pbag.size / 4 ? rd16(d, pbag.offset + (pb + 1) * 4)
                               : pgen.size / 4;
    const auto pgens = readSf2Gens(d, pgen, genStart, genEnd);
    int instIndex = -1;
    int pKeyLo = 0, pKeyHi = 127, pVelLo = 0, pVelHi = 127;
    for (const auto& g : pgens) {
      if (g.oper == 41) instIndex = g.amount;
      if (g.oper == 43) {
        pKeyLo = sf2RangeLo(g.amount);
        pKeyHi = sf2RangeHi(g.amount);
      }
      if (g.oper == 44) {
        pVelLo = sf2RangeLo(g.amount);
        pVelHi = sf2RangeHi(g.amount);
      }
    }
    if (instIndex < 0 || instIndex >= static_cast<int>(inst.size / 22)) continue;
    const size_t iBagIndex = rd16(d, inst.offset + instIndex * 22 + 20);
    const size_t iBagNext =
        instIndex + 1 < static_cast<int>(inst.size / 22)
            ? rd16(d, inst.offset + (instIndex + 1) * 22 + 20)
            : ibag.size / 4;
    int dKeyLo = 0, dKeyHi = 127, dVelLo = 0, dVelHi = 127;
    int dRoot = -1, dCoarse = 0, dFine = 0, dMode = 0;
    float dAtt = 0.0f, dPan = 0.0f;
    for (size_t ib = iBagIndex; ib < iBagNext; ++ib) {
      const size_t igStart = rd16(d, ibag.offset + ib * 4);
      const size_t igEnd =
          ib + 1 < ibag.size / 4 ? rd16(d, ibag.offset + (ib + 1) * 4)
                                 : igen.size / 4;
      const auto igens = readSf2Gens(d, igen, igStart, igEnd);
      Zone z;
      int sampleID = -1;
      int keyLo = dKeyLo, keyHi = dKeyHi, velLo = dVelLo, velHi = dVelHi;
      int root = dRoot, coarse = dCoarse, fine = dFine, mode = dMode;
      float attenuation = dAtt, pan = dPan;
      for (const auto& g : igens) {
        switch (g.oper) {
          case 43: keyLo = sf2RangeLo(g.amount); keyHi = sf2RangeHi(g.amount); break;
          case 44: velLo = sf2RangeLo(g.amount); velHi = sf2RangeHi(g.amount); break;
          case 51: coarse = g.amount; break;
          case 52: fine = g.amount; break;
          case 46: keyLo = keyHi = g.amount; break;
          case 47: velLo = velHi = g.amount; break;
          case 53: sampleID = g.amount; break;
          case 54: mode = g.amount; break;
          case 58: root = g.amount; break;
          case 17: pan = static_cast<float>(g.amount) / 500.0f; break;
          case 48: attenuation = static_cast<float>(g.amount) / 10.0f; break;
          default: break;
        }
      }
      if (sampleID < 0) {
        dKeyLo = keyLo;
        dKeyHi = keyHi;
        dVelLo = velLo;
        dVelHi = velHi;
        dRoot = root;
        dCoarse = coarse;
        dFine = fine;
        dMode = mode;
        dAtt = attenuation;
        dPan = pan;
        continue;
      }
      if (sampleID < static_cast<int>(rightLinked.size()) &&
          rightLinked[static_cast<size_t>(sampleID)]) {
        continue;
      }
      keyLo = std::max(keyLo, pKeyLo);
      keyHi = std::min(keyHi, pKeyHi);
      velLo = std::max(velLo, pVelLo);
      velHi = std::min(velHi, pVelHi);
      if (keyLo > keyHi || velLo > velHi) continue;
      if (!sampleCache.count(sampleID)) {
        sampleCache[sampleID] = sampleAt(sampleID);
      }
      z.sample = sampleCache[sampleID];
      if (!z.sample) continue;
      z.lokey = keyLo;
      z.hikey = keyHi;
      z.lovel = velLo;
      z.hivel = velHi;
      z.rootKey = root >= 0 ? root : z.sample->rootKey;
      z.tuneCents = static_cast<float>(coarse * 100 + fine) +
                    z.sample->tuneCents;
      z.gainDb = -attenuation;
      z.pan = std::clamp(pan, -1.0f, 1.0f);
      z.loop = (mode == 1 || mode == 3) && z.sample->loopStart >= 0;
      instrument.zones.push_back(std::move(z));
    }
  }
  instrument.type = "sf2";
  instrument.path = path;
  instrument.name = presetName.empty()
                        ? std::filesystem::path(path).stem().string()
                        : presetName;
  return !instrument.zones.empty();
}

// --- WAV folders -----------------------------------------------------------

inline double samplePeak(const SampleBuffer& s) {
  double peak = 0.0;
  const size_t n = std::min<size_t>(s.data.size(), 5000);
  for (size_t i = 0; i < n; ++i) {
    peak = std::max(peak, std::fabs(static_cast<double>(s.data[i])));
  }
  return peak;
}

inline bool parseLayerTag(const std::string& name, int* velocity,
                          int* roundRobin) {
  int v = 0, rr = 0;
  for (size_t i = 0; i + 1 < name.size(); ++i) {
    if ((name[i] == 'v' || name[i] == 'V') &&
        std::isdigit(static_cast<unsigned char>(name[i + 1]))) {
      v = std::atoi(name.c_str() + i + 1);
    }
    if ((name[i] == 'r' || name[i] == 'R') &&
        std::isdigit(static_cast<unsigned char>(name[i + 1]))) {
      rr = std::atoi(name.c_str() + i + 1);
    }
  }
  if (velocity) *velocity = v;
  if (roundRobin) *roundRobin = rr;
  return v > 0 || rr > 0;
}

inline bool loadWavFolder(const std::string& dir, Instrument& instrument,
                          bool roundRobin = false) {
  std::vector<std::filesystem::path> files;
  std::error_code ec;
  for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
    if (ec) break;
    if (!e.is_regular_file(ec) || ec) continue;
    const std::string ext = lowerAscii(e.path().extension().string());
    if (ext == ".wav" || ext == ".flac") {
      files.push_back(e.path());
    }
  }
  if (files.empty()) return false;
  std::sort(files.begin(), files.end());

  struct Entry {
    std::shared_ptr<const SampleBuffer> buffer;
    double peak = 0.0;
    int velocity = 0;
    int rr = 0;
  };
  std::vector<Entry> entries;
  for (const auto& f : files) {
    auto b = loadWavBuffer(f.string());
    if (!b) continue;
    Entry e;
    e.buffer = std::move(b);
    e.peak = samplePeak(*e.buffer);
    parseLayerTag(f.stem().string(), &e.velocity, &e.rr);
    entries.push_back(std::move(e));
  }
  if (entries.empty()) return false;

  bool tagged = false;
  for (const auto& e : entries) {
    if (e.velocity > 0) {
      tagged = true;
      break;
    }
  }
  if (roundRobin) {
    for (size_t i = 0; i < entries.size(); ++i) {
      Zone z;
      z.sample = entries[i].buffer;
      z.lokey = 0;
      z.hikey = 127;
      z.lovel = 0;
      z.hivel = 127;
      z.seqLength = static_cast<int>(entries.size());
      z.seqPosition = static_cast<int>(i) + 1;
      instrument.zones.push_back(std::move(z));
    }
  } else if (tagged) {
    int maxVel = 1;
    std::map<int, std::vector<int>> rrByVel;
    for (const auto& e : entries) {
      const int v = std::max(1, e.velocity);
      maxVel = std::max(maxVel, v);
      rrByVel[v].push_back(std::max(1, e.rr));
    }
    for (auto& e : entries) {
      const int v = std::max(1, e.velocity);
      Zone z;
      z.sample = e.buffer;
      z.lokey = 0;
      z.hikey = 127;
      z.lovel = static_cast<int>((v - 1) * 128 / maxVel);
      z.hivel = static_cast<int>(v * 128 / maxVel) - 1;
      z.lovel = std::clamp(z.lovel, 0, 127);
      z.hivel = std::clamp(z.hivel, 0, 127);
      auto& rrs = rrByVel[v];
      const int maxRr = *std::max_element(rrs.begin(), rrs.end());
      z.seqLength = maxRr > 1 ? maxRr : 0;
      z.seqPosition = std::clamp(std::max(1, e.rr), 1, std::max(1, maxRr));
      instrument.zones.push_back(std::move(z));
    }
  } else {
    std::sort(entries.begin(), entries.end(),
              [](const Entry& a, const Entry& b) { return a.peak < b.peak; });
    const int n = static_cast<int>(entries.size());
    for (int i = 0; i < n; ++i) {
      Zone z;
      z.sample = entries[static_cast<size_t>(i)].buffer;
      z.lokey = 0;
      z.hikey = 127;
      z.lovel = static_cast<int>(i * 128 / n);
      z.hivel = static_cast<int>((i + 1) * 128 / n) - 1;
      z.lovel = std::clamp(z.lovel, 0, 127);
      z.hivel = std::clamp(z.hivel, 0, 127);
      instrument.zones.push_back(std::move(z));
    }
  }
  instrument.type = "wav-folder";
  instrument.path = dir;
  instrument.name = std::filesystem::path(dir).filename().string();
  return !instrument.zones.empty();
}

inline std::shared_ptr<const Instrument> loadInstrument(
    const std::string& path, bool roundRobin = false) noexcept {
  if (path.empty()) return nullptr;
  auto instrument = std::make_shared<Instrument>();
  const std::string ext = lowerAscii(
      std::filesystem::path(path).extension().string());
  bool ok = false;
  if (ext == ".sfz") {
    ok = parseSfzFile(path, *instrument);
  } else if (ext == ".sf2") {
    ok = parseSf2File(path, *instrument);
  } else if (ext == ".wav" || ext == ".flac") {
    auto b = loadWavBuffer(path);
    if (b) {
      Zone z;
      z.sample = std::move(b);
      z.lokey = 0;
      z.hikey = 127;
      z.lovel = 0;
      z.hivel = 127;
      instrument->zones.push_back(std::move(z));
      instrument->type = ext == ".flac" ? "flac" : "wav";
      instrument->path = path;
      instrument->name = std::filesystem::path(path).filename().string();
      ok = true;
    }
  } else if (std::filesystem::is_directory(path)) {
    ok = loadWavFolder(path, *instrument, roundRobin);
  }
  return ok ? instrument : nullptr;
}

inline std::shared_ptr<Instrument> sliceInstrument(
    std::shared_ptr<const SampleBuffer> sample, int hits,
    const std::string& name) {
  if (!sample || sample->data.empty() || hits < 1) return nullptr;
  const int n = static_cast<int>(sample->data.size());
  const int sr = sample->sampleRate > 0 ? sample->sampleRate : 48000;
  const int win = std::max(8, sr / 500);
  std::vector<float> env;
  for (int i = 0; i < n; i += win) {
    float peak = 0.0f;
    const int end = std::min(n, i + win);
    for (int j = i; j < end; ++j) {
      peak = std::max(peak, std::fabs(sample->data[static_cast<size_t>(j)]));
    }
    env.push_back(peak);
  }
  float maxEnv = 0.0f;
  for (float v : env) maxEnv = std::max(maxEnv, v);
  const float threshold = maxEnv * 0.28f;
  const int minGapWindows = std::max(1, (sr / 12) / win);  // ~80 ms
  std::vector<int> onsets;
  bool armed = true;
  for (size_t i = 0; i < env.size(); ++i) {
    if (env[i] < threshold) {
      if (i > 0 && env[i] < env[i - 1]) armed = true;
      continue;
    }
    if (!armed) continue;
    const bool rising = i == 0 || env[i] >= env[i - 1];
    if (!rising) continue;
    const int pos = static_cast<int>(i) * win;
    if (onsets.empty() || pos - onsets.back() >= minGapWindows * win) {
      onsets.push_back(pos);
      armed = false;
      if (static_cast<int>(onsets.size()) >= hits) break;
    }
  }
  if (static_cast<int>(onsets.size()) < hits) {
    onsets.clear();
    for (int i = 0; i < hits; ++i) {
      onsets.push_back(static_cast<int>((static_cast<int64_t>(n) * i) / hits));
    }
  }
  auto instrument = std::make_shared<Instrument>();
  instrument->name = name.empty() ? sample->name + " slices" : name;
  instrument->type = "sfz";
  for (int i = 0; i < hits; ++i) {
    const int onset = onsets[static_cast<size_t>(i)];
    const int next = i + 1 < hits ? onsets[static_cast<size_t>(i + 1)] : n;
    int start = std::max(0, onset - static_cast<int>(0.005f * sr));
    int end = std::min(n, std::max(onset + static_cast<int>(0.02f * sr),
                                   next - static_cast<int>(0.003f * sr)));
    if (end <= start + 8) end = std::min(n, start + static_cast<int>(0.05f * sr));
    if (end <= start + 8) continue;
    Zone z;
    z.sample = sample;
    z.lokey = 0;
    z.hikey = 127;
    z.lovel = 0;
    z.hivel = 127;
    z.start = static_cast<float>(start) / static_cast<float>(std::max(1, n - 1));
    z.end = static_cast<float>(end) / static_cast<float>(std::max(1, n));
    z.seqLength = hits;
    z.seqPosition = i + 1;
    instrument->zones.push_back(std::move(z));
  }
  return instrument->zones.empty() ? nullptr : instrument;
}

inline bool saveInstrumentSfz(const std::string& path,
                              const Instrument& instrument) {
  std::ofstream out(path, std::ios::trunc);
  if (!out) return false;
  out << "// Generated by Drum Suite\n";
  out << "<control> default_path=.\n\n";
  for (const Zone& z : instrument.zones) {
    if (!z.sample) continue;
    const size_t frames = z.sample->data.size();
    out << "<region> sample=\"" << z.sample->path << "\"";
    out << " lokey=" << z.lokey << " hikey=" << z.hikey;
    out << " lovel=" << z.lovel << " hivel=" << z.hivel;
    if (z.rootKey >= 0) out << " pitch_keycenter=" << z.rootKey;
    if (z.tuneCents != 0.0f) out << " tune=" << z.tuneCents;
    if (z.gainDb != 0.0f) out << " volume=" << z.gainDb;
    if (z.pan != 0.0f) out << " pan=" << (z.pan * 100.0f);
    if (z.seqLength > 1) {
      out << " seq_length=" << z.seqLength
          << " seq_position=" << z.seqPosition;
    }
    if (z.group != 0) out << " group=" << z.group;
    if (z.offBy != 0) out << " off_by=" << z.offBy;
    if (z.loop) out << " loop_mode=loop_continuous";
    if (frames > 1) {
      const long offset = std::lround(z.start * (frames - 1));
      const long end = std::lround(z.end * frames);
      if (offset > 0) out << " offset=" << offset;
      if (end < static_cast<long>(frames)) out << " end=" << end;
    }
    out << "\n";
  }
  return static_cast<bool>(out);
}

} // namespace drum
