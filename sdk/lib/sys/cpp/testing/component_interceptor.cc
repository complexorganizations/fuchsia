// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The intercepting mechanism works by creating an Environment containing a
// custom |fuchsia.sys.Loader| and |fuchsia.sys.Runner|. This custom environment
// loader, which answers to all components launches under this environment,
// responds with an autogenerated package directory with a .cmx pointing to a
// custom runner component. The runner component, which will also under the
// environment, forwards its requests back up to environment's injected
// |fuchsia.sys.Runner| implemented here.

#include <lib/sys/cpp/testing/component_interceptor.h>

#include <fuchsia/io/cpp/fidl.h>
#include <lib/fidl/cpp/interface_handle.h>
#include <lib/vfs/cpp/pseudo_dir.h>
#include <lib/vfs/cpp/pseudo_file.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <zircon/assert.h>
#include <zx/channel.h>
#include <memory>

namespace sys {
namespace testing {

namespace {
// The runner we inject in autogenerated .cmx files.
constexpr char kEnvironmentDelegatingRunner[] =
    "fuchsia-pkg://fuchsia.com/environment_delegating_runner#meta/"
    "environment_delegating_runner.cmx";

// Relative path within the autogenerated package directory to the manifest.
constexpr char kAutogenPkgDirManifestPath[] = "autogenerated_manifest.cmx";
// Path to the autogenerated cmx file of the intercepted component.
constexpr char kAutogenCmxPath[] =
    "fuchsia-pkg://example.com/fake_pkg#autogenerated_manifest.cmx";
}  // namespace

ComponentInterceptor::ComponentInterceptor(
    fuchsia::sys::LoaderPtr fallback_loader, async_dispatcher_t* dispatcher)
    : fallback_loader_(std::move(fallback_loader)), dispatcher_(dispatcher) {
  loader_svc_ = std::make_shared<vfs::Service>(
      [this](zx::channel h, async_dispatcher_t* dispatcher) mutable {
        loader_bindings_.AddBinding(
            this, fidl::InterfaceRequest<fuchsia::sys::Loader>(std::move(h)),
            dispatcher_);
      });
}

ComponentInterceptor::~ComponentInterceptor() = default;

// static
ComponentInterceptor ComponentInterceptor::CreateWithEnvironmentLoader(
    const fuchsia::sys::EnvironmentPtr& env, async_dispatcher_t* dispatcher) {
  // The fallback loader comes from |parent_env|.
  fuchsia::sys::LoaderPtr fallback_loader;
  fuchsia::sys::ServiceProviderPtr sp;
  env->GetServices(sp.NewRequest());
  sp->ConnectToService(fuchsia::sys::Loader::Name_,
                       fallback_loader.NewRequest().TakeChannel());

  return ComponentInterceptor(std::move(fallback_loader), dispatcher);
}

std::unique_ptr<EnvironmentServices>
ComponentInterceptor::MakeEnvironmentServices(
    const fuchsia::sys::EnvironmentPtr& parent_env) {
  auto env_services = EnvironmentServices::CreateWithCustomLoader(
      parent_env, loader_svc_, dispatcher_);

  env_services->AddService(runner_bindings_.GetHandler(this, dispatcher_));
  return env_services;
}

// Modifies the supplied |cmx| such that:
// * required fields in .cmx are set if not present:
//    - program.binary
// * the runner is the environment delegating runner.
void SetDefaultsForCmx(rapidjson::Document* cmx) {
  // 1. Enforce that it has delegating runner.
  cmx->RemoveMember("runner");
  cmx->AddMember("runner", kEnvironmentDelegatingRunner, cmx->GetAllocator());

  // 2. If "program" is not set, give it a default one with an empty binary.
  if (!cmx->HasMember("program")) {
    rapidjson::Value program;
    program.SetObject();
    program.AddMember("binary", "", cmx->GetAllocator());
    cmx->AddMember("program", program, cmx->GetAllocator());
  }
}

bool ComponentInterceptor::InterceptURL(std::string component_url,
                                        std::string extra_cmx_contents,
                                        ComponentLaunchHandler handler) {
  ZX_DEBUG_ASSERT_MSG(handler, "Must be a valid handler.");

  // 1. Parse the extra_cmx_contents. Enforce that our delgating runner is
  //    specified, and give it defaults for required fields.
  rapidjson::Document cmx;
  cmx.Parse(extra_cmx_contents);
  if (!cmx.IsObject() && !cmx.IsNull()) {
    return false;
  }
  if (cmx.IsNull()) {
    cmx.SetObject();
  }
  SetDefaultsForCmx(&cmx);

  // 2. Construct a package directory and put the |cmx| manifest in it
  // for this particular component URL.
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
  cmx.Accept(writer);
  std::string cmx_str = buf.GetString();

  ComponentLoadInfo info;
  info.pkg_dir = std::make_unique<vfs::PseudoDir>();
  info.pkg_dir->AddEntry(
      kAutogenPkgDirManifestPath,
      std::make_unique<vfs::BufferedPseudoFile>(
          [cmx_str = std::move(cmx_str)](std::vector<uint8_t>* out) {
            std::copy(cmx_str.begin(), cmx_str.end(), std::back_inserter(*out));
            return ZX_OK;
          }));
  info.handler = std::move(handler);

  std::lock_guard<std::mutex> lock(intercept_urls_mu_);
  intercepted_component_load_info_[component_url] = std::move(info);

  return true;
}

void ComponentInterceptor::LoadUrl(std::string url, LoadUrlCallback response) {
  std::lock_guard<std::mutex> lock(intercept_urls_mu_);

  auto it = intercepted_component_load_info_.find(url);
  if (it == intercepted_component_load_info_.end()) {
    fallback_loader_->LoadUrl(url, std::move(response));
    return;
  }

  auto pkg = std::make_unique<fuchsia::sys::Package>();
  fidl::InterfaceHandle<fuchsia::io::Directory> dir_handle;
  it->second.pkg_dir->Serve(fuchsia::io::OPEN_RIGHT_READABLE,
                            dir_handle.NewRequest().TakeChannel());

  pkg->directory = dir_handle.TakeChannel();
  pkg->resolved_url = kAutogenCmxPath;
  response(std::move(pkg));
  // After this point, the runner specified in the autogenerated manifest should
  // forward its requests back to us over our Runner fidl binding.
}

void ComponentInterceptor::StartComponent(
    fuchsia::sys::Package package, fuchsia::sys::StartupInfo startup_info,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController> controller) {
  // This is a buffer to store the move-only handler while we invoke it.
  ComponentLaunchHandler handler;
  auto url = startup_info.launch_info.url;
  {
    std::lock_guard<std::mutex> lock(intercept_urls_mu_);
    auto it = intercepted_component_load_info_.find(url);
    ZX_DEBUG_ASSERT(it != intercepted_component_load_info_.end());

    // This allows that handler to re-entrantly call InterceptURL() without
    // deadlocking this on |intercept_urls_mu_|
    handler = std::move(it->second.handler);
  }

  handler(std::move(startup_info), std::make_unique<InterceptedComponent>(
                                       std::move(controller), dispatcher_));

  // Put the |handler| back where it came from.
  {
    std::lock_guard<std::mutex> lock(intercept_urls_mu_);
    auto it = intercepted_component_load_info_.find(url);
    ZX_DEBUG_ASSERT(it != intercepted_component_load_info_.end());

    it->second.handler = std::move(handler);
  }
}

InterceptedComponent::InterceptedComponent(
    fidl::InterfaceRequest<fuchsia::sys::ComponentController> request,
    async_dispatcher_t* dispatcher)
    : binding_(this),
      termination_reason_(TerminationReason::EXITED),
      exit_code_(ZX_OK) {
  binding_.Bind(std::move(request), dispatcher);
  binding_.set_error_handler([this](zx_status_t status) {
    termination_reason_ = TerminationReason::UNKNOWN;
    Kill();
  });
}

InterceptedComponent::~InterceptedComponent() {
  on_kill_ = nullptr;
  Kill();
}

void InterceptedComponent::Exit(int64_t exit_code, TerminationReason reason) {
  exit_code_ = exit_code;
  termination_reason_ = reason;
  Kill();
}

void InterceptedComponent::Kill() {
  if (on_kill_) {
    on_kill_();
  }
  binding_.events().OnTerminated(exit_code_, termination_reason_);
  binding_.Unbind();
}

void InterceptedComponent::Detach() { binding_.set_error_handler(nullptr); }

}  // namespace testing
}  // namespace sys
