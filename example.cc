#include <utility>
#include <tuple>
#include <iostream>
#include <stack>
#include <cassert>


template<typename T>
class const_ref;

template<typename T>
class shared_ref;


// Object can be read, written and deleted through this reference/pointer.
// Should behave similar to unique_ptr.
template<typename T>
class owner_ref
{
    friend class shared_ref<T>;
    friend class const_ref<T>;

public:
    template<typename X, typename... Args>
    friend owner_ref<X> make_owner_ref(Args&&... args);

    owner_ref() = delete;
    owner_ref(const owner_ref&) = delete;
    owner_ref& operator=(const owner_ref&) = delete;
    owner_ref(owner_ref&& other) : _p(other._p)
    {
        assert(other._p != nullptr);
        // support on language level should allow skip run-time checks
        
        other._p = nullptr;
    }
    owner_ref& operator=(owner_ref&& other)
    {
        assert(other._p != nullptr);
        assert(_p == nullptr);
        // support on language level should allow skip run-time checks
        
        _p = other._p;
        other._p = nullptr;
        return *this;
    }
    ~owner_ref()
    {
        // support on language level should allow skip run-time checks
        if (_p != nullptr) {
            delete _p;
        }
    }

    T* operator->() const { return _p; }
    T& operator*() const { return *_p; }

private:
    explicit owner_ref(T* p) : _p(p) {}

    T* _p;
};

template<typename X, typename... Args>
owner_ref<X> make_owner_ref(Args&&... args)
{
    return owner_ref<X>(new X(std::forward<Args>(args)...));
}


// Object can be read and written through this reference/pointer.
// Should behave similar to ordinary reference.
template<typename T>
class shared_ref
{
    friend class const_ref<T>;

public:
    shared_ref(owner_ref<T>& src) : _p(src._p) {}

    T* operator->() const { return _p; }
    T& operator*() const { return *_p; }

private:
    T* _p;
};


// Object can be only read through this reference/pointer.
// Should behave simialr to const reference.
template<typename T>
class const_ref
{
public:
    const_ref(const owner_ref<T>& src) : _p(src._p) {}
    const_ref(const shared_ref<T>& src) : _p(src._p) {}

    const T* operator->() const { return _p; }
    const T& operator*() const { return *_p; }

private:
    const T* _p;
};



//TODO: duplicating of owner_ref
//TODO: shared_ptr analog that can be used in same places as owner_ref
//TODO: storing non-owning refs outside the stack, e.g. containers of shared_ref or const_ref


struct Trace
{
    Trace() { std::cout << "    ctor" << std::endl; }
    ~Trace() { std::cout << "    dtor" << std::endl; }
    Trace(const Trace&) { std::cout << "    copy ctor" << std::endl; }
    Trace(Trace&&) { std::cout << "    move ctor" << std::endl; }
    Trace& operator=(const Trace&) { std::cout << "    copy operator=" << std::endl; return *this; }
    Trace& operator=(Trace&&) { std::cout << "    move operator=" << std::endl; return *this; }

    void foo() const { std::cout << "    foo const" << std::endl; }
    void bar() { std::cout << "    bar" << std::endl; }
};

// producer or source of objects
// object is created inside the function and moved to caller
auto produce()
{
    return make_owner_ref<Trace>();
}

// can only view the object, modification is restricted
void look(const_ref<Trace> p)
{
    p->foo();

    // p->bar();
    // cant't change p's content
    // compile error: discarding qualifiers
}

// modification is allowed
void modify(shared_ref<Trace> p)
{
    // do anything you want with *p
    // caller is aware of it can be modified
    p->bar();
    look(p); // show p into look(), now worry - *p will not be changed
}

// consumer or sink for objects
// object was created somewhere outside the function
// moved inside
// and will be deleted after function ends
void consume(owner_ref<Trace>&& p)
{
    modify(p); // last modify before destroy
}


template<typename T>
class owning_stack
{
public:
    void push(owner_ref<T>&& p)
    {
        _v.push(std::move(p));
    }

    shared_ref<T> top()
    {
        return _v.top();
    }

    owner_ref<T> pop()
    {
        owner_ref<T> r = _v.top(); 
        _v.pop();
        return r;
    }

private:
    std::stack<owner_ref<T>> _v;
};


void test(shared_ref<owning_stack<Trace>> s)
{
    auto x = produce();

    modify(x); // easy to share an x into modify(), x can be modified, but not deleted

    s->push(std::move(x)); // should automatically add std::move()
    // from now any usage of x is invalid, except assign

    modify(s->top());
    look(s->top());

    x = produce();

    consume(std::move(x)); 
}


int main()
{
    owner_ref<owning_stack<Trace>> s = make_owner_ref<owning_stack<Trace>>();

    for (int i = 0; i < 2; i++) {
        printf("\nloop %d:\n", i);
        test(s);
    }
    printf("\nloop ended\n");


    return 0;
}


