// <FastList.hpp> -*- C++ -*-


/**
 * \file   FastList.hpp
 *
 * \brief File that defines the FastList class -- an alternative to
 * std::list when the user knows the size of the list ahead of time.
 */

#pragma once

#include <vector>
#include <iostream>
#include <exception>
#include <iterator>
#include <cinttypes>
#include <cassert>

#include "sparta/utils/SpartaAssert.hpp"

namespace sparta::utils
{
    /**
     * \class FastList
     * \brief An alternative to std::list, about 3x faster
     * \tparam T The object to maintain
     *
     * This class is a container type that allows back emplacement and
     * random deletion.  'std::list' provides the same type of
     * functionality, but performs heap allocation of the internal
     * nodes.  Under the covers, this class does not perform
     * new/delete of the Nodes, but reuses existing ones, performing
     * an inplace-new of the user's object.
     *
     * Testing shows this class is 2.5x faster than using std::list.
     * Caveats:
     *
     *  - The size of FastList is fixed to allow for optimization
     *  - The API isn't as complete as typicaly STL container types
     *
     */
    template <class T>
    class FastList
    {
        struct Node
        {
            // Stores the memory for an instance of 'T'.
            // Use placement new to construct the object and
            // manually invoke its dtor as necessary.
            typename std::aligned_storage<sizeof(T), alignof(T)>::type type_storage;

            Node(uint32_t _index) :
                index(_index)
            {}

            // Where this node is in the vector
            const uint32_t index;

            // Points to the next element or the next free
            // element if this node has been removed.
            int next = -1;

            // Points to the previous element.
            int prev = -1;

            // Is this node in use?
            bool in_use = false;
        };

        Node * advanceNode_(const Node * node) {
            Node * ret_node = (node->next == -1 ? nullptr : &nodes_[node->next]);
            return ret_node;
        }

    public:
        using value_type = T; //!< Handy using

        template<bool is_const = true>
        class NodeIterator // : public std::iterator<std::intput_iterator_tag, Node>
        {
            typedef std::conditional_t<is_const, const value_type &, value_type &> RefIteratorType;
            typedef std::conditional_t<is_const, const value_type *, value_type *> PtrIteratorType;
        public:

            NodeIterator() = default;

            NodeIterator(const NodeIterator<false> & iter) :
                node_(iter.node_)
            {}

            bool isValid() const { return (node_ != nullptr); }
            PtrIteratorType operator->()       {
                assert(isValid());
                return reinterpret_cast<value_type*>(&node_->type_storage);
            }
            PtrIteratorType operator->() const {
                assert(isValid());
                return reinterpret_cast<value_type*>(&node_->type_storage);
            }
            RefIteratorType operator* ()       {
                assert(isValid());
                return *reinterpret_cast<value_type*>(&node_->type_storage);
            }
            RefIteratorType operator* () const {
                assert(isValid());
                return *reinterpret_cast<value_type*>(&node_->type_storage);
            }

            int getIndex() const { return node_->index; }

            NodeIterator & operator++()
            {
                assert(isValid());
                node_ = flist_->advanceNode_(node_);
                return *this;
            }

            NodeIterator operator++(int)
            {
                NodeIterator orig = *this;
                assert(isValid());
                node_ = flist_->advanceNode_(node_);
                return orig;
            }

            bool operator!=(const NodeIterator &rhs)
            {
                return (rhs.flist_ != flist_) ||
                    (rhs.node_ != node_);
            }

            NodeIterator& operator=(const NodeIterator &rhs) = default;
            NodeIterator& operator=(NodeIterator &&rhs) = default;

        private:
            friend class FastList<T>;

            NodeIterator(FastList * flist, Node * node) :
                flist_(flist),
                node_(node)
            { }

            FastList * flist_ = nullptr;
            Node     * node_  = nullptr;
        };

        /**
         * \brief Construct FastList of a given size
         * \param size Fixed size of the list
         */
        FastList(size_t size)
        {
            nodes_.reserve(size);
            int node_idx = 0;
            for(size_t i = 0; i < size; ++i) {
                Node n(node_idx);
                n.prev = node_idx - 1;
                n.next = node_idx + 1;
                ++node_idx;
                nodes_.emplace_back(n);
            }
            nodes_.back().next = -1;
        }

