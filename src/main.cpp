#include "replayer.hpp"

int main(int argc, char **argv)
{
  HSAInit hsaInit;

  Replayer replayer;
  replayer.LoadVectorFile("/home/zjunwei/tmp/clang_vectoradd_co_v10.rpl");
  replayer.CreateQueue(64, HSA_QUEUE_TYPE_SINGLE);
  replayer.SubmitPacket();
  replayer.ShowSection(VC_KERN_ARG);

  return 0;
}