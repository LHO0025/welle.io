/*
 *    Copyright (C) 2020
 *    Matthias P. Braendli (matthias.braendli@mpb.li)
 *
 *    This file is part of the welle.io.
 *    Many of the ideas as implemented in welle.io are derived from
 *    other work, made available through the GNU general Public License.
 *    All copyrights of the original authors are recognized.
 *
 *    welle.io is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    welle.io is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with welle.io; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#pragma once

#include "various/Socket.h"
#include "backend/radio-receiver.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <chrono>
#include <string>
#include <atomic>
#include <deque>
#include <shared_mutex>
#include "webprogrammehandler.h"

class WebProgrammeHandler;
class ProgrammeSender {
    private:
        Socket s;

        std::atomic<bool> running = ATOMIC_VAR_INIT(true);
        mutable std::condition_variable cv;
        mutable std::mutex mutex;
        bool headerSent = false;

    public:
        bool isLive = true;
        int cached_mp3_index = 0;
        ProgrammeSender(Socket&& s);
        ProgrammeSender(ProgrammeSender&& other);
        ProgrammeSender& operator=(ProgrammeSender&& other);
        bool send_stream(const std::vector<uint8_t>& headerdata, const std::vector<uint8_t>& mp3data);
        bool send_cached_stream(WebProgrammeHandler& webProgrammeHandler, ssize_t index);
        void wait_for_termination() const;
        void cancel();
};


enum class OutputCodec {MP3, FLAC};

enum class MOTType { JPEG, PNG, Unknown };

class IEncoder;

class WebProgrammeHandler : public ProgrammeHandlerInterface {
    public:
        struct xpad_error_t {
            bool has_error = false;
            size_t announced_xpad_len = 0;
            size_t xpad_len = 0;
            std::chrono::time_point<std::chrono::system_clock> time;
        };

        struct audiolevels_t {
            std::chrono::time_point<std::chrono::system_clock> time;
            int last_audioLevel_L = -1;
            int last_audioLevel_R = -1;
        };

        struct errorcounters_t {
            std::chrono::time_point<std::chrono::system_clock> time;
            size_t num_frameErrors = 0;
            size_t num_rsErrors = 0;
            size_t num_aacErrors = 0;
        };

        mutable std::mutex cached_mp3_mutex;
        mutable std::mutex audio_buffer_mutex;

    private:
        uint32_t serviceId;
        const OutputCodec codec;
        std::unique_ptr<IEncoder> encoder;

        mutable std::mutex senders_mutex;
        std::list<ProgrammeSender*> senders;

        mutable std::mutex stats_mutex;

        errorcounters_t errorcounters;

        int audioBufferMaxLen;

        bool last_label_valid = false;
        std::chrono::time_point<std::chrono::system_clock> time_label;
        std::chrono::time_point<std::chrono::system_clock> time_label_change;
        std::string last_label;

        bool last_mot_valid = false;
        std::chrono::time_point<std::chrono::system_clock> time_mot;
        std::chrono::time_point<std::chrono::system_clock> time_mot_change;
        std::vector<uint8_t> last_mot;
        
        MOTType last_subtype = MOTType::Unknown;

        xpad_error_t xpad_error;

        audiolevels_t audiolevels;

    public:
        std::deque<uint8_t> audioBuffer;
        int rate = 0;
        std::string mode;

        WebProgrammeHandler(uint32_t serviceId, OutputCodec codec, long audioBufferMaxLen);
        WebProgrammeHandler(WebProgrammeHandler&& other);
        ~WebProgrammeHandler();

        void registerSender(ProgrammeSender *sender);
        void removeSender(ProgrammeSender *sender);
        bool needsToBeDecoded() const;
        void cancelAll();
        void send_to_all_clients(const std::vector<uint8_t>& headerData, const std::vector<uint8_t>& data);
        void cache_mp3(const std::vector<uint8_t>& data);
        std::vector<uint8_t> getAudioBufferChunk(int);

        struct dls_t {
            std::string label;
            std::chrono::time_point<std::chrono::system_clock> time;
            std::chrono::time_point<std::chrono::system_clock> last_changed; };
        dls_t getDLS() const;
        std::deque<dls_t> dls_buffer;

        std::string findClosestLabel(std::chrono::time_point<std::chrono::system_clock> targetTime);
        void cache_dls_data();

        struct mot_t {
            std::vector<uint8_t> data;
            MOTType subtype = MOTType::Unknown;
            std::chrono::time_point<std::chrono::system_clock> time;
            std::chrono::time_point<std::chrono::system_clock> last_changed; };
        mot_t getMOT() const;
        std::deque<mot_t> mot_buffer;

        struct dls_buffer_record {
            std::string label;
            std::chrono::time_point<std::chrono::system_clock> time;
            mot_t mot;
        };
        std::deque<dls_buffer_record> dlsDataBuffer;

        xpad_error_t getXPADErrors() const;
        audiolevels_t getAudioLevels() const;
        errorcounters_t getErrorCounters() const;

        virtual void onFrameErrors(int frameErrors) override;
        virtual void onNewAudio(std::vector<int16_t>&& audioData,
                int sampleRate, const std::string& mode) override;
        virtual void onRsErrors(bool uncorrectedErrors, int numCorrectedErrors) override;
        virtual void onAacErrors(int aacErrors) override;
        virtual void onNewDynamicLabel(const std::string& label) override;
        virtual void onMOT(const mot_file_t& mot_file) override;
        virtual void onPADLengthError(size_t announced_xpad_len, size_t xpad_len) override;
};

