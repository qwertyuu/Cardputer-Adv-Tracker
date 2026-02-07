// ============================================================
// CardputerTracker - Acid Synth Tracker
// For M5Stack Cardputer-Adv (ESP32-S3)
// ============================================================
// Real-time wavetable synthesis with resonant filters,
// ADSR envelopes, 808-style drums.
// Inspired by miniacid & AcidBox.
// ============================================================

#include "M5Cardputer.h"
#include <math.h>

// ============================================================
// AUDIO ENGINE CONFIG
// ============================================================
#define SAMPLE_RATE     22050
#define AUDIO_BUF_LEN   256
#define NUM_SYNTHS      3
#define NUM_DRUMS       4
#define NUM_STEPS       16
#define SCREEN_W        240
#define SCREEN_H        135
#define TWO_PI_F        6.283185307f
#define INV_SAMPLE_RATE (1.0f / (float)SAMPLE_RATE)

// Note definitions
#define NOTE_EMPTY      0
#define NOTE_C          1
#define NOTE_CS         2
#define NOTE_D          3
#define NOTE_DS         4
#define NOTE_E          5
#define NOTE_F          6
#define NOTE_FS         7
#define NOTE_G          8
#define NOTE_GS         9
#define NOTE_A          10
#define NOTE_AS         11
#define NOTE_B          12

// Waveform types
enum Wave : uint8_t { W_SAW=0, W_SQUARE, W_TRI, W_SINE, W_COUNT };
const char* waveNames[] = {"SAW","SQR","TRI","SIN"};

// Drum types
enum DrumType : uint8_t { D_KICK=0, D_SNARE, D_HIHAT, D_CLAP };
const char* drumNames[] = {"KCK","SNR","HH ","CLP"};

// UI Pages
enum Page : uint8_t { PAGE_PATTERN=0, PAGE_SOUND, PAGE_HELP, PAGE_COUNT };
const char* pageNames[] = {"PATTERN","SOUND","HELP"};

// ============================================================
// NOTE FREQUENCY TABLE
// ============================================================
// Base frequencies for octave 0 (C0=16.35 Hz)
const float baseFreqs[13] = {
  0.0f, 16.35f, 17.32f, 18.35f, 19.45f, 20.60f, 21.83f,
  23.12f, 24.50f, 25.96f, 27.50f, 29.14f, 30.87f
};
const char* noteNames[] = {
  "---","C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"
};

float noteToFreq(uint8_t note, uint8_t octave) {
  if (note == NOTE_EMPTY || note > 12) return 0.0f;
  return baseFreqs[note] * (float)(1 << octave);
}

// ============================================================
// SYNTH VOICE
// ============================================================
struct SynthVoice {
  // Oscillator
  float phase;
  float freq;           // current frequency in Hz
  float targetFreq;     // target frequency for glide
  Wave  waveform;

  // State Variable Filter
  float fltLP, fltBP;
  float fltCutoff;    // 0..1
  float fltReso;      // 0..1
  float fltEnvAmt;    // 0..1

  // Amp Envelope (exponential decay)
  float ampEnv;
  float ampDecRate;   // per-sample multiplier (0.9990 = fast, 0.9999 = slow)

  // Filter Envelope
  float filtEnv;
  float filtDecRate;

  // State
  float volume;
  bool  accent;
  bool  active;
  bool  slideActive;

  void init() {
    phase = 0; freq = 0; targetFreq = 0;
    waveform = W_SAW;
    fltLP = 0; fltBP = 0;
    fltCutoff = 0.4f; fltReso = 0.3f; fltEnvAmt = 0.5f;
    ampEnv = 0; ampDecRate = 0.99985f;
    filtEnv = 0; filtDecRate = 0.9996f;
    volume = 0.8f; accent = false; active = false; slideActive = false;
  }

  void noteOn(float newFreq, bool acc, bool slide) {
    targetFreq = newFreq;
    accent = acc;

    if (slide && active) {
      // Slide: keep playing, glide to new freq
      slideActive = true;
      // Re-trigger filter envelope for squelch
      filtEnv = 1.0f;
      // Boost amp envelope back up
      if (ampEnv < 0.6f) ampEnv = 0.6f;
    } else {
      // Hard note-on
      freq = newFreq;
      slideActive = false;
      ampEnv = 1.0f;
      filtEnv = 1.0f;
      // Reset filter state for clean attack
      fltLP = 0;
      fltBP = 0;
    }
    active = true;
  }

  void noteOff() {
    // Let decay handle it
  }

  inline float oscillator() {
    switch (waveform) {
      case W_SAW:    return 2.0f * phase - 1.0f;
      case W_SQUARE: return (phase < 0.5f) ? 0.7f : -0.7f;
      case W_TRI:    return (phase < 0.5f) ? (4.0f*phase - 1.0f) : (3.0f - 4.0f*phase);
      case W_SINE:   return sinf(phase * TWO_PI_F);
      default:       return 0.0f;
    }
  }

