#include "tcp_sender.hh"
#include "tcp_config.hh"

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  return lastSeq_-lastACK_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return consecutiveRetryNum_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  // Your code here.
  (void)transmit;
  uint64_t nowWindows=windowsSizeHaveSet?windowsSize_:1;
  while(true){
    auto msg=TCPSenderMessage();
    if(!hasSyn_){
      msg.seqno=Wrap32::wrap(0,isn_);
      msg.SYN=true;
      msg.RST=input_.reader().has_error();
      nowWindows-=1;
    }else{
      uint64_t nowLen=min({TCPConfig::MAX_PAYLOAD_SIZE,nowWindows,input_.reader().bytes_buffered()});
      string payload{input_.reader().peek().substr(0,nowLen)};
      input_.reader().pop(nowLen);
      msg.seqno=Wrap32::wrap(lastSeq_,isn_);
      msg.payload=payload;
      msg.RST=input_.reader().has_error();
      nowWindows-=msg.sequence_length();
    }
    if(input_.writer().is_closed()&&!hasFin_&&msg.sequence_length()+1<=windowsSize_){
      msg.FIN=true;
      hasFin_=true;
    }
    lastSeq_+=msg.sequence_length();
    if(msg.sequence_length()!=0){
      transmit(msg);
      data_.push_back(msg);
    }
    if(nowWindows==0||input_.reader().is_finished()||input_.reader().bytes_buffered()==0){
      break;
    }
  }
  windowsSize_=nowWindows;
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  // Your code here.
  TCPSenderMessage msg;
  msg.seqno=Wrap32::wrap(lastSeq_,isn_);
  msg.RST=input_.reader().has_error();
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // Your code here.
  (void)msg;
  if(msg.ackno.has_value()){
    uint64_t ackNo=msg.ackno.value().unwrap(isn_,0);
    if(ackNo>lastSeq_){
      return ;
    }
    lastACK_=max(lastACK_,msg.ackno.value().unwrap(isn_,0));
    while(!data_.empty()&&data_.front().seqno.unwrap(isn_,0)+data_.front().sequence_length()-1<=ackNo){
      if(data_.front().SYN){
        hasSyn_=true;
      }
      data_.pop_front();
      retransmissionTimer_=0;
    }
  }
  consecutiveRetryNum_=0;
  windowsSize_=msg.window_size;
  windowsSizeHaveSet=true;
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // Your code here.
  (void)ms_since_last_tick;
  (void)transmit;
  (void)initial_RTO_ms_;
  retransmissionTimer_+=ms_since_last_tick;
  if(retransmissionTimer_>=(initial_RTO_ms_<<consecutiveRetryNum_)){
    if(!data_.empty()){
      if(!windowsSizeHaveSet||windowsSize_!=0){
        consecutiveRetryNum_+=1;
      }
      transmit(data_.front());
    }
    retransmissionTimer_=0;
  }
}
