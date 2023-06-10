// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"
#include <string>
#include <vector>
#if HAVE_MYSQL == 1
#include <mysql_connection.h>
#endif
#if HAVE_MYSQL_RPM == 1
#include <mysql-cppconn-8/jdbc/mysql_connection.h>
#endif

#include "error.h"
#include "config.h"
#include "db.h"
#include "utils.h"
#include "log.h"

bool db::check_table_exists(const std::string & table)
{
#if HAVE_MYSQL == 1 || HAVE_MYSQL_RPM == 1
	sql::Statement *stmt = con->createStatement();
	sql::ResultSet *res = stmt->executeQuery(myformat("show tables like \"%s\"", table.c_str()).c_str());
	bool rc = res->next();
	delete res;
	delete stmt;

	return rc;
#endif
	return true;
}

#if HAVE_MYSQL == 1 || HAVE_MYSQL_RPM == 1
void db::log_sql_exception(const std::string query, const sql::SQLException & e)
{
	log(LL_ERR, "%s MySQL error: %s, error code: %d", query.c_str(), e.what(), e.getErrorCode());
}
#endif

void db::reconnect()
{
#if HAVE_MYSQL == 1 || HAVE_MYSQL_RPM == 1
	if (driver) {
		if (con) {
			try {
				sql::Statement *stmt = con->createStatement();
				sql::ResultSet *res = stmt->executeQuery("SELECT 1");
				if (res) {
					res->next();
					delete res;
				}
				delete stmt;
			}
			catch(sql::SQLException & e) {
				log_sql_exception("MySQL ping", e);

				delete con;
				con = nullptr;
			}
		}

		if (!con && !url.empty()) {
			try {
				sql::ConnectOptionsMap connection_properties;
				connection_properties["hostName"] = url;
				connection_properties["userName"] = username;
				connection_properties["password"] = password;

				con = driver->connect(url.c_str(), username.c_str(), password.c_str());

				bool myTrue = true;
				con->setClientOption("OPT_RECONNECT", &myTrue);
			}
			catch(sql::SQLException & e) {
				log_sql_exception("MySQL reconnect", e);

				delete con;
				con = nullptr;
			}
		}
	}
#endif
}

db::db(const std::string & url, const std::string & username, const std::string & password) : url(url), username(username), password(password)
{
#if HAVE_MYSQL == 1 || HAVE_MYSQL_RPM == 1
	if (url.empty()) {
		log(LL_INFO, "No database configured");
		return;
	}

	try {
		driver = get_driver_instance();

		reconnect();

		/// events
		if (!check_table_exists("events")) {
			sql::Statement *stmt = con->createStatement();
			stmt->execute("CREATE TABLE events(event_nr INTEGER PRIMARY KEY AUTO_INCREMENT, `id` text not null, ts_start datetime not null, ts_end datetime, type integer not null, parameter1 text)");
			delete stmt;
		}

		/// event-files
		if (!check_table_exists("event_files")) {
			sql::Statement *stmt = con->createStatement();
			stmt->execute("CREATE TABLE event_files(event_nr int not null, file_nr INTEGER PRIMARY KEY AUTO_INCREMENT, `id` text not null, ts datetime not null, file text not null, FOREIGN KEY (event_nr) REFERENCES events(event_nr) ON DELETE CASCADE)");
			delete stmt;
		}

		/// dashboards
		if (!check_table_exists("dashboards2")) {
			sql::Statement *stmt = con->createStatement();
			stmt->execute("CREATE TABLE dashboards2(name VARCHAR(128) NOT NULL, source_nr INTEGER PRIMARY KEY AUTO_INCREMENT, type TEXT NOT NULL, `id` TEXT NOT NULL, user TEXT NOT NULL, index i_name(name))");
			delete stmt;
		}

		/// cookies
		if (!check_table_exists("cookies")) {
			sql::Statement *stmt = con->createStatement();
			stmt->execute("CREATE TABLE cookies(`key` varchar(255) NOT NULL PRIMARY KEY, user VARCHAR(255) NOT NULL, since DATETIME NOT NULL, CONSTRAINT user_c UNIQUE (user))");
			delete stmt;
		}
	}
	catch(sql::SQLException & e) {
		log_sql_exception("(create tables)", e);
	}
#endif
}

