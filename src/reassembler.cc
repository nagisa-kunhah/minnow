#include "reassembler.hh"

using namespace std;

void Reassembler::insert(uint64_t first_index, std::string data, bool is_last_substring )
{
  if(is_last_substring){
    finishIdx=first_index+data.size();
  }
  for(size_t i=0;i<data.size();i++){
    if(i+first_index>=lastIdx_&&i+first_index<lastIdx_+output_.writer().available_capacity()){
      int gap=i+first_index-lastIdx_;
      unReassemble_[(front_+gap)%cap_]=make_tuple(true,first_index+i,data[i]);
    }
  }
  string toPush;
  while(get<0>(unReassemble_[front_])){
    auto [_,idx,ch]=unReassemble_[front_];
    get<0>(unReassemble_[front_])=false;
    front_+=1;
    front_%=cap_;
    toPush.push_back(ch);
    lastIdx_=idx+1;
  }
  if(!toPush.empty()){
    output_.writer().push(toPush);
  }
  if(lastIdx_==finishIdx){
    output_.writer().close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  return count_if(unReassemble_.begin(),unReassemble_.end(),[](const std::tuple<bool,uint64_t,char>&da){
    return get<0>(da);
  });
}
