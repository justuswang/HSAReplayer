#include "options.hpp"

Options g_opts;

Options::Options()
{
  m_defaultFileName = "./vc.rpl";
}

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

VCSectionType Options::TypePop()
{
  VCSectionType type = VC_NULL;

  if (m_type.size() == 0)
    return type;

  type = m_type[0];
  m_type.erase(m_type.begin());
  return type;
};

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
      m_type.push_back((VCSectionType)atoi(optarg));
      break;
    case 'h':
      goto err;
    default:
      goto err;
    }
  }

  // debug
  //m_fileName = "/home/zjunwei/tmp/clang_vectoradd_co_v10.rpl";

  m_type_cnt = m_type.size();
  if (m_fileName.empty()) {
    m_fileName = m_defaultFileName;
    std::cout << "Open default file: " << m_fileName << std::endl;
  }
  return 0;
err:
  PrintHelp();
  return -1;
}
