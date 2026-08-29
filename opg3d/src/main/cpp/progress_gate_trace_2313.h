#pragma once

#include <atomic>
#include <cstdint>

#include "elf_sym.h"
#include "hook.h"
#include "il2cpp.h"
#include "log.h"

// Run 15 named the blocker exactly: the Authorized branch parks on
// WaitUntil(东下丏丞七丌丟上丌.<>9__126_0), and that predicate returns
// (static field 丅三丈丝不丂丌万三 != null) -- polled every frame, false forever.
// The field holds the binded-account model the client expects the server to
// describe, and the sibling field 丅三丗丝专丛丘丛丞 is its second slot.
// This trace follows the static class that owns both fields: its socket
// answer handlers, its integer extractors, and the value of the two fields
// after every handler, so the exact reply that fails to fill them is named.
namespace progress_gate_trace_2313 {
namespace detail {

using MethodInfo = void;

constexpr int32_t kMaxReports = 6;

inline void* g_field_first = nullptr;
inline void* g_field_second = nullptr;
inline std::atomic<bool> g_filled{false};

inline void* static_value(void* field) {
    void* value = nullptr;
    if (field != nullptr && il2cpp::field_static_get_value != nullptr) {
        il2cpp::field_static_get_value(field, &value);
    }
    return value;
}

inline std::atomic<bool> g_slots_dumped{false};

// Metadata-usage slots read by the answer parser at 0x3DCFFC0, lifted from its
// disassembly. The first two hold the string literals it uses as dictionary
// keys, so resolving them from the live process names the exact fields the
// client expects a reply to carry. The class slots name the type the second
// key's value must have and the owner of the event emitted after the wait.
inline void dump_slots_once() {
    if (g_slots_dumped.load(std::memory_order_relaxed)) {
        return;
    }
    uintptr_t base = 0;
    if (!elfsym::find_library("libil2cpp.so", &base) || base == 0) {
        return;
    }
    struct Slot {
        uintptr_t offset;
        const char* what;
    };
    static const Slot kKeys[] = {
        {0x67DB910, "answer key read first"},
        {0x67F0A88, "answer key read second"},
    };
    bool resolved = false;
    for (const Slot& slot : kKeys) {
        // A metadata-usage slot holds the address of the cell that the runtime
        // fills in on first execution, so the object needs two dereferences.
        void** cell = *reinterpret_cast<void***>(base + slot.offset);
        void* value = cell != nullptr ? *cell : nullptr;
        if (value == nullptr) {
            continue;
        }
        const std::string text = il2cpp::to_utf8(value, 128);
        if (text.empty()) {
            continue;
        }
        resolved = true;
        LOGI("23.1.3-gate: %s = \"%s\"", slot.what, text.c_str());
    }

    static const Slot kClasses[] = {
        {0x67AFBC8, "class the second value must be"},
        {0x67E3280, "class owning the event emitted after the wait"},
    };
    for (const Slot& slot : kClasses) {
        void** klass_cell = *reinterpret_cast<void***>(base + slot.offset);
        void* klass = klass_cell != nullptr ? *klass_cell : nullptr;
        if (klass == nullptr || il2cpp::class_get_name == nullptr) {
            continue;
        }
        LOGI("23.1.3-gate: %s = %s", slot.what, il2cpp::class_get_name(klass));
        if (slot.offset != 0x67E3280) {
            continue;
        }
        void* statics = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(klass) + 0xb8);
        if (statics == nullptr) {
            continue;
        }
        void* name = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(statics) + 0x160);
        if (name != nullptr) {
            LOGI("23.1.3-gate: the event emitted after the wait is \"%s\"",
                 il2cpp::to_utf8(name, 128).c_str());
        }
    }
    if (resolved) {
        g_slots_dumped.store(true, std::memory_order_relaxed);
    }
}

