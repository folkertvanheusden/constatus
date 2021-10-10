// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include "config.h"
#if HAVE_MYSQL == 1
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#endif
#if HAVE_MYSQL_RPM == 1
#include <mysql-cppconn-8/jdbc/cppconn/driver.h>
#include <mysql-cppconn-8/jdbc/cppconn/exception.h>
#include <mysql-cppconn-8/jdbc/cppconn/resultset.h>
#include <mysql-cppconn-8/jdbc/cppconn/statement.h>
#include <mysql-cppconn-8/jdbc/cppconn/prepared_statement.h>
#endif
#include <mutex>
#include <string>
#include <vector>
#include "config.h"

typedef enum { EVT_MOTION, EVT_GENERIC } event_t;

typedef enum { DE_VIEW = 1, DE_SOURCE, DE_PLACEHOLDER } de_t;

typedef struct
{
	unsigned long int nr;
	de_t type;
	std::string id;
} dashboard_entry_t;

class db
{
private:
	std::mutex lock;
	std::string url, username, password;

#if HAVE_MYSQL == 1 || HAVE_MYSQL_RPM == 1
	sql::Driver *driver { nullptr };
	sql::Connection *con { nullptr };
#endif

	bool check_table_exists(const std::string & table);
#if HAVE_MYSQL == 1 || HAVE_MYSQL_RPM == 1
	void log_sql_exception(const std::string query, const sql::SQLException & e);
#endif

	void reconnect();

public:
	db(const std::string & url, const std::string & username, const std::string & password);
	~db();

#if HAVE_MYSQL == 1 || HAVE_MYSQL_RPM == 1
	bool using_db() const { return con != nullptr; }
#else
	bool using_db() const { return false; }
#endif

	unsigned long register_event(const std::string & id, const int type, const std::string & parameter);
	void register_event_end(const unsigned long event_nr);

	void register_file(const std::string & id, const unsigned long event_nr, const std::string & file);

	std::vector<std::string> purge(const int max_age);

	// returns a list of dates in yyyy-mm-dd format
	std::vector<std::string> list_days_with_data();
	// unsigned long is file_nr
	std::vector<std::pair<unsigned long, std::string> > list_files_for_a_date(const std::string & date);

	std::string retrieve_filename(const unsigned long file_nr);

	std::vector<std::string> get_dashboards(const std::string & user);
	std::vector<dashboard_entry_t> get_dashboard_ids(const std::string & user, const std::string & dashboard_name);
	int add_id_to_dashboard(const std::string & user, const std::string & dashboard_name, const de_t type, const std::string & id); // returns an id_nr
	void del_id_from_dashboard(const std::string & user, const std::string & dashboard_name, const int id_nr);
	void remove_dashboard(const std::string & user, const std::string & dashboard_name);
};