  inline float render() {
    if (!active) return 0.0f;

    // Glide (exponential, ~50ms to reach target)
    if (slideActive) {
      freq += (targetFreq - freq) * 0.005f;
    } else {
      freq = targetFreq;
    }

    // Advance oscillator (using freq directly)
    phase += freq * INV_SAMPLE_RATE;
    if (phase >= 1.0f) phase -= 1.0f;

    float osc = oscillator();

    // Decay envelopes
    ampEnv *= ampDecRate;
    filtEnv *= filtDecRate;

    if (ampEnv < 0.002f) {
      ampEnv = 0; active = false;
      return 0.0f;
    }

    // SVF filter
    float cutMod = fltCutoff + fltEnvAmt * filtEnv;
    if (accent) cutMod += 0.12f;
    if (cutMod > 0.8f) cutMod = 0.8f;
    if (cutMod < 0.05f) cutMod = 0.05f;

    // Compute filter coefficient
    float fc = cutMod * 0.45f;
    float f = 2.0f * sinf(3.14159f * fc);
    if (f > 0.85f) f = 0.85f;

    float q = 1.0f - fltReso * 0.85f;
    if (q < 0.1f) q = 0.1f;

    // SVF update
    fltLP += f * fltBP;
    fltBP += f * (osc - fltLP - q * fltBP);

    // Hard clamp to prevent blowup
    if (fltLP > 3.0f) fltLP = 3.0f;
    else if (fltLP < -3.0f) fltLP = -3.0f;
    if (fltBP > 3.0f) fltBP = 3.0f;
    else if (fltBP < -3.0f) fltBP = -3.0f;

    float out = fltLP * ampEnv * volume;
    if (accent) out *= 1.25f;
    return out;
  }
};

// ============================================================
// DRUM VOICE (808-style synthesis)
// ============================================================
struct DrumVoice {
  float phase;
  float freq;
  float pitchEnv;
  float ampEnv;
  float decayMul;      // multiplicative decay (0.999 = long, 0.99 = short)
  float pitchDecMul;
  float noiseAmt;
  float toneMix;
  bool  active;
  uint32_t noiseSeed;

  void init() {
    phase = 0; freq = 60; pitchEnv = 0; ampEnv = 0;
    decayMul = 0.9995f; pitchDecMul = 0.999f;
    noiseAmt = 0; toneMix = 1.0f;
    active = false; noiseSeed = 12345;
  }

  inline float noise() {
    noiseSeed = noiseSeed * 1664525 + 1013904223;
    return ((float)(noiseSeed >> 16) / 32768.0f) - 1.0f;
  }

  void trigger(DrumType type) {
    phase = 0;
    ampEnv = 1.0f;
    active = true;
    noiseSeed = (uint32_t)(micros() * 7 + (int)type * 13);

    switch (type) {
      case D_KICK:
        freq = 50.0f; pitchEnv = 180.0f;
        decayMul = 0.9996f; pitchDecMul = 0.998f;
        noiseAmt = 0.05f; toneMix = 1.0f;
        break;
      case D_SNARE:
        freq = 170.0f; pitchEnv = 50.0f;
        decayMul = 0.9992f; pitchDecMul = 0.996f;
        noiseAmt = 0.7f; toneMix = 0.4f;
        break;
      case D_HIHAT:
        freq = 800.0f; pitchEnv = 0.0f;
        decayMul = 0.9985f; pitchDecMul = 1.0f;
        noiseAmt = 1.0f; toneMix = 0.0f;
        break;
      case D_CLAP:
        freq = 1000.0f; pitchEnv = 0.0f;
        decayMul = 0.9988f; pitchDecMul = 1.0f;
        noiseAmt = 0.85f; toneMix = 0.15f;
        break;
    }
  }

  inline float render() {
    if (!active) return 0.0f;

    float currentFreq = freq + pitchEnv;
    pitchEnv *= pitchDecMul;

    // Sine oscillator for tone
    float tone = sinf(phase * TWO_PI_F) * toneMix;
    phase += currentFreq * INV_SAMPLE_RATE;
    if (phase >= 1.0f) phase -= 1.0f;

    float n = noise() * noiseAmt;
    float out = (tone + n) * ampEnv;

    ampEnv *= decayMul;
    if (ampEnv < 0.002f) { ampEnv = 0; active = false; }

    return out;
  }
};

// ============================================================
// PATTERN DATA
// ============================================================
struct SynthCell {
  uint8_t note;    // 0=empty, 1-12
  uint8_t octave;  // 2-7
  bool    accent;
  bool    slide;   // glide to next note
};

struct DrumCell {
  bool kick;
  bool snare;
  bool hihat;
  bool clap;
};

#define NUM_PATTERNS 4

struct Pattern {
  SynthCell synth[NUM_SYNTHS][NUM_STEPS];
  DrumCell  drums[NUM_STEPS];
};

// ============================================================
// GLOBALS
// ============================================================
// Audio engine
SynthVoice synths[NUM_SYNTHS];
DrumVoice  drumVoices[NUM_DRUMS];
int16_t    audioBufA[AUDIO_BUF_LEN];
int16_t    audioBufB[AUDIO_BUF_LEN];
volatile bool audioReady = false;
float scopeBuf[SCREEN_W];
int scopeIdx = 0;

// Sequencer
Pattern patterns[NUM_PATTERNS];
uint8_t curPattern = 0;
uint8_t curTrack = 0;    // 0-2 = synth, 3 = drums
uint8_t curStep = 0;
uint8_t curOctave = 4;
uint8_t curDrumLane = 0; // 0-3 within drum track
uint16_t bpm = 128;
bool playing = false;
uint8_t playStep = 0;
unsigned long lastStepTime = 0;
bool needRedraw = true;
bool fullRedraw = true;
Page curPage = PAGE_PATTERN;
bool synthMute[NUM_SYNTHS] = {false,false,false};
bool drumMute = false;

