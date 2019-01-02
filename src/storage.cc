#include "storage.h"

#include <iostream>
#include <memory>
#include <sstream>

#include <stdio.h>
#include <google/protobuf/message.h>

namespace pmo {

static ::google::protobuf::Message *createMessage(const std::string &type) {
    const google::protobuf::Descriptor *descriptor =
        google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(type);
    if (descriptor == NULL) {
        return NULL;
    }

    const google::protobuf::Message *prototype =
        google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor);
    if (prototype == NULL) {
        return NULL;
    }

    return prototype->New();
}

static void reflectionFill(google::protobuf::Message &message, const google::protobuf::FieldDescriptor *descriptor,
        const google::protobuf::Reflection *reflection, const std::map<std::string, std::string> &values) {
    if(descriptor == NULL) {
        return;
    }

    if(descriptor->label() == google::protobuf::FieldDescriptor::LABEL_REPEATED) {
        return;
    }

    switch (descriptor->type()) {
        case google::protobuf::FieldDescriptor::TYPE_FIXED64:
        case google::protobuf::FieldDescriptor::TYPE_INT64:
            reflection->SetInt64(&message, descriptor, (int64_t)atol(values.at(descriptor->name()).c_str()));
            break;
        case google::protobuf::FieldDescriptor::TYPE_UINT64:
            reflection->SetUInt64(&message, descriptor, (uint64_t)atol(values.at(descriptor->name()).c_str()));
            break;
        case google::protobuf::FieldDescriptor::TYPE_FIXED32:
        case google::protobuf::FieldDescriptor::TYPE_INT32:
            reflection->SetInt32(&message, descriptor, (int32_t)atoi(values.at(descriptor->name()).c_str()));
            break;
        case google::protobuf::FieldDescriptor::TYPE_UINT32:
            reflection->SetUInt32(&message, descriptor, (uint32_t)atoi(values.at(descriptor->name()).c_str()));
            break;
        case google::protobuf::FieldDescriptor::TYPE_STRING:
        case google::protobuf::FieldDescriptor::TYPE_BYTES:
            reflection->SetString(&message, descriptor, values.at(descriptor->name()));
            break;
        case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
            reflection->SetDouble(&message, descriptor, atof(values.at(descriptor->name()).c_str()));
            break;
        case google::protobuf::FieldDescriptor::TYPE_FLOAT:
            reflection->SetFloat(&message, descriptor, atof(values.at(descriptor->name()).c_str()));
            break;
        case google::protobuf::FieldDescriptor::TYPE_BOOL:
            if (values.at(descriptor->name()) == "true") {
                reflection->SetBool(&message, descriptor, true);
            } else {
                reflection->SetBool(&message, descriptor, false);
            }
            break;
        default:
            printf("no support type = %d.\n", descriptor->type());
            break;

    }
}

Storage::Storage(const std::string &host, const std::string &database,
                 const std::string &user, const std::string &passwd,
                 uint32_t port) :
        mysql_(NULL),
        host_(host),
        database_(database),
        port_(port),
        user_(user),
        passwd_(passwd) {}

Storage::~Storage() {
    if (mysql_ != NULL) {
        mysql_close(mysql_);
        mysql_ = NULL;
    }
}

void Storage::load(const std::string &type, const std::string &query, std::vector<std::string> &results) {
    ::google::protobuf::Message *message = createMessage(type);
    if(message == NULL) {
        printf("createMessage(%s) failed.\n", type.c_str());
        return;
    }

    message->ParseFromString(query);

    std::vector< ::google::protobuf::Message *> message_results;
    load(*message, message_results);

    for (size_t i = 0; i < message_results.size(); ++i) {
        results.push_back(message_results[i]->SerializeAsString());
        delete message_results[i];
    }

    delete message;
}

