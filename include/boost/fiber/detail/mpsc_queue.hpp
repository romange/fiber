// based on Dmitry Vyukov's intrusive MPSC queue
// http://www.1024cores.net/home/lock-free-algorithms/queues/intrusive-mpsc-node-based-queue
// https://groups.google.com/forum/#!topic/lock-free/aFHvZhu1G-0
//          Copyright Dmitry Vyukov 2014.
//          Copyright Edvard Severin Pettersen 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE.md or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_FIBER_DETAIL_MPSC_QUEUE_H
#define BOOST_FIBER_DETAIL_MPSC_QUEUE_H

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

#include <boost/fiber/detail/config.hpp>

namespace boost {
namespace fibers {
namespace detail {

template<typename T> class mpsc {
private:
    using ItemT = T;
    using StorageT = typename std::aligned_storage<
        sizeof( T ), alignof( T )>::type;

    alignas(cache_alignment) StorageT    storage_{};
    ItemT *                              stub_;

    alignas(cache_alignment) std::atomic< ItemT * >    head_;
    alignas(cache_alignment) ItemT *                   tail_;

public:
    // constructor and destructor
    mpsc()
        : stub_{ reinterpret_cast< ItemT * >( std::addressof( storage_ ) ) }
        , head_{ stub_ }
        , tail_{ stub_ }
    {
        stub_->mpsc_next_.store( nullptr, std::memory_order_release );
    }
    ~mpsc() {}

    // make non-copyable
    mpsc( mpsc const & )               = delete;
    mpsc & operator = ( mpsc const & ) = delete;

    // called by multiple producers. last item is stored in the head.
    void push( ItemT * item ) noexcept
    {
        BOOST_ASSERT( item != nullptr );
        item->mpsc_next_.store( nullptr, std::memory_order_release );
        ItemT * prev = head_.exchange( item, std::memory_order_acq_rel );
        prev->mpsc_next_.store( item, std::memory_order_release );
    }

    // called by single consumer
    ItemT * pop() noexcept
    {
        ItemT * tail = tail_;
        ItemT * next = tail->mpsc_next_.load( std::memory_order_acquire );
        if ( tail == stub_ ) {
            if ( next == nullptr ) {
                return nullptr;
            }
            tail_ = next;
            tail = next;
            next = next->mpsc_next_.load( std::memory_order_acquire );
        }
        if ( next != nullptr ) {
            tail_ = next;
            tail->mpsc_next_.store( nullptr, std::memory_order_release );
            return tail;
        }
        ItemT * head = head_.load( std::memory_order_acquire );
        if ( tail != head ) {
            return nullptr;
        }
        push( stub_ );
        next = tail->mpsc_next_.load( std::memory_order_acquire );
        if ( next != nullptr ) {
            tail_ = next;
            tail->mpsc_next_.store( nullptr, std::memory_order_release );
            return tail;
        }
        return nullptr;
    }
};

}}}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif

#endif // BOOST_FIBER_DETAIL_MPSC_QUEUE_H