// Display sprite
static M5Canvas canvas(&M5Cardputer.Display);

// Track colors
uint16_t synthColors[NUM_SYNTHS] = { 0x07FF, 0x07E0, 0xFFE0 }; // Cyan, Green, Yellow
uint16_t drumColor = 0xF81F; // Magenta
uint16_t trackColor(int t) { return (t < NUM_SYNTHS) ? synthColors[t] : drumColor; }

// ============================================================
// AUDIO TASK (runs on Core 0)
// ============================================================
TaskHandle_t audioTaskHandle = NULL;

void audioTask(void* param) {
  int16_t* buffers[2] = { audioBufA, audioBufB };
  int curBuf = 0;

  while (true) {
    int16_t* buf = buffers[curBuf];

    // Generate one buffer of audio
    for (int i = 0; i < AUDIO_BUF_LEN; i++) {
      float mix = 0.0f;

      // Synth voices
      for (int s = 0; s < NUM_SYNTHS; s++) {
        if (!synthMute[s]) mix += synths[s].render();
      }

      // Drum voices
      if (!drumMute) {
        for (int d = 0; d < NUM_DRUMS; d++) {
          mix += drumVoices[d].render() * 0.6f;
        }
      }

      // Soft clip
      if (mix > 1.0f) mix = 1.0f;
      else if (mix < -1.0f) mix = -1.0f;

      // 16-bit output
      buf[i] = (int16_t)(mix * 12000.0f);

      // Scope (heavy downsample)
      if ((i & 7) == 0 && scopeIdx < SCREEN_W) {
        scopeBuf[scopeIdx++] = mix;
      }
    }

    // Push to speaker - retry until accepted
    while (!M5Cardputer.Speaker.playRaw(buf, AUDIO_BUF_LEN, SAMPLE_RATE, false, 1, 0)) {
      vTaskDelay(1);
    }

    curBuf ^= 1;  // swap buffer
  }
}

// ============================================================
// SEQUENCER ENGINE
// ============================================================
#define pat() patterns[curPattern]

void triggerStep(uint8_t step) {
  // Synth tracks
  for (int s = 0; s < NUM_SYNTHS; s++) {
    SynthCell& cell = pat().synth[s][step];
    if (cell.note != NOTE_EMPTY) {
      float freq = noteToFreq(cell.note, cell.octave);
      if (freq > 10.0f) {
        // Check if previous step had slide set
        uint8_t prevStep = (step == 0) ? NUM_STEPS - 1 : step - 1;
        bool doSlide = pat().synth[s][prevStep].slide && synths[s].active;
        synths[s].noteOn(freq, cell.accent, doSlide);
      }
    } else {
      // Empty cell: let current note decay naturally
      synths[s].noteOff();
    }
  }

  // Drum track
  DrumCell& dc = pat().drums[step];
  if (dc.kick)  drumVoices[D_KICK].trigger(D_KICK);
  if (dc.snare) drumVoices[D_SNARE].trigger(D_SNARE);
  if (dc.hihat) drumVoices[D_HIHAT].trigger(D_HIHAT);
  if (dc.clap)  drumVoices[D_CLAP].trigger(D_CLAP);
}

