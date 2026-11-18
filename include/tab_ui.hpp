#pragma once
#include "exchange.hpp"
#include "input_mask.hpp"
#include "pda/organizer.hpp"
#include "pda/service.hpp"
#include "presentation.hpp"
#include "productivity.hpp"
#include "schedule.hpp"
#include "workspace.hpp"
#include <deque>
#include <list>
#include <lvgl.h>
#include <set>

LV_FONT_DECLARE(pda_font_16);
LV_FONT_DECLARE(pda_font_16_bold);
LV_FONT_DECLARE(pda_font_28);
LV_FONT_DECLARE(pda_font_48);
namespace tab5_pda {
struct WifiNetwork {
    std::string ssid;
    int rssi;
    bool secured;
};
class IPlatform {
  public:
    virtual ~IPlatform() = default;
    virtual uint32_t nowMs() const = 0;
    virtual std::string localMinute() const = 0;
    virtual std::string diagnostics() const = 0;
    virtual bool setClock(const std::string &, int, std::string &) = 0;
    virtual bool remount() = 0;
    virtual void applySettings(const pda::Settings &) = 0;
    virtual void notify() = 0;
    virtual std::string randomSalt() = 0;
    virtual std::string pinHash(const std::string &, const std::string &) = 0;
    virtual void syncTime() = 0;
    virtual std::string networkStatus() const = 0;
    virtual bool physicalKeyboardConnected() const {
        return false;
    }
    virtual void scanNetworks() {}
    virtual std::vector<WifiNetwork> networks() const {
        return {};
    }
    virtual void connectNetwork() {
        syncTime();
    }
    virtual void disconnectNetwork() {}

    virtual std::vector<FileEntry> files() const {
        return {};
    }
    virtual bool removeFile(const std::string &) {
        return false;
    }
    virtual bool audioRecord(const std::string &) {
        return false;
    }
    virtual bool audioPlay(const std::string &) {
        return false;
    }
    virtual void audioStop() {}
    virtual std::string audioStatus() const {
        return "Áudio indisponível";
    }
    virtual bool audioFinished(std::string &) {
        return false;
    }
    virtual std::string transfer(bool) {
        return "Transferência indisponível";
    }
    virtual bool transferActive() const {
        return false;
    }
    virtual void closeExternal() {
        audioStop();
        transfer(false);
    }
    virtual bool askAssistant(const std::string &, const std::string &, const std::string &) {
        return false;
    }
    virtual bool assistantResult(std::string &, std::string &) {
        return false;
    }
    virtual bool picture(const std::string &, std::vector<uint16_t> &, std::string &error) {
        error = "Imagem indisponível";
        return false;
    }
    virtual bool networkBusy() const {
        return false;
    }
};
enum class Command {
    App,
    Select,
    New,
    Edit,
    Delete,
    ConfirmDelete,
    Cancel,
    Save,
    Field,
    Search,
    Toggle,
    Month,
    Day,
    Calendar,
    Series,
    Occurrence,
    SkipOccurrence,
    Calculate,
    MemoryAdd,
    MemorySubtract,
    MemoryRecall,
    MemoryClear,
    Convert,
    Totals,
    Backup,
    Restore,
    Incoming,
    ConfirmRestore,
    Usb,
    Resume,
    DiscardDraft,
    Retry,
    SettingsSave,
    SettingsField,
    Sync,
    ScanNetworks,
    ConnectNetwork,
    DisconnectNetwork,
    SelectNetwork,
    Lock,
    Unlock,
    Pin,
    RemovePin,
    AlarmDismiss,
    AlarmSnooze,
    AlarmHide,
    AlarmShow,
    Accents,
    InsertAccent,
    KeyboardHide,
    AccentClose,
    PickerOpen,
    PickerAdjust,
    PickerDay,
    PickerOption,
    PickerApply,
    PickerClear,
    PickerCancel,
    PickerPage,
    OpenRecord,
    QuickNew,
    ToggleChild,
    NewSubtask,
    TaskFilter,
    ScheduleMode,
    ScheduleMove,
    ScheduleSlot,
    ScheduleSelect,
    Reschedule,
    Trash,
    TrashRestore,
    TrashUndo,
    TrashPurge,
    TrashPurgeConfirm,
    ListPage,
    MoveConfirm,
    Exchange,
    ExchangeExport,
    ExchangePreview,
    ExchangeConfirm,
    DailyRestore,
    Extras,
    Notebook,
    Favorite,
    Template,
    Shelf,
    ShelfContinue,
    ResumeShelf,
    DeleteShelf,
    AudioRecord,
    AudioStop,
    AudioPlay,
    Draw,
    DrawSave,
    DrawUndo,
    DrawClear,
    DrawColor,
    Files,
    FileOpen,
    FileNext,
    FileRemove,
    FileRemoveConfirm,
    Reminders,
    ReminderOpen,
    ReminderSave,
    Share,
    SyncPreview,
    SyncKeep,
    SyncRemote,
    SyncApply,
    Assistant,
    AssistantAsk,
    AssistantSave,
    FileAttach,
    Productivity,
    ProductFile,
    ProductLoad,
    ProductSave,
    ProductNew,
    SheetCell,
    SheetApply,
    DocCopy,
    DocCut,
    DocPaste,
    DocUndo,
    DocFind,
    DocReplace,
    DocSearch,
    ReaderMove,
    ReaderMark,
    ReaderBookmark,
    GalleryMove,
    FinanceNew,
    FinanceSave,
    FinancePost,
    FinanceBudget,
    FinanceExport,
    FinanceCancel,
    CalculatorMode,
    ScientificFunction,
    FinancialSolve,
    AngleMode,
    ConverterGroup,
    ConverterSwap
};
class Ui {
  public:
    Ui(pda::Service &s, IPlatform &p) : service_(s), platform_(p) {}
    void begin();
    void tick();
    void activity();
    void physicalKey(uint8_t modifier, uint8_t hidCode);
    bool locked() const {
        return locked_;
    }
    bool usbAllowed() const {
        return !locked_ && app_ == 12 && usbEnabled_ && mode_ == Mode::Normal;
    }
    void dispatch(Command command, uint32_t argument = 0);

