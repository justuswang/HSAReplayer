#include "options.hpp"
#include <typeinfo>

Options g_opts;

Options::Options()
{
  m_defaultFileName = "./vc.rpl";
  m_defaultJsonFile = "./config.json";
  m_debug = false;
}

void Options::PrintHelp()
{
  std::string helpInfo = m_progName + " -i <file> -p <section type>\n" +
    "  -i input file name to load (default ./vc.rpl)\n"
    "  -j input json file to parse kernel args data type (default ./config.json)\n"
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

void Options::ParseJson()
{
  std::ifstream f(m_jsonFile);
  if (!f.is_open()){
    std::cout << "Failed to open " << m_jsonFile << std::endl;
    return;
  }

  json j = json::parse(f);
  if (g_opts.Debug()) {
    std::cout << "version: " << j["version"] << std::endl;
    std::cout << "kernel args: " << j["vc_kernel_args"].size() << std::endl;
  }
  for (int i = 0; i < (int)j["vc_kernel_args"].size(); i++) {
    std::string str = j["vc_kernel_args"][i][1].dump();
    kernArgs.push_back(DataType((const char*)str.substr(1, str.size() - 2).c_str()));
  }

  // hsaco
  std::cout << "hsaco kernel object: " << j[HSACO_KERN_OBJ].size() << std::endl;

  std::string str = j[HSACO_KERN_OBJ]["symbol"].dump();
  m_symbol = str.substr(1, str.size() - 2);
  std::cout << "hsaco kernel symbol: " << m_symbol << std::endl;

  if (j[HSACO_KERN_OBJ].size() > 0) {
    j_kernObj.SetAll(j[HSACO_KERN_OBJ]["dim"],
                    j[HSACO_KERN_OBJ]["workgroup_size_x"],
                    j[HSACO_KERN_OBJ]["workgroup_size_y"],
                    j[HSACO_KERN_OBJ]["workgroup_size_z"],
                    j[HSACO_KERN_OBJ]["grid_size_x"],
                    j[HSACO_KERN_OBJ]["grid_size_y"],
                    j[HSACO_KERN_OBJ]["grid_size_z"]);
  }

  std::cout << "hsaco KernArgs: " << j[HSACO_KERN_ARGS].size() << std::endl;
  if (j[HSACO_KERN_ARGS].size() > 0) {
    for (size_t i = 0; i < j[HSACO_KERN_ARGS].size(); ++i) {
      std::string str = j[HSACO_KERN_ARGS][i]["data_type"].dump();
      std::cout << str.substr(1, str.size() - 2) << std::endl;
      j_kernArgs.push_back(std::unique_ptr<JsonKernArg>(new JsonKernArg));
      j_kernArgs[i].get()->SetAll(j[HSACO_KERN_ARGS][i]["index"],
                                  j[HSACO_KERN_ARGS][i]["store_type"],
                                  (const char*)str.substr(1, str.size() - 2).c_str(),
                                  j[HSACO_KERN_ARGS][i]["size"]);
      if (j_kernArgs[i].get()->dType == VC_FLOAT) {
        std::cout << "value: " << j[HSACO_KERN_ARGS][i]["value"] << std::endl;
        j_kernArgs[i].get()->value.f = j[HSACO_KERN_ARGS][i]["value"];
        std::cout << "val: " << j_kernArgs[i].get()->value.f << std::endl;
      } else if (j_kernArgs[i].get()->dType == VC_DOUBLE) {
        j_kernArgs[i].get()->value.d = j[HSACO_KERN_ARGS][i]["value"];
      } else if (j_kernArgs[i].get()->dType == VC_INT) {
        j_kernArgs[i].get()->value.i = j[HSACO_KERN_ARGS][i]["value"];
      } else {
        std::cerr << "json: Unknown data type " << i <<": " <<  j[HSACO_KERN_ARGS][i]["value"] << std::endl;
      }
    }
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

  if (m_fileName.empty()) {
    m_fileName = m_defaultFileName;
    std::cout << "Try to open default file: " << m_fileName << std::endl;
  }
  if (m_jsonFile.empty()) {
    m_jsonFile = m_defaultJsonFile;
    std::cout << "Try to open default json file: " << m_jsonFile << std::endl;
  }
  if (m_type.empty())
    m_type.push_back(VC_KERN_ARG);
  m_type_cnt = m_type.size();

  ParseJson();
  return 0;
err:
  PrintHelp();
  return -1;
}
