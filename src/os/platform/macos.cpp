
#if defined(SIP_OS_MACOS)
#include "os/os_layer.hpp"
#include <spdlog/spdlog.h>

// sysctl / proc info
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_mib.h>
#include <libproc.h>

// IOKit thermal
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

#include <mach/mach.h>
#include <mach/task_info.h>

#include <vector>
#include <string>
#include <cstring>
#include <unistd.h>

namespace sip::os {

// ── Process list ─────────────────────────────────────────────────────────────
std::vector<ProcessInfo> list_processes() {
    std::vector<ProcessInfo> result;

    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
    size_t size = 0;
    if (sysctl(mib, 4, nullptr, &size, nullptr, 0) != 0) return result;

    std::vector<kinfo_proc> procs(size / sizeof(kinfo_proc));
    if (sysctl(mib, 4, procs.data(), &size, nullptr, 0) != 0) return result;

    for (auto& kp : procs) {
        if (kp.kp_proc.p_pid == 0) continue;
        ProcessInfo pi{};
        pi.pid   = static_cast<uint32_t>(kp.kp_proc.p_pid);
        pi.ppid  = static_cast<uint32_t>(kp.kp_eproc.e_ppid);
        pi.name  = kp.kp_proc.p_comm;

        // Get full cmdline via libproc
        char cmdline[PROC_PIDPATHINFO_MAXSIZE]{};
        proc_pidpath(pi.pid, cmdline, sizeof(cmdline));
        pi.cmdline = cmdline;

        // RSS via task_info
        task_t task;
        if (task_for_pid(mach_task_self(), pi.pid, &task) == KERN_SUCCESS) {
            mach_task_basic_info_data_t info{};
            mach_msg_type_number_t cnt = MACH_TASK_BASIC_INFO_COUNT;
            if (task_info(task, MACH_TASK_BASIC_INFO,
                          reinterpret_cast<task_info_t>(&info), &cnt) == KERN_SUCCESS) {
                pi.rss_bytes = info.resident_size;
            }
            mach_port_deallocate(mach_task_self(), task);
        }

        // FD count via proc_pidinfo
        int fd_count = proc_pidinfo(pi.pid, PROC_PIDLISTFDS, 0, nullptr, 0);
        pi.open_fds = static_cast<uint32_t>(
            std::max(0, fd_count / static_cast<int>(sizeof(proc_fdinfo))));

        result.push_back(pi);
    }
    return result;
}

// ── Thermal via IOKit SMC ────────────────────────────────────────────────────
namespace {

struct SMCKey {
    uint32_t key;
    SMCKeyData_vers_t vers{};
    SMCKeyData_pLimitData_t pLimitData{};
    SMCKeyData_keyInfo_t keyInfo{};
    uint8_t  result{};
    uint8_t  status{};
    uint8_t  data8{};
    uint32_t data32{};
    uint8_t  bytes[32]{};
};

static io_connect_t g_smc_conn = 0;

bool smc_open() {
    io_service_t svc = IOServiceGetMatchingService(
        kIOMainPortDefault, IOServiceMatching("AppleSMC"));
    if (!svc) return false;
    kern_return_t r = IOServiceOpen(svc, mach_task_self(), 0, &g_smc_conn);
    IOObjectRelease(svc);
    return r == KERN_SUCCESS;
}

void smc_close() {
    if (g_smc_conn) { IOServiceClose(g_smc_conn); g_smc_conn = 0; }
}

uint32_t str_to_key(const char* s) {
    return ((uint32_t)s[0] << 24) | ((uint32_t)s[1] << 16) |
           ((uint32_t)s[2] << 8)  |  (uint32_t)s[3];
}

double smc_read_temp(const char* key_str) {
    if (!g_smc_conn) return -1.0;
    SMCKeyData_t in{}, out{};
    in.key = str_to_key(key_str);
    in.data8 = SMC_CMD_READ_KEYINFO;
    kern_return_t r = IOConnectCallStructMethod(
        g_smc_conn, KERNEL_INDEX_SMC, &in, sizeof(in), &out, nullptr);
    if (r != KERN_SUCCESS) return -1.0;
    in.keyInfo.dataSize = out.keyInfo.dataSize;
    in.data8 = SMC_CMD_READ_BYTES;
    r = IOConnectCallStructMethod(
        g_smc_conn, KERNEL_INDEX_SMC, &in, sizeof(in), &out, nullptr);
    if (r != KERN_SUCCESS) return -1.0;
    // sp78 format: fixed point 7.8
    return ((out.bytes[0] * 256 + out.bytes[1]) >> 2) / 64.0;
}

// Known SMC thermal sensor keys
static const struct { const char* key; const char* label; const char* type; } k_sensors[] = {
    {"TC0P", "CPU Package",    "cpu"},
    {"TC0D", "CPU Die",        "cpu"},
    {"TC1C", "CPU Core 1",     "cpu"},
    {"TC2C", "CPU Core 2",     "cpu"},
    {"TG0P", "GPU Package",    "gpu"},
    {"Ts0S", "Memory Slot 0",  "memory"},
    {"TB0T", "Battery",        "battery"},
    {nullptr, nullptr, nullptr}
};

} // anon

std::vector<ThermalReading> thermal_readings() {
    std::vector<ThermalReading> result;
    if (!g_smc_conn) smc_open();
    for (int i = 0; k_sensors[i].key; ++i) {
        double t = smc_read_temp(k_sensors[i].key);
        if (t > 0.0)
            result.push_back({k_sensors[i].key, k_sensors[i].type, t});
    }
    if (result.empty()) smc_close();
    return result;
}

// ── Network stats via sysctl net.if.mib ─────────────────────────────────────
std::vector<NetStats> net_stats() {
    std::vector<NetStats> result;
    int mib_count[6] = {CTL_NET, PF_LINK, NETLINK_GENERIC, IFMIB_SYSTEM, IFMIB_IFCOUNT, 0};
    int ifc = 0;
    size_t sz = sizeof(ifc);
    if (sysctl(mib_count, 5, &ifc, &sz, nullptr, 0) != 0) return result;

    for (int i = 1; i <= ifc; ++i) {
        int mib[6] = {CTL_NET, PF_LINK, NETLINK_GENERIC, IFMIB_IFDATA, i, IFDATA_GENERAL};
        ifmibdata d{};
        sz = sizeof(d);
        if (sysctl(mib, 6, &d, &sz, nullptr, 0) != 0) continue;
        if (!(d.ifmd_flags & IFF_UP)) continue;
        NetStats n;
        n.iface    = d.ifmd_name;
        n.rx_bytes = d.ifmd_data.ifi_ibytes;
        n.tx_bytes = d.ifmd_data.ifi_obytes;
        result.push_back(n);
    }
    return result;
}

// ── CPU throttle via cpuctl / powermetrics ───────────────────────────────────
void throttle_cpu(double fraction) {
    // macOS does not expose a direct userland frequency scaling API.
    // Best-effort: set QoS class to background for heavy threads.
    // A production implementation would use powerd SPI or IOPMrootDomain.
    spdlog::debug("[OS/macOS] throttle_cpu({:.2f}) — QoS hint applied", fraction);
    if (fraction < 0.5) {
        pthread_set_qos_class_self_np(QOS_CLASS_BACKGROUND, 0);
    } else {
        pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
    }
}

int64_t now_ms() {
    struct timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}

} // namespace sip::os
#endif // SIP_OS_MACOS
