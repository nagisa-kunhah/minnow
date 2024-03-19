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
  cerr << "to push..." << endl;
  cerr << "now:" << input_.reader().peek() << endl;
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
    cerr << "after push 1" << endl;
    for (auto x : outstandingSegments_) {
      cerr << x.seqno.raw_value_ << " " << x.SYN << " " << x.FIN << " "
           << x.payload << endl;
    }
    cerr << endl;
    return;
  }
  uint64_t nowWindows = receiverWindows_ > 0 ? receiverWindows_ : 1;
  if (input_.reader().is_finished() && !haveFin_ &&
      nowWindows > Seqno_ - lastAck_) {
    cerr << "push 2 con:" << endl;
    cerr << input_.reader().is_finished() << " " << (!haveFin_) << " "
         << receiverWindows_ << " " << Seqno_ - lastAck_ << endl;
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
    cerr << "after push 2" << endl;
    for (auto x : outstandingSegments_) {
      cerr << x.seqno.raw_value_ << " " << x.SYN << " " << x.FIN << " "
           << x.payload << endl;
    }
    cerr << endl;
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
  cerr << "after push 3" << endl;
  for (auto x : outstandingSegments_) {
    cerr << x.seqno.raw_value_ << " " << x.SYN << " " << x.FIN << " "
         << x.payload << endl;
  }
  cerr << endl;
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
  cerr << "receive:"
       << (msg.ackno.has_value() ? msg.ackno.value().raw_value_ : -1) << " "
       << msg.window_size << endl;
  if (msg.ackno.has_value() && msg.ackno.value().unwrap(isn_, 0) > Seqno_) {
    cerr << "receive: ignore" << endl;
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
    cerr << "after receive:" << ackNo << ", data:" << endl;
    for (auto x : outstandingSegments_) {
      cerr << x.seqno.unwrap(isn_, 0) << " " << x.payload << " " << x.SYN << " "
           << x.FIN << endl;
    }
    cerr << "bytes in flight:" << bytesInFlight_ << endl;
  }
  receiverWindows_ = msg.window_size;
  timerWork_ = !outstandingSegments_.empty();
  cerr << "receive exit" << endl;
}

void TCPSender::tick(uint64_t ms_since_last_tick,
                     const TransmitFunction& transmit) {
  // Your code here.
  (void)ms_since_last_tick;
  (void)transmit;
  (void)initial_RTO_ms_;
  cerr << "tick:" << ms_since_last_tick << endl;
  if (!timerWork_) {
    cerr << "tick:"
         << "not work,exit" << endl;
    return;
  }
  accumulatedTime_ += ms_since_last_tick;
  cerr << "now accumulated:" << accumulatedTime_ << endl;
  cerr << "timeout need:" << (initial_RTO_ms_ << consecutiveRetransmissions_)
       << endl;
  if (accumulatedTime_ >= (initial_RTO_ms_ << consecutiveRetransmissions_)) {
    cerr << "timeout! Retransmit" << endl;
    transmit(outstandingSegments_.front());
    accumulatedTime_ = 0;
    cerr << "condition:" << receiverWindows_ << " " << haveSyn_ << endl;
    if (receiverWindows_ != 0 || outstandingSegments_.front().SYN) {
      cerr << "consecutiveRetransmissions_ up" << endl;
      consecutiveRetransmissions_ += 1;
    }
  }
}
