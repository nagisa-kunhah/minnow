#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ),
close_(false),
popedCnt_(0),
pushCnt_(0) {}

bool Writer::is_closed() const
{
  return this->close_;
}

void Writer::push( string data )
{
  // Your code here.
  (void)data;
  for(auto byte:data){
    if(this->data_.size()<this->capacity_){
      this->data_+=byte;
      this->pushCnt_+=1;
    }else{
      break;
    }
  }
  return;
}

void Writer::close()
{
  this->close_=true;
}

uint64_t Writer::available_capacity() const
{
  return this->capacity_-this->data_.size();
}

uint64_t Writer::bytes_pushed() const
{
  return this->pushCnt_;
}

bool Reader::is_finished() const
{
  return  this->close_&&this->data_.empty();
}

uint64_t Reader::bytes_popped() const
{
  return this->popedCnt_;
}

string_view Reader::peek() const
{
  // Your code here.
  string_view s=this->data_;
  return s;
}

void Reader::pop( uint64_t len )
{
  // Your code here.
  (void)len;
  assert(this->data_.size()>=len);
  this->data_=this->data_.substr(len);
  this->popedCnt_+=len;
}

uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return this->data_.size();
}
