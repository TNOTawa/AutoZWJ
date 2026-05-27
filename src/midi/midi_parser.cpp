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
};

struct MidiEvent {
    double pos_sec;
    double length_sec;
    int note_number;
    double velocity;
    int track;
    std::string lyric;
};

struct MidiTrackData {
    std::string track_name;
    std::vector<MidiEvent> events;
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
        uint8_t running_status = 0;

        while (p < track_end) {
            uint32_t delta = read_varlen(p, track_end);
            if (p >= track_end) break;
            abs_time_sec += delta * usec_per_tick / 1000000.0;

            uint8_t byte = *p;
            uint8_t status;
            if (byte < 0x80) {
                if (running_status == 0) { p++; continue; }
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
                    usec_per_beat = (double)(((uint32_t)meta_data[0] << 16) |
                                             ((uint32_t)meta_data[1] << 8) | meta_data[2]);
                    usec_per_tick = usec_per_beat / ticks_per_beat;
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
                    if (p + 2 <= track_end) p += 2; else break;
                } else if (msg_type == 0xC) {
                    if (p + 1 <= track_end) p += 1; else break;
                } else if (msg_type == 0xD) {
                    if (p + 1 <= track_end) p += 1; else break;
                } else if (msg_type == 0xE) {
                    if (p + 2 <= track_end) p += 2; else break;
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
