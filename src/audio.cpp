#include "action52/audio.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>

namespace {
std::uint16_t le16(const std::uint8_t* p) { return std::uint16_t(p[0]) | std::uint16_t(p[1])<<8; }
std::uint32_t le32(const std::uint8_t* p) { return std::uint32_t(p[0]) | std::uint32_t(p[1])<<8 | std::uint32_t(p[2])<<16 | std::uint32_t(p[3])<<24; }

bool loadPcm16Wav(const std::filesystem::path& path, auto& clip, std::string& error) {
  std::ifstream file(path,std::ios::binary|std::ios::ate);
  if (!file) { error="Missing ROM-derived audio asset: "+path.string(); return false; }
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(file.tellg()));
  file.seekg(0); file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
  if (bytes.size()<44 || std::memcmp(bytes.data(),"RIFF",4) || std::memcmp(bytes.data()+8,"WAVE",4)) {
    error="Invalid WAV asset: "+path.string(); return false;
  }
  std::uint16_t format=0,channels=0,bits=0; std::uint32_t rate=0; const std::uint8_t* pcm=nullptr; std::size_t pcmSize=0;
  for (std::size_t at=12; at+8<=bytes.size();) {
    const auto size=le32(bytes.data()+at+4); const auto next=at+8+size+(size&1u);
    if (next>bytes.size()) break;
    if (!std::memcmp(bytes.data()+at,"fmt ",4) && size>=16) { format=le16(bytes.data()+at+8); channels=le16(bytes.data()+at+10); rate=le32(bytes.data()+at+12); bits=le16(bytes.data()+at+22); }
    if (!std::memcmp(bytes.data()+at,"data",4)) { pcm=bytes.data()+at+8; pcmSize=size; }
    at=next;
  }
  if (format!=1 || (channels!=1 && channels!=2) || bits!=16 || !rate || !pcm) { error="WAV must be 16-bit mono/stereo PCM: "+path.string(); return false; }
  const auto frames=pcmSize/(channels*2); clip.mono.resize(frames); clip.sampleRate=static_cast<int>(rate);
  for (std::size_t i=0;i<frames;++i) { int sum=0; for (std::size_t c=0;c<channels;++c) sum+=static_cast<std::int16_t>(le16(pcm+(i*channels+c)*2)); clip.mono[i]=float(sum)/(32768.0f*channels); }
  return true;
}
}

namespace a52 {

void NesSynth::setSampleRate(int sampleRate) { sampleRate_ = std::max(8000, sampleRate); }

bool NesSynth::loadUiAudio(const std::filesystem::path& assetDirectory, std::string& error) {
  if (!loadPcm16Wav(assetDirectory/"audio"/"intro_loop.wav",clips_[0],error)) return false;
  if (!loadPcm16Wav(assetDirectory/"audio"/"selection_voice.wav",clips_[1],error)) return false;
  clips_[0].loop=true;
  return true;
}

void NesSynth::handleUiCues(std::span<const UiAudioCue> cues) {
  for (const auto cue:cues) switch(cue) {
    case UiAudioCue::IntroTitle: clips_[0].cursor=0; break;
    case UiAudioCue::IntroStop: clips_[0].cursor=clips_[1].cursor=-1; break;
    case UiAudioCue::IntroVoice: clips_[0].cursor=-1; clips_[1].cursor=0; break;
    case UiAudioCue::MenuMove:
      uiPulses_[0]={0,0,10.0/120.0,0xf0,2,7,1,1.0/120.0,1.0/120.0,true,true}; break;
    case UiAudioCue::MenuPage:
      uiPulses_[1]={0,0,0.22,0xff,0,15,2,2.0/120.0,2.0/120.0,false,true}; break;
  }
}

void NesSynth::apply(std::span<const AudioEvent> events) {
  constexpr double cpuClock = 1789773.0;
  constexpr std::array<int,16> noisePeriods{4,8,16,32,64,96,128,160,202,254,380,508,762,1016,2034,4068};
  for (auto& voice : voices_) voice.enabled = false;
  for (const auto& event : events) {
    auto& voice = voices_[static_cast<std::size_t>(event.channel)];
    const bool triangle = event.channel == AudioChannel::Triangle;
    const bool noise = event.channel == AudioChannel::Noise;
    voice.frequency = noise ? cpuClock / noisePeriods[event.period & 15]
                            : cpuClock / ((triangle ? 32.0 : 16.0) * (event.period + 1.0));
    voice.volume = event.volume / 15.0f;
    voice.duty = event.duty & 3;
    voice.enabled = event.enabled;
  }
}

void NesSynth::render(std::span<float> output) {
  constexpr std::array<double, 4> duty = {0.125, 0.25, 0.5, 0.75};
  for (std::size_t i = 0; i + 1 < output.size(); i += 2) {
    float sample = 0.0f;
    for (auto& clip:clips_) if (clip.cursor>=0) {
      const auto index=static_cast<std::size_t>(clip.cursor);
      const bool limited=clip.maxSeconds>0 && clip.cursor>=clip.maxSeconds*clip.sampleRate;
      if (index>=clip.mono.size() || limited) {
        if (clip.loop && !clip.mono.empty()) clip.cursor=std::fmod(clip.cursor,double(clip.mono.size()));
        else clip.cursor=-1;
      }
      const auto wrappedIndex=static_cast<std::size_t>(std::max(0.0,clip.cursor));
      if (clip.cursor>=0 && wrappedIndex<clip.mono.size()) { sample+=clip.mono[wrappedIndex]*0.55f; clip.cursor+=double(clip.sampleRate)/sampleRate_; }
    }
    for (std::size_t channel = 0; channel < voices_.size(); ++channel) {
      auto& voice = voices_[channel];
      if (!voice.enabled || voice.frequency <= 0) continue;
      float wave{};
      voice.phase += voice.frequency / sampleRate_;
      if (channel == 3) {
        while (voice.phase >= 1.0) {
          voice.phase -= 1.0;
          noise_ = (noise_ >> 1) ^ ((0u - (noise_ & 1u)) & 0xB400u);
        }
        wave = (noise_ & 1) ? 1.0f : -1.0f;
      } else {
        voice.phase -= std::floor(voice.phase);
        if (channel < 2) wave = voice.phase < duty[voice.duty] ? 1.0f : -1.0f;
        else if (channel == 2) wave = static_cast<float>(1.0 - 4.0 * std::abs(voice.phase - 0.5));
      }
      sample += wave * voice.volume * 0.08f;
    }
    for (auto& pulse:uiPulses_) if (pulse.active) {
      pulse.elapsed+=1.0/sampleRate_;
      if (pulse.elapsed>=pulse.duration || pulse.timerPeriod<8) { pulse.active=false; continue; }
      if (pulse.elapsed>=pulse.nextSweep) {
        pulse.timerPeriod-=pulse.timerPeriod>>pulse.sweepShift;
        if (pulse.pulse1) --pulse.timerPeriod;
        pulse.nextSweep+=pulse.sweepInterval;
      }
      const double frequency=1789773.0/(16.0*(pulse.timerPeriod+1.0));
      pulse.phase+=frequency/sampleRate_; pulse.phase-=std::floor(pulse.phase);
      float volume=float(pulse.volume)/15.0f;
      if (pulse.pulse1) volume*=float(std::max(0.0,1.0-pulse.elapsed/pulse.duration));
      sample+=(pulse.phase<duty[pulse.duty]?1.0f:-1.0f)*volume*0.12f;
    }
    sample = std::clamp(sample, -1.0f, 1.0f);
    output[i] = output[i+1] = sample;
  }
}
}  // namespace a52
