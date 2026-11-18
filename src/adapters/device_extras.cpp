#include "device_extras.hpp"
#include "transfer_page.hpp"
#ifdef ARDUINO
#include <HTTPClient.h>
#include <WiFi.h>
#include <cstring>
#include <memory>
namespace tab5_pda {

bool DeviceExtras::record(const std::string &name) {
    stop();
    if (!sd_.mounted() || !safeName(name) || name.size() < 5 ||
        name.substr(name.size() - 4) != ".wav")
        return false;
    std::lock_guard<std::recursive_mutex> guard(sd_.mutex());
    if (SD.exists(("/Tab5PDA/" + name).c_str()))
        return false;
    M5.Speaker.end();
    if (!M5.Mic.begin()) {
        M5.Speaker.begin();
        audioStatus_ = "Microfone indisponível";
        return false;
    }
    audio_ = SD.open(("/Tab5PDA/" + name).c_str(), "w");
    const auto bytes = pcmHeader(0);
    if (!audio_ || audio_.write(bytes.data(), bytes.size()) != bytes.size()) {
        if (audio_)
            audio_.close();
        M5.Mic.end();
        M5.Speaker.begin();
        audioStatus_ = "Falha ao criar WAV";
        return false;
    }
    audioName_ = name;
    audioBytes_ = 0;
    recording_ = true;
    queued_ = false;
    audioFailed_ = false;
    audioStatus_ = "Gravando: " + name;
    return true;
}
bool DeviceExtras::play(const std::string &name) {
    stop();
    if (!publicFile(name))
        return false;
    std::lock_guard<std::recursive_mutex> guard(sd_.mutex());
    audio_ = SD.open(("/Tab5PDA/" + name).c_str());
    uint8_t bytes[44];
    uint32_t payload = 0;
    if (!audio_ || audio_.read(bytes, 44) != 44 || !pcmSize(bytes, audio_.size(), payload)) {
        if (audio_)
            audio_.close();
        audioStatus_ = "Use WAV PCM mono 16 kHz / 16 bits gerado pelo PDA";
        return false;
    }
    playRemaining_ = payload;
    playing_ = true;
    audioStatus_ = "Reproduzindo: " + name;
    return true;
}
void DeviceExtras::stop() {
    if (recording_) {
        // end() waits until the driver releases the queued buffer.
        const bool complete = queued_ && M5.Mic.isRecording() == 0;
        M5.Mic.end();
        std::lock_guard<std::recursive_mutex> guard(sd_.mutex());
        if (complete && audio_ && audioBytes_ < 9600000) {
            if (audio_.write(reinterpret_cast<uint8_t *>(samples_), sizeof(samples_)) ==
                sizeof(samples_))
                audioBytes_ += sizeof(samples_);
            else
                audioFailed_ = true;
        }
        const auto bytes = pcmHeader(audioBytes_);
        bool ok =
            audio_ && audio_.seek(0) && audio_.write(bytes.data(), bytes.size()) == bytes.size();
        if (audio_) {
            audio_.flush();
            audio_.close();
        }
        if (ok) {
            auto check = SD.open(("/Tab5PDA/" + audioName_).c_str());
            uint8_t verify[44];
            uint32_t payload = 0;
            ok = check && check.read(verify, 44) == 44 && pcmSize(verify, check.size(), payload) &&
                 payload == audioBytes_;
            check.close();
        }
        recording_ = queued_ = false;
        M5.Speaker.begin();
        M5.Speaker.setVolume(service_.state().settings.volume);
        if (ok && !audioFailed_ && audioBytes_ > 0) {
            completedAudio_ = audioName_;
            audioStatus_ = "WAV salvo: " + audioName_;
        } else if (ok) {
            audioStatus_ = audioFailed_ ? "Áudio parcial preservado, sem vínculo: " + audioName_
                                        : "Gravação vazia; anexo anterior preservado";
        } else
            audioStatus_ = "Não foi possível finalizar WAV";
    }
    if (playing_) {
        M5.Speaker.stop();
        std::lock_guard<std::recursive_mutex> guard(sd_.mutex());
        audio_.close();
        playing_ = false;
        audioStatus_ = "Reprodução encerrada";
    }
}
void DeviceExtras::tick() {
    if (share_ && WiFi.status() != WL_CONNECTED)
        share_ = false;
    if (recording_) {
        if (queued_ && M5.Mic.isRecording())
            return;
        if (queued_) {
            std::lock_guard<std::recursive_mutex> guard(sd_.mutex());
            auto n = audio_.write(reinterpret_cast<uint8_t *>(samples_), sizeof(samples_));
            queued_ = false;
            if (n != sizeof(samples_)) {
                audioFailed_ = true;
                stop();
                audioStatus_ = "Falha no microSD; áudio interrompido";
                return;
            }
            audioBytes_ += n;
        }
        if (audioBytes_ + sizeof(samples_) > 9600000) {
            stop();
            return;
        }
        queued_ = M5.Mic.record(samples_, 1024, 16000, false);
        if (!queued_) {
            audioFailed_ = true;
            stop();
            audioStatus_ = "Falha na captura de áudio";
        }
    } else if (playing_ && !M5.Speaker.isPlaying()) {
        if (!playRemaining_) {
            stop();
            return;
        }
        std::size_t n = std::min<std::size_t>(sizeof(samples_), playRemaining_);
        {
            std::lock_guard<std::recursive_mutex> guard(sd_.mutex());
            if (audio_.read(reinterpret_cast<uint8_t *>(samples_), n) != n) {
                stop();
                return;
            }
        }
        playRemaining_ -= n;
        if (!M5.Speaker.playRaw(samples_, n / 2, 16000, false, 1)) {
            stop();
            audioStatus_ = "Falha na reprodução";
        }
    }
}
bool DeviceExtras::finished(std::string &name) {
    if (completedAudio_.empty())
        return false;
    name = completedAudio_;
    completedAudio_.clear();
    return true;
}
std::string DeviceExtras::sharing(bool enabled, const std::string &token) {
    if (!enabled) {
        ++shareEpoch_;
        share_ = false;
        return "Sessão Wi-Fi encerrada";
    }
    if (WiFi.status() != WL_CONNECTED)
        return "Conecte ao Wi-Fi antes de abrir a sessão";
    {
        std::lock_guard<std::mutex> guard(stateMutex_);
        ++shareEpoch_;
        token_ = token;
    }
    share_ = true;
    if (!webStarted_.exchange(true)) {
        if (xTaskCreatePinnedToCore(networkTask, "PDA_FILES", 8192, this, 1, nullptr, 0) !=
            pdPASS) {
            webStarted_ = false;
            share_ = false;
            return "Não foi possível iniciar o servidor";
        }
    }
    return "http://" + std::string(WiFi.localIP().toString().c_str()) + ":8080  Código: " + token;
}
void DeviceExtras::close() {
    share_ = false;
    ++shareEpoch_;
    {
        std::lock_guard<std::mutex> guard(stateMutex_);
        ++epoch_;
        aiReady_ = false;
        aiText_.clear();
        aiError_.clear();
    }
    stop();
}
bool DeviceExtras::authorized() {
    std::lock_guard<std::mutex> guard(stateMutex_);
    return share_ && !token_.empty() && web_.header("X-PDA-Token") == token_.c_str();
}
void DeviceExtras::routes() {
    const char *headers[] = {"X-PDA-Token"};
    web_.collectHeaders(headers, 1);
    web_.on("/", HTTP_GET, [this] {
        if (!share_) {
            web_.send(403, "text/plain", "Sessão encerrada");
            return;
        }
        web_.send(200, "text/html; charset=utf-8", transferPage);
    });
    web_.on("/files", HTTP_GET, [this] {
        if (!authorized()) {
            web_.send(403, "text/plain", "Código incorreto ou sessão encerrada");
            return;
        }
        std::string text;
        for (const auto &f : sd_.list())
            if (publicFile(f.name))
                text += f.name + "\t" + std::to_string(f.bytes) + "\n";
        web_.send(200, "text/plain; charset=utf-8", text.c_str());
    });
    web_.on("/download", HTTP_GET, [this] {
        const auto session = shareEpoch_.load();
        auto name = std::string(web_.arg("name").c_str());
        if (!authorized() || session != shareEpoch_.load() || !publicFile(name)) {
            web_.send(403, "text/plain", "Arquivo não permitido");
            return;
        }
        fileBusy_ = true;
        File file;
        {
            std::lock_guard<std::recursive_mutex> guard(sd_.mutex());
            file = SD.open(("/Tab5PDA/" + name).c_str());
        }
        if (!file) {
            fileBusy_ = false;
            web_.send(404, "text/plain", "Arquivo ausente");
            return;
        }
        web_.sendHeader("Content-Disposition", ("attachment; filename=\"" + name + "\"").c_str());
        web_.setContentLength(file.size());
        web_.send(200, "application/octet-stream", "");
        uint8_t buffer[1024];
        while (share_ && session == shareEpoch_.load() && web_.client().connected()) {
            std::size_t n;
            {
                std::lock_guard<std::recursive_mutex> guard(sd_.mutex());
                n = file.read(buffer, sizeof(buffer));
            }
            if (!n)
                break;
            std::size_t sent = 0;
            while (sent < n && share_ && session == shareEpoch_.load() &&
                   web_.client().connected()) {
                const auto count = web_.client().write(buffer + sent, n - sent);
                if (!count)
                    break;
                sent += count;
            }
            if (sent != n) {
                web_.client().stop();
                break;
            }
        }
        {
            std::lock_guard<std::recursive_mutex> guard(sd_.mutex());
            if (file.position() != file.size())
                web_.client().stop();
            file.close();
        }
        fileBusy_ = false;
    });
    web_.on(
        "/upload", HTTP_POST,
        [this] {
            fileBusy_ = false;
            web_.send(uploadOkay_ ? 200 : 400, "text/plain; charset=utf-8",
                      uploadOkay_ ? ("Preparado no SD: " + uploadName_).c_str()
                                  : uploadError_.c_str());
        },
        [this] {
            auto &u = web_.upload();
            std::lock_guard<std::recursive_mutex> guard(sd_.mutex());
            if (u.status == UPLOAD_FILE_START) {
                fileBusy_ = true;
                if (upload_)
                    upload_.close();
                if (uploadOwned_ && safeName(uploadName_))
                    SD.remove(("/Tab5PDA/" + uploadName_ + ".part").c_str());
                uploadOwned_ = uploadOkay_ = false;
                uploadName_.clear();
                uploadError_.clear();
                uploadEpoch_ = shareEpoch_.load();
                auto name = std::string(web_.arg("name").c_str());
                auto staged = "incoming-" + std::to_string(millis()) + "-" + name;
                if (!authorized() || uploadEpoch_ != shareEpoch_.load() || !publicFile(name) ||
                    !safeName(staged)) {
                    uploadError_ = "Sessão ou nome inválido";
                    web_.client().stop();
                    return;
                }
                uploadName_ = staged;
                auto path = "/Tab5PDA/" + uploadName_ + ".part";
                if (SD.exists(path.c_str()) || SD.exists(("/Tab5PDA/" + uploadName_).c_str())) {
                    uploadError_ = "Arquivo já existe";
                    return;
                }
                upload_ = SD.open(path.c_str(), "w");
                uploadOwned_ = bool(upload_);
                if (!upload_)
                    uploadError_ = "microSD indisponível";
            } else if (u.status == UPLOAD_FILE_WRITE) {
                if (!share_ || uploadEpoch_ != shareEpoch_.load()) {
                    uploadError_ = "Sessão encerrada";
                    web_.client().stop();
                }
                if (u.totalSize + u.currentSize > 10 * 1024 * 1024) {
                    uploadError_ = "Arquivo maior que 10 MiB";
                    web_.client().stop();
                }
                if (upload_ && uploadError_.empty() &&
                    upload_.write(u.buf, u.currentSize) != u.currentSize)
                    uploadError_ = "Falha ao gravar";
            } else if (u.status == UPLOAD_FILE_END || u.status == UPLOAD_FILE_ABORTED) {
                if (upload_) {
                    upload_.flush();
                    upload_.close();
                }
                fileBusy_ = false;
                if (!uploadOwned_)
                    return;
                const auto path = "/Tab5PDA/" + uploadName_ + ".part",
                           finalPath = "/Tab5PDA/" + uploadName_;
                if (u.status == UPLOAD_FILE_END) {
                    File check = SD.open(path.c_str());
                    uploadOkay_ = share_ && uploadEpoch_ == shareEpoch_.load() &&
                                  uploadError_.empty() && check && check.size() == u.totalSize &&
                                  u.totalSize <= 10 * 1024 * 1024;
                    check.close();
                    if (uploadOkay_ && !SD.exists(finalPath.c_str()))
                        uploadOkay_ = SD.rename(path.c_str(), finalPath.c_str());
                    else
                        uploadOkay_ = false;
                }
                if (!uploadOkay_) {
                    SD.remove(path.c_str());
                    if (uploadError_.empty())
                        uploadError_ = "Envio incompleto ou interrompido";
                }
                uploadOwned_ = false;
            }
        });
}
void DeviceExtras::networkTask(void *arg) {
    static_cast<DeviceExtras *>(arg)->networkLoop();
}
void DeviceExtras::networkLoop() {
    routes();
    web_.begin();
    for (;;) {
        web_.handleClient();
        vTaskDelay(pdMS_TO_TICKS(4));
    }
}
bool DeviceExtras::ask(const std::string &url, const std::string &token, const std::string &prompt,
                       const std::string &now) {
    if (WiFi.status() != WL_CONNECTED || url.rfind("http://", 0) != 0 || url.size() > 180 ||
        prompt.empty() || prompt.size() > 4096 || aiBusy_.exchange(true))
        return false;
    auto *job = new AiJob{this, url, token, prompt, now, epoch_.load()};
    aiReady_ = false;
    if (xTaskCreatePinnedToCore(aiTask, "PDA_AI", 12288, job, 1, nullptr, 0) != pdPASS) {
        delete job;
        aiBusy_ = false;
        return false;
    }
    return true;
}
void DeviceExtras::aiTask(void *arg) {
    std::unique_ptr<AiJob> job(static_cast<AiJob *>(arg));
    auto *owner = job->owner;
    std::string text, error;
    WiFiClient client;
    HTTPClient http;
    http.setTimeout(60000);
    http.setConnectTimeout(5000);
    if (http.begin(client, job->url.c_str())) {
        http.addHeader("Content-Type", "text/plain; charset=utf-8");
        http.addHeader("Authorization", ("Bearer " + job->token).c_str());
        http.addHeader("X-PDA-Now", job->now.c_str());
        auto code = http.POST(reinterpret_cast<uint8_t *>(job->prompt.data()), job->prompt.size());
        if (code == 200 && http.getSize() >= 0 && http.getSize() <= 65536)
            text = http.getString().c_str();
        else
            error = "Assistente: HTTP " + std::to_string(code) + " ou resposta grande/sem tamanho";
        http.end();
    } else
        error = "URL do assistente inválida";
    {
        std::lock_guard<std::mutex> guard(owner->stateMutex_);
        if (job->epoch == owner->epoch_) {
            owner->aiText_ = std::move(text);
            owner->aiError_ = std::move(error);
            owner->aiReady_ = true;
        }
    }
    owner->aiBusy_ = false;
    job.reset();
    vTaskDelete(nullptr);
}
bool DeviceExtras::answer(std::string &text, std::string &error) {
    if (!aiReady_.exchange(false))
        return false;
    std::lock_guard<std::mutex> guard(stateMutex_);
    text = std::move(aiText_);
    error = std::move(aiError_);
    return true;
}
} // namespace tab5_pda
#endif
