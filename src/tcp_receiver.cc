#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // Your code here.
  (void)message;
  if(message.RST){
    reassembler_.reader().set_error();
  }
  if(message.SYN){
    setISN_=true;
    ISN_=message.seqno;
  }
  if(!setISN_){
    return ;
  }
  uint64_t checkpoint=reassembler_.writer().bytes_pushed();
  auto unWrapped=message.seqno.unwrap(ISN_,checkpoint);
  reassembler_.insert(message.SYN?0:unWrapped-1,message.payload,message.FIN);
}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage msg{};
  msg.window_size=min(reassembler_.writer().available_capacity(),static_cast<uint64_t>(UINT16_MAX));
  if(setISN_){
    uint32_t nxtSeq=reassembler_.writer().bytes_pushed()+1+reassembler_.writer().is_closed();
    msg.ackno=Wrap32::wrap(nxtSeq,ISN_);
  }
  msg.RST=(reassembler_.writer().has_error()||RST_);
  return msg;
}