db::~db()
{
#if HAVE_MYSQL == 1 || HAVE_MYSQL_RPM == 1
	delete con;
#endif
}

unsigned long db::register_event(const std::string & id, const int type, const std::string & parameter)
{
#if HAVE_MYSQL == 1 || HAVE_MYSQL_RPM == 1
	if (!driver)
		return 0;

	const std::lock_guard<std::mutex> lck(lock);

	reconnect();

	unsigned long rec_id = 0;
	sql::PreparedStatement *stmt { nullptr };
	sql::Statement *stmt2 { nullptr };
	sql::ResultSet *res { nullptr };

	try {
		stmt = con->prepareStatement("INSERT INTO events(`id`, ts_start, type, parameter1) VALUES(?, NOW(), ?, ?)");

		stmt->setString(1, id.c_str());
		stmt->setInt(2, type);
		stmt->setString(3, parameter.c_str());
		stmt->execute();

		stmt2 = con->createStatement();
		res = stmt2->executeQuery("SELECT LAST_INSERT_ID() AS `id`");
		res->next();
		rec_id = res->getInt(1);
	}
	catch(sql::SQLException & e) {
		log_sql_exception("(register_event)", e);
	}

	delete stmt;
	delete res;
	delete stmt2;

	return rec_id;
#else
	return 0;
#endif
}

void db::register_event_end(const unsigned long event_nr)
{
#if HAVE_MYSQL == 1 || HAVE_MYSQL_RPM == 1
	if (!driver)
		return;

	const std::lock_guard<std::mutex> lck(lock);

	reconnect();

	sql::PreparedStatement *stmt { nullptr };

	try {
		stmt = con->prepareStatement("UPDATE events SET ts_end=NOW() WHERE event_nr=?");

		stmt->setInt(1, event_nr);
		stmt->execute();
	}
	catch(sql::SQLException & e) {
		log_sql_exception("(event-end)", e);
	}

	delete stmt;
#endif
}

void db::register_file(const std::string & id, const unsigned long event_nr, const std::string & file)
{
#if HAVE_MYSQL == 1 || HAVE_MYSQL_RPM == 1
	if (!driver)
		return;

	const std::lock_guard<std::mutex> lck(lock);

	reconnect();

	sql::PreparedStatement *stmt { nullptr };

	try {
		stmt = con->prepareStatement("INSERT INTO event_files(event_nr, `id`, ts, file) VALUES(?, ?, NOW(), ?)");

		stmt->setInt(1, event_nr);
		stmt->setString(2, id.c_str());
		stmt->setString(3, file.c_str());
		stmt->execute();
	}
	catch(sql::SQLException & e) {
		log_sql_exception("(reg file)", e);
	}

	delete stmt;
#endif
}

std::vector<std::string> db::purge(const int max_age)
{
	std::vector<std::string> files;

#if HAVE_MYSQL == 1 || HAVE_MYSQL_RPM == 1
	if (!driver)
		return files;

	const std::lock_guard<std::mutex> lck(lock);

	reconnect();

	sql::PreparedStatement *stmt { nullptr }, *stmt2 { nullptr };
	sql::ResultSet *res { nullptr };

	try {
		std::string query_get = "SELECT file_nr, file FROM event_files WHERE TIME_TO_SEC(TIMEDIFF(NOW(), ts)) >= ? AND NOT ts is NULL";

		stmt = con->prepareStatement(query_get);
		stmt->setInt(1, max_age * 86400);
		res = stmt->executeQuery();

		std::vector<unsigned long> nrs;
		while(res->next()) {
			files.push_back(res->getString("file"));
			nrs.push_back(res->getInt("file_nr"));
		}

		std::string query_del = "DELETE FROM event_files WHERE file_nr=? LIMIT 1";

		stmt2 = con->prepareStatement(query_del);

		for(auto nr : nrs) {
			stmt2->setInt(1, nr);
			stmt2->execute();
		}
	}
	catch(sql::SQLException & e) {
		log_sql_exception("(purge)", e);
	}

	delete stmt2;
	delete res;
	delete stmt;
#endif

	return files;
}

