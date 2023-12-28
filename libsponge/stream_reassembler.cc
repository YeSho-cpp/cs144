#include "stream_reassembler.hh"

#include <algorithm>
#include <cstddef>
#include <tuple>
#include <unordered_map>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _unassembled_bytes(0), has_eof(false) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    auto [remain_data, first_index, last_index, is_return, is_rcut] = handledata(data, index);
    if (is_return) return;
    if (eof && !is_rcut) has_eof = true;  // 有eof且末端截断过，eof就算失效了

    if (first_index == get_first_unassemble()) {  // 当连接上了

        for (size_t i = first_index; i <= last_index; ++i) {
            if (_unassembled_map.find(i) != _unassembled_map.end()) {  // 发来的是连续，也要清除_unassembled_map有的
                --_unassembled_bytes;
                _unassembled_map.erase(i);
            }
        }
        _output.write(remain_data);
        if (eof) {
            _output.end_input();
        }

        // 判断后面又一段连续的字节流
        first_index = last_index + 1;
        last_index = get_first_unacceptable();
        // 开始遍历
        remain_data = "";
        for (size_t i = first_index; i < last_index; ++i) {
            // 需要增加_unassembled_bytes 以及更新_unassembled_map
            if (_unassembled_map.find(i) != _unassembled_map.end()) {
                --_unassembled_bytes;
                remain_data += _unassembled_map[i];
                _unassembled_map.erase(i);
            } else
                break;
        }
        if (remain_data != "") {
            _output.write(remain_data);
            if (has_eof && empty()) {  // eof出现过，并且empty()才能结果输入
                _output.end_input();
            }
        }
    } else {  // 没有连接，这里只会需要增加_unassembled_bytes 以及更新_unassembled_map,没有write操作
        for (size_t i = first_index, j = 0; i <= last_index; ++i, ++j) {
            if (_unassembled_map.find(i) == _unassembled_map.end()) {  // _unassembled_map不存在这个字节流，需要更新
                ++_unassembled_bytes;
                _unassembled_map[i] = remain_data[j];
            }
        }
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }

uint64_t StreamReassembler::get_first_unread() const { return _output.bytes_read(); }

uint64_t StreamReassembler::get_first_unassemble() const { return _output.bytes_written(); }

uint64_t StreamReassembler::get_first_unacceptable() const {
    return _output.bytes_written() + _output.remaining_capacity();
}

std::tuple<std::string, size_t, size_t, bool, bool> StreamReassembler::handledata(std::string data, size_t index) {
    auto lb = get_first_unassemble(), rb = get_first_unacceptable() - 1;  // 重组的范围，不在这个范围的都无效
    // 计算这个数据的范围
    auto orgin_l = index;
    auto orgin_r = (data.length()) ? (index + data.length() - 1) : index;

    // 处理一些特殊的情况
    if (orgin_r < lb || orgin_l > rb)
        return make_tuple("", 0, 0, true, true);
    if (data.length() == 0)
        return make_tuple("", index, index, false, false);

    // 开始裁剪
    auto l_index = max(lb, orgin_l);
    auto r_index = min(rb, orgin_r);
    auto remain_data = data.substr(l_index - index, r_index - l_index + 1);
    auto last_index = remain_data.length() ? l_index + remain_data.length() - 1 : l_index;
    return make_tuple(remain_data, l_index, last_index, false, r_index < orgin_r);
}
