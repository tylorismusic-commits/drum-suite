// Drum Suite — LV2 plugin
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Control ports mirror the CLAP parameter ids: port = kPortFirstParam + ParamId.

#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/midi/midi.h>
#include <lv2/state/state.h>
#include <lv2/urid/urid.h>

#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "dsp/engine.hpp"
#include "dsp/sample_io.hpp"
#include "params.hpp"

namespace {

constexpr const char* kPluginUri = "https://github.com/tylorismusic-commits/drum-suite";

enum Port : uint32_t {
  PORT_AUDIO_IN_L = 0,
  PORT_AUDIO_IN_R = 1,
  PORT_AUDIO_OUT_L = 2,
  PORT_AUDIO_OUT_R = 3,
  PORT_FIRST_PARAM = 4,
  PORT_IN_PEAK_L = PORT_FIRST_PARAM + drum::P_COUNT,
  PORT_IN_PEAK_R,
  PORT_OUT_PEAK_L,
  PORT_OUT_PEAK_R,
  PORT_IN_PEAK,
  PORT_OUT_PEAK,
  PORT_GR,
  PORT_DETECTOR,
  PORT_COMP_GR,
  PORT_LIMIT_GR,
  PORT_GATE,
  PORT_GHOST_ACTIVE,
  PORT_CLIP_ACTIVE,
  PORT_SAMPLE_VOICES,
  PORT_SAMPLE_ACTIVE,
  PORT_SAMPLE_RATE,
  PORT_LATENCY,
  PORT_MIDI_IN,
  PORT_COUNT
};

struct Plugin {
  float sampleRate = 48000.0f;
  float lastPort[drum::P_COUNT] = {};
  bool portsSeen = false;
  drum::Shared shared;
  drum::Engine engine;
  drum::Meters meters;
  std::unique_ptr<float*[]> ports;

