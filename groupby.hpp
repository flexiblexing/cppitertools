#ifndef ITER_GROUP_BY_HPP_
#define ITER_GROUP_BY_HPP_

#include "iterbase.hpp"

#include <utility>
#include <iterator>
#include <initializer_list>

namespace iter {

    template <typename Container, typename KeyFunc>
    class GroupBy;

    template <typename Container, typename KeyFunc>
    GroupBy<Container, KeyFunc> groupby(Container&&, KeyFunc);

    template <typename T, typename KeyFunc>
    GroupBy<std::initializer_list<T>, KeyFunc> groupby(
            std::initializer_list<T>, KeyFunc);

    template <typename Container, typename KeyFunc>
    class GroupBy  {
        private:
            Container container;
            KeyFunc key_func;

            friend GroupBy groupby<Container, KeyFunc>(Container&&, KeyFunc);

            template <typename T, typename KF>
            friend GroupBy<std::initializer_list<T>, KF> groupby(
                    std::initializer_list<T>, KF);

            using key_func_ret =
                decltype(std::declval<KeyFunc>()(
                            std::declval<iterator_deref<Container>>()));

            GroupBy(Container&& container, KeyFunc key_func)
                : container(std::forward<Container>(container)),
                key_func(key_func)
            { }

        public:
            GroupBy() = delete;
            GroupBy(const GroupBy&) = delete;
            GroupBy& operator=(const GroupBy&) = delete;
            GroupBy& operator=(GroupBy&&) = delete;

            GroupBy(GroupBy&&) = default;

            class Iterator;
            class Group;

        private:
            using KeyGroupPair =
                std::pair<key_func_ret, Group>;
        public:

            class Iterator 
                : public std::iterator<std::input_iterator_tag, KeyGroupPair>
            {
                private:
                    iterator_type<Container> sub_iter;
                    iterator_type<Container> sub_iter_peek;
                    iterator_type<Container> sub_end;
                    KeyFunc key_func;

                public:
                    Iterator (iterator_type<Container> si,
                              iterator_type<Container> end,
                              KeyFunc key_func)
                        : sub_iter{si},
                        sub_end{end},
                        key_func(key_func)
                    { }

                    KeyGroupPair operator*() {
                        return KeyGroupPair(
                                this->key_func(*this->sub_iter),
                                Group(
                                    *this, 
                                    this->key_func(*this->sub_iter)));
                    }

                    Iterator& operator++() { 
                        return *this;
                    }

                    Iterator operator++(int) {
                        auto ret = *this;
                        ++*this;
                        return ret;
                    }

                    bool operator!=(const Iterator&) const {
                        return !this->exhausted();
                    }

                    bool operator==(const Iterator& other) const {
                        return !(*this != other);
                    }

                    void increment_iterator() {
                        if (this->sub_iter != this->sub_end) {
                            ++this->sub_iter;
                        }
                    }

                    bool exhausted() const {
                        return !(this->sub_iter != this->sub_end);
                    }

                    iterator_deref<Container> current() {
                        return *this->sub_iter;
                    }

                    key_func_ret next_key() {
                        return this->key_func(*this->sub_iter);
                    }
            };


            class Group {
                private:
                    friend Iterator;
                    friend class GroupIterator;
                    Iterator& owner;
                    key_func_ret key;

                    // completed is set if a Group is iterated through
                    // completely.  It is checked in the destructor, and
                    // if the Group has not been completed, the destructor
                    // exhausts it.  This ensures that the next Group starts
                    // at the correct position when the user short-circuits
                    // iteration over a Group.
                    // The move constructor sets the rvalue's completed
                    // attribute to true, so its destructor doesn't do anything
                    // when called.
                    bool completed = false;

                    Group(Iterator& owner, key_func_ret key) :
                        owner(owner),
                        key(key)
                    { }

                public:
                    ~Group() {
                        if (!this->completed) {
                            for (auto iter = this->begin(), end = this->end();
                                    iter != end;
                                    ++iter) { }
                        }
                    }
                           
                    // move-constructible, non-copy-constructible,
                    // non-assignable
                    Group() = delete;
                    Group(const Group&) = delete;
                    Group& operator=(const Group&) = delete;
                    Group& operator=(Group&&) = delete;

                    Group(Group&& other)
                            : owner{other.owner},
                            key{other.key},
                            completed{other.completed} {
                        other.completed = true;
                    }

                    class GroupIterator 
                        : public std::iterator<std::input_iterator_tag,
                                iterator_traits_deref<Container>>
                    {
                        private:
                            key_func_ret key;
                            Group *group_p;

                            bool not_at_end() {
                                return !this->group_p->owner.exhausted()&&
                                    this->group_p->owner.next_key() == this->key;
                            }

                        public:
                            GroupIterator(Group *in_group_p,
                                          key_func_ret key)
                                : key{key},
                                group_p{in_group_p}
                            { }

                            bool operator!=(const GroupIterator& other) const {
                                return !(*this == other);
                            }

                            bool operator==(const GroupIterator& other) const {
                                return this->group_p == other.group_p;
                            }

                            GroupIterator& operator++() {
                                this->group_p->owner.increment_iterator();
                                if (!this->not_at_end()) {
                                    this->group_p->completed = true;
                                    this->group_p = nullptr;
                                }
                                return *this;
                            }

                            GroupIterator operator++(int) {
                                auto ret = *this;
                                ++*this;
                                return ret;
                            }

                            iterator_deref<Container> operator*() {
                                return this->group_p->owner.current();
                            }
                    };

                    GroupIterator begin() {
                        return {this, key};
                    }

                    GroupIterator end() {
                        return {nullptr, key};
                    }

            };


            Iterator begin() {
                return {std::begin(this->container),
                        std::end(this->container),
                        this->key_func};
            }

            Iterator end() {
                return {std::end(this->container),
                        std::end(this->container),
                        this->key_func};
            }

    };

    // Takes something and returns it, used for default key of comparing
    // items in the sequence directly
    template <typename Container>
    class ItemReturner {
        public:
            iterator_deref<Container> operator() (
                    iterator_deref<Container> item) const {
                return item;
            }
    };


    template <typename Container, typename KeyFunc>
    GroupBy<Container, KeyFunc> groupby(
            Container&& container, KeyFunc key_func) {
        return {std::forward<Container>(container), key_func};
    }


    template <typename Container>
    auto groupby(Container&& container) ->
            decltype(groupby(std::forward<Container>(container),
                        ItemReturner<Container>())) {
        return groupby(std::forward<Container>(container),
                ItemReturner<Container>());
    }

    
    template <typename T, typename KeyFunc>
    GroupBy<std::initializer_list<T>, KeyFunc> groupby(
            std::initializer_list<T> il, KeyFunc key_func) {
        return {std::move(il), key_func};
    }


    template <typename T>
    auto groupby(std::initializer_list<T> il) ->
            decltype(groupby(std::move(il),
                        ItemReturner<std::initializer_list<T>>())) {
        return groupby(
                std::move(il),
                ItemReturner<std::initializer_list<T>>());
    }

}


#endif
