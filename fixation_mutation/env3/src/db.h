#pragma once

#include "params.h"
#include "simulation.h"

#include <string>

// Thin RAII wrapper around the MySQL C API (used from the main thread only).
class DbWriter {
public:
    ~DbWriter();

    bool connect(const std::string& host, int port,
                 const std::string& user, const std::string& pass,
                 const std::string& db);
    void disconnect();

    bool ensure_schema();
    bool insert_run(const RunResult& r, const Params& p);
    bool aggregate(const Params& p); // single_run_result -> averaged_result

    bool connected() const { return conn_ != nullptr; }
    const std::string& last_error() const { return err_; }

private:
    bool query(const char* sql);

    void* conn_ = nullptr; // MYSQL*
    std::string err_;
    std::string db_name_;
};
