#include "replayer.hpp"
#include "options.hpp"

int main(int argc, char **argv)
{
  int ret = 0;

  if (g_opts.get_opts(argc, argv))
    return -1;

  HSAInit hsaInit;
  Replayer replayer;

  ret = replayer.LoadVectorFile(g_opts.FileName());
  if (ret != 0)
    return ret;

  replayer.CreateQueue(64, HSA_QUEUE_TYPE_MULTI);
  replayer.SubmitPacket();

  for (int i = 0; i < g_opts.TypeNum(); i++) {
    replayer.PrintSection(g_opts.TypePop(), g_opts.KernArgTypes());
  }

  return 0;
}