  float* audioInL = nullptr;
  float* audioInR = nullptr;
  float* audioOutL = nullptr;
  float* audioOutR = nullptr;
  float* inPeakL = nullptr;
  float* inPeakR = nullptr;
  float* outPeakL = nullptr;
  float* outPeakR = nullptr;
  float* inPeak = nullptr;
  float* outPeak = nullptr;
  float* gr = nullptr;
  float* detector = nullptr;
  float* compGr = nullptr;
  float* limitGr = nullptr;
  float* gate = nullptr;
  float* ghostActive = nullptr;
  float* clipActive = nullptr;
  float* sampleVoices = nullptr;
  float* sampleActive = nullptr;
  float* sampleRatePort = nullptr;
  float* latency = nullptr;
  LV2_Atom_Sequence* midiIn = nullptr;
  const LV2_URID_Map* map = nullptr;
  LV2_URID midiEventType = 0;
  LV2_URID stateKey = 0;
  LV2_URID chunkType = 0;
};

LV2_Handle instantiate(const LV2_Descriptor*, double rate, const char*,
                       const LV2_Feature* const* features) {
  auto* p = new Plugin();
  p->sampleRate = static_cast<float>(rate);
  p->shared.sampleRate.store(p->sampleRate);
  for (const LV2_Feature* const* f = features; f && *f; ++f) {
    if (!std::strcmp((*f)->URI, LV2_URID__map) && (*f)->data) {
      p->map = static_cast<const LV2_URID_Map*>(static_cast<const void*>((*f)->data));
    }
  }
  if (p->map) {
    p->midiEventType = p->map->map(p->map->handle, LV2_MIDI__MidiEvent);
    p->stateKey =
        p->map->map(p->map->handle,
                    "https://github.com/tylorismusic-commits/drum-suite#state");
    p->chunkType = p->map->map(p->map->handle, LV2_ATOM__Chunk);
  }
  p->engine.prepare(p->sampleRate);
  p->engine.setSample(p->shared.sample());
  p->engine.setControls(drum::controlsFrom(p->shared), 0.0f, false);
  return p;
}

void connectPort(LV2_Handle handle, uint32_t port, void* data) {
  auto* p = static_cast<Plugin*>(handle);
  switch (port) {
    case PORT_AUDIO_IN_L: p->audioInL = static_cast<float*>(data); break;
    case PORT_AUDIO_IN_R: p->audioInR = static_cast<float*>(data); break;
    case PORT_AUDIO_OUT_L: p->audioOutL = static_cast<float*>(data); break;
    case PORT_AUDIO_OUT_R: p->audioOutR = static_cast<float*>(data); break;
    case PORT_IN_PEAK_L: p->inPeakL = static_cast<float*>(data); break;
    case PORT_IN_PEAK_R: p->inPeakR = static_cast<float*>(data); break;
    case PORT_OUT_PEAK_L: p->outPeakL = static_cast<float*>(data); break;
    case PORT_OUT_PEAK_R: p->outPeakR = static_cast<float*>(data); break;
    case PORT_IN_PEAK: p->inPeak = static_cast<float*>(data); break;
    case PORT_OUT_PEAK: p->outPeak = static_cast<float*>(data); break;
    case PORT_GR: p->gr = static_cast<float*>(data); break;
    case PORT_DETECTOR: p->detector = static_cast<float*>(data); break;
    case PORT_COMP_GR: p->compGr = static_cast<float*>(data); break;
    case PORT_LIMIT_GR: p->limitGr = static_cast<float*>(data); break;
    case PORT_GATE: p->gate = static_cast<float*>(data); break;
    case PORT_GHOST_ACTIVE: p->ghostActive = static_cast<float*>(data); break;
    case PORT_CLIP_ACTIVE: p->clipActive = static_cast<float*>(data); break;
    case PORT_SAMPLE_VOICES: p->sampleVoices = static_cast<float*>(data); break;
    case PORT_SAMPLE_ACTIVE: p->sampleActive = static_cast<float*>(data); break;
    case PORT_SAMPLE_RATE: p->sampleRatePort = static_cast<float*>(data); break;
    case PORT_LATENCY: p->latency = static_cast<float*>(data); break;
    case PORT_MIDI_IN: p->midiIn = static_cast<LV2_Atom_Sequence*>(data); break;
    default:
      if (!p->ports) {
        p->ports = std::make_unique<float*[]>(PORT_COUNT);
        std::memset(p->ports.get(), 0, sizeof(float*) * PORT_COUNT);
      }
      if (port < PORT_COUNT) p->ports[port] = static_cast<float*>(data);
      break;
  }
}

void activate(LV2_Handle handle) {
  auto* p = static_cast<Plugin*>(handle);
  p->engine.reset();
  if (p->latency) *p->latency = static_cast<float>(p->engine.latencySamples());
}

void deactivate(LV2_Handle) {}

void run(LV2_Handle handle, uint32_t frames) {
  auto* p = static_cast<Plugin*>(handle);
  if (!p->audioInL || !p->audioOutL) return;
  if (!p->audioInR) p->audioInR = p->audioInL;
  if (!p->audioOutR) p->audioOutR = p->audioOutL;

  if (p->ports) {
    for (uint32_t i = 0; i < drum::P_COUNT; ++i) {
      float* v = p->ports[PORT_FIRST_PARAM + i];
      if (!v) continue;
      if (!p->portsSeen || *v != p->lastPort[i]) {
        p->shared.setFromHost(i, static_cast<double>(*v));
        p->lastPort[i] = *v;
      }
    }
    p->portsSeen = true;
  }

  // MIDI: note-on on the configured ghost note sets the ghost amount from
  // velocity and triggers a ghost window; the configured CC sets the amount.
  if (p->midiIn && p->midiEventType) {
    LV2_ATOM_SEQUENCE_FOREACH(p->midiIn, ev) {
      if (ev->body.type != p->midiEventType || ev->body.size < 3) continue;
      const auto* data = static_cast<const uint8_t*>(LV2_ATOM_BODY(&ev->body));
      const int status = data[0] & 0xf0;
      if (status == 0x90 && data[2] > 0) {
        p->shared.midiSampleTrigger.fetch_add(1, std::memory_order_relaxed);
        const int ghostNote = static_cast<int>(p->shared.get(drum::P_GHOST_NOTE) + 0.5);
        if (data[1] == ghostNote) {
          p->shared.set(drum::P_GHOST, (data[2] / 127.0) * 100.0);
          p->shared.midiGhostTrigger.fetch_add(1, std::memory_order_relaxed);
        }
      } else if (status == 0xb0) {
        const int ghostCc = static_cast<int>(p->shared.get(drum::P_GHOST_CC) + 0.5);
        if (data[1] == ghostCc) {
          p->shared.set(drum::P_GHOST, (data[2] / 127.0) * 100.0);
        }
      }
    }
  }

  p->engine.setSample(p->shared.sample());
  p->engine.setControls(drum::controlsFrom(p->shared), 0.0f, false);

  if (p->audioOutL != p->audioInL) {
    std::memcpy(p->audioOutL, p->audioInL, frames * sizeof(float));
  }
  if (p->audioOutR != p->audioInR) {
    std::memcpy(p->audioOutR, p->audioInR, frames * sizeof(float));
  }

  p->engine.process(p->audioOutL, p->audioOutR, static_cast<int>(frames),
                    p->meters);

  if (p->inPeakL) *p->inPeakL = p->meters.inPeakLDb;
  if (p->inPeakR) *p->inPeakR = p->meters.inPeakRDb;
  if (p->outPeakL) *p->outPeakL = p->meters.outPeakLDb;
  if (p->outPeakR) *p->outPeakR = p->meters.outPeakRDb;
  if (p->inPeak) *p->inPeak = p->meters.inPeakDb;
  if (p->outPeak) *p->outPeak = p->meters.outPeakDb;
  if (p->gr) *p->gr = p->meters.grDb;
  if (p->detector) *p->detector = p->meters.detectorDb;
  if (p->compGr) *p->compGr = p->meters.compGrDb;
  if (p->limitGr) *p->limitGr = p->meters.limitGrDb;
  p->shared.limitGrDb.store(p->meters.limitGrDb, std::memory_order_relaxed);
  {
    const int lh = p->shared.limitHistoryHead.load(std::memory_order_relaxed);
    p->shared.limitHistory[lh & (drum::Shared::kScopeSize - 1)].store(
        p->meters.limitGrDb, std::memory_order_relaxed);
    p->shared.limitHistoryHead.store(
        (lh + 1) & (drum::Shared::kScopeSize - 1), std::memory_order_relaxed);
  }
  if (p->gate) *p->gate = p->meters.gate;
  if (p->ghostActive) *p->ghostActive = p->meters.ghostActive ? 1.0f : 0.0f;
  if (p->clipActive) *p->clipActive = p->meters.clipActive ? 1.0f : 0.0f;
  p->shared.clipActive.store(p->meters.clipActive, std::memory_order_relaxed);
  p->shared.sampleVoices.store(p->meters.sampleVoices,
                               std::memory_order_relaxed);
  p->shared.sampleActive.store(
      p->meters.sampleVoices > 0 || p->meters.sampleTriggered,
      std::memory_order_relaxed);
  if (p->sampleVoices) *p->sampleVoices = static_cast<float>(p->meters.sampleVoices);
  if (p->sampleActive)
    *p->sampleActive =
        (p->meters.sampleVoices > 0 || p->meters.sampleTriggered) ? 1.0f : 0.0f;
  if (p->sampleRatePort) *p->sampleRatePort = p->sampleRate;
  if (p->latency) *p->latency = static_cast<float>(p->engine.latencySamples());

  const int head = p->shared.scopeHead.load(std::memory_order_relaxed) &
                   (drum::Shared::kScopeSize - 1);
  p->shared.scopeIn[head].store(p->meters.inPeakDb, std::memory_order_relaxed);
  p->shared.scopeGate[head].store(p->meters.gate, std::memory_order_relaxed);
  p->shared.compGrDb.store(p->meters.compGrDb, std::memory_order_relaxed);
  p->shared.transientDb.store(p->meters.transientDb, std::memory_order_relaxed);
  {
    const int ch = p->shared.compHistoryHead.load(std::memory_order_relaxed);
    p->shared.compHistory[ch & (drum::Shared::kScopeSize - 1)].store(
        p->meters.compGrDb, std::memory_order_relaxed);
    p->shared.compHistoryHead.store((ch + 1) & (drum::Shared::kScopeSize - 1),
                                    std::memory_order_relaxed);
  }
  p->shared.scopeHead.store((head + 1) & (drum::Shared::kScopeSize - 1),
                            std::memory_order_relaxed);
}

constexpr uint32_t kStateMagic = 0x44524D32;  // "DRM2"

LV2_State_Status stateSave(LV2_Handle handle,
                           LV2_State_Store_Function store,
                           LV2_State_Handle state, uint32_t,
                           const LV2_Feature* const*) {
  auto* p = static_cast<Plugin*>(handle);
  if (!p->chunkType) return LV2_STATE_ERR_NO_FEATURE;
  const auto sample = p->shared.sample();
  const std::string path = sample ? sample->path : std::string();
  const uint32_t pathSize = static_cast<uint32_t>(path.size());
  std::vector<uint8_t> blob(sizeof(uint32_t) * 2 +
                            sizeof(double) * drum::P_COUNT +
                            sizeof(uint32_t) + path.size());
  size_t off = 0;
  auto put = [&](const void* data, size_t size) {
    std::memcpy(blob.data() + off, data, size);
    off += size;
  };
  put(&kStateMagic, sizeof(kStateMagic));
  const uint32_t count = drum::P_COUNT;
  put(&count, sizeof(count));
  double values[drum::P_COUNT];
  for (uint32_t i = 0; i < drum::P_COUNT; ++i) values[i] = p->shared.get(i);
  put(values, sizeof(values));
  put(&pathSize, sizeof(pathSize));
  if (pathSize > 0) put(path.data(), path.size());
  store(state, p->stateKey, blob.data(), blob.size(), p->chunkType,
        LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
  return LV2_STATE_SUCCESS;
}

LV2_State_Status stateRestore(LV2_Handle handle,
                              LV2_State_Retrieve_Function retrieve,
                              LV2_State_Handle state, uint32_t,
                              const LV2_Feature* const*) {
  auto* p = static_cast<Plugin*>(handle);
  uint32_t type = 0, flags = 0;
  size_t size = 0;
  const void* data = retrieve(state, p->stateKey, &size, &type, &flags);
  if (!data) return LV2_STATE_SUCCESS;
  if (type != p->chunkType ||
      size < sizeof(uint32_t) * 2 + sizeof(uint32_t)) {
    return LV2_STATE_ERR_BAD_TYPE;
  }
  const uint8_t* bytes = static_cast<const uint8_t*>(data);
  size_t off = 0;
  uint32_t magic = 0;
  std::memcpy(&magic, bytes, sizeof(magic));
  off += sizeof(magic);
  if (magic != kStateMagic) return LV2_STATE_ERR_BAD_TYPE;
  uint32_t count = 0;
  std::memcpy(&count, bytes + off, sizeof(count));
  off += sizeof(count);
  if (count > 1024 ||
      size < sizeof(uint32_t) * 2 + sizeof(double) * count + sizeof(uint32_t)) {
    return LV2_STATE_ERR_BAD_TYPE;
  }
  const uint32_t limit = std::min(count, static_cast<uint32_t>(drum::P_COUNT));
  for (uint32_t i = 0; i < count; ++i) {
    double v = 0.0;
    std::memcpy(&v, bytes + off, sizeof(v));
    off += sizeof(v);
    if (i < limit) p->shared.setFromHost(i, v);
  }
  uint32_t pathSize = 0;
  std::memcpy(&pathSize, bytes + off, sizeof(pathSize));
  off += sizeof(pathSize);
  if (pathSize == 0 || pathSize > 4096 || off + pathSize > size) {
    p->shared.setSample(nullptr);
    return LV2_STATE_SUCCESS;
  }
  const std::string path(reinterpret_cast<const char*>(bytes + off), pathSize);
  auto sample = drum::loadSampleFile(path);
  if (!sample) p->shared.setFromHost(drum::P_SAMPLE_ON, 0.0);
  p->shared.setSample(std::move(sample));
  return LV2_STATE_SUCCESS;
}

const LV2_State_Interface kStateInterface = {stateSave, stateRestore};

const void* extensionData(const char* uri) {
  if (uri && !std::strcmp(uri, LV2_STATE__interface)) {
    return &kStateInterface;
  }
  return nullptr;
}

void cleanup(LV2_Handle handle) { delete static_cast<Plugin*>(handle); }

const LV2_Descriptor kDescriptor = {
    kPluginUri, instantiate, connectPort, activate, run, deactivate,
    cleanup, extensionData};

} // namespace

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index) {
  return index == 0 ? &kDescriptor : nullptr;
}
