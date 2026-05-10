#include "core/event_loop.hpp"
#include <uv.h>
#include <spdlog/spdlog.h>
#include <functional>
#include <vector>
#include <memory>

namespace sip {

struct WatchCtx {
    EventLoop::FsCallback cb;
    std::string path;
    uv_fs_event_t handle;
};

struct EventLoop::Impl {
    uv_loop_t loop;
    std::vector<std::unique_ptr<WatchCtx>> watchers;
};

static void on_fs_event(uv_fs_event_t* h, const char* fn, int events, int) {
    auto* ctx = static_cast<WatchCtx*>(h->data);
    std::string path = ctx->path;
    if (fn) path += std::string("/") + fn;
    ctx->cb(path, events);
}

EventLoop::EventLoop() : impl_(std::make_unique<Impl>()) {
    uv_loop_init(&impl_->loop);
}

EventLoop::~EventLoop() {
    impl_->watchers.clear();
    uv_loop_close(&impl_->loop);
}

uv_loop_s* EventLoop::raw() { return &impl_->loop; }

void EventLoop::watch_path(const std::string& path, FsCallback cb) {
    auto ctx = std::make_unique<WatchCtx>();
    ctx->cb   = std::move(cb);
    ctx->path = path;
    uv_fs_event_init(&impl_->loop, &ctx->handle);
    ctx->handle.data = ctx.get();
    uv_fs_event_start(&ctx->handle, on_fs_event, path.c_str(),
                      UV_FS_EVENT_RECURSIVE);
    impl_->watchers.push_back(std::move(ctx));
    spdlog::debug("[EventLoop] watching: {}", path);
}

void EventLoop::poll() { uv_run(&impl_->loop, UV_RUN_NOWAIT); }
void EventLoop::run()  { uv_run(&impl_->loop, UV_RUN_DEFAULT); }
void EventLoop::stop() { uv_stop(&impl_->loop); }

} // namespace sip
