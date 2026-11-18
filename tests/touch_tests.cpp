#include "memory_files.hpp"
#include "tab_ui.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace tab5_pda;
namespace {
std::vector<uint8_t> frame(1280 * 720 * 3);
lv_color_t pixels[1280 * 32];
lv_disp_draw_buf_t buffer;
lv_disp_drv_t display;
lv_indev_drv_t pointer;
struct Platform : IPlatform {
    uint32_t now = 1000;
    int notifications = 0, syncs = 0, clockAdjustments = 0, scans = 0, connections = 0,
        disconnections = 0;
    std::vector<WifiNetwork> visibleNetworks;
    std::string wifiStatus = "Wi-Fi simulado / NTP offline";
    bool busy = false;
    std::string minute = "2026-10-13 09:00";
    MemoryFiles *fileStore = nullptr;
    bool transferOn = false, closedExternal = false, hideFiles = false;
    bool hardwareKeyboard = false;
    bool physicalKeyboardConnected() const override {
        return hardwareKeyboard;
    }
    std::size_t picturePixels = 900 * 420;
    std::string lastPicture;
    std::string fakeAudio = "Áudio pronto", audioName, audioDone, assistantReply;
    std::vector<FileEntry> files() const override {
        std::vector<FileEntry> out;
        if (hideFiles)
            return out;
        if (fileStore)
            for (const auto &entry : fileStore->files) {
                auto slash = entry.first.find_last_of('/');
                out.push_back(
                    {entry.first.substr(slash + 1), static_cast<uint32_t>(entry.second.size())});
            }
        return out;
    }
    bool removeFile(const std::string &name) override {
        return fileStore && fileStore->files.erase("/CardputerPDA/" + name);
    }
    bool audioRecord(const std::string &name) override {
        audioName = name;
        fakeAudio = "Gravando: " + name;
        return true;
    }
    void audioStop() override {
        if (fakeAudio.rfind("Gravando:", 0) == 0) {
            audioDone = audioName;
            fakeAudio = "WAV salvo: " + audioName;
        }
    }
    bool audioFinished(std::string &name) override {
        if (audioDone.empty())
            return false;
        name = audioDone;
        audioDone.clear();
        return true;
    }
    bool audioPlay(const std::string &name) override {
        fakeAudio = "Reproduzindo: " + name;
        return true;
    }
    std::string audioStatus() const override {
        return fakeAudio;
    }
    std::string transfer(bool active) override {
        transferOn = active;
        return active ? "http://127.0.0.1:8080 Código: teste" : "Sessão encerrada";
    }
    bool transferActive() const override {
        return transferOn;
    }
    void closeExternal() override {
        closedExternal = true;
        transferOn = false;
        assistantReply.clear();
        audioStop();
    }
    bool askAssistant(const std::string &, const std::string &, const std::string &) override {
        return true;
    }
    bool assistantResult(std::string &text, std::string &) override {
        if (assistantReply.empty())
            return false;
        text = assistantReply;
        assistantReply.clear();
        return true;
    }
    bool picture(const std::string &name, std::vector<uint16_t> &pixels, std::string &) override {
        lastPicture = name;
        pixels.assign(picturePixels, name == "zzz-photo.bmp" ? 0xf800 : 0x001f);
        return true;
    }
    uint32_t nowMs() const override {
        return now;
    }
    std::string localMinute() const override {
        return minute;
    }
    std::string diagnostics() const override {
        return "Tab5 / plataforma de teste\nSem drivers físicos\nLVGL 8.3.11 / fonte 16 px";
    }
    bool setClock(const std::string &s, int, std::string &) override {
        ++clockAdjustments;
        minute = s;
        return true;
    }
    bool remount() override {
        return true;
    }
    void applySettings(const pda::Settings &) override {}
    void notify() override {
        ++notifications;
    }
    std::string randomSalt() override {
        return std::string(32, '1');
    }
    std::string pinHash(const std::string &p, const std::string &) override {
        return std::string(64, p == "1234" ? 'a' : 'b');
    }
    void syncTime() override {
        ++syncs;
    }
    void scanNetworks() override {
        ++scans;
        wifiStatus = "Busca concluída";
        visibleNetworks = {{"Network", -42, true}, {"Open", -65, false}};
    }
    std::vector<WifiNetwork> networks() const override {
        return visibleNetworks;
    }
    void connectNetwork() override {
        ++connections;
        wifiStatus = "Conectado";
    }
    void disconnectNetwork() override {
        ++disconnections;
        wifiStatus = "Desconectado";
    }
    bool networkBusy() const override {
        return busy;
    }
    std::string networkStatus() const override {
        return wifiStatus;
    }
};
struct Touch {
    int x = 0, y = 0;
    bool down = false;
} touch;
void read(lv_indev_drv_t *, lv_indev_data_t *d) {
    d->point.x = touch.x;
    d->point.y = touch.y;
    d->state = touch.down ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
}
void flush(lv_disp_drv_t *d, const lv_area_t *a, lv_color_t *color) {
    for (int y = a->y1; y <= a->y2; ++y)
        for (int x = a->x1; x <= a->x2; ++x) {
            lv_color32_t c;
            c.full = lv_color_to32(*color++);
            auto n = (y * 1280 + x) * 3;
            frame[n] = c.ch.red;
            frame[n + 1] = c.ch.green;
            frame[n + 2] = c.ch.blue;
        }
    lv_disp_flush_ready(d);
}
void step(Ui &ui, Platform &p, int ms = 20) {
    p.now += ms;
    lv_tick_inc(ms);
    lv_timer_handler();
    ui.tick();
}
void tap(Ui &ui, Platform &p, int x, int y) {
    touch = {x, y, true};
    step(ui, p);
    touch.down = false;
    step(ui, p);
    step(ui, p);
}
lv_obj_t *find(lv_obj_t *o, const lv_obj_class_t *type) {
    if (lv_obj_check_type(o, type))
        return o;
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(o); ++i)
        if (auto *found = find(lv_obj_get_child(o, i), type))
            return found;
    return nullptr;
}
lv_obj_t *labelled(lv_obj_t *o, const std::string &caption) {
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(o); ++i) {
        auto *child = lv_obj_get_child(o, i);
        if (lv_obj_check_type(child, &lv_label_class) && caption == lv_label_get_text(child))
            return child;
        if (auto *found = labelled(child, caption))
            return found;
    }
    return nullptr;
}
lv_obj_t *field(const std::string &caption, int occurrence = 0) {
    auto *screen = lv_scr_act();
    std::vector<lv_obj_t *> stack{screen};
    while (!stack.empty()) {
        auto *o = stack.back();
        stack.pop_back();
        for (uint32_t i = 0; i < lv_obj_get_child_cnt(o); ++i) {
            auto *c = lv_obj_get_child(o, i);
            if (lv_obj_check_type(c, &lv_label_class) && caption == lv_label_get_text(c)) {
                auto *input = lv_obj_get_child(o, i + 1);
                if (lv_obj_check_type(input, &lv_textarea_class) && occurrence-- == 0)
                    return input;
            }
        }
        for (int i = int(lv_obj_get_child_cnt(o)) - 1; i >= 0; --i)
            stack.push_back(lv_obj_get_child(o, i));
    }
    return nullptr;
}
lv_obj_t *control(const std::string &caption) {
    auto *l = labelled(lv_scr_act(), caption);
    CHECK(l);
    auto *parent = lv_obj_get_parent(l);
    return lv_obj_get_child(parent, lv_obj_get_index(l) + 1);
}
void selectOption(const std::string &caption, unsigned option) {
    auto *o = control(caption);
    CHECK(lv_obj_check_type(o, &lv_dropdown_class));
    lv_dropdown_set_selected(o, option);
    lv_event_send(o, LV_EVENT_VALUE_CHANGED, nullptr);
}
void set(const std::string &caption, const std::string &value) {
    auto *o = field(caption);
    CHECK(o);
    lv_textarea_set_text(o, value.c_str());
}
lv_obj_t *documentArea() {
    auto *name = field("Nome do documento");
    CHECK(name);
    auto *parent = lv_obj_get_parent(name);
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(parent); ++i) {
        auto *o = lv_obj_get_child(parent, i);
        if (o != name && lv_obj_check_type(o, &lv_textarea_class))
            return o;
    }
    throw std::runtime_error("Document editor not found");
}
void click(Ui &ui, Platform &p, const std::string &caption) {
    auto *l = labelled(lv_scr_act(), caption);
    if (!l)
        throw std::runtime_error("Button not found: " + caption);
    auto *b = lv_obj_get_parent(l);
    CHECK(lv_obj_check_type(b, &lv_btn_class));
    lv_obj_update_layout(lv_scr_act());
    lv_area_t a;
    lv_obj_get_coords(b, &a);
    tap(ui, p, (a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2);
}
void screenshot(const std::filesystem::path &path, Ui &ui, Platform &p) {
    step(ui, p);
    lv_refr_now(nullptr);
    std::ofstream out(path, std::ios::binary);
    out << "P6\n1280 720\n255\n";
    out.write(reinterpret_cast<const char *>(frame.data()), frame.size());
}
void assertForm(pda::RecordKind kind) {
    for (const auto &f : pda::recordFields(kind)) {
        auto *o = field(f.label);
        CHECK(o);
        lv_obj_update_layout(lv_scr_act());
        lv_area_t a;
        lv_obj_get_coords(o, &a);
        CHECK(a.x1 >= 280 && a.x2 < 1280 && a.y1 >= 56 && a.y2 < 680);
        CHECK(lv_obj_get_style_text_font(o, LV_PART_MAIN) == &pda_font_16);
    }
    CHECK(!find(lv_scr_act(), &lv_keyboard_class));
}
void key(Ui &ui, Platform &p, const std::string &text) {
    auto *kb = find(lv_scr_act(), &lv_keyboard_class);
    CHECK(kb);
    for (uint16_t i = 0; i < 56; ++i) {
        auto *label = lv_btnmatrix_get_btn_text(kb, i);
        if (label && text == label) {
            lv_btnmatrix_set_selected_btn(kb, i);
            lv_event_send(kb, LV_EVENT_VALUE_CHANGED, nullptr);
            ui.tick();
            step(ui, p);
            return;
        }
    }
    CHECK(false);
}
} // namespace
int main(int argc, char **argv) {
    try {
        std::filesystem::path out = argc > 1 ? argv[1] : "previews";
        std::filesystem::create_directories(out);
        lv_init();
        lv_disp_draw_buf_init(&buffer, pixels, nullptr, 1280 * 32);
        lv_disp_drv_init(&display);
        display.hor_res = 1280;
        display.ver_res = 720;
        display.draw_buf = &buffer;
        display.flush_cb = flush;
        lv_disp_drv_register(&display);
        lv_indev_drv_init(&pointer);
        pointer.type = LV_INDEV_TYPE_POINTER;
        pointer.read_cb = read;
        lv_indev_drv_register(&pointer);
        MemoryFiles files;
        pda::SnapshotStore store(files);
        pda::Service service(store);
        CHECK(service.load());
        Platform p;
        Ui ui(service, p);
        ui.begin();
        step(ui, p);
        for (const auto &name : std::vector<std::string>{
                 "Hoje", "Contatos", "Agenda", "Tarefas", "Notas", "Aniversários", "Despesas",
                 "Calculadora", "Conversores", "Relógios", "Calendário", "Busca geral", "Backup",
                 "Ajustes", "Wi-Fi / hora", "Bloqueio", "Diagnóstico"}) {
            auto *l = labelled(lv_scr_act(), name);
            CHECK(l);
            lv_area_t a;
            lv_obj_get_coords(lv_obj_get_parent(l), &a);
            CHECK(a.y1 >= 56 && a.y2 < 680);
        }
        screenshot(out / "today.ppm", ui, p);
        click(ui, p, "Despesas");
        click(ui, p, "Novo");
        set("Descrição", "Rascunho anterior de despesa");
        click(ui, p, "Contatos");
        click(ui, p, "Novo");
        CHECK(labelled(lv_scr_act(), "Rascunho de Despesas"));
        click(ui, p, "Retomar Despesas");
        CHECK(field("Valor: 123.45 (sem separador de milhar)"));
        CHECK(std::string(lv_textarea_get_text(field("Descrição"))) ==
              "Rascunho anterior de despesa");
        click(ui, p, "Contatos");
        click(ui, p, "Novo");
        click(ui, p, "Descartar e criar Contatos");
        assertForm(pda::RecordKind::Contact);
        // Real HID reports from the physical connector feed the same LVGL textareas.
        ui.physicalKey(0, 4);  // a
        ui.physicalKey(2, 5);  // B (Aa/Shift)
        ui.physicalKey(2, 30); // ! (Sym)
        CHECK(std::string(lv_textarea_get_text(field("Nome"))) == "AB!");
        ui.physicalKey(0, 0);    // release must not repeat
        ui.physicalKey(0, 0x50); // left
        ui.physicalKey(0, 0x2a); // backspace removes B
        CHECK(std::string(lv_textarea_get_text(field("Nome"))) == "A!");
        ui.physicalKey(0, 0x4c); // delete forward removes !
        CHECK(std::string(lv_textarea_get_text(field("Nome"))) == "A");
        ui.physicalKey(0, 0x2b); // Tab focuses Empresa
        ui.physicalKey(0, 6);
        CHECK(std::string(lv_textarea_get_text(field("Empresa"))) == "C");
        ui.physicalKey(2, 0x2b); // Shift+Tab returns to Nome
        ui.physicalKey(0, 7);
        CHECK(std::string(lv_textarea_get_text(field("Nome"))) == "Ad");
        CHECK(!find(lv_scr_act(), &lv_keyboard_class));
        assertForm(pda::RecordKind::Contact);
        set("Nome", "João");
        set("Empresa", "Empresa local");
        set("Telefones", "65 99999-1234");
        set("E-mail", "joao@example.com");
        set("Endereço", "Cuiabá");
        set("Categoria", "Pessoal");
        set("Observações", "Contato importável pelo CardputerPDA");
        screenshot(out / "contact-full-form.ppm", ui, p);

        auto *phone = field("Telefones");
        lv_event_send(phone, LV_EVENT_CLICKED, nullptr);
        auto *kb = find(lv_scr_act(), &lv_keyboard_class);
        CHECK(kb);
        lv_obj_update_layout(lv_scr_act());
        lv_area_t keyboardArea;
        lv_obj_get_coords(kb, &keyboardArea);
        CHECK(keyboardArea.x1 == 280 && keyboardArea.y1 == 392 && keyboardArea.x2 == 1279 &&
              keyboardArea.y2 == 679);
        CHECK(lv_obj_get_height(lv_obj_get_parent(phone)) == 256);
        auto *matrix = reinterpret_cast<lv_btnmatrix_t *>(kb);
        auto keyArea = matrix->button_areas[0];
        tap(ui, p, keyboardArea.x1 + (keyArea.x1 + keyArea.x2) / 2,
            keyboardArea.y1 + (keyArea.y1 + keyArea.y2) / 2);

        CHECK(std::string(lv_textarea_get_text(phone)) == "65 99999-12341");
        key(ui, p, "Símbolos");
        key(ui, p, "_");
        key(ui, p, "Apagar");
        key(ui, p, "ABC");
        key(ui, p, "Shift");
        key(ui, p, "A");
        key(ui, p, "Apagar");
        key(ui, p, "Shift");
        key(ui, p, "Acentos");
        ui.dispatch(Command::InsertAccent, 0);
        key(ui, p, "Apagar");
        screenshot(out / "contact-touch-keyboard.ppm", ui, p);
        key(ui, p, "Fechar");
        CHECK(lv_obj_get_height(lv_obj_get_parent(phone)) == 542);
        CHECK(lv_obj_has_flag(kb, LV_OBJ_FLAG_HIDDEN));
        // A contact draft offered from another app must retain its original kind/fields.
        click(ui, p, "Despesas");
        click(ui, p, "Novo");
        CHECK(labelled(lv_scr_act(), "Rascunho de Contatos"));
        click(ui, p, "Retomar Contatos");
        CHECK(field("Nome") && !field("Valor: 123.45 (sem separador de milhar)"));
        CHECK(std::string(lv_textarea_get_text(field("Nome"))) == "João");
        click(ui, p, "Salvar");
        CHECK(service.state().records.size() == 1);
        CHECK(service.state().records[0].fields.at("email") == "joao@example.com");
        screenshot(out / "contacts.ppm", ui, p);
        click(ui, p, "Agenda");
        click(ui, p, "Novo");
        assertForm(pda::RecordKind::Appointment);
        set("Título", "Reunião");
        auto *dateInput = field("Início: AAAA-MM-DD HH:MM");
        lv_textarea_set_text(dateInput, "");
        lv_event_send(dateInput, LV_EVENT_CLICKED, nullptr);
        auto *numericKb = find(lv_scr_act(), &lv_keyboard_class);
        CHECK(numericKb && std::string(lv_btnmatrix_get_btn_text(numericKb, 0)) == "7");
        key(ui, p, "7"); // touch keypad feeds the same mask
        CHECK(std::string(lv_textarea_get_text(dateInput)) == "7");
        lv_textarea_set_text(dateInput, "");
        ui.physicalKey(0, 31);
        ui.physicalKey(0, 39);
        ui.physicalKey(0, 31);
        ui.physicalKey(0, 35); // 2026 via the physical keyboard
        lv_textarea_add_text(dateInput, "10131000");
        CHECK(std::string(lv_textarea_get_text(dateInput)) == "2026-10-13 10:00");
        ui.physicalKey(0, 4); // letters cannot enter a date
        CHECK(std::string(lv_textarea_get_text(dateInput)) == "2026-10-13 10:00");
        for (int i = 0; i < 20; ++i)
            lv_textarea_del_char(dateInput);
        CHECK(std::string(lv_textarea_get_text(dateInput)).empty());
        lv_textarea_add_text(dateInput, "20261");
        CHECK(std::string(lv_textarea_get_text(dateInput)) == "2026-1");
        lv_textarea_del_char(dateInput);
        CHECK(std::string(lv_textarea_get_text(dateInput)) == "2026");
        set("Início: AAAA-MM-DD HH:MM", "2026-10-13 10:00");
        set("Fim: AAAA-MM-DD HH:MM", "2026-10-13 11:00");
        set("Local", "Escritório");
        set("Categoria", "Trabalho");
        set("Aviso: minutos antes (-1 desliga)", "15");
        set("Repetir: none/daily/weekly/monthly/yearly", "weekly");
        set("Até: AAAA-MM-DD (opcional)", "2027-10-13");
        set("Dia inteiro: 0 não / 1 sim", "0");
        set("Observações", "Todos os campos visíveis ao mesmo tempo.");
        screenshot(out / "appointment-full-form.ppm", ui, p);
        set("Início: AAAA-MM-DD HH:MM", "2026-02-30 10:00");
        click(ui, p, "Salvar");
        CHECK(field("Início: AAAA-MM-DD HH:MM"));
        CHECK(service.state().records.size() ==
              1); // A syntactically masked date must still be real.
        set("Início: AAAA-MM-DD HH:MM", "2026-10-13 10:00");
        click(ui, p, "Salvar");
        CHECK(service.state().records.size() == 2);
        click(ui, p, "Editar ocorrência");
        lv_textarea_set_text(find(lv_scr_act(), &lv_textarea_class), "2026-10-20");
        click(ui, p, "Cancelar ocorrência");
        CHECK(pda::value(service.state().records[1].fields, "exceptions") == "2026-10-20");
        click(ui, p, "Calendário");
        screenshot(out / "calendar.ppm", ui, p);
        click(ui, p, "Próximo");
        click(ui, p, "Anterior");
        click(ui, p, "Notas");
        click(ui, p, "Novo");
        assertForm(pda::RecordKind::Note);
        set("Título", "Nota com espaço para texto");
        set("Categoria", "Pessoal");
        set("Texto / observações", "Uma nota multilinha\ncom acentos: ação, café e João.");
        screenshot(out / "note-full-form.ppm", ui, p);
        auto *note = field("Texto / observações");
        std::string paragraphs;
        for (int line = 0; line < 24; ++line)
            paragraphs += "Linha " + std::to_string(line) + ": texto com acentos.\n";
        lv_textarea_set_text(note, paragraphs.c_str());
        lv_textarea_set_cursor_pos(note, LV_TEXTAREA_CURSOR_LAST);
        lv_event_send(note, LV_EVENT_CLICKED, nullptr);
        step(ui, p);
        lv_obj_update_layout(lv_scr_act());
        lv_area_t noteBounds;
        lv_obj_get_coords(note, &noteBounds);
        CHECK(noteBounds.y1 >= 124 && noteBounds.y2 < 392);
        lv_point_t cursor;
        lv_label_get_letter_pos(lv_textarea_get_label(note), lv_textarea_get_cursor_pos(note),
                                &cursor);
        CHECK(cursor.y + pda_font_16.line_height - lv_obj_get_scroll_top(note) <=
              lv_obj_get_content_height(note) + 1);
        screenshot(out / "note-keyboard-caret.ppm", ui, p);
        key(ui, p, "Fechar");
        CHECK(lv_obj_get_height(note) > 350);

        step(ui, p, 1600);
        CHECK(service.state().draft.active);
        click(ui, p, "Voltar");
        click(ui, p, "Hoje");
        click(ui, p, "Retomar rascunho");
        click(ui, p, "Retomar edição");
        click(ui, p, "Salvar");
        CHECK(service.state().records.size() == 3);
        click(ui, p, "Tarefas");
        click(ui, p, "Novo");
        set("Título", "Teste touch");
        set("Prioridade: 1 alta / 2 média / 3 baixa", "1");
        click(ui, p, "Salvar");
        click(ui, p, "Concluir / reabrir");
        CHECK(service.state().records.back().completed);
        click(ui, p, "Despesas");
        click(ui, p, "Novo");
        assertForm(pda::RecordKind::Expense);
        set("Descrição", "Café");
        set("Data: AAAA-MM-DD HH:MM", "2026-10-13 08:00");
        auto *moneyInput = field("Valor: 123.45 (sem separador de milhar)");
        lv_textarea_set_text(moneyInput, "");
        lv_event_send(moneyInput, LV_EVENT_CLICKED, nullptr);
        lv_textarea_add_text(moneyInput, "12,30");
        lv_textarea_add_text(moneyInput,
                             "x9,"); // letters, third decimal and second separator are rejected
        CHECK(std::string(lv_textarea_get_text(moneyInput)) == "12,30");

        click(ui, p, "Salvar");
        CHECK(service.state().records.size() == 5);
        CHECK(service.state().records.back().fields.at("amount") == "12.30");
        click(ui, p, "Totais por período");
        set("Período: AAAA-MM, AAAA ou vazio", "2026-10");
        click(ui, p, "Salvar");
        screenshot(out / "expense-totals.ppm", ui, p);
        click(ui, p, "Calculadora");
        auto *calc = find(lv_scr_act(), &lv_textarea_class);
        lv_textarea_set_text(calc, "200*10%");
        ui.physicalKey(0, 0x28);
        CHECK(labelled(lv_scr_act(), "20"));
        click(ui, p, "Científica");
        click(ui, p, "M+");
        click(ui, p, "MC");
        screenshot(out / "calculator.ppm", ui, p);
        click(ui, p, "Conversores");
        ui.dispatch(Command::ConverterGroup, 3);
        set("Valor", "32");
        selectOption("Unidade de origem", 1);
        selectOption("Unidade de destino", 0);
        click(ui, p, "Converter");
        CHECK(labelled(lv_scr_act(), "0 C"));
        screenshot(out / "converters.ppm", ui, p);
        click(ui, p, "Busca geral");
        click(ui, p, "Buscar");
        lv_textarea_set_text(find(lv_scr_act(), &lv_textarea_class), "JOAO");
        ui.dispatch(Command::Search, 1);
        screenshot(out / "global-search.ppm", ui, p);
        click(ui, p, "Backup");
        click(ui, p, "Criar backup no microSD");
        CHECK(files.files.count("/CardputerPDA/backup.jsonl"));
        p.busy = true;
        click(ui, p, "Ativar sessão USB");
        CHECK(!ui.usbAllowed());
        p.busy = false;
        click(ui, p, "Ativar sessão USB");
        CHECK(ui.usbAllowed());
        screenshot(out / "backup.ppm", ui, p);
        click(ui, p, "Restaurar backup.jsonl");
        CHECK(!ui.usbAllowed());
        click(ui, p, "Cancelar");
        auto savedSettings = service.state().settings;
        savedSettings.screenTimeoutSeconds = 20;
        CHECK(service.saveSettings(savedSettings));
        click(ui, p, "Ajustes");
        CHECK(!field("Brilho: 16 a 255") && field("Hora manual: AAAA-MM-DD HH:MM"));
        ui.physicalKey(0, 0x2b); // first Tab goes from brightness to volume
        ui.physicalKey(0, 0x4f);
        CHECK(lv_slider_get_value(control("Volume: 0 a 255")) == 101);
        ui.physicalKey(0, 0x50);
        ui.physicalKey(2, 0x2b);
        auto *brightness = control("Brilho: 16 a 255");
        lv_obj_update_layout(lv_scr_act());
        lv_area_t sliderArea;
        lv_obj_get_coords(brightness, &sliderArea);
        tap(ui, p, sliderArea.x1 + (sliderArea.x2 - sliderArea.x1) * 3 / 4,
            (sliderArea.y1 + sliderArea.y2) / 2);
        CHECK(lv_slider_get_value(brightness) > 180 && lv_slider_get_value(brightness) < 210);
        CHECK(lv_obj_check_type(brightness, &lv_slider_class));
        lv_slider_set_value(brightness, 176, LV_ANIM_OFF);
        lv_event_send(brightness, LV_EVENT_VALUE_CHANGED, nullptr);
        ui.physicalKey(0, 0x4f); // keyboard can adjust a focused slider safely
        CHECK(lv_slider_get_value(brightness) == 177);
        ui.physicalKey(0, 0x50);
        auto *zone = control("Fuso UTC: horas");
        lv_area_t zoneArea;
        lv_obj_get_coords(zone, &zoneArea);
        tap(ui, p, (zoneArea.x1 + zoneArea.x2) / 2, (zoneArea.y1 + zoneArea.y2) / 2);
        CHECK(lv_dropdown_is_open(zone));
        screenshot(out / "settings-timezone-picker.ppm", ui, p);
        lv_dropdown_close(zone);
        selectOption("Fuso UTC: horas", 8); // UTC-4
        selectOption("Formato de data: ISO ou BR", 1);
        CHECK(!find(lv_scr_act(), &lv_keyboard_class));
        screenshot(out / "settings-full-form.ppm", ui, p);
        click(ui, p, "Salvar");
        CHECK(service.state().settings.brightness == 176);
        CHECK(service.state().settings.screenTimeoutSeconds == 20);
        CHECK(p.clockAdjustments == 0);
        CHECK(service.state().settings.utcOffsetMinutes == -240);
        click(ui, p, "Ajustes");
        CHECK(field("Hora manual: DD/MM/AAAA HH:MM"));
        CHECK(std::string(lv_textarea_get_text(field("Hora manual: DD/MM/AAAA HH:MM"))) ==
              dateToBR(p.minute));
        click(ui, p, "Salvar");
        CHECK(p.clockAdjustments == 0);
        click(ui, p, "Relógios");
        click(ui, p, "Editar cidades e fusos");
        selectOption("Fuso UTC: horas", 8);
        click(ui, p, "Salvar");
        CHECK(pda::value(service.state().settings.fields, "city0Offset") == "-240");
        click(ui, p, "Wi-Fi / hora");
        click(ui, p, "Buscar redes");
        CHECK(p.scans == 1);
        CHECK(labelled(lv_scr_act(), "Network\n-42 dBm  /  Protegida"));
        click(ui, p, "Network\n-42 dBm  /  Protegida");
        CHECK(std::string(lv_textarea_get_text(field("SSID Wi-Fi (vazio desliga)"))) == "Network");
        set("Senha Wi-Fi", "Secret123");
        screenshot(out / "wifi-select-and-connect.ppm", ui, p);
        click(ui, p, "Salvar e conectar");
        CHECK(p.connections == 1);
        CHECK(pda::value(service.state().settings.fields, "wifiSSID") == "Network");
        CHECK(pda::value(service.state().settings.fields, "wifiPass") == "Secret123");
        screenshot(out / "wifi-networks.ppm", ui, p);
        p.busy = true;
        click(ui, p, "Buscar redes");
        CHECK(p.scans == 1);
        click(ui, p, "Conectar");
        CHECK(p.connections == 1);
        p.busy = false;
        click(ui, p, "Desconectar");
        CHECK(p.disconnections == 1);

        click(ui, p, "Sincronizar hora NTP");
        CHECK(p.syncs == 1);
        click(ui, p, "Open\n-65 dBm  /  Aberta");
        CHECK(std::string(lv_textarea_get_text(field("Senha Wi-Fi"))).empty());
        set("SSID Wi-Fi (vazio desliga)", "Network");
        set("Senha Wi-Fi", "Texto mantido");
        p.wifiStatus = "Resultado de operação anterior";
        step(ui, p, 600);
        CHECK(std::string(lv_textarea_get_text(field("Senha Wi-Fi"))) == "Texto mantido");
        click(ui, p, "Cancelar");
        click(ui, p, "Despesas");
        click(ui, p, "Novo");
        set("Descrição", "Máscara brasileira");
        auto *brDate = field("Data: DD/MM/AAAA HH:MM");
        CHECK(brDate);
        lv_textarea_set_text(brDate, "");
        lv_textarea_add_text(brDate, "131020261430");
        CHECK(std::string(lv_textarea_get_text(brDate)) == "13/10/2026 14:30");
        set("Valor: 123,45 (sem separador de milhar)", "45,6");
        click(ui, p, "Salvar");
        CHECK(service.state().records.back().when == "2026-10-13 14:30");
        CHECK(service.state().records.back().fields.at("amount") == "45.60");
        click(ui, p, "Editar");
        CHECK(std::string(lv_textarea_get_text(field("Data: DD/MM/AAAA HH:MM"))) ==
              "13/10/2026 14:30");
        CHECK(std::string(lv_textarea_get_text(field("Valor: 123,45 (sem separador de milhar)"))) ==
              "45,60");
        screenshot(out / "expense-br-form.ppm", ui, p);
        click(ui, p, "Salvar");
        click(ui, p, "Agenda");
        click(ui, p, "Novo");
        set("Título", "Compromisso com data BR");
        set("Início: DD/MM/AAAA HH:MM", "311020261000");
        set("Repetir: none/daily/weekly/monthly/yearly", "monthly");
        set("Até: DD/MM/AAAA (opcional)", "31122027");
        CHECK(std::string(lv_textarea_get_text(field("Até: DD/MM/AAAA (opcional)"))) ==
              "31/12/2027");
        click(ui, p, "Salvar");
        CHECK(service.state().records.back().when == "2026-10-31 10:00");
        CHECK(service.state().records.back().fields.at("repeatEnd") == "2027-12-31");
        click(ui, p, "Bloqueio");
        click(ui, p, "Definir PIN");
        set("Novo PIN: 4 a 8 dígitos", "1234");
        selectOption("Bloqueio automático: segundos (0 desliga)", 4);
        click(ui, p, "Salvar");
        click(ui, p, "Bloquear agora");
        CHECK(ui.locked());
        ui.dispatch(Command::App, 12);
        CHECK(ui.locked() && !ui.usbAllowed());
        screenshot(out / "pin-lock.ppm", ui, p);
        lv_textarea_set_text(find(lv_scr_act(), &lv_textarea_class), "0000");
        ui.dispatch(Command::Unlock);
        CHECK(ui.locked());
        step(ui, p, 3100);
        lv_textarea_set_text(find(lv_scr_act(), &lv_textarea_class), "");
        ui.physicalKey(0, 30);
        ui.physicalKey(0, 31);
        ui.physicalKey(0, 32);
        ui.physicalKey(0, 33);
        ui.physicalKey(0, 0x28); // Enter unlocks without a touch event.

        CHECK(!ui.locked());
        step(ui, p, 120001);
        CHECK(ui.locked());
        lv_textarea_set_text(find(lv_scr_act(), &lv_textarea_class), "1234");
        ui.dispatch(Command::Unlock);
        CHECK(!ui.locked());
        click(ui, p, "Bloqueio");
        click(ui, p, "Remover PIN");
        click(ui, p, "Notas");
        click(ui, p, "Novo");
        set("Título", "Preservar durante lembrete");
        set("Texto / observações", "Texto em RAM");
        p.minute = "2026-10-13 09:45";
        step(ui, p, 1001);
        CHECK(p.notifications == 1);
        screenshot(out / "alarm.ppm", ui, p);
        click(ui, p, "Adiar 5 minutos");
        CHECK(field("Título"));
        CHECK(std::string(lv_textarea_get_text(field("Título"))) == "Preservar durante lembrete");
        click(ui, p, "Salvar");
        p.minute = "2026-10-13 09:50";
        step(ui, p, 1001);
        click(ui, p, "Dispensar");
        CHECK(pda::dueAlarms(service.state(), p.minute).empty());
        click(ui, p, "Notas");
        click(ui, p, "Novo");
        set("Título", "Salvar com falha de SD");
        files.writeFailure = true;
        click(ui, p, "Salvar");
        CHECK(field("Título"));
        CHECK(std::string(lv_textarea_get_text(field("Título"))) == "Salvar com falha de SD");
        screenshot(out / "sd-error.ppm", ui, p);
        files.writeFailure = false;
        click(ui, p, "Voltar");
        click(ui, p, "Diagnóstico");
        click(ui, p, "Reabrir microSD");
        click(ui, p, "Retomar rascunho");
        click(ui, p, "Retomar edição");
        click(ui, p, "Salvar");
        pda::SnapshotStore rebootStore(files);
        pda::Service reboot(rebootStore);
        CHECK(reboot.load());
        CHECK(reboot.state().records.size() == service.state().records.size());
        click(ui, p, "Notas");
        for (int i = 0; i < 40; ++i)
            CHECK(service.saveRecord({0, pda::RecordKind::Note, "Rolagem " + std::to_string(i),
                                      "Sem clique durante o gesto", "", false}));
        ui.dispatch(Command::App, 4);
        step(ui, p);
        auto detailText = [&]() {
            std::vector<lv_obj_t *> objects{lv_scr_act()};
            while (!objects.empty()) {
                auto *o = objects.back();
                objects.pop_back();
                if (lv_obj_check_type(o, &lv_label_class)) {
                    std::string text = lv_label_get_text(o);
                    if (text.rfind("Registro #", 0) == 0)
                        return text;
                }
                for (uint32_t i = 0; i < lv_obj_get_child_cnt(o); ++i)
                    objects.push_back(lv_obj_get_child(o, i));
            }
            return std::string{};
        };
        auto beforeScroll = detailText();
        CHECK(!beforeScroll.empty());
        touch = {460, 590, true};
        step(ui, p);
        for (int y = 560; y >= 250; y -= 30) {
            touch.y = y;
            step(ui, p);
        }
        touch.down = false;
        step(ui, p);
        step(ui, p);
        CHECK(detailText() == beforeScroll);
        screenshot(out / "scroll-without-selection.ppm", ui, p);
        std::cout << "Touch: 17 menus, full forms, 16px font, keyboard resizing, CRUD, PIN, "
                     "backups and alarms OK\n";
        // Independent form exercises, with real touch selection and SD reload.
        MemoryFiles selectionFiles;
        pda::SnapshotStore selectionStore(selectionFiles);
        pda::Service selectionService(selectionStore);
        CHECK(selectionService.load());
        auto settings = selectionService.state().settings;
        settings.fields["dateFormat"] = "BR";
        CHECK(selectionService.saveSettings(settings));
        Platform selectionPlatform;
        selectionPlatform.minute = "2028-01-31 23:59";
        Ui form(selectionService, selectionPlatform);
        form.begin();
        form.dispatch(Command::App, 2);
        form.dispatch(Command::New);
        set("Título", "Selecionado por toque");
        auto choose = [&](const std::string &caption) {
            auto *target = field(caption);
            CHECK(target);
            auto *button =
                lv_obj_get_child(lv_obj_get_parent(target), lv_obj_get_index(target) + 1);
            CHECK(lv_obj_check_type(button, &lv_btn_class));
            lv_obj_scroll_to_view(button, LV_ANIM_OFF);
            lv_obj_update_layout(lv_scr_act());
            lv_area_t a;
            lv_obj_get_coords(button, &a);
            tap(form, selectionPlatform, (a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2);
        };
        choose("Início: DD/MM/AAAA HH:MM");
        selectOption("Minuto", 45);
        click(form, selectionPlatform, "Mês +");
        click(form, selectionPlatform, "Ano +");
        click(form, selectionPlatform, "28");
        screenshot(out / "date-selector.ppm", form, selectionPlatform);
        click(form, selectionPlatform, "Aplicar data");
        CHECK(std::string(lv_textarea_get_text(field("Início: DD/MM/AAAA HH:MM"))) ==
              "28/02/2029 23:45");
        choose("Início: DD/MM/AAAA HH:MM");
        click(form, selectionPlatform, "Mês -");
        click(form, selectionPlatform, "Cancelar seleção");
        CHECK(std::string(lv_textarea_get_text(field("Início: DD/MM/AAAA HH:MM"))) ==
              "28/02/2029 23:45");
        choose("Fim: DD/MM/AAAA HH:MM");
        click(form, selectionPlatform, "Aplicar data");
        CHECK(std::string(lv_textarea_get_text(field("Fim: DD/MM/AAAA HH:MM"))) ==
              "28/02/2029 23:45");
        choose("Repetir: none/daily/weekly/monthly/yearly");
        screenshot(out / "option-selector.ppm", form, selectionPlatform);
        click(form, selectionPlatform, "Diariamente");
        choose("Até: DD/MM/AAAA (opcional)");
        click(form, selectionPlatform, "Aplicar data");
        CHECK(std::string(lv_textarea_get_text(field("Até: DD/MM/AAAA (opcional)"))) ==
              "28/02/2029");
        choose("Até: DD/MM/AAAA (opcional)");
        click(form, selectionPlatform, "Limpar campo");
        CHECK(std::string(lv_textarea_get_text(field("Até: DD/MM/AAAA (opcional)"))).empty());
        form.dispatch(Command::Save);
        CHECK(selectionService.state().records.size() == 1);
        CHECK(selectionService.state().records[0].when == "2029-02-28 23:45");
        CHECK(pda::value(selectionService.state().records[0].fields, "repeat") == "daily");
        pda::SnapshotStore selectionRebootStore(selectionFiles);
        pda::Service selectionReboot(selectionRebootStore);
        CHECK(selectionReboot.load());
        CHECK(selectionReboot.state().records[0].when == "2029-02-28 23:45");
        // Exercise the priority-1 screens with the LVGL pointer.
        selectionPlatform.minute = "2029-02-28 23:45";
        pda::Record late{0, pda::RecordKind::Task, "Comprar café", "", "2029-02-28 18:00", false};
        late.fields["priority"] = "1";
        CHECK(selectionService.saveRecord(late));
        form.dispatch(Command::App, 0);
        screenshot(out / "today-interactive.ppm", form, selectionPlatform);
        click(form, selectionPlatform, "[ ]");
        CHECK(selectionService.state().records[1].completed);
        click(form, selectionPlatform, "Nova tarefa");
        set("Título", "Projeto de casa");
        set("Projeto / lista", "Casa");
        form.dispatch(Command::Save);
        auto root = selectionService.state().nextId - 1;
        form.dispatch(Command::Select, root);
        click(form, selectionPlatform, "Nova subtarefa");
        CHECK(std::string(lv_textarea_get_text(field("Tarefa principal (ID opcional)"))) ==
              std::to_string(root));
        set("Título", "Comprar filtro");
        form.dispatch(Command::Save);
        form.dispatch(Command::Select, root);
        screenshot(out / "task-subtasks.ppm", form, selectionPlatform);
        tap(form, selectionPlatform, 950,
            210); // Detail checklist, not the list row with the same title.
        CHECK(selectionService.state().records.back().completed);
        pda::Record morning{0,  pda::RecordKind::Appointment, "Manhã",
                            "", "2029-02-28 09:00",           false};
        morning.fields["until"] = "2029-02-28 11:00";
        CHECK(selectionService.saveRecord(morning));
        auto morningId = selectionService.state().nextId - 1;
        form.dispatch(Command::App, 0);
        click(form, selectionPlatform, "Agenda semanal");
        screenshot(out / "weekly-schedule.ppm", form, selectionPlatform);
        click(form, selectionPlatform, "09:00 Manhã");
        click(form, selectionPlatform, "Reagendar");
        form.dispatch(Command::ScheduleSlot, 2 * 24 + 14); // Wednesday, 14:00.
        CHECK(labelled(lv_scr_act(), "Confirmar reagendamento"));
        click(form, selectionPlatform, "Confirmar reagendamento");
        CHECK(pda::findRecord(selectionService.state(), morningId)->when == "2029-02-28 14:00");
        form.dispatch(Command::ScheduleMode, 1);
        screenshot(out / "daily-schedule.ppm", form, selectionPlatform);
        form.dispatch(Command::OpenRecord, morningId);
        click(form, selectionPlatform, "Excluir");
        click(form, selectionPlatform, "Confirmar exclusão");
        CHECK(pda::trashed(*pda::findRecord(selectionService.state(), morningId)));
        form.dispatch(Command::App, 12);
        click(form, selectionPlatform, "Lixeira");
        screenshot(out / "trash.ppm", form, selectionPlatform);
        click(form, selectionPlatform, "Restaurar");
        CHECK(!pda::trashed(*pda::findRecord(selectionService.state(), morningId)));
        form.dispatch(Command::App, 12);
        click(form, selectionPlatform, "Importar / exportar");
        selectOption("Formato", 2);
        set("Nome do arquivo (na pasta /Tab5PDA)", "import.csv");
        CHECK(selectionService.writeFile("import.csv",
                                         "title,when,amount,currency,category,detail\r\nCafé,2029-"
                                         "02-28 10:00,4.50,BRL,Casa,Teste\r\n"));
        click(form, selectionPlatform, "Prévia da importação");
        screenshot(out / "import-preview.ppm", form, selectionPlatform);
        auto beforeImport = selectionService.state().records.size();
        click(form, selectionPlatform, "Confirmar importação");
        CHECK(selectionService.state().records.size() == beforeImport + 1);
        CHECK(selectionService.state().records.back().fields.at("amount") == "4.50");
        form.dispatch(Command::App, 0);
        step(form, selectionPlatform, 1000);
        CHECK(selectionFiles.files.count("/CardputerPDA/daily-2029-02-28.jsonl"));
        // The enlarged base uses pages rather than thousands of simultaneous widgets.
        MemoryFiles largeFiles;
        pda::SnapshotStore largeStore(largeFiles);
        pda::Service largeService(largeStore);
        CHECK(largeService.load());
        pda::State largeState;
        largeState.nextId = 1025;
        for (uint32_t id = 1; id <= 1024; ++id) {
            char title[32];
            std::snprintf(title, sizeof(title), "Contato %04u", id);
            largeState.records.push_back({id, pda::RecordKind::Contact, title, "", "", false});
        }
        CHECK(largeService.restore(largeState));
        Platform largePlatform;
        Ui largeUi(largeService, largePlatform);
        largeUi.begin();
        largeUi.dispatch(Command::App, 1);
        for (int i = 0; i < 31; ++i)
            largeUi.dispatch(Command::ListPage, 1);
        CHECK(labelled(lv_scr_act(), "Contato 1024"));
        largeUi.dispatch(Command::App, 5);
        largeUi.dispatch(Command::New);
        largeUi.dispatch(Command::PickerOpen, 2);
        for (int i = 0; i < 32; ++i)
            largeUi.dispatch(Command::PickerPage, 1);
        click(largeUi, largePlatform, "Contato 1024");
        CHECK(std::string(lv_textarea_get_text(field("ID do contato (opcional)"))) == "1024");
        // The device adapters are replaced here; the new LVGL flows still use real touch.
        MemoryFiles extraFiles;
        pda::SnapshotStore extraStore(extraFiles);
        pda::Service extraService(extraStore);
        CHECK(extraService.load());
        Platform extraPlatform;
        extraPlatform.fileStore = &extraFiles;
        Ui extra(extraService, extraPlatform);
        extra.begin();
        extra.dispatch(Command::App, 4);
        extra.dispatch(Command::New);
        set("Título", "Rascunho A");
        set("Texto / observações", "Texto preservado");
        set("Caderno / pasta", "Trabalho");
        set("Etiquetas", "reunião,ideia");
        click(extra, extraPlatform, "Guardar rascunho");
        CHECK(!extraService.state().draft.active && extraService.state().records.empty());
        extra.dispatch(Command::App, 4);
        extra.dispatch(Command::New);
        set("Título", "Rascunho B");
        click(extra, extraPlatform, "Guardar rascunho");
        int shelves = 0;
        for (const auto &f : extraPlatform.files())
            if (f.name.rfind("draft-", 0) == 0)
                ++shelves;
        CHECK(shelves == 2);
        screenshot(out / "notebooks-drafts.ppm", extra, extraPlatform);
        extra.dispatch(Command::ResumeShelf, 0);
        CHECK(std::string(lv_textarea_get_text(field("Título"))) == "Rascunho A");
        extra.dispatch(Command::Save);
        CHECK(extraService.state().records.size() == 1);
        extra.dispatch(Command::Favorite, 1);
        CHECK(pda::value(extraService.state().records[0].fields, "favorite") == "1");
        extra.dispatch(Command::Draw, 1);
        for (int i = 0; i <= 10; ++i) {
            touch = {400 + i * 8, 220 + i * 4, true};
            step(extra, extraPlatform, 25);
        }
        touch.down = false;
        step(extra, extraPlatform);
        screenshot(out / "ink-drawing.ppm", extra, extraPlatform);
        click(extra, extraPlatform, "Salvar desenho");
        auto drawing = pda::value(extraService.state().records[0].fields, "drawing");
        CHECK(!drawing.empty());
        std::string ink, error;
        CHECK(extraService.readFile(drawing, ink));
        std::vector<InkStroke> strokes;
        CHECK(decodeInk(ink, strokes, error));
        CHECK(!strokes.empty());
        std::size_t inkPoints = 0;
        for (const auto &stroke : strokes)
            inkPoints += stroke.points.size();
        CHECK(inkPoints >= 2);

        extra.dispatch(Command::Extras, 2);
        click(extra, extraPlatform, "Iniciar gravação");
        click(extra, extraPlatform, "Parar e salvar áudio");
        step(extra, extraPlatform);
        CHECK(!pda::value(extraService.state().records[0].fields, "audio").empty());
        screenshot(out / "audio-recorder.ppm", extra, extraPlatform);
        extra.dispatch(Command::Reminders);
        set("Adiamento (min)", "15");
        set("Silêncio de (HH:MM)", "22:00");
        set("Até (HH:MM)", "07:00");
        selectOption("Som do lembrete", 1);
        click(extra, extraPlatform, "Salvar preferências de aviso");
        CHECK(pda::value(extraService.state().settings.fields, "snooze") == "15");
        screenshot(out / "reminder-center.ppm", extra, extraPlatform);
        extra.dispatch(Command::Files);
        screenshot(out / "sd-files.ppm", extra, extraPlatform);
        CHECK(extraService.writeFile("table.csv", "nome,valor\nCafé,4.50\nLeite,8.00\n"));
        extra.dispatch(Command::Files);
        std::vector<FileEntry> visibleFiles;
        for (const auto &f : extraPlatform.files())
            if (publicFile(f.name))
                visibleFiles.push_back(f);
        auto tableFile = std::find_if(visibleFiles.begin(), visibleFiles.end(),
                                      [](const auto &f) { return f.name == "table.csv"; });
        CHECK(tableFile != visibleFiles.end());
        extra.dispatch(Command::FileOpen, std::distance(visibleFiles.begin(), tableFile));
        CHECK(find(lv_scr_act(), &lv_table_class));
        screenshot(out / "csv-table-viewer.ppm", extra, extraPlatform);

        auto baseline = syncExport(extraService.state(), error);
        CHECK(extraService.writeFile("sync-base.jsonl", baseline));
        auto remote = extraService.state();
        remote.records[0].detail = "Editado no computador";
        auto incoming = syncExport(remote, error);
        CHECK(extraService.writeFile("incoming-sync.jsonl", incoming));
        extra.dispatch(Command::Extras, 7);
        click(extra, extraPlatform, "Preparar prévia da sincronização");
        screenshot(out / "sync-preview.ppm", extra, extraPlatform);
        auto duringPreview = extraService.state().settings;
        duringPreview.brightness = 200;
        CHECK(extraService.saveSettings(duringPreview));
        click(extra, extraPlatform, "Aplicar sincronização");
        CHECK(extraService.state().records[0].detail != "Editado no computador");
        CHECK(extraService.state().settings.brightness == 200);
        extra.dispatch(Command::Extras, 7);
        click(extra, extraPlatform, "Preparar prévia da sincronização");
        click(extra, extraPlatform, "Aplicar sincronização");
        CHECK(extraService.state().records[0].detail == "Editado no computador");
        extra.dispatch(Command::Assistant);
        auto *url = field("Servidor local do assistente (http://...:8787/assistant)");
        CHECK(url);
        lv_textarea_set_text(url, "http://192.168.1.10:8787/assistant");
        auto *token = lv_obj_get_child(lv_obj_get_parent(url), lv_obj_get_index(url) + 1);
        CHECK(lv_obj_check_type(token, &lv_textarea_class));
        lv_textarea_set_text(token, "teste-local");
        set("Pedido — somente este texto e a hora local são enviados ao servidor configurado",
            "Resuma minha reunião");
        click(extra, extraPlatform, "Salvar servidor e código");
        click(extra, extraPlatform, "Gerar prévia");
        pda::State proposed;
        proposed.nextId = 2;
        proposed.records = {
            {1, pda::RecordKind::Note, "Resumo proposto", "Texto da proposta", "", false}};
        extraPlatform.assistantReply = pda::exportJsonl(proposed, error);
        auto beforeProposal = extraService.state().records.size();
        step(extra, extraPlatform);
        CHECK(extraService.state().records.size() == beforeProposal);
        screenshot(out / "assistant-preview.ppm", extra, extraPlatform);
        click(extra, extraPlatform, "Confirmar importação");
        CHECK(extraService.state().records.size() == beforeProposal + 1);
        auto oldAudio = pda::value(extraService.state().records[0].fields, "audio");
        extra.dispatch(Command::App, 12);
        extra.dispatch(Command::Extras, 2);
        extra.dispatch(Command::AudioRecord);
        CHECK(extraPlatform.audioName.rfind("audio-0-", 0) == 0);
        extra.dispatch(Command::AudioStop);
        step(extra, extraPlatform);
        CHECK(pda::value(extraService.state().records[0].fields, "audio") == oldAudio);
        extra.dispatch(Command::Share, 1);
        CHECK(extraPlatform.transferOn);
        screenshot(out / "wifi-file-transfer.ppm", extra, extraPlatform);
        auto secured = extraService.state().settings;
        secured.fields["pin"] = std::string(64, 'a');
        secured.fields["salt"] = std::string(32, '1');
        CHECK(extraService.saveSettings(secured));
        extra.dispatch(Command::Lock);
        CHECK(extra.locked() && extraPlatform.closedExternal && !extraPlatform.transferOn);
        MemoryFiles productFiles;
        pda::SnapshotStore productStore(productFiles);
        pda::Service productService(productStore);
        CHECK(productService.load());
        Platform productPlatform;
        productPlatform.fileStore = &productFiles;
        Ui products(productService, productPlatform);
        products.begin();
        std::string savedProduct, productError;
        products.dispatch(Command::Productivity);
        screenshot(out / "productivity-apps.ppm", products, productPlatform);
        products.dispatch(Command::Productivity, 1);
        set("Arquivo CSV", "calc.csv");
        set("Célula A1", "10");
        products.dispatch(Command::SheetApply);
        products.dispatch(Command::SheetCell, 1);
        set("Célula B1", "=A1*2");
        products.dispatch(Command::SheetApply);
        set("Arquivo CSV", "sheet-recovery.csv");
        products.dispatch(Command::ProductSave);
        CHECK(!productService.readFile("sheet-recovery.csv", savedProduct));
        set("Arquivo CSV", "calc.csv");
        products.dispatch(Command::ProductSave);
        CHECK(productService.readFile("calc.csv", savedProduct));
        Sheet savedSheet;
        CHECK(savedSheet.load(savedProduct, productError));
        double cellResult;
        CHECK(savedSheet.value(1, cellResult, productError) && cellResult == 20);
        screenshot(out / "mini-sheet.ppm", products, productPlatform);
        products.dispatch(Command::Productivity, 2);
        set("Nome do documento", "doc-test.txt");
        auto *docName = field("Nome do documento");
        lv_obj_t *docArea = nullptr;
        auto *docParent = lv_obj_get_parent(docName);
        for (uint32_t i = 0; i < lv_obj_get_child_cnt(docParent); ++i) {
            auto *o = lv_obj_get_child(docParent, i);
            if (o != docName && lv_obj_check_type(o, &lv_textarea_class))
                docArea = o;
        }
        CHECK(docArea);
        lv_textarea_set_text(docArea, "Uma nota sobre café.");
        products.dispatch(Command::DocReplace);
        set("Localizar", "café");
        set("Substituir por", "chá");
        products.dispatch(Command::DocReplace, 1);
        products.dispatch(Command::DocUndo);
        products.dispatch(Command::ProductSave);
        CHECK(productService.readFile("doc-test.txt", savedProduct));
        CHECK(savedProduct == "Uma nota sobre café.");
        products.dispatch(Command::DocReplace);
        products.dispatch(Command::DocReplace, 1);
        products.dispatch(Command::ProductSave);
        CHECK(productService.readFile("doc-test.txt", savedProduct));
        CHECK(savedProduct == "Uma nota sobre chá.");
        screenshot(out / "document-editor.ppm", products, productPlatform);
        CHECK(productService.writeFile("book.txt", std::string(5000, 'a')));
        products.dispatch(Command::Productivity, 3);
        products.dispatch(Command::ProductLoad);
        products.dispatch(Command::ProductFile, 0);
        products.dispatch(Command::ReaderMove, 1);
        products.dispatch(Command::ReaderMark);
        screenshot(out / "book-reader.ppm", products, productPlatform);
        products.dispatch(Command::Productivity, 4);
        CHECK(productService.writeFile("picture.bmp", "BM"));
        products.dispatch(Command::ProductLoad);
        products.dispatch(Command::ProductFile, 0);
        CHECK(find(lv_scr_act(), &lv_img_class));
        screenshot(out / "image-gallery.ppm", products, productPlatform);
        products.dispatch(Command::Productivity, 5);
        products.dispatch(Command::FinanceNew);
        set("Descrição", "Aluguel");
        set("Valor (positivo)", "10.00");
        set("Categoria", "Casa");
        selectOption("Repetição", 1);
        products.dispatch(Command::FinanceSave);
        set("Mês (AAAA-MM)", "2026-11");
        products.dispatch(Command::FinancePost);
        products.dispatch(Command::FinancePost);
        products.dispatch(Command::FinanceExport);
        CHECK(productService.readFile("finance-export.csv", savedProduct));
        std::vector<LedgerEntry> financeRows;
        CHECK(loadLedger(savedProduct, financeRows, productError));
        CHECK(financeRows.size() == 2);
        products.dispatch(Command::FinanceBudget);
        set("Valor (positivo)", "20.00");
        set("Categoria", "Casa");
        products.dispatch(Command::FinanceSave);
        screenshot(out / "personal-finance.ppm", products, productPlatform);
        products.dispatch(Command::FinanceNew);
        set("Descrição", "Não deve ser confirmado");
        set("Valor (positivo)", "99.00");
        productFiles.shortWrite = true;
        products.dispatch(Command::FinanceSave);
        productFiles.shortWrite = false;
        pda::SnapshotStore restoredStore(productFiles);
        pda::Service restoredService(restoredStore);
        CHECK(restoredService.load());
        Ui restored(restoredService, productPlatform);
        restored.begin();
        restored.dispatch(Command::Productivity, 5);
        restored.dispatch(Command::FinanceExport);
        CHECK(restoredService.readFile("finance-export.csv", savedProduct));
        CHECK(loadLedger(savedProduct, financeRows, productError));
        CHECK(financeRows.size() == 3);
        CHECK(std::none_of(financeRows.begin(), financeRows.end(), [](const auto &entry) {
            return entry.title == "Não deve ser confirmado";
        }));
        restored.dispatch(Command::Productivity, 3);
        restored.dispatch(Command::ProductLoad);
        restored.dispatch(Command::ProductFile, 0);
        CHECK(labelled(lv_scr_act(), "book.txt — página 2/3"));
        restored.dispatch(Command::ReaderMove, 1);
        restored.dispatch(Command::ReaderBookmark);
        CHECK(labelled(lv_scr_act(), "book.txt — página 2/3"));

        MemoryFiles reviewFiles;
        pda::SnapshotStore reviewStore(reviewFiles);
        pda::Service reviewService(reviewStore);
        CHECK(reviewService.load());
        Platform reviewPlatform;
        reviewPlatform.fileStore = &reviewFiles;
        Ui review(reviewService, reviewPlatform);
        review.begin();
        // Earlier scenarios retain their Ui instances. A fresh device session
        // only has one screen; release inactive screens before the large file test.
        auto *reviewDisplay = lv_disp_get_default();
        for (int i = int(reviewDisplay->screen_cnt) - 1; i >= 0; --i) {
            auto *old = reviewDisplay->screens[i];
            if (old != lv_scr_act() && old != lv_layer_top() && old != lv_layer_sys())
                lv_obj_del(old);
        }
        review.dispatch(Command::Productivity, 2);
        auto *reviewDoc = documentArea();
        lv_textarea_set_text(reviewDoc, std::string(200, 'a').c_str());
        lv_event_send(reviewDoc, LV_EVENT_CLICKED, nullptr);
        CHECK(lv_obj_get_height(reviewDoc) <= 208);
        key(review, reviewPlatform, "b");
        CHECK(std::string(lv_textarea_get_text(reviewDoc)).size() == 201);
        review.dispatch(Command::Accents);
        review.dispatch(Command::InsertAccent, 0);
        CHECK(std::string(lv_textarea_get_text(reviewDoc)).size() > 201);
        screenshot(out / "document-keyboard-review.ppm", review, reviewPlatform);
        auto clockPixels = [&] {
            std::vector<uint8_t> clock;
            for (int y = 16; y < 38; ++y)
                for (int x = 680; x < 1000; ++x) {
                    auto at = (y * 1280 + x) * 3;
                    clock.insert(clock.end(), frame.begin() + at, frame.begin() + at + 3);
                }
            return clock;
        };
        auto expectedClock = clockPixels();
        review.dispatch(Command::KeyboardHide);
        lv_textarea_set_text(reviewDoc, "ação café");
        auto selectCafe = [&] {
            auto *text = lv_textarea_get_label(documentArea());
            lv_label_set_text_sel_start(text, 5);
            lv_label_set_text_sel_end(text, 9);
        };
        selectCafe();
        review.dispatch(Command::DocCopy);
        lv_textarea_clear_selection(reviewDoc);
        lv_textarea_set_cursor_pos(reviewDoc, LV_TEXTAREA_CURSOR_LAST);
        review.dispatch(Command::DocPaste);
        CHECK(std::string(lv_textarea_get_text(documentArea())) == "ação cafécafé");
        review.dispatch(Command::DocUndo);
        CHECK(std::string(lv_textarea_get_text(documentArea())) == "ação café");
        selectCafe();
        review.dispatch(Command::DocCut);
        CHECK(std::string(lv_textarea_get_text(documentArea())) == "ação ");
        review.dispatch(Command::DocPaste);
        CHECK(std::string(lv_textarea_get_text(documentArea())) == "ação café");

        review.dispatch(Command::Productivity);
        review.dispatch(Command::Productivity, 2);
        lv_textarea_set_text(documentArea(), "");
        review.dispatch(Command::DocFind);
        review.dispatch(Command::Productivity, 2);
        CHECK(std::string(lv_textarea_get_text(documentArea())).empty());
        review.dispatch(Command::ProductSave);
        CHECK(reviewService.writeFile("aaa-windows.txt", "\xef\xbb\xbfLinha 1\r\nLinha 2\r\n"));
        review.dispatch(Command::ProductLoad);
        review.dispatch(Command::ProductFile, 0);
        CHECK(std::string(lv_textarea_get_text(documentArea())) == "Linha 1\nLinha 2\n");

        std::string largeDocument;
        for (int i = 0; i < 21845; ++i)
            largeDocument += "linha\n";
        largeDocument += "ab";
        CHECK(largeDocument.size() == 128 * 1024);
        CHECK(reviewService.writeFile("000-large.txt", largeDocument));
        review.dispatch(Command::ProductLoad);
        review.dispatch(Command::ProductFile, 0);
        reviewDoc = documentArea();
        CHECK(std::string(lv_textarea_get_text(reviewDoc)) == largeDocument);
        lv_obj_update_layout(lv_scr_act());
        CHECK(lv_obj_get_height(lv_textarea_get_label(reviewDoc)) > 300000);
        review.physicalKey(0, 4);
        CHECK(std::string(lv_textarea_get_text(reviewDoc)) == largeDocument);
        CHECK(lv_textarea_get_cursor_pos(reviewDoc) == largeDocument.size());
        step(review, reviewPlatform, 500); // LVGL scroll animations can take 400 ms.
        lv_point_t lastLetter;
        lv_label_get_letter_pos(lv_textarea_get_label(reviewDoc),
                                lv_textarea_get_cursor_pos(reviewDoc), &lastLetter);
        CHECK(lastLetter.y >= lv_obj_get_scroll_top(reviewDoc));
        CHECK(lastLetter.y - lv_obj_get_scroll_top(reviewDoc) <
              lv_obj_get_content_height(reviewDoc));
        screenshot(out / "large-document-end.ppm", review, reviewPlatform);
        CHECK(clockPixels() == expectedClock);
        review.dispatch(Command::ProductSave);
        CHECK(reviewService.readFile("000-large.txt", savedProduct));
        CHECK(savedProduct == largeDocument);

        review.dispatch(Command::Productivity, 5);
        review.dispatch(Command::FinanceNew);
        set("Descrição", "Receita preservada no alarme");
        set("Conta", "Banco");
        set("Valor (positivo)", "123.45");
        selectOption("Tipo", 1);
        selectOption("Repetição", 1);
        pda::Record interruption{0,  pda::RecordKind::Appointment, "Interrupção",
                                 "", "2026-10-13 09:01",           false};
        interruption.fields["reminder"] = "0";
        CHECK(reviewService.saveRecord(interruption));
        reviewPlatform.minute = "2026-10-13 09:01";
        step(review, reviewPlatform, 1001);
        CHECK(reviewPlatform.notifications == 1);
        review.dispatch(Command::AlarmDismiss);
        CHECK(std::string(lv_textarea_get_text(field("Descrição"))) ==
              "Receita preservada no alarme");
        CHECK(std::string(lv_textarea_get_text(field("Valor (positivo)"))) == "123.45");
        CHECK(lv_dropdown_get_selected(control("Tipo")) == 1);
        step(review, reviewPlatform, 500);
        screenshot(out / "finance-alarm-resume.ppm", review, reviewPlatform);
        review.dispatch(Command::FinanceSave);
        auto financeWrites = reviewFiles.writes;
        review.dispatch(Command::FinanceSave); // Late duplicate from the closed form.
        CHECK(reviewFiles.writes == financeWrites);
        review.dispatch(Command::FinanceExport);
        CHECK(reviewService.readFile("finance-export.csv", savedProduct));
        CHECK(loadLedger(savedProduct, financeRows, productError));
        CHECK(financeRows.size() == 1 && financeRows[0].cents == 12345 && financeRows[0].monthly);
        review.dispatch(Command::FinanceNew);
        CHECK(std::string(lv_textarea_get_text(field("Descrição"))).empty());
        set("Descrição", "Descartar formulário");
        review.dispatch(Command::FinanceCancel);
        review.dispatch(Command::FinanceNew);
        CHECK(std::string(lv_textarea_get_text(field("Descrição"))).empty());
        review.dispatch(Command::FinanceCancel);

        reviewFiles.readFailure = true;
        reviewPlatform.hideFiles = true;
        review.dispatch(Command::Productivity, 5);
        review.dispatch(Command::FinanceNew);
        CHECK(!field("Descrição"));
        financeWrites = reviewFiles.writes;
        review.dispatch(Command::FinanceExport);
        CHECK(reviewFiles.writes == financeWrites);
        reviewFiles.readFailure = false;
        reviewPlatform.hideFiles = false;
        review.dispatch(Command::Productivity, 5);
        review.dispatch(Command::FinanceExport);
        CHECK(reviewService.readFile("finance-export.csv", savedProduct));
        CHECK(loadLedger(savedProduct, financeRows, productError) && financeRows.size() == 1);

        review.dispatch(Command::FinanceNew);
        set("Descrição", "Segunda geração");
        set("Valor (positivo)", "1.00");
        review.dispatch(Command::FinanceSave);
        std::string bankBeforeDamage;
        CHECK(reviewService.readFile("finance-b.dat", bankBeforeDamage));
        CHECK(bankBeforeDamage.rfind("PDA-LEDGER-2\n", 0) == 0);
        auto damagedBank = bankBeforeDamage;
        damagedBank[std::string("PDA-LEDGER-2\n").size()] = '9';
        CHECK(reviewService.writeFile("finance-b.dat", damagedBank));
        review.dispatch(Command::Productivity, 5);
        review.dispatch(Command::FinanceExport);
        CHECK(reviewService.readFile("finance-export.csv", savedProduct));
        CHECK(loadLedger(savedProduct, financeRows, productError) && financeRows.size() == 1);
        CHECK(reviewService.writeFile("finance-b.dat", bankBeforeDamage));
        review.dispatch(Command::Productivity, 5);
        review.dispatch(Command::FinanceExport);
        CHECK(reviewService.readFile("finance-export.csv", savedProduct));
        CHECK(loadLedger(savedProduct, financeRows, productError) && financeRows.size() == 2);
        auto oldCsv = ledgerCsv(financeRows);
        auto oldEnvelope = "PDA-LEDGER-1\n3\n" +
                           std::to_string(pda::crc32(
                               reinterpret_cast<const uint8_t *>(oldCsv.data()), oldCsv.size())) +
                           "\n" + oldCsv;
        CHECK(reviewService.writeFile("finance-a.dat", oldEnvelope));
        CHECK(reviewPlatform.removeFile("finance-b.dat"));
        review.dispatch(Command::Productivity, 5);
        review.dispatch(Command::FinanceExport);
        CHECK(reviewService.readFile("finance-export.csv", savedProduct));
        CHECK(loadLedger(savedProduct, financeRows, productError) && financeRows.size() == 2);
        review.dispatch(Command::FinancePost);
        CHECK(reviewService.readFile("finance-b.dat", bankBeforeDamage));
        CHECK(bankBeforeDamage.rfind("PDA-LEDGER-2\n", 0) == 0);

        review.dispatch(Command::Productivity, 1);
        set("Célula A1", "=1+2");
        interruption.id = 0;
        interruption.when = "2026-10-13 09:02";
        CHECK(reviewService.saveRecord(interruption));
        reviewPlatform.minute = "2026-10-13 09:02";
        step(review, reviewPlatform, 1001);
        CHECK(reviewPlatform.notifications == 2);
        review.dispatch(Command::AlarmDismiss);
        CHECK(std::string(lv_textarea_get_text(field("Célula A1"))) == "=1+2");
        review.dispatch(Command::SheetApply);
        review.dispatch(Command::ProductSave);
        CHECK(reviewService.readFile("sheet.csv", savedProduct));
        CHECK(savedSheet.load(savedProduct, productError));
        CHECK(savedSheet.value(0, cellResult, productError) && cellResult == 3);

        CHECK(reviewService.writeFile("photo.bmp", "BM"));
        CHECK(reviewService.writeFile("zzz-photo.bmp", "BM"));
        review.dispatch(Command::Productivity, 4);
        review.dispatch(Command::ProductLoad);
        reviewPlatform.picturePixels = 1;
        review.dispatch(Command::ProductFile, 0);
        CHECK(!find(lv_scr_act(), &lv_img_class));
        reviewPlatform.picturePixels = 900 * 420;
        review.dispatch(Command::ProductFile, 0);
        CHECK(find(lv_scr_act(), &lv_img_class));
        CHECK(reviewPlatform.lastPicture == "photo.bmp");
        for (unsigned i = 0; i < 10; ++i) {
            review.dispatch(Command::GalleryMove, 1);
            CHECK(reviewPlatform.lastPicture == "zzz-photo.bmp");
            auto *imageObject = find(lv_scr_act(), &lv_img_class);
            CHECK(imageObject);
            auto *description = static_cast<const lv_img_dsc_t *>(lv_img_get_src(imageObject));
            CHECK(reinterpret_cast<const uint16_t *>(description->data)[0] == 0xf800);
            review.dispatch(Command::GalleryMove, 0);
            CHECK(reviewPlatform.lastPicture == "photo.bmp");
        }
        review.dispatch(Command::Files);
        std::vector<FileEntry> reviewVisible;
        for (const auto &f : reviewPlatform.files())
            if (publicFile(f.name))
                reviewVisible.push_back(f);
        auto imageFile = std::find_if(reviewVisible.begin(), reviewVisible.end(),
                                      [](const auto &f) { return f.name == "photo.bmp"; });
        CHECK(imageFile != reviewVisible.end());
        review.dispatch(Command::FileOpen, std::distance(reviewVisible.begin(), imageFile));
        CHECK(find(lv_scr_act(), &lv_img_class));
        review.dispatch(Command::SettingsField, 1);
        set("SSID Wi-Fi (vazio desliga)", "rede-de-teste");
        review.dispatch(Command::SettingsSave);
        auto settingsWrites = reviewFiles.writes;
        review.dispatch(Command::SettingsSave);
        CHECK(reviewFiles.writes == settingsWrites);
        review.dispatch(Command::Productivity, 2);
        auto documentBeforeLateEvents = std::string(lv_textarea_get_text(documentArea()));
        review.dispatch(Command::Save);
        review.dispatch(Command::SettingsSave);
        review.dispatch(Command::ReminderSave);
        review.dispatch(Command::AssistantSave);
        review.dispatch(Command::ExchangeConfirm);
        CHECK(std::string(lv_textarea_get_text(documentArea())) == documentBeforeLateEvents);

        reviewPlatform.hardwareKeyboard = true;
        review.dispatch(Command::App, 1);
        review.dispatch(Command::New);
        set("Nome", "joão da ávila");
        set("Empresa", "café do porto");
        set("E-mail", "joao@exemplo.com");
        set("Observações", "esta observação descreve um contato de exemplo com informações de "
                           "trabalho e de lazer para conferir a leitura do texto no espaço "
                           "disponível, mantendo o conteúdo original para edição posterior.");
        CHECK(std::string(lv_textarea_get_text(field("Nome"))) == "João Da Ávila");
        CHECK(std::string(lv_textarea_get_text(field("Empresa"))) == "Café Do Porto");
        CHECK(std::string(lv_textarea_get_text(field("E-mail"))) == "joao@exemplo.com");
        auto *nameInput = field("Nome");
        auto *companyInput = field("Empresa");
        CHECK(lv_obj_get_style_border_width(nameInput, LV_PART_CURSOR) == 0);
        CHECK(lv_obj_get_style_border_width(companyInput, LV_PART_CURSOR) == 0);
        lv_event_send(nameInput, LV_EVENT_CLICKED, nullptr);
        CHECK(!find(lv_scr_act(), &lv_keyboard_class));
        CHECK(lv_obj_get_style_border_width(nameInput, LV_PART_CURSOR) == 1);
        CHECK(lv_obj_get_style_border_side(nameInput, LV_PART_CURSOR) == LV_BORDER_SIDE_LEFT);
        CHECK(lv_obj_get_style_bg_opa(nameInput, LV_PART_CURSOR) == LV_OPA_TRANSP);
        CHECK(lv_obj_get_style_border_width(companyInput, LV_PART_CURSOR) == 0);
        screenshot(out / "contact-form-hardware-keyboard.ppm", review, reviewPlatform);
        reviewPlatform.hardwareKeyboard = false;
        step(review, reviewPlatform);
        auto *fallback = find(lv_scr_act(), &lv_keyboard_class);
        CHECK(fallback && !lv_obj_has_flag(fallback, LV_OBJ_FLAG_HIDDEN));
        reviewPlatform.hardwareKeyboard = true;
        step(review, reviewPlatform);
        CHECK(lv_obj_has_flag(fallback, LV_OBJ_FLAG_HIDDEN));
        CHECK(std::string(lv_textarea_get_text(nameInput)) == "João Da Ávila");
        review.dispatch(Command::Save);
        auto *companyCaption = labelled(lv_scr_act(), "Empresa");
        CHECK(companyCaption && lv_obj_get_style_text_font(companyCaption, 0) == &pda_font_16_bold);
        CHECK(labelled(lv_scr_act(), "Café Do Porto"));
        screenshot(out / "contact-reading-sereno.ppm", review, reviewPlatform);

        for (unsigned theme = 0; theme < 3; ++theme) {
            review.dispatch(Command::App, 13);
            CHECK(field("Hora manual: AAAA-MM-DD HH:MM"));
            CHECK(!labelled(lv_scr_act(), "Editar todos os ajustes"));
            selectOption("Tema visual", theme);
            review.dispatch(Command::SettingsSave);
            const char *ids[] = {"sereno", "areia", "ardosia"};
            CHECK(pda::value(reviewService.state().settings.fields, "theme") == ids[theme]);
            CHECK(lv_obj_get_style_bg_color(control("Tema visual"), 0).full ==
                  lv_color_hex(palette(ids[theme]).surface).full);
            CHECK(lv_obj_get_style_text_color(control("Tema visual"), 0).full ==
                  lv_color_hex(palette(ids[theme]).ink).full);
            CHECK(field("Hora manual: AAAA-MM-DD HH:MM"));
            CHECK(lv_obj_get_style_bg_color(lv_scr_act(), 0).full ==
                  lv_color_hex(palette(ids[theme]).background).full);
            screenshot(out / (std::string("settings-theme-") + ids[theme] + ".ppm"), review,
                       reviewPlatform);
            review.dispatch(Command::App, 1);
            screenshot(out / (std::string("contact-theme-") + ids[theme] + ".ppm"), review,
                       reviewPlatform);
        }
        pda::SnapshotStore themeStore(reviewFiles);
        pda::State themeState;
        CHECK(themeStore.load(themeState));
        CHECK(pda::value(themeState.settings.fields, "theme") == "ardosia");
        review.dispatch(Command::App, 13);
        selectOption("Tema visual", 0);
        review.dispatch(Command::SettingsSave);

        review.dispatch(Command::App, 7);
        auto noBasicButtons = [&] {
            for (const auto &caption :
                 {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "+", "-", "*", "/"}) {
                auto *label = labelled(lv_scr_act(), caption);
                CHECK(!label || !lv_obj_check_type(lv_obj_get_parent(label), &lv_btn_class));
            }
        };
        noBasicButtons();
        lv_textarea_set_text(find(lv_scr_act(), &lv_textarea_class), "2+3*4");
        review.physicalKey(0, 0x28);
        CHECK(labelled(lv_scr_act(), "14"));
        screenshot(out / "calculator-basic.ppm", review, reviewPlatform);
        review.dispatch(Command::CalculatorMode, 1);
        lv_textarea_set_text(find(lv_scr_act(), &lv_textarea_class), "90");
        review.dispatch(Command::ScientificFunction, 0);
        CHECK(labelled(lv_scr_act(), "1"));
        noBasicButtons();
        screenshot(out / "calculator-scientific.ppm", review, reviewPlatform);
        review.dispatch(Command::CalculatorMode, 2);
        review.dispatch(Command::FinancialSolve, 3);
        double calculatedPayment;
        CHECK(scientificCalculate(lv_textarea_get_text(field("Parcela (PMT)")), true,
                                  calculatedPayment, productError));
        CHECK(std::abs(calculatedPayment + 888.48788678) < 1e-6);
        noBasicButtons();
        screenshot(out / "calculator-financial.ppm", review, reviewPlatform);

        review.dispatch(Command::App, 8);
        selectOption("Grandeza", 0);
        step(review, reviewPlatform);
        set("Valor", "1500");
        selectOption("Unidade de origem", 2);
        selectOption("Unidade de destino", 3);
        review.dispatch(Command::Convert);
        CHECK(labelled(lv_scr_act(), "1.5 km"));
        screenshot(out / "converter-units.ppm", review, reviewPlatform);
        review.dispatch(Command::ConverterSwap);
        CHECK(lv_dropdown_get_selected(control("Unidade de origem")) == 3);
        CHECK(lv_dropdown_get_selected(control("Unidade de destino")) == 2);
        review.dispatch(Command::ConverterGroup, 4);
        set("Valor", "10");
        set("Taxa manual — unidades de destino por unidade de origem", "5.25");
        selectOption("Unidade de origem", 1);
        selectOption("Unidade de destino", 0);
        review.dispatch(Command::Convert);
        CHECK(labelled(lv_scr_act(), "52.5 BRL"));
        review.dispatch(Command::ConverterSwap);
        CHECK(std::string(lv_textarea_get_text(
                              field("Taxa manual — unidades de destino por unidade de origem")))
                  .rfind("0.190476", 0) == 0);
        selectOption("Unidade de origem", 0);
        selectOption("Unidade de destino", 0);
        set("Taxa manual — unidades de destino por unidade de origem", "5.25");
        review.dispatch(Command::Convert);
        CHECK(labelled(lv_scr_act(), "10 BRL"));
        review.dispatch(Command::App, 9);
        CHECK(labelled(lv_scr_act(), "Cuiabá"));
        auto *digitalClock = labelled(lv_scr_act(), "09:02");
        CHECK(digitalClock && lv_obj_get_style_text_font(digitalClock, 0) == &pda_font_48);
        screenshot(out / "digital-world-clocks.ppm", review, reviewPlatform);
        reviewPlatform.visibleNetworks = {{"Minha rede", -42, true},
                                          {"Rede visitante", -65, false}};
        review.dispatch(Command::App, 14);
        screenshot(out / "wifi-redesigned.ppm", review, reviewPlatform);
        review.dispatch(Command::App, 7);
        review.dispatch(Command::CalculatorMode, 0);
        reviewPlatform.hardwareKeyboard = false;
        auto *touchCalculation = find(lv_scr_act(), &lv_textarea_class);
        lv_textarea_set_text(touchCalculation, "7*8");
        lv_event_send(touchCalculation, LV_EVENT_CLICKED, nullptr);
        key(review, reviewPlatform, "Enter");
        CHECK(labelled(lv_scr_act(), "56"));
        lv_deinit();
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
