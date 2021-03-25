// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package codegen

const testBaseTmpl = `
{{- define "TestBase" -}}
{{- UseWire -}}
// WARNING: This file is machine generated by fidlgen.

#pragma once

#include <{{ .PrimaryHeader }}>

{{- range .Decls }}
  {{- if Eq .Kind Kinds.Protocol }}
{{ EnsureNamespace .TestBase }}


class {{ .TestBase.Name }} : public {{ . }}::Interface {
  public:
  virtual ~{{ .TestBase.Name }}() { }
  virtual void NotImplemented_(const std::string& name, ::fidl::CompleterBase& completer) = 0;

  using Interface = {{ . }}::Interface;

  {{- range .Methods }}
    {{- if .HasRequest }}
    virtual void {{ .Name }}(
        {{- .RequestArgs | Params }}{{ if .RequestArgs }}, {{ end -}}
        {{ .WireCompleter }}::Sync& completer) override { NotImplemented_("{{ .Name }}", completer); }
    {{- end }}
  {{- end }}
};
  {{- end }}
{{- end -}}

{{ EndOfFile }}
{{ end }}
`