inline void report_fields(const char* where) {
    void* first = static_value(g_field_first);
    void* second = static_value(g_field_second);
    LOGI("23.1.3-gate: after %s the awaited field is %s (%p) and its sibling is %s (%p)",
         where, first != nullptr ? "filled" : "still null", first,
         second != nullptr ? "filled" : "still null", second);
    if (first != nullptr && !g_filled.exchange(true)) {
        LOGI("23.1.3-gate: the WaitUntil condition just became true, set by %s", where);
    }
}

using Handler0 = void (*)(void*, void*, void*, const MethodInfo*);
inline Handler0 g_handler0 = nullptr;
inline std::atomic<int32_t> g_handler0_calls;
void handler_0(void* answer, void* payload, void* attachments, const MethodInfo* method) {
    const int32_t call = g_handler0_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool report = call <= kMaxReports;
    if (report) {
        LOGI("23.1.3-gate: handler 0x3DCF224 ENTER answer=%p payload=%p attachments=%p call#%d", answer, payload, attachments, call);
    }
    if (g_handler0 != nullptr) {
        g_handler0(answer, payload, attachments, method);
    }
    if (report) {
        report_fields("handler 0x3DCF224");
    }
}

using Handler1 = void (*)(void*, void*, void*, const MethodInfo*);
inline Handler1 g_handler1 = nullptr;
inline std::atomic<int32_t> g_handler1_calls;
void handler_1(void* answer, void* payload, void* attachments, const MethodInfo* method) {
    const int32_t call = g_handler1_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool report = call <= kMaxReports;
    if (report) {
        LOGI("23.1.3-gate: handler 0x3DCF4C8 ENTER answer=%p payload=%p attachments=%p call#%d", answer, payload, attachments, call);
    }
    if (g_handler1 != nullptr) {
        g_handler1(answer, payload, attachments, method);
    }
    if (report) {
        report_fields("handler 0x3DCF4C8");
    }
}

using Handler2 = void (*)(void*, void*, void*, const MethodInfo*);
inline Handler2 g_handler2 = nullptr;
inline std::atomic<int32_t> g_handler2_calls;
void handler_2(void* answer, void* payload, void* attachments, const MethodInfo* method) {
    const int32_t call = g_handler2_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool report = call <= kMaxReports;
    if (report) {
        LOGI("23.1.3-gate: handler 0x3DCF62C ENTER answer=%p payload=%p attachments=%p call#%d", answer, payload, attachments, call);
    }
    if (g_handler2 != nullptr) {
        g_handler2(answer, payload, attachments, method);
    }
    if (report) {
        report_fields("handler 0x3DCF62C");
    }
}

using Handler3 = void (*)(void*, void*, void*, const MethodInfo*);
inline Handler3 g_handler3 = nullptr;
inline std::atomic<int32_t> g_handler3_calls;
void handler_3(void* answer, void* payload, void* attachments, const MethodInfo* method) {
    const int32_t call = g_handler3_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool report = call <= kMaxReports;
    if (report) {
        LOGI("23.1.3-gate: handler 0x3DCFB50 ENTER answer=%p payload=%p attachments=%p call#%d", answer, payload, attachments, call);
    }
    if (g_handler3 != nullptr) {
        g_handler3(answer, payload, attachments, method);
    }
    if (report) {
        report_fields("handler 0x3DCFB50");
    }
}

using Handler4 = void (*)(void*, void*, void*, const MethodInfo*);
inline Handler4 g_handler4 = nullptr;
inline std::atomic<int32_t> g_handler4_calls;
void handler_4(void* answer, void* payload, void* attachments, const MethodInfo* method) {
    const int32_t call = g_handler4_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool report = call <= kMaxReports;
    if (report) {
        LOGI("23.1.3-gate: handler 0x3DCFE58 ENTER answer=%p payload=%p attachments=%p call#%d", answer, payload, attachments, call);
    }
    if (g_handler4 != nullptr) {
        g_handler4(answer, payload, attachments, method);
    }
    if (report) {
        report_fields("handler 0x3DCFE58");
    }
}

