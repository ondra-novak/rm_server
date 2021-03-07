/*
 * rmrpcfsys.cpp
 *
 *  Created on: 6. 3. 2021
 *      Author: ondra
 */

#include <fstream>
#include <shared/filesystem.h>

#include "rmrpcfsys.h"

#include <cctype>
#include <iterator>

#include <imtjson/object.h>
#include <imtjson/serializer.h>
#include <imtjson/operations.h>
#include <shared/logOutput.h>
#include <shared/streams.h>
#include <userver/query_parser.h>
#include "csscolor.h"
#include "rmparser.h"

using ondra_shared::logError;
using ondra_shared::logWarning;


RmRpcFSys::RmRpcFSys(const std::string_view &rootPath):root(rootPath) {

}

void RmRpcFSys::initRpc(std::shared_ptr<RmRpcFSys> me, json::RpcServer &rpc) {

}

std::string_view RmRpcFSys::vpathToFileID(std::string_view vpath) {
	if (vpath.empty()) return vpath;
	if (vpath[0] == '/') vpath = vpath.substr(1);
	for (char c: vpath) {
		if (!std::isxdigit(c) && c!='-') return std::string_view();
	}
	return vpath;

}

void RmRpcFSys::initHttp(std::shared_ptr<RmRpcFSys> me, userver::HttpServer &http) {
	http.addPath("/pdf", [me](userver::PHttpServerRequest &req, std::string_view vpath){
		if (!req->allowMethods({"GET"})) return true;
		return me->serveFile(req, vpathToFileID(vpath), ".pdf", "application/pdf");
	});
	http.addPath("/epub", [me](userver::PHttpServerRequest &req, std::string_view vpath){
		if (!req->allowMethods({"GET"})) return true;
		return me->serveFile(req, vpathToFileID(vpath), ".epub", "application/epub+zip");
	});
	http.addPath("/thumb", [me](userver::PHttpServerRequest &req, std::string_view vpath){
		if (!req->allowMethods({"GET"})) return true;
		userver::QueryParser qp(vpath);
		auto page = qp["page"];
		if (page.defined) {
			return me->getThumb(req, vpathToFileID(qp.getPath()), page.getUInt());
		} else {
			return me->getThumb(req, vpathToFileID(qp.getPath()));
		}
	});
	http.addPath("/list", [me](userver::PHttpServerRequest &req, std::string_view vpath){
		if (!req->allowMethods({"GET"})) return true;
		if (vpath.empty()) {
			me->listFiles(req);
			return true;
		} else {
			return false;
		}
	});
	http.addPath("/info", [me](userver::PHttpServerRequest &req, std::string_view vpath){
		if (!req->allowMethods({"GET"})) return true;
		auto id = vpathToFileID(vpath);
		if (id.empty()) return false;
		return me->getFileInfo(req, id);
	});
	http.addPath("/lines", [me](userver::PHttpServerRequest &req, std::string_view vpath){
		if (!req->allowMethods({"GET"})) return true;
		userver::QueryParser qp(vpath);
		auto page = qp["page"];
		auto format = qp["format"];
		auto smooth = qp["smooth"].getUInt();
		auto id = vpathToFileID(qp.getPath());
		if (id.empty()) return false;
		LinesFormat fmt;
		if (format == "json") fmt = LinesFormat::json;
		else if (format == "svg") fmt = LinesFormat::svg;
		else fmt = LinesFormat::raw;
		return me->getLines(req, id, page.getUInt(), fmt, smooth);

	});
}

void RmRpcFSys::sendJSON(userver::PHttpServerRequest &req, json::Value json) {
	req->setContentType("application/json");
	userver::Stream s = req->send();
	json.serialize([&](int c){s.putChar(c);});
}

