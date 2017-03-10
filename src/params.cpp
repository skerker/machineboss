#include <fstream>
#include "params.h"
#include "schema.h"
#include "util.h"

void Params::readJson (istream& in) {
  json pj;
  in >> pj;
  readJson(pj);
}

void Params::readJson (const json& pj) {
  MachineSchema::validateOrDie ("params", pj);
  param.clear();
  const auto& p = pj.at("param");
  for (auto iter = p.begin(); iter != p.end(); ++iter)
    param[iter.key()] = iter.value().get<double>();
}

void Params::writeJson (ostream& out) const {
  out << "{\"param\":{";
  size_t n = 0;
  for (auto& pv: param)
    out << (n++ ? "," : "") << "\"" << pv.first << "\":" << pv.second;
  out << "}}";
}

Params Params::fromJson (istream& in) {
  Params p;
  p.readJson (in);
  return p;
}

Params Params::fromFile (const char* filename) {
  ifstream infile (filename);
  if (!infile)
    Fail ("File not found: %s", filename);
  return fromJson (infile);
}