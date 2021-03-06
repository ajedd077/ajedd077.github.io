#include <iostream> 
#include <memory>
#include <bits/shared_ptr_base.h> 
#include <atomic>
#include <mutex>
#include <vector>

namespace ver5
{ 

enum class LockPolicy 
{
   single, 
   atomic, 
   mutex
}; 



constexpr LockPolicy default_lock_policy() { return LockPolicy::mutex; }

template <LockPolicy P>
struct lock_policy_mutex {}; 

template <>
struct lock_policy_mutex<LockPolicy::mutex> 
{
   std::mutex _ref_cntr_mutex;  
}; 


template <LockPolicy P> 
struct ref_counter_ptr_base : lock_policy_mutex<P>
{ 
  using counter_type = typename std::conditional<P == LockPolicy::atomic, std::atomic<int>, int>::type; 
  counter_type _ref_cntr{1}; 
    
  void acquire(); 
  void release(); 
  
  virtual void dispose() = 0; 
  virtual ~ref_counter_ptr_base() = default; 
 
};


 
template <LockPolicy P> 
void ref_counter_ptr_base<P>::acquire() 
{ 
  std::cout << "acquire lock policy: general \n"; 
  ++_ref_cntr;
} 

template <LockPolicy P> 
void ref_counter_ptr_base<P>::release()
{
  std::cout << "release lock policy: general\n"; 
  if (--_ref_cntr == 0) 
  { 
    dispose(); 
    delete this;  
  }

}

template <> 
void ref_counter_ptr_base<LockPolicy::mutex>::acquire() 
{ 
  std::cout << "acquire lock policy: mutex \n"; 
  std::lock_guard<std::mutex> lock{_ref_cntr_mutex};
  ++_ref_cntr;
} 

template <> 
void ref_counter_ptr_base<LockPolicy::mutex>::release()
{
  std::cout << "release lock policy: mutex\n"; 
  std::lock_guard<std::mutex> lock{_ref_cntr_mutex};
  if (--_ref_cntr == 0) 
  { 
    dispose(); 
    delete this;  
  }

}

template <> 
void ref_counter_ptr_base<LockPolicy::atomic>::release()
{
  std::cout << "release lock policy: atomic\n"; 
  if (--_ref_cntr == 0) 
  { 
    dispose(); 
    delete this;  
  }

}


template <class T, LockPolicy P> 
struct ref_counter_ptr_default final : public ref_counter_ptr_base<P>
{
  ref_counter_ptr_default(T* ptr) : _ptr{ptr} {} 
  
  void dispose() override 
  { 
    std::cout << "Deleting with default \n"; 
    delete _ptr; 
  }

  T* _ptr; 
  
};

template <class T, typename Deleter, LockPolicy P> 
struct ref_counter_ptr_deleter final : public ref_counter_ptr_base<P>
{ 
  ref_counter_ptr_deleter(T* ptr, Deleter d)
   : _ptr{ptr} 
   , _deleter{d}
  {} 
   
  void dispose() override 
  { 
    deleter(_ptr); 
  }
  
  T* _ptr;
  Deleter _deleter; 
};

template <class T, LockPolicy P> 
struct shared_ptr_counter 
{
   shared_ptr_counter(T* ptr=nullptr); 
   
   template <typename Deleter> 
   shared_ptr_counter(T* ptr, Deleter d); 
   
   shared_ptr_counter(shared_ptr_counter const&); 
   shared_ptr_counter& operator=(shared_ptr_counter const&); 
   ~shared_ptr_counter(); 
   
