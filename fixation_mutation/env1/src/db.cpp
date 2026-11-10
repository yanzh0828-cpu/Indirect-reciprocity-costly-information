#include "db.h"

#include <mysql/mysql.h>

#include <cstdio>
#include <string>

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
        "CREATE TABLE IF NOT EXISTS single_run_result_fix ("
        " id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,"
        " param_key VARCHAR(255) NOT NULL,"
        " N INT UNSIGNED NOT NULL,"
        " b DOUBLE NOT NULL, c DOUBLE NOT NULL, q DOUBLE NOT NULL, dq DOUBLE NOT NULL,"
        " p DOUBLE NOT NULL, beta DOUBLE NOT NULL, w DOUBLE NOT NULL,"
        " rounds INT UNSIGNED NOT NULL,"
        " burn_in_fix BIGINT UNSIGNED NOT NULL, measure_fix BIGINT UNSIGNED NOT NULL,"
        " seed BIGINT UNSIGNED NOT NULL,"
        " fixations BIGINT UNSIGNED NOT NULL, generations BIGINT UNSIGNED NOT NULL,"
        " abund_ALLC DOUBLE NOT NULL, abund_ALLD DOUBLE NOT NULL,"
        " abund_DIS DOUBLE NOT NULL, abund_DIS_TILDE DOUBLE NOT NULL,"
        " fix_ALLC DOUBLE NOT NULL, fix_ALLD DOUBLE NOT NULL,"
        " fix_DIS DOUBLE NOT NULL, fix_DIS_TILDE DOUBLE NOT NULL,"
        " created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        " UNIQUE KEY uq_param_seed (param_key, seed),"
        " KEY idx_beta (beta)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4";

    const char* t2 =
        "CREATE TABLE IF NOT EXISTS averaged_result_fix ("
        " id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,"
        " param_key VARCHAR(255) NOT NULL,"
        " N INT UNSIGNED NOT NULL,"
        " b DOUBLE NOT NULL, c DOUBLE NOT NULL, q DOUBLE NOT NULL, dq DOUBLE NOT NULL,"
        " p DOUBLE NOT NULL, beta DOUBLE NOT NULL, w DOUBLE NOT NULL,"
        " rounds INT UNSIGNED NOT NULL,"
        " burn_in_fix BIGINT UNSIGNED NOT NULL, measure_fix BIGINT UNSIGNED NOT NULL,"
        " num_seeds INT UNSIGNED NOT NULL,"
        " mean_fixations DOUBLE NOT NULL, mean_generations DOUBLE NOT NULL,"
        " mean_ALLC DOUBLE NOT NULL, mean_ALLD DOUBLE NOT NULL,"
        " mean_DIS DOUBLE NOT NULL, mean_DIS_TILDE DOUBLE NOT NULL,"
        " se_ALLC DOUBLE NOT NULL, se_ALLD DOUBLE NOT NULL,"
        " se_DIS DOUBLE NOT NULL, se_DIS_TILDE DOUBLE NOT NULL,"
        " mean_fix_ALLC DOUBLE NOT NULL, mean_fix_ALLD DOUBLE NOT NULL,"
        " mean_fix_DIS DOUBLE NOT NULL, mean_fix_DIS_TILDE DOUBLE NOT NULL,"
        " se_fix_ALLC DOUBLE NOT NULL, se_fix_ALLD DOUBLE NOT NULL,"
        " se_fix_DIS DOUBLE NOT NULL, se_fix_DIS_TILDE DOUBLE NOT NULL,"
        " created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        " UNIQUE KEY uq_param (param_key)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4";

    return query(t1) && query(t2);
}

bool DbWriter::insert_run(const RunResult& r, const Params& p) {
    char key[512];
    std::snprintf(key, sizeof(key),
        "N=%d;b=%.10g;c=%.10g;q=%.10g;dq=%.10g;p=%.10g;beta=%.10g;T=%d",
        p.N, p.b, p.c, p.q, p.dq, p.p, p.beta, p.rounds);

    char sql[4096];
    std::snprintf(sql, sizeof(sql),
        "INSERT INTO single_run_result_fix "
        "(param_key,N,b,c,q,dq,p,beta,w,rounds,burn_in_fix,measure_fix,seed,"
        " fixations,generations,"
        " abund_ALLC,abund_ALLD,abund_DIS,abund_DIS_TILDE,"
        " fix_ALLC,fix_ALLD,fix_DIS,fix_DIS_TILDE) VALUES ("
        "'%s',%d,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%d,0,%llu,%llu,"
        "%llu,0,"
        "0,0,0,0,"
        "%.17g,%.17g,%.17g,%.17g) "
        "ON DUPLICATE KEY UPDATE "
        "fixations=VALUES(fixations),"
        "fix_ALLC=VALUES(fix_ALLC),fix_ALLD=VALUES(fix_ALLD),"
        "fix_DIS=VALUES(fix_DIS),fix_DIS_TILDE=VALUES(fix_DIS_TILDE)",
        key, p.N, p.b, p.c, p.q, p.dq, p.p, p.beta, p.w, p.rounds,
        static_cast<unsigned long long>(r.measure_fix),
        static_cast<unsigned long long>(r.seed),
        static_cast<unsigned long long>(r.fixations),
        r.fix_abund[0], r.fix_abund[1], r.fix_abund[2], r.fix_abund[3]);

    return query(sql);
}

