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
    : _output(capacity), _capacity(capacity), _unassembled_bytes(0),_sign(),end_p(SIZE_MAX) {
        _sign.reserve(capacity);
        _cache.reserve(capacity);
    }

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.

void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    
    bool eof_flag=false;
    size_t expand_size=index+data.length();
    auto write_p=get_first_unassemble();
    auto rightB =get_first_unacceptable(); // 有边界
    if (index>=rightB) return;
    
    if (index+data.length()<=rightB)
    {
        eof_flag=true;
        expand_size=index+data.length();
    }
    else {expand_size=rightB;}
    
    if (eof&&eof_flag)
    {
        end_p=expand_size;
    }
    
    const size_t cache_raw_length = _cache.length();

        // 先扩大一次容量，用于写入多余的内容
    if (expand_size > cache_raw_length) {
        _cache.resize(expand_size);
        _sign.resize(expand_size);
    }

    int num=std::count(_sign.begin()+index,_sign.begin()+index+data.length(),'1');
    _unassembled_bytes+=(data.length()-num);
    // 将要排序的内容先写入cache当中
    _cache.replace(index, data.length(), data);
    _sign.replace(index, data.length(), data.length(), '1');

    // 缩回原来的大小，将缓冲区外多余的内容丢弃
    if (expand_size > cache_raw_length) {
        _cache.resize(expand_size);
        _sign.resize(expand_size);
    }

    if (_sign[write_p])
    {
        size_t len=0;
        while (_sign[write_p+len]&&len<_output.remaining_capacity())
        { 
            if (_sign[write_p+len])
            {
                --_unassembled_bytes;
            }
            len++;        
        }
        _output.write(_cache.substr(write_p,len));
        write_p+=len;
    }
    // 写入位和EOF位相同，代表写入结束
    if (write_p == end_p) {
        _output.end_input();
    }
    
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }

uint64_t StreamReassembler::get_first_unread() const { return _output.bytes_read(); }

uint64_t StreamReassembler::get_first_unassemble() const { return _output.bytes_written(); }

uint64_t StreamReassembler::get_first_unacceptable() const {
    return _output.bytes_written() + _output.remaining_capacity();
}

