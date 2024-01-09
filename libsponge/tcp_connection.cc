#include "tcp_connection.hh"

#include "tcp_config.hh"
#include "tcp_segment.hh"

#include <cstddef>
#include <cstdint>
#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _last_segment_time_elapsed; }

void TCPConnection::segment_received(const TCPSegment &seg) {

    //1.判断是否是closed状态
    if (!active()) return;

    _last_segment_time_elapsed = 0; //刷新上次收到的时间

    // 2. 如果rst标志为true 将入站和出战字节流设置为error
    //? 怎么将流设置成error 将两个流的状态变量设置为true
    if (seg.header().rst) {
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        _active=false;
        return;
    }
    
    //3. 判断是否是listen状态下的报文
        // 在连接的监听状态下，只接受带syn的报文，其他一律忽略(直接返回)
    if (!_receiver.ackno().has_value()&&!_sender.next_seqno_absolute()) {  // 监听状态就是send为close，receiver为listen
        if (seg.header().syn) {
            _sender.fill_window();
        } else
            return;
    }

    //4 .其他情况都要接收
     _receiver.segment_received(seg);

    //5. 检测segment的ack，为true，代表要处理ackno和window_size
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }
    
    // 读段被通知关闭，而写段未关闭，代表被动关闭，要设置_linger_after_streams_finish为false
    if (_receiver.stream_out().input_ended()&&!_sender.stream_in().eof()) { 
        _linger_after_streams_finish=false;
    }


    // 只要报文有长度并且没有回应的段时我们都要回应
    if (_receiver.ackno().has_value()&& (seg.length_in_sequence_space()==0)&&seg.header().seqno==_receiver.ackno().value()-1) {
        _sender.send_empty_segment();
    }
    else if ((seg.length_in_sequence_space()!=0)&&_sender.segments_out().empty()) {
        _sender.send_empty_segment();
    }

    if (!_linger_after_streams_finish&&Prereq()) { // 先决条件1，2，3满足，并且_linger_after_streams_finish为false那么就算连接结束了
        _active=false;
    }

    // if (!_sender.stream_in().eof() && bytes_in_flight() == 0 && !_linger_after_streams_finish) {
    //     _active = false;
    // }

    push_out();
}

bool TCPConnection::active() const {
    return _active;
}

size_t TCPConnection::write(const string &data) {
    auto len = _sender.stream_in().write(data);
    _sender.fill_window();
    push_out();
    return len;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (!active())
        return;

    _last_segment_time_elapsed += ms_since_last_tick;
    // 1. 告诉tcpsender流逝的时间
    _sender.tick(ms_since_last_tick);



    // 2 如果超过重传次数上限
    if (_sender.get_timer().get_retrans_num() > TCPConfig::MAX_RETX_ATTEMPTS) {
        push_rst();
        return;
    }

    if (Prereq()) {  // todo 判断条件干净的结束
        if (time_since_last_segment_received() >= 10 * _cfg.rt_timeout) {
            _active = false;
            _linger_after_streams_finish = false;
        }
    }
    push_out();  //超时了，需要重传
}

// 单方面关闭写段的输出
void TCPConnection::end_input_stream() {
    // 发送连接释放报文
    _sender.stream_in().end_input();
    _sender.fill_window();
    push_out();
}

void TCPConnection::connect() {
    _sender.fill_window();
    push_out();
}

void TCPConnection::push_out() {
    // 讲sender里面的segments_out推到tcp_connection
    while (!_sender.segments_out().empty()) {
        auto seg = _sender.segments_out().front();
        // 在lab3我们没有设置ack，rst，win
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().win = _receiver.window_size() > UINT16_MAX ? UINT16_MAX : _receiver.window_size();
            seg.header().ackno = _receiver.ackno().value();
        }
        _segments_out.push(seg);
        _sender.segments_out().pop();
    }
}

void TCPConnection::push_rst() {
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active=false;
    _sender.send_reset_segment();
    push_out();
}

bool TCPConnection::Prereq() const{
    return _receiver.stream_out().input_ended()&&_sender.fin_acked();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            push_rst();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