bool DbWriter::aggregate(const Params& p) {
    (void)p;
    const char* sql =
        "INSERT INTO averaged_result_fix "
        "(param_key,N,b,c,q,dq,p,beta,w,rounds,burn_in_fix,measure_fix,num_seeds,"
        " mean_fixations,mean_generations,"
        " mean_ALLC,mean_ALLD,mean_DIS,mean_DIS_TILDE,"
        " se_ALLC,se_ALLD,se_DIS,se_DIS_TILDE,"
        " mean_fix_ALLC,mean_fix_ALLD,mean_fix_DIS,mean_fix_DIS_TILDE,"
        " se_fix_ALLC,se_fix_ALLD,se_fix_DIS,se_fix_DIS_TILDE) "
        "SELECT param_key,N,b,c,q,dq,p,beta,w,rounds,MIN(burn_in_fix),measure_fix,COUNT(*),"
        " AVG(fixations),AVG(generations),"
        " AVG(abund_ALLC),AVG(abund_ALLD),AVG(abund_DIS),AVG(abund_DIS_TILDE),"
        " IF(COUNT(*)>1, STDDEV_SAMP(abund_ALLC)/SQRT(COUNT(*)), 0.0),"
        " IF(COUNT(*)>1, STDDEV_SAMP(abund_ALLD)/SQRT(COUNT(*)), 0.0),"
        " IF(COUNT(*)>1, STDDEV_SAMP(abund_DIS)/SQRT(COUNT(*)), 0.0),"
        " IF(COUNT(*)>1, STDDEV_SAMP(abund_DIS_TILDE)/SQRT(COUNT(*)), 0.0),"
        " AVG(fix_ALLC),AVG(fix_ALLD),AVG(fix_DIS),AVG(fix_DIS_TILDE),"
        " IF(COUNT(*)>1, STDDEV_SAMP(fix_ALLC)/SQRT(COUNT(*)), 0.0),"
        " IF(COUNT(*)>1, STDDEV_SAMP(fix_ALLD)/SQRT(COUNT(*)), 0.0),"
        " IF(COUNT(*)>1, STDDEV_SAMP(fix_DIS)/SQRT(COUNT(*)), 0.0),"
        " IF(COUNT(*)>1, STDDEV_SAMP(fix_DIS_TILDE)/SQRT(COUNT(*)), 0.0) "
        "FROM single_run_result_fix "
        "GROUP BY param_key,N,b,c,q,dq,p,beta,w,rounds,measure_fix "
        "ON DUPLICATE KEY UPDATE "
        " num_seeds=VALUES(num_seeds),"
        " mean_fixations=VALUES(mean_fixations), mean_generations=VALUES(mean_generations),"
        " mean_ALLC=VALUES(mean_ALLC), mean_ALLD=VALUES(mean_ALLD),"
        " mean_DIS=VALUES(mean_DIS), mean_DIS_TILDE=VALUES(mean_DIS_TILDE),"
        " se_ALLC=VALUES(se_ALLC), se_ALLD=VALUES(se_ALLD),"
        " se_DIS=VALUES(se_DIS), se_DIS_TILDE=VALUES(se_DIS_TILDE),"
        " mean_fix_ALLC=VALUES(mean_fix_ALLC), mean_fix_ALLD=VALUES(mean_fix_ALLD),"
        " mean_fix_DIS=VALUES(mean_fix_DIS), mean_fix_DIS_TILDE=VALUES(mean_fix_DIS_TILDE),"
        " se_fix_ALLC=VALUES(se_fix_ALLC), se_fix_ALLD=VALUES(se_fix_ALLD),"
        " se_fix_DIS=VALUES(se_fix_DIS), se_fix_DIS_TILDE=VALUES(se_fix_DIS_TILDE)";

    return query(sql);
}
