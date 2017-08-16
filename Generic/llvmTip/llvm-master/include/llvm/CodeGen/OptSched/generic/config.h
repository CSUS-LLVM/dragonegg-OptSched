/*******************************************************************************
Description:  Provides an interface to read a configuration file. The format is
              trivial: each entry is a name and value, separated by whitespace.
              Multiple entries are also separated by whitespace (usually line
              breaks). Hash marks after whitespace cause the rest of the line to
              be ignored.
Author:       Max Shawabkeh
Created:      Mar. 2011
Last Update:  Mar. 2011
*******************************************************************************/

#ifndef OPTSCHED_GENERIC_CONFIG_H
#define OPTSCHED_GENERIC_CONFIG_H

#include "llvm/CodeGen/OptSched/generic/defines.h"
#include <iostream>
#include <list>
#include <map>
#include <string>

namespace opt_sched {

using std::string;
using std::list;

class Config {
public:
  // Loads settings from a configuration file.
  void Load(const string &filepath);
  void Load(std::istream &file);
  // All these functions return the value of a setting record of the given
  // name, with optional automatic parsing and defaults.
  string GetString(const string &name) const;
  string GetString(const string &name, const string &default_) const;
  int64_t GetInt(const string &name) const;
  int64_t GetInt(const string &name, int64_t default_) const;
  float GetFloat(const string &name) const;
  float GetFloat(const string &name, float default_) const;
  bool GetBool(const string &name) const;
  bool GetBool(const string &name, bool default_) const;
  list<string> GetStringList(const string &name) const;
  list<int64_t> GetIntList(const string &name) const;
  list<float> GetFloatList(const string &name) const;

protected:
  std::map<string, string> settings;
};

} // end namespace opt_sched

#endif
