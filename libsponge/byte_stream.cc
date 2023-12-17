#include "byte_stream.hh"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : _capacity(capacity), _write_cur_num(0), _bytes_written(0), _bytes_read(0), _write_end(false), _read_end(false) {}

size_t ByteStream::write(const string &data) {
    if (_write_end) {
        return 0;
    }
    size_t len = data.length();
    len = min(remaining_capacity(), len);
    _buffer.resize(_write_cur_num + len);
    for (size_t i = 0; i < len; i++) {
        _buffer[_write_cur_num + i] = data[i];
    }
    // 这里要加_write_cur_num
    _write_cur_num += len;
    // 加_bytes_written
    _bytes_written += len;
    return len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t _len = min(_write_cur_num, len);
    // 这里只是查看，不读取
    return _buffer.substr(0, _len);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t _len = min(_write_cur_num, len);
    _buffer.erase(0, _len);
    // 这里要减_write_cur_num
    _write_cur_num -= _len;
    _buffer.resize(_write_cur_num);
    // 加_bytes_read
    _bytes_read += _len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    // 先复制
    string read_data = peek_output(len);
    // 后删除
    pop_output(len);
    return read_data;
}

void ByteStream::end_input() {
    // 设置写端的结束标志
    _write_end = true;
}

bool ByteStream::input_ended() const { return _write_end; }

size_t ByteStream::buffer_size() const { return _write_cur_num; }

bool ByteStream::buffer_empty() const { return _write_cur_num == 0; }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

size_t ByteStream::remaining_capacity() const { return _capacity - _write_cur_num; }