        using iterator       = NodeIterator<false>;  //!< Iterator type
        using const_iterator = NodeIterator<true>;   //!< Iterator type, const

        //! Obtain a beginning iterator
        iterator       begin()       { return iterator(this, (first_node_ == -1 ? nullptr : &nodes_[first_node_])); }

        //! Obtain a beginning const_iterator
        const_iterator begin() const { return const_iterator(this, (first_node_ == -1 ? nullptr : &nodes_[first_node_])); }

        //! Obtain an end iterator
        iterator       end()       { return iterator(this, nullptr); }
        //! Obtain an end const_iterator
        const_iterator end() const { return const_iterator(this, nullptr); }

        //! \return Is this container empty?
        bool   empty()    const { return size_ == 0; }

        //! \return The current size of the container
        size_t size()     const { return size_; };

        //! \return The maximum size of this list
        size_t max_size() const { return nodes_.capacity(); };

        ////////////////////////////////////////////////////////////////////////////////
        // Modifiers
        void     clear()  noexcept { sparta_assert(!"Not implemented yet"); }
        iterator insert(iterator pos, const T& value) noexcept
        { sparta_assert(!"Not implemented yet"); return iterator(nullptr, nullptr); }

        template<class ...ArgsT>
        iterator emplace(const_iterator pos, ArgsT&&...args);

        /**
         * \brief Erase an element with the given iterator
         * \param entry Iterator to the entry being erased
         */
        iterator erase(const const_iterator & entry)
        {
            const auto node_idx = entry.getIndex();
            auto & curr_node = nodes_[node_idx];
            assert(curr_node.in_use == true);
            reinterpret_cast<T*>(&curr_node.type_storage)->~T();
            curr_node.in_use = false;

            if(first_node_ == node_idx) {
                first_node_ = curr_node.next;
            }

            if(SPARTA_EXPECT_FALSE(curr_node.next != -1))
            {
                auto & next_node = nodes_[curr_node.next];
                next_node.prev = curr_node.prev;
            }

            if(SPARTA_EXPECT_FALSE(curr_node.prev != -1))
            {
                auto & prev_node = nodes_[curr_node.prev];
                prev_node.next = curr_node.next;
            }

            if(SPARTA_EXPECT_TRUE(free_head_ != -1)) {
                nodes_[free_head_].prev = node_idx;
                curr_node.next = free_head_;
            }
            free_head_ = node_idx;
            --size_;
            return iterator(nullptr, nullptr);
        }

        /**
         * \brief Add an element to the back of the list
         * \tparam args Arguments to be passed to the user type for construction
         * \return iterator to the newly emplaced object
         */
        template<class ...ArgsT>
        iterator emplace_back(ArgsT&&...args)
        {
            assert(free_head_ != -1);

            auto & n = nodes_[free_head_];
            new (&n.type_storage) T(args...);
            n.in_use = true;

            if(SPARTA_EXPECT_TRUE(first_node_ != -1))
            {
                auto & old_first = nodes_[first_node_];
                const int old_first_idx = first_node_;
                first_node_ = free_head_;
                free_head_ = n.next;

                old_first.prev = first_node_;
                n.next = old_first_idx;
                n.prev = -1;
            }
            else {
                first_node_ = free_head_;
                free_head_ = n.next;
                n.next = -1;
            }
            ++size_;
            return iterator(this, &n);
        }

        void pop_back() { sparta_assert(!"Not implemented yet"); }
        void push_front() { sparta_assert(!"Not implemented yet"); }

    private:

        // Friendly printer
        friend std::ostream & operator<<(std::ostream & os, const FastList<T> & fl)
        {
            int next_node = fl.first_node_;
            if(next_node == -1) {
                os << "<empty>" << std::endl;
            }
            else {
                int index = fl.size_ - 1;
                do
                {
                    const auto & n = fl.nodes_[next_node];
                    os << index << " elem="    << *reinterpret_cast<const T*>(&n.type_storage)
                       << " n.next=" << n.next
                       << " n.prev=" << n.prev << std::endl;
                    next_node = n.next;
                    --index;
                } while(next_node != -1);
            }
            return os;
        }

        // Stores all the nodes.
        std::vector<Node> nodes_;

        int free_head_  = 0;  //!< The free head
        int first_node_ = -1; //!< The first node in the list (-1 for empty)
        size_t size_    = 0;  //!< The number of elements in the list
    };
}
