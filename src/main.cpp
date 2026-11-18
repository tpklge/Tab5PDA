#include "device_extras.hpp"
#include "sd_files.hpp"
#include "tab_ui.hpp"
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <atomic>
#include <ctime>
#include <esp_heap_caps.h>
#include <esp_sntp.h>
#include <esp_timer.h>
#include <lgfx/v1/platforms/esp32p4/Panel_DSI.hpp>
#include <mbedtls/pkcs5.h>
#include <memory>

namespace {
using namespace tab5_pda;
bool keyboardReady = false, keyboardBusReady = false;
SdFiles sdFiles;
pda::SnapshotStore store(sdFiles);
pda::Service service(store);
class Platform final : public IPlatform {
  public:
    bool begin() {
        auto cfg = M5.config();
        cfg.internal_mic = true;
        cfg.internal_imu = false;
        cfg.internal_rtc = true;
        M5.begin(cfg);
        // Landscape with the physical keyboard below the display.
        M5.Display.setRotation(3);
        if (M5.getBoard() != m5::board_t::board_M5Tab5 || M5.Display.width() != 1280 ||
            M5.Display.height() != 720 || ESP.getPsramSize() < 16 * 1024 * 1024)
            return false;
        // Clear the entire DSI framebuffer, including areas untouched by partial refreshes.
        M5.Display.fillScreen(TFT_BLACK);
        // Identification belongs to M5GFX; read the selected panel timing for diagnostics only.
        auto *panel = M5.Display.getPanel();
        touch_ = M5.Touch.isEnabled() && panel && panel->getTouch();
        if (panel) {
            auto detail = static_cast<lgfx::Panel_DSI *>(panel)->config_detail();
            displayModel_ = detail.vsync_front_porch == 200   ? "ST7121"
                            : detail.vsync_front_porch == 220 ? "ST7123"
                            : detail.vsync_front_porch == 20  ? "ILI9881C / GT911"
                                                              : "Tab5 / perfil SDK";
        }
        auto &ioe = M5.getIOExpander(0);
        ioe.digitalWrite(0, false);
        return true;
    }
    uint32_t nowMs() const override {
        return millis();
    }
    std::string localMinute() const override {
        if (!clockValid_)
            return {};
        return pda::formatCivilMinute(
            pda::unixToCivil(epoch_ + (esp_timer_get_time() - startUs_) / 1000000,
                             service.state().settings.utcOffsetMinutes));
    }
    bool setClock(const std::string &local, int offset, std::string &error) override {
        pda::CivilMinute t;
        if (!pda::parseCivilMinute(local, t)) {
            error = "Hora inválida";
            return false;
        }
        epoch_ = pda::civilToUnix(t, offset);
        startUs_ = esp_timer_get_time();
        clockValid_ = true;
        source_ = "manual";
        writeRtc();
        return true;
    }
    void applySettings(const pda::Settings &s) override {
        M5.Display.setBrightness(sleeping_ ? 0 : s.brightness);
        M5.Speaker.setVolume(s.volume);
    }
    bool physicalKeyboardConnected() const override {
        return keyboardReady;
    }
    bool remount() override {
        extras_.close();
        std::lock_guard<std::recursive_mutex> ioGuard(sdFiles.mutex());
        if (extras_.fileBusy()) {
            store.unavailable("Aguarde a transferência terminar antes de reabrir o SD");
            return false;
        }
        if (sdFiles.begin())
            return true;
        store.unavailable("microSD ausente ou indisponível");
        return false;
    }
    void notify() override {
        activity();
        if (quietNow(service.state().settings, localMinute()))
            return;
        auto tone = std::stoi(pda::value(service.state().settings.fields, "alarmTone", "1200"));
        if (service.state().settings.volume && tone > 0) {
            extras_.stop();
            if (M5.Speaker.isEnabled())
                M5.Speaker.tone(tone, 350);
        }
    }
    std::string randomSalt() override {
        char s[33];
        for (int i = 0; i < 16; ++i)
            std::snprintf(s + i * 2, 3, "%02x", unsigned(esp_random() & 255));
        return s;
    }
    std::string pinHash(const std::string &pin, const std::string &salt) override {
        if (pin.size() > 8 || salt.size() != 32)
            return {};
        uint8_t bytes[32];
        if (mbedtls_pkcs5_pbkdf2_hmac_ext(
                MBEDTLS_MD_SHA256, reinterpret_cast<const unsigned char *>(pin.data()), pin.size(),
                reinterpret_cast<const unsigned char *>(salt.data()), salt.size(), 10000, 32,
                bytes))
            return {};
        char hash[65];
        for (int i = 0; i < 32; ++i)
            std::snprintf(hash + i * 2, 3, "%02x", bytes[i]);
        return hash;
    }
    void restoreClock() {
        if (M5.Rtc.isEnabled() && !M5.Rtc.getVoltLow()) {
            m5::rtc_datetime_t rtc;
            if (M5.Rtc.getDateTime(&rtc)) {
                pda::CivilMinute t{rtc.date.year, rtc.date.month, rtc.date.date, rtc.time.hours,
                                   rtc.time.minutes},
                    check;
                if (pda::parseCivilMinute(pda::formatCivilMinute(t), check) &&
                    rtc.time.seconds >= 0 && rtc.time.seconds <= 59) {
                    epoch_ = pda::civilToUnix(t, 0) + rtc.time.seconds;
                    startUs_ = esp_timer_get_time();
                    clockValid_ = true;
                    source_ = "RTC RX8130CE (UTC)";
                }
            }
        }
        if (!pda::value(service.state().settings.fields, "wifiSSID").empty())
            syncTime();
    }
    void syncTime() override {
        startNetwork(NetworkOperation::Sync);
    }
    void scanNetworks() override {
        startNetwork(NetworkOperation::Scan);
    }
    void connectNetwork() override {
        startNetwork(NetworkOperation::Connect);
    }
    void disconnectNetwork() override {
        startNetwork(NetworkOperation::Disconnect);
    }
    std::vector<WifiNetwork> networks() const override {
        return networks_;
    }
    std::string networkStatus() const override {
        return network_;
    }
    bool picture(const std::string &name, std::vector<uint16_t> &pixels,
                 std::string &error) override {
        if (!safeName(name)) {
            error = "Nome de imagem inválido";
            return false;
        }
        std::string data;
        if (!service.readFile(name, data)) {
            error = service.error();
            return false;
        }
        unsigned w, h;
        if (!imageDimensions(data, w, h)) {
            error = "Imagem inválida ou acima de 4096 × 4096";
            return false;
        }
        if (data.size() > 4 * 1024 * 1024) {
            error = "Imagem maior que 4 MiB";
            return false;
        }
        M5Canvas canvas(&M5.Display);
        canvas.setPsram(true);
        canvas.setColorDepth(16);
        if (!canvas.createSprite(900, 420)) {
            error = "Memória insuficiente para imagem";
            return false;
        }
        canvas.fillScreen(TFT_BLACK);
        float scale = std::min(900.0f / w, 420.0f / h);
        scale = std::min(1.0f, scale);
        int x = (900 - int(w * scale)) / 2, y = (420 - int(h * scale)) / 2;
        auto *bytes = reinterpret_cast<const uint8_t *>(data.data());
        bool ok = data.size() > 2 && data[0] == 'B' && data[1] == 'M'
                      ? canvas.drawBmp(bytes, data.size(), x, y, 900, 420, 0, 0, scale)
                  : data.size() > 1 && static_cast<unsigned char>(data[0]) == 0x89
                      ? canvas.drawPng(bytes, data.size(), x, y, 900, 420, 0, 0, scale)
                      : canvas.drawJpg(bytes, data.size(), x, y, 900, 420, 0, 0, scale);
        if (!ok) {
            error = "Formato de imagem não suportado pelo decodificador";
            return false;
        }
        std::vector<uint16_t> next(900 * 420);
        for (int yy = 0; yy < 420; ++yy)
            for (int xx = 0; xx < 900; ++xx) {
                uint32_t rgb = canvas.readPixel(xx, yy);
                next[yy * 900 + xx] =
                    ((rgb >> 19) & 31) << 11 | ((rgb >> 10) & 63) << 5 | ((rgb >> 3) & 31);
            }
        pixels = std::move(next);
        error.clear();
        return true;
    }
    std::vector<FileEntry> files() const override {
        return sdFiles.list();
    }
    bool removeFile(const std::string &name) override {
        return sdFiles.remove(name);
    }
    bool audioRecord(const std::string &name) override {
        return extras_.record(name);
    }
    bool audioPlay(const std::string &name) override {
        return extras_.play(name);
    }
    void audioStop() override {
        extras_.stop();
    }
    std::string audioStatus() const override {
        return extras_.audioStatus();
    }
    bool audioFinished(std::string &name) override {
        return extras_.finished(name);
    }
    std::string transfer(bool enabled) override {
        return extras_.sharing(enabled, randomSalt().substr(0, 12));
    }
    bool transferActive() const override {
        return extras_.active();
    }
    void closeExternal() override {
        extras_.close();
    }
    bool askAssistant(const std::string &url, const std::string &token,
                      const std::string &prompt) override {
        return extras_.ask(url, token, prompt, localMinute());
    }
    bool assistantResult(std::string &text, std::string &error) override {
        return extras_.answer(text, error);
    }
    void tickExtras() {
        extras_.tick();
    }
    bool networkBusy() const override {
        return networkPhase_.load() != 0;
    }
    void tickNetwork() {
        auto phase = networkPhase_.load(std::memory_order_acquire);
        if (phase == 1)
            network_ = millis() - networkAt_ > 30000 ? "Inicialização do C6 demorando; PDA ativo"
                                                     : "Inicializando rádio C6...";
        if (phase == 2)
            network_ = "Conectando Wi-Fi...";
        if (phase == 3)
            network_ = "Sincronizando NTP...";
        if (phase == 4) {
            epoch_ = networkEpoch_;
            startUs_ = networkStartUs_;
            clockValid_ = true;
            source_ = "NTP / C6 SDIO";
            writeRtc();
            network_ = networkConnected_
                           ? "Conectado a " + connectedSSID_ + " / hora NTP sincronizada"
                           : "Hora NTP sincronizada / rádio desligado";
            networkPhase_.store(0);
        }
        if (phase == 5) {
            network_ = networkConnected_ ? "Wi-Fi conectado; NTP indisponível"
                                         : "Wi-Fi indisponível: confira rede e senha";
            networkPhase_.store(0);
        }
        if (phase == 6) {
            networks_ = std::move(scanResults_);
            network_ = networks_.empty()
                           ? "Nenhuma rede encontrada; tente buscar novamente"
                           : std::to_string(networks_.size()) + " redes encontradas; selecione uma";
            networkPhase_.store(0);
        }
        if (phase == 7) {
            network_ = "Conectado a " + connectedSSID_ + " / IP " + connectedIP_;
            networkPhase_.store(0);
        }
        if (phase == 8) {
            network_ = "Wi-Fi desconectado / rádio desligado";
            networkPhase_.store(0);
        }
        if (phase == 10) {
            network_ = "Falha na busca Wi-Fi; tente novamente";
            networkPhase_.store(0);
        }
        if (phase == 9)
            network_ = "Buscando redes Wi-Fi...";
        if (phase == 0 && networkConnected_ && WiFi.status() != WL_CONNECTED) {
            networkConnected_ = false;
            network_ = "Conexão Wi-Fi perdida; toque Conectar para tentar novamente";
        }
    }
    std::string diagnostics() const override {
        char s[600];
        std::snprintf(s, sizeof(s),
                      "Tab5PDA 1.4.0\nPlaca: ESP32-P4 / rev %u\nTela: %s / 1280 x 720\nTouch: "
                      "%s\nLVGL 8.3.11 / Montserrat 16 px\nPSRAM: %u MiB / livre %u bytes\nHeap "
                      "interno livre: %u bytes\nmicroSD SPI: %llu MiB\nRTC: %s\nHora: %s\nÁudio "
                      "ES8388: %s\nBateria: %d mV\nWi-Fi: %s",
                      ESP.getChipRevision(), displayModel_.c_str(),
                      touch_ ? "driver disponível (validar em bancada)" : "indisponível",
                      unsigned(ESP.getPsramSize() / 1048576), unsigned(ESP.getFreePsram()),
                      unsigned(ESP.getFreeHeap()),
                      static_cast<unsigned long long>(sdFiles.cardBytes() / 1048576),
                      M5.Rtc.isEnabled() ? "RX8130CE detectado" : "indisponível",
                      clockValid_ ? source_.c_str() : "ajuste necessário",
                      M5.Speaker.isEnabled() ? "disponível" : "indisponível",
                      M5.Power.getBatteryVoltage(), network_.c_str());
        return std::string(s) + "\nTeclado I2C 0x6D: " +
               (keyboardReady ? "conectado / HID" : "ausente ou indisponível");
    }
    void activity() {
        lastActivity_ = millis();
        if (sleeping_) {
            sleeping_ = false;
            applySettings(service.state().settings);
        }
    }
    void tickPower() {
        auto s = service.state().settings.screenTimeoutSeconds;
        if (!sleeping_ && s && millis() - lastActivity_ >= uint32_t(s) * 1000) {
            sleeping_ = true;
            M5.Display.setBrightness(0);
        }
    }
    bool sleeping() const {
        return sleeping_;
    }
    bool touch() const {
        return touch_;
    }

