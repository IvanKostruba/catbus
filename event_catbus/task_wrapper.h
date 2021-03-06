#pragma once

#include <type_traits>
#include <utility>

namespace catbus {

namespace _detail {
    struct vtable {
        void (*run)(void* ptr, std::size_t q);

        void (*destroy_)(void* ptr);
        void (*clone)(void* storage, const void* ptr);
        void (*move_clone)(void* storage, void* ptr);
    };

    template<typename Handler, typename Event>
    constexpr vtable vtable_for {
        [](void* ptr, std::size_t q) {
            auto* p = static_cast<std::pair<Handler, Event>*>(ptr);
            p->first->handle(std::move(p->second), q);
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

// Previously tasks were enqueued as std::function bound to lambda, which captured handler ref and
// the event. But std::function is extremely slow, so it was replaced with this wrapper. While it
// puts a limit on event size (adjustable though), it works ~35% faster. In case of variable event
// sizes it can be relatively easily swithced to heap-allocated storage with SBO.
class TaskWrapper {
public:
    TaskWrapper()
        : vtable_{nullptr}
    {}

    template<typename Handler, typename Event>
    TaskWrapper(Handler x, Event c)
        : vtable_{&_detail::vtable_for<Handler, Event>}
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

    void run(std::size_t q) {
        vtable_->run(&buf_, q);
    }

    bool is_valid() const {
        return vtable_ != nullptr;
    }

private:
    std::aligned_storage_t<64> buf_;
    const _detail::vtable* vtable_;
};

};