using Handler5 = void (*)(void*, void*, void*, const MethodInfo*);
inline Handler5 g_handler5 = nullptr;
inline std::atomic<int32_t> g_handler5_calls;
void handler_5(void* answer, void* payload, void* attachments, const MethodInfo* method) {
    const int32_t call = g_handler5_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool report = call <= kMaxReports;
    if (report) {
        LOGI("23.1.3-gate: handler 0x3DCFF30 ENTER answer=%p payload=%p attachments=%p call#%d", answer, payload, attachments, call);
    }
    if (g_handler5 != nullptr) {
        g_handler5(answer, payload, attachments, method);
    }
    if (report) {
        report_fields("handler 0x3DCFF30");
    }
}

using Extractor0 = int32_t (*)(void*, const MethodInfo*);
inline Extractor0 g_extractor0 = nullptr;
inline std::atomic<int32_t> g_extractor0_calls;
int32_t extractor_0(void* answer, const MethodInfo* method) {
    const int32_t value = g_extractor0 != nullptr ? g_extractor0(answer, method) : 0;
    dump_slots_once();
    const int32_t call = g_extractor0_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call <= kMaxReports) {
        LOGI("23.1.3-gate: extractor 0x3DCF30C read %d from answer=%p call#%d", value, answer, call);
    }
    return value;
}

using Extractor1 = int32_t (*)(void*, const MethodInfo*);
inline Extractor1 g_extractor1 = nullptr;
inline std::atomic<int32_t> g_extractor1_calls;
int32_t extractor_1(void* answer, const MethodInfo* method) {
    const int32_t value = g_extractor1 != nullptr ? g_extractor1(answer, method) : 0;
    dump_slots_once();
    const int32_t call = g_extractor1_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call <= kMaxReports) {
        LOGI("23.1.3-gate: extractor 0x3DCF550 read %d from answer=%p call#%d", value, answer, call);
    }
    return value;
}

using Extractor2 = int32_t (*)(void*, const MethodInfo*);
inline Extractor2 g_extractor2 = nullptr;
inline std::atomic<int32_t> g_extractor2_calls;
int32_t extractor_2(void* answer, const MethodInfo* method) {
    const int32_t value = g_extractor2 != nullptr ? g_extractor2(answer, method) : 0;
    dump_slots_once();
    const int32_t call = g_extractor2_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call <= kMaxReports) {
        LOGI("23.1.3-gate: extractor 0x3DCFC04 read %d from answer=%p call#%d", value, answer, call);
    }
    return value;
}

using Extractor3 = int32_t (*)(void*, const MethodInfo*);
inline Extractor3 g_extractor3 = nullptr;
inline std::atomic<int32_t> g_extractor3_calls;
int32_t extractor_3(void* answer, const MethodInfo* method) {
    const int32_t value = g_extractor3 != nullptr ? g_extractor3(answer, method) : 0;
    dump_slots_once();
    const int32_t call = g_extractor3_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call <= kMaxReports) {
        LOGI("23.1.3-gate: extractor 0x3DCF910 read %d from answer=%p call#%d", value, answer, call);
    }
    return value;
}

using Extractor4 = int32_t (*)(void*, const MethodInfo*);
inline Extractor4 g_extractor4 = nullptr;
inline std::atomic<int32_t> g_extractor4_calls;
int32_t extractor_4(void* answer, const MethodInfo* method) {
    const int32_t value = g_extractor4 != nullptr ? g_extractor4(answer, method) : 0;
    dump_slots_once();
    const int32_t call = g_extractor4_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call <= kMaxReports) {
        LOGI("23.1.3-gate: extractor 0x3DCFFC0 read %d from answer=%p call#%d", value, answer, call);
    }
    return value;
}

