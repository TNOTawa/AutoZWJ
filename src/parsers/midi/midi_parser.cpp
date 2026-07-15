#include "midi_parser.h"
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>

static uint16_t read_be16(const uint8_t*& p) {
    uint16_t v = ((uint16_t)p[0] << 8) | p[1]; p += 2; return v;
}

static int16_t read_be16s(const uint8_t*& p) {
    int16_t v = (int16_t)(((uint16_t)p[0] << 8) | p[1]); p += 2; return v;
}

static uint32_t read_be32(const uint8_t*& p) {
    uint32_t v = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; p += 4; return v;
}

static uint32_t read_varlen(const uint8_t*& p, const uint8_t* end) {
    uint32_t v = 0;
    while (p < end) {
        uint8_t b = *p++;
        v = (v << 7) | (b & 0x7F);
        if ((b & 0x80) == 0) break;
    }
    return v;
}

struct PendingNote {
    uint8_t note;
    uint8_t channel;
    double start_sec;
    uint8_t velocity;
    double volume = -1.0;      // CC7 at note-on time, -1 = 未设置
    double pan = -1.0;         // CC10 at note-on time, -1 = 未设置
    double pitch_bend = 0.0;   // PitchBend at note-on time, 0 = 中心
};

struct MidiEvent {
    double pos_sec;
    double length_sec;
    int note_number;
    double velocity;
    int track;
    std::string lyric;
    double volume = -1.0;
    double pan = -1.0;
    double pitch_bend = 0.0;
};

struct MidiTrackData {
    std::string track_name;
    std::vector<MidiEvent> events;
};

struct RawMidiTempoEvent {
    uint64_t tick;
    bool is_timesig;
    uint32_t tempo;
    uint8_t numerator;
};