// ============================================================
// DISPLAY - PATTERN PAGE
// ============================================================
void drawPatternPage() {
  canvas.fillSprite(TFT_BLACK);

  int totalTracks = NUM_SYNTHS + 1; // 3 synth + 1 drum
  int colW = SCREEN_W / totalTracks; // 60px each
  int headerH = 13;
  int trackHeaderH = 9;
  int gridTop = headerH + trackHeaderH;
  int rowH = 7;

  // === Top header bar ===
  // BPM
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE);
  canvas.setCursor(1, 2);
  canvas.printf("BPM:%3d", bpm);

  // Play state
  canvas.setCursor(50, 2);
  if (playing) {
    canvas.setTextColor(0x07E0); // Green
    canvas.print(">>"); // Play symbol
  } else {
    canvas.setTextColor(0xF800); // Red
    canvas.print("||");
  }

  // Pattern
  canvas.setCursor(68, 2);
  canvas.setTextColor(0xFBE0); // Orange-ish
  canvas.printf("P:%c", 'A' + curPattern);

  // Octave
  canvas.setCursor(92, 2);
  canvas.setTextColor(TFT_WHITE);
  canvas.printf("O:%d", curOctave);

  // Page indicator
  canvas.setCursor(112, 2);
  canvas.setTextColor(0x7BEF); // Gray
  canvas.print("[PAT]");

  // Mute indicators
  for (int t = 0; t < NUM_SYNTHS; t++) {
    canvas.setCursor(175 + t*12, 2);
    canvas.setTextColor(synthMute[t] ? 0x3186 : synthColors[t]);
    canvas.printf("%d", t+1);
  }
  canvas.setCursor(175 + NUM_SYNTHS*12, 2);
  canvas.setTextColor(drumMute ? 0x3186 : drumColor);
  canvas.print("D");

  // Thin separator
  canvas.drawFastHLine(0, headerH-1, SCREEN_W, 0x2104);

  // === Track headers ===
  for (int t = 0; t < totalTracks; t++) {
    int x = t * colW;
    uint16_t col = trackColor(t);
    // Gradient-ish header
    canvas.fillRect(x, headerH, colW - 1, trackHeaderH, col);
    canvas.setTextColor(TFT_BLACK);
    canvas.setCursor(x + 2, headerH + 1);
    if (t < NUM_SYNTHS) {
      canvas.printf("%s%d", waveNames[synths[t].waveform], t+1);
    } else {
      canvas.print("DRUM");
    }
    // Selection indicator
    if (t == curTrack) {
      canvas.drawRect(x, headerH, colW - 1, trackHeaderH, TFT_WHITE);
    }
  }

  // === Grid ===
  for (int s = 0; s < NUM_STEPS; s++) {
    int y = gridTop + s * rowH;

    // Beat highlight (every 4 steps)
    bool isBeat = (s % 4 == 0);

    for (int t = 0; t < totalTracks; t++) {
      int x = t * colW;

      // Background
      uint16_t bg = TFT_BLACK;
      if (isBeat) bg = 0x0841; // subtle beat marker
      if (playing && s == playStep) bg = 0x1082; // play position
      if (t == curTrack && s == curStep) bg = 0x2945; // cursor

      canvas.fillRect(x, y, colW - 1, rowH - 1, bg);

      // Step number column (left edge of each cell)
      if (t == 0) {
        canvas.setTextColor(isBeat ? 0x6B4D : 0x3186);
        canvas.setCursor(x + 1, y);
        canvas.printf("%X", s);
      }

      if (t < NUM_SYNTHS) {
        // Synth cell
        SynthCell& cell = pat().synth[t][s];
        if (cell.note != NOTE_EMPTY) {
          uint16_t nc = (playing && s == playStep) ? TFT_WHITE : synthColors[t];
          if (synthMute[t]) nc = 0x3186;
          canvas.setTextColor(nc);
          canvas.setCursor(x + 8, y);
          canvas.printf("%s%d", noteNames[cell.note], cell.octave);
          // Accent/slide indicators
          if (cell.accent) {
            canvas.setTextColor(0xFD20); // Orange
            canvas.setCursor(x + colW - 12, y);
            canvas.print("!");
          }
          if (cell.slide) {
            canvas.setTextColor(0x07FF); // Cyan
            canvas.setCursor(x + colW - 6, y);
            canvas.print("~");
          }
        } else {
          canvas.setTextColor(0x18C3);
          canvas.setCursor(x + 8, y);
          canvas.print("---");
        }
      } else {
        // Drum cell
        DrumCell& dc = pat().drums[s];
        int dx = x + 3;
        // Show K S H C indicators
        canvas.setTextColor(dc.kick  ? 0xF800 : 0x18C3); canvas.setCursor(dx, y);      canvas.print("K");
        canvas.setTextColor(dc.snare ? 0xFFE0 : 0x18C3); canvas.setCursor(dx+11, y);   canvas.print("S");
        canvas.setTextColor(dc.hihat ? 0x07E0 : 0x18C3); canvas.setCursor(dx+22, y);   canvas.print("H");
        canvas.setTextColor(dc.clap  ? 0xF81F : 0x18C3); canvas.setCursor(dx+33, y);   canvas.print("C");

        // Cursor within drum lane
        if (t == curTrack && s == curStep) {
          int laneX = dx + curDrumLane * 11 - 1;
          canvas.drawRect(laneX, y, 10, rowH - 1, TFT_WHITE);
        }
      }
    }
  }

  // Push sprite to display
  canvas.pushSprite(0, 0);
}

