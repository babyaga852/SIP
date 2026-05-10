#include "core/storage.hpp"
#include <sqlite3.h>
#include <spdlog/spdlog.h>
#include <fstream>
#include <stdexcept>

namespace sip {

Storage::Storage(std::string db_path) {
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK)
        throw std::runtime_error("Storage: cannot open DB: " + db_path);
    exec("CREATE TABLE IF NOT EXISTS kv "
         "(key TEXT PRIMARY KEY, value TEXT NOT NULL)");
    exec("CREATE TABLE IF NOT EXISTS metrics "
         "(id INTEGER PRIMARY KEY, series TEXT, value REAL, meta TEXT, ts INTEGER)");
}

Storage::~Storage() { if (db_) sqlite3_close(db_); }

void Storage::exec(const std::string& sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg(err); sqlite3_free(err);
        throw std::runtime_error("Storage::exec: " + msg);
    }
}

void Storage::set(const std::string& key, const nlohmann::json& value) {
    std::string sql = "INSERT OR REPLACE INTO kv(key,value) VALUES(?,?)";
    sqlite3_stmt* stmt; sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    auto s = value.dump();
    sqlite3_bind_text(stmt, 1, key.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, s.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt); sqlite3_finalize(stmt);
}

nlohmann::json Storage::get(const std::string& key) const {
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, "SELECT value FROM kv WHERE key=?", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    nlohmann::json result;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = nlohmann::json::parse(
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
    sqlite3_finalize(stmt);
    return result;
}

bool Storage::has(const std::string& key) const {
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, "SELECT 1 FROM kv WHERE key=?", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt); return found;
}

void Storage::del(const std::string& key) {
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, "DELETE FROM kv WHERE key=?", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt); sqlite3_finalize(stmt);
}

void Storage::snapshot(const std::string& path) const {
    nlohmann::json out;
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, "SELECT key,value FROM kv", -1, &stmt, nullptr);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        auto key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        auto val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        out[key] = nlohmann::json::parse(val);
    }
    sqlite3_finalize(stmt);
    std::ofstream(path) << out.dump(2);
}

void Storage::append_metric(const std::string& series, double value,
                             const std::string& meta) {
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_,
        "INSERT INTO metrics(series,value,meta,ts) VALUES(?,?,?,?)",
        -1, &stmt, nullptr);
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    sqlite3_bind_text(stmt, 1, series.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, value);
    sqlite3_bind_text(stmt, 3, meta.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, now);
    sqlite3_step(stmt); sqlite3_finalize(stmt);
}

} // namespace sip