bool parse_midi(const std::string& path, ObjDict& objdict,
                std::vector<TrackNode>& tracks, std::vector<std::string>& file_paths) {
    std::wstring wpath = utf8_to_wide(path);
    HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size;
    if (!GetFileSizeEx(hFile, &size) || size.QuadPart <= 0) { CloseHandle(hFile); return false; }
    std::vector<uint8_t> data((size_t)size.QuadPart);
    DWORD read = 0;
    BOOL ok = ReadFile(hFile, data.data(), (DWORD)size.QuadPart, &read, nullptr);
    CloseHandle(hFile);
    if (!ok || read != (DWORD)size.QuadPart) return false;

    const uint8_t* p = data.data();
    const uint8_t* end = p + data.size();

    if (end - p < 8 || memcmp(p, "MThd", 4) != 0) return false;
    p += 4;
    uint32_t header_len = read_be32(p);
    if (header_len < 6) return false;
    read_be16(p);
    uint16_t ntrks = read_be16s(p);
    int16_t division = read_be16s(p);
    p += header_len - 6;
    if (division <= 0) return false;

    double ticks_per_beat = (double)division;
    double usec_per_beat = 500000.0;
    double usec_per_tick = usec_per_beat / ticks_per_beat;

    std::vector<MidiTrackData> midi_tracks;
    std::vector<RawMidiTempoEvent> raw_tempo_events;
    bool has_any_note = false;

    for (int track_idx = 0; track_idx < (int)ntrks; track_idx++) {
        if (end - p < 8 || memcmp(p, "MTrk", 4) != 0) break;
        p += 4;
        uint32_t track_len = read_be32(p);
        const uint8_t* track_start = p;
        const uint8_t* track_end = p + track_len;

        MidiTrackData trk;

        std::vector<PendingNote> pending;
        double abs_time_sec = 0.0;
        uint64_t abs_tick = 0;
        uint8_t running_status = 0;

        // per-channel CC 状态，MIDI 标准 power-on 默认: volume=100, pan=64
        double ch_volume[16];
        double ch_pan[16];
        double ch_pitch_bend[16];
        for (int i = 0; i < 16; i++) {
            ch_volume[i] = -1.0;   // -1 表示该 track 尚未遇到 CC7
            ch_pan[i] = -1.0;      // -1 表示该 track 尚未遇到 CC10
            ch_pitch_bend[i] = 0.0;
        }

        bool need_delta = true;
        while (p < track_end) {
            if (need_delta) {
                uint32_t delta = read_varlen(p, track_end);
                if (p >= track_end) break;
                abs_tick += delta;
                abs_time_sec += delta * usec_per_tick / 1000000.0;
            }
            need_delta = true;

            uint8_t byte = *p;
            uint8_t status;
            if (byte < 0x80) {
                if (running_status == 0) {
                    // 孤立 data byte：无前置 running status，跳过并继续解析
                    // 不读取新的 delta time，直接尝试解析下一个事件
                    p++;
                    need_delta = false;
                    continue;
                }
                status = running_status;
            } else {
                status = *p++;
                if (status < 0xF0) running_status = status;
                else running_status = 0;
            }

            if (status == 0xFF) {
                if (p >= track_end) break;
                uint8_t meta_type = *p++;
                uint32_t meta_len = read_varlen(p, track_end);
                const uint8_t* meta_data = p;
                p += meta_len;
                if (meta_type == 0x51 && meta_len == 3 && meta_data + 3 <= track_end) {
                    uint32_t new_tempo = ((uint32_t)meta_data[0] << 16) |
                                         ((uint32_t)meta_data[1] << 8) | meta_data[2];
                    if (new_tempo > 0) {
                        usec_per_beat = (double)new_tempo;
                        usec_per_tick = usec_per_beat / ticks_per_beat;
                        raw_tempo_events.push_back({abs_tick, false, new_tempo, 0});
                    }
                } else if (meta_type == 0x58 && meta_len >= 1 && meta_data < track_end) {
                    uint8_t numerator = meta_data[0];
                    if (numerator > 0) {
                        raw_tempo_events.push_back({abs_tick, true, 0, numerator});
                    }
                } else if (meta_type == 0x03 && meta_len > 0) {
                    std::string name((const char*)meta_data, meta_len);
                    trk.track_name = name;
                } else if (meta_type == 0x05 && meta_len > 0) {
                    std::string lyric((const char*)meta_data, meta_len);
                    if (!pending.empty()) {
                    }
                }
                if (meta_type == 0x2F) break;
            } else if (status == 0xF0 || status == 0xF7) {
                uint32_t sysex_len = read_varlen(p, track_end);
                p += sysex_len;
            } else if (status < 0xF0) {
                uint8_t msg_type = (status >> 4) & 0x0F;
                uint8_t channel = status & 0x0F;

                if (msg_type == 0x8 || msg_type == 0x9) {
                    if (p + 2 > track_end) break;
                    uint8_t note = *p++;
                    uint8_t velocity = *p++;

                    if (msg_type == 0x9 && velocity > 0) {
                        PendingNote pn;
                        pn.note = note;
                        pn.channel = channel;
                        pn.start_sec = abs_time_sec;
                        pn.velocity = velocity;
                        pn.volume = ch_volume[channel];
                        pn.pan = ch_pan[channel];
                        pn.pitch_bend = ch_pitch_bend[channel];
                        pending.push_back(pn);
                    } else {
                        for (size_t i = 0; i < pending.size(); i++) {
                            if (pending[i].note == note && pending[i].channel == channel) {
                                MidiEvent evt;
                                evt.pos_sec = pending[i].start_sec;
                                evt.length_sec = abs_time_sec - pending[i].start_sec;
                                evt.note_number = (int)note;
                                evt.velocity = (double)pending[i].velocity;
                                evt.track = track_idx + 1;
                                evt.volume = pending[i].volume;
                                evt.pan = pending[i].pan;
                                evt.pitch_bend = pending[i].pitch_bend;
                                trk.events.push_back(evt);
                                has_any_note = true;
                                pending.erase(pending.begin() + i);
                                break;
                            }
                        }
                    }
                } else if (msg_type == 0xA) {
                    if (p + 2 <= track_end) p += 2; else break;
                } else if (msg_type == 0xB) {
                    if (p + 2 <= track_end) {
                        uint8_t cc_num = *p++;
                        uint8_t cc_val = *p++;
                        if (cc_num == 7) ch_volume[channel] = (double)cc_val;
                        else if (cc_num == 10) ch_pan[channel] = (double)cc_val;
                    } else {
                        break;
                    }
                } else if (msg_type == 0xC) {
                    if (p + 1 <= track_end) p += 1; else break;
                } else if (msg_type == 0xD) {
                    if (p + 1 <= track_end) p += 1; else break;
                } else if (msg_type == 0xE) {
                    if (p + 2 <= track_end) {
                        uint8_t lsb = *p++;
                        uint8_t msb = *p++;
                        int raw = ((int)msb << 7) | (int)lsb;
                        ch_pitch_bend[channel] = (double)(raw - 8192);
                    } else {
                        break;
                    }
                }
            }
        }

        for (auto& pn : pending) {
            MidiEvent evt;
            evt.pos_sec = pn.start_sec;
            evt.length_sec = abs_time_sec - pn.start_sec;
            if (evt.length_sec <= 0) evt.length_sec = 0.5;
            evt.note_number = (int)pn.note;
            evt.velocity = (double)pn.velocity;
            evt.track = track_idx + 1;
            evt.volume = pn.volume;
            evt.pan = pn.pan;
            evt.pitch_bend = pn.pitch_bend;
            trk.events.push_back(evt);
            has_any_note = true;
        }

        midi_tracks.push_back(trk);
        p = track_start + track_len;
    }

    if (!has_any_note) return false;

    objdict = ObjDict{};
    objdict.pos = {-1.0};
    objdict.length = {-1.0};
    objdict.loop = {-1};
    objdict.soffs = {-1.0};
    objdict.pitch = {-99.9};
    objdict.playrate = {-1.0};
    objdict.fileidx = {-1};
    objdict.filetype = {""};
    file_paths.clear();
    tracks.clear();

    objdict.bpm = 60000000.0 / usec_per_beat;
    objdict.track_count = (int)midi_tracks.size();

    // 构建 tempo_map：按 tick 排序（拍号优先于同 tick 的 tempo），tick→秒换算
    std::sort(raw_tempo_events.begin(), raw_tempo_events.end(),
        [](const RawMidiTempoEvent& a, const RawMidiTempoEvent& b) {
            if (a.tick != b.tick) return a.tick < b.tick;
            return a.is_timesig > b.is_timesig;
        });

    {
        uint64_t cur_tick = 0;
        double cur_time = 0.0;
        uint32_t cur_tempo = 500000;
        int cur_beat = 4;

        objdict.tempo_map.push_back({0.0, 60000000.0 / cur_tempo, cur_beat});

        for (const auto& ev : raw_tempo_events) {
            cur_time += (double)(ev.tick - cur_tick) * cur_tempo / 1000000.0 / ticks_per_beat;
            cur_tick = ev.tick;

            if (ev.is_timesig) {
                cur_beat = ev.numerator;
            } else {
                cur_tempo = ev.tempo;
            }

            double bpm = 60000000.0 / cur_tempo;
            if (!objdict.tempo_map.empty() && objdict.tempo_map.back().time_sec == cur_time) {
                objdict.tempo_map.back() = {cur_time, bpm, cur_beat};
            } else {
                objdict.tempo_map.push_back({cur_time, bpm, cur_beat});
            }
        }
    }

    for (size_t ti = 0; ti < midi_tracks.size(); ti++) {
        auto& trk = midi_tracks[ti];
        if (trk.events.empty()) continue;

        TrackNode node;
        if (!trk.track_name.empty())
            node.name = utf8_to_wide(trk.track_name);
        else
            node.name = L"Track " + std::to_wstring(ti + 1);
        node.number = (int)ti + 1;
        node.index = (int)objdict.pos.size();
        node.selected = true;

        for (size_t ei = 0; ei < trk.events.size(); ei++) {
            auto& ev = trk.events[ei];
            objdict.pos.push_back(ev.pos_sec);
            objdict.length.push_back(ev.length_sec);
            objdict.loop.push_back(0);
            objdict.soffs.push_back(0.0);
            objdict.pitch.push_back((double)(ev.note_number - 69));
            objdict.playrate.push_back(1.0);
            objdict.fileidx.push_back(-1);
            objdict.filetype.push_back("XMIDI");
            objdict.midi_note.push_back(ev.note_number);
            objdict.midi_lyric.push_back(ev.lyric);
            objdict.midi_len.push_back(ev.length_sec);
            objdict.midi_volume.push_back(ev.volume);
            objdict.midi_pan.push_back(ev.pan);
            objdict.midi_pitch_bend.push_back(ev.pitch_bend);
        }

        if (ti < midi_tracks.size() - 1) {
            objdict.pos.push_back(-1.0);
            objdict.length.push_back(-1.0);
            objdict.loop.push_back(-1);
            objdict.soffs.push_back(-1.0);
            objdict.pitch.push_back(-99.9);
            objdict.playrate.push_back(0);
            objdict.fileidx.push_back(-1);
            objdict.filetype.push_back("");
        }

        node.count = (int)objdict.pos.size() - node.index;
        if (node.count > 0 && objdict.pos.back() == -1.0) node.count--;
        tracks.push_back(node);
    }

    return true;
}