void RmRpcFSys::listFiles(userver::PHttpServerRequest &req) {
	struct Info {
		json::Value metadata;
		std::vector<std::string> objects;
	};

	std::unordered_map<std::string, Info> files;


	for (auto &p: std::filesystem::directory_iterator(root)) {
		const std::filesystem::path &fpath = p;
		auto name = fpath.stem().string();
		auto ext = fpath.extension().string();
		auto path = fpath.string();

		files[name].objects.push_back(ext);

		if (ext == ".metadata") {
			json::Value data = readJSON(fpath);
			if (data.defined()) {
				files[name].metadata = data;
			}
		}
	}
	sendJSON(req, json::Value(json::object, files.begin(), files.end(), [&](const auto &item) ->json::Value {
		if (item.second.metadata.defined()) {
			return json::Value(item.first, json::Object
						("metadata",item.second.metadata)
						("objects",json::Value(json::array,
								item.second.objects.begin(),
								item.second.objects.end(),[&](const std::string &x) -> json::Value{
				return x;
			})));
		} else {
			return json::Value();
		}
	}));

}

json::Value RmRpcFSys::readJSON(const std::string &pathname) {
	std::ifstream in(pathname, std::ios::in);
	if (in) {
		try {
			return  json::Value::fromStream(in);
		} catch (std::exception &e) {
			logError("Parse error: $1 - error: $2", pathname, e.what());
		}
	} else {
		logError("Can't open file: $1 - error: $2", pathname);
	}
	return json::undefined;
}


bool RmRpcFSys::serveFile(userver::PHttpServerRequest &req, std::string_view id, std::string_view ext, std::string_view ctx) {
	if (id.empty()) {
		return false;
	} else {
		auto path = root / id;
		path.replace_extension(ext);
		req->setContentType(ctx);
		return req->sendFile(std::move(req), path.native());
	}

}

bool RmRpcFSys::getThumb(userver::PHttpServerRequest &req, std::string_view id, json::Value content, unsigned long page) {
	auto thumb_path = root/id;
	thumb_path.replace_extension(".thumbnails");
	std::string thumbId = content["pages"][page].getString();
	thumb_path = thumb_path / thumbId;
	thumb_path.replace_extension(".jpg");
	return req->sendFile(std::move(req), thumb_path.native());
}

bool RmRpcFSys::getThumb(userver::PHttpServerRequest &req, std::string_view id, unsigned long page) {
	auto content_path = root/id;
	content_path.replace_extension(".content");
	json::Value content = readJSON(content_path);
	return getThumb(req, id, content, page);

}

bool RmRpcFSys::getThumb(userver::PHttpServerRequest &req, std::string_view id) {
	auto metadata_path = root/id;
	auto content_path = metadata_path;
	auto thumb_path = metadata_path;
	metadata_path.replace_extension(".metadata");
	content_path.replace_extension(".content");
	thumb_path.replace_extension(".thumbnails");

	json::Value content = readJSON(content_path);
	json::Value metadata = readJSON(metadata_path);
	auto cover = content["coverPageNumber"].getInt();
	if (cover < 0) {
		cover = metadata["lastOpenedPage"].getInt();
	}
	return getThumb(req, id, content, cover);
}