   ref_counter_ptr_base<P>* _ref_cntr{nullptr}; 
}; 


template <class T, LockPolicy P> 
shared_ptr_counter<T, P>::shared_ptr_counter(T* ptr) 
{ 
  
  try
  { 
    _ref_cntr = new ref_counter_ptr_default<T, P>(ptr); 
  } 
  catch (...)     
  { 
    delete ptr;  
    throw; 
  }   

} 

template <class T, LockPolicy P> 
template <typename Deleter>
shared_ptr_counter<T, P>::shared_ptr_counter(T* ptr, Deleter d)
{ 
  try
  { 
    _ref_cntr = new ref_counter_ptr_deleter<T, Deleter, P>(ptr, d); 
  } 
  catch (...)     
  { 
    d(ptr);   
    throw; 
  }  
} 

template <class T, LockPolicy P>  
shared_ptr_counter<T, P>::shared_ptr_counter(shared_ptr_counter<T, P> const& rhs) 
{ 
    _ref_cntr = rhs._ref_cntr; 
    if (_ref_cntr != nullptr) 
        _ref_cntr->acquire(); 
} 


template <class T, LockPolicy P> 
shared_ptr_counter<T, P>& shared_ptr_counter<T, P>::operator=(shared_ptr_counter<T, P> const& rhs) 
{  
  if (this != &rhs) 
  {
    if (_ref_cntr != nullptr)  _ref_cntr->release();  // release the current _ref_cntr. 
    _ref_cntr = rhs._ref_cntr;  // change the pointer of the current
                                // ref_cntr to rhs._ref_cntr. 
                                
    if (_ref_cntr != nullptr) _ref_cntr->acquire();     // acquire now. 
  }
  return *this; 
} 



template <class T, LockPolicy P> 
shared_ptr_counter<T, P>::~shared_ptr_counter() 
{
    if (_ref_cntr != nullptr) 
        _ref_cntr->release(); 
}  

//  This was the ref_counter_ptr_t .. how does the 
// the shared_ptr looks like now. 
template <class T, LockPolicy P = default_lock_policy()> 
struct shared_ptr
{
    shared_ptr(T* ptr=nullptr)
     : _shared_ptr_ctr{ptr} 
     , _ptr{ptr} 
    {} 
    
    template <typename Deleter> 
    shared_ptr(T* ptr, Deleter d)
      : _shared_ptr_ctr{ptr, d} 
      , _ptr{ptr} 
    {} 
    
    shared_ptr_counter<T, P>   _shared_ptr_ctr{nullptr}; 
    T* _ptr{nullptr};  
};






 
    
} 

 

// --------------------------------
struct test_struct
{ 
   test_struct() { std::cout << "cstr\n"; }
   ~test_struct() { std::cout << "~dstrct\n"; }
};

template <typename T>
struct default_deleter
{
   void operator()(T* ptr) 
   { 
     std::cout << "Deleting with the customized default_deleter \n"; 
     delete ptr; 
   } 
}; 

template <class T, class ShPtr> 
ShPtr make_shared_ptr() 
{
    return ShPtr(new T); 
} 

/*

void vector_of_void_ptr() 
{
  std::cout << "running vector_of_void_ptr\n"; 
  {
  std::vector<void*> vec; 
  vec.emplace_back(new test_struct{}); 
  void* ptr = vec.back();  
  vec.pop_back(); 
  delete ptr;    // undefined behaviour. test_struct is never called.
  }
  std::cout << " ... done naked ptr version\n"; 
} 

void vector_of_void_shared_ptr() 
{
  std::cout << "running vector_of_void_shared_ptr \n"; 
  {
  std::vector<std::shared_ptr<void>> vec; 
  vec.emplace_back(new test_struct{}); 
  }
  std::cout << " ... done shared_ptr version\n"; 
}
*/ 

int main() 
{
    
    std::cout << "created s1 \n";
    auto s1 = ver5::shared_ptr<test_struct>(new test_struct);
    
    std::cout << "created s2 \n";  
    auto s2 = ver5::shared_ptr<test_struct>(new test_struct); 
    
    std::cout << "s3=s1\n"; 
    auto s3 = s1; 
    
    std::cout << "s3=s2\n";
    s3 = s2; 
    
    std::cout << "s3=s3\n"; 
    s3 = s3;
    
    std::cout << "s3=s2\n";
    s3 = s2; 
    
    std::cout << "s3=s2\n";
    s3 = s2; 
    
    std::cout << "calling destructors now\n"; 

    // // How to use __shared_ptr? 
    // std::__shared_ptr<int> _sp_in_default;  
    // std::__shared_ptr<int, std::_Lock_policy::_S_single> _sp_in_single; 
    // (void) _sp_in_single; 
    
    // vector_of_void_ptr();
    // vector_of_void_shared_ptr();
    /* 
    running vector_of_void_ptr
    cstr
    ... done naked ptr version
    running vector_of_void_shared_ptr 
    cstr
    ~dstrct
    ... done shared_ptr version
    */ 
}