using Single0 = void (*)(void*, const MethodInfo*);
inline Single0 g_single0 = nullptr;
inline std::atomic<int32_t> g_single0_calls;
void single_0(void* argument, const MethodInfo* method) {
    const int32_t call = g_single0_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool report = call <= kMaxReports;
    if (report) {
        LOGI("23.1.3-gate: 0x3DD0268 ENTER argument=%p call#%d", argument, call);
    }
    if (g_single0 != nullptr) {
        g_single0(argument, method);
    }
    if (report) {
        report_fields("0x3DD0268");
    }
}

using Single1 = void (*)(void*, const MethodInfo*);
inline Single1 g_single1 = nullptr;
inline std::atomic<int32_t> g_single1_calls;
void single_1(void* argument, const MethodInfo* method) {
    const int32_t call = g_single1_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool report = call <= kMaxReports;
    if (report) {
        LOGI("23.1.3-gate: 0x3DD01F4 ENTER argument=%p call#%d", argument, call);
    }
    if (g_single1 != nullptr) {
        g_single1(argument, method);
    }
    if (report) {
        report_fields("0x3DD01F4");
    }
}

using Single2 = void (*)(void*, const MethodInfo*);
inline Single2 g_single2 = nullptr;
inline std::atomic<int32_t> g_single2_calls;
void single_2(void* argument, const MethodInfo* method) {
    const int32_t call = g_single2_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool report = call <= kMaxReports;
    if (report) {
        LOGI("23.1.3-gate: 0x3DD0150 ENTER argument=%p call#%d", argument, call);
    }
    if (g_single2 != nullptr) {
        g_single2(argument, method);
    }
    if (report) {
        report_fields("0x3DD0150");
    }
}

using Step0 = void (*)(const MethodInfo*);
inline Step0 g_step0 = nullptr;
inline std::atomic<int32_t> g_step0_calls;
void step_0(const MethodInfo* method) {
    const int32_t call = g_step0_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool report = call <= kMaxReports;
    if (report) {
        LOGI("23.1.3-gate: 0x3DCF1AC ENTER call#%d", call);
    }
    if (g_step0 != nullptr) {
        g_step0(method);
    }
    if (report) {
        report_fields("0x3DCF1AC");
    }
}

using Step1 = void (*)(const MethodInfo*);
inline Step1 g_step1 = nullptr;
inline std::atomic<int32_t> g_step1_calls;
void step_1(const MethodInfo* method) {
    const int32_t call = g_step1_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool report = call <= kMaxReports;
    if (report) {
        LOGI("23.1.3-gate: 0x3DC4E28 ENTER call#%d", call);
    }
    if (g_step1 != nullptr) {
        g_step1(method);
    }
    if (report) {
        report_fields("0x3DC4E28");
    }
}

using Flag0 = void (*)(bool, const MethodInfo*);
inline Flag0 g_flag0 = nullptr;
inline std::atomic<int32_t> g_flag0_calls;
void flag_0(bool value, const MethodInfo* method) {
    const int32_t call = g_flag0_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call <= kMaxReports) {
        LOGI("23.1.3-gate: 0x3DCAEAC called with %s call#%d", value ? "true" : "false", call);
    }
    if (g_flag0 != nullptr) {
        g_flag0(value, method);
    }
}

using Getter0 = void* (*)(const MethodInfo*);
inline Getter0 g_getter0 = nullptr;
inline std::atomic<int32_t> g_getter0_calls;
void* getter_0(const MethodInfo* method) {
    void* value = g_getter0 != nullptr ? g_getter0(method) : nullptr;
    const int32_t call = g_getter0_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call <= kMaxReports) {
        LOGI("23.1.3-gate: getter 0x3DCEC08 returned %p call#%d", value, call);
    }
    return value;
}