// ============================================================
// DISPLAY - SOUND PAGE
// ============================================================
void drawSoundPage() {
  canvas.fillSprite(TFT_BLACK);

  int headerH = 13;
  // Header
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE);
  canvas.setCursor(1, 2);
  canvas.printf("BPM:%3d P:%c", bpm, 'A' + curPattern);
  canvas.setCursor(112, 2);
  canvas.setTextColor(0x7BEF);
  canvas.print("[SND]");
  canvas.drawFastHLine(0, headerH-1, SCREEN_W, 0x2104);

  // Show synth params for current track (if synth)
  if (curTrack < NUM_SYNTHS) {
    SynthVoice& v = synths[curTrack];
    uint16_t col = synthColors[curTrack];
    int y = headerH + 2;
    int barW = 80;
    int barH = 6;
    int labelX = 4;
    int barX = 55;

    canvas.setTextColor(col);
    canvas.setCursor(labelX, y);
    canvas.printf("=== SYNTH %d ===", curTrack + 1);
    y += 12;

    // Waveform
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(labelX, y);
    canvas.printf("Wave: %s", waveNames[v.waveform]);
    y += 10;

    // Cutoff bar
    canvas.setCursor(labelX, y);
    canvas.print("Cut:");
    canvas.drawRect(barX, y, barW, barH, 0x4208);
    canvas.fillRect(barX+1, y+1, (int)((barW-2)*v.fltCutoff), barH-2, col);
    y += 10;

    // Resonance bar
    canvas.setCursor(labelX, y);
    canvas.print("Res:");
    canvas.drawRect(barX, y, barW, barH, 0x4208);
    canvas.fillRect(barX+1, y+1, (int)((barW-2)*v.fltReso), barH-2, col);
    y += 10;

    // Env Amount bar
    canvas.setCursor(labelX, y);
    canvas.print("Env:");
    canvas.drawRect(barX, y, barW, barH, 0x4208);
    canvas.fillRect(barX+1, y+1, (int)((barW-2)*v.fltEnvAmt), barH-2, 0xFBE0);
    y += 10;

    // Decay bar (0.999=short to 0.99999=long, map to 0..1)
    canvas.setCursor(labelX, y);
    canvas.print("Dec:");
    float decDisplay = (v.ampDecRate - 0.999f) / (0.99999f - 0.999f);
    decDisplay = constrain(decDisplay, 0.0f, 1.0f);
    canvas.drawRect(barX, y, barW, barH, 0x4208);
    canvas.fillRect(barX+1, y+1, (int)((barW-2)*decDisplay), barH-2, 0xFD20);
    y += 10;

    // Volume
    canvas.setCursor(labelX, y);
    canvas.print("Vol:");
    canvas.drawRect(barX, y, barW, barH, 0x4208);
    canvas.fillRect(barX+1, y+1, (int)((barW-2)*v.volume), barH-2, 0x07E0);
    y += 10;

    // Synth status
    canvas.setCursor(labelX, y);
    canvas.printf("Active: %s  Slide: %s", v.active ? "Y" : "N", v.slideActive ? "Y" : "N");

    // === Right side: mini scope ===
    int scopeX = 145;
    int scopeY = headerH + 5;
    int scopeW = 90;
    int scopeH = 50;
    canvas.drawRect(scopeX, scopeY, scopeW, scopeH, 0x2104);

    // Draw waveform from scope buffer
    scopeIdx = 0; // reset for next frame
    int midY = scopeY + scopeH / 2;
    canvas.drawFastHLine(scopeX, midY, scopeW, 0x1082); // center line
    for (int i = 1; i < scopeW && i < SCREEN_W; i++) {
      int y1 = midY - (int)(scopeBuf[i-1] * (scopeH/2 - 2));
      int y2 = midY - (int)(scopeBuf[i] * (scopeH/2 - 2));
      y1 = constrain(y1, scopeY+1, scopeY+scopeH-2);
      y2 = constrain(y2, scopeY+1, scopeY+scopeH-2);
      canvas.drawLine(scopeX+i-1, y1, scopeX+i, y2, col);
    }

    // VU meters at bottom right
    int vuY = scopeY + scopeH + 5;
    for (int s = 0; s < NUM_SYNTHS; s++) {
      float level = synths[s].ampEnv * synths[s].volume;
      int barLen = (int)(level * 30);
      canvas.fillRect(scopeX, vuY + s*8, barLen, 5, synthColors[s]);
      canvas.drawRect(scopeX, vuY + s*8, 30, 5, 0x2104);
    }
    // Drum VU
    float drumLevel = 0;
    for (int d = 0; d < NUM_DRUMS; d++) drumLevel += drumVoices[d].ampEnv * 0.25f;
    int drumBar = (int)(drumLevel * 30);
    canvas.fillRect(scopeX, vuY + NUM_SYNTHS*8, drumBar, 5, drumColor);
    canvas.drawRect(scopeX, vuY + NUM_SYNTHS*8, 30, 5, 0x2104);

  } else {
    // Drum track selected - show drum info
    int y = headerH + 4;
    canvas.setTextColor(drumColor);
    canvas.setCursor(4, y);
    canvas.print("=== DRUMS ===");
    y += 14;

    const char* names[] = {"Kick","Snare","HiHat","Clap"};
    uint16_t cols[] = {0xF800, 0xFFE0, 0x07E0, 0xF81F};
    for (int d = 0; d < NUM_DRUMS; d++) {
      canvas.setTextColor(cols[d]);
      canvas.setCursor(10, y);
      canvas.printf("%s", names[d]);

      // Activity bar
      float level = drumVoices[d].ampEnv;
      int barLen = (int)(level * 60);
      canvas.fillRect(60, y, barLen, 6, cols[d]);
      canvas.drawRect(60, y, 60, 6, 0x2104);
      y += 12;
    }

  }

  canvas.pushSprite(0, 0);
}

// ============================================================
// DISPLAY - HELP PAGE
// ============================================================
void drawHelpPage() {
  canvas.fillSprite(TFT_BLACK);

  int y = 2;
  canvas.setTextSize(1);
  canvas.setTextColor(0x07FF);
  canvas.setCursor(2, y); canvas.print("CARDPUTER TRACKER");
  y += 12;
  canvas.drawFastHLine(0, y-2, SCREEN_W, 0x2104);

  canvas.setTextColor(0xFFE0);
  canvas.setCursor(2, y); canvas.print("NOTES: Z X C V B N M = C..B");
  y += 9;
  canvas.setTextColor(TFT_WHITE);
  canvas.setCursor(2, y); canvas.print("SHARP: H J K L /  = C# D# F# G# A#");
  y += 9;
  canvas.setCursor(2, y); canvas.print("NAV:   W/S=up/dn A/D=left/right");
  y += 9;
  canvas.setCursor(2, y); canvas.print("       1-4=track [/]=octave Q/E=bpm");
  y += 9;
  canvas.setTextColor(0x07E0);
  canvas.setCursor(2, y); canvas.print("PLAY:  SPACE=play/stop  P=demo");
  y += 9;
  canvas.setTextColor(0xFBE0);
  canvas.setCursor(2, y); canvas.print("SOUND: F/G=cut  R/T=res  Y/U=env");
  y += 9;
  canvas.setCursor(2, y); canvas.print("       I/O=decay  []=oct  9=wave");
  y += 9;
  canvas.setTextColor(0xF81F);
  canvas.setCursor(2, y); canvas.print("DRUMS: ENTER=toggle  A/D=lane");
  y += 9;
  canvas.setTextColor(0x07FF);
  canvas.setCursor(2, y); canvas.print("MISC:  0=mute TAB=page");
  y += 9;
  canvas.setCursor(2, y); canvas.print("       8=pattern  !=accent ~=slide");
  y += 9;
  canvas.setTextColor(0x4A49);
  canvas.setCursor(2, y); canvas.print("      DEL=clear  SPACE+R=reset all");

  canvas.pushSprite(0, 0);
}

