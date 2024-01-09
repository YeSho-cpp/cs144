#include "tcp_sender.hh"

#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <random>
#include <utility>

Timer::Timer(bool is_run, const uint16_t retx_timeout)
    : running(is_run), time_elapsed(0), retrans_num(0), _current_rto(retx_timeout){};

bool Timer::get_running() const { return running; }

size_t Timer::get_time_elapsed() const { return time_elapsed; }

bool Timer::is_timeout() const { return time_elapsed >= _current_rto; }

void Timer::reset_time() { time_elapsed = 0; }

unsigned int Timer::get_retrans_num() const { return retrans_num; }

void Timer::begin_timer() { running = true; }

void Timer::slow_start() {
    ++retrans_num;
    _current_rto *= 2;
}

void Timer::stop_timer() { running = false; }

void Timer::plus_time_elapsed(const size_t &time_last) { time_elapsed += time_last; }

void Timer::reset(const uint16_t retx_timeout) {
    time_elapsed = 0;
    retrans_num = 0;
    _current_rto = retx_timeout;
}
// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _window_size(1)
    , _ackno(0)
    , _stream(capacity)
    , _timer(false, retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - _ackno; }

// 从输入ByteStream中读取，以Tcpsegment格式发送，确保Tcpsegment足够大 ,不超过MAX_PAYLOAD_SIZE
void TCPSender::fill_window() {
    int fill_window_size = _window_size ? _window_size : 1;  // 能填充窗口的大小
    fill_window_size -= bytes_in_flight();
    while (fill_window_size > 0) {  // 循环是因为一次发送的长度可能可以分几次了发送
        TCPSegment segment;
        segment.header().syn = (_next_seqno == 0);
        size_t payload_size =
            min(static_cast<size_t>(TCPConfig::MAX_PAYLOAD_SIZE), static_cast<size_t>(fill_window_size));
        segment.payload() = _stream.read(payload_size);
        segment.header().fin =
            (_stream.eof() && static_cast<size_t>(fill_window_size) >
                                  segment.length_in_sequence_space());  // 能发fin的条件是流中止了，并且有位置给fin放
        segment.header().seqno = next_seqno();
        if ((segment.length_in_sequence_space() == 0) ||
            (next_seqno_absolute() == _stream.bytes_written() + 2))  // 无效的字节流就取消
            return;
        _next_seqno += segment.length_in_sequence_space();
        fill_window_size -= segment.length_in_sequence_space();

        // 开始发送
        send_segment(segment);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    //! 要忽略哪些无效的ackno
    uint64_t abs_ackno=unwrap(ackno, _isn, _next_seqno);
    if (abs_ackno < _ackno || abs_ackno > _next_seqno) {
        return;
    }
    _ackno = abs_ackno, _window_size = window_size;  // 追踪最新的ackno和window_size

    // 查看outstanding segments的数据结构，移除已经被确认的，其实就是ackno一下数字的
    auto orgin_size = _cache.size();
    auto it = _cache.lower_bound(_ackno);
    _cache.erase(_cache.begin(), it);

    if (_cache.empty()) {     // 确认完成所有的未完成的数据
        _timer.stop_timer();  // 停止重传计时器
    }

    auto cur_size = _cache.size();
    if (orgin_size != cur_size) {  // 删除了，就要重置RTO和超时次数
        _timer.reset(_initial_retransmission_timeout);
        if (!_cache.empty())
            _timer.begin_timer();  // 有任何未完成的数据，重新启动重传计时器
    }
   
    fill_window();

}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    // ms_since_last_tick 超过了RTO ，就会触发过期

    if (_timer.get_running()) {                        // 计时器运行了才能进行下面的操作
        _timer.plus_time_elapsed(ms_since_last_tick);  // 更新内部计时器

        if (not _timer.is_timeout())
            return;  // 检测是否超时

        if (_window_size) {
            _timer.slow_start();  // 执行满开始
        }

        if (_timer.get_retrans_num() <=TCPConfig::MAX_RETX_ATTEMPTS) {
            retrans_minseqno();  // 重传最小的序列号
        } 
        _timer.reset_time();  //重传了一定要重置计时器时间的
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _timer.get_retrans_num(); }

void TCPSender::send_empty_segment() {
    // 生成并发送一个序列空间中长度为零的TCPSegment
    // 但是序号设置正确
    TCPSegment segment;
    segment.header().seqno = next_seqno();
    _segments_out.push(segment);
}
void TCPSender::send_reset_segment() {
    // 生成并发送一个序列空间中长度为零的TCPSegment
    // 但是序号设置正确
    TCPSegment segment;
    segment.header().rst = true;
    segment.header().seqno = next_seqno();
    _segments_out.push(segment);
}


void TCPSender::send_segment(TCPSegment &seg) {
    // 1. 第一步就是推入_segments_out;

    _segments_out.push(seg);
    _cache.insert(
        make_pair(unwrap(seg.header().seqno, _isn, _stream.bytes_written()) + seg.length_in_sequence_space() - 1,
                  seg));  //备份seg

    // 一旦发送或者重传非0的segment发现没running就会激活
    if (!_timer.get_running()) {
        _timer.begin_timer();
        _timer.reset(_initial_retransmission_timeout);
    }
}


void TCPSender::retrans_minseqno() {
    // TCPSender的重传(重传的是最小的序列号)
    // 遍历整个_cache找到最小的序列号的segment重传 所谓的重传就是再push进去
    _segments_out.push(_cache.begin()->second);
}

bool TCPSender::fin_acked() const{
    return _stream.eof()&&(next_seqno_absolute()==_stream.bytes_written()+2)&&bytes_in_flight()==0;
}

Timer TCPSender::get_timer()const {return _timer;}

unsigned int TCPSender::get_timeout() const {return _initial_retransmission_timeout;}