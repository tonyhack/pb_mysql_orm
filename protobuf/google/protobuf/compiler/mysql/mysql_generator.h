// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.
// http://code.google.com/p/protobuf/
//

// Author: tonyjobmails@gmail.com (Tao Han)
// Based on original Protocol Buffers design by
// Sanjay Ghemawat, Jeff Dean, and others.
//

#ifndef GOOGLE_PROTOBUF_COMPILER_MYSQL_GENERATOR_H__
#define GOOGLE_PROTOBUF_COMPILER_MYSQL_GENERATOR_H__

#include <string>

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {

class Descriptor;
class EnumDescriptor;
class EnumValueDescriptor;
class FieldDescriptor;
class ServiceDescriptor;

namespace io { class Printer; }

namespace compiler {
namespace mysql {

// CodeGenerator implementation for generated Python protocol buffer classes.
// If you create your own protocol compiler binary and you want it to support
// Python output, you can do so by registering an instance of this
// CodeGenerator with the CommandLineInterface in your main() function.
class LIBPROTOC_EXPORT Generator : public CodeGenerator {
 public:
  Generator();
  virtual ~Generator();

  // CodeGenerator methods.
  virtual bool Generate(const FileDescriptor* file,
                        const string& parameter,
                        GeneratorContext* generator_context,
                        string* error) const;

 private:
  void PrintMessages() const;
  void PrintMessage(const Descriptor& message_descriptor) const;
  void PrintStoredProcedure(const Descriptor& message_descriptor) const;

  // Very coarse-grained lock to ensure that Generate() is reentrant.
  // Guards file_, printer_ and file_descriptor_serialized_.
  mutable Mutex mutex_;
  mutable const FileDescriptor* file_;
  mutable string file_descriptor_serialized_;
  mutable io::Printer* printer_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(Generator);
};

}  // namespace mysql
}  // namespace compiler

}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_COMPILER_MYSQL_GENERATOR_H__
