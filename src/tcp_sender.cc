#include "tcp_sender.hh"

#include "tcp_config.hh"

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const {
  return bytesInFlight_;
}

uint64_t TCPSender::consecutive_retransmissions() const {
  return consecutiveRetransmissions_;
}

void TCPSender::push(const TransmitFunction& transmit) {
  (void)transmit;
  if (!haveSyn_) {
    haveSyn_ = true;
    TCPSenderMessage msg;
    msg.SYN = true;
    msg.RST = input_.has_error();
    msg.seqno = Wrap32::wrap(Seqno_, isn_);
    if (input_.reader().is_finished() && !haveFin_ &&
        receiverWindows_ > Seqno_ - lastAck_) {
      msg.FIN = true;
      haveFin_ = true;
    }
    Seqno_ += msg.sequence_length();
    bytesInFlight_ += msg.sequence_length();
    outstandingSegments_.push_back(msg);
    transmit(msg);
    timerWork_ = !outstandingSegments_.empty();
    return;
  }
  uint64_t nowWindows = receiverWindows_ > 0 ? receiverWindows_ : 1;
  if (input_.reader().is_finished() && !haveFin_ &&
      nowWindows > Seqno_ - lastAck_) {
    haveFin_ = true;
    TCPSenderMessage msg;
    msg.FIN = true;
    msg.RST = input_.has_error();
    msg.seqno = Wrap32::wrap(Seqno_, isn_);
    Seqno_ += msg.sequence_length();
    bytesInFlight_ += msg.sequence_length();
    outstandingSegments_.push_back(msg);
    transmit(msg);
    timerWork_ = !outstandingSegments_.empty();
    return;
  }
  while (input_.reader().bytes_buffered() != 0 &&
         lastAck_ + nowWindows > Seqno_) {
    TCPSenderMessage msg;
    uint64_t nowLen =
        min({input_.reader().bytes_buffered(), nowWindows - (Seqno_ - lastAck_),
             TCPConfig::MAX_PAYLOAD_SIZE});
    string payload{input_.reader().peek().substr(0, nowLen)};
    input_.reader().pop(nowLen);
    msg.seqno = Wrap32::wrap(Seqno_, isn_);
    msg.payload = payload;
    msg.RST = input_.reader().has_error();
    if (input_.reader().is_finished() && !haveFin_ &&
        bytesInFlight_ + msg.sequence_length() < nowWindows) {
      msg.FIN = true;
      haveFin_ = true;
      msg.RST = input_.has_error();
    }
    Seqno_ += msg.sequence_length();
    bytesInFlight_ += msg.sequence_length();
    outstandingSegments_.push_back(msg);
    transmit(msg);
  }
  timerWork_ = !outstandingSegments_.empty();
}

TCPSenderMessage TCPSender::make_empty_message() const {
  // Your code here.
  TCPSenderMessage msg;
  msg.seqno = Wrap32::wrap(Seqno_, isn_);
  msg.RST = input_.has_error();
  return msg;
}

void TCPSender::receive(const TCPReceiverMessage& msg) {
  // Your code here.
  (void)msg;
  if (msg.RST) {
    input_.writer().set_error();
  }
  if (msg.ackno.has_value() && msg.ackno.value().unwrap(isn_, 0) > Seqno_) {
    return;
  }
  if (msg.ackno.has_value()) {
    uint64_t ackNo = msg.ackno.value().unwrap(isn_, 0);
    lastAck_ = ackNo;
    receiverWindows_ = msg.window_size;
    while (!outstandingSegments_.empty() &&
           outstandingSegments_.front().seqno.unwrap(isn_, 0) +
                   outstandingSegments_.front().sequence_length() <=
               lastAck_) {
      accumulatedTime_ = 0;
      bytesInFlight_ -= outstandingSegments_.front().sequence_length();
      consecutiveRetransmissions_ = 0;
      outstandingSegments_.pop_front();
    }
  }
  receiverWindows_ = msg.window_size;
  timerWork_ = !outstandingSegments_.empty();
}

void TCPSender::tick(uint64_t ms_since_last_tick,
                     const TransmitFunction& transmit) {
  // Your code here.
  (void)ms_since_last_tick;
  (void)transmit;
  (void)initial_RTO_ms_;
  if (!timerWork_) {
    return;
  }
  accumulatedTime_ += ms_since_last_tick;
  if (accumulatedTime_ >= (initial_RTO_ms_ << consecutiveRetransmissions_)) {
    transmit(outstandingSegments_.front());
    accumulatedTime_ = 0;
    if (receiverWindows_ != 0 || outstandingSegments_.front().SYN) {
      consecutiveRetransmissions_ += 1;
    }
  }
}
