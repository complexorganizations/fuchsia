// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.25.0
// 	protoc        v3.5.1
// source: set_artifacts.proto

package proto

import (
	proto "github.com/golang/protobuf/proto"
	protoreflect "google.golang.org/protobuf/reflect/protoreflect"
	protoimpl "google.golang.org/protobuf/runtime/protoimpl"
	reflect "reflect"
	sync "sync"
)

const (
	// Verify that this generated code is sufficiently up-to-date.
	_ = protoimpl.EnforceVersion(20 - protoimpl.MinVersion)
	// Verify that runtime/protoimpl is sufficiently up-to-date.
	_ = protoimpl.EnforceVersion(protoimpl.MaxVersion - 20)
)

// This is a compile-time assertion that a sufficiently up-to-date version
// of the legacy proto package is being used.
const _ = proto.ProtoPackageIsVersion4

// SetArtifacts contains information about the manifests and other metadata
// produced by `fint set`.
type SetArtifacts struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	// A brief error log populated in case of a recognized failure mode (e.g.
	// `gn gen` failure due to a broken build graph).
	FailureSummary string `protobuf:"bytes,1,opt,name=failure_summary,json=failureSummary,proto3" json:"failure_summary,omitempty"`
}

func (x *SetArtifacts) Reset() {
	*x = SetArtifacts{}
	if protoimpl.UnsafeEnabled {
		mi := &file_set_artifacts_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *SetArtifacts) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*SetArtifacts) ProtoMessage() {}

func (x *SetArtifacts) ProtoReflect() protoreflect.Message {
	mi := &file_set_artifacts_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use SetArtifacts.ProtoReflect.Descriptor instead.
func (*SetArtifacts) Descriptor() ([]byte, []int) {
	return file_set_artifacts_proto_rawDescGZIP(), []int{0}
}

func (x *SetArtifacts) GetFailureSummary() string {
	if x != nil {
		return x.FailureSummary
	}
	return ""
}

var File_set_artifacts_proto protoreflect.FileDescriptor

var file_set_artifacts_proto_rawDesc = []byte{
	0x0a, 0x13, 0x73, 0x65, 0x74, 0x5f, 0x61, 0x72, 0x74, 0x69, 0x66, 0x61, 0x63, 0x74, 0x73, 0x2e,
	0x70, 0x72, 0x6f, 0x74, 0x6f, 0x12, 0x04, 0x66, 0x69, 0x6e, 0x74, 0x22, 0x37, 0x0a, 0x0c, 0x53,
	0x65, 0x74, 0x41, 0x72, 0x74, 0x69, 0x66, 0x61, 0x63, 0x74, 0x73, 0x12, 0x27, 0x0a, 0x0f, 0x66,
	0x61, 0x69, 0x6c, 0x75, 0x72, 0x65, 0x5f, 0x73, 0x75, 0x6d, 0x6d, 0x61, 0x72, 0x79, 0x18, 0x01,
	0x20, 0x01, 0x28, 0x09, 0x52, 0x0e, 0x66, 0x61, 0x69, 0x6c, 0x75, 0x72, 0x65, 0x53, 0x75, 0x6d,
	0x6d, 0x61, 0x72, 0x79, 0x42, 0x39, 0x5a, 0x37, 0x67, 0x6f, 0x2e, 0x66, 0x75, 0x63, 0x68, 0x73,
	0x69, 0x61, 0x2e, 0x64, 0x65, 0x76, 0x2f, 0x66, 0x75, 0x63, 0x68, 0x73, 0x69, 0x61, 0x2f, 0x74,
	0x6f, 0x6f, 0x6c, 0x73, 0x2f, 0x69, 0x6e, 0x74, 0x65, 0x67, 0x72, 0x61, 0x74, 0x69, 0x6f, 0x6e,
	0x2f, 0x63, 0x6d, 0x64, 0x2f, 0x66, 0x69, 0x6e, 0x74, 0x2f, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x62,
	0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_set_artifacts_proto_rawDescOnce sync.Once
	file_set_artifacts_proto_rawDescData = file_set_artifacts_proto_rawDesc
)

func file_set_artifacts_proto_rawDescGZIP() []byte {
	file_set_artifacts_proto_rawDescOnce.Do(func() {
		file_set_artifacts_proto_rawDescData = protoimpl.X.CompressGZIP(file_set_artifacts_proto_rawDescData)
	})
	return file_set_artifacts_proto_rawDescData
}

var file_set_artifacts_proto_msgTypes = make([]protoimpl.MessageInfo, 1)
var file_set_artifacts_proto_goTypes = []interface{}{
	(*SetArtifacts)(nil), // 0: fint.SetArtifacts
}
var file_set_artifacts_proto_depIdxs = []int32{
	0, // [0:0] is the sub-list for method output_type
	0, // [0:0] is the sub-list for method input_type
	0, // [0:0] is the sub-list for extension type_name
	0, // [0:0] is the sub-list for extension extendee
	0, // [0:0] is the sub-list for field type_name
}

func init() { file_set_artifacts_proto_init() }
func file_set_artifacts_proto_init() {
	if File_set_artifacts_proto != nil {
		return
	}
	if !protoimpl.UnsafeEnabled {
		file_set_artifacts_proto_msgTypes[0].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*SetArtifacts); i {
			case 0:
				return &v.state
			case 1:
				return &v.sizeCache
			case 2:
				return &v.unknownFields
			default:
				return nil
			}
		}
	}
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: file_set_artifacts_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   1,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_set_artifacts_proto_goTypes,
		DependencyIndexes: file_set_artifacts_proto_depIdxs,
		MessageInfos:      file_set_artifacts_proto_msgTypes,
	}.Build()
	File_set_artifacts_proto = out.File
	file_set_artifacts_proto_rawDesc = nil
	file_set_artifacts_proto_goTypes = nil
	file_set_artifacts_proto_depIdxs = nil
}
