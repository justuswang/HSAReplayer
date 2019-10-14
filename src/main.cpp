#include "replayer.hpp"
#include <stdio.h>
#include <unistd.h>

class Options {
public:
  Options();
  std::string fileName;
  void SetName(char** name) { progName = *name; }
  std::string progName;
  VCSectionType type;

  void PrintHelp();

} g_opts;

Options::Options()
{
  type = VC_NULL;
}

void Options::PrintHelp()
{
  std::string helpInfo = progName + " -i <file> -p <section type>\n" +
    "  -i input file name to load\n" +
    "  -p print section info\n\
     0: AQL packet\n\
     1: Kernel Object\n\
     2: Kernel Arg Pool\n\
     3: Kernel Args\n\
     4: All sections\n"
    "  -h show this help info";

  std::cout << helpInfo << std::endl;
}

int get_opts(int argc, char **argv)
{
  int opt;
  g_opts.SetName(&argv[0]);
  while ((opt = getopt(argc, argv, "i:p:h")) != -1) {
    switch (opt) {
    case 'i':
      g_opts.fileName = optarg;
      break;
    case 'p':
      if (atoi(optarg) > (int)VC_TYPE_MAX)
        g_opts.PrintHelp();
      g_opts.type = (VCSectionType)atoi(optarg);
      break;
    case 'h':
      g_opts.PrintHelp();
      return -1;
    default:
      g_opts.PrintHelp();
      return -1;
    }
  }

  if (g_opts.fileName.empty()) {
    std::cerr << "No file to open!" << std::endl;
    g_opts.PrintHelp();
    return -1;
  }

  return 0;
}

int main(int argc, char **argv)
{
  int ret = 0;

  if (get_opts(argc, argv))
    return -1;

  HSAInit hsaInit;
  Replayer replayer;

  ret = replayer.LoadVectorFile(g_opts.fileName.c_str());
  if (ret != 0)
    return ret;

  replayer.CreateQueue(64, HSA_QUEUE_TYPE_MULTI);
  replayer.SubmitPacket();

  replayer.PrintSection(g_opts.type);
  return 0;
}