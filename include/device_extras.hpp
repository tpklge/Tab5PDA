#pragma once
#ifdef ARDUINO
#include "pda/service.hpp"
#include "sd_files.hpp"
#include <M5Unified.h>
#include <WebServer.h>
#include <atomic>
#include <mutex>
namespace tab5_pda {
class DeviceExtras {
  public:
    DeviceExtras(SdFiles &sd, pda::Service &service) : sd_(sd), service_(service) {}
    void tick();
    bool record(const std::string &);
    bool play(const std::string &);
    void stop();
    std::string audioStatus() const {
        return audioStatus_;
    }
    bool finished(std::string &);
    std::string sharing(bool enabled, const std::string &token);
    void close();
    bool active() const {
        return share_.load();
    }
    bool fileBusy() const {
        return fileBusy_.load();
    }
    bool ask(const std::string &, const std::string &, const std::string &, const std::string &);
    bool answer(std::string &, std::string &);

  private:
    void routes();
    bool authorized();
    void networkLoop();
    static void networkTask(void *);
    struct AiJob {
        DeviceExtras *owner;
        std::string url, token, prompt, now;
        uint32_t epoch;
    };
    static void aiTask(void *);
    SdFiles &sd_;
    pda::Service &service_;
    File audio_;
    int16_t samples_[1024]{};
    bool recording_ = false, playing_ = false, queued_ = false, audioFailed_ = false;
    uint32_t audioBytes_ = 0, playRemaining_ = 0;
    std::string audioName_, completedAudio_, audioStatus_ = "Áudio pronto";
    WebServer web_{8080};
    File upload_;
    std::string uploadName_, uploadError_;
    bool uploadOkay_ = false, uploadOwned_ = false;
    std::atomic<bool> share_{false}, fileBusy_{false}, webStarted_{false}, aiBusy_{false},
        aiReady_{false};
    std::atomic<uint32_t> epoch_{0}, shareEpoch_{0};
    uint32_t uploadEpoch_ = 0;
    std::mutex stateMutex_;
    std::string token_, aiText_, aiError_;
};
} // namespace tab5_pda
#endif
