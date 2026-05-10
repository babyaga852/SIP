
#if defined(SIP_OS_WINDOWS)
#include "os/os_layer.hpp"
#include <spdlog/spdlog.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#include <tlhelp32.h>
#include <iphlpapi.h>
#include <comdef.h>
#include <Wbemidl.h>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wbemuuid.lib")

#include <vector>
#include <string>
#include <memory>
#include <sstream>
#include <chrono>
#include <codecvt>
#include <locale>

namespace sip::os {

// ── Helper: wide string → UTF-8 ──────────────────────────────────────────────
static std::string w2s(const std::wstring& w) {
    if (w.empty()) return {};
    int sz = WideCharToMultiByte(CP_UTF8, 0, w.data(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(sz - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), -1, s.data(), sz, nullptr, nullptr);
    return s;
}

// ── PDH CPU query (initialized once) ─────────────────────────────────────────
namespace {
    struct PdhCtx {
        PDH_HQUERY   query{};
        PDH_HCOUNTER counter{};
        bool         ready{false};

        PdhCtx() {
            if (PdhOpenQuery(nullptr, 0, &query) != ERROR_SUCCESS) return;
            if (PdhAddEnglishCounterW(query,
                    L"\\Processor(_Total)\\% Processor Time",
                    0, &counter) != ERROR_SUCCESS) {
                PdhCloseQuery(query); return;
            }
            PdhCollectQueryData(query);
            ready = true;
        }
        ~PdhCtx() { if (ready) PdhCloseQuery(query); }
        double cpu_pct() {
            if (!ready) return 0.0;
            PdhCollectQueryData(query);
            PDH_FMT_COUNTERVALUE val{};
            PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &val);
            return val.doubleValue;
        }
    };
    static PdhCtx g_pdh;
} // anon

// ── WMI helper ────────────────────────────────────────────────────────────────
namespace {

class WmiSession {
public:
    IWbemLocator*  loc{};
    IWbemServices* svc{};
    bool ready{false};

    WmiSession() {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(CoCreateInstance(CLSID_WbemLocator, nullptr,
                CLSCTX_INPROC_SERVER, IID_IWbemLocator,
                reinterpret_cast<void**>(&loc)))) return;
        BSTR ns = SysAllocString(L"ROOT\\CIMV2");
        HRESULT hr = loc->ConnectServer(ns, nullptr, nullptr, nullptr,
                                        0, nullptr, nullptr, &svc);
        SysFreeString(ns);
        if (FAILED(hr)) { loc->Release(); loc = nullptr; return; }
        CoSetProxyBlanket(svc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
            nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
            nullptr, EOAC_NONE);
        ready = true;
    }
    ~WmiSession() {
        if (svc) svc->Release();
        if (loc) loc->Release();
        CoUninitialize();
    }
};

static WmiSession g_wmi;

std::vector<std::vector<std::wstring>> wmi_query(
    const std::wstring& wql,
    const std::vector<std::wstring>& fields)
{
    std::vector<std::vector<std::wstring>> rows;
    if (!g_wmi.ready) return rows;

    BSTR lang  = SysAllocString(L"WQL");
    BSTR query = SysAllocString(wql.c_str());
    IEnumWbemClassObject* enumerator = nullptr;
    HRESULT hr = g_wmi.svc->ExecQuery(lang, query,
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr, &enumerator);
    SysFreeString(lang); SysFreeString(query);
    if (FAILED(hr)) return rows;

    IWbemClassObject* obj = nullptr;
    ULONG ret = 0;
    while (enumerator->Next(WBEM_INFINITE, 1, &obj, &ret) == S_OK) {
        std::vector<std::wstring> row;
        for (auto& f : fields) {
            VARIANT var; VariantInit(&var);
            BSTR fname = SysAllocString(f.c_str());
            obj->Get(fname, 0, &var, nullptr, nullptr);
            SysFreeString(fname);
            std::wstring val;
            if (var.vt == VT_BSTR && var.bstrVal) val = var.bstrVal;
            else if (var.vt == VT_I4)  val = std::to_wstring(var.intVal);
            else if (var.vt == VT_UI4) val = std::to_wstring(var.uintVal);
            else if (var.vt == VT_UI8) val = std::to_wstring(var.ullVal);
            VariantClear(&var);
            row.push_back(val);
        }
        rows.push_back(row);
        obj->Release();
    }
    enumerator->Release();
    return rows;
}

} // anon

