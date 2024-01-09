#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <queue>
#include <map>


// 重传计时器
//todo 可以在某个时间点启动，一旦RTO的时间流过，就会过期
//todo 时间的流逝通过tick方法获得
class Timer{

private:
  bool running; // 检测是否运行
  size_t time_elapsed; // 计时器
  unsigned int retrans_num; // 重传的次数  次数过多会触发abort方法
  unsigned int _current_rto; // 当前RTO的值
public:
  Timer(bool is_run,const uint16_t retx_timeout);
  
  bool get_running() const;
  unsigned int get_retrans_num() const;

  size_t get_time_elapsed() const;

  void begin_timer();

  void stop_timer();
  
  bool is_timeout() const;

  void slow_start();

  void plus_time_elapsed(const size_t &time_last);

  void reset_time();
  void reset(const uint16_t retx_timeout);
};


//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};

    std::map<uint64_t,TCPSegment> _cache{}; //已发送seg的副本，用于记录 “outstanding” seg

    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout; // RTO最初的值 


    uint16_t _window_size; // 接收方当前窗口大小

    uint64_t _ackno; // 接收方当前的ackno

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};

    Timer _timer;

  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
      // 1. 收到ackno ，resent RTO
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();
    // 发送重置段
    void send_reset_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}


    void send_segment(TCPSegment& seg);

    void retrans_minseqno();

    Timer get_timer()const;

    bool fin_acked() const; // 是否发送fin已经确认

    unsigned int get_timeout() const;
};


#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