std::vector<std::string> db::list_days_with_data()
{
	std::vector<std::string> out;

#if HAVE_MYSQL == 1 || HAVE_MYSQL_RPM == 1
	if (!driver)
		return out;

	const std::lock_guard<std::mutex> lck(lock);

	reconnect();

	sql::Statement *stmt { nullptr };
	sql::ResultSet *res { nullptr };

	try {
		std::string query = "SELECT DISTINCT DATE(ts) AS date FROM event_files";

		stmt = con->createStatement();
		res = stmt->executeQuery(query);

		while(res->next())
			out.push_back(res->getString("date"));
	}
	catch(sql::SQLException & e) {
		log_sql_exception("(days with data)", e);
	}

	delete res;
	delete stmt;
#endif

	return out;
}

std::vector<std::pair<unsigned long, std::string> > db::list_files_for_a_date(const std::string & date)
{
	std::vector<std::pair<unsigned long, std::string> > out;

#if HAVE_MYSQL == 1 || HAVE_MYSQL_RPM == 1
	if (!driver)
		return out;

	const std::lock_guard<std::mutex> lck(lock);

	reconnect();

	sql::PreparedStatement *stmt { nullptr };
	sql::ResultSet *res { nullptr };

	try {
		std::string query = "SELECT file_nr, file FROM event_files WHERE DATE(ts)=? ORDER BY ts ASC";
		stmt = con->prepareStatement(query);
		stmt->setString(1, date.c_str());

		res = stmt->executeQuery();

		while(res->next())
			out.push_back(std::pair<unsigned long, std::string>(res->getInt("file_nr"), res->getString("file")));
	}
	catch(sql::SQLException & e) {
		log_sql_exception("(files for a date)", e);
	}

	delete res;
	delete stmt;
#endif

	return out;
}

std::string db::retrieve_filename(const unsigned long file_nr)
{
#if HAVE_MYSQL == 1 || HAVE_MYSQL_RPM == 1
	if (!driver)
		return "";

	const std::lock_guard<std::mutex> lck(lock);

	reconnect();

	std::string rc;

	sql::PreparedStatement *stmt { nullptr };
	sql::ResultSet *res { nullptr };

	try {
		std::string query = "SELECT file FROM event_files WHERE file_nr=? ORDER BY ts ASC";
		stmt = con->prepareStatement(query);
		stmt->setInt(1, file_nr);

		res = stmt->executeQuery();

		rc = res->next() ? res->getString("file") : "";
	}
	catch(sql::SQLException & e) {
		log_sql_exception("(retr filename)", e);
	}

	delete res;
	delete stmt;

	return rc;
#else
	return "";
#endif
}

std::vector<std::string> db::get_dashboards(const std::string & user)
{
	std::vector<std::string> out;

#if HAVE_MYSQL == 1 || HAVE_MYSQL_RPM == 1
	if (!driver)
		return out;

	const std::lock_guard<std::mutex> lck(lock);

	reconnect();

	std::string rc;

	sql::PreparedStatement *stmt { nullptr };
	sql::ResultSet *res { nullptr };

	try {
		std::string query = "SELECT DISTINCT name AS name FROM dashboards2 WHERE user=? ORDER BY name ASC";
		stmt = con->prepareStatement(query);
		stmt->setString(1, user);
		res = stmt->executeQuery();

		while(res->next())
			out.push_back(res->getString("name"));
	}
	catch(sql::SQLException & e) {
		log_sql_exception("(dashboard names)", e);
	}

	delete res;
	delete stmt;
#endif
	return out;
}

