protoc --cpp_out=. pb_orm_test.proto
protoc --mysql_out=. pb_orm_test.proto
g++ -g storage.cc main.cc pb_orm_test.pb.cc -lmysqlclient -lprotobuf -L/home/taohan/open_src/protobuf/protobuf-2.6.1/src/.libs -L/usr/local/mysql/lib
