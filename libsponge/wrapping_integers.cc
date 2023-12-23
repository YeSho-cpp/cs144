#include "wrapping_integers.hh"
#include <cmath>
#include <cstdint>

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {

    // n是相对于起点0的n个距离，同理返回的seqno也是相对于起点isn的n个距离
    uint32_t dis=n&UINT32_MAX;
    return WrappingInt32{dis+isn.raw_value()};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {

    uint64_t isn_max = 1ULL << 32;
    
    // 求绝对序号的就是求syn到n实际的步数，也就是 num*isn_max+cha   num>=0
    uint32_t cha= n-isn;

    // 下面要做的就是求这个num的值
    uint64_t m=checkpoint/(isn_max);
    uint64_t pVal=m*isn_max+cha; // 这里的pVal是一个可能最接近checkpoint的值

    if (pVal<checkpoint) {
        uint64_t num=((m+1)>UINT32_MAX)?m:m+1;
        uint64_t oVal=num*isn_max+cha;
        if (oVal-checkpoint<checkpoint-pVal) {
            return oVal;
        }
        else return pVal;
    }
    else{
       uint64_t num=(m==0)?m:m-1;
       uint64_t oVal=num*isn_max+cha;
        if (checkpoint-oVal<pVal-checkpoint) {
            return oVal;
        }
        else return pVal;
    }
}
