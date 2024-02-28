#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  (void)n;
  (void)zero_point;
  return Wrap32 { zero_point+n };
}
uint64_t calDis(uint64_t a,uint64_t b){
  if(a>b)return a-b;
  return b-a;
}
uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Your code here.
  (void)zero_point;
  (void)checkpoint;
  uint64_t Mod=1UL<<32;
  uint64_t checkpoint32=wrap(checkpoint,zero_point).raw_value_;
  uint64_t moveLeft,moveRight;
  if(checkpoint32>=raw_value_){
    moveLeft=checkpoint32-raw_value_;
    moveRight=Mod-checkpoint32+raw_value_;
  }else{
    moveLeft=checkpoint32+Mod-raw_value_;
    moveRight=raw_value_-checkpoint32;
  }
  // cerr<<moveLeft<<" "<<moveRight<<" "<<checkpoint<<endl;
  if(moveLeft<=moveRight&&checkpoint>=moveLeft){
    return checkpoint-moveLeft;
  }else{
    return checkpoint+moveRight;
  }
}