// ============================================================
// DISPLAY ROUTER
// ============================================================
void drawScreen() {
  switch (curPage) {
    case PAGE_PATTERN: drawPatternPage(); break;
    case PAGE_SOUND:   drawSoundPage(); break;
    case PAGE_HELP:    drawHelpPage(); break;
    default: break;
  }
}

// ============================================================
// INPUT HANDLING
// ============================================================
void handleInput() {
  M5Cardputer.update();
  if (!M5Cardputer.Keyboard.isChange()) return;
  if (!M5Cardputer.Keyboard.isPressed()) return;

  Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

  // DEL = clear cell
  if (status.del) {
    if (curTrack < NUM_SYNTHS) {
      pat().synth[curTrack][curStep] = {NOTE_EMPTY, curOctave, false, false};
    } else {
      pat().drums[curStep] = {false, false, false, false};
    }
    needRedraw = true;
    return;
  }

  // ENTER = toggle drum hit (when on drum track)
  if (status.enter) {
    if (curTrack >= NUM_SYNTHS) {
      // Toggle the selected drum lane
      DrumCell& dc = pat().drums[curStep];
      switch (curDrumLane) {
        case 0: dc.kick  = !dc.kick; break;
        case 1: dc.snare = !dc.snare; break;
        case 2: dc.hihat = !dc.hihat; break;
        case 3: dc.clap  = !dc.clap; break;
      }
      // Auto-advance
      if (curStep < NUM_STEPS - 1) curStep++;
      needRedraw = true;
    }
    return;
  }

  // Process character keys
  for (auto key : status.word) {
    // --- Synth parameter adjustment (works anytime) ---
    if (curTrack < NUM_SYNTHS) {
      SynthVoice& v = synths[curTrack];
      switch (key) {
        case 'f': v.fltCutoff = min(1.0f, v.fltCutoff + 0.05f); needRedraw = true; continue;
        case 'g': v.fltCutoff = max(0.0f, v.fltCutoff - 0.05f); needRedraw = true; continue;
        case 'r': v.fltReso = min(1.0f, v.fltReso + 0.05f); needRedraw = true; continue;
        case 't': v.fltReso = max(0.0f, v.fltReso - 0.05f); needRedraw = true; continue;
        case 'y': v.fltEnvAmt = min(1.0f, v.fltEnvAmt + 0.05f); needRedraw = true; continue;
        case 'u': v.fltEnvAmt = max(0.0f, v.fltEnvAmt - 0.05f); needRedraw = true; continue;
        case 'i': v.ampDecRate = min(0.99999f, v.ampDecRate + 0.00005f); needRedraw = true; continue; // longer decay
        case 'o': v.ampDecRate = max(0.999f, v.ampDecRate - 0.00005f); needRedraw = true; continue;   // shorter
        case '9': v.waveform = (Wave)((v.waveform + 1) % W_COUNT); needRedraw = true; continue;
      }
    }

    switch (key) {
      // --- Navigation ---
      case 'w': case ';':
        if (curStep > 0) curStep--;
        needRedraw = true;
        break;
      case 's': case '.':
        if (curStep < NUM_STEPS - 1) curStep++;
        needRedraw = true;
        break;
      case 'a':
        if (curTrack >= NUM_SYNTHS && curDrumLane > 0) {
          curDrumLane--;
        } else if (curTrack > 0) {
          curTrack--;
          if (curTrack >= NUM_SYNTHS) curDrumLane = 3;
        }
        needRedraw = true;
        break;
      case 'd':
        if (curTrack >= NUM_SYNTHS && curDrumLane < 3) {
          curDrumLane++;
        } else if (curTrack < NUM_SYNTHS) {
          curTrack++;
          if (curTrack >= NUM_SYNTHS) curDrumLane = 0;
        }
        needRedraw = true;
        break;

      // --- Track selection ---
      case '1': curTrack = 0; needRedraw = true; break;
      case '2': curTrack = 1; needRedraw = true; break;
      case '3': curTrack = 2; needRedraw = true; break;
      case '4': curTrack = NUM_SYNTHS; curDrumLane = 0; needRedraw = true; break;

      // --- Octave ---
      case ']': if (curOctave < 7) curOctave++; needRedraw = true; break;
      case '[': if (curOctave > 2) curOctave--; needRedraw = true; break;

      // --- BPM ---
      case 'q': if (bpm > 40)  bpm -= 5; needRedraw = true; break;
      case 'e': if (bpm < 300) bpm += 5; needRedraw = true; break;

      // --- Play/Stop ---
      case ' ':
        playing = !playing;
        if (playing) {
          playStep = 0;
          lastStepTime = millis();
        } else {
          for (int s = 0; s < NUM_SYNTHS; s++) synths[s].noteOff();
        }
        needRedraw = true;
        break;

      // --- Pattern select ---
      case '8':
        curPattern = (curPattern + 1) % NUM_PATTERNS;
        needRedraw = true;
        break;

      // --- Mute toggle ---
      case '0':
        if (curTrack < NUM_SYNTHS) synthMute[curTrack] = !synthMute[curTrack];
        else drumMute = !drumMute;
        needRedraw = true;
        break;

      // --- Accent toggle on current cell ---
      case '!':
        if (curTrack < NUM_SYNTHS) {
          pat().synth[curTrack][curStep].accent = !pat().synth[curTrack][curStep].accent;
          needRedraw = true;
        }
        break;

      // --- Slide toggle on current cell ---
      case '~':
        if (curTrack < NUM_SYNTHS) {
          pat().synth[curTrack][curStep].slide = !pat().synth[curTrack][curStep].slide;
          needRedraw = true;
        }
        break;

      // --- Note entry (only on synth tracks) ---
      case 'z': enterNote(NOTE_C); break;
      case 'x': enterNote(NOTE_D); break;
      case 'c': enterNote(NOTE_E); break;
      case 'v': enterNote(NOTE_F); break;
      case 'b': enterNote(NOTE_G); break;
      case 'n': enterNote(NOTE_A); break;
      case 'm': enterNote(NOTE_B); break;
      case 'h': enterNote(NOTE_CS); break;
      case 'j': enterNote(NOTE_DS); break;
      case 'k': enterNote(NOTE_FS); break;
      case 'l': enterNote(NOTE_GS); break;
      case '/': enterNote(NOTE_AS); break;

      // --- Demo pattern ---
      case 'p':
        if (!playing) {
          loadDemoPattern();
          needRedraw = true;
        }
        break;
    }
  }

  // Long press detection for page switch: use Fn or opt key
  // We'll use the BtnA on cardputer for page cycling
  if (M5Cardputer.BtnA.wasPressed()) {
    curPage = (Page)((curPage + 1) % PAGE_COUNT);
    needRedraw = true;
  }
}