using Getter1 = void* (*)(const MethodInfo*);
inline Getter1 g_getter1 = nullptr;
inline std::atomic<int32_t> g_getter1_calls;
void* getter_1(const MethodInfo* method) {
    void* value = g_getter1 != nullptr ? g_getter1(method) : nullptr;
    const int32_t call = g_getter1_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call <= kMaxReports) {
        LOGI("23.1.3-gate: getter 0x3DCEC60 returned %p call#%d", value, call);
    }
    return value;
}

// The factory that builds the awaited model:
//   internal static <model> <factory>(Dictionary<string, object> answer)
// Run 23 proved the reply reaches apply and the model is still never stored,
// so the factory itself has to name the keys it reads. Every field assignment
// in its body is guarded by a ContainsKey probe, which means a dictionary with
// the wrong key names builds a blank model and leaves the gate closed. These
// are the metadata-usage slots the body loads before each key lookup.
inline std::atomic<bool> g_factory_slots_dumped{false};

inline void dump_factory_slots_once() {
    if (g_factory_slots_dumped.load(std::memory_order_relaxed)) {
        return;
    }
    uintptr_t base = 0;
    if (!elfsym::find_library("libil2cpp.so", &base) || base == 0) {
        return;
    }
    struct FactorySlot {
        uintptr_t offset;
        const char* what;
    };
    static const FactorySlot kSlots[] = {
        {0x67660C8, "factory key for the int at +0x10"},
        {0x675D7A0, "factory key for the int at +0x14"},
        {0x6799888, "factory key for the flag at +0x18"},
        {0x67B6F08, "factory key for the string at +0x20"},
        {0x6785438, "factory key read next to the string at +0x20"},
        {0x6839990, "factory key for the string at +0x28"},
        {0x683AF10, "factory key for the object at +0x30"},
        {0x683EE38, "factory key for the collection at +0x38"},
        {0x67DAA90, "factory key read while filling the collection"},
        {0x67DA570, "factory key of the collection entry, first half"},
        {0x675FEE0, "factory key of the collection entry, second half"},
    };
    bool resolved = false;
    for (const FactorySlot& slot : kSlots) {
        // A metadata-usage slot holds the address of the cell the runtime fills
        // in on first execution, so the object needs two dereferences.
        void** cell = *reinterpret_cast<void***>(base + slot.offset);
        void* value = cell != nullptr ? *cell : nullptr;
        if (value == nullptr) {
            continue;
        }
        const std::string text = il2cpp::to_utf8(value, 128);
        if (text.empty()) {
            continue;
        }
        resolved = true;
        LOGI("23.1.3-gate: %s = \"%s\"", slot.what, text.c_str());
    }
    if (resolved) {
        g_factory_slots_dumped.store(true, std::memory_order_relaxed);
    }
}

inline std::string field_string(void* object, uintptr_t offset) {
    if (object == nullptr) {
        return std::string("<no object>");
    }
    void* value = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(object) + offset);
    if (value == nullptr) {
        return std::string("<null>");
    }
    return il2cpp::to_utf8(value, 128);
}

using Factory = void* (*)(void*, const MethodInfo*);
inline Factory g_factory = nullptr;
inline std::atomic<int32_t> g_factory_calls;
void* factory(void* answer, const MethodInfo* method) {
    const int32_t call = g_factory_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool report = call <= kMaxReports;
    if (report) {
        LOGI("23.1.3-gate: factory ENTER answer=%p call#%d", answer, call);
    }
    void* model = g_factory != nullptr ? g_factory(answer, method) : nullptr;
    if (!report) {
        return model;
    }
    // The usage cells are only filled once the body has executed, so the keys
    // are read after the original call and never at install time.
    dump_factory_slots_once();
    LOGI("23.1.3-gate: factory returned %p", model);
    if (model != nullptr) {
        const uintptr_t self = reinterpret_cast<uintptr_t>(model);
        LOGI("23.1.3-gate: model ints +0x10=%d +0x14=%d flag +0x18=%d",
             *reinterpret_cast<int32_t*>(self + 0x10),
             *reinterpret_cast<int32_t*>(self + 0x14),
             static_cast<int>(*reinterpret_cast<uint8_t*>(self + 0x18)));
        LOGI("23.1.3-gate: model strings +0x20=\"%s\" +0x28=\"%s\"",
             field_string(model, 0x20).c_str(), field_string(model, 0x28).c_str());
        LOGI("23.1.3-gate: model refs +0x30=%p +0x38=%p",
             *reinterpret_cast<void**>(self + 0x30),
             *reinterpret_cast<void**>(self + 0x38));
    }
    report_fields("factory");
    return model;
}

} // namespace detail

