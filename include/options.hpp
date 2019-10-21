#ifndef __OPTIOINS_H__
#define __OPTIOINS_H__

#include <string>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include "section.hpp"
#include "json.hpp"

#define HSACO_KERN_OBJ    "hsaco_kernel_obj"
#define HSACO_KERN_ARGS    "hsaco_kernel_args"

class Options {
public:
  Options();
  int get_opts(int argc, char **argv);
  VCSectionType TypePop();
  int TypeNum() const { return m_type_cnt; }
  const char * FileName() const { return m_fileName.c_str(); }
  const char * KernelSymbol() const { return m_symbol.c_str(); }
  void PrintSection(void (*print)(VCSectionType type));
  std::vector<VCDataType>* KernArgTypes()
  {
    if (kernArgs.size() == 0)
      return NULL;
    else
      return &kernArgs;
  }
  bool Debug() const { return m_debug; }
  JsonKernObj* KernObj() { return &j_kernObj; }
  std::vector<std::unique_ptr<JsonKernArg>> j_kernArgs;

private:
  void SetName(char** name) { m_progName = *name; }
  void PrintHelp();
  void ParseJson();

  std::string m_progName;
  std::string m_fileName;
  std::string m_symbol; // kernel symbol for hsaco only
  std::string m_defaultFileName;
  std::string m_jsonFile;
  std::string m_defaultJsonFile;
  std::vector<VCDataType> kernArgs;
  std::vector<VCSectionType> m_type; // include all
  int m_type_cnt;
  bool m_debug;

  JsonKernObj j_kernObj;
};

extern Options g_opts;

#endif /* OPTIOINS_H__ */
