#include "options.hpp"

Options g_opts;

void Options::PrintHelp()
{
  std::string helpInfo = m_progName + " -i <file> -p <section type>\n" +
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

int Options::get_opts(int argc, char **argv)
{
  int opt;
  SetName(&argv[0]);

  while ((opt = getopt(argc, argv, "i:p:h")) != -1) {
    switch (opt) {
    case 'i':
      m_fileName = optarg;
      break;
    case 'p':
      if (atoi(optarg) > (int)VC_TYPE_MAX)
        goto err;
      m_type = (VCSectionType)atoi(optarg);
      break;
    case 'h':
      goto err;
    default:
      goto err;
    }
  }
  if (m_fileName.empty()) {
    std::cerr << "No file to open!" << std::endl;
    goto err;
  }
  return 0;
err:
  PrintHelp();
  return -1;
}