void enterNote(uint8_t note) {
  if (curTrack >= NUM_SYNTHS) {
    // On drum track, enter toggles drum hit instead
    DrumCell& dc = pat().drums[curStep];
    switch (curDrumLane) {
      case 0: dc.kick  = !dc.kick; break;
      case 1: dc.snare = !dc.snare; break;
      case 2: dc.hihat = !dc.hihat; break;
      case 3: dc.clap  = !dc.clap; break;
    }
  } else {
    pat().synth[curTrack][curStep] = {note, curOctave, false, false};
    // Preview sound
    float freq = noteToFreq(note, curOctave);
    if (freq > 10) synths[curTrack].noteOn(freq, false, false);
  }
  if (curStep < NUM_STEPS - 1) curStep++;
  needRedraw = true;
}

// ============================================================
// DEMO PATTERN - Acid house style
// ============================================================
void loadDemoPattern() {
  memset(&patterns[curPattern], 0, sizeof(Pattern));
  Pattern& p = pat();

  // -- Synth 1: Acid bassline (SAW, high reso) --
  synths[0].waveform = W_SAW;
  synths[0].fltCutoff = 0.15f;
  synths[0].fltReso = 0.7f;
  synths[0].fltEnvAmt = 0.7f;
  synths[0].ampDecRate = 0.9998f;   // long decay for acid bass

  p.synth[0][0]  = {NOTE_C,  3, true,  false};
  p.synth[0][1]  = {NOTE_C,  3, false, false};
  p.synth[0][3]  = {NOTE_C,  3, false, true};
  p.synth[0][4]  = {NOTE_DS, 3, true,  false};
  p.synth[0][6]  = {NOTE_C,  3, false, false};
  p.synth[0][7]  = {NOTE_G,  2, false, true};
  p.synth[0][8]  = {NOTE_AS, 2, true,  false};
  p.synth[0][10] = {NOTE_C,  3, false, false};
  p.synth[0][12] = {NOTE_DS, 3, false, true};
  p.synth[0][13] = {NOTE_F,  3, true,  false};
  p.synth[0][14] = {NOTE_DS, 3, false, true};
  p.synth[0][15] = {NOTE_C,  3, false, false};

  // -- Synth 2: Pad / Chord stabs (SQUARE) --
  synths[1].waveform = W_SQUARE;
  synths[1].fltCutoff = 0.35f;
  synths[1].fltReso = 0.2f;
  synths[1].fltEnvAmt = 0.3f;
  synths[1].ampDecRate = 0.9996f;  // medium decay
  synths[1].volume = 0.45f;

  p.synth[1][0]  = {NOTE_G,  4, false, false};
  p.synth[1][4]  = {NOTE_AS, 4, false, false};
  p.synth[1][8]  = {NOTE_G,  4, true,  false};
  p.synth[1][12] = {NOTE_F,  4, false, false};

  // -- Synth 3: Lead / Arp (TRI) --
  synths[2].waveform = W_TRI;
  synths[2].fltCutoff = 0.5f;
  synths[2].fltReso = 0.15f;
  synths[2].fltEnvAmt = 0.4f;
  synths[2].ampDecRate = 0.9993f;  // shorter decay for arp
  synths[2].volume = 0.35f;

  p.synth[2][0]  = {NOTE_C,  5, false, false};
  p.synth[2][2]  = {NOTE_DS, 5, false, false};
  p.synth[2][4]  = {NOTE_G,  5, false, false};
  p.synth[2][6]  = {NOTE_AS, 5, false, true};
  p.synth[2][7]  = {NOTE_G,  5, false, false};
  p.synth[2][8]  = {NOTE_F,  5, false, false};
  p.synth[2][10] = {NOTE_DS, 5, false, false};
  p.synth[2][12] = {NOTE_C,  5, false, true};
  p.synth[2][13] = {NOTE_DS, 5, false, false};

  // -- Drums: Classic house pattern --
  // Kick on 1,2,3,4 (four on the floor)
  for (int i = 0; i < 16; i += 4) p.drums[i].kick = true;
  // Snare on 2 and 4
  p.drums[4].snare  = true;
  p.drums[12].snare = true;
  // HiHat offbeats
  for (int i = 2; i < 16; i += 4) p.drums[i].hihat = true;
  // Extra hihats for groove
  p.drums[6].hihat  = true;
  p.drums[10].hihat = true;
  p.drums[14].hihat = true;
  // Clap on snare hits
  p.drums[4].clap  = true;
  p.drums[12].clap = true;

}

