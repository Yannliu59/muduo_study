# Chapter01 线程安全的的对象生命期管理 #

### 1.1 当析构函数遇到多线程

------

在多线程场景时，可以用同步原语保护对象的内部状态，但是对象的创建和消亡不能由对象自身拥有的`mutex`保护，在对象的销毁时，可以会出现多种竞态条件`(race condition)`, 处理这些竞态条件是多线程面临的基本问题，可以借助`Boost`库中的`shared_ptr`和`weak_ptr`完美解决。

**race condition：**

+ 析构对象时，别的线程正在执行该对象的成员函数
+ 执行成员函数时，对象在另一个线程被析构
+ 调用某个对象的成员函数之前，对象的析构函数正执行到一半

#### 1.1.2 MutexLock 与 MutexLockGuard（&2.4）

> > MutexLock封装临界区（critial section），这是一个简单的资源类，用RAII（Resource Acquisition is Initialization，即在构造函数中获取资源，在析构函数中释放资源）手法封装互斥器的创建于销毁。Linux临界区是`Pthread_mutex_t`默认不可重入（&2.1.1）。Mutex一般是别的对象的数据成员。
> >
> > MutexLockGuard封装临界区的进入与推出，即加锁与解锁。MutexLockGuard一般是个栈上对象，它的作用域刚好等于临界区域，这两个class都不允许拷贝构造和赋值，使用原则见（2.1）