  private:
    enum class Mode {
        Normal,
        Edit,
        Search,
        ConfirmDelete,
        ConfirmRestore,
        Recovery,
        Lock,
        Alarm,
        Preferences,
        Occurrence,
        Trash,
        Purge,
        Exchange,
        ImportPreview,
        MoveConfirm,
        Extras,
        Drawing,
        FileView,
        FileDelete,
        SyncPreview,
        Productivity
    };
    struct Binding {
        Ui *ui;
        Command command;
        uint32_t argument;
    };
    static void clicked(lv_event_t *event);
    static void changed(lv_event_t *event);
    static void focused(lv_event_t *event);
    static void typed(lv_event_t *event);
    static void controlChanged(lv_event_t *event);
    lv_obj_t *slider(lv_obj_t *, int, int, int, int, int, int);
    lv_obj_t *dropdown(lv_obj_t *, int, int, const std::string &, unsigned);
    std::string inputValue(lv_obj_t *) const;
    bool brazilian() const;
    std::string fieldText(const std::string &, const std::string &) const;
    std::string fieldCaption(const std::string &) const;
    void render();
    void renderToday();
    void renderSchedule();
    void renderPriority();
    void renderExtras();
    void renderProductivity();
    bool handleProductivity(Command, uint32_t);
    void captureProductivity();
    bool openPicture(const std::string &);
    bool saveProductDraft();
    bool saveFinance(const std::vector<LedgerEntry> &);
    void loadFinance();
    void readerPosition(bool save);
    static void productChanged(lv_event_t *);
    static void sheetTouched(lv_event_t *);
    bool handleExtras(Command, uint32_t);
    void tickExtras();
    bool shelfDraft();
    void renderInk();
    static void inkTouch(lv_event_t *);
    void drawSegment(InkPoint, InkPoint, const InkStroke &);
    bool handlePriority(Command, uint32_t);
    void captureExchange();
    void renderRecords();
    void renderDetail(lv_obj_t *parent, const pda::Record *record, int y = 12);
    void renderCalendar();
    void renderEditor();
    std::vector<std::pair<std::string, std::string>> fieldOptions(const std::string &) const;
    void pickerButton(lv_obj_t *, int, int, lv_obj_t *, const std::string &);
    void openPicker(unsigned);
    void drawPicker();
    void closePicker();
    void readPickerTime();
    void renderPreferences();
    void renderTools();
    void renderCalculator();
    void renderConverter();
    void renderClocks();
    void renderNetwork();
    bool handleToolCommands(Command, uint32_t);
    void captureTools();
    void caseInput(lv_obj_t *, TextCase);
    void applyTheme(lv_obj_t *);
    std::string justified(const std::string &, int width);
    void renderOverlay();
    lv_obj_t *button(lv_obj_t *, int, int, int, int, const std::string &, Command, uint32_t = 0,
                     uint32_t = 0);
    lv_obj_t *label(lv_obj_t *, int, int, int, const std::string &, uint32_t = 0x172b4d);
    lv_obj_t *panel(lv_obj_t *, int, int, int, int, uint32_t = 0xffffff);
    lv_obj_t *input(lv_obj_t *, int, int, int, int, const std::string &, std::size_t, bool = false,
                    bool = false);
    void keyboard(lv_obj_t *target);
    std::size_t inputByteLimit() const;
    void mask(lv_obj_t *target, InputMask kind, const char *hint, unsigned max);
    struct MaskedInput {
        lv_obj_t *object;
        InputMask kind;
        std::string previous;
    };
    std::vector<MaskedInput> masks_;
    bool masking_ = false;
    struct CasedInput {
        lv_obj_t *object;
        TextCase mode;
    };
    std::vector<CasedInput> casedInputs_;
    bool casing_ = false, lastKeyboardConnected_ = false;
    const Palette *theme_ = &palette("sereno");
    int calculatorMode_ = 0, converterGroup_ = 0, converterFrom_ = 0, converterTo_ = 1;
    bool degrees_ = true;
    std::vector<std::string> financeCalc_ = {"12", "1", "10000", "0", "0"};
    bool financeBeginning_ = false;
    std::string calculatorResult_ = "0", converterValue_ = "1", converterRate_ = "1",
                converterResult_;
    void fitForm(lv_obj_t *target, int height);
    void capture();
    void startEdit(const pda::Record *record = nullptr);
    void saveEditor();
    void persistDraft();
    void openPreferences(const std::string &purpose);
    void savePreferences();
    void message(const std::string &, bool error = false);
    void lock();
    pda::RecordKind kind() const;
    const pda::Record *selectedRecord() const;
    pda::Service &service_;
    IPlatform &platform_;
    lv_obj_t *screen_ = nullptr, *body_ = nullptr, *status_ = nullptr, *clock_ = nullptr,
             *editor_ = nullptr, *keyboard_ = nullptr;
    lv_obj_t *form_ = nullptr;
    struct PickerField {
        lv_obj_t *target;
        std::string key;
    };
    std::vector<PickerField> pickerFields_;
    lv_obj_t *picker_ = nullptr, *pickerContent_ = nullptr, *pickerHour_ = nullptr,
             *pickerMinute_ = nullptr;
    lv_obj_t *pickerTarget_ = nullptr;
    pda::CivilMinute pickerDate_;
    std::string pickerKey_;
    std::size_t pickerBindingsStart_ = 0;
    int pickerPage_ = 0;
    bool pickerDateOnly_ = false;
    std::vector<std::pair<std::string, std::string>> pickerOptions_;
    std::vector<lv_obj_t *> inputs_;
    std::vector<std::size_t> inputLimits_;
    std::vector<int> inputHeights_;
    std::vector<std::pair<lv_obj_t *, lv_obj_t *>> sliderLabels_;
    std::vector<WifiNetwork> networks_;
    bool uppercase_ = false, symbols_ = false;
    lv_obj_t *accentPalette_ = nullptr;
    std::list<Binding> bindings_;
    std::deque<std::pair<Command, uint32_t>> commands_;
    int productPage_ = 0, productFilePage_ = 0, sheetCell_ = 0;
    bool sheetDirty_ = false, sheetLoaded_ = false, documentLoaded_ = false;
    int sheetEditCell_ = -1, financeDraftPage_ = 0;
    std::string sheetEdit_;
    std::vector<std::string> financeDraft_;
    std::string galleryName_, documentName_ = "document.txt", sheetName_ = "sheet.csv";
    bool productListing_ = false, documentDirty_ = false, changingDocument_ = false,
         financeOkay_ = true;
    Sheet sheet_;
    std::string productName_ = "sheet.csv", documentText_, clipboard_, documentFind_,
                documentReplace_, financeMonth_;
    std::vector<std::string> documentUndo_;
    uint32_t documentUndoAt_ = 0;
    unsigned readerPage_ = 0, bookmarkIndex_ = 0;
    std::vector<unsigned> readerMarks_;
    std::string bookName_, bookText_;
    std::vector<uint16_t> galleryPixels_;
    lv_img_dsc_t galleryImage_{};
    lv_obj_t *productEditor_ = nullptr;
    std::vector<LedgerEntry> ledger_;
    uint64_t ledgerGeneration_ = 0;
    int ledgerBank_ = -1;
    int extrasPage_ = 0, filesPage_ = 0, template_ = -1;
    bool favoritesOnly_ = false, sharing_ = false, inkPressed_ = false, inkDirty_ = false,
         drawingResume_ = false;
    std::string notebookFilter_, shelfName_, activeShelf_, fileName_, fileText_, extrasStatus_,
        assistantURL_, assistantToken_, assistantPrompt_;
    std::vector<FileEntry> fileEntries_;
    std::vector<pda::Alarm> reminderEntries_;
    std::vector<InkStroke> ink_;
    std::vector<lv_color_t> inkPixels_;
    lv_obj_t *inkCanvas_ = nullptr;
    uint32_t inkColor_ = 0, attachmentId_ = 0, audioAttachment_ = 0;
    lv_obj_t *extrasLive_ = nullptr;
    std::vector<std::string> notebooks_;
    SyncPlan syncPlan_;
    uint64_t syncGeneration_ = 0;
    std::string syncFile_ = "incoming-sync.jsonl";
    int app_ = 0, field_ = 0;
    int scheduleMode_ = 0, taskFilter_ = 0, exchangeFormat_ = 0, listPage_ = 0;
    uint32_t lastTrashed_ = 0, moveRecord_ = 0, backupAt_ = 0;
    bool creatingSubtask_ = false;
    std::string backupDay_, exchangeFile_ = "calendar.ics", restoreFile_, scheduleOccurrence_,
                            moveTarget_, newWhen_, subtaskParent_;
    std::vector<pda::Record> pendingImports_;
    std::vector<ScheduleEntry> scheduleEntries_;
    uint32_t selected_ = 0, lastActivity_ = 0, lastEdit_ = 0, lastDraft_ = 0, lastClock_ = 0,
             lastAlarm_ = 0, pinRetry_ = 0;
    bool locked_ = false, dirty_ = false, usbEnabled_ = false, incoming_ = false,
         overlapConfirmed_ = false;
    Mode mode_ = Mode::Normal, beforeAlarm_ = Mode::Normal;
    pda::Draft draft_;
    bool pendingCreate_ = false, pendingExisting_ = false;
    int pendingApp_ = 0;
    pda::Record pendingRecord_;
    std::string initialClock_, query_, calendarDay_, dailyFilter_, expression_, result_, purpose_,
        lastMinute_, lastNetwork_;
    std::vector<std::pair<std::string, std::string>> preferences_;
    std::set<std::string> muted_, notifiedLocked_;
    pda::Alarm alarm_{};
    double memory_ = 0;
};
} // namespace tab5_pda
