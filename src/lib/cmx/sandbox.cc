// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/lib/cmx/sandbox.h"

#include <algorithm>
#include <map>

#include "rapidjson/document.h"
#include "src/lib/files/path.h"
#include "src/lib/fxl/strings/string_printf.h"
#include "src/lib/fxl/strings/substitute.h"
#include "src/lib/json_parser/json_parser.h"

namespace component {

constexpr char kDev[] = "dev";
constexpr char kSystem[] = "system";
constexpr char kServices[] = "services";
constexpr char kPkgfs[] = "pkgfs";
constexpr char kFeatures[] = "features";
constexpr char kBoot[] = "boot";

void SandboxMetadata::Parse(const rapidjson::Value& sandbox_value, json::JSONParser* json_parser) {
  const std::map<std::string, std::vector<std::string>*> name_to_vec = {
      {kDev, &dev_},     {kSystem, &system_},     {kServices, &services_},
      {kPkgfs, &pkgfs_}, {kFeatures, &features_}, {kBoot, &boot_}};

  for (const auto& entry : name_to_vec) {
    entry.second->clear();
  }
  null_ = true;

  if (!sandbox_value.IsObject()) {
    json_parser->ReportError("Sandbox is not an object.");
    return;
  }

  for (const auto& entry : name_to_vec) {
    const std::string& name = entry.first;
    auto* vec = entry.second;
    auto member = sandbox_value.FindMember(name);
    if (member != sandbox_value.MemberEnd()) {
      if (!member->value.IsArray()) {
        json_parser->ReportError(fxl::StringPrintf("'%s' is not an array.", name.c_str()));
        return;
      }
      vec->clear();
      for (const auto& value_entry : member->value.GetArray()) {
        if (!value_entry.IsString()) {
          json_parser->ReportError(
              fxl::StringPrintf("'%s' contains an item that's not a string.", name.c_str()));
          return;
        }
        std::string path(value_entry.GetString(), value_entry.GetStringLength());
        if (!files::IsValidCanonicalPath(path)) {
          json_parser->ReportError(fxl::StringPrintf(
              "'%s' contained a path that is not in canonical path form", name.c_str()));
          return;
        }
        vec->push_back(path);
      }
    }
  }

  if (!json_parser->HasError()) {
    null_ = false;
  }
}

bool SandboxMetadata::HasFeature(const std::string& feature) const {
  return std::find(features_.begin(), features_.end(), feature) != features_.end();
}

void SandboxMetadata::AddFeature(std::string feature) { features_.push_back(std::move(feature)); }

bool SandboxMetadata::HasService(const std::string& service) const {
  return std::find(services_.begin(), services_.end(), service) != services_.end();
}

bool SandboxMetadata::HasPkgFsPath(const std::string& pkgfs_path) const {
  return std::find(pkgfs_.begin(), pkgfs_.end(), pkgfs_path) != pkgfs_.end();
}

}  // namespace component
