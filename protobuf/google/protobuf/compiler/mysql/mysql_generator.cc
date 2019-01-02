
// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.
// http://code.google.com/p/protobuf/
//

// Author: tonyjobmails@gmail.com (Tao Han)
// Based on original Protocol Buffers design by
// Sanjay Ghemawat, Jeff Dean, and others.
//

#include <google/protobuf/compiler/mysql/mysql_generator.h>

#include <limits>
#include <map>
#include <utility>
#include <string>
#include <vector>

#include <google/protobuf/descriptor.pb.h>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/stringprintf.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/substitute.h>
#include <google/protobuf/wire_format_lite.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace mysql {

// Returns a copy of |filename| with any trailing ".protodevel" or ".proto
// suffix stripped.
// TODO(robinson): Unify with copy in compiler/cpp/internal/helpers.cc.
string StripProto(const string& filename) {
  if (HasSuffixString(filename, ".protodevel")) {
    return StripSuffixString(filename, ".protodevel");
  } else {
    return StripSuffixString(filename, ".proto");
  }
}

// Returns the Python module name expected for a given .proto filename.
string ModuleName(const string& filename) {
  string basename = StripProto(filename);
  StripString(&basename, "-", '_');
  StripString(&basename, "/", '.');
  return basename + "_pb2";
}

const char* PrimitiveTypeName(FieldDescriptor::Type type) {
  switch (type) {
    case FieldDescriptor::TYPE_DOUBLE  : return "double";
    case FieldDescriptor::TYPE_FLOAT   : return "float";
    case FieldDescriptor::TYPE_INT64   : return "bigint";
    case FieldDescriptor::TYPE_UINT64  : return "bigint";
    case FieldDescriptor::TYPE_INT32   : return "int";
    case FieldDescriptor::TYPE_FIXED64 : return "bigint";
    case FieldDescriptor::TYPE_FIXED32 : return "int";
    case FieldDescriptor::TYPE_BOOL    : return "tinyint";
    case FieldDescriptor::TYPE_STRING  : return "varchar(64)";
    case FieldDescriptor::TYPE_GROUP   : return NULL;
    case FieldDescriptor::TYPE_MESSAGE : return NULL;
    case FieldDescriptor::TYPE_BYTES   : return "blob";
    case FieldDescriptor::TYPE_UINT32  : return "int";
    case FieldDescriptor::TYPE_ENUM    : return NULL;
    case FieldDescriptor::TYPE_SFIXED32: return "int";
    case FieldDescriptor::TYPE_SFIXED64: return "bigint";
    case FieldDescriptor::TYPE_SINT32  : return "int";
    case FieldDescriptor::TYPE_SINT64  : return "bigint";

    // No default because we want the compiler to complain if any new
    // CppTypes are added.
  }

  GOOGLE_LOG(FATAL) << "Can't get here.";
  return NULL;
}

// For encodings with fixed sizes, returns that size in bytes.  Otherwise
// returns -1.
int FixedSize(FieldDescriptor::Type type) {
  switch (type) {
    case FieldDescriptor::TYPE_INT32   : return -1;
    case FieldDescriptor::TYPE_INT64   : return -1;
    case FieldDescriptor::TYPE_UINT32  : return -1;
    case FieldDescriptor::TYPE_UINT64  : return -1;
    case FieldDescriptor::TYPE_SINT32  : return -1;
    case FieldDescriptor::TYPE_SINT64  : return -1;
    case FieldDescriptor::TYPE_FIXED32 : return internal::WireFormatLite::kFixed32Size;
    case FieldDescriptor::TYPE_FIXED64 : return internal::WireFormatLite::kFixed64Size;
    case FieldDescriptor::TYPE_SFIXED32: return internal::WireFormatLite::kSFixed32Size;
    case FieldDescriptor::TYPE_SFIXED64: return internal::WireFormatLite::kSFixed64Size;
    case FieldDescriptor::TYPE_FLOAT   : return internal::WireFormatLite::kFloatSize;
    case FieldDescriptor::TYPE_DOUBLE  : return internal::WireFormatLite::kDoubleSize;

    case FieldDescriptor::TYPE_BOOL    : return internal::WireFormatLite::kBoolSize;
    case FieldDescriptor::TYPE_ENUM    : return -1;

    case FieldDescriptor::TYPE_STRING  : return -1;
    case FieldDescriptor::TYPE_BYTES   : return -1;
    case FieldDescriptor::TYPE_GROUP   : return -1;
    case FieldDescriptor::TYPE_MESSAGE : return -1;

    // No default because we want the compiler to complain if any new
    // types are added.
  }
  GOOGLE_LOG(FATAL) << "Can't get here.";
  return -1;
}

