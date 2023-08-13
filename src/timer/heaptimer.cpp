/*
定时器的具体实现，针对小根堆的调整，制定了两种形式：
- 一是上浮操作，如果上浮操作没进行则进行下沉操作；
- 二是下沉操作，同上；
 */ 
#include "heaptimer.h"

/**
 * @brief 上浮调整指定索引的结点的位置；
 * @param i 要进行上浮操作的结点索引号；
 */
void HeapTimer::siftup_(size_t i) {
    assert(i >= 0 && i < heap_.size());
    size_t j = (i - 1) / 2; // 找到父节点，不断上浮，直到根节点；
    while(j >= 0) {
        if(heap_[j] < heap_[i]) { break; }
        SwapNode_(i, j);
        i = j;
        j = (i - 1) / 2;
    }
}

void HeapTimer::SwapNode_(size_t i, size_t j) {
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());
    std::swap(heap_[i], heap_[j]);
    ref_[heap_[i].id] = i;  // 更新id->index的映射
    ref_[heap_[j].id] = j;
} 

/**
 * @brief 下沉调整指定索引的结点的位置；
 * @param index 要进行下沉操作处理的结点索引号；
 * @param n 初步判断是要进行下沉处理的堆节点范围，一般是堆大小；
 * @return 是否进行过下沉操作，true or false；
 */
bool HeapTimer::siftdown_(size_t index, size_t n) {
    assert(index >= 0 && index < heap_.size());
    assert(n >= 0 && n <= heap_.size());
    size_t i = index;       // 要处理的索引节点；
    size_t j = i * 2 + 1;   // 要处理的结点的左节点；(堆是一颗完全二叉树)
    while(j < n) {          // while循环不断做调整  
        if(j + 1 < n && heap_[j + 1] < heap_[j]) j++;   // 如果右节点更小，判断右节点
        if(heap_[i] < heap_[j]) break;  // 如果已经是一个小根堆的形式，跳出循环
        SwapNode_(i, j);
        i = j;  // 左孩子或者右孩子的下一个结点
        j = i * 2 + 1;
    }
    return i > index;
}

/**
 * @brief 添加结点的操作，并且附带了相应的回调函数；
 * @param id 结点ID；
 * @param timeout 要添加的结点的超时时间；
 * @param cb 回调函数；
 */
void HeapTimer::add(int id, int timeout, const TimeoutCallBack& cb) {   // 给定了超时信息；
    assert(id >= 0);
    size_t i;
    if(ref_.count(id) == 0) {   // 如果哈希中没有找到该ID，那么新建；
        // 新节点：堆尾插入，调整堆；
        i = heap_.size();
        ref_[id] = i;   // 更新新ID的索引；
        heap_.push_back({id, Clock::now() + MS(timeout), cb});  // 直接构造定时器节点(不用管函数，函数不属于类成员)
        siftup_(i); // 最后一行添加的当然只能进行上浮操作；
    } 
    else {
        // 已有结点：调整堆；
        i = ref_[id];   // 找到索引；
        heap_[i].expires = Clock::now() + MS(timeout);  // 赋新的时间点；
        heap_[i].cb = cb;   // 更新回调函数；
        if(!siftdown_(i, heap_.size())) {   // 如果没有进行下沉处理；
            siftup_(i); // 则进行上浮操作，反正得进行上浮操作；
        }
    }
}

void HeapTimer::doWork(int id) {
    /* 删除指定id结点，并触发回调函数 */
    if(heap_.empty() || ref_.count(id) == 0) {
        return;
    }
    size_t i = ref_[id];
    TimerNode node = heap_[i];
    node.cb();
    del_(i);
}

void HeapTimer::del_(size_t index) {
    /* 删除指定位置的结点 */
    assert(!heap_.empty() && index >= 0 && index < heap_.size());
    /* 将要删除的结点换到队尾，然后调整堆 */
    size_t i = index;
    size_t n = heap_.size() - 1;
    assert(i <= n);
    if(i < n) {
        SwapNode_(i, n);
        if(!siftdown_(i, n)) {
            siftup_(i);
        }
    }
    /* 堆尾元素删除 */
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}

/**
 * @brief 调整小根堆，指定新的超时时间；
 * @param id 要调整的节点序列号；
 */
void HeapTimer::adjust(int id, int timeout) {
    assert(!heap_.empty() && ref_.count(id) > 0);   // 前提条件
    heap_[ref_[id]].expires = Clock::now() + MS(timeout);  // 新的时间点
    if(!siftdown_(ref_[id], heap_.size())) {   // 如果没有进行下沉处理；
        siftup_(ref_[id]); // 则进行上浮操作，反正得进行上浮操作；
    }
    // siftdown_(ref_[id], heap_.size());  // 调整结点位置(一定是进行下沉操作？)
}

void HeapTimer::tick() {
    /* 清除超时结点 */
    if(heap_.empty()) {
        return;
    }
    while(!heap_.empty()) {
        TimerNode node = heap_.front(); // 最前面的元素
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) { 
            break;  // 没超时则中断
        }
        node.cb();
        pop();
    }
}

void HeapTimer::pop() { 
    assert(!heap_.empty());
    del_(0);    // 删除第一个
}

void HeapTimer::clear() {
    ref_.clear();   // 清空哈希表
    heap_.clear();  // 清空堆
}

int HeapTimer::GetNextTick() {
    tick(); // 清空超时节点
    size_t res = -1;
    if(!heap_.empty()) {
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        if(res < 0) { res = 0; }
    }
    return res; // 返回值为0则表明当前存在的一个定时器已经到期
}