void Storage::load(const ::google::protobuf::Message &query, std::vector< ::google::protobuf::Message *> &results) {
    connect();

    const ::google::protobuf::Reflection *reflection = query.GetReflection();
    const ::google::protobuf::Descriptor *descriptor = query.GetDescriptor();

    bool first_field = true;
    std::ostringstream oss;
    oss << "SELECT * FROM `" << query.GetTypeName() << "` WHERE";

    for (size_t i = 0; i < descriptor->field_count(); ++i) {
        const ::google::protobuf::FieldDescriptor *field_descriptor = descriptor->field(i);
        if (reflection->HasField(query, field_descriptor) == false) {
            continue;
        }

        if (first_field == false) {
            oss << " AND ";
        } else {
            first_field = false;
        }

        switch(field_descriptor->type()) {
            case ::google::protobuf::FieldDescriptor::TYPE_FIXED64:
            case ::google::protobuf::FieldDescriptor::TYPE_INT64:
                oss << " `" << field_descriptor->name() << "`=" << reflection->GetInt64(query, field_descriptor);
                break;
            case ::google::protobuf::FieldDescriptor::TYPE_UINT64:
                oss << " `" << field_descriptor->name() << "`=" << reflection->GetUInt64(query, field_descriptor);
                break;
            case ::google::protobuf::FieldDescriptor::TYPE_FIXED32:
            case ::google::protobuf::FieldDescriptor::TYPE_INT32:
                oss << " `" << field_descriptor->name() << "`=" << reflection->GetInt32(query, field_descriptor);
                break;
            case ::google::protobuf::FieldDescriptor::TYPE_UINT32:
                oss << " `" << field_descriptor->name() << "`=" << reflection->GetUInt32(query, field_descriptor);
                break;
            case ::google::protobuf::FieldDescriptor::TYPE_STRING:
                oss << " `" << field_descriptor->name() << "`=\"" <<
                    mysqlEscape(reflection->GetString(query, field_descriptor)) << "\"";
                break;
            case ::google::protobuf::FieldDescriptor::TYPE_DOUBLE:
                oss << " `" << field_descriptor->name() << "`=" << reflection->GetDouble(query, field_descriptor);
                break;
            case ::google::protobuf::FieldDescriptor::TYPE_FLOAT:
                oss << " `" << field_descriptor->name() << "`=" << reflection->GetFloat(query, field_descriptor);
                break;
            case ::google::protobuf::FieldDescriptor::TYPE_BOOL:
                oss << " `" << field_descriptor->name() << "`=" <<
                    reflection->GetBool(query, field_descriptor) ? "true" : "false";
                break;
            default:
                printf("Not support name(%s).\n", field_descriptor->name().c_str());
                return;
        }
    }

    execute(oss.str());
    ::MYSQL_RES *res = ::mysql_store_result(mysql_);
    if (res == NULL) {
        printf("mysql_store_result failed.\n");
        return;
    }

    ::MYSQL_FIELD *fields = ::mysql_fetch_fields(res);
    uint32_t field_num = ::mysql_num_fields(res);

    ::MYSQL_ROW row;
    while (row = ::mysql_fetch_row(res)) {
        std::map<std::string, std::string> values;
        for (int i = 0; i < field_num; ++i) {
            values[fields[i].name] = row[i];
        }

        ::google::protobuf::Message *message = createMessage(query.GetTypeName());
        if (message == NULL) {
            printf("createMessage(%s) failed.\n", query.GetTypeName().c_str());
            return;
        }
        const google::protobuf::Reflection *reflection = message->GetReflection();
        const google::protobuf::Descriptor *descriptor = message->GetDescriptor();
        for(size_t i = 0; i < descriptor->field_count(); ++i) {
            reflectionFill(*message, descriptor->field(i), reflection, values);
        }

        results.push_back(message);
    }

    ::mysql_free_result(res);
    disconnect();
}

void Storage::save(const std::string &type, const std::string &data) {
    ::google::protobuf::Message *message = createMessage(type);
    if(message == NULL) {
        printf("createMessage(%s) failed.\n", type.c_str());
        return;
    }

    message->ParseFromString(data);
    save(*message);

    delete message;
}

