#pragma once

// Socket event dispatch trace (23.1.3).
//
// Runs 30-33 end the same way: the config system emits `subscribe`, this
// server answers it in full, the client decrypts the answer, and the config
// parser is never called. tools/find_callers.py reports zero static call sites
// for that parser, so it can only be reached through a delegate held by the
// socket layer. dump2313.cs:381487 is that layer -- a static class in namespace
// Rilisoft.WebSocket whose only field is the subscriber registry
//
//   Dictionary<string, List<handler>>            // offset 0x0
//
// and whose dispatch method takes exactly what the config parser takes:
//
//   0x35BA5E0  static void <dispatch>(string event,
//                                     Dictionary<string, object> head,
//                                     Dictionary<object, object> body,
//                                     List<byte[]> attachments)
//   0x35C1818  static void <subscribe>(string event, handler h)
//   0x35C188C  static void <unsubscribe>(string event, handler h)
//   0x35C18F4  static void <addRemove>(string event, handler h, bool add)
//
// Three questions, one run: which events are dispatched at boot, which events
// have handlers registered and by which object, and whether `subscribe` -- the
// config poll answer -- has any handler at all. Nothing is altered: every proxy
// calls the original with its own arguments.

#include <atomic>
#include <cstdint>
#include <string>

#include "elf_sym.h"
#include "hook.h"
#include "il2cpp.h"
#include "log.h"