string FieldName(const FieldDescriptor* field) {
  string result = field->name();
  LowerString(&result);
  return result;
}

void SetPrimitiveVariables(const FieldDescriptor* descriptor,
                           map<string, string>* variables) {
  (*variables)["type"] = PrimitiveTypeName(descriptor->type());
  (*variables)["name"] = FieldName(descriptor);
  // (*variables)["default"] = DefaultValue(descriptor);
  // (*variables)["tag"] = SimpleItoa(internal::WireFormat::MakeTag(descriptor));
  int fixed_size = FixedSize(descriptor->type());
  if (fixed_size != -1) {
    (*variables)["fixed_size"] = SimpleItoa(fixed_size);
  }
}

// Prints the common boilerplate needed at the top of every .sql
// file output by this generator.
void PrintTopBoilerplate(
    io::Printer* printer, const FileDescriptor* file, bool descriptor_proto) {
  printer->Print(
      "# Generated by the protocol buffer compiler.  DO NOT EDIT!\n"
      "# source: $filename$\n"
      "\n",
      "filename", file->name());
  printer->Print("\n\n");
}

Generator::Generator() : file_(NULL) {}

Generator::~Generator() {}

bool Generator::Generate(const FileDescriptor* file,
                         const string& parameter,
                         GeneratorContext* context,
                         string* error) const {
  MutexLock lock(&mutex_);
  file_ = file;
  string basename = StripProto(file->name());
  basename.append(".pb.sql");

  FileDescriptorProto file_proto;
  file_->CopyTo(&file_proto);
  file_proto.SerializeToString(&file_descriptor_serialized_);

  scoped_ptr<io::ZeroCopyOutputStream> output(context->Open(basename));
  GOOGLE_CHECK(output.get());
  io::Printer printer(output.get(), '$');
  printer_ = &printer;

  this->PrintMessages();

  return true;
}

void Generator::PrintMessages() const {
  for (int i = 0; i < file_->message_type_count(); ++i) {
    PrintMessage(*file_->message_type(i));
    printer_->Print("\n");
    PrintStoredProcedure(*file_->message_type(i));
    printer_->Print("\n");
  }
}

void Generator::PrintMessage(const Descriptor& message_descriptor) const {
  printer_->Print("Drop TABLE IF EXISTS `$name$`;\n", "name",
      message_descriptor.full_name());
  printer_->Indent();

  printer_->Print("CREATE TABLE `$name$` (\n", "name",
      message_descriptor.full_name());
  printer_->Indent();

  for (int i = 0; i < message_descriptor.field_count(); ++i) {
    map<string, string> variables;
    SetPrimitiveVariables(message_descriptor.field(i), &variables);
    if (i == message_descriptor.field_count() - 1) {
        printer_->Print(variables, "`$name$` $type$\n");
    } else {
        printer_->Print(variables, "`$name$` $type$,\n");
    }
  }

  printer_->Print(") ENGINE=InnoDB DEFAULT CHARSET=utf8;");
}

void Generator::PrintStoredProcedure(const Descriptor& message_descriptor) const {
}

}  // namespace mysql
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