void Storage::save(const google::protobuf::Message &message) {
    connect();

    const ::google::protobuf::Reflection *reflection = message.GetReflection();
    const ::google::protobuf::Descriptor *descriptor = message.GetDescriptor();

    bool first_field = true;
    std::ostringstream oss;
    // UPDATE `table` SET `field1`="value1", `field2`=value2;
    oss << "REPLACE INTO `" << message.GetTypeName() << "` SET";

    for (size_t i = 0; i < descriptor->field_count(); ++i) {
        const ::google::protobuf::FieldDescriptor *field_descriptor = descriptor->field(i);
        if (reflection->HasField(message, field_descriptor) == false) {
            continue;
        }

        if (first_field == false) {
            oss << ",";
        } else {
            first_field = false;
        }

        switch(field_descriptor->type()) {
            case ::google::protobuf::FieldDescriptor::TYPE_FIXED64:
            case ::google::protobuf::FieldDescriptor::TYPE_INT64:
                oss << " `" << field_descriptor->name() << "`=" << reflection->GetInt64(message, field_descriptor);
                break;
            case ::google::protobuf::FieldDescriptor::TYPE_UINT64:
                oss << " `" << field_descriptor->name() << "`=" << reflection->GetUInt64(message, field_descriptor);
                break;
            case ::google::protobuf::FieldDescriptor::TYPE_FIXED32:
            case ::google::protobuf::FieldDescriptor::TYPE_INT32:
                oss << " `" << field_descriptor->name() << "`=" << reflection->GetInt32(message, field_descriptor);
                break;
            case ::google::protobuf::FieldDescriptor::TYPE_UINT32:
                oss << " `" << field_descriptor->name() << "`=" << reflection->GetUInt32(message, field_descriptor);
                break;
            case ::google::protobuf::FieldDescriptor::TYPE_STRING:
                oss << " `" << field_descriptor->name() << "`=\"" <<
                    mysqlEscape(reflection->GetString(message, field_descriptor)) << "\"";
                break;
            case ::google::protobuf::FieldDescriptor::TYPE_DOUBLE:
                oss << " `" << field_descriptor->name() << "`=" << reflection->GetDouble(message, field_descriptor);
                break;
            case ::google::protobuf::FieldDescriptor::TYPE_FLOAT:
                oss << " `" << field_descriptor->name() << "`=" << reflection->GetFloat(message, field_descriptor);
                break;
            case ::google::protobuf::FieldDescriptor::TYPE_BOOL:
                oss << " `" << field_descriptor->name() << "`=" <<
                    reflection->GetBool(message, field_descriptor) ? "true" : "false";
                break;
            default:
                printf("Not support name(%s).\n", field_descriptor->name().c_str());
                return;
        }
    }

    execute(oss.str());
    disconnect();
}

std::string Storage::mysqlEscape(const std::string &str) {
    connect();

    std::string temp_str;
    temp_str.resize(str.size() * 2 + 1);
    ::mysql_real_escape_string(mysql_, &temp_str[0], str.data(), str.size());
    temp_str.resize(strlen(temp_str.c_str()));

    return temp_str;
}

bool Storage::connect() {
    if (mysql_ != NULL) {
        return true;
    }

    mysql_ = mysql_init(NULL);
    if (mysql_ == NULL) {
        return false;
    }

    if (mysql_real_connect(mysql_, host_.c_str(), user_.c_str(),
            passwd_.c_str(), database_.c_str(), port_, NULL, 0) == NULL) {
        mysql_ = NULL;
        std::cout << host_ << user_ << passwd_ << database_ << port_ << std::endl;
        return false;
    }

    return true;
}

void Storage::disconnect() {
    if (mysql_ == NULL) {
        return;
    }

    ::mysql_close(mysql_);
    mysql_ = NULL;
    mysql_ = NULL;
}

bool Storage::execute(const std::string &sql) {
    if (connect() == false || mysql_ == NULL) {
        std::cout << "mysql connect failed." << std::endl;
        return false;
    }

    int ret = ::mysql_real_query(mysql_, sql.c_str(), sql.length());
    if (ret != 0) {
        std::cout << "mysql query error: " << ::mysql_error(mysql_) << ", sql: " << sql << std::endl;
        disconnect();
        return false;
    }

    std::cout << sql << std::endl;

    return true;
}

}  // namespace pmo