// ============================================================
// SETUP
// ============================================================
void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);

  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.fillScreen(TFT_BLACK);

  // Init sprite for double-buffered rendering
  canvas.createSprite(SCREEN_W, SCREEN_H);
  canvas.setTextFont(1);
  canvas.setTextSize(1);

  // Speaker config
  auto spk_cfg = M5Cardputer.Speaker.config();
  spk_cfg.sample_rate = SAMPLE_RATE;
  spk_cfg.task_priority = 3;       // higher than audio gen task
  spk_cfg.dma_buf_count = 4;       // fewer = lower latency
  spk_cfg.dma_buf_len = AUDIO_BUF_LEN;
  M5Cardputer.Speaker.config(spk_cfg);
  M5Cardputer.Speaker.begin();
  M5Cardputer.Speaker.setVolume(200);

  // Init synth voices
  for (int s = 0; s < NUM_SYNTHS; s++) synths[s].init();
  synths[0].waveform = W_SAW;
  synths[0].fltCutoff = 0.3f;
  synths[0].fltReso = 0.4f;
  synths[0].fltEnvAmt = 0.6f;
  synths[0].ampDecRate = 0.99985f;
  synths[1].waveform = W_SQUARE;
  synths[1].fltCutoff = 0.45f;
  synths[1].volume = 0.6f;
  synths[1].ampDecRate = 0.9998f;
  synths[2].waveform = W_TRI;
  synths[2].fltCutoff = 0.5f;
  synths[2].volume = 0.5f;
  synths[2].ampDecRate = 0.9997f;

  // Init drums
  for (int d = 0; d < NUM_DRUMS; d++) drumVoices[d].init();

  // Clear patterns
  memset(patterns, 0, sizeof(patterns));
  memset(scopeBuf, 0, sizeof(scopeBuf));

  // Splash screen
  canvas.fillSprite(TFT_BLACK);
  canvas.setTextColor(0x07FF);
  canvas.setTextSize(2);
  canvas.setCursor(15, 10);
  canvas.print("CARDPUTER");
  canvas.setCursor(15, 32);
  canvas.setTextColor(0x07E0);
  canvas.print("TRACKER");

  canvas.setTextSize(1);
  canvas.setTextColor(0xFBE0);
  canvas.setCursor(10, 58);
  canvas.print("Real-time synth + 808 drums");

  canvas.setTextColor(TFT_WHITE);
  canvas.setCursor(10, 74);
  canvas.print("SPACE=Play  P=Demo  BtnA=Page");
  canvas.setCursor(10, 86);
  canvas.print("ZXCVBNM=Notes  1-4=Track");
  canvas.setCursor(10, 98);
  canvas.print("F/G=Cutoff R/T=Reso 9=Wave");

  canvas.setTextColor(0xFFE0);
  canvas.setCursor(10, 116);
  canvas.print("Press any key to start...");
  canvas.pushSprite(0, 0);

  // Wait for keypress
  while (true) {
    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) break;
    if (M5Cardputer.BtnA.wasPressed()) break;
    delay(50);
  }

  // Start audio task on Core 0
  xTaskCreatePinnedToCore(audioTask, "audio", 8192, NULL, 1, &audioTaskHandle, 0);

  needRedraw = true;
}

// ============================================================
// MAIN LOOP (Core 1: UI + Sequencer)
// ============================================================
void loop() {
  handleInput();

  // Sequencer tick
  if (playing) {
    uint32_t stepMs = 60000 / bpm / 4; // 16th notes
    unsigned long now = millis();
    if (now - lastStepTime >= stepMs) {
      lastStepTime = now;
      triggerStep(playStep);
      playStep = (playStep + 1) % NUM_STEPS;
      needRedraw = true;
    }
  }

  // Redraw
  if (needRedraw) {
    drawScreen();
    needRedraw = false;
  }

  // Continuous scope refresh on sound page
  if (curPage == PAGE_SOUND && playing) {
    static unsigned long lastScope = 0;
    if (millis() - lastScope > 60) { // ~16fps for scope
      lastScope = millis();
      drawScreen();
    }
  }

  delay(8);
}
