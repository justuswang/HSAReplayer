#include "options.hpp"

Options g_opts;

Options::Options()
{
  m_defaultFileName = "./vc.rpl";
  m_defaultJsonFile = "./ka.json";
  m_debug = false;
}

void Options::PrintHelp()
{
  std::string helpInfo = m_progName + " -i <file> -p <section type>\n" +
    "  -i input file name to load (default ./vc.rpl)\n"
    "  -j input json file to parse kernel args data type (default ./ka.json)\n"
    "  -p print section info\n\
     0: AQL packet\n\
     1: Kernel Object\n\
     2: Kernel Arg Pool\n\
     3: Kernel Args\n\
     4: All sections\n"
    "  -d show debug info\n"
    "  -h show this help info";

  std::cout << helpInfo << std::endl;
}

using json = nlohmann::json;

void Options::ParseKernArg(const char *type)
{
  if (strncmp(type, "float", sizeof("float")-1) == 0)
    kernArgs.push_back(KA_FLOAT);
  else if (strncmp(type, "int", sizeof("int")-1) == 0)
    kernArgs.push_back(KA_INT);
  else if (strncmp(type, "double", sizeof("double")-1) == 0)
    kernArgs.push_back(KA_DOUBLE);
  else if (strncmp(type, "uint32", sizeof("uint32")-1) == 0)
    kernArgs.push_back(KA_UINT32);
}

void Options::ParseJson()
{
  std::ifstream f(m_jsonFile);
  if (!f.is_open())
    return;

  json j = json::parse(f);
  if (g_opts.Debug()) {
    std::cout << "version: " << j["version"] << std::endl;
    std::cout << "kernel args: " << j["kernel_args"].size() << std::endl;
  }
  for (int i = 0; i < (int)j["kernel_args"].size(); i++) {
    std::string str = j["kernel_args"][i][1].dump();
    ParseKernArg((const char*)str.substr(1, str.size() - 2).c_str());
  }
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

  while ((opt = getopt(argc, argv, "i:j:p:h")) != -1) {
    switch (opt) {
    case 'i':
      m_fileName = optarg;
      break;
    case 'j':
      m_jsonFile = optarg;
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

  m_type_cnt = m_type.size();
  if (m_fileName.empty()) {
    m_fileName = m_defaultFileName;
    std::cout << "Try to open default file: " << m_fileName << std::endl;
  }
  if (m_jsonFile.empty()) {
    m_jsonFile = m_defaultJsonFile;
    std::cout << "Try to open default json file: " << m_jsonFile << std::endl;
  }
  ParseJson();

  return 0;
err:
  PrintHelp();
  return -1;
}
