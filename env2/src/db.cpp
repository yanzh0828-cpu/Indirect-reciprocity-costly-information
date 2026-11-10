#include "db.h"

#include <mysql/mysql.h>

#include <cmath>
#include <cstdio>
#include <string>

namespace {

// β 专用表路由：μ 程序按选择强度 β 决定写入哪套表。
//   β≈0.0003（当前新参数）→ 独立新表 *_b0003_env2（旧 β=0.001 历史数据表保持不动）
//   其它 β（含历史 0.001）→ 无 _b0003 旧表，保持原有行为。
const char* kDetailB0003  = "single_run_result_b0003_env2";
const char* kAvgB0003     = "averaged_result_b0003_env2";
const char* kDetailLegacy = "single_run_result_env2";
const char* kAvgLegacy    = "averaged_result_env2";

inline bool is_beta_0003(const Params& p) {
    return std::fabs(p.beta - 0.0003) < 1e-12;
}
inline const char* detail_tbl(const Params& p) {
    return is_beta_0003(p) ? kDetailB0003 : kDetailLegacy;
}
inline const char* avg_tbl(const Params& p) {
    return is_beta_0003(p) ? kAvgB0003 : kAvgLegacy;
}

} // namespace

DbWriter::~DbWriter() { disconnect(); }

void DbWriter::disconnect() {
    if (conn_) {
        mysql_close(static_cast<MYSQL*>(conn_));
        conn_ = nullptr;
    }
}

bool DbWriter::connect(const std::string& host, int port,
                       const std::string& user, const std::string& pass,
                       const std::string& db) {
    disconnect();
    MYSQL* m = mysql_init(nullptr);
    if (!m) {
        err_ = "mysql_init failed";
        return false;
    }
    if (!mysql_real_connect(m, host.c_str(), user.c_str(), pass.c_str(),
                            db.c_str(), static_cast<unsigned int>(port),
                            nullptr, 0)) {
        err_ = mysql_error(m);
        mysql_close(m);
        return false;
    }
    conn_ = m;
    db_name_ = db;
    return true;
}

bool DbWriter::query(const char* sql) {
    if (!conn_) { err_ = "not connected"; return false; }
    if (mysql_query(static_cast<MYSQL*>(conn_), sql) != 0) {
        err_ = mysql_error(static_cast<MYSQL*>(conn_));
        return false;
    }
    return true;
}

bool DbWriter::ensure_schema() {
    const char* t1 =
        "CREATE TABLE IF NOT EXISTS single_run_result_env2 ("
        " id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,"
        " param_key VARCHAR(255) NOT NULL,"
        " N INT UNSIGNED NOT NULL,"
        " b DOUBLE NOT NULL, c DOUBLE NOT NULL, q DOUBLE NOT NULL, dq DOUBLE NOT NULL,"
        " p DOUBLE NOT NULL, beta DOUBLE NOT NULL, mu DOUBLE NOT NULL, w DOUBLE NOT NULL,"
        " rounds INT UNSIGNED NOT NULL,"
        " burn_in BIGINT UNSIGNED NOT NULL, measure BIGINT UNSIGNED NOT NULL,"
        " seed BIGINT UNSIGNED NOT NULL,"
        " thread_id INT UNSIGNED NOT NULL DEFAULT 0,"
        " abund_ALLD_1 DOUBLE NOT NULL, abund_ALLD_2 DOUBLE NOT NULL,"
        " abund_DIS DOUBLE NOT NULL, abund_DIS_TILDE DOUBLE NOT NULL,"
        " created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        " UNIQUE KEY uq_param_seed (param_key, seed),"
        " KEY idx_mu (mu)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4";

    const char* t2 =
        "CREATE TABLE IF NOT EXISTS averaged_result_env2 ("
        " id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,"
        " param_key VARCHAR(255) NOT NULL,"
        " N INT UNSIGNED NOT NULL,"
        " b DOUBLE NOT NULL, c DOUBLE NOT NULL, q DOUBLE NOT NULL, dq DOUBLE NOT NULL,"
        " p DOUBLE NOT NULL, beta DOUBLE NOT NULL, mu DOUBLE NOT NULL, w DOUBLE NOT NULL,"
        " rounds INT UNSIGNED NOT NULL,"
        " burn_in BIGINT UNSIGNED NOT NULL, measure BIGINT UNSIGNED NOT NULL,"
        " num_seeds INT UNSIGNED NOT NULL,"
        " mean_ALLD_1 DOUBLE NOT NULL, mean_ALLD_2 DOUBLE NOT NULL,"
        " mean_DIS DOUBLE NOT NULL, mean_DIS_TILDE DOUBLE NOT NULL,"
        " se_ALLD_1 DOUBLE NOT NULL, se_ALLD_2 DOUBLE NOT NULL,"
        " se_DIS DOUBLE NOT NULL, se_DIS_TILDE DOUBLE NOT NULL,"
        " created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        " UNIQUE KEY uq_param (param_key)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4";

    // 旧行为（确保旧表存在）+ β=0.0003 专用新表（结构复制自旧表，旧表数据不动）
    const bool ok_legacy = query(t1) && query(t2);
    const bool ok_b0003 =
        query("CREATE TABLE IF NOT EXISTS single_run_result_b0003_env2 LIKE single_run_result_env2") &&
        query("CREATE TABLE IF NOT EXISTS averaged_result_b0003_env2 LIKE averaged_result_env2");
    return ok_legacy && ok_b0003;
}

