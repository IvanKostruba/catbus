#pragma once

#include <type_traits>
#include <utility>

namespace catbus {

namespace _detail {
    struct vtable {
        void (*run)(void* ptr);

        void (*destroy_)(void* ptr);
        void (*clone)(void* storage, const void* ptr);
        void (*move_clone)(void* storage, void* ptr);
    };

    template<typename Handler, typename Event>
    constexpr vtable vtable_ctx_for {
        [](void* ptr) {
            auto* p = static_cast<std::pair<Handler, Event>*>(ptr);
            p->first->Handle(std::move(p->second));
        },

        [](void* ptr) {
            static_cast<std::pair<Handler, Event>*>(ptr)->~pair();
        },
        [](void* storage, const void* ptr) {
            new (storage) std::pair<Handler, Event>{
                *static_cast<const std::pair<Handler, Event>*>(ptr)};
        },
        [](void* storage, void* ptr) {
            new (storage) std::pair<Handler, Event>{
                std::move(*static_cast<std::pair<Handler, Event>*>(ptr))};
        }
    };
};  // namespace detail

class TaskWrapper {
public:
    TaskWrapper()
        : vtable_{nullptr} {
    }

    template<typename Handler, typename Event>
    TaskWrapper(Handler x, Event c)
        : vtable_{&_detail::vtable_ctx_for<Handler, Event>}
    {
        static_assert(sizeof(std::pair<Handler, Event>) <= sizeof(buf_),
            "Wrapper buffer is too small!");
        new(&buf_) std::pair<Handler, Event>{std::move(x), std::move(c)};
    }

    ~TaskWrapper() {
        if (vtable_) {
            vtable_->destroy_(&buf_);
        }
    }

    TaskWrapper(const TaskWrapper& other) {
        other.vtable_->clone(&buf_, &other.buf_);
        vtable_ = other.vtable_;
    }

    TaskWrapper(TaskWrapper&& other) noexcept {
        other.vtable_->move_clone(&buf_, &other.buf_);
        vtable_ = other.vtable_;
        other.vtable_ = nullptr;
    }

    TaskWrapper& operator=(const TaskWrapper& other) {
        if (vtable_) {
            vtable_->destroy_(&buf_);
        }
        if (other.vtable_) {
            other.vtable_->clone(&buf_, &other.buf_);
        }
        vtable_ = other.vtable_;
        return *this;
    }
    TaskWrapper& operator=(TaskWrapper&& other) noexcept {
        if (vtable_) {
            vtable_->destroy_(&buf_);
        }
        if (other.vtable_) {
            other.vtable_->move_clone(&buf_, &other.buf_);
        }
        vtable_ = other.vtable_;
        other.vtable_ = nullptr;
        return *this;
    }

    void run() {
        vtable_->run(&buf_);
    }

    bool is_valid() const {
        return vtable_ != nullptr;
    }

private:
    std::aligned_storage_t<64> buf_;
    const _detail::vtable* vtable_;
};

};