std::vector<dashboard_entry_t> db::get_dashboard_ids(const std::string & user, const std::string & dashboard_name)
{
	std::vector<dashboard_entry_t> out;

#if HAVE_MYSQL == 1 || HAVE_MYSQL_RPM == 1
	if (!driver)
		return out;

	const std::lock_guard<std::mutex> lck(lock);

	reconnect();

	std::string rc;

	sql::PreparedStatement *stmt { nullptr };
	sql::ResultSet *res { nullptr };

	try {
		std::string query = "SELECT source_nr, type, `id` FROM dashboards2 WHERE user=? AND name=? ORDER BY source_nr ASC";
		stmt = con->prepareStatement(query);
		stmt->setString(1, user);
		stmt->setString(2, dashboard_name);
		res = stmt->executeQuery();

		while(res->next()) {
			dashboard_entry_t de { 0 };
			de.nr = res->getInt("source_nr");

			std::string type = res->getString("type");
			if (type == "source")
				de.type = DE_SOURCE;
			else if (type == "view")
				de.type = DE_VIEW;
			else if (type == "placeholder")
				de.type = DE_PLACEHOLDER;

			de.id = res->getString("id");

			out.push_back(de);
		}
	}
	catch(sql::SQLException & e) {
		log_sql_exception("(dashboard ids)", e);
	}

	delete res;
	delete stmt;
#endif
	return out;
}

int db::add_id_to_dashboard(const std::string & user, const std::string & dashboard_name, const de_t type, const std::string & id)
{
	int rec_id = -1;

#if HAVE_MYSQL == 1 || HAVE_MYSQL_RPM == 1
	if (!driver)
		return rec_id;

	const std::lock_guard<std::mutex> lck(lock);

	reconnect();

	sql::PreparedStatement *stmt { nullptr }, *stmt3 { nullptr };
	sql::Statement *stmt2 { nullptr };
	sql::ResultSet *res { nullptr }, *res3 { nullptr };

	try {
		stmt3 = con->prepareStatement("SELECT source_nr FROM dashboards2 WHERE user=? AND `id`=? AND name=? LIMIT 1");
		stmt3->setString(1, user);
		stmt3->setString(2, id);
		stmt3->setString(3, dashboard_name);
		res3 = stmt3->executeQuery();

		if (res3->next())
			rec_id = res3->getInt("source_nr");
		else {
			stmt = con->prepareStatement("INSERT INTO dashboards2(type, `id`, user, name) VALUES(?, ?, ?, ?)");

			std::string type_str = "---";
			if (type == DE_SOURCE)
				type_str = "source";
			else if (type == DE_VIEW)
				type_str = "view";
			else if (type == DE_PLACEHOLDER)
				type_str = "placeholder";

			stmt->setString(1, type_str);

			stmt->setString(2, id);
			stmt->setString(3, user);
			stmt->setString(4, dashboard_name);
			stmt->execute();

			stmt2 = con->createStatement();
			res = stmt2->executeQuery("SELECT LAST_INSERT_ID() AS `source_nr`");
			res->next();
			rec_id = res->getInt(1);
		}
	}
	catch(sql::SQLException & e) {
		log_sql_exception("(add to dashboard)", e);
	}

	delete res;
	delete stmt2;
	delete stmt;
	delete res3;
	delete stmt3;
#endif

	return rec_id;
}

void db::del_id_from_dashboard(const std::string & user, const std::string & dashboard_name, const int id_nr)
{
#if HAVE_MYSQL == 1 || HAVE_MYSQL_RPM == 1
	if (!driver)
		return;

	const std::lock_guard<std::mutex> lck(lock);

	reconnect();

	sql::PreparedStatement *stmt { nullptr };

	try {
		std::string query_del = "DELETE FROM dashboards2 WHERE source_nr=? AND user=? AND name=? LIMIT 1";

                stmt = con->prepareStatement(query_del);
		stmt->setInt(1, id_nr);
		stmt->setString(2, user);
		stmt->setString(3, dashboard_name);
		stmt->execute();
	}
	catch(sql::SQLException & e) {
		log_sql_exception("(del from dashboard)", e);
	}

	delete stmt;
#endif
}

void db::remove_dashboard(const std::string & user, const std::string & dashboard_name)
{
#if HAVE_MYSQL == 1 || HAVE_MYSQL_RPM == 1
	if (!driver)
		return;

	const std::lock_guard<std::mutex> lck(lock);

	reconnect();

	sql::PreparedStatement *stmt { nullptr };

	try {
		std::string query_del = "DELETE FROM dashboards2 WHERE user=? AND name=?";

                stmt = con->prepareStatement(query_del);
		stmt->setString(1, user);
		stmt->setString(2, dashboard_name);
		stmt->execute();
	}
	catch(sql::SQLException & e) {
		log_sql_exception("(del dashboard)", e);
	}

	delete stmt;
#endif
}
