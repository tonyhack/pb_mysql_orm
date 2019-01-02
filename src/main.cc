#include <iostream>

#include "storage.h"
#include "pb_orm_test.pb.h"

int main() {
    pmo::Storage storage("192.168.30.51", "orm_test", "mttd", "mttd2014");

    pmo::tutorial::PbOrmTest pot;
    pot.set_id(1);
    pot.set_name("pot1");
    pot.set_type(1);
    pot.set_value1(99);
    pot.set_value2("port_v1");
    storage.save(pot);

    pot.set_id(2);
    pot.set_name("pot2");
    pot.set_type(3);
    pot.set_value1(999);
    pot.set_value2("port_v2");
    storage.save(pot.GetTypeName(), pot.SerializeAsString());

    std::vector< ::google::protobuf::Message *> pot_results;
    pmo::tutorial::PbOrmTest pot_query;
    pot_query.set_id(1);
    storage.load(pot_query, pot_results);
    for (size_t i = 0; i < pot_results.size(); ++i) {
        std::cout << "-----" << i << "-----" << std::endl;
        pot_results[i]->PrintDebugString();
    }

    std::vector<std::string> str_results;
    pot_query.set_id(2);
    storage.load(pot_query.GetTypeName(), pot_query.SerializeAsString(), str_results);
    for (size_t i = 0; i < str_results.size(); ++i) {
        std::cout << "-----" << i << "-----" << std::endl;
        pmo::tutorial::PbOrmTest pot_result;
        pot_result.ParseFromString(str_results[i]);
        pot_result.PrintDebugString();
    }

    return 0;
}