// ── Process list ─────────────────────────────────────────────────────────────
std::vector<ProcessInfo> list_processes() {
    std::vector<ProcessInfo> result;

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return result;

    PROCESSENTRY32W pe{sizeof(pe)};
    double total_cpu = g_pdh.cpu_pct();

    for (bool ok = Process32FirstW(snap, &pe); ok;
              ok = Process32NextW(snap, &pe)) {
        ProcessInfo pi{};
        pi.pid  = pe.th32ProcessID;
        pi.ppid = pe.th32ParentProcessID;
        pi.name = w2s(pe.szExeFile);

        HANDLE ph = OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pi.pid);
        if (ph) {
            // RSS
            PROCESS_MEMORY_COUNTERS pmc{};
            if (GetProcessMemoryInfo(ph, &pmc, sizeof(pmc)))
                pi.rss_bytes = pmc.WorkingSetSize;

            // FD (handle count)
            DWORD handles{};
            GetProcessHandleCount(ph, &handles);
            pi.open_fds = handles;

            // Cmdline from QueryFullProcessImageName
            wchar_t path[MAX_PATH]{};
            DWORD sz = MAX_PATH;
            if (QueryFullProcessImageNameW(ph, 0, path, &sz))
                pi.cmdline = w2s(path);

            CloseHandle(ph);
        }
        // Rough CPU share (split total evenly — real impl needs per-process counters)
        pi.cpu_pct = total_cpu / std::max(1u, (unsigned)result.size() + 1);
        result.push_back(pi);
    }
    CloseHandle(snap);
    return result;
}

// ── Thermal via WMI MSAcpi_ThermalZoneTemperature ────────────────────────────
std::vector<ThermalReading> thermal_readings() {
    std::vector<ThermalReading> result;
    // Try ACPI thermal zones first
    auto rows = wmi_query(
        L"SELECT InstanceName, CurrentTemperature FROM MSAcpi_ThermalZoneTemperature",
        {L"InstanceName", L"CurrentTemperature"});

    for (auto& row : rows) {
        if (row.size() < 2) continue;
        ThermalReading r;
        r.id   = w2s(row[0]);
        r.type = "cpu";
        // Windows reports in tenths of Kelvin
        double kelvin_x10 = std::stod(row[1].empty() ? L"0" : row[1]);
        r.temp_c = (kelvin_x10 / 10.0) - 273.15;
        if (r.temp_c > 0.0 && r.temp_c < 150.0)
            result.push_back(r);
    }

    // Fallback: PDH thermal counters
    if (result.empty()) {
        PDH_HQUERY q{}; PDH_HCOUNTER c{};
        if (PdhOpenQuery(nullptr, 0, &q) == ERROR_SUCCESS) {
            if (PdhAddEnglishCounterW(q,
                    L"\\Thermal Zone Information(_Total)\\Temperature", 0, &c)
                == ERROR_SUCCESS) {
                PdhCollectQueryData(q);
                PDH_FMT_COUNTERVALUE val{};
                PdhGetFormattedCounterValue(c, PDH_FMT_DOUBLE, nullptr, &val);
                if (val.doubleValue > 0)
                    result.push_back({"PDH_TZ0", "cpu", val.doubleValue - 273.15});
            }
            PdhCloseQuery(q);
        }
    }
    return result;
}

// ── Network stats via GetIfTable2 ────────────────────────────────────────────
std::vector<NetStats> net_stats() {
    std::vector<NetStats> result;
    MIB_IF_TABLE2* table = nullptr;
    if (GetIfTable2(&table) != NO_ERROR) return result;
    for (ULONG i = 0; i < table->NumEntries; ++i) {
        auto& row = table->Table[i];
        if (row.OperStatus != IfOperStatusUp) continue;
        NetStats n;
        n.iface    = w2s(row.Description);
        n.rx_bytes = row.InOctets;
        n.tx_bytes = row.OutOctets;
        result.push_back(n);
    }
    FreeMibTable(table);
    return result;
}

// ── CPU throttle via SetThreadPriority + PowerSetActiveScheme ────────────────
void throttle_cpu(double fraction) {
    spdlog::debug("[OS/Win] throttle_cpu({:.2f})", fraction);
    THREAD_PRIORITY_INFORMATION tpi{};
    if (fraction < 0.3) {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_IDLE);
    } else if (fraction < 0.6) {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    } else {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
    }
    // Optionally switch to Power Saver scheme
    if (fraction < 0.4) {
        GUID* scheme = nullptr;
        PowerGetActiveScheme(nullptr, &scheme);
        // PowerSetActiveScheme(nullptr, &GUID_MIN_POWER_SAVINGS); // power saver
        if (scheme) LocalFree(scheme);
    }
}

int64_t now_ms() {
    FILETIME ft{};
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER li{};
    li.LowPart  = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    // Convert from 100ns intervals since 1601 to ms since Unix epoch
    return static_cast<int64_t>((li.QuadPart - 116444736000000000ULL) / 10000);
}

} // namespace sip::os
#endif // SIP_OS_WINDOWS
