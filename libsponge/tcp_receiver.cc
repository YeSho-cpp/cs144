#include "tcp_receiver.hh"

#include "wrapping_integers.hh"

#include <cstddef>
#include <string>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

// 这个方法将TCPSegment转变成StreamReassembler，最后写入到ByteStream，读是从ByteStream读取
void TCPReceiver::segment_received(const TCPSegment &seg) {
    // 1. 追踪syn和fin
    WrappingInt32 seqno = seg.header().seqno;
    std::string data = seg.payload().copy();

    if (seg.header().syn) {
        // 如果syn标志为true,要记录这s
        _syn = true;  //代表syn来过
        _isn = seqno;
    }

    // 2. push data和fin到reassembler
    if (_syn) {
        //? 对于data为空的情况，只含有标记了，指数不能正常转换
        //? 这个checkpoint怎么确定 这里选用get_first_unassemble
        size_t index = unwrap(seqno, _isn.value(), _reassembler.get_first_unassemble());

        if (!seg.header().syn && index == 0) return;  //处理无效的字节流index

        index = (index == 0) ? index : index - 1;
        _reassembler.push_substring(data, index, seg.header().fin);
    }
}

// 这个方法是用来获取当前的ackno，如果没有ackno，就返回nullopt，这里的ackno就是first_unassembled字节的序号
optional<WrappingInt32> TCPReceiver::ackno() const {
    //? 这里的一个问题是如何判断是否有ackno？ 需要等待syn，syn没来，ackno就空
    if (!_syn) {
        return nullopt;
    } else
        return (_reassembler.stream_out().input_ended()) ? wrap(_reassembler.get_first_unassemble() + 2, _isn.value())
                                                         : wrap(_reassembler.get_first_unassemble() + 1, _isn.value());
}

// 作为接收方的滑动窗口实际就是ByteStream的remaining_capacity
size_t TCPReceiver::window_size() const { return _reassembler.stream_out().remaining_capacity(); }