bool DbWriter::insert_run(const RunResult& r, const Params& p) {
    char key[512];
    std::snprintf(key, sizeof(key),
        "env=2;N=%d;b=%.10g;c=%.10g;q=%.10g;dq=%.10g;p=%.10g;beta=%.10g;mu=%.10g;T=%d",
        p.N, p.b, p.c, p.q, p.dq, p.p, p.beta, r.mu, p.rounds);

    char sql[2048];
    std::snprintf(sql, sizeof(sql),
        "INSERT INTO %s "
        "(param_key,N,b,c,q,dq,p,beta,mu,w,rounds,burn_in,measure,seed,"
        " abund_ALLD_1,abund_ALLD_2,abund_DIS,abund_DIS_TILDE) VALUES ("
        "'%s',%d,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%d,%llu,%llu,%llu,"
        "%.17g,%.17g,%.17g,%.17g) "
        "ON DUPLICATE KEY UPDATE "
        "abund_ALLD_1=VALUES(abund_ALLD_1),abund_ALLD_2=VALUES(abund_ALLD_2),"
        "abund_DIS=VALUES(abund_DIS),abund_DIS_TILDE=VALUES(abund_DIS_TILDE)",
        detail_tbl(p), key, p.N, p.b, p.c, p.q, p.dq, p.p, p.beta, r.mu, p.w, p.rounds,
        static_cast<unsigned long long>(r.burn_in),
        static_cast<unsigned long long>(r.measure),
        static_cast<unsigned long long>(r.seed),
        r.abund[0], r.abund[1], r.abund[2], r.abund[3]);

    return query(sql);
}

bool DbWriter::aggregate(const Params& p) {
    char sql[4096];
    std::snprintf(sql, sizeof(sql),
        "INSERT INTO %s "
        "(param_key,N,b,c,q,dq,p,beta,mu,w,rounds,burn_in,measure,num_seeds,"
        " mean_ALLD_1,mean_ALLD_2,mean_DIS,mean_DIS_TILDE,"
        " se_ALLD_1,se_ALLD_2,se_DIS,se_DIS_TILDE) "
        "SELECT param_key,N,b,c,q,dq,p,beta,mu,w,rounds,burn_in,measure,COUNT(*),"
        " AVG(abund_ALLD_1),AVG(abund_ALLD_2),AVG(abund_DIS),AVG(abund_DIS_TILDE),"
        " IF(COUNT(*)>1, STDDEV_SAMP(abund_ALLD_1)/SQRT(COUNT(*)), 0.0),"
        " IF(COUNT(*)>1, STDDEV_SAMP(abund_ALLD_2)/SQRT(COUNT(*)), 0.0),"
        " IF(COUNT(*)>1, STDDEV_SAMP(abund_DIS)/SQRT(COUNT(*)), 0.0),"
        " IF(COUNT(*)>1, STDDEV_SAMP(abund_DIS_TILDE)/SQRT(COUNT(*)), 0.0) "
        "FROM %s "
        "GROUP BY param_key,N,b,c,q,dq,p,beta,mu,w,rounds,burn_in,measure "
        "ON DUPLICATE KEY UPDATE "
        " num_seeds=VALUES(num_seeds),"
        " mean_ALLD_1=VALUES(mean_ALLD_1), mean_ALLD_2=VALUES(mean_ALLD_2),"
        " mean_DIS=VALUES(mean_DIS), mean_DIS_TILDE=VALUES(mean_DIS_TILDE),"
        " se_ALLD_1=VALUES(se_ALLD_1), se_ALLD_2=VALUES(se_ALLD_2),"
        " se_DIS=VALUES(se_DIS), se_DIS_TILDE=VALUES(se_DIS_TILDE)",
        avg_tbl(p), detail_tbl(p));

    return query(sql);
}