namespace socket_dispatch_trace_2313 {
namespace detail {

using MethodInfo = void;

// Spelled as escapes so this file's source encoding cannot change which class
// or method is resolved, and taken from dump2313.cs by line rather than
// retyped. The namespace is the `// Namespace:` line above the class, not a
// guess: run 30 armed 0/4 because that line was skipped.
constexpr const char* kNamespace = "Rilisoft.WebSocket";
constexpr const char* kDispatcher =
    u8"\u4e08\u4e12\u4e0b\u4e18\u4e16\u4e05\u4e0e\u4e03\u4e06";
constexpr const char* kDispatch =
    u8"\u4e05\u4e15\u4e19\u4e0c\u4e15\u4e16\u4e04\u4e1e\u4e18";
constexpr const char* kSubscribe =
    u8"\u4e14\u4e16\u4e13\u4e07\u4e01\u4e0a\u4e13\u4e19\u4e0e";
constexpr const char* kUnsubscribe =
    u8"\u4e13\u4e0d\u4e1b\u4e0d\u4e0b\u4e16\u4e13\u4e12\u4e11";
constexpr const char* kAddRemove =
    u8"\u4e0f\u4e10\u4e0b\u4e03\u4e0e\u4e14\u4e01\u4e0c\u4e14";

// Same layout notes as the config system trace: this build's CoreLib keeps the
// Dictionary entry count at 0x20 and the List size at 0x18. A delegate keeps
// its target object at 0x20, after the method pointer and the invoker.
constexpr size_t kDictCount = 0x20u;
constexpr size_t kListSize = 0x18u;
constexpr size_t kDelegateTarget = 0x20u;
constexpr size_t kDelegateMethod = 0x10u;
constexpr size_t kTextPreview = 96u;

using DispatchFn = void (*)(void*, void*, void*, void*, const MethodInfo*);
using RegisterFn = void (*)(void*, void*, const MethodInfo*);
using AddRemoveFn = void (*)(void*, void*, bool, const MethodInfo*);

inline DispatchFn g_dispatch = nullptr;
inline RegisterFn g_subscribe = nullptr;
inline RegisterFn g_unsubscribe = nullptr;
inline AddRemoveFn g_add_remove = nullptr;

inline std::atomic<uint32_t> g_dispatches{0u};
inline std::atomic<uint32_t> g_registrations{0u};

inline int32_t read_i32(const void* base, size_t offset) {
    if (base == nullptr) return -1;
    return *reinterpret_cast<const int32_t*>(
        static_cast<const char*>(base) + offset);
}

inline void* read_ptr(const void* base, size_t offset) {
    if (base == nullptr) return nullptr;
    return *reinterpret_cast<void* const*>(
        static_cast<const char*>(base) + offset);
}

inline const char* class_name_of(void* object) {
    if (object == nullptr) return "null";
    if (il2cpp::object_get_class == nullptr ||
        il2cpp::class_get_name == nullptr) {
        return "?";
    }
    void* klass = il2cpp::object_get_class(object);
    if (klass == nullptr) return "?";
    const char* name = il2cpp::class_get_name(klass);
    return name != nullptr ? name : "?";
}

inline std::string text_of(void* object) {
    if (object == nullptr) return std::string();
    return il2cpp::to_utf8(object, kTextPreview);
}

// Every inbound event passes here. The answer to the config poll is 5569 bytes
// of head dictionary, so the entry count is logged next to the name: an event
// that arrives with a full head and leaves without reaching the parser proves
// the loss is in the registry rather than on the wire.
inline void hook_dispatch(void* name, void* head, void* body, void* attachments,
                          const MethodInfo* method) {
    const uint32_t call = g_dispatches.fetch_add(1u) + 1u;
    const std::string event = text_of(name);
    LOGI("23.1.3-sio[dispatch] #%u event='%s' head=%p count@0x20=%d body=%p "
         "count@0x20=%d attachments=%p size=%d",
         call, event.c_str(), head, read_i32(head, kDictCount), body,
         read_i32(body, kDictCount), attachments,
         read_i32(attachments, kListSize));

    if (g_dispatch != nullptr) {
        g_dispatch(name, head, body, attachments, method);
    }

    LOGI("23.1.3-sio[dispatch] #%u event='%s' returned", call, event.c_str());
}

inline void log_registration(const char* verb, void* name, void* handler) {
    const uint32_t call = g_registrations.fetch_add(1u) + 1u;
    void* target = read_ptr(handler, kDelegateTarget);
    // A delegate keeps the method it will call first, before the invoker and
    // the target, so the registry can name every subscriber by address.
    void* fn = read_ptr(handler, kDelegateMethod);
    uintptr_t base = 0;
    unsigned long long rva = 0ull;
    if (elfsym::find_library("libil2cpp.so", &base) && base != 0 &&
        reinterpret_cast<uintptr_t>(fn) > base) {
        rva = static_cast<unsigned long long>(
            reinterpret_cast<uintptr_t>(fn) - base);
    }
    LOGI("23.1.3-sio[dispatch] #%u %s event='%s' handler=%p class=%s "
         "target=%p targetClass=%s method=0x%llX",
         call, verb, text_of(name).c_str(), handler, class_name_of(handler),
         target, class_name_of(target), rva);
}

inline void hook_subscribe(void* name, void* handler,
                           const MethodInfo* method) {
    log_registration("subscribe", name, handler);
    if (g_subscribe != nullptr) {
        g_subscribe(name, handler, method);
    }
}

inline void hook_unsubscribe(void* name, void* handler,
                             const MethodInfo* method) {
    log_registration("unsubscribe", name, handler);
    if (g_unsubscribe != nullptr) {
        g_unsubscribe(name, handler, method);
    }
}

inline void hook_add_remove(void* name, void* handler, bool add,
                            const MethodInfo* method) {
    log_registration(add ? "registry-add" : "registry-remove", name, handler);
    if (g_add_remove != nullptr) {
        g_add_remove(name, handler, add, method);
    }
}

} // namespace detail

// None of the four is required: a build where one name no longer resolves keeps
// the others armed, and the verdict line says how many went in so a quiet run
// is never mistaken for a negative result.
inline bool install_hooks() {
    int armed = 0;

    armed += hook::install(
                 {detail::kNamespace, detail::kDispatcher, detail::kDispatch, 4},
                 reinterpret_cast<void*>(&detail::hook_dispatch),
                 reinterpret_cast<void**>(&detail::g_dispatch), false)
                 ? 1
                 : 0;
    armed += hook::install(
                 {detail::kNamespace, detail::kDispatcher, detail::kSubscribe, 2},
                 reinterpret_cast<void*>(&detail::hook_subscribe),
                 reinterpret_cast<void**>(&detail::g_subscribe), false)
                 ? 1
                 : 0;
    armed += hook::install(
                 {detail::kNamespace, detail::kDispatcher, detail::kUnsubscribe, 2},
                 reinterpret_cast<void*>(&detail::hook_unsubscribe),
                 reinterpret_cast<void**>(&detail::g_unsubscribe), false)
                 ? 1
                 : 0;
    armed += hook::install(
                 {detail::kNamespace, detail::kDispatcher, detail::kAddRemove, 3},
                 reinterpret_cast<void*>(&detail::hook_add_remove),
                 reinterpret_cast<void**>(&detail::g_add_remove), false)
                 ? 1
                 : 0;

    LOGI("23.1.3-sio[dispatch]: socket dispatch trace armed on %d/4 entry "
         "point(s) in namespace %s; dispatch=inbound event delivery, "
         "subscribe/unsubscribe/registry=handler registry",
         armed, detail::kNamespace);
    return armed > 0;
}

} // namespace socket_dispatch_trace_2313
