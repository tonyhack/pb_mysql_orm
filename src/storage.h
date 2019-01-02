#ifndef PMO_STORAGE_H
#define PMO_STORAGE_H

#include <map>
#include <string>
#include <vector>

#include <mysql.h>
#include <stddef.h>
#include <stdint.h>

namespace google {
namespace protobuf {

class Message;

}  // namespace protobuf
}  // namespace google

namespace pmo {

class Storage {
public:
    Storage(const std::string &host, const std::string &database,
            const std::string &user, const std::string &passwd,
            uint32_t port = 3306);
    virtual ~Storage();

    void load(const std::string &type, const std::string &query, std::vector<std::string> &results);
    void load(const google::protobuf::Message &query, std::vector<google::protobuf::Message *> &results);

    void save(const std::string &type, const std::string &data);
    void save(const google::protobuf::Message &message);

private:
    std::string mysqlEscape(const std::string &str);

    bool connect();
    void disconnect();
    bool execute(const std::string &sql);

    MYSQL *mysql_;

    std::string host_;
    std::string database_;
    uint32_t port_;
    std::string user_;
    std::string passwd_;
};

}   // namespace pmo

#endif  // PMO_STORAGE_H
