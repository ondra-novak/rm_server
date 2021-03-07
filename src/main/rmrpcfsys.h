/*
 * rmrpcfsys.h
 *
 *  Created on: 6. 3. 2021
 *      Author: ondra
 */

#ifndef SRC_MAIN_RMRPCFSYS_H_
#define SRC_MAIN_RMRPCFSYS_H_
#include <string_view>
#include <shared/filesystem.h>
#include <imtjson/rpc.h>
#include <userver/http_server.h>

class RmRpcFSys {
public:
	RmRpcFSys(const std::string_view &rootPath);

	static void initRpc(std::shared_ptr<RmRpcFSys> me, json::RpcServer &rpc);
	static void initHttp(std::shared_ptr<RmRpcFSys> me, userver::HttpServer &http);




	enum class LinesFormat {
		raw,
		json,
		svg
	};

protected:
	std::filesystem::path root;

	static std::string_view vpathToFileID(std::string_view vpath);

	bool serveFile(userver::PHttpServerRequest &req, std::string_view id, std::string_view ext, std::string_view ctx);
	bool getThumb(userver::PHttpServerRequest &req, std::string_view id);
	bool getThumb(userver::PHttpServerRequest &req, std::string_view id, unsigned long page);
	bool getThumb(userver::PHttpServerRequest &req, std::string_view id, json::Value content, unsigned long page);
	static json::Value readJSON(const std::string &pathname);
	void sendJSON(userver::PHttpServerRequest &req, json::Value json);

	void listFiles(userver::PHttpServerRequest &req);
	bool getFileInfo(userver::PHttpServerRequest &req, std::string_view id);
	bool getLines(userver::PHttpServerRequest &req, std::string_view id, unsigned long page, LinesFormat fmt, int smooth);
};

#endif /* SRC_MAIN_RMRPCFSYS_H_ */