bool RmRpcFSys::getFileInfo(userver::PHttpServerRequest &req, std::string_view id) {
	auto content_path = root/id;
	auto metadata_path = root/id;
	auto lines_path = content_path;
	auto thumb_path = content_path;
	auto conv_path = content_path;
	content_path.replace_extension(".content");
	metadata_path.replace_extension(".metadata");
	conv_path.replace_extension(".textconversion");
	thumb_path.replace_extension(".thumbnails");
	json::Value content = readJSON(content_path);
	if (!content.defined()) return false;
	std::unordered_map<std::string, long> pages;
	{
		long p = 0;
		for (json::Value v: content["pages"]) pages[v.getString()] = p++;
	}
	json::Object result;
	result.set("content",content.replace("pages",json::undefined));
	json::Value metadata = readJSON(metadata_path);
	result.set("metadata",metadata);
	try {
	std::filesystem::directory_iterator diriter(lines_path);
	result.set("drawings",json::Value(json::array,std::filesystem::begin(diriter), std::filesystem::end(diriter), [&](const std::filesystem::directory_entry &entry){
			const std::filesystem::path &pathname = entry;
			if (pathname.extension().string() == ".rm") {
				auto stem = pathname.stem().string();
				auto pg = pages.find(stem);
				if (pg != pages.end()) {
					json::Object res;
					res.set("page",pg->second);
					auto mdata_path = pathname.parent_path() / (stem+"-metadata.json");
					auto layers = readJSON(mdata_path);
					res.set("layers", layers["layers"]);
					return json::Value(res);
				}
			}
		return json::Value(json::undefined);
	}).sort([](json::Value a, json::Value b){
		return json::Value::compare(a["page"],b["page"]);
	}));
	} catch (...) {}
	try {
		std::filesystem::directory_iterator diriter (thumb_path);
	result.set("thumbnails",json::Value(json::array,std::filesystem::begin(diriter),
				std::filesystem::end(diriter), [&](const std::filesystem::directory_entry &entry){
			const std::filesystem::path &pathname = entry;
			if (pathname.extension().string() == ".jpg") {
				auto stem = pathname.stem().string();
				auto pg = pages.find(stem);
				if (pg != pages.end()) {
					return json::Value(pg->second);
				}
			}
		return json::Value(json::undefined);
	}).sort([](json::Value a, json::Value b){
		return json::Value::compare(a,b);
	}));
	} catch (...) {}

	try {
		std::filesystem::directory_iterator diriter  (conv_path);
	result.set("text_conversions",json::Value(json::array,std::filesystem::begin(diriter),
				std::filesystem::end(diriter), [&](const std::filesystem::directory_entry &entry){
			const std::filesystem::path &pathname = entry;
			if (pathname.extension().string() == ".json") {
				auto stem = pathname.stem().string();
				auto pg = pages.find(stem);
				if (pg != pages.end()) {
					json::Value jf = readJSON(pathname);
					return json::Value(json::Object("page", pg->second)("text",jf["text"]));
				}
			}
		return json::Value(json::undefined);
	}).sort([](json::Value a, json::Value b){
		return json::Value::compare(a["page"],b["page"]);
	}));
	} catch (...) {}

	sendJSON(req, result);
	return true;
}

bool RmRpcFSys::getLines(userver::PHttpServerRequest &req, std::string_view id, unsigned long page, LinesFormat fmt, int smooth) {
	auto content_path = root/id;
	content_path.replace_extension(".content");
	json::Value content = readJSON(content_path);
	auto lines_path = root/id;
	std::string thumbId = content["pages"][page].getString();
	lines_path = lines_path / thumbId;
	lines_path.replace_extension(".rm");

	if (fmt == LinesFormat::raw) {
		return req->sendFile(std::move(req), lines_path.native());
	} else {

		std::ifstream rmf(lines_path.native());
		if (!rmf) return false;
		Drawing drw;
		drw.load_rm(rmf);
		if (smooth) drw.smooth(smooth);
		if (fmt == LinesFormat::json) {
			auto out = drw.toJSON();
			sendJSON(req, out);
		} else {
			auto mdata_path = lines_path.parent_path() / (lines_path.stem().string()+"-metadata.json");
			json::Value layers = readJSON(mdata_path)["layers"];
			Drawing::ColorDef colorDef;
			std::string color_name;
			int lrpos = 1;
			for (json::Value lr: layers) {
				auto name = lr["name"].getString();
				auto colorpos = name.indexOf("/");
				if (colorpos != name.npos) {
					auto color = name.substr(colorpos+1);
					for (char c: color) {
						if (isspace(c)) break;
						color_name.push_back(c);
					}
					if (!color_name.empty()) {
						CSSColor baseColor(color_name);
						CSSColor white("#FFFFFF");
						white.a = baseColor.a;
						CSSColor mixed = baseColor.mix(white);
						colorDef.layerColors.push_back({lrpos,{
							std::string(baseColor.getCSSColor()),
							std::string(mixed.getCSSColor()),
							std::string(white.getCSSColor()),
							0
						}});
						color_name.clear();
					}
				}
				lrpos++;
			}

			colorDef.prepare();
			req->setContentTypeFromExt("svg");
			userver::Stream s = req->send();
			ondra_shared::ostream out([&](char c){s.putChar(c);});
			drw.render_svg(out, colorDef);
		}
		return true;
	}
}
