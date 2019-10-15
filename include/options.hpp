#ifndef __OPTIOINS_H__
#define __OPTIOINS_H__

#include <string>
#include <stdio.h>
#include <unistd.h>
#include "section.hpp"

class Options {
public:
  Options();
  int get_opts(int argc, char **argv);
  //VCSectionType Type() const { return m_type; }
  VCSectionType TypePop();// { return m_type; }
  int TypeNum() const { return m_type_cnt; }
  const char * FileName() const { return m_fileName.c_str(); }
  void PrintSection(void (*print)(VCSectionType type));

private:
  void SetName(char** name) { m_progName = *name; }
  void PrintHelp();

  std::string m_progName;
  std::string m_fileName;
  std::string m_defaultFileName;
  std::vector<VCSectionType> m_type; // include all
  int m_type_cnt;
};

extern Options g_opts;

#endif /* OPTIOINS_H__ */
