#include "runner.hpp"
#include <regex>
#include <iostream>

using namespace std;

static void collect_files(const filesystem::path& path, vector<filesystem::path>& files)
{
  static const regex pattern("\\.(h|hpp|hxx)$");
  auto filename = path.filename().string();
  auto match = sregex_iterator(filename.begin(), filename.end(), pattern);

  if (filesystem::is_directory(path))
  {
    for (const auto& entry : filesystem::recursive_directory_iterator(path))
      collect_files(entry.path(), files);
  }
  else if (match != sregex_iterator())
    files.push_back(path);
}

bool probe_and_run_parser(TwiliParser& parser, int argc, const char** argv, vector<filesystem::path>& files)
{
  for (const string& dirpath : parser.get_directories())
    collect_files(filesystem::path(dirpath), files);
  return run_parser(parser, files, argc, argv);
}

bool probe_and_run_parser(TwiliParser& parser, int argc, const char** argv)
{
  vector<filesystem::path> files;

  for (const string& dirpath : parser.get_directories())
    collect_files(filesystem::path(dirpath), files);
  return run_parser(parser, files, argc, argv);
}

bool run_parser(TwiliParser& parser, const vector<filesystem::path>& files, int argc, const char** argv)
{
  for (const auto& filepath : files)
  {
    CXIndex index = clang_createIndex(0, 0);
    CXTranslationUnit unit = clang_parseTranslationUnit(
      index,
      filepath.string().c_str(),
      argv, argc,
      nullptr, 0,
      CXTranslationUnit_None
    );

    cout << "- Importing " << filepath.string() << endl;
    if (unit == nullptr || !parser(unit))
    {
      cerr << "/!\\ Failed to parse file " << filepath.string() << endl;
      return false;
    }
    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(index);
  }
  return true;
}
