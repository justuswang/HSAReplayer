#include "replayer.hpp"
#include "options.hpp"

int main(int argc, char **argv)
{
  int ret = 0;

  if (g_opts.get_opts(argc, argv))
    return -1;

  HSAInit hsaInit;
  Replayer replayer;

  replayer.CreateQueue(64, HSA_QUEUE_TYPE_MULTI);

  ret = replayer.LoadData(g_opts.FileName(), g_opts.KernelSymbol());
  if (ret != 0)
    return ret;

  replayer.SetDataTypes(g_opts.KernArgTypes());
  replayer.SubmitPacket(g_opts.KernObj(), &g_opts.j_kernArgs);

  for (int i = 0; i < g_opts.TypeNum(); i++) {
    replayer.PrintSection(g_opts.TypePop());
  }

  return 0;
}
