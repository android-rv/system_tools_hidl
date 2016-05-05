/*
 * Copyright (C) 2015, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "io_delegate.h"

#include <cstring>
#include <fstream>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <android-base/strings.h>

#include "logging.h"
#include "os.h"

using std::string;
using std::unique_ptr;
using std::vector;

using android::base::Split;

namespace android {
namespace hidl {

unique_ptr<string> IoDelegate::GetFileContents(
    const string& filename,
    const string& content_suffix) const {
  unique_ptr<string> contents;
  std::ifstream in(filename, std::ios::in | std::ios::binary);
  if (!in) {
    return contents;
  }
  contents.reset(new string);
  in.seekg(0, std::ios::end);
  ssize_t file_size = in.tellg();
  contents->resize(file_size + content_suffix.length());
  in.seekg(0, std::ios::beg);
  // Read the file contents into the beginning of the string
  in.read(&(*contents)[0], file_size);
  // Drop the suffix in at the end.
  contents->replace(file_size, content_suffix.length(), content_suffix);
  in.close();

  return contents;
}

unique_ptr<LineReader> IoDelegate::GetLineReader(
    const string& file_path) const {
  return LineReader::ReadFromFile(file_path);
}

bool IoDelegate::FileIsReadable(const string& path) const {
#ifdef _WIN32
  // check that the file exists and is not Write-only
  return (0 == _access(path.c_str(), 0)) &&  // mode 0=exist
         (0 == _access(path.c_str(), 4));    // mode 4=readable
#else
  return (0 == access(path.c_str(), R_OK));
#endif
}

bool IoDelegate::CreatedNestedDirs(
    const string& caller_base_dir,
    const vector<string>& nested_subdirs) const {
  string base_dir = caller_base_dir;
  if (base_dir.empty()) {
    base_dir = ".";
  }
  for (const string& subdir : nested_subdirs) {
    if (base_dir[base_dir.size() - 1] != OS_PATH_SEPARATOR) {
      base_dir += OS_PATH_SEPARATOR;
    }
    base_dir += subdir;
    bool success;
#ifdef _WIN32
    success = _mkdir(base_dir.c_str()) == 0;
#else
    success = mkdir(base_dir.c_str(),
                    S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0;
#endif
    // On darwin when you try to mkdir("/", ...) we get EISDIR.
    if (!success && (errno != EEXIST && errno != EISDIR)) {
       LOG(ERROR) << "Error while creating directories: " << strerror(errno);
       return false;
    }
  }
  return true;
}

bool IoDelegate::CreatePathForFile(const string& path) const {
  if (path.empty()) {
    return true;
  }

  string base = ".";
  if (path[0] == OS_PATH_SEPARATOR) {
    base = "/";
  }

  auto split = Split(path, string{1u, OS_PATH_SEPARATOR});
  split.pop_back();

  return CreatedNestedDirs(base, split);
}

unique_ptr<CodeWriter> IoDelegate::GetCodeWriter(
    const string& file_path) const {
  return GetFileWriter(file_path);
}

void IoDelegate::RemovePath(const std::string& file_path) const {
#ifdef _WIN32
  _unlink(file_path.c_str());
#else
  unlink(file_path.c_str());
#endif
}

}  // namespace android
}  // namespace hidl
