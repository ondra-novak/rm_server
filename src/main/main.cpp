#include <shared/default_app.h>
#include <imtjson/rpc.h>
#include <shared/linux_crash_handler.h>
#include <shared/logOutput.h>
#include <userver/http_server.h>
#include <userver/static_webserver.h>
#include <userver_jsonrpc/rpcServer.h>

#include "rmrpcfsys.h"

using ondra_shared::logFatal;
using ondra_shared::logNote;

ondra_shared::DefaultApp app({}, std::cerr);

class MyHttpServer: public userver::RpcHttpServer {
public:
	MyHttpServer():lo("http") {

	}

	virtual void log(userver::ReqEvent event, const userver::HttpServerRequest &req) {
		if (event == userver::ReqEvent::done) {
			auto now = std::chrono::system_clock::now();
			auto dur = std::chrono::duration_cast<std::chrono::microseconds>(now-req.getRecvTime());
			char buff[100];
			snprintf(buff,100,"%1.3f ms", dur.count()*0.001);
			std::lock_guard _(mx);
			lo.progress("#$1 $2 $3 $4 $5 $6", req.getIdent(), req.getStatus(), req.getMethod(), req.getHost(), req.getURI(), buff);
		}
	}
	virtual void log(const userver::HttpServerRequest &req, const std::string_view &msg) {
		std::lock_guard _(mx);
		lo.note("#$1 $2", req.getIdent(), msg);
	}
	virtual void logDirect(const std::string_view &s) {
		std::lock_guard _(mx);
		lo.progress("[RPC-Direct] $1",s);
	};
	virtual void unhandled() override {
		try {
			throw;
		} catch (const std::exception &e) {
			std::lock_guard _(mx);
			lo.error("Unhandled exception: $1", e.what());
		} catch (...) {
			std::lock_guard _(mx);
			lo.error("Unhandled exception: <unknown>");
		}
	}
protected:
	std::mutex mx;
	ondra_shared::LogObject lo;

};

int main(int argc, char **argv) {

	if (!app.init(argc, argv)) {
		std::cerr << "Invalid arguments. Use -h for help" << std::endl;
		return EFAULT;
	}

	logNote("---- SERVER INIT ----");

	ondra_shared::CrashHandler crashHandler([](const char *c){
		logFatal("CrashHandler: $1", c);
	});
	crashHandler.install();
	auto section_server = app.config["server"];
	auto section_www = app.config["www"];
	auto section_filesystem = app.config["filesystem"];

	userver::StaticWebserver::Config static_web_cfg = {
			section_www.mandatory["document_root"].getPath(),
			section_www.mandatory["index"].getString(),
			section_www["version_prefix"].getString(),
			section_www["version_nr"].getString(),
			static_cast<unsigned int>(section_www["cache_interval"].getUInt(0))
	};

	auto rmfs = std::make_shared<RmRpcFSys>(section_filesystem.mandatory["path"].getPath());

	MyHttpServer server;

	server.addRPCPath("/RPC", {true,true,true,10*1024*1024});
	server.addPath("", userver::StaticWebserver(static_web_cfg));
	server.add_listMethods();
	server.add_ping();
	server.addStats("/stats", nullptr);

	rmfs->initRpc(rmfs, server);
	rmfs->initHttp(rmfs, server);


	server.start(userver::NetAddr::fromString(section_server.mandatory["listen"].getString(), "3456"),
				section_server.mandatory["threads"].getUInt(), 1);
	logNote("---- SERVER START ----");

	server.stopOnSignal();
	server.addThread();
	server.stop();

	logNote("---- SERVER STOPPED ----");



return 0;


}
