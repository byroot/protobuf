#include "RubyGenerator.h"

namespace google {
namespace protobuf  {
namespace compiler {
namespace ruby {

RubyGenerator::RubyGenerator() {}  // Constructor
RubyGenerator::~RubyGenerator() {} // Destructor

// Generate one or more ruby source files for the given proto file.
bool RubyGenerator::Generate(const FileDescriptor* file,
                              const string& parameter,
                              GeneratorContext* context,
                              string* error) const {
  file_ = file;
  context_ = context;

  filename = CreateRubyFileName(file_->name());
  ns_vector.clear();
  SplitStringUsing(file_->package(), ".", &ns_vector);

  // Get a ZeroCopyOutputStream object of the data.
  scoped_ptr<io::ZeroCopyOutputStream> output(context_->Open(filename));

  // Check the data against Google's proto checking algorithm.
  GOOGLE_CHECK(output.get());

  // Get a printer.
  io::Printer printer(output.get(), '$');
  printer_ = &printer;

  PrintGeneratedFileComment();
  PrintGenericRequires();
  PrintImportRequires();

  PrintEnclosingNamespaceModules();

  PrintEnumsForFileDescriptor(file_, false);
  PrintNewLine();
  PrintMessagesForFileDescriptor(file_, false);
  PrintNewLine();

  PrintEnumsForFileDescriptor(file_, true);
  PrintNewLine();
  PrintMessagesForFileDescriptor(file_, true);
  PrintNewLine();

  PrintServices();

  PrintEnclosingNamespaceModuleEnds();

  if (printer.failed()) {
    *error = "An unknown error occured writing file " + filename;
    return false;
  }
  else {
    return true;
  }
} // end Generate()

//
///////////////////////////////////////////////// [ namespaces ] //////////////
//

void RubyGenerator::PrintEnclosingNamespaceModules() const {
  PrintNewLine();
  vector<string>::iterator iter;
  map<string,string> data;
  for (iter = ns_vector.begin(); iter < ns_vector.end(); iter++) {
    data["ns"] = Constantize(*iter, false);

    printer_->Print(data, "module $ns$");
    PrintNewLine();
    printer_->Indent();
  }
}

void RubyGenerator::PrintEnclosingNamespaceModuleEnds() const {
  vector<string>::iterator iter;
  for (iter = ns_vector.begin(); iter < ns_vector.end(); iter++) {
    printer_->Outdent();
    printer_->Print("end");
    PrintNewLine();
  }
}


//
///////////////////////////////////////////////// [ messages ] ////////////////
//

void RubyGenerator::PrintMessagesForFileDescriptor(const FileDescriptor* descriptor, bool print_fields) const {
  if (descriptor->message_type_count() > 0) {
    if (print_fields) {
      PrintComment("Message Fields", true);
    }
    else {
      PrintComment("Message Classes", true);
    }

    for (int i = 0; i < descriptor->message_type_count(); i++) {
      if (print_fields) {
        PrintMessageFields(descriptor->message_type(i));
      }
      else {
        PrintMessageClass(descriptor->message_type(i));
      }
    }
  }
}

void RubyGenerator::PrintMessagesForDescriptor(const Descriptor* descriptor, bool print_fields) const {
  for (int i = 0; i < descriptor->nested_type_count(); i++) {
    if (print_fields) {
      PrintMessageFields(descriptor->nested_type(i));
    }
    else {
      PrintMessageClass(descriptor->nested_type(i));
    }
  }
}
//
// Print out the given descriptor message as a Ruby class.
void RubyGenerator::PrintMessageClass(const Descriptor* descriptor) const {
  map<string,string> data;
  data["class_name"] = descriptor->name();

  printer_->Print(data, "class $class_name$ < ::Protobuf::Message; end");
  PrintNewLine();

  PrintEnumsForDescriptor(descriptor, false);
  PrintMessagesForDescriptor(descriptor, false);
}

void RubyGenerator::PrintExtensionRangesForDescriptor(const Descriptor* descriptor) const {
  if (descriptor->extension_range_count() > 0) {
    for (int i = 0; i < descriptor->extension_range_count(); i++) {
      const Descriptor::ExtensionRange* range = descriptor->extension_range(i);
      map<string,string> data;
      data["message_class"] = Constantize(descriptor->full_name());
      data["start"] = SimpleItoa(range->start);
      data["end"] = SimpleItoa(range->end);
      printer_->Print(data, "$message_class$.extensions $start$...$end$");
      PrintNewLine();
    }
  }
}

// Print out the given descriptor message as a Ruby class.
void RubyGenerator::PrintMessageFields(const Descriptor* descriptor) const {
  PrintExtensionRangesForDescriptor(descriptor);

  if (descriptor->field_count() > 0) {
    for (int i = 0; i < descriptor->field_count(); i++) {
      PrintMessageField(descriptor->field(i));
    }

    // Print Extension Fields
    if (descriptor->extension_count() > 0) {
      for (int i = 0; i < descriptor->extension_count(); i++) {
        PrintMessageField(descriptor->extension(i));
      }
    }
    PrintNewLine();
  }

  PrintEnumsForDescriptor(descriptor, true);
  PrintMessagesForDescriptor(descriptor, true);
}

// Print the given FieldDescriptor to the Message DSL methods.
void RubyGenerator::PrintMessageField(const FieldDescriptor* descriptor) const {
  map<string,string> data;
  data["message_class"] = Constantize(descriptor->containing_type()->full_name());
  data["field_name"] = descriptor->lowercase_name();
  data["tag_number"] = SimpleItoa(descriptor->number());
  data["data_type"] = "";
  data["default_opt"] = "";
  data["packed_opt"] = "";
  data["deprecated_opt"] = "";
  data["extension_opt"] = "";

  if (descriptor->is_required()) {
    data["field_label"] = "required";
  }
  else if (descriptor->is_optional()) {
    data["field_label"] = "optional";
  }
  else if (descriptor->is_repeated()) {
    data["field_label"] = "repeated";
  }

  switch (descriptor->type()) {
    // Primitives
    case FieldDescriptor::TYPE_DOUBLE:   data["data_type"] = "::Protobuf::Field::DoubleField"; break;
    case FieldDescriptor::TYPE_FLOAT:    data["data_type"] = "::Protobuf::Field::FloatField"; break;
    case FieldDescriptor::TYPE_INT64:    data["data_type"] = "::Protobuf::Field::Int64Field"; break;
    case FieldDescriptor::TYPE_UINT64:   data["data_type"] = "::Protobuf::Field::Uint64Field"; break;
    case FieldDescriptor::TYPE_INT32:    data["data_type"] = "::Protobuf::Field::Int32Field"; break;
    case FieldDescriptor::TYPE_FIXED64:  data["data_type"] = "::Protobuf::Field::Fixed64Field"; break;
    case FieldDescriptor::TYPE_FIXED32:  data["data_type"] = "::Protobuf::Field::Fixed32Field"; break;
    case FieldDescriptor::TYPE_BOOL:     data["data_type"] = "::Protobuf::Field::BoolField"; break;
    case FieldDescriptor::TYPE_STRING:   data["data_type"] = "::Protobuf::Field::StringField"; break;
    case FieldDescriptor::TYPE_BYTES:    data["data_type"] = "::Protobuf::Field::BytesField"; break;
    case FieldDescriptor::TYPE_UINT32:   data["data_type"] = "::Protobuf::Field::Uint32Field"; break;
    case FieldDescriptor::TYPE_SFIXED32: data["data_type"] = "::Protobuf::Field::Sfixed32Field"; break;
    case FieldDescriptor::TYPE_SFIXED64: data["data_type"] = "::Protobuf::Field::Sfixed64Field"; break;
    case FieldDescriptor::TYPE_SINT32:   data["data_type"] = "::Protobuf::Field::Sint32Field"; break;
    case FieldDescriptor::TYPE_SINT64:   data["data_type"] = "::Protobuf::Field::Sint64Field"; break;

    // Enums
    case FieldDescriptor::TYPE_ENUM:
      data["data_type"] = Constantize(descriptor->enum_type()->full_name());
      break;

    // Messages
    case FieldDescriptor::TYPE_GROUP:
    case FieldDescriptor::TYPE_MESSAGE:
    default:
      data["data_type"] = Constantize(descriptor->message_type()->full_name());
      break;
  }

  if (descriptor->has_default_value()) {
    string value;
    switch(descriptor->cpp_type()) {
      case FieldDescriptor::CPPTYPE_INT32:  value = SimpleItoa(descriptor->default_value_int32()); break;
      case FieldDescriptor::CPPTYPE_INT64:  value = SimpleItoa(descriptor->default_value_int64()); break;
      case FieldDescriptor::CPPTYPE_UINT32: value = SimpleItoa(descriptor->default_value_uint32()); break;
      case FieldDescriptor::CPPTYPE_UINT64: value = SimpleItoa(descriptor->default_value_uint64()); break;
      case FieldDescriptor::CPPTYPE_DOUBLE: value = SimpleDtoa(descriptor->default_value_double()); break;
      case FieldDescriptor::CPPTYPE_FLOAT:  value = SimpleFtoa(descriptor->default_value_float()); break;
      case FieldDescriptor::CPPTYPE_BOOL:   value = descriptor->default_value_bool() ? "true" : "false"; break;
      case FieldDescriptor::CPPTYPE_ENUM:   value = Constantize(descriptor->default_value_enum()->full_name()); break;
      case FieldDescriptor::CPPTYPE_STRING: value = "\"" + descriptor->default_value_string() + "\""; break;
      default: break;
    }
    data["default_opt"] = strings::Substitute(", :default => $0", value);
  }

  if (descriptor->is_packable() && descriptor->options().has_packed()) {
    string packed_bool = descriptor->options().packed() ? "true" : "false";
    data["packed_opt"] = strings::Substitute(", :packed => $0", packed_bool);
  }

  if (descriptor->options().has_deprecated()) {
    string deprecated_bool = descriptor->options().deprecated() ? "true" : "false";
    data["deprecated_opt"] = strings::Substitute(", :deprecated => $0", deprecated_bool);
  }

  if (descriptor->is_extension()) {
    data["extension_opt"] = ", :extension => true";
  }

  printer_->Print(data,
    "$message_class$.$field_label$("
    "$data_type$, "
    ":$field_name$, "
    "$tag_number$"
    "$default_opt$"
    "$packed_opt$"
    "$deprecated_opt$"
    "$extension_opt$"
    ")\n");
}


//
///////////////////////////////////////////////// [ enums ] ///////////////////
//

void RubyGenerator::PrintEnumsForDescriptor(const Descriptor* descriptor, bool print_values) const {
  for (int i = 0; i < descriptor->enum_type_count(); i++) {
    if (print_values) {
      PrintEnumValues(descriptor->enum_type(i));
    }
    else {
      PrintEnumClass(descriptor->enum_type(i));
    }
  }
}

void RubyGenerator::PrintEnumsForFileDescriptor(const FileDescriptor* descriptor, bool print_values) const {
  if (descriptor->enum_type_count() > 0) {
    if (print_values) {
      PrintComment("Enum Values", true);
    }
    else {
      PrintComment("Enum Classes", true);
    }

    for (int i = 0; i < descriptor->enum_type_count(); i++) {
      if (print_values) {
        PrintEnumValues(descriptor->enum_type(i));
      }
      else {
        PrintEnumClass(descriptor->enum_type(i));
      }
    }
  }
}

// Print the given enum descriptor as a Ruby class.
void RubyGenerator::PrintEnumClass(const EnumDescriptor* descriptor) const {
  map<string,string> data;
  data["class_name"] = descriptor->name();
  printer_->Print(data, "class $class_name$ < ::Protobuf::Enum; end");
  PrintNewLine();
}

// Print the given enum descriptor as a Ruby class.
void RubyGenerator::PrintEnumValues(const EnumDescriptor* descriptor) const {
  for (int i = 0; i < descriptor->value_count(); i++) {
    PrintEnumValue(descriptor->value(i));
  }
  PrintNewLine();
}

// Print the given enum value to the Enum class DSL methods.
void RubyGenerator::PrintEnumValue(const EnumValueDescriptor* descriptor) const {
  map<string,string> data;
  data["enum_class"] = Constantize(descriptor->type()->full_name());
  data["name"] = descriptor->name();
  data["number"] = ConvertIntToString(descriptor->number());
  printer_->Print(data, "$enum_class$.define :$name$, $number$\n");
}

//
///////////////////////////////////////////////// [ services ] ////////////////
//

void RubyGenerator::PrintServices() const {
  if (file_->service_count() > 0) {
    PrintComment("Services", true);
    for (int i = 0; i < file_->service_count(); i++) {
      PrintService(file_->service(i));
    }
  }
}

// Print the given service as a Ruby class.
void RubyGenerator::PrintService(const ServiceDescriptor* descriptor) const {
  map<string,string> data;
  data["class_name"] = descriptor->name();

  printer_->Print(data, "class $class_name$ < ::Protobuf::Service");
  PrintNewLine();
  printer_->Indent();

  for (int i = 0; i < descriptor->method_count(); i++) {
    PrintServiceMethod(descriptor->method(i));
  }

  printer_->Outdent();
  printer_->Print("end");
  PrintNewLine();
}

// Print the rpc DSL declaration to the Ruby service class.
void RubyGenerator::PrintServiceMethod(const MethodDescriptor* descriptor) const {
  map<string,string> data;
  data["name"] = Underscore(descriptor->name());
  data["request_klass"] = Constantize(descriptor->input_type()->full_name());
  data["response_klass"] = Constantize(descriptor->output_type()->full_name());
  printer_->Print(data, "rpc :$name$, $request_klass$, $response_klass$");
  PrintNewLine();
}


//
///////////////////////////////////////////////// [ general ] ////////////////
//

// Print a header or one-line comment (as indicated by the as_header bool).
void RubyGenerator::PrintComment(string comment, bool as_header) const {
  char format[] = "# $comment$\n";
  char format_multi[] = "##\n# $comment$\n#\n";

  map<string,string> data;
  data["comment"] = comment;

  if (as_header) {
    printer_->Print(data, format_multi);
  }
  else {
    printer_->Print(data, format);
  }
}

// Prints a require with the given ruby library.
void RubyGenerator::PrintRequire(string lib_name) const {
  map<string,string> data;
  data["lib"] = lib_name;
  printer_->Print(data, "require '$lib$'\n");
}

// Print a comment indicating that the file was generated.
void RubyGenerator::PrintGeneratedFileComment() const {
  PrintComment("This file is auto-generated. DO NOT EDIT!", true);
}

// Print out message requires as they pertain to the ruby library.
void RubyGenerator::PrintGenericRequires() const {
  if (file_->message_type_count() > 0) {
    PrintRequire("protobuf/message");
  }
  if (file_->service_count() > 0) {
    PrintRequire("protobuf/rpc/service");
  }
}

void RubyGenerator::PrintImportRequires() const {
  if (file_->dependency_count() > 0) {
    PrintNewLine();
    PrintComment("Imports", true);
    for (int i = 0; i < file_->dependency_count(); i++) {
      PrintRequire(CreateRubyFileName(file_->dependency(i)->name(), true));
    }
  }
}

// Print a one-line comment.
void RubyGenerator::PrintComment(string comment) const {
  PrintComment(comment, false);
}

void RubyGenerator::PrintNewLine() const {
  printer_->Print("\n");
}

} // namespace ruby
} // namespace compiler
} // namespace protobuf
} // namespace google
