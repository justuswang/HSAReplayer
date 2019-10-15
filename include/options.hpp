#ifndef __OPTIOINS_H__
#define __OPTIOINS_H__

#include <string>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include "section.hpp"
#include "json.hpp"

class Options {
public:
  Options();
  int get_opts(int argc, char **argv);
  //VCSectionType Type() const { return m_type; }
  VCSectionType TypePop();// { return m_type; }
  int TypeNum() const { return m_type_cnt; }
  const char * FileName() const { return m_fileName.c_str(); }
  void PrintSection(void (*print)(VCSectionType type));
  std::vector<KernArgDataType>* KernArgTypes()
  {
    if (kernArgs.size() == 0)
      return NULL;
    else
      return &kernArgs;
  }

private:
  void SetName(char** name) { m_progName = *name; }
  void PrintHelp();
  void ParseJson();
  void ParseKernArg(const char *type);

  std::string m_progName;
  std::string m_fileName;
  std::string m_defaultFileName;
  std::string m_jsonFile;
  std::string m_defaultJsonFile;
  std::vector<KernArgDataType> kernArgs;
  std::vector<VCSectionType> m_type; // include all
  int m_type_cnt;
};

extern Options g_opts;

#endif /* OPTIOINS_H__ */