[RAII介绍与用法](https://www.cnblogs.com/jiangbin/p/6986511.html)

#### 1.1.3 一个线程安全的counter示例

[const关键字](https://www.cnblogs.com/kevinWu7/p/10163449.html)

[mutable关键字](https://blog.csdn.net/aaa123524457/article/details/80967330)

### 1.2 对象的创建很简单

对象构造要做到线程安全，要求在构造期间不要泄露this指针，即：

+ 不要在构造函数中注册任何回调
+ 也不要在构造函数中把this传给跨线程的对象
+ 即便在构造函数的最后一行也不行

防止this被泄露给其他对象，被其他线程访问这个半成品对象，造成难以预料的后果

~~~c++
// Don't do this
class Foo : public Observer
{
    public:
    Foo(Observable* s)
    {
        s->register_(this);//错误，非线程安全
    }
	virtual void update();
};
// Don't this
class Foo : public Observer
{
    public:
    Foo();
    virtual void update();
    
    // 另外定义一个函数，在构造之后执行回调函数的注册工作
    void observe(Observable* s)
    {
        s->register_(this);
    }
};

Foo* pFoo = new Foo;
Observable* s = getSubject();
pFoo->observe(s); // 二段式构造，或者直接写 s->register_(pFOo);
~~~

二段式构造---构造函数+初始化----是好办法，但是不符合C++教条，但是多线程之下别无选择。即使构造函数最后一行也不要泄露this，因为Foo有可能是个基类，基类先于派生类构造，执行完Foo::Foo()的最后一行代码还会执行派生类的构造函数。

### 1.3 销毁太难

> > 在多线程并发时，需要用互斥器mutex保护临界区，但是析构函数会把mutex成员变量销毁，由此会导致错误。作为class数据成员的MutexLock只能用于同步本class的其他数据的读写，它不能保护安全的析构。对于基类对象，调用基类析构函数的时候，派生类那部分已经析构了，那么基类对象拥有的MutexLock不能保护整个析构过程。
> >
> > 如果一个函数要锁住相同类型的多个对象，为了保证始终按相同的顺序加锁，我们可以比较mutex对象的地址，始终先加锁地址较小的mutex

### 1.4 线程安全的Observer有多难

无法通过指针和引用看出一个动态创建的对象是否活着，如果一块内存上的对象已经销毁，那么就根本无法访问。判断指针是不是合法指针有没有高效的办法，是c++指针问题的根源

面向对象程序设计中，对象的关系主要三种：**composition，aggregation，association**. **composition (组合)** 关系在多线程里安全，因为对象x的生命期是由其唯一的拥有者owner控制，owner析构时会把x也析构掉。后两种关系比较难办，可能会造成内存泄漏或者重复释放，**association (关联)** 是一种很宽泛的关系，它表示一个对象a用到了另一个对象b，调用了后者的成员函数。从形式上看，a持有b的指针 (或者引用) ，但是b的生命期不由a单独控制。 **aggregation (聚合)**关系从形式上看与association相同，除了a和b有逻辑上的整体与部分关系。如果b是动态创建并且在程序结束之前有可能提前释放，就有可能出现竞态条件。

### 1.5 原始指针有何不妥

指向对象的原始指针 (raw pointer) 是坏的，尤其当暴露给别的线程时。Observable应当保存的不是原始的Observer*，而是别的什么东西，能分辨Observer对象是否存活。类似的，如果在Observer要在析构函数里解注册，那么也不能使用原始的Observable *

这里可以使用智能指针，但是需要注意，直接使用shared_ptr是不行的，会形成循环引用，直接造成资源泄露。

**空悬指针**

两个指针p1和p2，指向堆上的同一个对象Object，p1和p2处于不同的线程中，如图，如果线程A通过p1把对象销毁了，那p2就成了空悬指针。这是一种典型的C/C++内存错误。

![image-20220822003123778](C:\Users\chengzi\AppData\Roaming\Typora\typora-user-images\image-20220822003123778.png)

想要安全的销毁对象，最好在别的线程看不到的情况下做（这是垃圾回收的原理，所有人都用不到的东西一定是垃圾）

**一个"解决办法"**

引入一层间接性可以解决，让p1和p2所指的对象永久有效，比如图中proxy对象，这个对象有一个指向Object的指针，p1和p2是二级指针。

![image-20220822004047803](C:\Users\chengzi\AppData\Roaming\Typora\typora-user-images\image-20220822004047803.png)

当销毁Object之后，proxy对象继续存在，其值变为0，p2也没有变为空悬指针，可以通过proxy内容查看Object是否活着。这里依然存在竞态条件，p2看到proxy不是0，正准备调用Object的成员函数，期间对象已经被p1销毁了。

所以关键在于何时释放proxy指针。

**一个更好的解决办法**

为了安全释放proxy指针，可以引用计数（reference counting），再把p1和p2都从指针变成对象sp1和sp2，proxy现在又两个成员，指针和计算器

1. 一开始，有两个引用，计数值为2

   ![image-20220822004426665](C:\Users\chengzi\AppData\Roaming\Typora\typora-user-images\image-20220822004426665.png)

2. sp1析构了，引用计数的值减1

   ![image-20220822004537978](C:\Users\chengzi\AppData\Roaming\Typora\typora-user-images\image-20220822004537978.png)

3. sp2也析构了，引用计数将为0，可以安全的销毁proxy和Object了

   ![image-20220822004549297](C:\Users\chengzi\AppData\Roaming\Typora\typora-user-images\image-20220822004549297.png)

​	这就是引用计数型智能指针

**一个万能的解决方案**

引用另外一层间接性，（another layer of indirection）,用对象来管理共享资源，当然编写线程安全且高效的计数handle又难度，C++的TR1标准库提供了。

[explicit关键字](https://blog.csdn.net/guoyunfei123/article/details/89003369)

### 1.6 神器shared_ptr / weak_ptr

share_ptr是引用计数型智能指针，在Boost和std::tr1里均提供，也纳入c++11标准库，主流的c++编译器都支持。shared_ptr<T>是一个类模板（class template）, 它只有一个类型参数，使用方便。引用计数是自动化资源管理常用手法，当引用计数降为0时，对象自动销毁，weak_prt也是一个引用计数型智能指针，他不增加对象引用次数，即弱 (weak) 引用。

**key point**

+  shared_ptr 控制对象的生命期，share_ptr是强引用，只要有一个指向x对象的shared_ptr 存在，x对象就不会析构，当指向对象x的最后一个 shared_ptr 析构或reset()的时候，x保证会被销毁。
+ weak_ptr 不控制对象的生命周期，但是它知道对象是否还活着，如果对象还活着，它可以提升为有效的shared_ptr; 如果对象死了，提升会失败，返回一个空的shared_ptr.
+ shared_ptr / weak_ptr的 ”计数“在主流平台上是原子操作，没有用锁
+ shared_ptr / weak_ptr的线程安全级别与std::string和STL容器一样。
+ 当出现循环引用时，会出现析构异常，可以使用弱指针

[shared_ptr / weak_ptr](https://blog.csdn.net/qingdujun/article/details/74858071)

### 1.7 插曲：系统地避免各种指针错误

c++可能出现的内存问题大致分为以下几个方面

1. 缓冲区溢出
2. 空悬指针/野指针
3. 重复释放
4. 内存泄露
5. 不配对的new[] / delete
6. 内存碎片