inline bool install_hooks() {
    detail::g_field_first = il2cpp::find_field("", u8"东下丏丞七丌丟上丌", u8"丅三丈丝不丂丌万三");
    detail::g_field_second = il2cpp::find_field("", u8"东下丏丞七丌丟上丌", u8"丅三丗丝专丛丘丛丞");
    LOGI("23.1.3-gate: awaited field %s, sibling field %s",
         detail::g_field_first != nullptr ? "resolved" : "NOT resolved",
         detail::g_field_second != nullptr ? "resolved" : "NOT resolved");
    int armed = 0;
    armed += hook::install({"", u8"东下丏丞七丌丟上丌", u8"业三丕且丝下丈丕丘", 3},
                           reinterpret_cast<void*>(&detail::handler_0),
                           reinterpret_cast<void**>(&detail::g_handler0), false) ? 1 : 0;  // 0x3DCF224
    armed += hook::install({"", u8"东下丏丞七丌丟上丌", u8"一不丏与丝丞丙不不", 3},
                           reinterpret_cast<void*>(&detail::handler_1),
                           reinterpret_cast<void**>(&detail::g_handler1), false) ? 1 : 0;  // 0x3DCF4C8
    armed += hook::install({"", u8"东下丏丞七丌丟上丌", u8"丑丁丕专丙不下三丞", 3},
                           reinterpret_cast<void*>(&detail::handler_2),
                           reinterpret_cast<void**>(&detail::g_handler2), false) ? 1 : 0;  // 0x3DCF62C
    armed += hook::install({"", u8"东下丏丞七丌丟上丌", u8"丏丈丂丐不三丞丄丒", 3},
                           reinterpret_cast<void*>(&detail::handler_3),
                           reinterpret_cast<void**>(&detail::g_handler3), false) ? 1 : 0;  // 0x3DCFB50
    armed += hook::install({"", u8"东下丏丞七丌丟上丌", u8"丒丛下丑丝与丗丑丌", 3},
                           reinterpret_cast<void*>(&detail::handler_4),
                           reinterpret_cast<void**>(&detail::g_handler4), false) ? 1 : 0;  // 0x3DCFE58
    armed += hook::install({"", u8"东下丏丞七丌丟上丌", u8"丂丛下丄专丞丞东丐", 3},
                           reinterpret_cast<void*>(&detail::handler_5),
                           reinterpret_cast<void**>(&detail::g_handler5), false) ? 1 : 0;  // 0x3DCFF30
    armed += hook::install({"", u8"东下丏丞七丌丟上丌", u8"丟业丅一三丏丗世丐", 1},
                           reinterpret_cast<void*>(&detail::extractor_0),
                           reinterpret_cast<void**>(&detail::g_extractor0), false) ? 1 : 0;  // 0x3DCF30C
    armed += hook::install({"", u8"东下丏丞七丌丟上丌", u8"丈与丐下丕业丒七不", 1},
                           reinterpret_cast<void*>(&detail::extractor_1),
                           reinterpret_cast<void**>(&detail::g_extractor1), false) ? 1 : 0;  // 0x3DCF550
    armed += hook::install({"", u8"东下丏丞七丌丟上丌", u8"丈丄东丞上丗不丈不", 1},
                           reinterpret_cast<void*>(&detail::extractor_2),
                           reinterpret_cast<void**>(&detail::g_extractor2), false) ? 1 : 0;  // 0x3DCFC04
    armed += hook::install({"", u8"东下丏丞七丌丟上丌", u8"丟不丑丛丟丝丞丒丆", 1},
                           reinterpret_cast<void*>(&detail::extractor_3),
                           reinterpret_cast<void**>(&detail::g_extractor3), false) ? 1 : 0;  // 0x3DCF910
    armed += hook::install({"", u8"东下丏丞七丌丟上丌", u8"三丅上丏丟不丂丝上", 1},
                           reinterpret_cast<void*>(&detail::extractor_4),
                           reinterpret_cast<void**>(&detail::g_extractor4), false) ? 1 : 0;  // 0x3DCFFC0
    armed += hook::install({"", u8"东下丏丞七丌丟上丌", u8"丏与世丘丈丕丘丆不", 1},
                           reinterpret_cast<void*>(&detail::single_0),
                           reinterpret_cast<void**>(&detail::g_single0), false) ? 1 : 0;  // 0x3DD0268
    armed += hook::install({"", u8"东下丏丞七丌丟上丌", u8"丑业丘丘丙上七丗与", 1},
                           reinterpret_cast<void*>(&detail::single_1),
                           reinterpret_cast<void**>(&detail::g_single1), false) ? 1 : 0;  // 0x3DD01F4
    armed += hook::install({"", u8"东下丏丞七丌丟上丌", u8"七丅丕丗丏丙丆七丞", 1},
                           reinterpret_cast<void*>(&detail::single_2),
                           reinterpret_cast<void**>(&detail::g_single2), false) ? 1 : 0;  // 0x3DD0150
    armed += hook::install({"", u8"东下丏丞七丌丟上丌", u8"丙丏下丙丝丁丆丂丗", 0},
                           reinterpret_cast<void*>(&detail::step_0),
                           reinterpret_cast<void**>(&detail::g_step0), false) ? 1 : 0;  // 0x3DCF1AC
    armed += hook::install({"", u8"东下丏丞七丌丟上丌", u8"丕丝上万丑丞七丛丟", 0},
                           reinterpret_cast<void*>(&detail::step_1),
                           reinterpret_cast<void**>(&detail::g_step1), false) ? 1 : 0;  // 0x3DC4E28
    armed += hook::install({"", u8"东下丏丞七丌丟上丌", u8"丁与上丝丁丕丐丕丄", 1},
                           reinterpret_cast<void*>(&detail::flag_0),
                           reinterpret_cast<void**>(&detail::g_flag0), false) ? 1 : 0;  // 0x3DCAEAC
    armed += hook::install({"", u8"东下丏丞七丌丟上丌", u8"丟丒世丆丁丙丌丂不", 0},
                           reinterpret_cast<void*>(&detail::getter_0),
                           reinterpret_cast<void**>(&detail::g_getter0), false) ? 1 : 0;  // 0x3DCEC08
    armed += hook::install({"", u8"东下丏丞七丌丟上丌", u8"万丆专东一不丗丟丄", 0},
                           reinterpret_cast<void*>(&detail::getter_1),
                           reinterpret_cast<void**>(&detail::g_getter1), false) ? 1 : 0;  // 0x3DCEC60
    armed += hook::install({"", u8"业丒丏丆一丆丘丞万", u8"与丄三丄丕专丗丆丐", 1},
                           reinterpret_cast<void*>(&detail::factory),
                           reinterpret_cast<void**>(&detail::g_factory), false) ? 1 : 0;  // 0x4C98F44
    LOGI("23.1.3-gate: binded-account gate trace armed on %d/20 entry point(s); "
         "passive passthrough", armed);
    detail::report_fields("install");
    return armed > 0;
}

} // namespace progress_gate_trace_2313