  private:
    DeviceExtras extras_{sdFiles, service};
    enum class NetworkOperation { Sync, Scan, Connect, Disconnect };
    struct NetworkJob {
        Platform *owner;
        std::string ssid, password;
        NetworkOperation operation;
        bool keepConnected;
    };
    void startNetwork(NetworkOperation operation) {
        if (networkBusy()) {
            network_ = "Aguarde a operação Wi-Fi em andamento";
            return;
        }
        auto ssid = pda::value(service.state().settings.fields, "wifiSSID");
        if ((operation == NetworkOperation::Sync || operation == NetworkOperation::Connect) &&
            ssid.empty()) {
            network_ = "Selecione ou configure uma rede primeiro";
            return;
        }
        auto *job =
            new NetworkJob{this, ssid, pda::value(service.state().settings.fields, "wifiPass"),
                           operation, operation == NetworkOperation::Connect || networkConnected_};
        networkAt_ = millis();
        networkPhase_.store(1);
        network_ = "Inicializando rádio C6...";
        if (xTaskCreatePinnedToCore(networkWorker, "PDA_WIFI", 8192, job, 1, nullptr, 0) !=
            pdPASS) {
            delete job;
            networkPhase_.store(0);
            network_ = "Não foi possível iniciar Wi-Fi";
        }
    }
    static void networkWorker(void *argument) {
        auto *owner = static_cast<NetworkJob *>(argument)->owner;
        int result = 5;
        {
            std::unique_ptr<NetworkJob> job(static_cast<NetworkJob *>(argument));
            uint32_t started = millis();
            WiFi.persistent(false);
            WiFi.setPins(12, 13, 11, 10, 9, 8, 15);
            if (job->operation == NetworkOperation::Disconnect) {
                esp_sntp_stop();
                WiFi.disconnect(true, false);
                WiFi.mode(WIFI_OFF);
                owner->networkConnected_ = false;
                result = 8;
            } else {
                WiFi.mode(WIFI_STA);
                WiFi.setAutoReconnect(false);
                if (job->operation == NetworkOperation::Scan) {
                    owner->networkPhase_.store(9);
                    WiFi.setScanTimeout(10000);
                    int n = WiFi.scanNetworks(false, false, false, 200);
                    owner->scanResults_.clear();
                    for (int i = 0; i < n && owner->scanResults_.size() < 32; ++i) {
                        std::string ssid = WiFi.SSID(i).c_str();
                        if (ssid.empty() || ssid.size() > 32 || !pda::validUtf8(ssid))
                            continue;
                        auto found = std::find_if(
                            owner->scanResults_.begin(), owner->scanResults_.end(),
                            [&](const WifiNetwork &network) { return network.ssid == ssid; });
                        WifiNetwork network{ssid, WiFi.RSSI(i),
                                            WiFi.encryptionType(i) != WIFI_AUTH_OPEN};
                        if (found == owner->scanResults_.end())
                            owner->scanResults_.push_back(network);
                        else if (network.rssi > found->rssi)
                            *found = network;
                    }
                    std::sort(
                        owner->scanResults_.begin(), owner->scanResults_.end(),
                        [](const WifiNetwork &a, const WifiNetwork &b) { return a.rssi > b.rssi; });
                    WiFi.scanDelete();
                    result = n < 0 ? 10 : 6;
                } else {
                    owner->networkPhase_.store(2);
                    if (WiFi.status() != WL_CONNECTED || WiFi.SSID() != job->ssid.c_str()) {
                        WiFi.disconnect(false, false);
                        WiFi.begin(job->ssid.c_str(), job->password.c_str());
                    }
                    while (WiFi.status() != WL_CONNECTED && millis() - started < 30000)
                        vTaskDelay(pdMS_TO_TICKS(100));
                    if (WiFi.status() == WL_CONNECTED) {
                        owner->connectedSSID_ = job->ssid;
                        owner->connectedIP_ = WiFi.localIP().toString().c_str();
                        result = 7;
                        if (job->operation == NetworkOperation::Sync) {
                            result = 5;
                            sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
                            configTime(0, 0, "pool.ntp.org", "time.google.com");
                            owner->networkPhase_.store(3);
                            while (millis() - started < 30000) {
                                if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
                                    auto epoch = std::time(nullptr);
                                    auto t = pda::unixToCivil(epoch, 0);
                                    if (t.year >= 2000 && t.year <= 2099) {
                                        owner->networkEpoch_ = epoch;
                                        owner->networkStartUs_ = esp_timer_get_time();
                                        result = 4;
                                    }
                                    break;
                                }
                                vTaskDelay(pdMS_TO_TICKS(100));
                            }
                            esp_sntp_stop();
                        }
                    }
                }
                bool connected = WiFi.status() == WL_CONNECTED;
                owner->networkConnected_ = job->keepConnected && connected;
                if (!job->keepConnected || !connected) {
                    WiFi.disconnect(true, false);
                    WiFi.mode(WIFI_OFF);
                }
            }
        }
        // Publish all worker results before the UI thread reads them. No LVGL/SD/RTC here.
        owner->networkPhase_.store(result, std::memory_order_release);
        vTaskDelete(nullptr);
    }
    void writeRtc() {
        if (!clockValid_ || !M5.Rtc.isEnabled())
            return;
        auto t = pda::unixToCivil(epoch_ + (esp_timer_get_time() - startUs_) / 1000000, 0);
        m5::rtc_date_t date(t.year, t.month, t.day);
        m5::rtc_time_t time(t.hour, t.minute, 0);
        M5.Rtc.setDateTime(&date, &time);
    }
    bool clockValid_ = false, sleeping_ = false, touch_ = false;
    uint32_t lastActivity_ = 0, networkAt_ = 0;
    std::atomic<int> networkPhase_{0};
    std::atomic<bool> networkConnected_{false};
    std::vector<WifiNetwork> networks_, scanResults_;
    std::string connectedSSID_, connectedIP_;
    int64_t networkEpoch_ = 0;
    int64_t networkStartUs_ = 0;
    int64_t epoch_ = 0, startUs_ = 0;
    std::string source_ = "desconhecida", network_ = "Wi-Fi desligado",
                displayModel_ = "desconhecido";
} platform;
Ui ui(service, platform);
lv_disp_draw_buf_t drawBuffer;
lv_disp_drv_t display;
lv_indev_drv_t pointer;
lv_color_t *pixelsA = nullptr, *pixelsB = nullptr;
void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels) {
    int w = area->x2 - area->x1 + 1, h = area->y2 - area->y1 + 1;
    M5.Display.startWrite();
    // writeImage transforms dirty bounds to native panel coordinates before cache writeback.
    // The pinned framebuffer writePixels path does not do this correctly when rotated.
    M5.Display.pushImage(area->x1, area->y1, w, h, reinterpret_cast<lgfx::rgb565_t *>(pixels));
    M5.Display.endWrite();
    lv_disp_flush_ready(driver);
}
void readTouch(lv_indev_drv_t *, lv_indev_data_t *data) {
    static bool consumed = false;
    static lv_point_t last{0, 0};
    int32_t x = 0, y = 0;
    bool pressed = false;
    if (M5.Touch.getCount()) {
        const auto &point = M5.Touch.getDetail(0);
        pressed = point.isPressed();
        x = point.x;
        y = point.y;
    }
    if (!pressed)
        consumed = false;
    else {
        if (platform.sleeping()) {
            platform.activity();
            consumed = true;
        } else
            platform.activity();
        ui.activity();
    }
    if (pressed && !consumed && x >= 0 && x < 1280 && y >= 0 && y < 720) {
        last.x = x;
        last.y = y;
        data->state = LV_INDEV_STATE_PR;
    } else
        data->state = LV_INDEV_STATE_REL;
    data->point = last;
}
void tickUsb() {
    static std::string command, payload;
    static std::size_t expected = 0, sent = 0;
    static uint32_t expectedCrc = 0, last = 0;
    static bool receiving = false, sending = false;
    auto reset = [&] {
        command.clear();
        std::string().swap(payload);
        expected = sent = 0;
        receiving = sending = false;
    };
    if (!ui.usbAllowed()) {
        reset();
        for (int i = 0; i < 128 && Serial.available(); ++i)
            Serial.read();
        return;
    }
    if ((receiving || sending) && millis() - last > 15000) {
        reset();
        Serial.println("PDA ERROR timeout");
    }
    if (sending) {
        auto remaining = payload.size() - sent;
        int available = Serial.availableForWrite();
        if (available > 0) {
            auto count = std::min<std::size_t>(remaining, std::min(available, 128));
            sent += Serial.write(reinterpret_cast<const uint8_t *>(payload.data() + sent), count);
            last = millis();
        }
        if (sent == payload.size()) {
            reset();
            Serial.print("\nPDA END\n");
        }
        return;
    }
    for (int i = 0; i < 128 && Serial.available(); ++i) {
        char c = Serial.read();
        last = millis();
        if (receiving) {
            payload += c;
            if (payload.size() == expected) {
                pda::State candidate;
                std::string error;
                if (pda::crc32(reinterpret_cast<const uint8_t *>(payload.data()), payload.size()) !=
                        expectedCrc ||
                    !pda::importJsonl(payload, candidate, error))
                    Serial.println("PDA ERROR invalid");
                else {
                    candidate = pda::State{};
                    std::vector<uint8_t> data(payload.begin(), payload.end()), verify;
                    std::string().swap(payload);
                    const char *staged = "/CardputerPDA/incoming.jsonl";
                    if (!sdFiles.write(staged, data) ||
                        sdFiles.read(staged, verify) != pda::ReadResult::Ok || verify != data)
                        Serial.println("PDA ERROR storage");
                    else
                        Serial.println("PDA STAGED"); // The UI confirms replacement separately.
                }
                reset();
            }
        } else if (c == '\n') {
            if (command == "PDA GET") {
                std::string error;
                payload = pda::exportJsonl(service.state(), error);
                if (payload.empty())
                    Serial.println("PDA ERROR export");
                else {
                    Serial.printf("PDA DATA %u %08x\n", static_cast<unsigned>(payload.size()),
                                  pda::crc32(reinterpret_cast<const uint8_t *>(payload.data()),
                                             payload.size()));
                    sending = true;
                    sent = 0;
                }
            } else {
                unsigned size = 0, crc = 0;
                int end = 0;
                if (std::sscanf(command.c_str(), "PDA PUT %u %x%n", &size, &crc, &end) == 2 &&
                    end == static_cast<int>(command.size()) && size > 0 &&
                    size <= pda::kMaxSnapshotBytes && service.writable()) {
                    expected = size;
                    expectedCrc = crc;
                    payload.clear();
                    payload.reserve(size);
                    receiving = true;
                    Serial.println("PDA READY");
                } else
                    Serial.println("PDA ERROR command");
            }
            command.clear();
            if (sending)
                break;
        } else if (c != '\r') {
            if (command.size() < 80)
                command += c;
            else {
                reset();
                Serial.println("PDA ERROR command");
            }
        }
    }
}
// Dedicated keyboard connector: GPIO0/1, independent of internal I2C (port 1).
// Use the SDK external bus on port 0; no Arduino Wire/legacy driver is installed.
uint32_t keyboardPoll = 0;
void tickKeyboard() {
    if (!keyboardBusReady)
        return;
    uint32_t now = millis();
    if (now - keyboardPoll < (keyboardReady ? 20u : 1000u))
        return;
    keyboardPoll = now;
    auto &bus = M5.Ex_I2C;
    constexpr uint8_t addr = 0x6d;
    constexpr uint32_t freq = 100000;
    if (!keyboardReady) {
        uint8_t version = 0;
        if (!bus.readRegister(addr, 0xfe, &version, 1, freq))
            return;
        keyboardReady = bus.writeRegister8(addr, 0x10, 1, freq) && // HID mode
                        bus.writeRegister8(addr, 0x00, 2, freq) && // HID interrupt bit
                        bus.writeRegister8(addr, 0x02, 0, freq) && // Clear stale events
                        bus.writeRegister8(addr, 0x01, 0, freq);
        return;
    }
    uint8_t count = 0;
    if (!bus.readRegister(addr, 0x02, &count, 1, freq)) {
        keyboardReady = false;
        return;
    }
    // Bound work per loop so keyboard bursts cannot block touch/alarms.
    for (unsigned i = 0; i < std::min<unsigned>(count, 8); ++i) {
        uint8_t event[2];
        if (!bus.readRegister(addr, 0x30, event, 2, freq)) {
            keyboardReady = false;
            break;
        }
        if (event[1] < 4 || event[1] == 255)
            continue;
        if (platform.sleeping()) {
            platform.activity();
            ui.activity();
            continue;
        }
        platform.activity();
        ui.physicalKey(event[0], event[1]);
    }
}
bool ready = false;
uint32_t lastTick = 0;
} // namespace
void setup() {
    Serial.begin(115200);
    if (!platform.begin()) {
        M5.Display.setTextSize(2);
        M5.Display.println("Tab5PDA: placa/tela/PSRAM incompatível");
        Serial.println("Tab5PDA: inicialização interrompida");
        return;
    }
    M5.Ex_I2C.release();
    keyboardBusReady = M5.Ex_I2C.begin(I2C_NUM_0, 0, 1);
    heap_caps_malloc_extmem_enable(64); // Large STL buffers use PSRAM.
    if (platform.remount())
        service.load();
    platform.applySettings(service.state().settings);
    platform.restoreClock();
    platform.activity();
    pixelsA = static_cast<lv_color_t *>(
        heap_caps_malloc(1280 * 32 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    pixelsB = static_cast<lv_color_t *>(
        heap_caps_malloc(1280 * 32 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!pixelsA || !pixelsB) {
        M5.Display.println("Tab5PDA: falta memória para a interface");
        return;
    }
    lv_init();
    lv_disp_draw_buf_init(&drawBuffer, pixelsA, pixelsB, 1280 * 32);
    lv_disp_drv_init(&display);
    display.hor_res = 1280;
    display.ver_res = 720;
    display.draw_buf = &drawBuffer;
    display.flush_cb = flush;
    lv_disp_drv_register(&display);
    lv_indev_drv_init(&pointer);
    pointer.type = LV_INDEV_TYPE_POINTER;
    pointer.read_cb = readTouch;
    lv_indev_drv_register(&pointer);
    ui.begin();
    if (!platform.touch())
        ui.dispatch(Command::App, 16);
    // Paint a complete first frame before entering the incremental refresh loop.
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(nullptr);
    lastTick = millis();
    ready = true;
    Serial.println("Tab5PDA 1.4.0 / dados no microSD");
}
void loop() {
    if (!ready) {
        delay(20);
        return;
    }
    M5.update();
    platform.tickNetwork();
    platform.tickExtras();
    uint32_t now = millis();
    lv_tick_inc(now - lastTick);
    lastTick = now;
    lv_timer_handler();
    ui.tick();
    tickKeyboard();
    tickUsb();
    platform.tickPower();
    delay(2);
}
